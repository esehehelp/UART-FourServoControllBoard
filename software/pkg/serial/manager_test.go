package serial

import (
	"sort"
	"testing"

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
