import { useState } from 'react'
import { SetPDVoltage } from '../wails'

const PRESETS = [
  { label: '5V', mv: 5000 },
  { label: '9V', mv: 9000 },
  { label: '15V', mv: 15000 },
  { label: '20V', mv: 20000 },
]

export default function PDControl() {
  const [active, setActive] = useState<number | null>(null)
  const [custom, setCustom] = useState('')

  const apply = (mv: number) => {
    setActive(mv)
    SetPDVoltage(mv)
  }

  const applyCustom = () => {
    const mv = parseInt(custom, 10)
    if (!isNaN(mv) && mv > 0) {
      setActive(mv)
      SetPDVoltage(mv)
    }
  }

  return (
    <div className="card">
      <h3>USB-PD Voltage</h3>
      <div className="pd-presets">
        {PRESETS.map(p => (
          <button
            key={p.mv}
            onClick={() => apply(p.mv)}
            style={active === p.mv ? { borderColor: 'var(--accent)', color: 'var(--accent)' } : {}}
          >
            {p.label}
          </button>
        ))}
      </div>
      <div className="pd-custom">
        <input
          type="number"
          placeholder="mV"
          value={custom}
          onChange={e => setCustom(e.target.value)}
          onKeyDown={e => e.key === 'Enter' && applyCustom()}
        />
        <button onClick={applyCustom}>Set</button>
      </div>
    </div>
  )
}
