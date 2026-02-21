# CH32X035F7P6 Firmware Specification (Revised)

## 1. Peripheral Mapping
| Function | Peripheral | Pin / Channel | Notes |
| :--- | :--- | :--- | :--- |
| **Servo CH0** | TIM2_CH1 | PA0 (Pin 6) | |
| **Servo CH1** | TIM2_CH2 | PA1 (Pin 7) | |
| **Servo CH2** | TIM2_CH4 | PA3 (Pin 9) | Freed by USART2 RX remap |
| **Servo CH3** | TIM1_CH2 | PC1 (Pin 5) | Full Remap required |
| **UART_A (TX)**| USART2_TX | PA2 (Pin 8) | |
| **UART_B (RX)**| USART2_RX | PA5 (Pin 11)| Remapped from PA3 |
| **V_SENSE** | ADC_IN6 | PA6 (Pin 12)| R1=5.1k, R2=1k |
| **T_SENSE** | ADC_IN9 | PB1 (Pin 14)| NTC 22k, B=4050 |
| **I_SENSE (P)**| OPA2_P0 | PA7 (Pin 13)| Shunt 10mOhm |
| **I_SENSE (O)**| OPA2_OUT | PA4 (Internal)| NC Pin, sampled via **ADC_IN4** |
| **LED** | GPIO | PB12 (Pin 1) | Status / DLM Indicator |

## 2. OPA2 & Current Sensing
- **PGA Mode**: Gain x32 (`NSEL2 = 111`, `FB_EN2 = 1`).
- **Internal Path**: `MODE2 = 0` to drive internal PA4/ADC_IN4.
- **GPIO**: PA4 must be configured as `GPIO_Mode_AIN` (even if NC) to ensure ADC connectivity.

## 3. USART2 & Remapping
- **Conflict**: Default USART2_RX is PA3, which is needed for Servo CH2.
- **Solution**: Set `AFIO_PCFR1` to remap USART2_RX to **PA5**.
- **Mode**: 1-Wire Half-duplex (`HDSEL=1`) on PA2 if required, but PA2/PA5 separate pins are also possible once remapped.

## 4. Initialization Sequence
1. **Clocks**: Enable GPIOA, B, C, AFIO, TIM1, TIM2, ADC1, OPA.
2. **AFIO**: 
   - Remap TIM1 to PC1.
   - Remap USART2 RX to PA5.
3. **OPA2**: Configure as PGA x32, output to PA4.
4. **ADC**: Initialize with channels 4, 6, 9.
5. **GPIO**: Set PA4, PA6, PA7, PB1 to `AIN`.
6. **PWM**: Initialize TIM1/TIM2.

## 5. Firmware Update (DLM)
- **Mechanism**: Software Reset to System Bootloader.
- **Trigger**: UART Command `0xF0` (DLM).
- **Behavior**:
    1.  Upon receiving `0xF0`, the board enters "Armed" state.
    2.  Status LED (PB12) blinks rapidly (5Hz) for 2 seconds as a warning.
    3.  Board unlocks BOOT config, sets `BOOT_MODE` bit, and performs a system reset via PFIC.
    4.  Device re-enumerates as WCH ISP device (`VID: 0x1A86, PID: 0x55E0`).
- **Tooling**: `wchisp` (Rust-based) or WCHISPTool.
- **Driver Note**: On Windows, `wchisp` requires WinUSB driver for `1a86:55e0`. Use Zadig to install if not detected.
