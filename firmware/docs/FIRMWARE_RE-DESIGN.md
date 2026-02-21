# Firmware Re-design for CH32X035F7P6

## Hardware Constraints
- **Package**: TSSOP20 (F7P6)
- **Available Peripherals**:
  - USART: USART1, USART2, USART4 (No USART3)
  - OPA: OPA2 only (No OPA1)
  - TIM: TIM1, TIM2, TIM3
- **Critical Pin Conflicts**:
  - **PA2 (Pin 16)**: USART2_TX vs OPA2_OUT -> **Conflict**.
    - Solution: Use OPA2 in `MODE2=0` (Internal output only) to free PA2 for UART.
  - **PA3 (Pin 9)**: TIM2_CH4 (PWM) vs USART2_RX -> **Conflict**.
    - Solution: Use USART2 in **Half-Duplex Mode** (`HDSEL=1`). This disconnects the RX function from PA3, allowing it to be used for PWM.

## Implementation Plan

### 1. UART Configuration (src/UART.c)
- **USART2 (Main Bus)**:
  - Mode: 1-Wire Half-Duplex (`HDSEL = 1`)
  - Pin: **PA2** (TX/RX shared)
  - **PA3** is freed for PWM usage.
  - `AFIO` Remap is **NOT** required if `HDSEL=1` effectively isolates the RX pin logic.

### 2. Servo Configuration (src/servo.c)
- **TIM2**:
  - CH1: PA0
  - CH2: PA1
  - CH4: PA3
- **TIM1**:
  - CH2: PC1
    - Note: `PC1` supports `T1C2` via **Full Remap** (TIM1_RM=11) or **Partial Remap** depending on the specific silicon revision.
    - `main` branch used `AFIO_PCFR1` manipulation for TIM1 remap.

### 3. Current Sensing (src/adc.c)
- **OPA2 Configuration**:
  - **PSEL2** (Positive Input): `00` -> **PA7** (Pin 13, OPA2_P0)
  - **NSEL2** (Negative Input): `101` -> **PGA x16**
  - **FB_EN2** (Feedback): `1` -> Enable
  - **MODE2** (Output Mode): `0` -> **Internal Output** (Disconnected from PA2)
- **ADC Configuration**:
  - Channel: **ADC_Channel_10** (Internal connection from OPA2)
  - GPIO: PA7 as `GPIO_Mode_AIN`.

### 4. Protocol (src/protocol.c)
- Update `0x02` response to include 2-byte current data (already done).
- Ensure ADC reading uses `ADC_Channel_10`.

## Action Items
1.  **Refine `adc.c`**:
    - Switch from `OPA1` to `OPA2`.
    - Set `MODE2=0`, `PSEL2=00` (PA7).
    - Read from `ADC_Channel_10`.
2.  **Refine `servo.c`**:
    - Ensure `TIM1` remap is correct for PC1.
    - Remove direct register hacks if standard library functions exist, or comment clearly.
3.  **Refine `UART.c`**:
    - Verify `HDSEL=1` is sufficient to free PA3.
