package device

import (
	"math"
	"testing"
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
