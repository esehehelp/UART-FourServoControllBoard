import { useState } from 'react'
import { SetPDVoltage } from '../wails'

// V0.8 board limit (#38): keep in sync with PD_VOLTAGE_MIN / PD_VOLTAGE_MAX_UI
// in config/config.go. The firmware refuses anything above 16.8 V as well.
const MIN_MV = 5000
const MAX_MV = 12000

const PRESETS = [
  { label: '5V', mv: 5000 },
  { label: '9V', mv: 9000 },
  { label: '12V', mv: 12000 },
]

export default function PDControl() {
  const [active, setActive] = useState<number | null>(null)
  const [custom, setCustom] = useState('')
  const [error, setError] = useState<string | null>(null)

  const apply = async (mv: number) => {
    if (mv < MIN_MV || mv > MAX_MV) {
      setError(`${MIN_MV}–${MAX_MV} mV only`)
      return
    }
    try {
      await SetPDVoltage(mv)
      setActive(mv)
      setError(null)
    } catch (e) {
      setError(String(e))
    }
  }

  const applyCustom = () => {
    const mv = parseInt(custom, 10)
    if (!isNaN(mv)) apply(mv)
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
          placeholder={`mV (${MIN_MV}–${MAX_MV})`}
          min={MIN_MV}
          max={MAX_MV}
          value={custom}
          onChange={e => setCustom(e.target.value)}
          onKeyDown={e => e.key === 'Enter' && applyCustom()}
        />
        <button onClick={applyCustom}>Set</button>
      </div>
      {error && <div style={{ color: '#ef5350', fontSize: '0.85em' }}>{error}</div>}
    </div>
  )
}
