# UART Four Servo Control Board 通信プロトコル仕様 (v2.1)

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
電圧・温度・電流情報を要求します。
- **Request Data**: `[Type (1 byte)]` (0: All)
- **Response Data (0x82)**: `[Type] [Volt H] [Volt L] [Temp H] [Temp L] [Curr H] [Curr L]`
  - 電圧: mV単位
  - 温度: ADC生の値をベースにした計算値
  - 電流: mA単位

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
