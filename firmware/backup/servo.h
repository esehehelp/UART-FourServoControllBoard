#ifndef _SERVO_H
#define _SERVO_H

#include "config.h"

void setup_servo_gpio();
void setup_servo_timers();
void set_servo_pos(uint8_t index, uint16_t pos);

#endif
