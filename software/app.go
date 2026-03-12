package main

import (
	"context"
	"time"

	"github.com/wailsapp/wails/v2/pkg/runtime"
	"uart-servo-controller/config"
	"uart-servo-controller/pkg/calibration"
	"uart-servo-controller/pkg/device"
	"uart-servo-controller/pkg/serial"
)

// SensorDataEvent is emitted on the "sensor-data" event (~30fps)
type SensorDataEvent struct {
	Voltage   float64    `json:"voltage"`
	Current   float64    `json:"current"`
	Temp      float64    `json:"temp"`
	FBVolt    [4]float64 `json:"fbVolt"`
	Timestamp string     `json:"timestamp"`
}

// PlotDataEvent is emitted on the "plot-data" event (~30fps)
type PlotDataEvent struct {
	Volts []float64    `json:"volts"`
	Currs []float64    `json:"currs"`
	Temps []float64    `json:"temps"`
	FBV   [4][]float64 `json:"fbv"`
}

// StatusEvent is emitted on the "status" event when connection state changes
type StatusEvent struct {
	Msg   string `json:"msg"`
	Color string `json:"color"`
}

// CalStatusEvent is emitted on the "cal-status" event during calibration
type CalStatusEvent struct {
	State int    `json:"state"`
	Msg   string `json:"msg"`
}

// App is the Wails application struct; its exported methods are bound to JS
type App struct {
	ctx  context.Context
	sm   *serial.Manager
	ctrl *device.Controller
	cal  *calibration.StateMachine
	done chan struct{}
}

// NewApp creates an uninitialised App (startup() does real init)
func NewApp() *App {
	return &App{
		done: make(chan struct{}),
	}
}

// startup is called by Wails after the webview is ready
func (a *App) startup(ctx context.Context) {
	a.ctx = ctx

	a.sm = serial.NewManager(func(msg, color string) {
		runtime.EventsEmit(ctx, "status", StatusEvent{Msg: msg, Color: color})
	})

	a.ctrl = device.NewController(a.sm)

	a.cal = calibration.NewStateMachine(a.ctrl, func(state calibration.State, msg string) {
		runtime.EventsEmit(ctx, "cal-status", CalStatusEvent{State: int(state), Msg: msg})
	})

	a.sm.Start()

	go a.sensorLoop(ctx)
}

// shutdown is called by Wails before the process exits
func (a *App) shutdown(_ context.Context) {
	close(a.done)
	a.ctrl.Stop()
	a.sm.Stop()
}

// sensorLoop emits sensor and plot data to the frontend at ~30 fps
func (a *App) sensorLoop(ctx context.Context) {
	ticker := time.NewTicker(time.Duration(config.UPDATE_INTERVAL_MS) * time.Millisecond)
	defer ticker.Stop()

	for {
		select {
		case <-a.done:
			return
		case <-ticker.C:
			d := a.ctrl.GetSensorData()
			runtime.EventsEmit(ctx, "sensor-data", SensorDataEvent{
				Voltage:   d.Voltage,
				Current:   d.Current,
				Temp:      d.Temp,
				FBVolt:    d.FBVolt,
				Timestamp: d.Timestamp.Format("15:04:05"),
			})

			_, volts, currs, temps, fbv := a.ctrl.GetPlotData()
			runtime.EventsEmit(ctx, "plot-data", PlotDataEvent{
				Volts: volts,
				Currs: currs,
				Temps: temps,
				FBV:   fbv,
			})
		}
	}
}

// ---- Bound methods (callable from JavaScript) ----

// SetServo sets servo pulse width in microseconds (ch: 0-3, us: 0-3000)
func (a *App) SetServo(ch uint8, us uint16) error {
	return a.ctrl.SetServo(ch, us)
}

// SetLED sets LED PWM duty cycle (0-255)
func (a *App) SetLED(duty uint8) error {
	return a.ctrl.SetLED(duty)
}

// SetPDVoltage sets USB-PD negotiated voltage in millivolts
func (a *App) SetPDVoltage(millivolts uint16) error {
	return a.ctrl.SetPDVoltage(millivolts)
}

// StartCalibration begins auto-calibration for channel ch (0-3)
func (a *App) StartCalibration(ch uint8) error {
	return a.cal.Start(ch)
}

// StopCalibration cancels an in-progress calibration
func (a *App) StopCalibration() {
	a.cal.Stop()
}

// ConfirmCalibrationPosition signals that the user has moved the arm to the
// requested position. Valid only while waiting for confirmation
// (StateArmFreeMin or StateArmFreeMax).
func (a *App) ConfirmCalibrationPosition() error {
	return a.cal.Confirm()
}
