#include "adc.h"

void setup_adc() {
    RCC->APB2PCENR |= RCC_ADC1EN;
    ADC1->CTLR2 |= ADC_ADON;
    ADC1->CTLR2 |= ADC_RSTCAL;
    while(ADC1->CTLR2 & ADC_RSTCAL);
    ADC1->CTLR2 |= ADC_CAL;
    while(ADC1->CTLR2 & ADC_CAL);
}

uint16_t read_adc(uint8_t channel) {
    ADC1->RSQR3 = channel;
    ADC1->CTLR2 |= ADC_SWSTART;
    while (!(ADC1->STATR & ADC_EOC));
    return ADC1->RDATAR;
}
