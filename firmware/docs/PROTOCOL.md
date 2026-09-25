# UART Four Servo Control Board 通信プロトコル仕様 (v3.0)

> v3.0 (FW/SW V0.8.0): TTL フィールド追加 (#13)、エラー応答 `0xEE` と ACK 追加 (#51, #52)。v2.x とはパケット形式に互換性がありません。

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
| 3 | TTL | 0xXX | 残りホップ数。送信元は 16 を設定 |
| 4 | Command | 0xXX | 命令コード |
| 5 | Length | N | データ部のバイト数 |
| 6..6+N-1 | Data | ... | 命令に応じたデータ |
| 6+N | CRC8 | 0xXX | HeaderからData末尾までのCRC |

- **最大パケット長**: 128 bytes (ファームウェアの受信バッファ長)。フレーミングが 7 bytes なので Data 部 (N) は最大 **121 bytes**。
  Length に 121 を超える値を持つパケットはファームウェアで破棄されます。

### TTL とリング転送 (#13)
- パケットを新たに送信するノード (PC・各ボード) は TTL = 16 を設定します。
- 自分宛・ブロードキャスト以外のパケットを受信したノードは、TTL が 0 なら転送せずに破棄し、受信したインターフェースへ `ERR_TTL_EXPIRED` のエラー応答を返します (エラー応答 `0xEE` 自体には返しません)。
  TTL が 1 以上なら 1 減らして CRC を再計算し、次段へ転送します。
- ブロードキャスト (`0xFF`) 宛パケットは受信したデバイスで実行され、転送されません (#14)。

## 応答とエラー (#51, #52)

コマンドは処理の前に検証を行い、エラーがあれば何も変更せずにエラー応答を返します。
ブロードキャスト宛のコマンドにはエラー応答を返しません。CRC 不一致のパケットは応答せずに破棄します。

### 0xEE: エラー応答
- **Data**: 2 bytes — `[0]`: 失敗したコマンド, `[1]`: エラーコード

| Range | 分類 | コード | 名前 |
| :--- | :--- | :--- | :--- |
| `0x00` | 成功 | `0x00` | `ERR_OK` |
| `0x01–0x0F` | 汎用 | `0x01` / `0x02` / `0x03` / `0x04` | `ERR_UNKNOWN_CMD` / `ERR_BAD_LENGTH` / `ERR_BAD_CHANNEL` / `ERR_BAD_VALUE` |
| `0x10–0x1F` | 通信 | `0x10` / `0x11` / `0x12` / `0x13` | `ERR_CRC` / `ERR_TIMEOUT` / `ERR_BUFFER_OVERFLOW` / `ERR_TTL_EXPIRED` |
| `0x20–0x2F` | ハードウェア | `0x20` / `0x21` / `0x22` | `ERR_FLASH_WRITE` / `ERR_ADC` / `ERR_PWM` |
| `0x30–0x3F` | 保護 (#47) | `0x30` / `0x31` / `0x32` / `0x33` | `ERR_OVERCURRENT` / `ERR_UNDERVOLTAGE` / `ERR_OVERHEAT` / `ERR_STALL` |
| `0x40–0x4F` | 設定 | `0x40` / `0x41` | `ERR_CONFIG_INVALID` / `ERR_CAL_INVALID` |
| `0xF0–0xFF` | 予約 | — | — |

定義: `firmware/src/error_codes.h` / `software/config/config.go` (`ErrCode*`)。

### コマンドごとの応答

| コマンド | 成功時 | 主なエラー |
| :--- | :--- | :--- |
| `0x01` Write Servo | なし | BAD_LENGTH, BAD_CHANNEL, BAD_VALUE (キャリブレーション範囲外) |
| `0x02` Read Sensors | `0x82` データ | BAD_LENGTH, BAD_VALUE (type ≠ 0) |
| `0x03` SyncWrite | なし | BAD_LENGTH, BAD_VALUE (1 ch でも範囲外なら全 ch 不変) |
| `0x04` CfgWrite | `0x84` `[sub_cmd]` | BAD_LENGTH, BAD_VALUE, FLASH_WRITE |
| `0x30` LED Set | なし | BAD_LENGTH, BAD_CHANNEL |
| `0x06` PD Voltage | `0x86` `[mv_h, mv_l]` | BAD_LENGTH, BAD_VALUE (5000–16800 mV 外) |
| `0x07` Cal Save | `0x87` `[ch]` | BAD_LENGTH, BAD_CHANNEL, CAL_INVALID, FLASH_WRITE |
| `0x08` Cal Get | `0x88` データ | BAD_LENGTH, BAD_CHANNEL |
| `0x09` Servo Free | なし | BAD_LENGTH, BAD_CHANNEL (mask > 0x0F) |
| `0xA0` Ping | `0xA1` Pong | なし |
| `0xF0` DLM | なし (リセットするため) | なし |
| 未定義 (`0x05` 含む) | — | UNKNOWN_CMD |

### サーボチャンネルとコネクタの対応
チャンネル番号はタイマーチャンネルの割り当て順であり、サーボコネクタ J3 のピン順や回路図のネット名 (PWM0〜3) とは一致しません。

| J3 ピン | 回路図ネット | MCU ピン | チャンネル | タイマー |
| :--- | :--- | :--- | :--- | :--- |
| 1 | PWM0/ADC0 | PA3 | **CH2** | TIM2_CH4 |
| 2 | PWM1/ADC1 | PA1 | **CH1** | TIM2_CH2 |
| 3 | PWM2/ADC2 | PA0 | **CH0** | TIM2_CH1 |
| 4 | PWM3/ADC3 | PC1 | **CH3** | TIM1_CH2 (Full Remap) |

J3 のピン順に並べると **CH2 → CH1 → CH0 → CH3** となります ([`docs/pinassign.md`](../../docs/pinassign.md) 参照)。

## 3. 命令コード (Command)

### 0x01: Write (サーボ個別設定)
特定のチャンネルのサーボパルス幅を設定します。
電源投入直後は全チャンネルが PWM 停止 (出力 LOW、`0x09` と同じ状態) で、`0x01` / `0x03` を受信したチャンネルから駆動を開始します。
パルス幅がチャンネルごとのキャリブレーション範囲 (デフォルト 500-2500μs) の外なら `ERR_BAD_VALUE` を返し、サーボは動かしません。
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
  - `[0:1] = [0x02, Role]`: デバイスのロールを変更し、Flashに保存します。
- NewID に `0x00` (ホスト) / `0xFF` (ブロードキャスト)、Role に 0/1 以外は指定できません。
- 成功時 `0x84` `[sub_cmd]` を返します。

### ~~0x05: STATIC_LED~~ (廃止 → `0x30` に移行)

### 0x30: LED_SET (LED制御)
基板上のインジケータLEDを個別に制御します。
- **Data**: 2 bytes
  - `[0]`: Channel (0=LED1/PB12/Active-High, 1=LED2/PC3/Active-Low)
  - `[1]`: Duty (0-255, soft PWM)

### 0x06: Set Voltage (USB PD PPS設定)
USB PD PPS対応電源を使用している場合、供給電圧を変更します。
- **Data**: 2 bytes
  - `[0:1]`: Target Voltage (uint16, unit: mV)
- 受け付ける範囲は **5000–16800 mV** (#38: 16.8V = 4S LiPo 上限、3.3V LDO の入力耐圧以下)。GUI はさらに 12V までに制限しています。
- 成功時 `0x86` `[mv_h, mv_l]` を返します (電源側とのネゴシエーション完了ではなく、要求を受け付けたことを示します)。

### 0x07: Set Calibration (キャリブレーション保存)
特定チャンネルのキャリブレーションパラメータと安全範囲を Flash に保存します。
- **Data**: 13 bytes
  - `[0]`: Channel Index (0-3)
  - `[1:4]`: Slope (float32, little-endian)
  - `[5:8]`: Intercept (float32, little-endian)
  - `[9:10]`: Min Pulse (uint16, big-endian)
  - `[11:12]`: Max Pulse (uint16, big-endian)
- Slope / Intercept はファームウェアが `memcpy` で float に直接コピーするため **little-endian** です (パケット全体の既定であるビッグエンディアンの例外)。
- `Min Pulse >= Max Pulse` のデータは `ERR_CAL_INVALID` で拒否され、保存されません。
- Flash への書き込みと読み返し確認に成功すると `0x87` `[ch]` を返します。失敗時は `ERR_FLASH_WRITE`。

### 0x08: Get Calibration (キャリブレーション取得)
保存されている設定を読み出します。
- **Request Data**: 1 byte (Channel Index)
- **Response Data (0x88)**: 13 bytes (0x07 と同形式)

### 0x09: Servo Free (PWM停止)
指定チャンネルのPWM出力を停止し、サーボを脱力させます。再度動かすには `0x01` または `0x03` を送信します。
- **Data**: 1 byte (ch_mask: bit0=CH0, bit1=CH1, bit2=CH2, bit3=CH3)

### 0xA0: Ping (デバイス探索要求)
リングバス上のデバイスを探索します。動作は受信したボードのロール (`0x04` CfgWrite の Role) によって異なります。
- **Data**: なし (0 bytes)
- **ROLE_HOST (0x01) のボードが USB から受信した場合**:
  Target=`0xFF` (ブロードキャスト)、Source=自身のデバイスID で Ping を UART2 (リング下流) **のみ**へ送信し、100 ms の探索ウィンドウを開始します。
  UART4 や USB には送信しません (#14: 両方向に送るとリングの両側から届き応答が重複するため、安全側に固定)。
- **ROLE_DEVICE (0x00) のボードが受信した場合**:
  受信したインターフェースへ `0xA1` (Pong) を返信します (Target=Ping の Source、Data=`[device_id]`)。

### 0xA1: Pong (デバイス探索応答)
- **デバイス → ホストボード**: Data 1 byte (`[0]`: 応答したデバイスのID)。
  ROLE_HOST のボードは探索ウィンドウ中に受信した ID を最大 16 個まで記録します。
- **ホストボード → PC (USB)**: 探索開始から 100 ms 経過後、Target=`0x00` で送信します。
  - **Data**: N bytes (N = 発見したデバイス数、0-16)。各バイトが発見したデバイスID。

> 注: ブロードキャスト (`0xFF`) 宛パケットは受信したデバイスで実行され、リングの次段へは転送されません。そのため現状の探索で見つかるのは UART2 側の隣接デバイスのみです。

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
