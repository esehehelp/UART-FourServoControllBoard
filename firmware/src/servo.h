#ifndef SERVO_H
#define SERVO_H

#include "ch32x035.h"

#define PWM_PERIOD  20000
#define PWM_DEFAULT 1500

void Servo_Init(void);
void Set_Servo(uint8_t idx, uint16_t pos);
void Servo_Free(uint8_t ch_mask); // bit0=CH0, bit1=CH1, bit2=CH2, bit3=CH3

#endif
