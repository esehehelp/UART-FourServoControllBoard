#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "ch32x035.h"
#include <stddef.h>

#define HOST_ID      0x00
#define BROADCAST_ID 0xFF
#define PKT_HEADER   0xAA

/* Packet: [0xAA | Target | Source | TTL | Cmd | Len | Data... | CRC8]
 * Parser/transmit buffer size. Framing is 7 bytes (header, target, source,
 * ttl, cmd, len, crc), so the data part is limited to PKT_MAX_LEN - 7 bytes. */
#define PKT_MAX_LEN       128
#define PKT_OVERHEAD      7
#define PKT_DATA_OFFSET   6
#define PKT_TTL_OFFSET    3
#define PKT_MAX_DATA_LEN  (PKT_MAX_LEN - PKT_OVERHEAD)

/* TTL set on every packet this node originates; each ring hop decrements it
 * and a packet with TTL 0 is not forwarded (loop prevention, #13). */
#define PKT_DEFAULT_TTL   16

/* Response command codes */
#define RESP_SENSOR_DATA  0x82
#define RESP_CFG_ACK      0x84
#define RESP_PD_ACK       0x86
#define RESP_CAL_ACK      0x87
#define RESP_CAL_DATA     0x88
#define RESP_ERROR        0xEE /* data: [original_cmd, error_code] (#51, #52) */

typedef enum {
    IF_USB,
    IF_UART2,
    IF_UART3,
    IF_UART4
} Interface_t;

typedef enum {
    STATE_HEADER,
    STATE_TARGET,
    STATE_SOURCE,
    STATE_TTL,
    STATE_CMD,
    STATE_LEN,
    STATE_DATA,
    STATE_CRC
} State_t;

typedef struct {
    State_t state;
    uint8_t buf[PKT_MAX_LEN];
    uint8_t len;
    uint8_t data_idx;
    uint8_t target_id;
    uint8_t source_id;
    uint8_t ttl;
    uint8_t cmd;
    uint8_t expected_len;
} Parser_t;

extern volatile uint8_t g_dlm_requested;

void Protocol_Init(uint8_t device_id);
void Process_Byte(Interface_t iface, uint8_t b);
void Process_Packet(uint8_t *buf, uint8_t len);
void Send_Packet(Interface_t iface, uint8_t target, uint8_t source, uint8_t cmd, uint8_t *data, uint8_t len);
void Forward_Packet(Interface_t source_iface, uint8_t *pkt, uint8_t len);
void Send_Error(Interface_t iface, uint8_t target, uint8_t orig_cmd, uint8_t code);
uint8_t crc8(const uint8_t *data, size_t len);

#endif
