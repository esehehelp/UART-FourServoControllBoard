import { useState } from 'react'
import { SetLED } from '../wails'

export default function LEDControl() {
  const [duty, setDuty] = useState(0)

  const handleChange = (val: number) => {
    setDuty(val)
    SetLED(val)
  }

  return (
    <div className="card">
      <h3>LED</h3>
      <div className="servo-row">
        <label>PWM</label>
        <input
          type="range"
          min={0}
          max={255}
          value={duty}
          onChange={e => handleChange(Number(e.target.value))}
        />
        <span className="us-val">{duty}</span>
      </div>
    </div>
  )
}
