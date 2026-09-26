// Package selftest runs non-destructive checks against a connected board
// (#32). It only reads data, toggles the LEDs and sends requests the
// firmware must reject, so no servo moves and no setting is changed.
package selftest

import (
	"fmt"
	"io"
	"time"

	"uart-servo-controller/config"
	"uart-servo-controller/pkg/serial"
)

// Result of one check
type Result struct {
	Name   string
	Passed bool
	Detail string
}

// Runner talks to one board over port (any byte stream: serial port or a
// fake device in tests).
type Runner struct {
	Port     io.ReadWriter
	Target   uint8
	Timeout  time.Duration
	buf      []uint8
	readByte []uint8
}

// NewRunner creates a runner for the board with the given device ID.
func NewRunner(port io.ReadWriter, target uint8) *Runner {
	return &Runner{Port: port, Target: target, Timeout: 500 * time.Millisecond, readByte: make([]uint8, 64)}
}

// Run executes all checks in order.
func (r *Runner) Run() []Result {
	checks := []struct {
		name string
		fn   func() error
	}{
		{"sensor read (0x02 -> 0x82)", r.checkSensors},
		{"calibration read CH0-3 (0x08 -> 0x88)", r.checkCalibration},
		{"config read FW version (0x21 -> 0x85)", r.checkConfigRead},
		{"LED1/LED2 on/off (0x30)", r.checkLED},
		{"bad LED channel -> ERR_BAD_CHANNEL", r.expectError(config.CMD_LED_SET, []uint8{5, 0}, config.ErrCodeBadChannel)},
		{"retired 0x05 -> ERR_UNKNOWN_CMD", r.expectError(0x05, []uint8{0}, config.ErrCodeUnknownCmd)},
		{"servo 3000 us -> ERR_BAD_VALUE (no motion)", r.expectError(config.CMD_SERVO_WRITE, []uint8{0, 0x0B, 0xB8}, config.ErrCodeBadValue)},
		{"PD 20 V -> ERR_BAD_VALUE (not requested)", r.expectError(config.CMD_PD_VOLTAGE, []uint8{0x4E, 0x20}, config.ErrCodeBadValue)},
		{"cal save min>=max -> ERR_CAL_INVALID (not saved)", r.expectError(config.CMD_CAL_SAVE, invalidCal(), config.ErrCodeCalInvalid)},
	}
	results := make([]Result, 0, len(checks))
	for _, c := range checks {
		err := c.fn()
		res := Result{Name: c.name, Passed: err == nil}
		if err != nil {
			res.Detail = err.Error()
		}
		results = append(results, res)
	}
	return results
}

// invalidCal is a CMD_CAL_SAVE payload with min_pulse > max_pulse, which the
// firmware must reject before touching the flash.
func invalidCal() []uint8 {
	d := make([]uint8, config.CAL_DATA_LEN)
	d[9], d[10] = 0x09, 0xC4  // min 2500
	d[11], d[12] = 0x01, 0xF4 // max 500
	return d
}

func (r *Runner) send(cmd uint8, data []uint8) error {
	_, err := r.Port.Write(serial.NewPacket(r.Target, cmd, data).Marshal())
	return err
}

// next returns the next valid packet from the board, or nil on timeout.
func (r *Runner) next(timeout time.Duration) (*serial.Packet, error) {
	deadline := time.Now().Add(timeout)
	for {
		if pkt := r.takePacket(); pkt != nil {
			return pkt, nil
		}
		if time.Now().After(deadline) {
			return nil, nil
		}
		n, err := r.Port.Read(r.readByte)
		if err != nil && err != io.EOF {
			return nil, err
		}
		if n == 0 {
			time.Sleep(5 * time.Millisecond)
		}
		r.buf = append(r.buf, r.readByte[:n]...)
	}
}

// takePacket removes and returns the first complete valid packet in buf.
func (r *Runner) takePacket() *serial.Packet {
	for len(r.buf) > 0 {
		if r.buf[0] != config.PKT_HEADER {
			r.buf = r.buf[1:]
			continue
		}
		if len(r.buf) < config.PKT_OVERHEAD {
			return nil
		}
		size := config.PKT_OVERHEAD + int(r.buf[5])
		if len(r.buf) < size {
			return nil
		}
		pkt, err := serial.Unmarshal(r.buf[:size])
		if err != nil {
			r.buf = r.buf[1:]
			continue
		}
		r.buf = r.buf[size:]
		return pkt
	}
	return nil
}

// request sends cmd and waits for want (or an error response).
func (r *Runner) request(cmd uint8, data []uint8, want uint8) (*serial.Packet, error) {
	r.buf = r.buf[:0]
	if err := r.send(cmd, data); err != nil {
		return nil, err
	}
	for {
		pkt, err := r.next(r.Timeout)
		if err != nil {
			return nil, err
		}
		if pkt == nil {
			return nil, fmt.Errorf("no response to cmd 0x%02X", cmd)
		}
		if pkt.Cmd == want {
			return pkt, nil
		}
		if pkt.Cmd == config.RESP_ERROR && len(pkt.Data) >= 2 && pkt.Data[0] == cmd {
			return pkt, nil
		}
	}
}

func describeError(pkt *serial.Packet) string {
	if pkt.Cmd != config.RESP_ERROR || len(pkt.Data) < 2 {
		return fmt.Sprintf("unexpected response 0x%02X", pkt.Cmd)
	}
	name, ok := config.ErrCodeNames[pkt.Data[1]]
	if !ok {
		name = "unknown"
	}
	return fmt.Sprintf("device error %s (0x%02X)", name, pkt.Data[1])
}

func (r *Runner) checkSensors() error {
	pkt, err := r.request(config.CMD_SENSOR_READ, []uint8{config.SENSOR_TYPE_ALL}, config.RESP_SENSOR_DATA)
	if err != nil {
		return err
	}
	if pkt.Cmd != config.RESP_SENSOR_DATA {
		return fmt.Errorf("%s", describeError(pkt))
	}
	if len(pkt.Data) != 15 {
		return fmt.Errorf("sensor data is %d bytes, want 15", len(pkt.Data))
	}
	raw := func(i int) uint16 { return uint16(pkt.Data[i])<<8 | uint16(pkt.Data[i+1]) }
	volt := float64(raw(1)) * config.VOLTAGE_SCALE
	if volt < 4.0 || volt > 21.0 {
		return fmt.Errorf("bus voltage %.2f V outside 4-21 V", volt)
	}
	if t := raw(3); t == 0 || t >= 4095 {
		return fmt.Errorf("temperature ADC %d at the rail (NTC open/short?)", t)
	}
	return nil
}

func (r *Runner) checkCalibration() error {
	for ch := uint8(0); ch < 4; ch++ {
		pkt, err := r.request(config.CMD_CAL_GET, []uint8{ch}, config.RESP_CAL_DATA)
		if err != nil {
			return err
		}
		if pkt.Cmd != config.RESP_CAL_DATA {
			return fmt.Errorf("CH%d: %s", ch, describeError(pkt))
		}
		if len(pkt.Data) != 13 || pkt.Data[0] != ch {
			return fmt.Errorf("CH%d: malformed calibration data", ch)
		}
		minP := uint16(pkt.Data[9])<<8 | uint16(pkt.Data[10])
		maxP := uint16(pkt.Data[11])<<8 | uint16(pkt.Data[12])
		if minP >= maxP {
			return fmt.Errorf("CH%d: min %d >= max %d", ch, minP, maxP)
		}
	}
	return nil
}

// checkConfigRead reads the firmware version tag (#49)
func (r *Runner) checkConfigRead() error {
	pkt, err := r.request(config.CMD_CONFIG_READ, []uint8{config.CFG_TAG_FW_VERSION}, config.RESP_CFG_DATA)
	if err != nil {
		return err
	}
	if pkt.Cmd != config.RESP_CFG_DATA {
		return fmt.Errorf("%s", describeError(pkt))
	}
	if len(pkt.Data) != 4 || pkt.Data[0] != config.CFG_TAG_FW_VERSION {
		return fmt.Errorf("malformed version response % X", pkt.Data)
	}
	return nil
}

// checkLED blinks both LEDs; success means no error response comes back.
func (r *Runner) checkLED() error {
	for _, step := range [][2]uint8{{0, 255}, {1, 255}, {0, 0}, {1, 0}} {
		r.buf = r.buf[:0]
		if err := r.send(config.CMD_LED_SET, step[:]); err != nil {
			return err
		}
		if pkt, err := r.next(100 * time.Millisecond); err != nil {
			return err
		} else if pkt != nil && pkt.Cmd == config.RESP_ERROR {
			return fmt.Errorf("LED%d: %s", step[0]+1, describeError(pkt))
		}
	}
	return nil
}

// expectError returns a check that sends cmd and requires error code want.
func (r *Runner) expectError(cmd uint8, data []uint8, want uint8) func() error {
	return func() error {
		pkt, err := r.request(cmd, data, config.RESP_ERROR)
		if err != nil {
			return err
		}
		if pkt.Cmd != config.RESP_ERROR || len(pkt.Data) < 2 {
			return fmt.Errorf("accepted (got 0x%02X), want error 0x%02X", pkt.Cmd, want)
		}
		if pkt.Data[1] != want {
			return fmt.Errorf("got %s, want 0x%02X", describeError(pkt), want)
		}
		return nil
	}
}
