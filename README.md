# UART-FourServoControllBoard

## 概要 (Overview)

**UART-FourServoControllBoard** (以下UART-FSCB) は、わずか **22.82mm x 23.62mm** のサイズに、ロボットアーム制御に必要な機能をおおよそ詰め込んだ4軸スマートサーボコントローラ
従来のPCA9685等の単なるPWMドライバとは異なり、**電流・電圧・温度監視**、**USB-PD** 機能を搭載。MG996Rなどのハイパワーサーボ使用時でも、電流値と消費電力を監視しながら安全に駆動することが可能

## 特徴 (Features)

* **超小型サイズ:** 22.82mm x 23.62mm
* **USB PD対応:** CCピン制御により、モバイルバッテリーやPD充電器から電源を直接引き出し可能
  * 固定電圧 PDO に加え PPS (APDO) による電圧指定に対応 (`0x06` コマンド, mV 単位)
  * USB (CDC) 経由の通信対応
  * DLM (ブートローダ) 移行: `0xF0` コマンド、または背面の JP1 (DLM_JMP) をショート (R9 5.1k 経由で D+ をプルアップ)
    * USB経由の書き込み可能
* **電流監視機能:**
  * ローサイド電流検知回路を搭載（CH32X035内蔵OPA + 10mΩシャント抵抗）
  * サーボの負荷状態をモニタリングし、過負荷時の緊急停止などが実装可能
  * 停止閾値などは別途ソフトウェアで書き込み可能
* **電圧監視機能**
  * 分圧抵抗によって電圧を監視可能
  * PPSと組み合わせたフィードバック制御も行う予定
* **温度監視**
  * NTCサーミスタによってVBUS-GNDに近い位置の温度計測が可能
* **PC用GUI** (`software/`)
  * Go + Wails (React) 製。USB 接続したボードを自動検出
  * 電圧・電流・温度・サーボフィードバックのリアルタイムプロット
  * サーボ 4ch / LED 2ch / USB-PD 電圧の操作、サーボ位置キャリブレーション
* **MCU:** WCH CH32X035F7P6 (RISC-V, 48MHz)
* **制御IF:** 1Wire-UART, USB
* **その他:**
  * 外部電源入出力ピンヘッダx2 (バッテリー駆動対応、BMS無し)
  * デイジーチェーン対応 (1Wire-UARTかつ電源ピン仕様時)
  * デバック用1.27ピンヘッダ搭載
  * 片面実装
  * 手ハンダ可能 (難易度は高い)
  * XC6206 LDOによる安定した3.3V系電源
    * より高耐圧な互換LDOによるさらなる高電圧対応

## 仕様 (Specifications)

| 項目 | 内容 |
| :--- | :--- |
| **MCU** | WCH CH32X035F7P6 (TSSOP-20) |
| **入力電圧** | USB VBUS (5V-20V) または EXT_IN (5V〜20V) |
| **ロジック電圧** | 3.3V (XC6206 LDO内蔵) |
| **出力チャンネル** | 4PWM |
| **通信** | 1-Wire UARTx2 (Default: 115200bps, 8N1) |
| **電流検知** | 0 〜 6.8A (Full Scale) |
| **コンデンサ** | 100uF / 35V (Polymer) + 22uF (Ceramic) |
| **基板サイズ** | 22.86mm x 23.62mm |

### サーボチャンネルとコネクタ

チャンネル番号はサーボコネクタ J3 のピン順・回路図のネット名 (PWM0〜3) と一致しません (J3 の 1 番ピンから **CH2 → CH1 → CH0 → CH3**)。

| J3 ピン | 回路図ネット | MCU ピン | チャンネル |
| :--- | :--- | :--- | :--- |
| 1 | PWM0/ADC0 | PA3 | CH2 |
| 2 | PWM1/ADC1 | PA1 | CH1 |
| 3 | PWM2/ADC2 | PA0 | CH0 |
| 4 | PWM3/ADC3 | PC1 | CH3 |

ピンアサインの詳細は [`docs/pinassign.md`](docs/pinassign.md)、回路定数は [`firmware/docs/constants.md`](firmware/docs/constants.md)。

## リポジトリ構成

| ディレクトリ | 内容 |
| :--- | :--- |
| `firmware/` | CH32X035 用ファームウェア (PlatformIO)。通信仕様は [`firmware/docs/PROTOCOL.md`](firmware/docs/PROTOCOL.md) |
| `software/` | PC 用 GUI (Go + Wails)。詳細は [`software/README.md`](software/README.md) |
| `hardware/` | KiCad 9.0 設計データ |

## 開発環境 (Development)

* **IDE:** Goland, CLion
* **Firmware:** PlatformIO (`platform = ch32v`)
* **Writer:** WCH-LinkE、または USB 経由 (DLM + wchisp)

### ファームウェアのビルド・書き込み

```bash
cd firmware
pio run                 # ビルド
pio run -t upload       # WCH-LinkE 等で書き込み

# USB 経由 (0xF0 で DLM に移行してから wchisp で書き込み)
python test/dlm_upload.py <PORT> .pio/build/genericCH32X035F7P6/firmware.bin
```

### GUI のビルド

```bash
cd software
~/go/bin/wails build    # -> build/bin/servo-controller (Wails CLI が必要)
go test ./pkg/... ./test/...   # Go ユニットテスト
```

## 設計データ (Design Data)

* **EDA:** KiCad 9.0
* **PCB:** 2層基板 (1.6mm厚推奨)
