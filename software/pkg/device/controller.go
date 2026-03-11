package device

import (
	"fmt"
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
	RawTemp  uint16    // raw temperature value
	Timestamp time.Time
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
	if microseconds > config.SERVO_MAX_PULSE {
		microseconds = config.SERVO_MAX_PULSE
	}

	data := []uint8{ch, uint8(microseconds >> 8), uint8(microseconds & 0xFF)}
	pkt := serial.NewPacket(config.DEVICE_ID, config.CMD_SERVO_WRITE, data)
	return c.sm.Send(pkt)
}

// SetLED sends LED control command
func (c *Controller) SetLED(duty uint8) error {
	pkt := serial.NewPacket(config.DEVICE_ID, config.CMD_LED_SET, []uint8{duty})
	return c.sm.Send(pkt)
}

// SetPDVoltage sends USB-PD voltage setting command
func (c *Controller) SetPDVoltage(millivolts uint16) error {
	data := []uint8{uint8(millivolts >> 8), uint8(millivolts & 0xFF)}
	pkt := serial.NewPacket(config.DEVICE_ID, config.CMD_PD_VOLTAGE, data)
	return c.sm.Send(pkt)
}

// RequestSensorRead requests sensor data
func (c *Controller) RequestSensorRead() error {
	pkt := serial.NewPacket(config.DEVICE_ID, config.CMD_SENSOR_READ, []uint8{config.SENSOR_TYPE_ALL})
	return c.sm.Send(pkt)
}

// processorLoop handles incoming packets and periodic sensor reads
func (c *Controller) processorLoop() {
	ticker := time.NewTicker(time.Duration(config.UPDATE_INTERVAL_MS) * time.Millisecond)
	defer ticker.Stop()

	startTime := time.Now()

	for {
		select {
		case <-c.done:
			return

		case <-ticker.C:
			// Periodically request sensor data
			c.RequestSensorRead()

		case pkt := <-c.sm.RxChan():
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

// processPacket processes an incoming packet
func (c *Controller) processPacket(pkt *serial.Packet, startTime time.Time) {
	if pkt.Cmd == config.RESP_SENSOR_DATA {
		c.processSensorData(pkt, startTime)
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
	for j := 0; j < 4; j++ {
		rawFB := (uint16(d[7+j*2]) << 8) | uint16(d[8+j*2])
		fbVolts[j] = float64(rawFB) * config.FB_VOLTAGE_SCALE
	}

	// Apply Kalman filtering
	filteredV := c.kfV.Update(volt)
	filteredI := c.kfI.Update(curr)

	// Update data storage
	c.mu.Lock()
	c.data.Voltage = filteredV
	c.data.Current = filteredI
	c.data.Temp = temp
	c.data.RawTemp = rawT
	c.data.FBVolt = fbVolts
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

	// R = R0 * (4095 - T) / T
	res := config.TEMP_R0 * float64(4095-rawTemp) / float64(rawTemp)

	// T = 1 / (ln(R/R0)/B + 1/T0) - 273.15
	logRatio := math.Log(res / config.TEMP_R0)
	invT := logRatio/config.TEMP_B + 1/config.TEMP_T0
	tempK := 1 / invT

	return tempK - 273.15
}
