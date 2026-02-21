# UART Four Servo Control Board 通信プロトコル仕様 (v2.0)

## 1. 物理層
- **通信方式**: 1-Wire 半二重 UART (Multi-drop / Ring Bus)
- **ピン割り当て**:
  - ポートA: PA2 (USART2)
  - ポートB: PA5 (USART3)
- **ボーレート**: デフォルト 115200 bps (8N1)
- **トポロジ**: リングバス（各ノードがPA2から受信し、自分宛てでなければPA5から転送、あるいはその逆を想定）

## 2. パケット構造
パケットは常にヘッダー `0xAA` で始まり、最後に `CRC8` が付与されます。

| Byte | Field | Description |
| :--- | :--- | :--- |
| 0 | Header | `0xAA` (固定) |
| 1 | Target ID | 宛先ID (0x00: Host, 0x01-0xFD: Devices, 0xFE: Proxy, 0xFF: Broadcast) |
| 2 | Source ID | 送信元ID (0x00: Host, 01-FD: Devices) |
| 3 | Command | 命令コード (後述) |
| 4 | Length | ペイロード(Data)のバイト数 |
| 5..N | Data | 命令に応じたデータ |
| N+1 | CRC8 | Byte 0 から Data末尾までの CRC8 (多項式: 0x07) |

## 3. 命令コード (Command)

### 0x01: Write (個別設定)
特定のサーボの位置を設定します。
- **Data**: `[Servo Index (1 byte)] [Position High (1 byte)] [Position Low (1 byte)]`
- **Position**: 500 - 2500 (μs)

### 0x02: Read (センサー読み出し)
電圧・温度などの情報を要求します。
- **Request Data**: `[Type (1 byte)]` (0: All, 1: Voltage, 2: Temp)
- **Response Data**: `[Type] [Val1 High] [Val1 Low] ...`
  - All (0) の場合: `[0x00] [Volt H] [Volt L] [Temp H] [Temp L]`

### 0x03: SyncWrite (全軸一括設定)
4つのサーボ位置を同時に設定します。
- **Data**: `[CH1_H] [CH1_L] [CH2_H] [CH2_L] [CH3_H] [CH3_L] [CH4_H] [CH4_L]` (8 bytes)

### 0x04: CfgWrite (設定書き換え)
デバイスの設定を変更し、Flashメモリに保存します。
- **Data**: `[Config Type (1 byte)] [Value... ]`
  - Type 0x01: Device ID 変更 (Value: 1 byte)
  - Type 0x02: デフォルト位置変更 (Value: 8 bytes)

### 0x05: Blink LED (LED制御)
LEDのデューティ比を設定します。
- **Data**: `[Duty (1 byte)]` (0-255)

### 0x06: Set Voltage (電圧設定)
USB-PD PPSを用いて出力電圧を設定します。
- **Data**: `[Volt High (1 byte)] [Volt Low (1 byte)]` (mV単位)
  - 例: 5000 (0x1388) -> 5.0V

### 0xF0: DLM (Download Mode)
ブートローダーモードへ移行し、ファームウェアアップデートを待機します。
- **Data**: なし (または解除用パスワード)

## 4. ID体系
- **0x00**: ホストデバイス (PC, メインコントローラ)
- **0x01 - 0xFD**: 個別デバイスID
- **0xFF**: ブロードキャスト (全てのデバイスが実行、返信はしない)

## 5. CRC8 計算
多項式 `x^8 + x^2 + x + 1` (0x07) を使用します。
初期値: 0x00
