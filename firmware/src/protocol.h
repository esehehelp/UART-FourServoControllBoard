#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "ch32x035.h"
#include <stddef.h>

#define HOST_ID      0x00
#define BROADCAST_ID 0xFF
#define PKT_HEADER   0xAA

/* Parser/transmit buffer size. Framing is 6 bytes (header, target, source,
 * cmd, len, crc), so the data part is limited to PKT_MAX_LEN - 6 bytes. */
#define PKT_MAX_LEN       128
#define PKT_MAX_DATA_LEN  (PKT_MAX_LEN - 6)

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
    uint8_t cmd;
    uint8_t expected_len;
} Parser_t;

extern volatile uint8_t g_dlm_requested;

void Protocol_Init(uint8_t device_id);
void Process_Byte(Interface_t iface, uint8_t b);
void Process_Packet(uint8_t *buf, uint8_t len);
void Send_Packet(Interface_t iface, uint8_t target, uint8_t source, uint8_t cmd, uint8_t *data, uint8_t len);
void Forward_Packet(Interface_t source_iface, uint8_t *pkt, uint8_t len);

#endif
