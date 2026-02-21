#include "servo.h"

void Set_Servo(uint8_t idx, uint16_t pos) {
    if (pos < 500) pos = 500;
    if (pos > 2500) pos = 2500;
    switch(idx) {
        case 0: TIM_SetCompare1(TIM2, pos); break;
        case 1: TIM_SetCompare2(TIM2, pos); break;
        case 2: TIM_SetCompare4(TIM2, pos); break;
        case 3: TIM_SetCompare2(TIM1, pos); break;
    }
}

/* Servo Initialization - Nuclear Option for PA3 (Restored from main branch) */
void Servo_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};

    // 1. Enable Clocks
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    
    // Enable OPA Clock directly (bit 22 of APB2PCENR)
    RCC->APB2PCENR |= (1 << 22);

    // 2. Explicitly DISABLE OPA1 (Outputs on PA3)
    // OPA_CTLR1: bit 0 is EN1.
    *((volatile uint32_t*)0x40026004) &= ~0x00000001; 

    // 3. Force TIM1 into Full Remap for PC1
    // TIM1_RM [17:15] = 011 (Full remap)
    AFIO->PCFR1 = (AFIO->PCFR1 & ~(0x7 << 15)) | (0x03 << 15);

    // 4. GPIO Setup
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // 5. Timer Base Config
    TIM_TimeBaseStructure.TIM_Period = PWM_PERIOD - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = (SystemCoreClock / 1000000) - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    // 6. OC Setup
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = PWM_DEFAULT;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

    TIM_OC1Init(TIM2, &TIM_OCInitStructure);
    TIM_OC2Init(TIM2, &TIM_OCInitStructure);
    TIM_OC4Init(TIM2, &TIM_OCInitStructure);
    TIM_OC2Init(TIM1, &TIM_OCInitStructure);

    // 7. Enable Preload and MOE
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_OC4PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Enable);
    
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_CtrlPWMOutputs(TIM2, ENABLE);

    // 8. Start
    TIM_Cmd(TIM1, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
}
