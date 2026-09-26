# CH32X035F7P6 ファームウェア

ファームウェアの仕様と設計判断の記録。通信仕様は [`PROTOCOL.md`](PROTOCOL.md)、ピンは [`docs/pinassign.md`](../../docs/pinassign.md)、回路定数は [`constants.md`](constants.md)。

## 1. アーキテクチャ概要

| ファイル | 役割 |
| :-- | :-- |
| `main.c` | 初期化とメインループ (USB-PD 処理、1ms タスク、UART ポーリング) |
| `protocol.c/h` | パケットパーサ (全インターフェース共通)、コマンド実行、リング転送 |
| `app.c/h` | デバイス探索 (Ping/Pong) |
| `servo.c/h` | TIM1/TIM2 による 4ch PWM |
| `adc.c/h` | 電圧・電流 (OPA2)・温度・サーボ FB の ADC |
| `UART.c/h` | USART2 / USART4 の 1-Wire 半二重 |
| `usb_pd.c/h` | USB-PD / PPS ネゴシエーション |
| `led.c/h` | LED1/LED2 ソフト PWM |
| `config.c/h` | Flash 上の設定 (目的別構造体、256B ページ、CRC-32 検証、旧形式の移行) |
| `config_cmd.c/h` | 設定の読み書きコマンド `0x20`/`0x21` (タグ方式) |
| `protection.c/h` | 過電流・過電圧・低電圧・過熱・ストールの保護 |
| `dlm_jump.c/h` | ブートローダ (DLM) への移行 |

## 2. ペリフェラル設定

| 機能 | 周辺機能 | ピン / チャンネル | 備考 |
| :--- | :--- | :--- | :--- |
| Servo CH0 | TIM2_CH1 | PA0 | |
| Servo CH1 | TIM2_CH2 | PA1 | |
| Servo CH2 | TIM2_CH4 | PA3 | |
| Servo CH3 | TIM1_CH2 | PC1 | Full Remap |
| UART_A | USART2 | PA2 | 1-Wire (HDSEL) |
| UART_B | USART4 | PA5 | 1-Wire / Remap 001 |
| V_SENSE | ADC_IN6 | PA6 | |
| T_SENSE | ADC_IN9 | PB1 | |
| I_SENSE | OPA2 / ADC_IN4 | PA7(+) / PA4(出力) | PGA x32 |
| LED1 / LED2 | GPIO | PB12 / PC3 | ソフト PWM |
| 1ms Tick | TIM3 | 内部 | 時間管理・DLM カウントダウン |

### サーボ PWM
- 周期 20 ms (1 MHz カウント)。電源投入時はチャンネルごとの `default_pulse` (既定 0 = 出力 LOW、脱力) を適用し、既定では `0x01`/`0x03` 受信後に駆動開始。
- パルス幅はチャンネルごとのキャリブレーション範囲 (デフォルト 500–2500 µs) にクランプ。
- FB 読み取り: 20 ms 周期の 10 ms 時点で PWM ピンを一時的に AIN に切り替えて ADC 取得 (`Update_Servo_Feedback`)。切り替え中は割り込みを禁止 (約 60 µs、#12)。

### UART 構成とリマップ
- USART2 (PA2): `HDSEL` による 1-Wire 半二重。
- USART4 (PA5): `AFIO_PCFR1` の USART4 リマップを `001` にして PA5 に割り当て、1-Wire 運用。
- パケットルーティング: 自ノード宛 / ブロードキャスト以外のパケットは TTL をデクリメントしてリングの次段へ転送 (PROTOCOL.md 参照)。

### OPA2 と電流計測
- PGA モード、ゲイン x32。
- `MODE2 = 0` で OPA2 出力を内部 PA4 (ADC_IN4) に固定し、USART2 の PA2 を解放。
- `NSEL2 = 111` で負入力を PA5 から切り離し、USART4 の PA5 を解放。

### 時間管理
- TIM3: 1 ms 割り込み (`g_ms_ticks`)。
- `Delay_Ms` は SysTick を停止させるため、経過時間の計測には `g_ms_ticks` を使う。

### 設定の保存 (Flash)
- `CONFIG_FLASH_ADDR` (0x0800F000) の 256 B ページに `Config_t` を保存。
- `Config_t` (レイアウト v2、#48) = magic + version + `DeviceConfig_t` (ID・ロール・名前) + `ServoConfig_t`×4 (slope/intercept・min/max・default_pulse・calibrated) + `ProtectionConfig_t` (閾値・有効化マスク) + CRC-32。
- `FLASH_ROM_ERASE` / `FLASH_ROM_WRITE` は 256 B 単位のアドレス・長さしか受け付けない。
- 読み込み時に magic・version・CRC-32・内容 (min < max、default_pulse が範囲内、ID・ロール・名前) を検証し、不正ならデフォルトに戻す。V0.8.0 の v1 形式は ID・ロール・キャリブレーションを保持して移行する。
- 各項目は `0x20`/`0x21` (タグ方式、#49) で読み書きする。

### 保護機能 (#47, #28)
- `Protection_Tick()` をメインループから毎 ms 呼び、10 ms ごとに電圧・電流・温度を測定 (ADC 読み取り中は割り込み禁止)。温度は NTC の変換表を線形補間 (libm 不使用)。
- 3 周期連続で条件を満たすと発動し、条件が解消するまでラッチ (通知は 1 回)。動作と既定値は PROTOCOL.md「保護イベント通知」を参照。
- ストール検出は電流が全サーボ合計でしか測れないため、「目標から離れている」「動いていない」の両条件でチャンネルを特定する。キャリブレーション済みのチャンネルのみ対象で、既定では無効。
- 閾値の既定値は仮置き。実機で調整が必要。

### ファームウェアアップデート (DLM)
- トリガー: コマンド `0xF0`、または JP1 をショートして起動。
- `0xF0` 受信後 LED が高速点滅し、約 2 秒後にリセットしてシステムブートローダ (ISP) へ移行。
- 書き込み: `wchisp` / WCHISPTool、または `uploader/` (0xF0 送信 + wchisp 呼び出し)。

## 3. 設計判断の記録

- **ピン数**: TSSOP-20 (F7P6) のためリマップが必須。
- **USART2 と PWM の競合**: PA3 は USART2_RX がデフォルトだがサーボ PWM と競合するため、USART2 を 1-Wire 半二重にして RX ピンを使わない構成とした。
- **OPA2 と UART の競合**: PA2 / PA5 が OPA2 のピンと重なるため、OPA2 出力を内部モード・負入力を PGA 内部に切り替えて解放。
- **時間管理**: `Delay_Ms` (SysTick) との干渉を避けるため、時間管理を TIM3 (1 ms 割り込み) に移行。
- **プロトコル**: 全 UART/USB インターフェースで共通のパーサを使用し、リングバスを構成。ループ防止に TTL を使用 (#13)。
- **USB-PD**: PPS 対応済み。起動時は 5 V で安全を確保。
- **起動時 PWM**: 1500 µs を即時出力するとストール位置のモータが過熱するため、起動時は脱力 (#27)。

## 4. 既知の制約・注意事項

- サーボ FB は PWM ピンを ADC に一時切り替えて読むため、サンプリング中 (数十 µs) は PWM 出力が止まる。
- ブロードキャスト (`0xFF`) は受信ノードで実行され、転送されない (#14)。
- `0x82` のサーボ FB 値はキャリブレーション係数を適用した値 (`raw × slope + intercept`)。デフォルト係数 (1, 0) では ADC 生値。
