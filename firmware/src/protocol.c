#include "protocol.h"
#include "servo.h"
#include "adc.h"
#include "UART.h"
#include "usb_pd.h"
#include <string.h>
#include <stdio.h>

static uint8_t g_device_id = 0x01;
static Parser_t g_parsers[4];

volatile uint8_t g_dlm_requested = 0;
volatile uint8_t g_led_duty = 0;

void Protocol_Init(uint8_t device_id) {
    g_device_id = device_id;
    g_dlm_requested = 0;
    g_led_duty = 0;
    memset(g_parsers, 0, sizeof(g_parsers));
}

uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x07;
            else crc <<= 1;
        }
    }
    return crc;
}

void Send_Packet(Interface_t iface, uint8_t target, uint8_t source, uint8_t cmd, uint8_t *data, uint8_t len) {
    uint8_t pkt[64];
    pkt[0] = PKT_HEADER;
    pkt[1] = target;
    pkt[2] = source;
    pkt[3] = cmd;
    pkt[4] = len;
    if (len > 0) memcpy(&pkt[5], data, len);
    pkt[5 + len] = crc8(pkt, 5 + len);

    if (iface == IF_USB) {
        USBFS_Endp_DataUp(DEF_UEP3, pkt, 6 + len, DEF_UEP_CPY_LOAD);
    } else if (iface == IF_UART2) {
        for(int i=0; i<6+len; i++) {
            while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
            USART_SendData(USART2, pkt[i]);
        }
    } else if (iface == IF_UART4) {
        for(int i=0; i<6+len; i++) {
            while(USART_GetFlagStatus(USART4, USART_FLAG_TXE) == RESET);
            USART_SendData(USART4, pkt[i]);
        }
    }
}

void Forward_Packet(Interface_t source_iface, uint8_t *pkt, uint8_t len) {
    // Forward to USB if source was not USB
    if (source_iface != IF_USB) {
        USBFS_Endp_DataUp(DEF_UEP3, pkt, len, DEF_UEP_CPY_LOAD);
    }
    // Forward to UART2 if source was not UART2
    if (source_iface != IF_UART2) {
        for(int i=0; i<len; i++) {
            while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
            USART_SendData(USART2, pkt[i]);
        }
    }
    // Forward to UART4 if source was not UART4
    if (source_iface != IF_UART4) {
        for(int i=0; i<len; i++) {
            while(USART_GetFlagStatus(USART4, USART_FLAG_TXE) == RESET);
            USART_SendData(USART4, pkt[i]);
        }
    }
}

void Execute_Command(Interface_t source_iface, uint8_t target, uint8_t source, uint8_t cmd, uint8_t *data, uint8_t len) {
    switch(cmd) {
        case 0x01: // Write (Single Servo)
            if (len >= 3) {
                Set_Servo(data[0], (data[1] << 8) | data[2]);
            }
            break;
        case 0x02: // Read (Sensors)
            {
                uint16_t v = Get_ADC_Val(ADC_CH_VSENSE);
                uint16_t t = Get_ADC_Val(ADC_CH_TEMPSENSE);
                uint16_t c = Get_ADC_Val(ADC_CH_CURSENSE);
                uint8_t res[7];
                res[0] = 0x00; // Type All
                res[1] = v >> 8; res[2] = v & 0xFF;
                res[3] = t >> 8; res[4] = t & 0xFF;
                res[5] = c >> 8; res[6] = c & 0xFF;
                Send_Packet(source_iface, source, g_device_id, 0x82, res, 7);
            }
            break;
        case 0x03: // SyncWrite (All 4 Servos)
            if (len >= 8) {
                for(int i=0; i<4; i++) {
                    Set_Servo(i, (data[i*2] << 8) | data[i*2+1]);
                }
            }
            break;
        case 0x04: // CfgWrite
            if (len >= 2 && data[0] == 0x01) { // Change ID
                g_device_id = data[1];
            }
            break;
        case 0x05: // BLINK_LED
            if (len >= 1) {
                g_led_duty = data[0];
            }
            break;
        case 0x06: // Set Voltage (PD PPS)
            if (len >= 2) {
                uint16_t mv = (data[0] << 8) | data[1];
                USB_PD_Request_Voltage(mv);
            }
            break;
        case 0xF0: // DLM (Start Countdown to ISP)
            printf("DLM Received! Entering ISP in 2 seconds...\n");
            g_dlm_requested = 1;
            break;
    }
}

void Process_Byte(Interface_t iface, uint8_t b) {
    Parser_t *p = &g_parsers[iface];
    p->buf[p->len++] = b;

    switch(p->state) {
        case STATE_HEADER:
            if (b == PKT_HEADER) {
                p->state = STATE_TARGET;
            } else {
                p->len = 0;
            }
            break;
        case STATE_TARGET:
            p->target_id = b;
            p->state = STATE_SOURCE;
            break;
        case STATE_SOURCE:
            p->source_id = b;
            p->state = STATE_CMD;
            break;
        case STATE_CMD:
            p->cmd = b;
            p->state = STATE_LEN;
            break;
        case STATE_LEN:
            p->expected_len = b;
            p->data_idx = 0;
            if (p->expected_len == 0) p->state = STATE_CRC;
            else p->state = STATE_DATA;
            break;
        case STATE_DATA:
            p->data_idx++;
            if (p->data_idx >= p->expected_len) p->state = STATE_CRC;
            break;
        case STATE_CRC:
            if (b == crc8(p->buf, p->len - 1)) {
                if (p->target_id == g_device_id || p->target_id == BROADCAST_ID) {
                    Execute_Command(iface, p->target_id, p->source_id, p->cmd, &p->buf[5], p->expected_len);
                } else {
                    Forward_Packet(iface, p->buf, p->len);
                }
            }
            p->state = STATE_HEADER;
            p->len = 0;
            break;
    }
}

void Process_Packet(uint8_t *buf, uint8_t len) {
    for(uint8_t i=0; i<len; i++) {
        Process_Byte(IF_USB, buf[i]);
    }
}
