import { useEffect, useState } from 'react'
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  ResponsiveContainer,
  Tooltip,
} from 'recharts'
import { EventsOn, EventsOff } from '../wails'
import type { PlotDataEvent } from '../types'

interface ChartPoint {
  i: number
  v: number
}

function toPoints(arr: number[]): ChartPoint[] {
  return arr.map((v, i) => ({ i, v }))
}

function MiniGraph({
  title,
  data,
  color,
  unit,
  domain,
}: {
  title: string
  data: ChartPoint[]
  color: string
  unit: string
  domain?: [number | 'auto', number | 'auto']
}) {
  return (
    <div className="graph-card">
      <h4>{title}</h4>
      <div style={{ flex: 1, minHeight: 0 }}>
        <ResponsiveContainer width="100%" height="100%">
          <LineChart data={data} margin={{ top: 2, right: 4, bottom: 2, left: 0 }}>
            <XAxis dataKey="i" hide />
            <YAxis
              domain={domain ?? ['auto', 'auto']}
              tick={{ fontSize: 10, fill: '#888' }}
              tickFormatter={v => `${v}${unit}`}
              width={42}
            />
            <Tooltip
              contentStyle={{ background: '#1e1e1e', border: '1px solid #333', fontSize: 11 }}
              labelFormatter={() => ''}
              formatter={(v: number) => [`${v.toFixed(3)} ${unit}`, '']}
            />
            <Line
              type="monotone"
              dataKey="v"
              stroke={color}
              dot={false}
              strokeWidth={1.5}
              isAnimationActive={false}
            />
          </LineChart>
        </ResponsiveContainer>
      </div>
    </div>
  )
}

const FB_COLORS = ['#4fc3f7', '#aed581', '#ffb74d', '#f06292']

export default function SensorGraph() {
  const [volts, setVolts] = useState<ChartPoint[]>([])
  const [currs, setCurrs] = useState<ChartPoint[]>([])
  const [temps, setTemps] = useState<ChartPoint[]>([])
  const [fbv, setFbv] = useState<ChartPoint[][]>([[], [], [], []])

  useEffect(() => {
    EventsOn('plot-data', (data: unknown) => {
      const ev = data as PlotDataEvent
      setVolts(toPoints(ev.volts))
      setCurrs(toPoints(ev.currs))
      setTemps(toPoints(ev.temps))
      setFbv(ev.fbv.map(arr => toPoints(arr)))
    })
    return () => { EventsOff('plot-data') }
  }, [])

  // Merge 4 feedback channels into one chart
  const fbvMerged = fbv[0].map((pt, i) => ({
    i: pt.i,
    fb0: fbv[0][i]?.v ?? 0,
    fb1: fbv[1][i]?.v ?? 0,
    fb2: fbv[2][i]?.v ?? 0,
    fb3: fbv[3][i]?.v ?? 0,
  }))

  return (
    <div className="graph-grid">
      <MiniGraph title="Voltage" data={volts} color="#4fc3f7" unit="V" />
      <MiniGraph title="Current" data={currs} color="#66bb6a" unit="mA" />
      <MiniGraph title="Temperature" data={temps} color="#ffa726" unit="°C" />

      {/* Feedback voltages: 4 lines on one chart */}
      <div className="graph-card">
        <h4>Feedback Voltages</h4>
        <div style={{ flex: 1, minHeight: 0 }}>
          <ResponsiveContainer width="100%" height="100%">
            <LineChart data={fbvMerged} margin={{ top: 2, right: 4, bottom: 2, left: 0 }}>
              <XAxis dataKey="i" hide />
              <YAxis
                domain={[0, 'auto']}
                tick={{ fontSize: 10, fill: '#888' }}
                tickFormatter={v => `${v}V`}
                width={36}
              />
              <Tooltip
                contentStyle={{ background: '#1e1e1e', border: '1px solid #333', fontSize: 11 }}
                labelFormatter={() => ''}
                formatter={(v: number, name: string) => [`${v.toFixed(3)} V`, name.toUpperCase()]}
              />
              {(['fb0', 'fb1', 'fb2', 'fb3'] as const).map((key, i) => (
                <Line
                  key={key}
                  type="monotone"
                  dataKey={key}
                  stroke={FB_COLORS[i]}
                  dot={false}
                  strokeWidth={1.5}
                  isAnimationActive={false}
                />
              ))}
            </LineChart>
          </ResponsiveContainer>
        </div>
      </div>
    </div>
  )
}
