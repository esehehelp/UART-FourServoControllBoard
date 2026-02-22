#include "debug.h"
#include "ch32x035_usbfs_device.h"
#include "UART.h"
#include "servo.h"
#include "adc.h"
#include "protocol.h"
#include "dlm_jump.h"
#include "usb_pd.h"
#include <stdio.h>

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
    // Set to default state (0 = OFF for active-high)
    g_led_duty = 0;

    uint32_t dlm_timer = 0;
    uint32_t last_ms_tick = (uint32_t)SysTick->CNT;
    uint8_t soft_pwm_cnt = 0;

    while(1) {
        USB_PD_Process();

        // Soft PWM for LED
        soft_pwm_cnt++;
        if (g_dlm_requested) {
            // DLM Mode: Aggressive Fast Blink (5Hz)
            if ((dlm_timer / 100) % 2) GPIOB->BSHR = GPIO_Pin_12;
            else GPIOB->BCR = GPIO_Pin_12;
        } else {
            // Normal Mode: Soft PWM
            if (soft_pwm_cnt < g_led_duty) GPIOB->BSHR = GPIO_Pin_12;
            else GPIOB->BCR = GPIO_Pin_12;
        }

        // 1ms Timebase
        uint32_t now = (uint32_t)SysTick->CNT;
        uint32_t diff = now - last_ms_tick;
        uint32_t ticks_per_ms = SystemCoreClock / 1000;

        if (diff >= ticks_per_ms) {
            uint32_t elapsed = diff / ticks_per_ms;
            last_ms_tick += elapsed * ticks_per_ms;
            
            Protocol_Tick(elapsed);
            
            if (g_dlm_requested) {
                if (dlm_timer == 0) dlm_timer = 2000;
                else {
                    if (dlm_timer > elapsed) dlm_timer -= elapsed;
                    else dlm_timer = 0;

                    if (dlm_timer <= 1800 && dlm_timer > 0) { 
                        // Use the robust method to enter ISP via Software Reset
                        Jump_To_DLM();
                    }
                }
            }
        }

        // Poll UART2
        if (USART_GetFlagStatus(USART2, USART_FLAG_RXNE) != RESET) {
            Process_Byte(IF_UART2, USART_ReceiveData(USART2));
        }
        
        // Poll UART4
        if (USART_GetFlagStatus(USART4, USART_FLAG_RXNE) != RESET) {
            Process_Byte(IF_UART4, USART_ReceiveData(USART4));
        }
    }
}
