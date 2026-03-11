package ui

import (
	"fmt"
	"image/color"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"
	"uart-servo-controller/pkg/device"
)

// StatusBar displays connection status and sensor readings
type StatusBar struct {
	statusLabel *canvas.Text
	dataLabel   *canvas.Text
	box         *fyne.Container
}

// NewStatusBar creates a new status bar
func NewStatusBar() *StatusBar {
	sb := &StatusBar{
		statusLabel: canvas.NewText("Connecting...", color.White),
		dataLabel:   canvas.NewText("V: -- I: -- T: --", color.White),
	}
	sb.statusLabel.TextSize = 14
	sb.dataLabel.TextSize = 12

	sb.box = container.NewVBox(sb.statusLabel, sb.dataLabel)
	return sb
}

// UpdateStatus updates the status label with color
func (sb *StatusBar) UpdateStatus(msg string, col color.Color) {
	sb.statusLabel.Text = msg
	sb.statusLabel.Color = col
	sb.statusLabel.Refresh()
}

// UpdateData updates the sensor data label
func (sb *StatusBar) UpdateData(data device.SensorData) {
	sb.dataLabel.Text = fmt.Sprintf("V: %.2fV  I: %.0fmA  T: %.1f°C", 
		data.Voltage, data.Current, data.Temp)
	sb.dataLabel.Refresh()
}

// Container returns the UI container
func (sb *StatusBar) Container() *fyne.Container {
	return sb.box
}

// LogViewer displays log messages
type LogViewer struct {
	richText  *widget.RichText
	scroll    *container.Scroll
	box         *fyne.Container
}

// NewLogViewer creates a new log viewer
func NewLogViewer() *LogViewer {
	richText := widget.NewRichTextFromMarkdown("")
	scroll := container.NewScroll(richText)
	scroll.SetMinSize(fyne.NewSize(200, 200))

	lv := &LogViewer{
		richText: richText,
		scroll:   scroll,
		box:      container.NewVBox(scroll),
	}
	return lv
}

// AddLog adds a log message
func (lv *LogViewer) AddLog(msg string) {
	current := lv.richText.String()
	lv.richText.ParseMarkdown(current + msg + "\n")
}

// Clear clears the log
func (lv *LogViewer) Clear() {
	lv.richText.ParseMarkdown("")
}

// Container returns the UI container
func (lv *LogViewer) Container() *fyne.Container {
	return lv.box
}

// GraphCanvas is a placeholder for graph rendering
type GraphCanvas struct {
	rect *canvas.Rectangle
	box         *fyne.Container
}

// NewGraphCanvas creates a new graph canvas
func NewGraphCanvas() *GraphCanvas {
	rect := canvas.NewRectangle(color.NRGBA{G: 40, B: 40, A: 255})
	rect.SetMinSize(fyne.NewSize(400, 600))

	gc := &GraphCanvas{
		rect: rect,
		box:  container.NewVBox(rect),
	}
	return gc
}

// Container returns the UI container
func (gc *GraphCanvas) Container() *fyne.Container {
	return gc.box
}

// ServoControl provides servo slider controls
type ServoControl struct {
	sliders [4]*widget.Slider
	labels  [4]*canvas.Text
	onSet   func(ch uint8, us uint16)
	box         *fyne.Container
}

// NewServoControl creates servo control widgets
func NewServoControl(onSet func(ch uint8, us uint16)) *ServoControl {
	sc := &ServoControl{onSet: onSet}

	items := []fyne.CanvasObject{}

	for i := 0; i < 4; i++ {
		ch := uint8(i)
		label := canvas.NewText(fmt.Sprintf("CH%d: 1500μs", i), color.White)
		label.TextSize = 12

		slider := widget.NewSlider(0, 3000)
		slider.Value = 1500
		slider.OnChanged = func(val float64) {
			us := uint16(val)
			label.Text = fmt.Sprintf("CH%d: %dμs", ch, us)
			label.Refresh()
			if onSet != nil {
				onSet(ch, us)
			}
		}

		sc.sliders[i] = slider
		sc.labels[i] = label

		items = append(items, label, slider)
	}

	sc.box = container.NewVBox(items...)
	return sc
}

// Container returns the UI container
func (sc *ServoControl) Container() *fyne.Container {
	return sc.box
}

// LEDControl provides LED brightness control
type LEDControl struct {
	slider   *widget.Slider
	label    *canvas.Text
	onSet    func(duty uint8)
	box         *fyne.Container
}

// NewLEDControl creates LED control widget
func NewLEDControl(onSet func(duty uint8)) *LEDControl {
	lc := &LEDControl{onSet: onSet}

	lc.label = canvas.NewText("LED: 128", color.White)
	lc.label.TextSize = 14

	lc.slider = widget.NewSlider(0, 255)
	lc.slider.Value = 128
	lc.slider.OnChanged = func(val float64) {
		duty := uint8(val)
		lc.label.Text = fmt.Sprintf("LED: %d/255", duty)
		lc.label.Refresh()
		if onSet != nil {
			onSet(duty)
		}
	}

	lc.box = container.NewVBox(
		canvas.NewText("LED Control", color.White),
		lc.label,
		lc.slider,
	)

	return lc
}

// Container returns the UI container
func (lc *LEDControl) Container() *fyne.Container {
	return lc.box
}

// PDControl provides USB-PD voltage selection
type PDControl struct {
	voltageSelect *widget.Select
	customEntry   *widget.Entry
	applyBtn      *widget.Button
	onSet         func(millivolts uint16)
	box         *fyne.Container
}

// NewPDControl creates USB-PD control widget
func NewPDControl(onSet func(millivolts uint16)) *PDControl {
	pc := &PDControl{onSet: onSet}

	pc.voltageSelect = widget.NewSelect(
		[]string{"5V (5000mV)", "9V (9000mV)", "15V (15000mV)", "20V (20000mV)", "Custom"},
		func(s string) {},
	)
	pc.voltageSelect.PlaceHolder = "Select voltage"

	pc.customEntry = widget.NewEntry()
	pc.customEntry.SetPlaceHolder("Custom voltage (mV)")

	pc.applyBtn = widget.NewButton("Apply", func() {
		var mv uint16
		switch pc.voltageSelect.Selected {
		case "5V (5000mV)":
			mv = 5000
		case "9V (9000mV)":
			mv = 9000
		case "15V (15000mV)":
			mv = 15000
		case "20V (20000mV)":
			mv = 20000
		case "Custom":
			var customMV int
			fmt.Sscanf(pc.customEntry.Text, "%d", &customMV)
			mv = uint16(customMV)
		default:
			return
		}
		if onSet != nil {
			onSet(mv)
		}
	})

	pc.box = container.NewVBox(
		canvas.NewText("USB-PD Voltage", color.White),
		pc.voltageSelect,
		pc.customEntry,
		pc.applyBtn,
	)

	return pc
}

// Container returns the UI container
func (pc *PDControl) Container() *fyne.Container {
	return pc.box
}
