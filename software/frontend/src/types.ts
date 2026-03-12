export interface SensorDataEvent {
  voltage: number
  current: number
  temp: number
  fbVolt: [number, number, number, number]
  timestamp: string
}

export interface PlotDataEvent {
  volts: number[]
  currs: number[]
  temps: number[]
  fbv: [number[], number[], number[], number[]]
}

export interface StatusEvent {
  msg: string
  color: string
}

export interface CalStatusEvent {
  state: number
  msg: string
}

export const CalState = {
  Idle: 0,
  Init: 1,
  ArmFreeMin: 2,
  ArmFreeMax: 3,
  Saving: 4,
  Error: 5,
} as const
