#ifndef ERROR_CODES_H
#define ERROR_CODES_H

/* Error codes carried in the 0xEE error response: [original_cmd, error_code].
 * Values agreed in issue #52; keep in sync with ErrCode* in
 * software/config/config.go. */
typedef enum {
    ERR_OK              = 0x00,

    /* 0x01-0x0F: generic */
    ERR_UNKNOWN_CMD     = 0x01,
    ERR_BAD_LENGTH      = 0x02,
    ERR_BAD_CHANNEL     = 0x03,
    ERR_BAD_VALUE       = 0x04,

    /* 0x10-0x1F: communication */
    ERR_CRC             = 0x10,
    ERR_TIMEOUT         = 0x11,
    ERR_BUFFER_OVERFLOW = 0x12,
    ERR_TTL_EXPIRED     = 0x13, /* ring forwarding stopped: TTL reached 0 (#13) */

    /* 0x20-0x2F: hardware */
    ERR_FLASH_WRITE     = 0x20,
    ERR_ADC             = 0x21,
    ERR_PWM             = 0x22,

    /* 0x30-0x3F: protection (#47) */
    ERR_OVERCURRENT     = 0x30,
    ERR_UNDERVOLTAGE    = 0x31,
    ERR_OVERHEAT        = 0x32,
    ERR_STALL           = 0x33,

    /* 0x40-0x4F: configuration */
    ERR_CONFIG_INVALID  = 0x40,
    ERR_CAL_INVALID     = 0x41,

    /* 0xF0-0xFF: reserved */
} ErrorCode_t;

#endif
