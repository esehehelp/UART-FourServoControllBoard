# ピンアサイン (CH32X035F7P6, TSSOP-20)

ピンアサインの **Single Source of Truth**。回路図 (`hardware/4servoboard.kicad_sch`) のネットとファームウェアソースから検証済み。
回路定数は [`firmware/docs/constants.md`](../firmware/docs/constants.md) を参照。

## MCU ピン

| Pin | Port | 回路図ネット | 用途 | ペリフェラル | ADC | ソース | 備考 |
| :-- | :-- | :-- | :-- | :-- | :-- | :-- | :-- |
| 1 | PB12 | MCU_LED1 | LED1 | GPIO 出力 | — | `led.c` | Active-High (PB12 → R6 1k → D2 → GND) |
| 2 | PC14 | CC1 | USB-PD CC1 | USB-PD | — | `usb_pd.c` | R3 5.1k プルダウン。PD / PPS ネゴシエーション実装済み |
| 3 | PC15 | CC2 | USB-PD CC2 | USB-PD | — | `usb_pd.c` | R4 5.1k プルダウン |
| 4 | PC3 | MCU_LED2 | LED2 | GPIO 出力 | — | `led.c` | Active-Low (VDD → R5 1k → D1 → PC3)。NRST と兼用のため RST_MOD=3、CMP1 無効化が必要 |
| 5 | PC1 | PWM3/ADC3 | サーボ CH3 PWM + FB | TIM1_CH2 (Full Remap) | ADC_IN11 | `servo.c` | AFIO PCFR1[17:15]=011 |
| 6 | PA0 | PWM2/ADC2 | サーボ CH0 PWM + FB | TIM2_CH1 | ADC_IN0 | `servo.c`, `adc.h` | |
| 7 | PA1 | PWM1/ADC1 | サーボ CH1 PWM + FB | TIM2_CH2 | ADC_IN1 | `servo.c`, `adc.h` | |
| 8 | PA2 | UART_A | リング UART A | USART2 (1-Wire) | — | `UART.c` | 22Ω 直列 (R13) + 1k プルアップ (R11) → J2.1 |
| 9 | PA3 | PWM0/ADC0 | サーボ CH2 PWM + FB | TIM2_CH4 | ADC_IN3 | `servo.c`, `adc.h` | |
| 10 | PA4 | — | 電流検出 OPA2 出力 | OPA2_OUT (内部) | ADC_IN4 | `adc.c` | 外部未接続 |
| 11 | PA5 | UART_B | リング UART B | USART4 (1-Wire, Remap 001) | — | `UART.c` | 22Ω 直列 (R14) + 1k プルアップ (R12) → J2.4 |
| 12 | PA6 | V_SENSE | 電源電圧検出 | ADC | ADC_IN6 | `adc.c` | R7 5.1k / R8 1k 分圧 |
| 13 | PA7 | CURRENT_SENSE | 電流検出入力 | OPA2_P0 (PGA x32) | — | `adc.c` | ローサイドシャント R15 (10mΩ) |
| 14 | PB1 | TEMP_SENSE | 温度検出 | ADC | ADC_IN9 | `adc.c` | R10 5.1k プルアップ + NTC 22k |
| 15 | GND | GND | | | | | |
| 16 | VDD | VDD | 3.3V | | | | LDO (U2) 出力 |
| 17 | PC16 | D- | USB D- | USBFS | — | | R2 22Ω 直列 |
| 18 | PC17 | D+ | USB D+ | USBFS | — | | R1 22Ω 直列。JP1 (DLM_JMP) + R9 5.1k で VDD へプルアップ → DLM 移行 |
| 19 | PC18 | SWDIO | デバッグ | SWD | — | | J7.2 |
| 20 | PC19 | SWCLK | デバッグ | SWD | — | | J7.1 |

## サーボコネクタ J3 (PWM) とチャンネル番号

回路図のネット名 (PWM0〜3) とファームウェア / プロトコルのチャンネル番号は一致しない。**プロトコルで指定するのはファームウェアのチャンネル番号。**

| J3 ピン | 回路図ネット | MCU ピン | ファームウェア / プロトコル チャンネル | タイマー |
| :-- | :-- | :-- | :-- | :-- |
| 1 | PWM0/ADC0 | PA3 (pin 9) | **CH2** | TIM2_CH4 |
| 2 | PWM1/ADC1 | PA1 (pin 7) | **CH1** | TIM2_CH2 |
| 3 | PWM2/ADC2 | PA0 (pin 6) | **CH0** | TIM2_CH1 |
| 4 | PWM3/ADC3 | PC1 (pin 5) | **CH3** | TIM1_CH2 |

各 PWM ピンは J3 に直結 (基板上に分圧・保護抵抗なし)。サーボのポテンショメータ電圧を読む場合の分圧回路は外付け (constants.md「サーボ FB 分圧」参照)。
ファームウェアは 20ms 周期の 10ms 時点でピンを一時的にアナログ入力へ切り替えて FB 電圧を読む (`adc.c: Update_Servo_Feedback`)。

## その他のコネクタ

| コネクタ | ピン | 内容 |
| :-- | :-- | :-- |
| J2 (UART) | 1 / 2 / 3 / 4 | UART_A (PA2) / GND / GND / UART_B (PA5) |
| J4 (V_SERVO) | 1–4 | サーボ電源 V_SERVO |
| J5 | 1–4 | サーボ GND 戻り (シャント R15 経由で GND、電流検出対象) |
| J6 (EXT_POWER) | 1 / 2 | EXT_VIN / GND |
| J7 (DEBUG) | 1 / 2 | SWCLK / SWDIO |
| JP1 (DLM_JMP) | — | ショートで D+ を R9 (5.1k) 経由で VDD にプルアップし DLM (ブートローダ) へ |
