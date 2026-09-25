import { useState } from 'react'
import { SetLED } from '../wails'

const LEDS = [0, 1]

export default function LEDControl() {
  const [duties, setDuties] = useState<number[]>([0, 0])

  const handleChange = (led: number, val: number) => {
    const next = [...duties]
    next[led] = val
    setDuties(next)
    SetLED(next[0], next[1])
  }

  return (
    <div className="card">
      <h3>LED</h3>
      {LEDS.map(led => (
        <div key={led} className="servo-row">
          <label>LED{led + 1}</label>
          <input
            type="range"
            min={0}
            max={255}
            value={duties[led]}
            onChange={e => handleChange(led, Number(e.target.value))}
          />
          <span className="us-val">{duties[led]}</span>
        </div>
      ))}
    </div>
  )
}
