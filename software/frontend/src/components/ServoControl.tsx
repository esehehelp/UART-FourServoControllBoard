import { useState } from 'react'
import { SetServo } from '../wails'

const CHANNELS = [0, 1, 2, 3]
const MIN = 0
const MAX = 3000
const DEFAULT = 1500

export default function ServoControl() {
  const [values, setValues] = useState<number[]>([DEFAULT, DEFAULT, DEFAULT, DEFAULT])

  const handleChange = (ch: number, us: number) => {
    setValues(prev => {
      const next = [...prev]
      next[ch] = us
      return next
    })
    SetServo(ch, us)
  }

  return (
    <div className="card">
      <h3>Servos</h3>
      {CHANNELS.map(ch => (
        <div key={ch} className="servo-row">
          <label>CH{ch}</label>
          <input
            type="range"
            min={MIN}
            max={MAX}
            value={values[ch]}
            onChange={e => handleChange(ch, Number(e.target.value))}
          />
          <span className="us-val">{values[ch]} µs</span>
        </div>
      ))}
    </div>
  )
}
