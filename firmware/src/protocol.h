#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "ch32x035.h"
#include <stddef.h>

#define HOST_ID      0x00
#define BROADCAST_ID 0xFF
#define PKT_HEADER   0xAA

// Command codes
#define CMD_WRITE_SERVO      0x01
#define CMD_READ_SENSORS     0x02
#define CMD_SYNC_WRITE       0x03
#define CMD_CONFIG_WRITE     0x04
#define CMD_LED_CONTROL      0x05
#define CMD_SET_VOLTAGE      0x06
#define CMD_SET_CALIB        0x07
#define CMD_GET_CALIB        0x08
#define CMD_SERVO_FREE       0x09
#define CMD_TELEOPERATE      0x0A
#define CMD_PING             0xA0
#define CMD_PONG             0xA1
#define CMD_DLM              0xF0

// Response codes
#define RSP_READ_SENSORS     0x82
#define RSP_GET_CALIB        0x88
#define RSP_PONG             0xA1

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
    uint8_t buf[128];
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
