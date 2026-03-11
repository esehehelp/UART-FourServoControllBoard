package main

import (
	"fmt"
	"image/color"
	"log"
	"time"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"
	"uart-servo-controller/config"
	"uart-servo-controller/pkg/device"
	"uart-servo-controller/pkg/serial"
	"uart-servo-controller/pkg/ui"
)

// App manages the main application state
type App struct {
	sm        *serial.Manager
	ctrl      *device.Controller
	statusBar *ui.StatusBar
	logView   *ui.LogViewer
	servoCtl  *ui.ServoControl
	ledCtl    *ui.LEDControl
	pdCtl     *ui.PDControl
	graphCtl  *ui.GraphCanvas
	done      chan struct{}
	fyneApp   fyne.App
}

// NewApp creates a new application
func NewApp() *App {
	a := &App{
		done: make(chan struct{}),
	}

	// Create serial manager with status callback
	a.sm = serial.NewManager(func(msg, colorName string) {
		a.updateStatus(msg, a.colorFromName(colorName))
	})

	// Create device controller
	a.ctrl = device.NewController(a.sm)

	// Create UI components
	a.statusBar = ui.NewStatusBar()
	a.logView = ui.NewLogViewer()
	a.servoCtl = ui.NewServoControl(a.onServoSet)
	a.ledCtl = ui.NewLEDControl(a.onLEDSet)
	a.pdCtl = ui.NewPDControl(a.onPDVoltageSet)
	a.graphCtl = ui.NewGraphCanvas()

	return a
}

// Run starts the application
func (a *App) Run() {
	a.fyneApp = app.New()
	mainWin := a.fyneApp.NewWindow("Servo Controller")
	mainWin.Resize(fyne.NewSize(1200, 800))

	// Start serial manager
	a.sm.Start()
	defer a.Stop()

	// Build left panel
	leftPanel := container.NewBorder(
		a.statusBar.Container(), // top
		nil, // bottom
		nil, // left
		nil, // right
		container.NewVBox(
			widget.NewCard("LED Control", "", a.ledCtl.Container()),
			widget.NewCard("USB-PD", "", a.pdCtl.Container()),
			widget.NewCard("Servos", "", a.servoCtl.Container()),
		),
	)

	// Build center panel with logs
	centerPanel := container.NewVBox(
		widget.NewCard("Logs", "", a.logView.Container()),
	)

	// Build main layout: left | center | right
	mainLayout := container.NewBorder(
		nil, // top
		nil, // bottom
		leftPanel, // left
		a.graphCtl.Container(), // right
		centerPanel, // center
	)

	mainWin.SetContent(mainLayout)

	// Handle window close
	mainWin.SetOnClosed(func() {
		close(a.done)
		a.fyneApp.Quit()
	})

	// Create a timer for UI updates in the main Fyne thread
	updateTicker := time.NewTicker(time.Duration(config.UPDATE_INTERVAL_MS) * time.Millisecond)
	go func() {
		for {
			select {
			case <-a.done:
				updateTicker.Stop()
				return
			case <-updateTicker.C:
				data := a.ctrl.GetSensorData()
				a.statusBar.UpdateData(data)

				times, volts, currs, temps, fbv := a.ctrl.GetPlotData()
				a.updateGraphs(times, volts, currs, temps, fbv)
			}
		}
	}()

	mainWin.ShowAndRun()
}

// Stop gracefully stops the application
func (a *App) Stop() {
	a.ctrl.Stop()
	a.sm.Stop()
}



// updateGraphs updates the graph displays
func (a *App) updateGraphs(times, volts, currs, temps []float64, fbv [4][]float64) {
	a.graphCtl.UpdateVoltage(volts)
	a.graphCtl.UpdateCurrent(currs)
	a.graphCtl.UpdateTemperature(temps)
	a.graphCtl.UpdateFeedbackVoltages(fbv)
}

// Callbacks
func (a *App) onServoSet(ch uint8, us uint16) {
	if err := a.ctrl.SetServo(ch, us); err != nil {
		a.logView.AddLog(fmt.Sprintf("❌ Servo CH%d error: %v", ch, err))
	}
}

func (a *App) onLEDSet(duty uint8) {
	if err := a.ctrl.SetLED(duty); err != nil {
		a.logView.AddLog(fmt.Sprintf("❌ LED error: %v", err))
	} else {
		a.logView.AddLog(fmt.Sprintf("✓ LED duty set to %d", duty))
	}
}

func (a *App) onPDVoltageSet(millivolts uint16) {
	if err := a.ctrl.SetPDVoltage(millivolts); err != nil {
		a.logView.AddLog(fmt.Sprintf("❌ PD error: %v", err))
	}
	a.logView.AddLog(fmt.Sprintf("✓ PD voltage set to %dmV", millivolts))
}

func (a *App) updateStatus(msg string, col color.Color) {
	a.statusBar.UpdateStatus(msg, col)
	a.logView.AddLog(msg)
}

func (a *App) colorFromName(name string) color.Color {
	switch name {
	case "green":
		return color.NRGBA{G: 200, A: 255}
	case "red":
		return color.NRGBA{R: 255, A: 255}
	case "orange":
		return color.NRGBA{R: 255, G: 165, A: 255}
	case "gray":
		return color.NRGBA{R: 128, G: 128, B: 128, A: 255}
	default:
		return color.White
	}
}

func main() {
	log.SetFlags(log.LstdFlags | log.Lshortfile)

	application := NewApp()
	application.Run()
}
