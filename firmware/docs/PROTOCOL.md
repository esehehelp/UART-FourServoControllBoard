# UART Four Servo Control Board 通信プロトコル仕様 (v2.3)

## 1. 物理層
- **通信方式**: 1-Wire 半二重 UART (Ring Bus 構成)
- **インターフェース**: USB (CDC), UART2, UART4
- **ボーレート**: 115200 bps (8N1)
- **デフォルトデバイスID**: `0x01` (ホストは `0x00`, ブロードキャストは `0xFF`)

## 2. パケット構造
すべてのパケットは以下の形式で構成されます。数値は特に指定がない限りビッグエンディアン（Big-Endian）です。

| Offset | Field | Value | Description |
| :--- | :--- | :--- | :--- |
| 0 | Header | 0xAA | パケット開始フラグ |
| 1 | Target ID | 0xXX | 送信先デバイスID |
| 2 | Source ID | 0xXX | 送信元デバイスID |
| 3 | Command | 0xXX | 命令コード |
| 4 | Length | N | データ部のバイト数 |
| 5..5+N-1 | Data | ... | 命令に応じたデータ |
| 5+N | CRC8 | 0xXX | HeaderからData末尾までのCRC |

## 3. 命令コード (Command)

### 0x01: Write (サーボ個別設定)
特定のチャンネルのサーボパルス幅を設定します。
- **Data**: 3 bytes
  - `[0]`: Channel Index (0-3)
  - `[1:2]`: Pulse Width (uint16, 500-2500μs)

### 0x02: Read (センサー読み出し)
各種センサー情報とサーボの現在位置を取得します。
- **Request Data**: 1 byte (0x00: All)
- **Response Data (0x82)**: 15 bytes
  - `[0]`: Data Type (0x00)
  - `[1:2]`: Bus Voltage (ADC Raw Value)
  - `[3:4]`: MCU Temperature (ADC Raw Value)
  - `[5:6]`: Total Current (ADC Raw Value)
  - `[7:14]`: Servo 0-3 Feedback (μs, 16-bit x 4 channels)
    - 保存されたキャリブレーション係数を用いて計算された値が返ります。

### 0x03: SyncWrite (全サーボ同時設定)
全4チャンネルのパルス幅を一度に設定します。
- **Data**: 8 bytes
  - `[0:1]`: CH0 Pulse, `[2:3]`: CH1 Pulse, `[4:5]`: CH2 Pulse, `[6:7]`: CH3 Pulse (all uint16)

### 0x04: CfgWrite (システム設定)
デバイス自体の設定を変更します。
- **Sub-Commands**:
  - `[0:1] = [0x01, NewID]`: デバイスIDを変更し、Flashに保存します。

### 0x05: STATIC_LED (LED制御)
基板上のインジケータLEDの輝度を設定します。
- **Data**: 1 byte (0-255: PWM Duty)

### 0x06: Set Voltage (USB PD PPS設定)
USB PD PPS対応電源を使用している場合、供給電圧を変更します。
- **Data**: 2 bytes
  - `[0:1]`: Target Voltage (uint16, unit: mV)

### 0x07: Set Calibration (キャリブレーション保存)
特定チャンネルのキャリブレーションパラメータと安全範囲を Flash に保存します。
- **Data**: 13 bytes
  - `[0]`: Channel Index (0-3)
  - `[1:4]`: Slope (float32, little-endian)
  - `[5:8]`: Intercept (float32, little-endian)
  - `[9:10]`: Min Pulse (uint16, big-endian)
  - `[11:12]`: Max Pulse (uint16, big-endian)

### 0x08: Get Calibration (キャリブレーション取得)
保存されている設定を読み出します。
- **Request Data**: 1 byte (Channel Index)
- **Response Data (0x88)**: 13 bytes (0x07 と同形式)

### 0xF0: DLM (Download Mode)
ブートローダー(ISP)モードへ移行するためのカウントダウンを開始します。

## 4. CRC8 計算
- **多項式**: `0x07` (x^8 + x^2 + x + 1)
- **初期値**: `0x00`
- **対象**: Header から Data の末尾まで

```c
uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x07;
            else crc <<= 1;
        }
    }
    return crc;
}
```
