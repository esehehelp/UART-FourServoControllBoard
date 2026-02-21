#include "usb_cdc.h"
#include "uart_bus.h"
#include <string.h>

// CDC ACM Requests
#define USB_GET_DESCRIPTOR      0x06
#define USB_SET_ADDRESS         0x05
#define USB_SET_CONFIGURATION   0x09
#define CDC_GET_LINE_CODING     0x21
#define CDC_SET_LINE_CODING     0x20
#define CDC_SET_LINE_CTLSTE     0x22

// Endpoint configuration from EVT
static const uint8_t MyDevDescr[] = {
    0x12, 0x01, 0x10, 0x01, 0x02, 0x00, 0x00, 64,
    0x86, 0x1A, 0x0C, 0xFE, 0x01, 0x00, 0x01, 0x02, 0x00, 0x01
};

static const uint8_t MyCfgDescr[] = {
    0x09, 0x02, 0x43, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
    0x05, 0x24, 0x00, 0x10, 0x01,
    0x05, 0x24, 0x01, 0x00, 0x01,
    0x04, 0x24, 0x02, 0x02,
    0x05, 0x24, 0x06, 0x00, 0x01,
    0x07, 0x05, 0x81, 0x03, 0x08, 0x00, 0x01,
    0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
    0x07, 0x05, 0x02, 0x02, 64, 0x00, 0x00,
    0x07, 0x05, 0x83, 0x02, 64, 0x00, 0x00,
};

static const uint8_t MyLangDescr[] = { 0x04, 0x03, 0x09, 0x04 };
static const uint8_t MyManuInfo[]  = { 0x0E, 0x03, 'w', 0, 'c', 0, 'h', 0, '.', 0, 'c', 0, 'n', 0 };
static const uint8_t MyProdInfo[]  = { 0x16, 0x03, 'S', 0, 'e', 0, 'r', 0, 'v', 0, 'o', 0, ' ', 0, 'B', 0, 'o', 0, 'a', 0, 'r', 0, 'd', 0 };

static uint8_t ep0_buf[64] __attribute__((aligned(4)));
static uint8_t ep2_rx_buf[64] __attribute__((aligned(4)));

static volatile uint8_t  usb_addr = 0;
static volatile uint16_t setup_len = 0;
static const uint8_t    *setup_ptr = NULL;

void setup_usb_cdc() {
    RCC->AHBPCENR |= (1 << 12); // USBFS
    RCC->APB2PCENR |= RCC_AFIOEN | RCC_IOPCEN;

    // AFIO Setup: 
    // USB_IOEN(bit7) | USB_PHY_V33(bit6)=0 (for 5V VDD) | UDP_PUE_1K5(bit3-2)=11
    AFIO->CTLR = (AFIO->CTLR & ~(0x0F << 2)) | (1 << 7) | (0x03 << 2);

    R8_USB_CTRL = RB_UC_RESET_SIE | RB_UC_CLR_ALL;
    Delay_Ms(20);
    R8_USB_CTRL = 0;

    // Endpoint Modes from EVT
    R8_UEP4_1_MOD = RB_UEP1_TX_EN;
    R8_UEP2_3_MOD = RB_UEP2_RX_EN | RB_UEP3_TX_EN;

    R16_UEP0_DMA = (uint16_t)(uintptr_t)ep0_buf;
    R16_UEP2_DMA = (uint16_t)(uintptr_t)ep2_rx_buf;
    
    R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;

    R8_USB_INT_EN = RB_UIE_TRANSFER | RB_UIE_BUS_RST;
    R8_USB_INT_FG = 0xFF; // Clear all
    R8_USB_DEV_AD = 0x00;
    R8_USB_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN;
    R8_UDEV_CTRL = 0x81; 
}

void usb_cdc_poll() {
    uint8_t intflag = R8_USB_INT_FG;
    if (intflag & RB_UIF_TRANSFER) {
        uint8_t status = R8_USB_INT_ST;
        uint8_t ep = status & 0x0F;
        uint8_t token = status & MASK_UIS_TOKEN;

        if (ep == 0) {
            switch (token) {
                case UIS_TOKEN_SETUP: {
                    uint8_t req_type = ep0_buf[0];
                    uint8_t req_code = ep0_buf[1];
                    setup_len = (ep0_buf[7] << 8) | ep0_buf[6];
                    setup_ptr = NULL;

                    if ((req_type & 0x60) == 0x00) { // Standard
                        if (req_code == USB_GET_DESCRIPTOR) {
                            uint8_t type = ep0_buf[3];
                            uint8_t idx = ep0_buf[2];
                            if (type == 0x01) { setup_ptr = MyDevDescr; setup_len = (setup_len > 18) ? 18 : setup_len; }
                            else if (type == 0x02) { setup_ptr = MyCfgDescr; setup_len = (setup_len > 0x43) ? 0x43 : setup_len; }
                            else if (type == 0x03) {
                                if (idx == 0) setup_ptr = MyLangDescr;
                                else if (idx == 1) setup_ptr = MyManuInfo;
                                else if (idx == 2) setup_ptr = MyProdInfo;
                                if (setup_ptr) setup_len = (setup_len > setup_ptr[0]) ? setup_ptr[0] : setup_len;
                            }
                        } else if (req_code == USB_SET_ADDRESS) {
                            usb_addr = ep0_buf[2];
                        }
                    }

                    if (setup_ptr) {
                        uint16_t send_len = (setup_len > 64) ? 64 : setup_len;
                        memcpy(ep0_buf, setup_ptr, send_len);
                        setup_ptr += send_len; setup_len -= send_len;
                        R8_UEP0_T_LEN = send_len;
                        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_ACK | RB_UEP_T_TOG; // Start with DATA1
                    } else {
                        R8_UEP0_T_LEN = 0;
                        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_ACK | RB_UEP_T_TOG;
                    }
                    break;
                }
                case UIS_TOKEN_IN:
                    if (setup_len > 0 && setup_ptr) {
                        uint16_t send_len = (setup_len > 64) ? 64 : setup_len;
                        memcpy(ep0_buf, setup_ptr, send_len);
                        setup_ptr += send_len; setup_len -= send_len;
                        R8_UEP0_T_LEN = send_len;
                        R8_UEP0_CTRL ^= RB_UEP_T_TOG; // Flip DATA0/1
                    } else {
                        if (usb_addr != 0) {
                            R8_USB_DEV_AD = usb_addr;
                            usb_addr = 0;
                        }
                        R8_UEP0_T_LEN = 0;
                        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                    }
                    break;
                case UIS_TOKEN_OUT:
                    R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                    break;
            }
        } else if (ep == 2 && token == UIS_TOKEN_OUT) {
            uint8_t len = R16_USB_RX_LEN;
            if (len > 0) process_packet(ep2_rx_buf, len, NULL);
            R8_UEP2_CTRL ^= RB_UEP_R_TOG;
        }
        R8_USB_INT_FG = RB_UIF_TRANSFER;
    }

    if (intflag & RB_UIF_BUS_RST) {
        R8_USB_DEV_AD = 0x00; usb_addr = 0;
        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_USB_INT_FG = RB_UIF_BUS_RST;
    }
}
