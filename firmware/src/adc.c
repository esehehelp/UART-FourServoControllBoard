#include "adc.h"
#include "ch32x035_opa.h"

void ADC_Init_Custom(void) {
    ADC_InitTypeDef ADC_InitStructure = {0};
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    OPA_InitTypeDef OPA_InitStructure = {0};

    // 1. Enable Clocks
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_ADC1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC->APB2PCENR |= (1 << 22); // OPA Clock

    // 2. Set ADC Clock
    ADC_CLKConfig(ADC1, ADC_CLK_Div6);

    // 3. GPIO Setup
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // PA4 (OPA2 Output / ADC_IN4)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. OPA2 Initialization (PGA x32)
    OPA_Unlock();
    OPA_InitStructure.OPA_NUM = OPA2;
    // MODE2 = 0 (OUT_IO_OUT0): Output to PA4 (Internal/ADC), frees PA2
    OPA_InitStructure.Mode = OUT_IO_OUT0; 
    OPA_InitStructure.PSEL = CHP0;        // PA7 (OPA2_P0)
    // NSEL2 = 111: Internal PGA x32, disconnects PA5 (OPA2_N0)
    OPA_InitStructure.NSEL = CHN_PGA_32xIN; 
    OPA_InitStructure.FB   = FB_ON;
    OPA_Init(&OPA_InitStructure);
    OPA_Cmd(OPA2, ENABLE);
    OPA_Lock();

    // 5. ADC Initialization
    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);
    
    ADC_Cmd(ADC1, ENABLE);

    // 6. Calibration
    ADC1->CTLR2 |= (1 << 3); while(ADC1->CTLR2 & (1 << 3));
    ADC1->CTLR2 |= (1 << 2); while(ADC1->CTLR2 & (1 << 2));
}

uint16_t Get_ADC_Val(uint8_t ch) {
    ADC_RegularChannelConfig(ADC1, ch, 1, ADC_SampleTime_11Cycles);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    return ADC_GetConversionValue(ADC1);
}
