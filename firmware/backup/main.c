#include "ch32fun.h"
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "servo.h"
#include "adc.h"
#include "uart_bus.h"
#include "usb_pd.h"
#include "usb_cdc.h"

int main() {
    SystemInit();
    // Delay_Init is sometimes needed in ch32fun for microsecond precision
    
    // Global base clock
    RCC->APB2PCENR |= RCC_IOPBEN;

    // Setup Modules
    setup_servo_gpio();
    setup_servo_timers();
    setup_adc();
    setup_usb_pd();
    setup_usb_cdc();
    setup_uart_bus();

    // LED Init
    GPIOB->CFGHR &= ~(0xf << (4 * (12 - 8)));
    GPIOB->CFGHR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP) << (4 * (12 - 8));

    uint32_t last_tick = SysTick->CNT;
    
    while (1) {
        // Heartbeat (Toggle LED every ~250ms)
        if (SysTick->CNT - last_tick > 12000000) {
            GPIOB->OUTDR ^= (1 << 12);
            last_tick = SysTick->CNT;
            // printf("Loop...\n");
        }

        usb_pd_poll();
        usb_cdc_poll();
        uart_bus_poll();
    }
}
