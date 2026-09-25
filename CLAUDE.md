# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

UART-FourServoControllBoard (UART-FSCB) is a complete embedded systems project: a compact 4-axis smart servo controller board (WCH CH32X035F7P6, RISC-V 48MHz) with USB-PD power support. It has three components:

- `firmware/` — C firmware for the CH32X035, built with PlatformIO
- `hardware/` — KiCad 9.0 PCB design
- `software/` — Cross-platform Go + Wails (React/TypeScript frontend) GUI for device control, monitoring, and calibration

## Build Commands (software/)

```bash
cd software

make build-linux     # wails build -> bin/servo-controller
make build-win       # wails build -platform windows/amd64 -> bin/servo-controller.exe
make dev             # wails dev (hot reload)
make test            # go test ./pkg/... ./test/...
make clean           # Remove build artifacts

./build.sh [linux|windows|all|test|dev|clean]
```

Requires the Wails v2 CLI (`~/go/bin/wails`) and npm. CGO is required (serial port access).
`go vet .` on the main package needs `frontend/dist` (run `npm run build` in `frontend/` first).

## Software Architecture

The GUI app is written in Go using Wails v2; the UI is React + TypeScript under `software/frontend/`.

### Communication Flow

```
React UI -(Wails binding)-> App -> Controller -> serial.Manager -> USB-CDC -> Firmware
React UI <-(Wails events)-- App <- Controller <- rx channel     <-
```

- `config/config.go` — Protocol constants, command codes (0x01–0x09, 0xF0), max packet size, sensor scale factors (see firmware/docs/constants.txt), Kalman filter and calibration parameters
- `pkg/serial/manager.go` — Packet struct `[0xAA | Target | Source | Command | Length | Data... | CRC8]` with `Marshal()`/`Unmarshal()` and CRC8 (poly 0x07); port auto-detection (WCH VID first) and probing; receive goroutine feeding a buffered channel. `pkg/device/packet.go` only aliases these — do not duplicate the CRC logic
- `pkg/device/controller.go` — High-level device API (`SetServo`, `SetLED(duty1, duty2)`, `SetPDVoltage`, `ServoFree`, `RequestSensorRead`); parses 0x82 sensor data, holds ring buffers and Kalman state, marks data invalid after `SENSOR_TIMEOUT_MS`
- `pkg/data/ringbuffer.go` — Thread-safe ring buffer (RWMutex, capacity 100) + 1D Kalman filter implementation
- `pkg/calibration/state_machine.go` — Manual position calibration: center → PWM off, user confirms min → user confirms max → compute slope/intercept → CMD 0x07 (floats little-endian)
- `app.go` — Methods bound to JS and the ~30 FPS `sensor-data` / `plot-data` event loop; `status` / `cal-status` events
- `frontend/src/` — React components: StatusBar, ServoControl (500–2500µs), LEDControl (LED1/LED2), PDControl (5/9/15/20V presets + custom), CalibrationPanel, SensorGraph. `wails.ts` declares the bound Go methods — keep it in sync with `app.go`

### Concurrency Model

- serial.Manager runs the receive loop in a goroutine and delivers packets via a channel (closed when the manager stops)
- RingBuffer uses `sync.RWMutex` for concurrent reads from the UI loop and writes from the controller
- The frontend is updated only through Wails events emitted from `app.go`

### Protocol

Packet header byte is `0xAA`. Key commands:
- `0x01` Write servo (channel + pulse width µs)
- `0x02` Read sensors
- `0x03` SyncWrite (4 servos simultaneously)
- `0x05` LED control
- `0x06` USB-PD voltage
- `0x07` Save calibration to flash
- `0x08` Get calibration data
- `0x09` Servo free (PWM off, channel mask)
- `0xA0` / `0xA1` Ping / Pong (ring-bus device discovery)
- `0xF0` Enter DLM bootloader mode

Max packet length is 128 bytes (data ≤ 122). Full spec: `firmware/docs/PROTOCOL.md`.

Sensor response (`0x82`): 16-bit ADC values for voltage, temperature, current, and 4 feedback voltages.

## Firmware

Built with PlatformIO targeting CH32X035F7P6. Key source files in `firmware/src/`:
- `protocol.c/h` — Packet parsing and command dispatch
- `servo.c/h` — TIM2 (CH0–2: PA0/PA1/PA3) and TIM1 (CH3: PC1) PWM output
- `adc.c/h` — Voltage (PA6), current (PA7, OPA2 PGA x32, 10mΩ shunt), NTC temp (PB1), servo feedback ADC
- `UART.c/h` — Dual 1-Wire UART on USART2 (PA2) and USART4 (PA5) at 115200 bps
- `usb_pd.c/h` — USB-PD voltage negotiation
- `config.c/h` — Flash-backed configuration storage (one 256-byte page via `FLASH_ROM_ERASE`/`FLASH_ROM_WRITE`, CRC-32 verified on load)

No RISC-V toolchain may be available; `gcc -fsyntax-only -Isrc -Ilib/Drivers/inc -DCH32X035 src/<file>.c` works as a host-side syntax check for most files.
