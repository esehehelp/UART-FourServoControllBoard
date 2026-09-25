package calibration

import (
	"testing"
)

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
