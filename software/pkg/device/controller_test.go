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
