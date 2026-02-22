# CH32X035F7P6 ファームウェア仕様 (v2.1)

## 1. 周辺機能の割り当て
| 機能 | 周辺機能 | ピン / チャンネル | 備考 |
| :--- | :--- | :--- | :--- |
| **Servo CH0** | TIM2_CH1 | PA0 | |
| **Servo CH1** | TIM2_CH2 | PA1 | |
| **Servo CH2** | TIM2_CH4 | PA3 | |
| **Servo CH3** | TIM1_CH2 | PC1 | Full Remap 適用 |
| **UART_A** | USART2 | PA2 | 1-Wire モード |
| **UART_B** | USART4 | PA5 | 1-Wire モード / Partial Remap 1 |
| **V_SENSE** | ADC_IN6 | PA6 | 電圧監視 (5.1k/1k分圧) |
| **T_SENSE** | ADC_IN9 | PB1 | 温度監視 (NTC 22k) |
| **I_SENSE** | OPA2 / ADC_IN4 | PA7(+) / PA4(O) | 電流監視 (10mOhm / PGA x32) |
| **LED** | GPIO | PB12 | ステータス表示 (Active-High) |
| **1ms Tick** | TIM3 | 内部 | 時間管理・DLMカウントダウン用 |

## 2. UART 構成とリマップ
- **USART2 (PA2)**: デフォルト設定。`HDSEL` ビットにより 1-Wire モードで使用。
- **USART4 (PA5)**: `AFIO_PCFR1` の `USART4_REMAP` ビットを `001` に設定し、TX 機能を PA5 に割り当て。同様に `HDSEL` で 1-Wire 運用。
- **パケットルーティング**: 受信したパケットのターゲットIDが自ノードでない場合、受信したポート以外（UART2, UART4, USB）へパケットをブリッジします。

## 3. OPA2 と電流計測
- **構成**: PGA モード、ゲイン x32。
- **ピン競合回避**: 
  - `MODE2 = 0` により OPA2 出力を内部の PA4 (ADC_IN4) に固定し、USART2 で使用する PA2 を解放。
  - `NSEL2 = 111` により負入力を PA5 から切り離し、USART4 で使用する PA5 を解放。

## 4. 時間管理
- **TIM3**: 1ms 周期の割り込みを生成。
- **用途**: センサーデータの受信タイムアウト監視、DLM（ISPモード）移行への秒数カウント、将来的な時間ベースの制御用。
- **Delay**: 標準の `Delay_Ms` は SysTick を停止させるため、経過時間の計測には TIM3 のカウント（`g_ms_ticks`）を使用すること。

## 5. ファームウェアアップデート (DLM)
- **トリガー**: UART コマンド `0xF0`。
- **シーケンス**:
    1. `0xF0` 受信後、`g_dlm_requested` フラグをセット。
    2. LED が高速点滅 (5Hz)。
    3. 約2秒後、ソフトウェアリセットを実行し、System Bootloader (ISP) モードへ移行。
- **ツール**: `wchisp` または `WCHISPTool`。
