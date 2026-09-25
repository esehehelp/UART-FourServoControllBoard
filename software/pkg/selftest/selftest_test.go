package selftest

import (
	"sync"
	"testing"
	"time"

	"uart-servo-controller/config"
	"uart-servo-controller/pkg/serial"
)

// fakeBoard answers like firmware protocol v3.0 (or, with legacy=true, like
// the old firmware that never sends error responses).
type fakeBoard struct {
	mu     sync.Mutex
	out    []uint8
	legacy bool
}

func (f *fakeBoard) reply(cmd uint8, data []uint8) {
	p := serial.NewPacket(config.HOST_ID, cmd, data)
	p.Source = 0x01
	f.out = append(f.out, p.Marshal()...)
}

func (f *fakeBoard) fail(cmd, code uint8) {
	if !f.legacy {
		f.reply(config.RESP_ERROR, []uint8{cmd, code})
	}
}

func (f *fakeBoard) Write(b []uint8) (int, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	pkt, err := serial.Unmarshal(b)
	if err != nil {
		return len(b), nil
	}
	d := pkt.Data
	switch pkt.Cmd {
	case config.CMD_SENSOR_READ:
		res := make([]uint8, 15)
		res[1], res[2] = 0x04, 0x00 // ~5.0 V
		res[3], res[4] = 0x0C, 0xFC // ~25 °C
		f.reply(config.RESP_SENSOR_DATA, res)
	case config.CMD_CAL_GET:
		res := make([]uint8, 13)
		res[0] = d[0]
		res[9], res[10], res[11], res[12] = 0x01, 0xF4, 0x09, 0xC4
		f.reply(config.RESP_CAL_DATA, res)
	case config.CMD_LED_SET:
		if d[0] > 1 {
			f.fail(pkt.Cmd, config.ErrCodeBadChannel)
		}
	case config.CMD_SERVO_WRITE:
		if pulse := uint16(d[1])<<8 | uint16(d[2]); pulse > 2500 {
			f.fail(pkt.Cmd, config.ErrCodeBadValue)
		}
	case config.CMD_PD_VOLTAGE:
		if mv := uint16(d[0])<<8 | uint16(d[1]); mv > 16800 {
			f.fail(pkt.Cmd, config.ErrCodeBadValue)
		}
	case config.CMD_CAL_SAVE:
		f.fail(pkt.Cmd, config.ErrCodeCalInvalid)
	default:
		f.fail(pkt.Cmd, config.ErrCodeUnknownCmd)
	}
	return len(b), nil
}

func (f *fakeBoard) Read(b []uint8) (int, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	n := copy(b, f.out)
	f.out = f.out[n:]
	return n, nil
}

func run(t *testing.T, board *fakeBoard) []Result {
	r := NewRunner(board, config.DEVICE_ID)
	r.Timeout = 50 * time.Millisecond
	return r.Run()
}

func TestSelftestPassesOnConformingBoard(t *testing.T) {
	for _, res := range run(t, &fakeBoard{}) {
		if !res.Passed {
			t.Errorf("%s: %s", res.Name, res.Detail)
		}
	}
}

func TestSelftestDetectsMissingErrorResponses(t *testing.T) {
	failed := 0
	for _, res := range run(t, &fakeBoard{legacy: true}) {
		if !res.Passed {
			failed++
		}
	}
	// the five "-> ERR_*" checks must fail against firmware without #51
	if failed != 5 {
		t.Errorf("failed checks = %d, want 5", failed)
	}
}
