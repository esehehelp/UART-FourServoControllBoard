# UART Four Servo Control Board 通信プロトコル仕様 (v2.2)

## 1. 物理層
- **通信方式**: 1-Wire 半二重 UART (Ring Bus 構成)
- **ピン割り当て**:
  - **Port A (PA2)**: USART2 を使用。1-Wire モード。
  - **Port B (PA5)**: USART4 を使用。1-Wire モード (AFIOリマップ Partial 1)。
- **ボーレート**: 115200 bps (8N1)
- **トポロジ**: リングバス。受信したパケットが自デバイス宛てでない場合、もう一方の UART ポートへ自動的に転送（フォワーディング）されます。USB (CDC) からのパケットも同様に転送されます。

## 2. パケット構造
パケットは常にヘッダー `0xAA` で始まり、最後に `CRC8` が付与されます。

| Byte | Field | Description |
| :--- | :--- | :--- |
| 0 | Header | `0xAA` (固定) |
| 1 | Target ID | 宛先ID (0x00: Host, 0x01-0xFD: Devices, 0xFE: Proxy, 0xFF: Broadcast) |
| 2 | Source ID | 送信元ID (0x00: Host, 01-FD: Devices) |
| 3 | Command | 命令コード |
| 4 | Length | ペイロード(Data)のバイト数 |
| 5..N | Data | 命令に応じたデータ |
| N+1 | CRC8 | Byte 0 から Data末尾までの CRC8 (多項式: 0x07) |

## 3. 命令コード (Command)

### 0x01: Write (サーボ個別設定)
特定のサーボの位置を設定します。
- **Data**: `[Servo Index (1 byte)] [Position High (1 byte)] [Position Low (1 byte)]`
- **Position**: 500 - 2500 (μs)

### 0x02: Read (センサー読み出し)
システム全体のセンサー情報（電圧・温度・電流・サーボ状態）を要求します。
- **Request Data**: `[Type (1 byte)]` (0: All)
- **Response Data (0x82)**: 15 bytes
  - `[0]`: Type (0: All)
  - `[1:2]`: Bus Voltage (mV)
  - `[3:4]`: Board Temp (ADC Raw)
  - `[5:6]`: Total Current (mA)
  - `[7:8]`: Servo CH0 Feedback (ADC Raw)
  - `[9:10]`: Servo CH1 Feedback (ADC Raw)
  - `[11:12]`: Servo CH2 Feedback (ADC Raw)
  - `[13:14]`: Servo CH3 Feedback (ADC Raw)
- **サンプリング**: サーボフィードバックは PWM 周期の中間（10ms経過時）に一時的に GPIO をアナログ入力に切り替えて取得されます。
- **分圧回路**: サーボフィードバック端子には 2.7k (信号側) : 3.3k (GND側) の分圧回路が搭載されており、ADC 入力電圧は約 0.55 倍（V_FB * 0.55）に抑制されます。

### 0x03: SyncWrite (全軸一括設定)
4つのサーボ位置を同時に設定します。
- **Data**: `[CH1_H] [CH1_L] [CH2_H] [CH2_L] [CH3_H] [CH3_L] [CH4_H] [CH4_L]` (8 bytes)

### 0x04: CfgWrite (設定書き換え)
デバイスの設定を変更します。
- **Data**: `[Config Type (1 byte)] [Value... ]`
  - Type 0x01: Device ID 変更 (Value: 1 byte)

### 0x05: Set LED (LEDデューティ設定)
LEDの明るさを設定します。
- **Data**: `[Duty (1 byte)]` (0-255)
- **注意**: 回路は Active-High のため、255で最大照度、0で消灯となります。

### 0x06: Set Voltage (USB PD 電圧設定)
USB-PD PPSを用いて出力電圧を設定します。
- **Data**: `[Volt High (1 byte)] [Volt Low (1 byte)]` (mV単位)
  - 範囲: 5000 (5.0V) 〜 アダプタの上限まで（安全制限によりGUIでは6.0Vまで）。
  - **重要**: 電源投入時は安全のため必ず 5.0V に設定されます。

### 0xF0: DLM (Download Mode)
ブートローダーモード (System ISP) へ移行します。
- **動作**: 受信後、LEDが高速点滅し、約2秒後に自動的にリセットがかかり ISP モードに入ります。

## 4. CRC8 計算
多項式 `x^8 + x^2 + x + 1` (0x07) を使用します。初期値: 0x00
