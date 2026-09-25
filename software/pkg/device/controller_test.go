package device

import (
	"math"
	"testing"
	"time"

	"uart-servo-controller/config"
	"uart-servo-controller/pkg/data"
	"uart-servo-controller/pkg/serial"
)

func TestCalcTemperature(t *testing.T) {
	tests := []struct {
		name string
		raw  uint16
		want float64
		tol  float64
	}{
		// NTC = 22k (25°C): 4095 * 22000 / (5100 + 22000) ≈ 3324
		{"room_25C", 3324, 25, 0.5},
		// NTC = 5.1k: raw ≈ 2047.5, R_ntc = R_series -> ~62°C
		{"half_scale", 2048, 62, 2},
		{"zero_is_invalid", 0, 0, 0},
		{"full_scale_is_invalid", 4095, 0, 0},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := calcTemperature(tt.raw)
			if math.Abs(got-tt.want) > tt.tol {
				t.Errorf("calcTemperature(%d) = %.2f, want %.2f±%.1f", tt.raw, got, tt.want, tt.tol)
			}
		})
	}
}

func newTestController() *Controller {
	c := &Controller{
		times: data.NewRingBuffer(config.MAX_PLOT_POINTS),
		volts: data.NewRingBuffer(config.MAX_PLOT_POINTS),
		currs: data.NewRingBuffer(config.MAX_PLOT_POINTS),
		temps: data.NewRingBuffer(config.MAX_PLOT_POINTS),
		kfV:   data.NewKalmanFilter(config.KF_Q_VOLTAGE, config.KF_R_VOLTAGE),
		kfI:   data.NewKalmanFilter(config.KF_Q_CURRENT, config.KF_R_CURRENT),
	}
	for i := range c.fbv {
		c.fbv[i] = data.NewRingBuffer(config.MAX_PLOT_POINTS)
	}
	return c
}

func sensorPacket(rawV uint16) *serial.Packet {
	d := make([]uint8, 15)
	d[1], d[2] = uint8(rawV>>8), uint8(rawV)
	d[3], d[4] = 0x0C, 0xFC // 3324 -> ~25°C
	return serial.NewPacket(config.HOST_ID, config.RESP_SENSOR_DATA, d)
}

func TestSensorTimeout(t *testing.T) {
	c := newTestController()
	if c.GetSensorData().Valid {
		t.Fatal("data must be invalid before the first response")
	}

	c.processPacket(sensorPacket(1000), time.Now())
	if !c.GetSensorData().Valid {
		t.Fatal("data must be valid after a response")
	}

	c.checkTimeout()
	if !c.GetSensorData().Valid {
		t.Fatal("fresh data must stay valid")
	}

	c.mu.Lock()
	c.data.Timestamp = time.Now().Add(-time.Duration(config.SENSOR_TIMEOUT_MS+100) * time.Millisecond)
	c.mu.Unlock()
	c.checkTimeout()
	if c.GetSensorData().Valid {
		t.Fatal("data must be invalid after SENSOR_TIMEOUT_MS")
	}

	// The Kalman filter restarts after a timeout: the first estimate is
	// pulled most of the way toward the new measurement, not blended with
	// the pre-disconnect state.
	c.processPacket(sensorPacket(3000), time.Now())
	got := c.GetSensorData().Voltage
	want := 3000 * config.VOLTAGE_SCALE
	if math.Abs(got-want) > want*0.1 {
		t.Errorf("voltage after reconnect = %.3f, want ≈%.3f (filter reset)", got, want)
	}
}

// ackTestController answers every sent packet with reply (if non-nil)
// through processPacket, like the processor goroutine would.
func ackTestController(reply func(req *serial.Packet) *serial.Packet) *Controller {
	c := newTestController()
	c.sendFn = func(req *serial.Packet) error {
		if r := reply(req); r != nil {
			go c.processPacket(r, time.Now())
		}
		return nil
	}
	return c
}

func TestSetPDVoltageAck(t *testing.T) {
	c := ackTestController(func(req *serial.Packet) *serial.Packet {
		return serial.NewPacket(config.HOST_ID, config.RESP_PD_ACK, req.Data)
	})
	if err := c.SetPDVoltage(9000); err != nil {
		t.Fatalf("SetPDVoltage(9000) = %v, want nil", err)
	}
}

func TestSetPDVoltageLimit(t *testing.T) {
	sent := false
	c := ackTestController(func(*serial.Packet) *serial.Packet { sent = true; return nil })
	for _, mv := range []uint16{4000, 15000, 20000} {
		if err := c.SetPDVoltage(mv); err == nil {
			t.Errorf("SetPDVoltage(%d) = nil, want error", mv)
		}
	}
	if sent {
		t.Error("out-of-range voltage must not be sent to the device")
	}
}

func TestCalSaveDeviceError(t *testing.T) {
	c := ackTestController(func(req *serial.Packet) *serial.Packet {
		return serial.NewPacket(config.HOST_ID, config.RESP_ERROR, []uint8{req.Cmd, config.ErrCodeFlashWrite})
	})
	err := c.RequestCalibrationSave(make([]uint8, config.CAL_DATA_LEN))
	devErr, ok := err.(*DeviceError)
	if !ok || devErr.Code != config.ErrCodeFlashWrite || devErr.Cmd != config.CMD_CAL_SAVE {
		t.Fatalf("RequestCalibrationSave() = %v, want DeviceError flash write on 0x07", err)
	}
}

func TestAckTimeout(t *testing.T) {
	c := ackTestController(func(*serial.Packet) *serial.Packet { return nil })
	if err := c.RequestCalibrationSave(make([]uint8, config.CAL_DATA_LEN)); err == nil {
		t.Fatal("expected timeout error without a response")
	}
}

func TestUnsolicitedErrorHandler(t *testing.T) {
	c := newTestController()
	var got *DeviceError
	c.SetErrorHandler(func(e *DeviceError) { got = e })
	c.processPacket(serial.NewPacket(config.HOST_ID, config.RESP_ERROR,
		[]uint8{config.CMD_SERVO_WRITE, config.ErrCodeBadValue}), time.Now())
	if got == nil || got.Cmd != config.CMD_SERVO_WRITE || got.Code != config.ErrCodeBadValue {
		t.Fatalf("error handler got %v", got)
	}
}
