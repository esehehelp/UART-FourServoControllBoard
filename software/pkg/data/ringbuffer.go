package data

import (
	"sync"
)

// RingBuffer is a thread-safe circular buffer
type RingBuffer struct {
	data  []float64
	head  int
	count int
	mu    sync.RWMutex
}

// NewRingBuffer creates a new ring buffer
func NewRingBuffer(capacity int) *RingBuffer {
	return &RingBuffer{
		data: make([]float64, capacity),
	}
}

// Push adds a value to the buffer
func (rb *RingBuffer) Push(value float64) {
	rb.mu.Lock()
	defer rb.mu.Unlock()

	rb.data[rb.head] = value
	rb.head = (rb.head + 1) % len(rb.data)

	if rb.count < len(rb.data) {
		rb.count++
	}
}

// Values returns a copy of all values in insertion order
func (rb *RingBuffer) Values() []float64 {
	rb.mu.RLock()
	defer rb.mu.RUnlock()

	if rb.count == 0 {
		return []float64{}
	}

	result := make([]float64, rb.count)
	for i := 0; i < rb.count; i++ {
		idx := (rb.head - rb.count + i + len(rb.data)) % len(rb.data)
		result[i] = rb.data[idx]
	}
	return result
}

// Count returns number of values in buffer
func (rb *RingBuffer) Count() int {
	rb.mu.RLock()
	defer rb.mu.RUnlock()
	return rb.count
}

// Clear empties the buffer
func (rb *RingBuffer) Clear() {
	rb.mu.Lock()
	defer rb.mu.Unlock()
	rb.head = 0
	rb.count = 0
}

// Last returns the most recent value
func (rb *RingBuffer) Last() (float64, bool) {
	rb.mu.RLock()
	defer rb.mu.RUnlock()

	if rb.count == 0 {
		return 0, false
	}

	idx := (rb.head - 1 + len(rb.data)) % len(rb.data)
	return rb.data[idx], true
}

// KalmanFilter implements a simple 1D Kalman filter
type KalmanFilter struct {
	Q float64 // Process noise
	R float64 // Measurement noise
	X float64 // State estimate
	P float64 // Error estimate
}

// NewKalmanFilter creates a new Kalman filter
func NewKalmanFilter(q, r float64) *KalmanFilter {
	return &KalmanFilter{
		Q: q,
		R: r,
		X: 0,
		P: 1,
	}
}

// Update applies Kalman filter to measurement
func (kf *KalmanFilter) Update(measurement float64) float64 {
	// Predict
	kf.P = kf.P + kf.Q

	// Update
	k := kf.P / (kf.P + kf.R)
	kf.X = kf.X + k*(measurement-kf.X)
	kf.P = (1 - k) * kf.P

	return kf.X
}

// Reset resets the filter state
func (kf *KalmanFilter) Reset() {
	kf.X = 0
	kf.P = 1
}
