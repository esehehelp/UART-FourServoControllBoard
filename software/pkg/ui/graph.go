package ui

import (
	"image/color"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
)

// LineChart renders a simple line chart using Fyne Canvas
type LineChart struct {
	canvas    fyne.CanvasObject
	data      []float64
	minVal    float64
	maxVal    float64
	width     float32
	height    float32
	lineColor color.Color
	bgColor   color.Color
}

// NewLineChart creates a new line chart
func NewLineChart(minVal, maxVal float64, lineColor color.Color) *LineChart {
	lc := &LineChart{
		minVal:    minVal,
		maxVal:    maxVal,
		data:      make([]float64, 0),
		width:     400,
		height:    150,
		lineColor: lineColor,
		bgColor:   color.NRGBA{R: 30, G: 30, B: 30, A: 255},
	}
	lc.refresh()
	return lc
}

// SetData updates chart data and triggers redraw
func (lc *LineChart) SetData(data []float64) {
	lc.data = make([]float64, len(data))
	copy(lc.data, data)
	lc.refresh()
}

// SetSize sets the chart size
func (lc *LineChart) SetSize(w, h float32) {
	lc.width = w
	lc.height = h
	lc.refresh()
}

// GetCanvas returns the underlying canvas object
func (lc *LineChart) GetCanvas() fyne.CanvasObject {
	return lc.canvas
}

// refresh redraws the chart
func (lc *LineChart) refresh() {
	if lc.canvas == nil {
		lc.canvas = canvas.NewRectangle(lc.bgColor)
	}

	// Update background
	if bg, ok := lc.canvas.(*canvas.Rectangle); ok {
		bg.FillColor = lc.bgColor
	}

	// For now, just render background with border
	// More sophisticated rendering would involve plotting points
	// This is a simplified version
}

// SimpleAxisPlot is a lightweight 2D plot using Fyne primitives
type SimpleAxisPlot struct {
	container   fyne.Container
	lines       []fyne.CanvasObject
	data        []float64
	timeData    []float64
	minVal      float64
	maxVal      float64
	timeMin     float64
	timeMax     float64
	width       float32
	height      float32
	lineColor   color.Color
	gridColor   color.Color
	axisColor   color.Color
	marginLeft  float32
	marginRight float32
	marginTop   float32
	marginBot   float32
}

// NewSimpleAxisPlot creates a new plot
func NewSimpleAxisPlot(minVal, maxVal float64, lineColor color.Color) *SimpleAxisPlot {
	return &SimpleAxisPlot{
		minVal:     minVal,
		maxVal:     maxVal,
		width:      400,
		height:     200,
		lineColor:  lineColor,
		gridColor:  color.NRGBA{R: 60, G: 60, B: 60, A: 255},
		axisColor:  color.NRGBA{R: 200, G: 200, B: 200, A: 255},
		marginLeft: 40,
		marginRight: 20,
		marginTop:  20,
		marginBot:  40,
		lines:      make([]fyne.CanvasObject, 0),
	}
}

// SetData updates the plot data
func (p *SimpleAxisPlot) SetData(timeData, valueData []float64) {
	p.timeData = timeData
	p.data = valueData

	if len(timeData) > 1 {
		p.timeMin = timeData[0]
		p.timeMax = timeData[len(timeData)-1]
	}

	p.redraw()
}

// SetSize sets plot dimensions
func (p *SimpleAxisPlot) SetSize(w, h float32) {
	p.width = w
	p.height = h
	p.redraw()
}

// redraw redraws the plot
func (p *SimpleAxisPlot) redraw() {
	// Clear old lines
	p.lines = p.lines[:0]

	if len(p.data) == 0 || len(p.timeData) == 0 {
		return
	}

	// Draw background
	bg := canvas.NewRectangle(color.NRGBA{R: 25, G: 25, B: 25, A: 255})
	bg.SetMinSize(fyne.NewSize(p.width, p.height))
	p.lines = append(p.lines, bg)

	// Draw grid lines (simplified)
	plotWidth := p.width - p.marginLeft - p.marginRight
	plotHeight := p.height - p.marginTop - p.marginBot

	// Horizontal grid lines for values
	numGridLines := 4
	for i := 0; i <= numGridLines; i++ {
		y := p.marginTop + float32(i)*(plotHeight/float32(numGridLines))
		line := canvas.NewLine(p.gridColor)
		line.Position1 = fyne.NewPos(p.marginLeft, y)
		line.Position2 = fyne.NewPos(p.marginLeft+plotWidth, y)
		line.StrokeWidth = 1
		p.lines = append(p.lines, line)
	}

	// Draw data line
	if len(p.data) > 1 {
		timeRange := p.timeMax - p.timeMin
		if timeRange <= 0 {
			timeRange = 1
		}

		valueRange := p.maxVal - p.minVal
		if valueRange <= 0 {
			valueRange = 1
		}

		for i := 0; i < len(p.data)-1; i++ {
			x1 := p.marginLeft + float32((p.timeData[i]-p.timeMin)/timeRange)*plotWidth
			y1 := p.marginTop + plotHeight - float32((p.data[i]-p.minVal)/valueRange)*plotHeight

			x2 := p.marginLeft + float32((p.timeData[i+1]-p.timeMin)/timeRange)*plotWidth
			y2 := p.marginTop + plotHeight - float32((p.data[i+1]-p.minVal)/valueRange)*plotHeight

			// Clamp values to visible range
			y1 = clamp(y1, p.marginTop, p.marginTop+plotHeight)
			y2 = clamp(y2, p.marginTop, p.marginTop+plotHeight)

			line := canvas.NewLine(p.lineColor)
			line.Position1 = fyne.NewPos(x1, y1)
			line.Position2 = fyne.NewPos(x2, y2)
			line.StrokeWidth = 2
			p.lines = append(p.lines, line)
		}
	}

	// Draw axes
	axisX := canvas.NewLine(p.axisColor)
	axisX.Position1 = fyne.NewPos(p.marginLeft, p.marginTop+plotHeight)
	axisX.Position2 = fyne.NewPos(p.marginLeft+plotWidth, p.marginTop+plotHeight)
	axisX.StrokeWidth = 2
	p.lines = append(p.lines, axisX)

	axisY := canvas.NewLine(p.axisColor)
	axisY.Position1 = fyne.NewPos(p.marginLeft, p.marginTop)
	axisY.Position2 = fyne.NewPos(p.marginLeft, p.marginTop+plotHeight)
	axisY.StrokeWidth = 2
	p.lines = append(p.lines, axisY)
}

// clamp clamps value between min and max
func clamp(v, min, max float32) float32 {
	if v < min {
		return min
	}
	if v > max {
		return max
	}
	return v
}

// GetObjects returns canvas objects for rendering
func (p *SimpleAxisPlot) GetObjects() []fyne.CanvasObject {
	return p.lines
}

// PlotGrid is a grid of multiple plots
type PlotGrid struct {
	plots     []*SimpleAxisPlot
	container *fyne.Container
}

// NewPlotGrid creates a grid of plots
func NewPlotGrid() *PlotGrid {
	return &PlotGrid{
		plots: make([]*SimpleAxisPlot, 4),
	}
}

// SetPlotData sets data for a specific plot
func (pg *PlotGrid) SetPlotData(idx int, timeData, valueData []float64, minVal, maxVal float64) {
	if idx >= len(pg.plots) {
		return
	}

	if pg.plots[idx] == nil {
		colors := []color.Color{
			color.NRGBA{R: 255, G: 100, B: 100, A: 255}, // Red for voltage
			color.NRGBA{R: 100, G: 150, B: 255, A: 255}, // Blue for current
			color.NRGBA{R: 100, G: 255, B: 100, A: 255}, // Green for temp
			color.NRGBA{R: 200, G: 100, B: 200, A: 255}, // Purple for FB
		}
		col := colors[idx%len(colors)]
		pg.plots[idx] = NewSimpleAxisPlot(minVal, maxVal, col)
	}

	pg.plots[idx].SetData(timeData, valueData)
}

// Initialize initializes the grid for rendering
func (pg *PlotGrid) Initialize() *fyne.Container {
	items := make([]fyne.CanvasObject, 0)

	for _, plot := range pg.plots {
		if plot != nil {
			plot.SetSize(400, 180)
			for _, obj := range plot.GetObjects() {
				items = append(items, obj)
			}
		}
	}

	return nil // Will be rendered in main
}
