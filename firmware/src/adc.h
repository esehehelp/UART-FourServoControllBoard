#ifndef ADC_H
#define ADC_H

#include "ch32x035.h"

/* --- ADC Channels --- */
#define ADC_CH_VSENSE    ADC_Channel_6  // PA6
#define ADC_CH_TEMPSENSE ADC_Channel_9  // PB1
#define ADC_CH_CURSENSE  ADC_Channel_4  // OPA2 Output (Internal PA4)

// Servo Feedback ADC Channels
#define ADC_CH_SERVO0    ADC_Channel_0  // PA0
#define ADC_CH_SERVO1    ADC_Channel_1  // PA1
#define ADC_CH_SERVO2    ADC_Channel_3  // PA3
#define ADC_CH_SERVO3    ADC_Channel_11 // PC1

extern volatile uint16_t g_servo_feedback[4];

void ADC_Init_Custom(void);
uint16_t Get_ADC_Val(uint8_t ch);
void Update_Servo_Feedback(void);

#endif
