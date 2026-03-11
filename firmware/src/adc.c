#include "adc.h"
#include "ch32x035_opa.h"

volatile uint16_t g_servo_feedback[4] = {0};

void ADC_Init_Custom(void) {
    ADC_InitTypeDef ADC_InitStructure = {0};
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    OPA_InitTypeDef OPA_InitStructure = {0};

    // 1. Enable Clocks
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_ADC1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC->APB2PCENR |= (1 << 22); // OPA Clock

    // 2. Set ADC Clock
    ADC_CLKConfig(ADC1, ADC_CLK_Div6);

    // 3. Standard Sensor GPIO Setup
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7; // PA6=V_SENSE, PA7=I_SENSE(+)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1; // PB1=T_SENSE
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // PA4 (OPA2 Output / ADC_IN4)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. OPA2 Initialization (PGA x32)
    OPA_Unlock();
    OPA_InitStructure.OPA_NUM = OPA2;
    OPA_InitStructure.Mode = OUT_IO_OUT0; // Output to PA4 (Internal/ADC), frees PA2
    OPA_InitStructure.PSEL = CHP0;        // PA7 (OPA2_P0)
    OPA_InitStructure.NSEL = CHN_PGA_32xIN; // Internal PGA x32, disconnects PA5 (OPA2_N0)
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

void Update_Servo_Feedback(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    // 1. Temporarily switch pins to AIN (Analog Input)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_3;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // 2. Sample Each Channel
    g_servo_feedback[0] = Get_ADC_Val(ADC_CH_SERVO0);
    g_servo_feedback[1] = Get_ADC_Val(ADC_CH_SERVO1);
    g_servo_feedback[2] = Get_ADC_Val(ADC_CH_SERVO2);
    g_servo_feedback[3] = Get_ADC_Val(ADC_CH_SERVO3);

    // 3. Restore to AF_PP (Alternative Function Push-Pull for TIM)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_3;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}
