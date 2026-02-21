#ifndef ADC_H
#define ADC_H

#include "ch32x035.h"

/* --- ADC Channels --- */
#define ADC_CH_VSENSE    ADC_Channel_6  // PA6
#define ADC_CH_TEMPSENSE ADC_Channel_9  // PB1
#define ADC_CH_CURSENSE  ADC_Channel_4  // OPA2 Output (Internal PA4)

void ADC_Init_Custom(void);
uint16_t Get_ADC_Val(uint8_t ch);

#endif
