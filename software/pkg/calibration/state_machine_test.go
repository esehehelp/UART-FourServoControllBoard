package calibration

import (
	"bytes"
	"testing"
)

// The firmware memcpy()s the float into a little-endian float field, so the
// wire bytes must be the IEEE-754 little-endian representation.
func TestPackFloat32LE(t *testing.T) {
	dst := make([]uint8, 4)
	packFloat32LE(dst, 1.0) // 0x3F800000
	if want := []uint8{0x00, 0x00, 0x80, 0x3F}; !bytes.Equal(dst, want) {
		t.Errorf("packFloat32LE(1.0) = % X, want % X", dst, want)
	}
}

func TestCheckFBRaw(t *testing.T) {
	for _, tt := range []struct {
		raw uint16
		ok  bool
	}{
		{0, false}, {50, false}, {124, true}, {2048, true}, {3971, true}, {4000, false}, {4095, false},
	} {
		if err := checkFBRaw(tt.raw); (err == nil) != tt.ok {
			t.Errorf("checkFBRaw(%d) err=%v, want ok=%v", tt.raw, err, tt.ok)
		}
	}
}
