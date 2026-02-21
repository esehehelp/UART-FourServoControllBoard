#include "servo.h"

void setup_servo_gpio() {
    // Already enabled in main/global? No, let's ensure it.
    RCC->APB2PCENR |= RCC_IOPAEN | RCC_IOPCEN;

    // PWM Pins: PA0, PA1, PA3 as AF PP
    GPIOA->CFGLR &= ~((0xf << (4 * 0)) | (0xf << (4 * 1)) | (0xf << (4 * 3)));
    GPIOA->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (4 * 0);
    GPIOA->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (4 * 1);
    GPIOA->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (4 * 3);

    // PWM Pin: PC1 as AF PP
    GPIOC->CFGLR &= ~(0xf << (4 * 1));
    GPIOC->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (4 * 1);
}

void setup_servo_timers() {
    RCC->APB1PCENR |= RCC_TIM2EN | RCC_TIM3EN;

    // TIM2 (PA0, PA1, PA3) 50Hz
    TIM2->PSC = 47; 
    TIM2->ATRLR = PWM_PERIOD - 1;
    TIM2->CHCTLR1 = TIM_OC1M_2 | TIM_OC1M_1 | TIM_OC1PE | TIM_OC2M_2 | TIM_OC2M_1 | TIM_OC2PE;
    TIM2->CHCTLR2 = TIM_OC4M_2 | TIM_OC4M_1 | TIM_OC4PE;
    TIM2->CCER = TIM_CC1E | TIM_CC2E | TIM_CC4E;
    TIM2->CH1CVR = PWM_DEFAULT;
    TIM2->CH2CVR = PWM_DEFAULT;
    TIM2->CH4CVR = PWM_DEFAULT;
    TIM2->CTLR1 = TIM_CEN | TIM_ARPE;

    // TIM3 (PC1) 50Hz
    TIM3->PSC = 47;
    TIM3->ATRLR = PWM_PERIOD - 1;
    TIM3->CHCTLR1 = TIM_OC1M_2 | TIM_OC1M_1 | TIM_OC1PE;
    TIM3->CCER = TIM_CC1E;
    TIM3->CH1CVR = PWM_DEFAULT;
    TIM3->CTLR1 = TIM_CEN | TIM_ARPE;
}

void set_servo_pos(uint8_t index, uint16_t pos) {
    if (index == 0) TIM2->CH1CVR = pos;
    else if (index == 1) TIM2->CH2CVR = pos;
    else if (index == 2) TIM2->CH4CVR = pos;
    else if (index == 3) TIM3->CH1CVR = pos;
}
