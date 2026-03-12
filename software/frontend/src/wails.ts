// Bridge to Wails runtime and Go bindings.
// These globals are injected by the Wails webview at runtime.

declare global {
  interface Window {
    go: {
      main: {
        App: {
          SetServo(ch: number, us: number): Promise<void>
          SetLED(duty: number): Promise<void>
          SetPDVoltage(millivolts: number): Promise<void>
          StartCalibration(ch: number): Promise<void>
          StopCalibration(): Promise<void>
          ConfirmCalibrationPosition(): Promise<void>
        }
      }
    }
    runtime: {
      EventsOn(eventName: string, callback: (...data: unknown[]) => void): void
      EventsOff(...eventNames: string[]): void
      EventsOnce(eventName: string, callback: (...data: unknown[]) => void): void
      EventsEmit(eventName: string, ...data: unknown[]): void
    }
  }
}

const g = () => window.go?.main?.App
const rt = () => window.runtime

export const SetServo = (ch: number, us: number) => g()?.SetServo(ch, us)
export const SetLED = (duty: number) => g()?.SetLED(duty)
export const SetPDVoltage = (mv: number) => g()?.SetPDVoltage(mv)
export const StartCalibration = (ch: number) => g()?.StartCalibration(ch)
export const StopCalibration = () => g()?.StopCalibration()
export const ConfirmCalibrationPosition = () => g()?.ConfirmCalibrationPosition()

export const EventsOn = (event: string, cb: (...data: unknown[]) => void) =>
  rt()?.EventsOn(event, cb)

export const EventsOff = (...events: string[]) =>
  rt()?.EventsOff(...events)
