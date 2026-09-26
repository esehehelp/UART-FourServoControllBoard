package device

import (
	"fmt"
	"log"
	"math"
	"sync"
	"time"

	"uart-servo-controller/config"
	"uart-servo-controller/pkg/data"
	"uart-servo-controller/pkg/serial"
)

// SensorData holds the latest sensor readings
type SensorData struct {
	Voltage  float64   // V
	Current  float64   // mA
	Temp     float64   // °C
	FBVolt   [4]float64 // FB voltage per channel (V)
	RawFB    [4]uint16  // raw feedback value per channel as sent by the firmware
	RawTemp  uint16    // raw temperature value
	Timestamp time.Time
	// Valid is false until the first response arrives and again after no
	// sensor response has been received for SENSOR_TIMEOUT_MS.
	Valid bool
}

// Controller manages device communication and data
type Controller struct {
	sm    *serial.Manager
	mu    sync.RWMutex
	data  SensorData
	times *data.RingBuffer
	volts *data.RingBuffer
	currs *data.RingBuffer
	temps *data.RingBuffer
	fbv   [4]*data.RingBuffer
	kfV   *data.KalmanFilter
	kfI   *data.KalmanFilter
	done  chan struct{}

	// ACK / error handling (#51)
	pmu     sync.Mutex
	pending []*ackWaiter
	onError func(*DeviceError)
	sendFn  func(*serial.Packet) error // nil: c.sm.Send (overridden in tests)
}

// DeviceError is an error response (RESP_ERROR) reported by the firmware.
// Cmd 0x00 means the board raised it on its own (protection, #47/#28);
// Detail then holds the affected channel mask.
type DeviceError struct {
	Cmd    uint8 // command that failed, 0x00 for protection events
	Code   uint8 // error code (config.ErrCode*)
	Detail uint8 // channel mask for protection events
}

// IsProtection reports whether the board raised this error by itself.
func (e *DeviceError) IsProtection() bool { return e.Cmd == 0x00 }

func (e *DeviceError) Error() string {
	name, ok := config.ErrCodeNames[e.Code]
	if !ok {
		name = "unknown error"
	}
	if e.IsProtection() {
		return fmt.Sprintf("protection: %s (0x%02X), servos freed: %s", name, e.Code, channelList(e.Detail))
	}
	return fmt.Sprintf("device error on cmd 0x%02X: %s (0x%02X)", e.Cmd, name, e.Code)
}

func channelList(mask uint8) string {
	s := ""
	for ch := 0; ch < 4; ch++ {
		if mask&(1<<ch) != 0 {
			if s != "" {
				s += ","
			}
			s += fmt.Sprintf("CH%d", ch)
		}
	}
	if s == "" {
		return "none"
	}
	return s
}

// ackWaiter is a command waiting for its response or an error for reqCmd
type ackWaiter struct {
	reqCmd uint8
	match  func(*serial.Packet) bool
	result chan waitResult
}

type waitResult struct {
	pkt *serial.Packet
	err error
}

// NewController creates a new device controller
func NewController(sm *serial.Manager) *Controller {
	ctl := &Controller{
		sm:   sm,
		data: SensorData{Timestamp: time.Now()},
		times: data.NewRingBuffer(config.MAX_PLOT_POINTS),
		volts: data.NewRingBuffer(config.MAX_PLOT_POINTS),
		currs: data.NewRingBuffer(config.MAX_PLOT_POINTS),
		temps: data.NewRingBuffer(config.MAX_PLOT_POINTS),
		kfV:   data.NewKalmanFilter(config.KF_Q_VOLTAGE, config.KF_R_VOLTAGE),
		kfI:   data.NewKalmanFilter(config.KF_Q_CURRENT, config.KF_R_CURRENT),
		done:  make(chan struct{}),
	}

	for i := 0; i < 4; i++ {
		ctl.fbv[i] = data.NewRingBuffer(config.MAX_PLOT_POINTS)
	}

	go ctl.processorLoop()
	return ctl
}

// SetErrorHandler registers a callback for device errors that no command is
// waiting for (e.g. a rejected servo write). Called from the processor
// goroutine.
func (c *Controller) SetErrorHandler(f func(*DeviceError)) {
	c.pmu.Lock()
	c.onError = f
	c.pmu.Unlock()
}

// target is the device ID commands are sent to: the ID the connected board
// answered with, or the factory default.
func (c *Controller) target() uint8 {
	if c.sm != nil {
		if info, ok := c.sm.Connected(); ok {
			return info.ID
		}
	}
	return config.DEVICE_ID
}

func (c *Controller) send(pkt *serial.Packet) error {
	if c.sendFn != nil {
		return c.sendFn(pkt)
	}
	return c.sm.Send(pkt)
}

// sendWithAck sends pkt and waits for ackCmd, a RESP_ERROR for pkt.Cmd
// (returned as *DeviceError) or ACK_TIMEOUT_MS.
func (c *Controller) sendWithAck(pkt *serial.Packet, ackCmd uint8) error {
	_, err := c.sendAndWait(pkt, func(r *serial.Packet) bool { return r.Cmd == ackCmd })
	return err
}

// sendAndWait sends pkt and returns the first response accepted by match,
// a *DeviceError if the board rejects pkt.Cmd, or a timeout error.
func (c *Controller) sendAndWait(pkt *serial.Packet, match func(*serial.Packet) bool) (*serial.Packet, error) {
	w := &ackWaiter{reqCmd: pkt.Cmd, match: match, result: make(chan waitResult, 1)}
	c.pmu.Lock()
	c.pending = append(c.pending, w)
	c.pmu.Unlock()
	defer c.removeWaiter(w)

	if err := c.send(pkt); err != nil {
		return nil, err
	}
	select {
	case r := <-w.result:
		return r.pkt, r.err
	case <-time.After(time.Duration(config.ACK_TIMEOUT_MS) * time.Millisecond):
		return nil, fmt.Errorf("no response to cmd 0x%02X within %d ms", pkt.Cmd, config.ACK_TIMEOUT_MS)
	}
}

func (c *Controller) removeWaiter(w *ackWaiter) {
	c.pmu.Lock()
	defer c.pmu.Unlock()
	for i, p := range c.pending {
		if p == w {
			c.pending = append(c.pending[:i], c.pending[i+1:]...)
			return
		}
	}
}

// resolveWaiter completes the oldest waiter accepted by match; false if none.
func (c *Controller) resolveWaiter(match func(*ackWaiter) bool, res waitResult) bool {
	c.pmu.Lock()
	defer c.pmu.Unlock()
	for i, p := range c.pending {
		if match(p) {
			c.pending = append(c.pending[:i], c.pending[i+1:]...)
			p.result <- res
			return true
		}
	}
	return false
}

// Stop stops the controller
func (c *Controller) Stop() {
	close(c.done)
}

// GetSensorData returns a copy of current sensor data
func (c *Controller) GetSensorData() SensorData {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.data
}

// GetPlotData returns data for plotting
func (c *Controller) GetPlotData() (times, volts, currs, temps []float64, fbv [4][]float64) {
	times = c.times.Values()
	volts = c.volts.Values()
	currs = c.currs.Values()
	temps = c.temps.Values()
	for i := 0; i < 4; i++ {
		fbv[i] = c.fbv[i].Values()
	}
	return
}

// SetServo sends servo control command
func (c *Controller) SetServo(ch uint8, microseconds uint16) error {
	if ch >= 4 {
		return fmt.Errorf("invalid servo channel: %d", ch)
	}
	if microseconds < config.SERVO_MIN_PULSE {
		microseconds = config.SERVO_MIN_PULSE
	}
	if microseconds > config.SERVO_MAX_PULSE {
		microseconds = config.SERVO_MAX_PULSE
	}

	data := []uint8{ch, uint8(microseconds >> 8), uint8(microseconds & 0xFF)}
	pkt := serial.NewPacket(c.target(), config.CMD_SERVO_WRITE, data)
	return c.send(pkt)
}

// SetLED sends LED control command (ch: 0=LED1, 1=LED2; duty: 0-255)
func (c *Controller) SetLED(ch, duty uint8) error {
	pkt := serial.NewPacket(c.target(), config.CMD_LED_SET, []uint8{ch, duty})
	return c.send(pkt)
}

// SetPDVoltage requests a USB-PD voltage and waits for the device's ACK.
// Requests outside PD_VOLTAGE_MIN..PD_VOLTAGE_MAX_UI are refused (#38).
func (c *Controller) SetPDVoltage(millivolts uint16) error {
	if millivolts < config.PD_VOLTAGE_MIN || millivolts > config.PD_VOLTAGE_MAX_UI {
		return fmt.Errorf("voltage %d mV outside the V0.8 board limit (%d-%d mV)",
			millivolts, config.PD_VOLTAGE_MIN, config.PD_VOLTAGE_MAX_UI)
	}
	data := []uint8{uint8(millivolts >> 8), uint8(millivolts & 0xFF)}
	pkt := serial.NewPacket(c.target(), config.CMD_PD_VOLTAGE, data)
	return c.sendWithAck(pkt, config.RESP_PD_ACK)
}

// RequestSensorRead requests sensor data
func (c *Controller) RequestSensorRead() error {
	pkt := serial.NewPacket(c.target(), config.CMD_SENSOR_READ, []uint8{config.SENSOR_TYPE_ALL})
	return c.send(pkt)
}

// RequestCalibrationSave sends calibration save command and waits until the
// device confirms the flash write (RESP_CAL_ACK) or reports an error.
func (c *Controller) RequestCalibrationSave(data []uint8) error {
	pkt := serial.NewPacket(c.target(), config.CMD_CAL_SAVE, data)
	return c.sendWithAck(pkt, config.RESP_CAL_ACK)
}

// ServoFree disables PWM output for channels specified by chMask (bit0=CH0..bit3=CH3).
// The servo becomes limp. Call SetServo to re-engage.
func (c *Controller) ServoFree(chMask uint8) error {
	pkt := serial.NewPacket(c.target(), config.CMD_SERVO_FREE, []uint8{chMask})
	return c.send(pkt)
}

// processorLoop handles incoming packets and periodic sensor reads
func (c *Controller) processorLoop() {
	ticker := time.NewTicker(time.Duration(config.UPDATE_INTERVAL_MS) * time.Millisecond)
	defer ticker.Stop()

	startTime := time.Now()
	rxChan := c.sm.RxChan()

	for {
		select {
		case <-c.done:
			return

		case <-ticker.C:
			c.checkTimeout()
			// Periodically request sensor data
			c.RequestSensorRead()

		case pkt, ok := <-rxChan:
			if !ok {
				// Serial manager stopped and closed the channel. A nil channel
				// blocks forever, so this case stops firing instead of spinning.
				rxChan = nil
				continue
			}
			if pkt != nil {
				c.processPacket(pkt, startTime)
			}

		case err := <-c.sm.ErrChan():
			if err != nil {
				fmt.Printf("Serial error: %v\n", err)
			}
		}
	}
}

// checkTimeout marks the sensor data invalid when no response has arrived
// for SENSOR_TIMEOUT_MS, so stale values are not shown as live data.
func (c *Controller) checkTimeout() {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.data.Valid && time.Since(c.data.Timestamp) > time.Duration(config.SENSOR_TIMEOUT_MS)*time.Millisecond {
		c.data.Valid = false
	}
}

// processPacket processes an incoming packet
func (c *Controller) processPacket(pkt *serial.Packet, startTime time.Time) {
	switch pkt.Cmd {
	case config.RESP_SENSOR_DATA:
		c.processSensorData(pkt, startTime)
	case config.RESP_ERROR:
		if len(pkt.Data) < 2 {
			return
		}
		devErr := &DeviceError{Cmd: pkt.Data[0], Code: pkt.Data[1]}
		if len(pkt.Data) >= 3 {
			devErr.Detail = pkt.Data[2]
		}
		if !devErr.IsProtection() &&
			c.resolveWaiter(func(w *ackWaiter) bool { return w.reqCmd == devErr.Cmd }, waitResult{err: devErr}) {
			return
		}
		log.Printf("%v", devErr)
		c.pmu.Lock()
		onError := c.onError
		c.pmu.Unlock()
		if onError != nil {
			onError(devErr)
		}
	default:
		c.resolveWaiter(func(w *ackWaiter) bool { return w.match(pkt) }, waitResult{pkt: pkt})
	}
}

// processSensorData extracts sensor values from response packet
func (c *Controller) processSensorData(pkt *serial.Packet, startTime time.Time) {
	if len(pkt.Data) < 15 {
		return
	}

	d := pkt.Data

	// Voltage: d[1:3] * 0.00491
	rawV := (uint16(d[1]) << 8) | uint16(d[2])
	volt := float64(rawV) * config.VOLTAGE_SCALE

	// Current: d[5:7] * 2.518
	rawI := (uint16(d[5]) << 8) | uint16(d[6])
	curr := float64(rawI) * config.CURRENT_SCALE

	// Temperature: d[3:5]
	rawT := (uint16(d[3]) << 8) | uint16(d[4])
	temp := calcTemperature(rawT)

	// Feedback voltages: d[7+j*2 : 9+j*2]
	fbVolts := [4]float64{}
	rawFBs := [4]uint16{}
	for j := 0; j < 4; j++ {
		rawFB := (uint16(d[7+j*2]) << 8) | uint16(d[8+j*2])
		rawFBs[j] = rawFB
		fbVolts[j] = float64(rawFB) * config.FB_VOLTAGE_SCALE
	}

	// After a timeout (disconnect / reconnect) start the filters from scratch
	// instead of blending with values from the previous session.
	c.mu.RLock()
	wasValid := c.data.Valid
	c.mu.RUnlock()
	if !wasValid {
		c.kfV.Reset()
		c.kfI.Reset()
	}

	// Apply Kalman filtering
	filteredV := c.kfV.Update(volt)
	filteredI := c.kfI.Update(curr)

	// Update data storage
	c.mu.Lock()
	c.data.Valid = true
	c.data.Voltage = filteredV
	c.data.Current = filteredI
	c.data.Temp = temp
	c.data.RawTemp = rawT
	c.data.FBVolt = fbVolts
	c.data.RawFB = rawFBs
	c.data.Timestamp = time.Now()
	c.mu.Unlock()

	// Add to plot buffers
	elapsed := time.Since(startTime).Seconds()
	c.times.Push(elapsed)
	c.volts.Push(filteredV)
	c.currs.Push(filteredI)
	c.temps.Push(temp)
	for j := 0; j < 4; j++ {
		c.fbv[j].Push(fbVolts[j])
	}
}

// calcTemperature converts raw ADC value to temperature using NTC formula
func calcTemperature(rawTemp uint16) float64 {
	if rawTemp == 0 || rawTemp >= 4095 {
		return 0
	}

	// Divider: Vout/Vref = R_ntc / (R_series + R_ntc)
	// => R_ntc = R_series * raw / (4095 - raw)
	res := config.TEMP_SERIES_R * float64(rawTemp) / float64(4095-rawTemp)

	// T = 1 / (ln(R/R0)/B + 1/T0) - 273.15
	logRatio := math.Log(res / config.TEMP_R0)
	invT := logRatio/config.TEMP_B + 1/config.TEMP_T0
	tempK := 1 / invT

	return tempK - 273.15
}
