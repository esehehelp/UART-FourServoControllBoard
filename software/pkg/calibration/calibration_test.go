package calibration

import (
	"math"
	"testing"
)

func TestPackFloat32LE(t *testing.T) {
	cases := []struct {
		name string
		val  float32
		want [4]uint8
	}{
		{"1.0", 1.0, [4]uint8{0x00, 0x00, 0x80, 0x3F}},
		{"0.0", 0.0, [4]uint8{0x00, 0x00, 0x00, 0x00}},
		{"-1.0", -1.0, [4]uint8{0x00, 0x00, 0x80, 0xBF}},
		{"slope_typical", 1.5, [4]uint8{0x00, 0x00, 0xC0, 0x3F}},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			got := make([]uint8, 4)
			packFloat32LE(got, tc.val)
			for i, b := range tc.want {
				if got[i] != b {
					t.Errorf("byte[%d]: got 0x%02X, want 0x%02X", i, got[i], b)
				}
			}
			// Verify roundtrip via LE reassembly (mirrors firmware memcpy behavior)
			bits := uint32(got[0]) | uint32(got[1])<<8 | uint32(got[2])<<16 | uint32(got[3])<<24
			if math.Float32frombits(bits) != tc.val {
				t.Errorf("roundtrip failed: got %v, want %v", math.Float32frombits(bits), tc.val)
			}
		})
	}
}
