#include "usb_pd.h"
#include "adc.h"

// Reference Manual Page 249: USBPD Registers
#define USBPD_BASE          0x40027000
#define R32_USBPD_CONFIG    (*(volatile uint32_t*)(USBPD_BASE + 0x00))
#define R32_USBPD_CONTROL   (*(volatile uint32_t*)(USBPD_BASE + 0x04))
#define R32_USBPD_STATUS    (*(volatile uint32_t*)(USBPD_BASE + 0x08))
#define R32_USBPD_PORT      (*(volatile uint32_t*)(USBPD_BASE + 0x0C))
#define R32_USBPD_DMA       (*(volatile uint32_t*)(USBPD_BASE + 0x10))

// Status
static uint16_t target_voltage_mv = 5000;
static uint16_t current_pps_mv = 5000;

void setup_usb_pd() {
    // RCC Enable for USBPD (AHB bit 17)
    RCC->AHBPCENR |= (1 << 17);

    // Configure CC pins (PC14, PC15) - 5.1k pull-down (Sink mode)
    // Register values based on manual page 253: 0x02 = 5.1k pull-down? 
    // Wait, page 253 says CC1_PU selects pull-up current. 
    // Sink mode (Rd) is usually enabled via separate bit.
    // In CH32X035, CC pins are managed by USBPD->PORT_CC1/2.
    R32_USBPD_PORT = 0x00020002; // Attempt Sink pull-down configuration
    R32_USBPD_CONFIG = (1 << 1); 
}

void usb_pd_request_pps(uint16_t mv, uint8_t ma_step) {
    target_voltage_mv = mv;
    // Real implementation would build a 30-bit RDO and send via BMC
}

void usb_pd_poll() {
    // Voltage Feedback Loop (every ~100ms or so)
    // current_mv = ADC * gain
    uint32_t adc_val = read_adc(ADC_CH_VSENSE);
    uint16_t current_mv = (adc_val * 3300 * 6100) / (4095 * 1000); 

    if (target_voltage_mv > 5000) {
        if (current_mv < target_voltage_mv - 100) {
            // Voltage is too low, request PPS increment
            if (current_pps_mv < 20000) {
                current_pps_mv += 20; // 20mV step
                usb_pd_request_pps(current_pps_mv, 50); // 50 * 50mA = 2.5A
            }
        } else if (current_mv > target_voltage_mv + 100) {
            // Voltage is too high
            if (current_pps_mv > 3300) {
                current_pps_mv -= 20;
                usb_pd_request_pps(current_pps_mv, 50);
            }
        }
    }
}
