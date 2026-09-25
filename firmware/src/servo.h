#ifndef SERVO_H
#define SERVO_H

#include "ch32x035.h"

#define PWM_PERIOD  20000
/* CCR value at power-up. 0 = output stays LOW (same as Servo_Free), so no
 * servo is driven until the host sends 0x01 / 0x03. A non-zero value (e.g.
 * 1500) would drive every servo immediately, which can stall a mechanism
 * that has a hard stop at that position. */
#define PWM_DEFAULT 0

void Servo_Init(void);
void Set_Servo(uint8_t idx, uint16_t pos);
void Servo_Free(uint8_t ch_mask); // bit0=CH0, bit1=CH1, bit2=CH2, bit3=CH3

#endif
