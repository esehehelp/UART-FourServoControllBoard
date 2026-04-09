# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

UART-FourServoControllBoard (UART-FSCB) is a complete embedded systems project: a compact 4-axis smart servo controller board (WCH CH32X035F7P6, RISC-V 48MHz) with USB-PD power support. It has three components:

- `firmware/` — C firmware for the CH32X035, built with PlatformIO
- `hardware/` — KiCad 9.0 PCB design
- `software/` — Cross-platform Go + Fyne GUI for device control, monitoring, and calibration

## Build Commands (software/)

```bash
cd software

~/go/bin/wails build             # Build -> build/bin/servo-controller
~/go/bin/wails dev               # Dev server with hot reload
go test -v ./test/...            # Run unit tests
```

CGO is required (serial port access). Wails v2 CLI (`~/go/bin/wails`) is required for building.

## Software Architecture

The GUI app is written in Go 1.22+ using Wails v2 (WebView2) + React/TypeScript frontend. All packages live under `software/`.

### Communication Flow

```
UI -> Controller -> SerialManager -> UART -> Firmware
                 <- packet channel <-
```

- `config/config.go` — All protocol constants, command codes (0x01–0x09, 0xF0), sensor types, Kalman filter parameters, calibration constants
- `pkg/device/packet.go` — Packet struct: `[0xAA | Target | Source | Command | Length | Data... | CRC8]`, with `Marshal()`/`Unmarshal()` and CRC8 (poly 0x07)
- `pkg/serial/manager.go` — Goroutine-based UART manager; auto-detects ports, parses packets, exposes a receive channel
- `pkg/device/controller.go` — High-level device API (`SetServo`, `SetLED`, `SetPDVoltage`, `RequestSensorRead`); holds ring buffers and Kalman state; runs a background processor goroutine
- `pkg/data/ringbuffer.go` — Thread-safe ring buffer (RWMutex, capacity 100) + 1D Kalman filter implementation
- `pkg/calibration/state_machine.go` — Multi-state calibration: coarse binary search (100µs steps) → fine search (1µs steps) → save to flash; uses ripple detection for limit finding
- `pkg/ui/widgets.go` — Fyne widgets: StatusBar, LogViewer, ServoControl (4x sliders, 0–3000µs), LEDControl, PDControl (5/9/15/20V presets), CalibrationControl
- `pkg/ui/graph.go` — Custom Fyne canvas line charts for Voltage, Current, Temperature, and 4x servo feedback voltages
- `main.go` — Window assembly, ~30 FPS update loop using `fyne.CurrentApp()` for thread-safe UI refreshes

### Concurrency Model

- SerialManager runs send/receive goroutines communicating via channels
- RingBuffer uses `sync.RWMutex` for concurrent reads from UI and writes from controller
- All Fyne UI updates must go through the Fyne event loop — use `canvas.Refresh()` or post via `fyne.CurrentApp()`; direct widget mutation from goroutines will race

### Protocol

Packet header byte is `0xAA`. Key commands:
- `0x01` Write servo (channel + pulse width µs)
- `0x02` Read sensors
- `0x03` SyncWrite (4 servos simultaneously)
- `0x05` LED control
- `0x06` USB-PD voltage
- `0x07` Save calibration to flash
- `0x08` Get calibration data
- `0xF0` Enter DLM bootloader mode

Sensor response (`0x82`): 16-bit ADC values for voltage, temperature, current, and 4 feedback voltages.

## Git / GitHub 運用ルール

### ブランチ戦略

- `main` — リリース済み安定版。直接 push 禁止。PR 経由でのみマージする。
- `V0.x` — ハードウェアリビジョン単位の開発ブランチ（例: `V0.8`）。
- `feature/<name>` — 実験的・破壊的変更用（例: `feature/usb-cdc-minimal`）。main / V0.x へのマージは動作確認後のみ。

### コミット規則

- メッセージ先頭にプレフィックスをつける: `feat:` / `fix:` / `docs:` / `hardware:` / `refactor:` / `test:`
- 末尾に `Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>` を付与（Claude が関与した場合）
- `firmware/` `software/` `hardware/` `docs/` の変更は原則として分けてコミットする
- 実機なしで firmware/src/ のコードを変更してはならない（ビルド・動作確認ができないため）

### PR ルール

- base: `main`、head: `V0.x` または `feature/*`
- PR 本文に以下を記載する:
  - **Summary**: 変更内容の箇条書き
  - **Version Matrix**: Hardware / Firmware / Software のバージョンと互換性
  - **Known Open Issues**: マージ時点で未解決の重要 issue

### Issue 運用

- issue をクローズする前に、クローズ理由をコメントとして残す
- 他 issue に統合してクローズする場合は「〇〇 に統合」と明記する
- 実装方針は issue のコメントに追記し、コードを直接変更する前に合意を得る
- 回路定数など根拠が必要な数値には必ずコメントで出典（ファイル名・行番号 or constants.md）を示す

### リリース成果物

GitHub Releases にビルド済みバイナリ・ファームウェア .hex・Gerber を配置する際は、リリースノートに以下を記載する:

```
Hardware: V0.x
Firmware: V0.x.y
Software: V0.x.y
互換性: ○ / ✕
既知の制限: ...
```

## Firmware

Built with PlatformIO targeting CH32X035F7P6. Key source files in `firmware/src/`:
- `protocol.c/h` — Packet parsing and command dispatch
- `servo.c/h` — TIM2 (CH0–2: PA0/PA1/PA3) and TIM1 (CH3: PC1) PWM output
- `adc.c/h` — Voltage (PA6), current (PA7, OPA2 PGA x32, 10mΩ shunt), NTC temp (PB1), servo feedback ADC
- `UART.c/h` — Dual 1-Wire UART on USART2 (PA2) and USART4 (PA5) at 115200 bps
- `usb_pd.c/h` — USB-PD voltage negotiation
- `config.c/h` — Flash-backed configuration storage
