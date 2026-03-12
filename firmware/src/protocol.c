#include "protocol.h"
#include "app.h"
#include "led.h"
#include "servo.h"
#include "adc.h"
#include "UART.h"
#include "usb_pd.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

static Parser_t g_parsers[4];

volatile uint8_t g_dlm_requested = 0;

// Teleoperate state (runtime, not persisted)
typedef struct {
    uint8_t active;
    uint8_t channel_mask;
} Teleoperate_t;

static Teleoperate_t g_teleoperate = {0, 0};

void Protocol_Init(uint8_t device_id) {
    Config_Load(); // Load settings from flash
    g_dlm_requested = 0;
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
    uint8_t pkt[128];
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
    switch (source_iface) {
        case IF_USB:
            // USB → UART2 TX (downstream into ring)
            for (int i = 0; i < len; i++) {
                while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
                USART_SendData(USART2, pkt[i]);
            }
            break;
        case IF_UART2:
            // Upstream received → forward downstream via UART4
            for (int i = 0; i < len; i++) {
                while (USART_GetFlagStatus(USART4, USART_FLAG_TXE) == RESET);
                USART_SendData(USART4, pkt[i]);
            }
            break;
        case IF_UART4:
            // Downstream received → forward upstream via UART2 (toward host)
            for (int i = 0; i < len; i++) {
                while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
                USART_SendData(USART2, pkt[i]);
            }
            break;
        default:
            break;
    }
}

void Execute_Command(Interface_t source_iface, uint8_t target, uint8_t source, uint8_t cmd, uint8_t *data, uint8_t len) {
    switch(cmd) {
        case CMD_WRITE_SERVO: // Write (Single Servo)
            if (len >= 3) {
                Set_Servo(data[0], (data[1] << 8) | data[2]);
            }
            break;
        case CMD_READ_SENSORS: // Read (Sensors)
            {
                uint16_t v = Get_ADC_Val(ADC_CH_VSENSE);
                uint16_t t = Get_ADC_Val(ADC_CH_TEMPSENSE);
                uint16_t c = Get_ADC_Val(ADC_CH_CURSENSE);
                uint8_t res[15];
                res[0] = 0x00; // Type All
                res[1] = v >> 8; res[2] = v & 0xFF;
                res[3] = t >> 8; res[4] = t & 0xFF;
                res[5] = c >> 8; res[6] = c & 0xFF;
                // Servo Feedback - Now calculates Microseconds
                for(int i=0; i<4; i++) {
                    float pulse = (float)g_servo_feedback[i] * g_config.cal[i].slope + g_config.cal[i].intercept;
                    uint16_t pulse16 = (uint16_t)pulse;
                    res[7 + i*2] = pulse16 >> 8;
                    res[8 + i*2] = pulse16 & 0xFF;
                }
                Send_Packet(source_iface, source, g_config.device_id, RSP_READ_SENSORS, res, 15);
            }
            break;
        case CMD_SYNC_WRITE: // SyncWrite (All 4 Servos)
            if (len >= 8) {
                for(int i=0; i<4; i++) {
                    Set_Servo(i, (data[i*2] << 8) | data[i*2+1]);
                }
            }
            break;
        case CMD_CONFIG_WRITE: // CfgWrite
            if (len >= 2 && data[0] == 0x01) { // Change device_id
                g_config.device_id = data[1];
                Config_Save();
            }
            if (len >= 2 && data[0] == 0x02) { // Change role
                g_config.role = data[1];
                Config_Save();
            }
            break;
        case CMD_LED_CONTROL: // LED control (2-LED)
            if (len >= 1) LED1_SetDuty(data[0]);
            if (len >= 2) LED2_SetDuty(data[1]);
            break;
        case CMD_SET_VOLTAGE: // Set Voltage (PD PPS)
            if (len >= 2) {
                uint16_t mv = (data[0] << 8) | data[1];
                USB_PD_Request_Voltage(mv);
            }
            break;
        case CMD_SET_CALIB: // Set Calibration (13 bytes: CH, Slope, Intercept, Min, Max)
            if (len >= 13) {
                uint8_t ch = data[0];
                if (ch < 4) {
                    memcpy(&g_config.cal[ch].slope, &data[1], 4);
                    memcpy(&g_config.cal[ch].intercept, &data[5], 4);
                    g_config.cal[ch].min_pulse = (data[9] << 8) | data[10];
                    g_config.cal[ch].max_pulse = (data[11] << 8) | data[12];
                    Config_Save();
                }
            }
            break;
        case CMD_GET_CALIB: // Get Calibration (1 byte: CH)
            if (len >= 1) {
                uint8_t ch = data[0];
                if (ch < 4) {
                    uint8_t res[13];
                    res[0] = ch;
                    memcpy(&res[1], &g_config.cal[ch].slope, 4);
                    memcpy(&res[5], &g_config.cal[ch].intercept, 4);
                    res[9] = g_config.cal[ch].min_pulse >> 8;
                    res[10] = g_config.cal[ch].min_pulse & 0xFF;
                    res[11] = g_config.cal[ch].max_pulse >> 8;
                    res[12] = g_config.cal[ch].max_pulse & 0xFF;
                    Send_Packet(source_iface, source, g_config.device_id, RSP_GET_CALIB, res, 13);
                }
            }
            break;
        case CMD_SERVO_FREE: // Servo Free (PWM off) — ch_mask: bit0=CH0..bit3=CH3
            if (len >= 1) {
                Servo_Free(data[0]);
            }
            break;
        case CMD_TELEOPERATE: // Teleoperate (MODE, CH_MASK)
            if (len >= 2) {
                uint8_t mode = data[0];
                uint8_t ch_mask = data[1];
                if (mode == 0x00) {
                    // Deactivate teleoperate mode
                    g_teleoperate.active = 0;
                    g_teleoperate.channel_mask = 0;
                } else if (mode == 0x01) {
                    // Activate teleoperate mode
                    g_teleoperate.active = 1;
                    g_teleoperate.channel_mask = ch_mask & 0x0F; // Only 4 channels
                }
            }
            break;
        case CMD_PING: // Ping (Device Discovery)
            if (g_config.role == ROLE_HOST && source_iface == IF_USB) {
                // Host received discovery request from USB → broadcast into ring
                App_Trigger_Discovery();
            } else if (g_config.role == ROLE_DEVICE) {
                // Device received Ping → reply with own device_id
                uint8_t id = g_config.device_id;
                Send_Packet(source_iface, source, g_config.device_id, RSP_PONG, &id, 1);
            }
            break;
        case CMD_PONG: // Pong (Discovery Response)
            if (g_config.role == ROLE_HOST && len >= 1) {
                App_On_Pong(data[0]);
            }
            break;
        case CMD_DLM: // DLM (Start Countdown to ISP)
            g_dlm_requested = 1;
            break;
    }
}

void Process_Byte(Interface_t iface, uint8_t b) {
    Parser_t *p = &g_parsers[iface];
    if (p->len >= 128) {
        p->len = 0;
        p->state = STATE_HEADER;
    }
    p->buf[p->len++] = b;

    switch(p->state) {
        case STATE_HEADER:
            if (b == PKT_HEADER) p->state = STATE_TARGET;
            else p->len = 0;
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
                if (p->target_id == g_config.device_id || p->target_id == BROADCAST_ID) {
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
    for(uint8_t i=0; i<len; i++) Process_Byte(IF_USB, buf[i]);
}
