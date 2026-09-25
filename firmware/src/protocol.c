#include "protocol.h"
#include "app.h"
#include "led.h"
#include "servo.h"
#include "adc.h"
#include "UART.h"
#include "usb_pd.h"
#include "config.h"
#include "error_codes.h"
#include <string.h>
#include <stdio.h>

static Parser_t g_parsers[4];

volatile uint8_t g_dlm_requested = 0;

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

static void Write_Bytes(Interface_t iface, const uint8_t *buf, uint8_t len) {
    if (iface == IF_USB) {
        USBFS_Endp_DataUp(DEF_UEP3, (uint8_t *)buf, len, DEF_UEP_CPY_LOAD);
    } else if (iface == IF_UART2) {
        for (int i = 0; i < len; i++) {
            while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
            USART_SendData(USART2, buf[i]);
        }
    } else if (iface == IF_UART4) {
        for (int i = 0; i < len; i++) {
            while (USART_GetFlagStatus(USART4, USART_FLAG_TXE) == RESET);
            USART_SendData(USART4, buf[i]);
        }
    }
}

void Send_Packet(Interface_t iface, uint8_t target, uint8_t source, uint8_t cmd, uint8_t *data, uint8_t len) {
    uint8_t pkt[PKT_MAX_LEN];
    if (len > PKT_MAX_DATA_LEN) return; // would overflow pkt[]
    pkt[0] = PKT_HEADER;
    pkt[1] = target;
    pkt[2] = source;
    pkt[PKT_TTL_OFFSET] = PKT_DEFAULT_TTL;
    pkt[4] = cmd;
    pkt[5] = len;
    if (len > 0) memcpy(&pkt[PKT_DATA_OFFSET], data, len);
    pkt[PKT_DATA_OFFSET + len] = crc8(pkt, PKT_DATA_OFFSET + len);
    Write_Bytes(iface, pkt, PKT_OVERHEAD + len);
}

void Send_Error(Interface_t iface, uint8_t target, uint8_t orig_cmd, uint8_t code) {
    uint8_t res[2] = { orig_cmd, code };
    Send_Packet(iface, target, g_config.device_id, RESP_ERROR, res, 2);
}

/* Interface a packet received on source_iface is forwarded to */
static int Forward_Iface(Interface_t source_iface, Interface_t *out) {
    switch (source_iface) {
        case IF_USB:   *out = IF_UART2; return 1; // USB → downstream into ring
        case IF_UART2: *out = IF_UART4; return 1; // upstream received → downstream
        case IF_UART4: *out = IF_UART2; return 1; // downstream received → upstream (toward host)
        default:       return 0;
    }
}

void Forward_Packet(Interface_t source_iface, uint8_t *pkt, uint8_t len) {
    Interface_t out;
    if (len < PKT_OVERHEAD || !Forward_Iface(source_iface, &out)) return;

    if (pkt[PKT_TTL_OFFSET] == 0) {
        // TTL exhausted: drop instead of circulating forever (#13).
        // Never answer an error with an error, or two nodes could ping-pong.
        if (pkt[4] != RESP_ERROR) {
            Send_Error(source_iface, pkt[2], pkt[4], ERR_TTL_EXPIRED);
        }
        return;
    }
    pkt[PKT_TTL_OFFSET]--;
    pkt[len - 1] = crc8(pkt, len - 1);
    Write_Bytes(out, pkt, len);
}

/* Pulse must be inside the channel's calibrated range; Set_Servo() would
 * otherwise clamp it silently. */
static int Pulse_Valid(uint8_t ch, uint16_t pulse) {
    return pulse >= g_config.cal[ch].min_pulse && pulse <= g_config.cal[ch].max_pulse;
}

/* Returns ERR_OK, or the error code to report. Handlers that answer with data
 * or an ACK send it themselves. Error first: nothing is changed once an
 * error is detected (#51). */
static uint8_t Execute(Interface_t source_iface, uint8_t source, uint8_t cmd, uint8_t *data, uint8_t len) {
    switch(cmd) {
        case 0x01: // Write (Single Servo) — no ACK (fire-and-forget)
            {
                if (len < 3) return ERR_BAD_LENGTH;
                if (data[0] >= 4) return ERR_BAD_CHANNEL;
                uint16_t pulse = (data[1] << 8) | data[2];
                if (!Pulse_Valid(data[0], pulse)) return ERR_BAD_VALUE;
                Set_Servo(data[0], pulse);
            }
            return ERR_OK;
        case 0x02: // Read (Sensors)
            {
                if (len < 1) return ERR_BAD_LENGTH;
                if (data[0] != 0x00) return ERR_BAD_VALUE; // only "all" is defined
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
                Send_Packet(source_iface, source, g_config.device_id, RESP_SENSOR_DATA, res, 15);
            }
            return ERR_OK;
        case 0x03: // SyncWrite (All 4 Servos) — no ACK
            if (len < 8) return ERR_BAD_LENGTH;
            for (int i = 0; i < 4; i++) {
                if (!Pulse_Valid(i, (data[i*2] << 8) | data[i*2+1])) return ERR_BAD_VALUE;
            }
            for (int i = 0; i < 4; i++) {
                Set_Servo(i, (data[i*2] << 8) | data[i*2+1]);
            }
            return ERR_OK;
        case 0x04: // CfgWrite → ACK 0x84 [sub_cmd]
            if (len < 2) return ERR_BAD_LENGTH;
            if (data[0] == 0x01) { // Change device_id
                if (data[1] == HOST_ID || data[1] == BROADCAST_ID) return ERR_BAD_VALUE;
                g_config.device_id = data[1];
            } else if (data[0] == 0x02) { // Change role
                if (data[1] != ROLE_DEVICE && data[1] != ROLE_HOST) return ERR_BAD_VALUE;
                g_config.role = data[1];
            } else {
                return ERR_BAD_VALUE;
            }
            if (!Config_Save()) return ERR_FLASH_WRITE;
            Send_Packet(source_iface, source, g_config.device_id, RESP_CFG_ACK, &data[0], 1);
            return ERR_OK;
        case 0x30: // LED Set (ch, duty) — replaces 0x05, no ACK
            if (len < 2) return ERR_BAD_LENGTH;
            if      (data[0] == 0) LED1_SetDuty(data[1]);
            else if (data[0] == 1) LED2_SetDuty(data[1]);
            else return ERR_BAD_CHANNEL;
            return ERR_OK;
        case 0x06: // Set Voltage (PD PPS) → ACK 0x86 [mv_h, mv_l]
            {
                if (len < 2) return ERR_BAD_LENGTH;
                uint16_t mv = (data[0] << 8) | data[1];
                if (mv < PD_MIN_MV || mv > PD_MAX_SAFE_MV) return ERR_BAD_VALUE; // #38
                USB_PD_Request_Voltage(mv);
                Send_Packet(source_iface, source, g_config.device_id, RESP_PD_ACK, data, 2);
            }
            return ERR_OK;
        case 0x07: // Set Calibration (13 bytes: CH, Slope, Intercept, Min, Max) → ACK 0x87 [ch]
            {
                if (len < 13) return ERR_BAD_LENGTH;
                uint8_t ch = data[0];
                if (ch >= 4) return ERR_BAD_CHANNEL;
                uint16_t min_pulse = (data[9] << 8) | data[10];
                uint16_t max_pulse = (data[11] << 8) | data[12];
                // min >= max would make Set_Servo() clamp in reverse
                if (min_pulse >= max_pulse) return ERR_CAL_INVALID;
                memcpy(&g_config.cal[ch].slope, &data[1], 4);
                memcpy(&g_config.cal[ch].intercept, &data[5], 4);
                g_config.cal[ch].min_pulse = min_pulse;
                g_config.cal[ch].max_pulse = max_pulse;
                if (!Config_Save()) return ERR_FLASH_WRITE;
                Send_Packet(source_iface, source, g_config.device_id, RESP_CAL_ACK, &ch, 1);
            }
            return ERR_OK;
        case 0x08: // Get Calibration (1 byte: CH) → 0x88
            {
                if (len < 1) return ERR_BAD_LENGTH;
                uint8_t ch = data[0];
                if (ch >= 4) return ERR_BAD_CHANNEL;
                uint8_t res[13];
                res[0] = ch;
                memcpy(&res[1], &g_config.cal[ch].slope, 4);
                memcpy(&res[5], &g_config.cal[ch].intercept, 4);
                res[9] = g_config.cal[ch].min_pulse >> 8;
                res[10] = g_config.cal[ch].min_pulse & 0xFF;
                res[11] = g_config.cal[ch].max_pulse >> 8;
                res[12] = g_config.cal[ch].max_pulse & 0xFF;
                Send_Packet(source_iface, source, g_config.device_id, RESP_CAL_DATA, res, 13);
            }
            return ERR_OK;
        case 0x09: // Servo Free (PWM off) — ch_mask: bit0=CH0..bit3=CH3, no ACK
            if (len < 1) return ERR_BAD_LENGTH;
            if (data[0] & 0xF0) return ERR_BAD_CHANNEL;
            Servo_Free(data[0]);
            return ERR_OK;
        case 0xA0: // Ping (Device Discovery) — never answered with an error
            if (g_config.role == ROLE_HOST && source_iface == IF_USB) {
                // Host received discovery request from USB → broadcast into ring
                App_Trigger_Discovery();
            } else if (g_config.role == ROLE_DEVICE) {
                // Device received Ping → reply with own device_id
                uint8_t id = g_config.device_id;
                Send_Packet(source_iface, source, g_config.device_id, 0xA1, &id, 1);
            }
            return ERR_OK;
        case 0xA1: // Pong (Discovery Response)
            if (g_config.role == ROLE_HOST && len >= 1) {
                App_On_Pong(data[0]);
            }
            return ERR_OK;
        case 0xF0: // DLM (Start Countdown to ISP) — no response (resets)
            g_dlm_requested = 1;
            return ERR_OK;
        case RESP_ERROR: // an error addressed to us: never answer it
            return ERR_OK;
        default:
            return ERR_UNKNOWN_CMD;
    }
}

void Execute_Command(Interface_t source_iface, uint8_t target, uint8_t source, uint8_t cmd, uint8_t *data, uint8_t len) {
    uint8_t err = Execute(source_iface, source, cmd, data, len);
    // Broadcasts are not answered with errors: every node would reply at once.
    if (err != ERR_OK && target != BROADCAST_ID) {
        Send_Error(source_iface, source, cmd, err);
    }
}

void Process_Byte(Interface_t iface, uint8_t b) {
    Parser_t *p = &g_parsers[iface];
    if (p->len >= PKT_MAX_LEN) {
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
            p->state = STATE_TTL;
            break;
        case STATE_TTL:
            p->ttl = b;
            p->state = STATE_CMD;
            break;
        case STATE_CMD:
            p->cmd = b;
            p->state = STATE_LEN;
            break;
        case STATE_LEN:
            if (b > PKT_MAX_DATA_LEN) {
                // Cannot fit in buf[]: drop the packet and resync on next header
                p->state = STATE_HEADER;
                p->len = 0;
                break;
            }
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
                    Execute_Command(iface, p->target_id, p->source_id, p->cmd, &p->buf[PKT_DATA_OFFSET], p->expected_len);
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
