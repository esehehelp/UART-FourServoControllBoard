package ui

import (
	"image/color"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/container"
)

// GraphCanvas displays sensor data graphs
type GraphCanvas struct {
	voltageChart *LineChart
	currentChart *LineChart
	tempChart    *LineChart
	fbvCharts    [4]*LineChart
	box          *fyne.Container
}

// NewGraphCanvas creates a new graph canvas
func NewGraphCanvas() *GraphCanvas {
	gc := &GraphCanvas{
		voltageChart: NewLineChart(-0.5, 25, color.NRGBA{R: 100, G: 200, B: 255, A: 255}),
		currentChart: NewLineChart(-100, 5000, color.NRGBA{R: 255, G: 100, B: 100, A: 255}),
		tempChart:    NewLineChart(0, 80, color.NRGBA{R: 255, G: 150, B: 0, A: 255}),
	}

	// Create feedback voltage charts for each servo
	colors := [4]color.Color{
		color.NRGBA{R: 150, G: 255, B: 100, A: 255},
		color.NRGBA{R: 100, G: 255, B: 200, A: 255},
		color.NRGBA{R: 200, G: 100, B: 255, A: 255},
		color.NRGBA{R: 255, G: 200, B: 100, A: 255},
	}
	for i := 0; i < 4; i++ {
		gc.fbvCharts[i] = NewLineChart(0, 5, colors[i])
	}

	// Build UI
	gc.box = container.NewVBox(
		canvas.NewText("Voltage (V)", color.White),
		gc.voltageChart.GetContainer(),
		canvas.NewText("Current (mA)", color.White),
		gc.currentChart.GetContainer(),
		canvas.NewText("Temperature (°C)", color.White),
		gc.tempChart.GetContainer(),
	)

	return gc
}

// UpdateVoltage updates voltage chart
func (gc *GraphCanvas) UpdateVoltage(data []float64) {
	gc.voltageChart.SetData(data)
}

// UpdateCurrent updates current chart
func (gc *GraphCanvas) UpdateCurrent(data []float64) {
	gc.currentChart.SetData(data)
}

// UpdateTemperature updates temperature chart
func (gc *GraphCanvas) UpdateTemperature(data []float64) {
	gc.tempChart.SetData(data)
}

// UpdateFeedbackVoltages updates feedback voltage charts
func (gc *GraphCanvas) UpdateFeedbackVoltages(fbv [4][]float64) {
	for i := 0; i < 4; i++ {
		gc.fbvCharts[i].SetData(fbv[i])
	}
}

// Container returns the UI container
func (gc *GraphCanvas) Container() *fyne.Container {
	return gc.box
}

// LineChart is a simple line chart
type LineChart struct {
	data      []float64
	minVal    float64
	maxVal    float64
	lineColor color.Color
	bgColor   color.Color
	box       *fyne.Container
}

// NewLineChart creates a new line chart
func NewLineChart(minVal, maxVal float64, lineColor color.Color) *LineChart {
	lc := &LineChart{
		minVal:    minVal,
		maxVal:    maxVal,
		data:      make([]float64, 0),
		lineColor: lineColor,
		bgColor:   color.NRGBA{R: 20, G: 20, B: 20, A: 255},
	}
	lc.box = container.NewVBox(canvas.NewRectangle(lc.bgColor))
	lc.box.Objects[0].(*canvas.Rectangle).SetMinSize(fyne.NewSize(400, 120))
	return lc
}

// SetData updates chart data
func (lc *LineChart) SetData(data []float64) {
	lc.data = make([]float64, len(data))
	copy(lc.data, data)
	lc.refresh()
}

// refresh redraws the chart
func (lc *LineChart) refresh() {
	objects := []fyne.CanvasObject{
		canvas.NewRectangle(lc.bgColor),
	}

	if len(lc.data) > 1 {
		objects = append(objects, lc.drawLines()...)
	}

	lc.box.Objects = objects
	lc.box.Refresh()
}

// drawLines draws lines connecting data points
func (lc *LineChart) drawLines() []fyne.CanvasObject {
	const width = float32(400)
	const height = float32(120)
	const padding = float32(20)

	plotWidth := width - 2*padding
	plotHeight := height - 2*padding
	dataLen := float32(len(lc.data))

	lines := make([]fyne.CanvasObject, 0)

	if dataLen < 2 {
		return lines
	}

	for i := 0; i < len(lc.data)-1; i++ {
		fi := float32(i)
		fi1 := float32(i + 1)

		x1 := fi / (dataLen - 1) * plotWidth
		x2 := fi1 / (dataLen - 1) * plotWidth

		norm := func(val float64) float32 {
			if lc.maxVal <= lc.minVal {
				return plotHeight / 2
			}
			ratio := (val - lc.minVal) / (lc.maxVal - lc.minVal)
			if ratio < 0 {
				ratio = 0
			}
			if ratio > 1 {
				ratio = 1
			}
			return float32(ratio) * plotHeight
		}

		y1 := plotHeight - norm(lc.data[i])
		y2 := plotHeight - norm(lc.data[i+1])

		line := canvas.NewLine(lc.lineColor)
		line.StrokeWidth = 2
		line.Position1 = fyne.NewPos(padding+x1, padding+y1)
		line.Position2 = fyne.NewPos(padding+x2, padding+y2)
		lines = append(lines, line)
	}

	return lines
}

// GetContainer returns the UI container
func (lc *LineChart) GetContainer() *fyne.Container {
	return lc.box
}
