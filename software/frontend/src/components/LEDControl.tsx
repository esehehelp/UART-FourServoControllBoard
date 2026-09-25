import { useState } from 'react'
import { SetLED } from '../wails'

export default function LEDControl() {
  const [duty1, setDuty1] = useState(0)
  const [duty2, setDuty2] = useState(0)

  const handleLED1 = (v: number) => { setDuty1(v); SetLED(0, v) }
  const handleLED2 = (v: number) => { setDuty2(v); SetLED(1, v) }

  return (
    <div className="card">
      <h3>LED</h3>
      <div className="servo-row">
        <label>LED1</label>
        <input
          type="range"
          min={0}
          max={255}
          value={duty1}
          onChange={e => handleLED1(Number(e.target.value))}
        />
        <span className="us-val">{duty1}</span>
      </div>
      <div className="servo-row">
        <label>LED2</label>
        <input
          type="range"
          min={0}
          max={255}
          value={duty2}
          onChange={e => handleLED2(Number(e.target.value))}
        />
        <span className="us-val">{duty2}</span>
      </div>
    </div>
  )
}
