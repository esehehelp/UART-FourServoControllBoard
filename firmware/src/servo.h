#ifndef SERVO_H
#define SERVO_H

#include "ch32x035.h"

#define PWM_PERIOD  20000
/* CCR value when the timers are set up. 0 = output stays LOW (same as
 * Servo_Free); Servo_Init() then applies each channel's configured
 * default_pulse (also 0 = off by default, #27/#48). */
#define PWM_DEFAULT 0

/* Last commanded pulse per channel (us), 0 while PWM is off */
extern volatile uint16_t g_servo_target[4];

void Servo_Init(void);
void Set_Servo(uint8_t idx, uint16_t pos);
void Servo_Free(uint8_t ch_mask); // bit0=CH0, bit1=CH1, bit2=CH2, bit3=CH3

#endif
