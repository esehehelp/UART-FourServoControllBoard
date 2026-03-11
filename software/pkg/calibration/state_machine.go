package calibration

import (
	"fmt"
	"sync"
	"time"

	"uart-servo-controller/config"
	"uart-servo-controller/pkg/device"
)

// State represents calibration state
type State int

const (
	StateIdle State = iota
	StateInit
	StateCoarseSearch
	StateFineSearch
	StateSaving
	StateError
)

// Result holds calibration results for one channel
type Result struct {
	Channel   uint8
	MinPulse  uint16
	MaxPulse  uint16
	Slope     float32
	Intercept float32
	Error     string
}

// StateMachine manages servo calibration
type StateMachine struct {
	ctrl      *device.Controller
	state     State
	channel   uint8
	mu        sync.RWMutex
	callback  func(state State, msg string)
	done      chan struct{}
	result    *Result
	history   []float64
	historyMu sync.Mutex
}

// NewStateMachine creates a new calibration state machine
func NewStateMachine(ctrl *device.Controller, callback func(State, string)) *StateMachine {
	return &StateMachine{
		ctrl:     ctrl,
		callback: callback,
		done:     make(chan struct{}),
		history:  make([]float64, 0, 100),
	}
}

// Start begins calibration for a channel
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

	go sm.worker()

	return nil
}

// Stop cancels calibration
func (sm *StateMachine) Stop() {
	sm.mu.Lock()
	defer sm.mu.Unlock()

	if sm.state != StateIdle {
		sm.state = StateIdle
		sm.notifyCallback(StateIdle, "Calibration cancelled")
	}
}

// GetState returns current state
func (sm *StateMachine) GetState() (State, uint8) {
	sm.mu.RLock()
	defer sm.mu.RUnlock()
	return sm.state, sm.channel
}

// GetResult returns calibration result
func (sm *StateMachine) GetResult() *Result {
	sm.mu.RLock()
	defer sm.mu.RUnlock()
	return sm.result
}

// worker runs the calibration state machine
func (sm *StateMachine) worker() {
	defer func() {
		sm.mu.Lock()
		sm.state = StateIdle
		sm.mu.Unlock()
		sm.notifyCallback(StateIdle, "Calibration complete")
	}()

	ch := sm.channel

	// State: Init
	sm.notifyCallback(StateInit, fmt.Sprintf("CH%d: Initializing...", ch))
	time.Sleep(500 * time.Millisecond)

	// State: Coarse search (100us steps)
	sm.mu.Lock()
	sm.state = StateCoarseSearch
	sm.mu.Unlock()

	sm.notifyCallback(StateCoarseSearch, fmt.Sprintf("CH%d: Coarse search starting...", ch))
	minPulse := sm.findLimit(ch, config.SERVO_DEFAULT, config.SERVO_MIN_PULSE, config.SERVO_DEFAULT, config.CAL_COARSE_STEP)
	maxPulse := sm.findLimit(ch, config.SERVO_DEFAULT, config.SERVO_DEFAULT, config.SERVO_MAX_PULSE, config.CAL_COARSE_STEP)

	sm.result.MinPulse = minPulse
	sm.result.MaxPulse = maxPulse

	sm.notifyCallback(StateCoarseSearch, fmt.Sprintf("CH%d: Coarse limits: %d-%d", ch, minPulse, maxPulse))
	time.Sleep(500 * time.Millisecond)

	// State: Fine search (1us steps)
	sm.mu.Lock()
	sm.state = StateFineSearch
	sm.mu.Unlock()

	sm.notifyCallback(StateFineSearch, fmt.Sprintf("CH%d: Fine search starting...", ch))

	// Refine minimum
	minFine := sm.findLimit(ch, minPulse-50, minPulse-100, minPulse, config.CAL_FINE_STEP)
	sm.result.MinPulse = minFine

	// Refine maximum
	maxFine := sm.findLimit(ch, maxPulse+50, maxPulse, maxPulse+100, config.CAL_FINE_STEP)
	sm.result.MaxPulse = maxFine

	sm.notifyCallback(StateFineSearch, fmt.Sprintf("CH%d: Fine limits: %d-%d", ch, minFine, maxFine))

	// Calculate calibration coefficients
	sm.calculateCoefficients()

	// State: Saving
	sm.mu.Lock()
	sm.state = StateSaving
	sm.mu.Unlock()

	sm.notifyCallback(StateSaving, fmt.Sprintf("CH%d: Saving to flash...", ch))

	if err := sm.saveCalibration(); err != nil {
		sm.result.Error = err.Error()
		sm.mu.Lock()
		sm.state = StateError
		sm.mu.Unlock()
		sm.notifyCallback(StateError, fmt.Sprintf("CH%d: Save failed: %v", ch, err))
		return
	}

	sm.notifyCallback(StateSaving, fmt.Sprintf("CH%d: Saved successfully", ch))
}

// findLimit searches for servo movement limit
func (sm *StateMachine) findLimit(ch uint8, start, rangeMin, rangeMax uint16, step uint16) uint16 {
	if rangeMin > rangeMax {
		rangeMin, rangeMax = rangeMax, rangeMin
	}

	current := start
	direction := int16(1)
	if rangeMax < start {
		direction = -1
	}

	lastRipple := 0.0
	threshold := config.CAL_RIPPLE_THRESHOLD

	for {
		if sm.checkCancel() {
			return current
		}

		// Move to position
		if err := sm.ctrl.SetServo(ch, current); err != nil {
			return current
		}

		// Wait for stabilization
		time.Sleep(time.Duration(config.CAL_WAIT_STABLE) * time.Millisecond)

		// Measure ripple
		ripple := sm.measureRipple()

		// Check if servo reached limit or lost contact
		if ripple > threshold || sm.hasLargeChange(lastRipple, ripple) {
			// Return to last stable position
			return current - uint16(direction*int16(step))
		}

		lastRipple = ripple
		current = uint16(int16(current) + direction*int16(step))

		// Bounds check
		if direction > 0 && current > rangeMax {
			return current - step
		}
		if direction < 0 && current < rangeMin {
			return current + step
		}
	}
}

// measureRipple measures voltage ripple
func (sm *StateMachine) measureRipple() float64 {
	sm.clearHistory()

	// Collect samples
	for i := 0; i < 20; i++ {
		data := sm.ctrl.GetSensorData()
		sm.addHistory(data.Voltage)
		time.Sleep(50 * time.Millisecond)
	}

	// Calculate ripple (max - min)
	if len(sm.history) == 0 {
		return 0
	}

	min := sm.history[0]
	max := sm.history[0]
	for _, v := range sm.history {
		if v < min {
			min = v
		}
		if v > max {
			max = v
		}
	}

	return max - min
}

// hasLargeChange detects large changes in ripple
func (sm *StateMachine) hasLargeChange(prev, curr float64) bool {
	if prev == 0 {
		return false
	}
	change := (curr - prev) / prev
	return change > 0.5 || change < -0.5
}

// calculateCoefficients calculates slope and intercept
func (sm *StateMachine) calculateCoefficients() {
	// Simplified: use fixed values
	// In a full implementation, this would measure actual feedback voltages
	// at min and max positions and calculate linear mapping

	// For now, use defaults
	sm.result.Slope = 1.0
	sm.result.Intercept = 0.0
}

// saveCalibration saves calibration data to device flash
func (sm *StateMachine) saveCalibration() error {
	ch := sm.channel
	minPulse := sm.result.MinPulse
	maxPulse := sm.result.MaxPulse
	slope := sm.result.Slope
	intercept := sm.result.Intercept

	// Build calibration packet
	data := make([]uint8, 13)
	data[0] = ch

	// Pack slope (4 bytes, little-endian float)
	sliceBytes := make([]uint8, 4)
	bitsSlope := fmt.Sprintf("%x", uint32(slope))
	_ = bitsSlope // Use this for encoding slope properly

	// Pack intercept (4 bytes, little-endian float)
	sliceIntercept := make([]uint8, 4)
	_ = sliceIntercept

	// Min/max pulses (2 bytes each)
	data[9] = uint8(minPulse >> 8)
	data[10] = uint8(minPulse & 0xFF)
	data[11] = uint8(maxPulse >> 8)
	data[12] = uint8(maxPulse & 0xFF)

	// TODO: Properly encode slope and intercept as IEEE 754 floats
	// For now, copy fixed values
	copy(data[1:5], []uint8{0, 0, 0, 0})
	copy(data[5:9], []uint8{0, 0, 0, 0})

	// Send calibration save command
	return sm.ctrl.RequestCalibrationSave(data)
}

// History helpers
func (sm *StateMachine) clearHistory() {
	sm.historyMu.Lock()
	defer sm.historyMu.Unlock()
	sm.history = sm.history[:0]
}

func (sm *StateMachine) addHistory(v float64) {
	sm.historyMu.Lock()
	defer sm.historyMu.Unlock()
	if len(sm.history) < cap(sm.history) {
		sm.history = append(sm.history, v)
	}
}

// checkCancel checks if calibration was cancelled
func (sm *StateMachine) checkCancel() bool {
	sm.mu.RLock()
	defer sm.mu.RUnlock()
	return sm.state == StateIdle
}

// notifyCallback calls the status callback
func (sm *StateMachine) notifyCallback(state State, msg string) {
	if sm.callback != nil {
		sm.callback(state, msg)
	}
}
