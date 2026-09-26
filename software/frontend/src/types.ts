export interface SensorDataEvent {
  voltage: number
  current: number
  temp: number
  fbVolt: [number, number, number, number]
  timestamp: string
  valid: boolean // false when the device has not answered recently
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

export interface DeviceErrorEvent {
  cmd: number
  code: number
  msg: string
}

// Keep in sync with serial.DeviceInfo (Go)
export interface DeviceInfo {
  port: string
  id: number
  name: string
  fwVersion: string
}

export interface DeviceState {
  connected: boolean
  device: DeviceInfo
  selected: string
}

// Keep in sync with device.ProtectionSettings (Go)
export interface ProtectionSettings {
  featureMask: number
  maxCurrentMA: number
  minVoltageMV: number
  maxVoltageMV: number
  maxTempC: number
  stallCurrentMA: number
  stallTimeMS: number
  stallFBDelta: number
  stallPosError: number
}

// Protection feature bits (config.PROT_*)
export const Prot = {
  Overcurrent: 0x01,
  Undervoltage: 0x02,
  Overvoltage: 0x04,
  Overheat: 0x08,
  Stall: 0x10,
} as const

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
