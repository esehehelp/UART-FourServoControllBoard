#ifndef SERVO_H
#define SERVO_H

#include "ch32x035.h"

#define PWM_PERIOD  20000
#define PWM_DEFAULT 1500

void Servo_Init(void);
void Set_Servo(uint8_t idx, uint16_t pos);

#endif
