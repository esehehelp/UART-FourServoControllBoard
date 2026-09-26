#include "config_cmd.h"
#include "config.h"
#include "error_codes.h"
#include <string.h>

#define PROT_ALL (PROT_OVERCURRENT | PROT_UNDERVOLTAGE | PROT_OVERVOLTAGE | PROT_OVERHEAT | PROT_STALL)

static int Is_Servo_Tag(uint8_t tag) { return tag >= 0x10 && tag <= 0x1F; }

static uint16_t Get_U16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static void Put_U16(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = v & 0xFF; }

/* Pointer to the u16 protection field for tag, or NULL */
static uint16_t *Prot_U16(ProtectionConfig_t *p, uint8_t tag) {
    switch (tag) {
        case CFG_TAG_MAX_CURRENT:     return &p->max_current_ma;
        case CFG_TAG_MIN_VOLTAGE:     return &p->min_voltage_mv;
        case CFG_TAG_MAX_VOLTAGE:     return &p->max_voltage_mv;
        case CFG_TAG_MAX_TEMP:        return (uint16_t *)&p->max_temp_c; // i16, same wire format
        case CFG_TAG_STALL_CURRENT:   return &p->stall_current_ma;
        case CFG_TAG_STALL_TIME:      return &p->stall_time_ms;
        case CFG_TAG_STALL_FB_DELTA:  return &p->stall_fb_delta;
        case CFG_TAG_STALL_POS_ERROR: return &p->stall_pos_error;
        default:                      return 0;
    }
}

static uint16_t *Servo_U16(ServoConfig_t *s, uint8_t tag) {
    switch (tag) {
        case CFG_TAG_DEFAULT_PULSE: return &s->default_pulse;
        case CFG_TAG_MIN_PULSE:     return &s->min_pulse;
        case CFG_TAG_MAX_PULSE:     return &s->max_pulse;
        default:                    return 0;
    }
}

uint8_t ConfigCmd_Write(const uint8_t *data, uint8_t len, uint8_t *ack, uint8_t *ack_len) {
    if (len < 2) return ERR_BAD_LENGTH;
    uint8_t tag = data[0];
    Config_t next = g_config; // edit a copy: nothing changes on error
    const uint8_t *val = &data[1];
    uint8_t vlen = len - 1;
    uint8_t head = 1;

    if (Is_Servo_Tag(tag)) {
        uint8_t ch = data[1];
        if (ch >= 4) return ERR_BAD_CHANNEL;
        uint16_t *f = Servo_U16(&next.servo[ch], tag);
        if (!f) return ERR_BAD_VALUE; // unknown or read-only
        if (len != 4) return ERR_BAD_LENGTH;
        *f = Get_U16(&data[2]);
        head = 2;
    } else if (tag == CFG_TAG_DEVICE_ID || tag == CFG_TAG_ROLE || tag == CFG_TAG_PROT_MASK) {
        if (vlen != 1) return ERR_BAD_LENGTH;
        if (tag == CFG_TAG_DEVICE_ID) {
            if (val[0] == 0x00 || val[0] == 0xFF) return ERR_BAD_VALUE; // host / broadcast
            next.device.device_id = val[0];
        } else if (tag == CFG_TAG_ROLE) {
            if (val[0] != ROLE_DEVICE && val[0] != ROLE_HOST) return ERR_BAD_VALUE;
            next.device.role = val[0];
        }
        else {
            if (val[0] & ~PROT_ALL) return ERR_BAD_VALUE;
            next.prot.feature_mask = val[0];
        }
    } else if (tag == CFG_TAG_NAME) {
        if (vlen > DEVICE_NAME_LEN - 1) return ERR_BAD_LENGTH;
        if (memchr(val, 0, vlen)) return ERR_BAD_VALUE;
        memset(next.device.name, 0, DEVICE_NAME_LEN);
        memcpy(next.device.name, val, vlen);
    } else {
        uint16_t *f = Prot_U16(&next.prot, tag);
        if (!f) return ERR_BAD_VALUE; // unknown or read-only
        if (vlen != 2) return ERR_BAD_LENGTH;
        *f = Get_U16(val);
        if (next.prot.min_voltage_mv >= next.prot.max_voltage_mv) return ERR_CONFIG_INVALID;
    }

    if (!Config_IsValid(&next)) return ERR_CONFIG_INVALID;

    Config_t prev = g_config;
    g_config = next;
    if (!Config_Save()) {
        g_config = prev; // keep RAM in line with the flash
        return ERR_FLASH_WRITE;
    }
    memcpy(ack, data, head);
    *ack_len = head;
    return ERR_OK;
}

uint8_t ConfigCmd_Read(const uint8_t *data, uint8_t len, uint8_t *out, uint8_t *out_len) {
    if (len < 1) return ERR_BAD_LENGTH;
    uint8_t tag = data[0];
    uint8_t n = 0;
    out[n++] = tag;

    if (Is_Servo_Tag(tag)) {
        if (len != 2) return ERR_BAD_LENGTH;
        uint8_t ch = data[1];
        if (ch >= 4) return ERR_BAD_CHANNEL;
        out[n++] = ch;
        if (tag == CFG_TAG_CALIBRATED) {
            out[n++] = g_config.servo[ch].calibrated;
        } else {
            uint16_t *f = Servo_U16(&g_config.servo[ch], tag);
            if (!f) return ERR_BAD_VALUE;
            Put_U16(&out[n], *f); n += 2;
        }
    } else {
        if (len != 1) return ERR_BAD_LENGTH;
        switch (tag) {
            case CFG_TAG_DEVICE_ID:      out[n++] = g_config.device.device_id; break;
            case CFG_TAG_ROLE:           out[n++] = g_config.device.role; break;
            case CFG_TAG_PROT_MASK:      out[n++] = g_config.prot.feature_mask; break;
            case CFG_TAG_CONFIG_VERSION: out[n++] = g_config.version; break;
            case CFG_TAG_NAME: {
                size_t l = strnlen(g_config.device.name, DEVICE_NAME_LEN - 1);
                memcpy(&out[n], g_config.device.name, l); n += l;
                break;
            }
            case CFG_TAG_FW_VERSION:
                out[n++] = FW_VERSION_MAJOR; out[n++] = FW_VERSION_MINOR; out[n++] = FW_VERSION_PATCH;
                break;
            default: {
                uint16_t *f = Prot_U16(&g_config.prot, tag);
                if (!f) return ERR_BAD_VALUE;
                Put_U16(&out[n], *f); n += 2;
            }
        }
    }
    *out_len = n;
    return ERR_OK;
}
