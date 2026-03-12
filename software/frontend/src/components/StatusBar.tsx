import { useEffect, useState } from 'react'
import { EventsOn, EventsOff } from '../wails'
import type { SensorDataEvent, StatusEvent } from '../types'

const DOT_COLORS: Record<string, string> = {
  green: '#66bb6a',
  red: '#ef5350',
  orange: '#ffa726',
  gray: '#888',
}

export default function StatusBar() {
  const [status, setStatus] = useState<StatusEvent>({ msg: 'Disconnected', color: 'gray' })
  const [sensor, setSensor] = useState<SensorDataEvent | null>(null)

  useEffect(() => {
    EventsOn('status', (data: unknown) => setStatus(data as StatusEvent))
    EventsOn('sensor-data', (data: unknown) => setSensor(data as SensorDataEvent))
    return () => { EventsOff('status', 'sensor-data') }
  }, [])

  const dot = DOT_COLORS[status.color] ?? '#888'

  return (
    <div className="status-bar">
      <div className="status-item">
        <span className="status-dot" style={{ background: dot }} />
        <span>{status.msg}</span>
      </div>
      {sensor && (
        <>
          <div className="status-item">
            <span>V:</span>
            <span className="val">{sensor.voltage.toFixed(2)} V</span>
          </div>
          <div className="status-item">
            <span>I:</span>
            <span className="val">{sensor.current.toFixed(1)} mA</span>
          </div>
          <div className="status-item">
            <span>T:</span>
            <span className="val">{sensor.temp.toFixed(1)} °C</span>
          </div>
          {sensor.fbVolt.map((v, i) => (
            <div key={i} className="status-item">
              <span>FB{i}:</span>
              <span className="val">{v.toFixed(3)} V</span>
            </div>
          ))}
          <div className="status-item" style={{ marginLeft: 'auto', color: 'var(--text-dim)' }}>
            {sensor.timestamp}
          </div>
        </>
      )}
    </div>
  )
}
