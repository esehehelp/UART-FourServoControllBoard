#include "debug.h"
#include "ch32x035_usbfs_device.h"
#include "UART.h"
#include "servo.h"
#include "adc.h"
#include "protocol.h"
#include "app.h"
#include "dlm_jump.h"
#include "usb_pd.h"
#include "led.h"
#include "usb_desc.h"
#include <stdio.h>

volatile uint32_t g_ms_ticks = 0;

/* Build USB serial number string from chip unique ID (0x1FFFF7E8, 8 bytes)
 * MySerNumInfo format: [len=0x22][type=0x03][16 hex chars in UTF-16LE] */
static void USB_BuildSerialFromUID(void) {
    static const char hex[] = "0123456789ABCDEF";
    const uint8_t *uid = (const uint8_t *)0x1FFFF7E8;
    for (int i = 0; i < 8; i++) {
        MySerNumInfo[2 + i*4 + 0] = hex[(uid[i] >> 4) & 0xF];
        MySerNumInfo[2 + i*4 + 1] = 0;
        MySerNumInfo[2 + i*4 + 2] = hex[uid[i] & 0xF];
        MySerNumInfo[2 + i*4 + 3] = 0;
    }
}
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

    USB_BuildSerialFromUID();
    USBFS_RCC_Init();
    USBFS_Device_Init(ENABLE, PWR_VDD_3V3);

    Protocol_Init(0x01);
    App_Init();
    Servo_Init();
    ADC_Init_Custom();
    
    // Initialize UARTs in Ring Mode (PA2=USART2, PA5=USART4, 1-Wire)
    UART_System_Init(UART_MODE_RING, 115200);
    
    USB_PD_Init();
    Timer3_Init();

    LED_Init();
    LED_StartupBlink();

    uint32_t last_ms = 0;

    while(1) {
        USB_PD_Process();

        // 1ms Tasks
        uint32_t now_ms = g_ms_ticks;
        if (now_ms != last_ms) {
            last_ms = now_ms;

            // Trigger ADC reading for servo feedback at 10ms into the 20ms period
            // Pulse is max 2.5ms at the start, so 10ms is safe.
            if ((now_ms % 20) == 10) {
                Update_Servo_Feedback();
            }

            App_Tick(now_ms);

            if (g_dlm_requested) {
                if (g_dlm_timer == 0) g_dlm_timer = 2000;
                if (g_dlm_timer <= 1800 && g_dlm_timer > 0) {
                    Jump_To_DLM(); // Reset into ISP
                }
            }
        }

        LED_Tick();

        // Poll UARTs
        if (USART_GetFlagStatus(USART2, USART_FLAG_RXNE) != RESET) {
            Process_Byte(IF_UART2, USART_ReceiveData(USART2));
        }
        if (USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET) {
            USART_ReceiveData(USART2); // Clear ORE by reading DR
        }

        if (USART_GetFlagStatus(USART4, USART_FLAG_RXNE) != RESET) {
            Process_Byte(IF_UART4, USART_ReceiveData(USART4));
        }
        if (USART_GetFlagStatus(USART4, USART_FLAG_ORE) != RESET) {
            USART_ReceiveData(USART4); // Clear ORE by reading DR
        }
    }
}
