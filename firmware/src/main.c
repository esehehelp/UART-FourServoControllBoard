#include "debug.h"
#include "ch32x035_usbfs_device.h"
#include "UART.h"
#include "servo.h"
#include "adc.h"
#include "protocol.h"
#include "dlm_jump.h"
#include "usb_pd.h"
#include <stdio.h>

volatile uint32_t g_ms_ticks = 0;
volatile uint32_t g_dlm_timer = 0;

void TIM3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM3_IRQHandler(void) {
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) {
        g_ms_ticks++;
        if (g_dlm_requested && g_dlm_timer > 0) {
            g_dlm_timer--;
        }
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    }
}

void Timer3_Init(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    TIM_TimeBaseStructure.TIM_Period = 1000 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = (SystemCoreClock / 1000000) - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
    NVIC_EnableIRQ(TIM3_IRQn);
    TIM_Cmd(TIM3, ENABLE);
}

int main(void) {
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);
    printf("UART 4-Servo Board v2.0 Startup\r\n");

    USBFS_RCC_Init();
    USBFS_Device_Init(ENABLE, PWR_VDD_3V3);

    Protocol_Init(0x01);
    Servo_Init();
    ADC_Init_Custom();
    
    // Initialize UARTs in Ring Mode (PA2=USART2, PA5=USART4, 1-Wire)
    UART_System_Init(UART_MODE_RING, 115200);
    
    USB_PD_Init();
    Timer3_Init();

    // Initialize PB12 for Status LED
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // Startup Verification: Blink LED 3 times
    for(int i=0; i<6; i++) {
        GPIOB->OUTDR ^= GPIO_Pin_12;
        Delay_Ms(100);
    }
    g_led_duty = 0;

    uint32_t last_ms = 0;
    uint8_t soft_pwm_cnt = 0;

    while(1) {
        USB_PD_Process();

        // 1ms Tasks
        uint32_t now_ms = g_ms_ticks;
        if (now_ms != last_ms) {
            last_ms = now_ms;

            if (g_dlm_requested) {
                if (g_dlm_timer == 0) g_dlm_timer = 2000;
                if (g_dlm_timer <= 1800 && g_dlm_timer > 0) {
                    Jump_To_DLM(); // Reset into ISP
                }
            }
        }

        // Soft PWM for LED (Run as fast as possible)
        soft_pwm_cnt++;
        uint8_t target_duty = g_led_duty;
        
        if (g_dlm_requested) {
            // DLM Blink Overrides normal duty
            target_duty = ((g_dlm_timer / 100) % 2) ? 255 : 0;
        }

        if (soft_pwm_cnt < target_duty) GPIOB->BSHR = GPIO_Pin_12;
        else GPIOB->BCR = GPIO_Pin_12;

        // Poll UARTs
        if (USART_GetFlagStatus(USART2, USART_FLAG_RXNE) != RESET) {
            Process_Byte(IF_UART2, USART_ReceiveData(USART2));
        }
        if (USART_GetFlagStatus(USART4, USART_FLAG_RXNE) != RESET) {
            Process_Byte(IF_UART4, USART_ReceiveData(USART4));
        }
    }
}
