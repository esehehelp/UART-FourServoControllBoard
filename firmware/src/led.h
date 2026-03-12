#ifndef LED_H
#define LED_H

#include "ch32x035.h"

/* LED1: PB12, active-high (source drive, pull-down resistor)
 *   BSHR (HIGH) = ON,  BCR (LOW) = OFF
 * LED2: PC3, active-low (sink drive, 3V3 pull-up)
 *   BCR  (LOW)  = ON,  BSHR (HIGH) = OFF
 */

void LED_Init(void);
void LED1_SetDuty(uint8_t duty);  /* LED1 (PB12): 0=off, 255=full on */
void LED2_SetDuty(uint8_t duty);  /* LED2 (PC3):  0=off, 255=full on */
void LED_Tick(void);              /* Call every main loop iteration (soft PWM) */
void LED_StartupBlink(void);      /* LED1のみ 3-blink startup sequence */

#endif /* LED_H */
