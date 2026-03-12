import { useEffect, useRef, useState } from 'react'
import {
  EventsOn,
  EventsOff,
  StartCalibration,
  StopCalibration,
  ConfirmCalibrationPosition,
} from '../wails'
import type { CalStatusEvent } from '../types'
import { CalState } from '../types'

const STATE_LABELS: Record<number, string> = {
  [CalState.Idle]:       'Idle',
  [CalState.Init]:       'Initializing',
  [CalState.ArmFreeMin]: 'Move arm → MIN',
  [CalState.ArmFreeMax]: 'Move arm → MAX',
  [CalState.Saving]:     'Saving',
  [CalState.Error]:      'Error',
}

export default function CalibrationPanel() {
  const [ch, setCh] = useState(0)
  const [calState, setCalState] = useState<number>(CalState.Idle)
  const [log, setLog] = useState<string[]>([])
  const logRef = useRef<HTMLDivElement>(null)

  useEffect(() => {
    EventsOn('cal-status', (data: unknown) => {
      const ev = data as CalStatusEvent
      setCalState(ev.state)
      setLog(prev => [...prev.slice(-49), ev.msg])
    })
    return () => { EventsOff('cal-status') }
  }, [])

  // Auto-scroll log to bottom
  useEffect(() => {
    if (logRef.current) {
      logRef.current.scrollTop = logRef.current.scrollHeight
    }
  }, [log])

  const running = calState !== CalState.Idle && calState !== CalState.Error
  const waitingConfirm = calState === CalState.ArmFreeMin || calState === CalState.ArmFreeMax
  const confirmLabel = calState === CalState.ArmFreeMin ? 'Confirm MIN' : 'Confirm MAX'

  return (
    <div className="card">
      <h3>Position Calibration</h3>

      {/* Channel selector */}
      <div className="cal-ch-row">
        {[0, 1, 2, 3].map(c => (
          <button
            key={c}
            className={ch === c ? 'active' : ''}
            onClick={() => setCh(c)}
            disabled={running}
          >
            CH{c}
          </button>
        ))}
      </div>

      {/* Start / Stop */}
      <div className="cal-actions">
        <button
          className="primary"
          onClick={() => { setLog([]); StartCalibration(ch) }}
          disabled={running}
        >
          Start
        </button>
        <button
          className="danger"
          onClick={() => StopCalibration()}
          disabled={!running}
        >
          Stop
        </button>
        <span style={{ marginLeft: 'auto', fontSize: '11px', color: 'var(--text-dim)' }}>
          {STATE_LABELS[calState] ?? 'Unknown'}
        </span>
      </div>

      {/* Confirm button — only visible when arm is free */}
      {waitingConfirm && (
        <button
          className="primary"
          style={{ width: '100%', marginBottom: '8px', padding: '6px' }}
          onClick={() => ConfirmCalibrationPosition()}
        >
          {confirmLabel}
        </button>
      )}

      {/* Status log */}
      <div className="cal-log" ref={logRef}>
        {log.length === 0
          ? <span style={{ color: 'var(--text-dim)' }}>—</span>
          : log.map((line, i) => <div key={i}>{line}</div>)
        }
      </div>
    </div>
  )
}
