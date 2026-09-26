import { useEffect, useState } from 'react'
import {
  GetDeviceState, GetProtection, ListDevices, SelectDevice, SetDeviceName, SetProtection,
} from '../wails'
import { Prot } from '../types'
import type { DeviceInfo, DeviceState, ProtectionSettings } from '../types'

const label = (d: DeviceInfo) =>
  `${d.name || `Device-${d.id.toString(16).padStart(2, '0').toUpperCase()}`} [ID:0x${d.id
    .toString(16).padStart(2, '0').toUpperCase()}]`

const FEATURES: { bit: number; name: string }[] = [
  { bit: Prot.Overcurrent, name: 'Overcurrent' },
  { bit: Prot.Overvoltage, name: 'Overvoltage' },
  { bit: Prot.Overheat, name: 'Overheat' },
  { bit: Prot.Undervoltage, name: 'Undervoltage' },
  { bit: Prot.Stall, name: 'Stall' },
]

const FIELDS: { key: keyof ProtectionSettings; name: string; unit: string }[] = [
  { key: 'maxCurrentMA', name: 'Max current', unit: 'mA' },
  { key: 'minVoltageMV', name: 'Min voltage', unit: 'mV' },
  { key: 'maxVoltageMV', name: 'Max voltage', unit: 'mV' },
  { key: 'maxTempC', name: 'Max temp', unit: '°C' },
  { key: 'stallCurrentMA', name: 'Stall current', unit: 'mA' },
  { key: 'stallTimeMS', name: 'Stall time', unit: 'ms' },
  { key: 'stallFBDelta', name: 'Stall FB delta', unit: 'µs' },
  { key: 'stallPosError', name: 'Stall pos error', unit: 'µs' },
]

// Device selection, name and protection settings (#29, #47, #49)
export default function DevicePanel() {
  const [state, setState] = useState<DeviceState | null>(null)
  const [devices, setDevices] = useState<DeviceInfo[]>([])
  const [name, setName] = useState('')
  const [prot, setProt] = useState<ProtectionSettings | null>(null)
  const [msg, setMsg] = useState<string | null>(null)
  const [busy, setBusy] = useState(false)

  const refreshState = async () => {
    const s = await GetDeviceState()
    if (s) setState(s)
    return s
  }

  useEffect(() => {
    refreshState()
    const t = setInterval(refreshState, 2000)
    return () => clearInterval(t)
  }, [])

  const run = async (what: string, f: () => Promise<unknown>) => {
    setBusy(true)
    try {
      await f()
      setMsg(`${what}: OK`)
    } catch (e) {
      setMsg(`${what}: ${String(e)}`)
    } finally {
      setBusy(false)
    }
  }

  const scan = () => run('Scan', async () => {
    setDevices((await ListDevices()) ?? [])
  })

  const select = (port: string) => run('Select', async () => {
    await SelectDevice(port)
    setProt(null)
    setTimeout(refreshState, 1500)
  })

  const loadSettings = () => run('Load', async () => {
    const s = await refreshState()
    setName(s?.device.name ?? '')
    setProt((await GetProtection()) ?? null)
  })

  const saveName = () => run('Name', async () => {
    await SetDeviceName(name)
    await refreshState()
  })

  const saveProt = () => prot && run('Protection', () => SetProtection(prot)!)

  const toggle = (bit: number) =>
    prot && setProt({ ...prot, featureMask: prot.featureMask ^ bit })

  const connected = state?.connected

  return (
    <div className="card">
      <h3>Device</h3>
      <div className="device-row">
        <span className="device-current">
          {connected && state ? `${label(state.device)} ${state.device.port}` : 'Not connected'}
          {connected && state?.device.fwVersion ? ` FW ${state.device.fwVersion}` : ''}
        </span>
      </div>
      <div className="device-row">
        <select
          value={state?.selected ?? ''}
          onChange={e => select(e.target.value)}
          disabled={busy}
        >
          <option value="">Auto</option>
          {devices.map(d => (
            <option key={d.port} value={d.port}>{`${label(d)} — ${d.port}`}</option>
          ))}
          {state?.selected && !devices.some(d => d.port === state.selected) && (
            <option value={state.selected}>{state.selected}</option>
          )}
        </select>
        <button onClick={scan} disabled={busy}>Scan</button>
        <button onClick={loadSettings} disabled={busy || !connected}>Load</button>
      </div>

      {prot && (
        <>
          <div className="device-row">
            <label>Name</label>
            <input value={name} maxLength={15} onChange={e => setName(e.target.value)} />
            <button onClick={saveName} disabled={busy}>Set</button>
          </div>
          <div className="device-row prot-features">
            {FEATURES.map(f => (
              <label key={f.bit}>
                <input
                  type="checkbox"
                  checked={(prot.featureMask & f.bit) !== 0}
                  onChange={() => toggle(f.bit)}
                />
                {f.name}
              </label>
            ))}
          </div>
          <div className="prot-grid">
            {FIELDS.map(f => (
              <label key={f.key}>
                <span>{f.name}</span>
                <input
                  type="number"
                  value={prot[f.key]}
                  onChange={e => setProt({ ...prot, [f.key]: Number(e.target.value) })}
                />
                <span className="unit">{f.unit}</span>
              </label>
            ))}
          </div>
          <button onClick={saveProt} disabled={busy}>Save protection</button>
        </>
      )}
      {msg && <div className="device-msg">{msg}</div>}
    </div>
  )
}
