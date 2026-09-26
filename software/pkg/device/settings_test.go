package device

import (
	"encoding/binary"
	"sync"
	"testing"

	"uart-servo-controller/config"
	"uart-servo-controller/pkg/serial"
)

// fakeConfigBoard answers 0x20/0x21 like firmware config_cmd.c for the
// scalar tags used by the GUI.
type fakeConfigBoard struct {
	mu     sync.Mutex
	vals   map[uint8][]uint8
	writes []uint8
}

func newFakeConfigBoard() *fakeConfigBoard {
	u16 := func(v uint16) []uint8 { return []uint8{uint8(v >> 8), uint8(v)} }
	return &fakeConfigBoard{vals: map[uint8][]uint8{
		config.CFG_TAG_NAME:            {},
		config.CFG_TAG_FW_VERSION:      {0, 8, 1},
		config.CFG_TAG_PROT_MASK:       {config.PROT_OVERCURRENT | config.PROT_OVERVOLTAGE | config.PROT_OVERHEAT},
		config.CFG_TAG_MAX_CURRENT:     u16(6000),
		config.CFG_TAG_MIN_VOLTAGE:     u16(4500),
		config.CFG_TAG_MAX_VOLTAGE:     u16(17500),
		config.CFG_TAG_MAX_TEMP:        u16(80),
		config.CFG_TAG_STALL_CURRENT:   u16(1500),
		config.CFG_TAG_STALL_TIME:      u16(500),
		config.CFG_TAG_STALL_FB_DELTA:  u16(20),
		config.CFG_TAG_STALL_POS_ERROR: u16(100),
	}}
}

func (f *fakeConfigBoard) reply(req *serial.Packet) *serial.Packet {
	f.mu.Lock()
	defer f.mu.Unlock()
	fail := func(code uint8) *serial.Packet {
		return serial.NewPacket(config.HOST_ID, config.RESP_ERROR, []uint8{req.Cmd, code})
	}
	tag := req.Data[0]
	switch req.Cmd {
	case config.CMD_CONFIG_READ:
		v, ok := f.vals[tag]
		if !ok {
			return fail(config.ErrCodeBadValue)
		}
		return serial.NewPacket(config.HOST_ID, config.RESP_CFG_DATA, append([]uint8{tag}, v...))
	case config.CMD_CONFIG_WRITE:
		if _, ok := f.vals[tag]; !ok || tag == config.CFG_TAG_FW_VERSION {
			return fail(config.ErrCodeBadValue)
		}
		old := f.vals[tag]
		f.vals[tag] = append([]uint8(nil), req.Data[1:]...)
		minV := binary.BigEndian.Uint16(f.vals[config.CFG_TAG_MIN_VOLTAGE])
		maxV := binary.BigEndian.Uint16(f.vals[config.CFG_TAG_MAX_VOLTAGE])
		if minV >= maxV {
			f.vals[tag] = old
			return fail(config.ErrCodeConfigInvalid)
		}
		f.writes = append(f.writes, tag)
		return serial.NewPacket(config.HOST_ID, config.RESP_CFG_ACK, []uint8{tag})
	}
	return nil
}

func TestNameRoundtrip(t *testing.T) {
	b := newFakeConfigBoard()
	c := ackTestController(b.reply)
	if err := c.SetName("ArmLeft"); err != nil {
		t.Fatal(err)
	}
	if got, err := c.GetName(); err != nil || got != "ArmLeft" {
		t.Fatalf("GetName() = %q, %v", got, err)
	}
	if err := c.SetName("this name is too long"); err == nil {
		t.Error("expected error for a 21-byte name")
	}
	if v, err := c.GetFirmwareVersion(); err != nil || v != "0.8.1" {
		t.Errorf("GetFirmwareVersion() = %q, %v", v, err)
	}
}

func TestProtectionRoundtripWritesOnlyChanges(t *testing.T) {
	b := newFakeConfigBoard()
	c := ackTestController(b.reply)
	p, err := c.GetProtection()
	if err != nil {
		t.Fatal(err)
	}
	if p.MaxCurrentMA != 6000 || p.MaxTempC != 80 || p.FeatureMask != 0x0D {
		t.Fatalf("GetProtection() = %+v", p)
	}
	p.MaxCurrentMA = 4000
	p.FeatureMask |= config.PROT_STALL
	if err := c.SetProtection(p); err != nil {
		t.Fatal(err)
	}
	if len(b.writes) != 2 || b.writes[0] != config.CFG_TAG_MAX_CURRENT || b.writes[1] != config.CFG_TAG_PROT_MASK {
		t.Errorf("writes = % X, want [31 30]", b.writes)
	}
	got, _ := c.GetProtection()
	if got != p {
		t.Errorf("after SetProtection: %+v, want %+v", got, p)
	}
}

func TestProtectionVoltageOrder(t *testing.T) {
	b := newFakeConfigBoard()
	c := ackTestController(b.reply)
	p, _ := c.GetProtection()
	// new min above the current max: max must be written first
	p.MinVoltageMV, p.MaxVoltageMV = 18000, 19000
	if err := c.SetProtection(p); err != nil {
		t.Fatalf("SetProtection() = %v", err)
	}
	p.MinVoltageMV, p.MaxVoltageMV = 5000, 5000
	if err := c.SetProtection(p); err == nil {
		t.Error("min == max must be refused")
	}
}

func TestProtectionEventMessage(t *testing.T) {
	e := &DeviceError{Cmd: 0, Code: config.ErrCodeStall, Detail: 0x02}
	if !e.IsProtection() || e.Error() != "protection: stall (0x33), servos freed: CH1" {
		t.Errorf("Error() = %q", e.Error())
	}
}
