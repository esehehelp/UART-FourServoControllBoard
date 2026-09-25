# 回路定数

回路定数の一元管理ファイル。値は回路図 `hardware/4servoboard.kicad_sch` (リファレンス記号を併記) から確認済み。
ソフトウェア側の換算係数は `software/config/config.go` にあり、下表の対応を保つこと。ピンアサインは [`docs/pinassign.md`](../../docs/pinassign.md)。

ADC: 12 bit (0–4095)、基準電圧 VDD = 3.3 V。

## 電圧検出 (V_SENSE, PA6)
- R7: 5.1 kΩ (V_SERVO 側), R8: 1.0 kΩ (GND 側)
- 分圧比: R8 / (R7 + R8) = 1 / 6.1
- 換算: V = raw × 3.3 / 4095 × 6.1 ≈ raw × 0.00491 [V]

## 温度検出 (TEMP_SENSE, PB1)
- NTC サーミスタ: 22 kΩ @25°C, B = 4050 (ローサイド)
- R10: 5.1 kΩ プルアップ (VDD 側)
- R_ntc = 5100 × raw / (4095 − raw)
- T = 1 / (ln(R_ntc / 22000) / 4050 + 1 / 298.15) − 273.15 [°C]

## 電流検出 (CURRENT_SENSE, PA7 → OPA2 → PA4)
- R15: シャント 0.01 Ω (10 mΩ)、サーボ GND 戻り (J5) のローサイド
- OPA2: PGA x32 (内部接続、出力は PA4 / ADC_IN4)
- 換算: I = raw × 3.3 / 4095 / 32 / 0.01 ≈ raw × 2.518 [mA]
- フルスケール: 3.3 / 32 / 0.01 ≈ 10.3 A (実用範囲は OPA 出力振幅で制限)

## サーボ FB 分圧 (実装者依存、基板外)
ほとんどの RC サーボは FB 端子を持たないため、ポテンショメータ出力から分圧回路を**基板外に**外付けする。
基板上の PWM/FB ピン (J3) は MCU に直結で、分圧抵抗は実装されていない (回路図で確認)。抵抗値は実装・改造内容によって異なる。

本実装値:
- V_FB_R1 = 2.7 kΩ (5% カーボン皮膜, ポテンショメータ出力側)
- V_FB_R2 = 3.3 kΩ (5% カーボン皮膜, GND 側、FB ピンは R1/R2 の中点)
- 分圧比 = R2 / (R1 + R2) = 3.3 / 6.0 = 0.55
- 換算: V_pot = raw × 3.3 / 4095 / 0.55 [V]

抵抗値を変更した場合は `software/config/config.go` の `V_FB_R1` / `V_FB_R2` を更新する。

## UART (J2)
- R13 / R14: 22 Ω ダンピング (直列)
- R11 / R12: 1 kΩ プルアップ (VDD、1-Wire バスライン)

## USB
- R1 / R2: 22 Ω 直列 (D+ / D−)
- R3 / R4: 5.1 kΩ CC プルダウン (Sink)
- R9: 5.1 kΩ D+ プルアップ (JP1 ショート時のみ、DLM 移行用)

## LED
- LED1: PB12 → R6 1 kΩ → D2 → GND (Active-High)
- LED2: VDD → R5 1 kΩ → D1 → PC3 (Active-Low)

## ソフトウェアとの対応 (`software/config/config.go`)

| 定数 | 値 | 根拠 |
| :-- | :-- | :-- |
| `VOLTAGE_SCALE` | 0.00491 | 電圧検出 |
| `CURRENT_SCALE` | 2.518 | 電流検出 |
| `TEMP_R0` / `TEMP_B` / `TEMP_T0` | 22000 / 4050 / 298.15 | NTC |
| `TEMP_SERIES_R` | 5100 | R10 |
| `V_FB_R1` / `V_FB_R2` → `FB_VOLTAGE_SCALE` | 2700 / 3300 | サーボ FB 分圧 (外付け) |
