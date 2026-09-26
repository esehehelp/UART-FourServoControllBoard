package serial

import (
	"sort"
	"testing"
	"time"

	"go.bug.st/serial/enumerator"
)

func TestPortPriorityOrder(t *testing.T) {
	ports := []*enumerator.PortDetails{
		{Name: "/dev/ttyS0"},
		{Name: "COM1"},
		{Name: "COM7", IsUSB: true, VID: "10C4"}, // other USB-serial
		{Name: "/dev/ttyUSB0"},
		{Name: "COM12", IsUSB: true, VID: "1a86"}, // board (VID reported lower-case on Linux)
		{Name: "/dev/rfcomm0"},
	}
	sort.SliceStable(ports, func(i, j int) bool {
		return portPriority(ports[i]) > portPriority(ports[j])
	})

	want := []string{"COM12", "COM7", "/dev/ttyUSB0", "COM1", "/dev/rfcomm0", "/dev/ttyS0"}
	for i, p := range ports {
		if p.Name != want[i] {
			t.Fatalf("order[%d] = %s, want %s (full order must be %v)", i, p.Name, want[i], want)
		}
	}
}

func TestNextPacketStream(t *testing.T) {
	a := NewPacket(0x00, 0x82, make([]uint8, 15)).Marshal()
	b := NewPacket(0x00, 0x85, []uint8{0x03, 'A'}).Marshal()
	buf := append(append(append([]uint8{0x13, 0x00}, a...), b...), b[:4]...)
	p1, rest := nextPacket(buf)
	p2, rest := nextPacket(rest)
	p3, rest := nextPacket(rest)
	if p1 == nil || p1.Cmd != 0x82 || p2 == nil || p2.Cmd != 0x85 || p3 != nil || len(rest) != 4 {
		t.Fatalf("got %v %v %v rest=%d", p1, p2, p3, len(rest))
	}
}

// echoPort answers every write with reply(req).
type echoPort struct {
	out   []uint8
	reply func(*Packet) *Packet
}

func (e *echoPort) Write(b []uint8) (int, error) {
	if req, err := Unmarshal(b); err == nil {
		if r := e.reply(req); r != nil {
			e.out = append(e.out, r.Marshal()...)
		}
	}
	return len(b), nil
}

func (e *echoPort) Read(b []uint8) (int, error) {
	n := copy(b, e.out)
	e.out = e.out[n:]
	return n, nil
}

func TestRequestSkipsUnrelatedPackets(t *testing.T) {
	port := &echoPort{reply: func(req *Packet) *Packet {
		// a board answers the broadcast probe with its own ID as source
		r := NewPacket(0x00, 0x82, make([]uint8, 15))
		r.Source = 0x05
		return r
	}}
	port.out = NewPacket(0x00, 0xEE, []uint8{0x00, 0x30, 0x0F}).Marshal() // stale event first
	got := request(port, NewPacket(0xFF, 0x02, []uint8{0}), 100*time.Millisecond,
		func(p *Packet) bool { return p.Cmd == 0x82 })
	if got == nil || got.Source != 0x05 {
		t.Fatalf("request() = %v", got)
	}
}

func TestDeviceInfoLabel(t *testing.T) {
	if l := (DeviceInfo{ID: 2}).Label(); l != "Device-02 [ID:0x02]" {
		t.Errorf("Label() = %q", l)
	}
	if l := (DeviceInfo{ID: 1, Name: "ArmLeft"}).Label(); l != "ArmLeft [ID:0x01]" {
		t.Errorf("Label() = %q", l)
	}
}
