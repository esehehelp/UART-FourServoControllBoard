#ifndef CONFIG_CMD_H
#define CONFIG_CMD_H

#include <stdint.h>

/* Tag-based config access for CMD 0x20 (write) / 0x21 (read) (#49).
 *
 * Write request: [tag, (ch), value...]  -> ACK [tag, (ch)]
 * Read request:  [tag, (ch)]            -> [tag, (ch), value...]
 * Servo tags (0x10-0x1F) carry a channel byte. Integers are big-endian.
 * See firmware/docs/PROTOCOL.md for the tag table. */

#define CFG_TAG_DEVICE_ID        0x01 // u8, not 0x00/0xFF
#define CFG_TAG_ROLE             0x02 // u8, ROLE_DEVICE / ROLE_HOST
#define CFG_TAG_NAME             0x03 // 0-15 bytes UTF-8 (write), NUL-free on the wire
#define CFG_TAG_DEFAULT_PULSE    0x10 // [ch] u16 us, 0 = PWM off at power-up
#define CFG_TAG_MIN_PULSE        0x11 // [ch] u16 us
#define CFG_TAG_MAX_PULSE        0x12 // [ch] u16 us
#define CFG_TAG_CALIBRATED       0x13 // [ch] u8, read-only
#define CFG_TAG_PROT_MASK        0x30 // u8, PROT_* bits
#define CFG_TAG_MAX_CURRENT      0x31 // u16 mA
#define CFG_TAG_MIN_VOLTAGE      0x32 // u16 mV
#define CFG_TAG_MAX_VOLTAGE      0x33 // u16 mV
#define CFG_TAG_MAX_TEMP         0x34 // i16 degC
#define CFG_TAG_STALL_CURRENT    0x35 // u16 mA
#define CFG_TAG_STALL_TIME       0x36 // u16 ms
#define CFG_TAG_STALL_FB_DELTA   0x37 // u16 us
#define CFG_TAG_STALL_POS_ERROR  0x38 // u16 us
#define CFG_TAG_FW_VERSION       0xF0 // [major, minor, patch], read-only
#define CFG_TAG_CONFIG_VERSION   0xF1 // u8, read-only

/* Apply one write request. Returns ERR_OK or an ErrorCode_t; on success the
 * config is saved and ack/ack_len hold the ACK payload. Nothing changes on
 * error. */
uint8_t ConfigCmd_Write(const uint8_t *data, uint8_t len, uint8_t *ack, uint8_t *ack_len);

/* Answer one read request into out (at least 20 bytes). */
uint8_t ConfigCmd_Read(const uint8_t *data, uint8_t len, uint8_t *out, uint8_t *out_len);

#endif
