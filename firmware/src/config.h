#ifndef CONFIG_H
#define CONFIG_H

#include "ch32x035.h"

/* Firmware version (#39). Major.Minor follow the hardware revision (V0.8);
 * compatibility between HW / FW / SW is recorded in PRs and release notes. */
#define FW_VERSION_MAJOR   0
#define FW_VERSION_MINOR   8
#define FW_VERSION_PATCH   0

#define CONFIG_FLASH_ADDR  0x0800F000 // Last 4KB of Flash for settings
#define CONFIG_MAGIC       0x43414C43 // "CALC"

#define ROLE_DEVICE  0x00
#define ROLE_HOST    0x01

typedef struct {
    float slope;
    float intercept;
    uint16_t min_pulse;
    uint16_t max_pulse;
} ServoCal_t;

typedef struct {
    uint32_t magic;
    uint8_t device_id;
    uint8_t role;        // ROLE_DEVICE or ROLE_HOST
    uint8_t _pad[2];     // alignment
    ServoCal_t cal[4];
    uint32_t crc;        // CRC-32 of all fields above (see Config_CalcCRC)
} Config_t;

extern Config_t g_config;

void Config_Load(void);
int  Config_Save(void);  // updates crc; returns 1 on verified write, 0 on failure
void Config_SetDefault(void);
uint32_t Config_CalcCRC(const Config_t *cfg);

#endif
