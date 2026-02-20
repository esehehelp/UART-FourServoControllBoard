#ifndef _CONFIG_H
#define _CONFIG_H

#include "ch32fun.h"

// Device Identity
#define DEVICE_ID 1
#define HOST_ID   0

// ADC Channels
#define ADC_CH_VSENSE    6 // PA6 (ADC_IN6)
#define ADC_CH_TEMPSENSE 9 // PB1 (ADC_IN9)

// PWM Servo Config (50Hz, 1ms-2ms)
#define PWM_PERIOD  20000
#define PWM_DEFAULT 1500

// UART Config
#define UART_BAUD 115200

// Packet Config
#define PKT_HEADER 0xAA
#define CMD_SERVO_SET 0x01
#define CMD_SENSOR_REQ 0x02
#define CMD_PD_SET_VOLT 0x03
#define RES_SENSOR_DATA 0x82

#endif
