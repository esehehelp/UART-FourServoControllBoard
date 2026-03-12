// Package calibration implements manual position calibration via PWM-off.
//
// Flow:
//
//	Idle → Init(center) → ArmFreeMin(PWM off, user moves) → [Confirm]
//	     → ArmFreeMax(PWM off, user moves) → [Confirm]
//	     → Saving → Idle
//
// The calibration records the FBV at the user-defined min and max physical
// positions. A linear mapping (pulse = Slope*fbVolt + Intercept) is then
// calculated so that the firmware can translate FBV readings into pulse widths.
package calibration

import (
	"fmt"
	"math"
	"sync"
	"time"

	"uart-servo-controller/config"
	"uart-servo-controller/pkg/device"
)

// State represents the calibration state machine stage.
type State int

const (
	StateIdle      State = iota // 0 – not running
	StateInit                   // 1 – moving to center position
	StateArmFreeMin             // 2 – PWM off, waiting for user to confirm min position
	StateArmFreeMax             // 3 – PWM off, waiting for user to confirm max position
	StateSaving                 // 4 – computing coefficients and writing to flash
	StateError                  // 5 – unrecoverable error
)

// Result holds calibration output for one channel.
// The linear model is: pulse_µs = Slope * fbVolt + Intercept
type Result struct {
	Channel   uint8
	MinPulse  uint16  // reference pulse for min physical position (µs)
	MaxPulse  uint16  // reference pulse for max physical position (µs)
	FBAtMin   float64 // FBV recorded at min position (V)
	FBAtMax   float64 // FBV recorded at max position (V)
	Slope     float32 // µs / V
	Intercept float32 // µs
	Error     string
}

// StateMachine manages the position calibration lifecycle.
type StateMachine struct {
	ctrl      *device.Controller
	state     State
	channel   uint8
	mu        sync.RWMutex
	callback  func(State, string)
	result    *Result
	confirmCh chan struct{}
}

// NewStateMachine creates a StateMachine with the given controller and status callback.
func NewStateMachine(ctrl *device.Controller, callback func(State, string)) *StateMachine {
	return &StateMachine{
		ctrl:      ctrl,
		callback:  callback,
		confirmCh: make(chan struct{}, 1),
	}
}

// Start begins position calibration for channel (0–3).
func (sm *StateMachine) Start(channel uint8) error {
	sm.mu.Lock()
	defer sm.mu.Unlock()

	if sm.state != StateIdle {
		return fmt.Errorf("calibration already in progress")
	}
	if channel >= 4 {
		return fmt.Errorf("invalid channel: %d", channel)
	}

	sm.channel = channel
	sm.state = StateInit
	sm.result = &Result{Channel: channel}

	// Drain any stale confirm signal
	select {
	case <-sm.confirmCh:
	default:
	}

	go sm.worker()
	return nil
}

// Stop cancels an in-progress calibration.
func (sm *StateMachine) Stop() {
	sm.mu.Lock()
	if sm.state == StateIdle {
		sm.mu.Unlock()
		return
	}
	sm.state = StateIdle
	sm.mu.Unlock()

	sm.notify(StateIdle, "Calibration cancelled")

	// Unblock any waitConfirm that may be sleeping
	select {
	case sm.confirmCh <- struct{}{}:
	default:
	}
}

// Confirm signals that the user has positioned the arm at the current target.
// Only valid while in StateArmFreeMin or StateArmFreeMax.
func (sm *StateMachine) Confirm() error {
	sm.mu.RLock()
	s := sm.state
	sm.mu.RUnlock()

	if s != StateArmFreeMin && s != StateArmFreeMax {
		return fmt.Errorf("not waiting for confirmation (state=%d)", s)
	}
	select {
	case sm.confirmCh <- struct{}{}:
		return nil
	default:
		return fmt.Errorf("already confirmed")
	}
}

// GetState returns the current state and active channel.
func (sm *StateMachine) GetState() (State, uint8) {
	sm.mu.RLock()
	defer sm.mu.RUnlock()
	return sm.state, sm.channel
}

// GetResult returns the latest calibration result (may be nil before first run).
func (sm *StateMachine) GetResult() *Result {
	sm.mu.RLock()
	defer sm.mu.RUnlock()
	return sm.result
}

// ── worker ──────────────────────────────────────────────────────────────────

func (sm *StateMachine) worker() {
	defer func() {
		sm.mu.Lock()
		sm.state = StateIdle
		sm.mu.Unlock()
		sm.notify(StateIdle, "Calibration complete") // ← ensures frontend sees Idle
	}()

	ch := sm.channel

	// ── Init: move to center so the arm starts from a known position ─────
	sm.notify(StateInit, fmt.Sprintf("CH%d: Moving to center (%dµs)…", ch, config.SERVO_DEFAULT))
	if err := sm.ctrl.SetServo(ch, config.SERVO_DEFAULT); err != nil {
		sm.fail(ch, "SetServo failed", err)
		return
	}
	if !sm.sleep(config.CAL_WAIT_CENTER_MS) {
		return
	}

	// ── ArmFreeMin: disable PWM, wait for user to position arm at min ────
	sm.setState(StateArmFreeMin)
	if err := sm.ctrl.ServoFree(1 << ch); err != nil {
		sm.fail(ch, "ServoFree failed", err)
		return
	}
	sm.notify(StateArmFreeMin, fmt.Sprintf("CH%d: Arm free — move to MIN position, then press Confirm", ch))
	if !sm.waitConfirm() {
		return
	}

	fbAtMin := sm.sampleFB(ch)
	sm.result.FBAtMin = fbAtMin
	sm.notify(StateArmFreeMin, fmt.Sprintf("CH%d: Min recorded  FB=%.3fV", ch, fbAtMin))

	// ── ArmFreeMax: disable PWM (already off), wait for max position ─────
	sm.setState(StateArmFreeMax)
	sm.notify(StateArmFreeMax, fmt.Sprintf("CH%d: Arm free — move to MAX position, then press Confirm", ch))
	if !sm.waitConfirm() {
		return
	}

	fbAtMax := sm.sampleFB(ch)
	sm.result.FBAtMax = fbAtMax
	sm.notify(StateArmFreeMax, fmt.Sprintf("CH%d: Max recorded  FB=%.3fV", ch, fbAtMax))

	// ── Saving: calculate slope/intercept and write to flash ─────────────
	sm.setState(StateSaving)

	fbRange := fbAtMax - fbAtMin
	if math.Abs(fbRange) < 0.005 {
		sm.fail(ch, "FBV range too small — is the feedback wired?",
			fmt.Errorf("fbAtMin=%.3f fbAtMax=%.3f", fbAtMin, fbAtMax))
		return
	}

	// Ensure minPulse < maxPulse regardless of pot wiring direction
	minPulse, maxPulse := uint16(config.CAL_PULSE_MIN), uint16(config.CAL_PULSE_MAX)
	if fbAtMin > fbAtMax {
		// Pot wired in reverse: swap so fbAtMin corresponds to the smaller pulse
		minPulse, maxPulse = maxPulse, minPulse
		fbAtMin, fbAtMax = fbAtMax, fbAtMin
		fbRange = fbAtMax - fbAtMin
		sm.result.FBAtMin, sm.result.FBAtMax = fbAtMin, fbAtMax
	}
	sm.result.MinPulse = minPulse
	sm.result.MaxPulse = maxPulse

	slope := float64(maxPulse-minPulse) / fbRange
	intercept := float64(minPulse) - slope*fbAtMin
	sm.result.Slope = float32(slope)
	sm.result.Intercept = float32(intercept)

	sm.notify(StateSaving, fmt.Sprintf("CH%d: slope=%.1fµs/V  intercept=%.1fµs — saving…", ch, slope, intercept))

	if err := sm.saveCalibration(); err != nil {
		sm.fail(ch, "save failed", err)
		return
	}

	sm.notify(StateSaving, fmt.Sprintf(
		"CH%d: Done. FBV %.3f–%.3fV → pulse %d–%dµs",
		ch, sm.result.FBAtMin, sm.result.FBAtMax, sm.result.MinPulse, sm.result.MaxPulse,
	))
}

// ── helpers ─────────────────────────────────────────────────────────────────

// sampleFB averages CAL_SAMPLE_COUNT FBV readings for channel ch.
func (sm *StateMachine) sampleFB(ch uint8) float64 {
	var sum float64
	for range config.CAL_SAMPLE_COUNT {
		sum += sm.ctrl.GetSensorData().FBVolt[ch]
		time.Sleep(time.Duration(config.CAL_SAMPLE_INTERVAL_MS) * time.Millisecond)
	}
	return sum / float64(config.CAL_SAMPLE_COUNT)
}

// saveCalibration packs the result and sends CMD_CAL_SAVE to the device.
// Wire format: ch(1) + slope(4, IEEE-754 BE float32) + intercept(4) + min(2) + max(2) = 13 bytes
func (sm *StateMachine) saveCalibration() error {
	data := make([]uint8, config.CAL_DATA_LEN)
	data[0] = sm.channel
	packFloat32BE(data[1:5], sm.result.Slope)
	packFloat32BE(data[5:9], sm.result.Intercept)
	data[9] = uint8(sm.result.MinPulse >> 8)
	data[10] = uint8(sm.result.MinPulse & 0xFF)
	data[11] = uint8(sm.result.MaxPulse >> 8)
	data[12] = uint8(sm.result.MaxPulse & 0xFF)
	return sm.ctrl.RequestCalibrationSave(data)
}

// packFloat32BE writes f as 4-byte IEEE-754 big-endian into dst.
func packFloat32BE(dst []uint8, f float32) {
	b := math.Float32bits(f)
	dst[0] = uint8(b >> 24)
	dst[1] = uint8(b >> 16)
	dst[2] = uint8(b >> 8)
	dst[3] = uint8(b)
}

// waitConfirm blocks until the user calls Confirm() or calibration is cancelled.
// Returns false if cancelled.
func (sm *StateMachine) waitConfirm() bool {
	ticker := time.NewTicker(100 * time.Millisecond)
	defer ticker.Stop()
	for {
		select {
		case <-sm.confirmCh:
			return sm.isActive()
		case <-ticker.C:
			if !sm.isActive() {
				return false
			}
		}
	}
}

// sleep sleeps for ms milliseconds, returning false if cancelled mid-sleep.
func (sm *StateMachine) sleep(ms int) bool {
	deadline := time.Now().Add(time.Duration(ms) * time.Millisecond)
	for time.Now().Before(deadline) {
		if !sm.isActive() {
			return false
		}
		time.Sleep(20 * time.Millisecond)
	}
	return true
}

func (sm *StateMachine) isActive() bool {
	sm.mu.RLock()
	defer sm.mu.RUnlock()
	return sm.state != StateIdle
}

func (sm *StateMachine) setState(s State) {
	sm.mu.Lock()
	sm.state = s
	sm.mu.Unlock()
}

func (sm *StateMachine) fail(ch uint8, reason string, err error) {
	msg := fmt.Sprintf("CH%d: %s: %v", ch, reason, err)
	if sm.result != nil {
		sm.result.Error = msg
	}
	sm.setState(StateError)
	sm.notify(StateError, msg)
}

func (sm *StateMachine) notify(s State, msg string) {
	if sm.callback != nil {
		sm.callback(s, msg)
	}
}
