#ifndef CONFIG_H
#define CONFIG_H

#include "ch32x035.h"

#define CONFIG_FLASH_ADDR  0x0800F000 // Last 4KB of Flash for settings
#define CONFIG_MAGIC       0x43414C43 // "CALC"

typedef struct {
    float slope;
    float intercept;
    uint16_t min_pulse;
    uint16_t max_pulse;
} ServoCal_t;

typedef struct {
    uint32_t magic;
    uint8_t device_id;
    ServoCal_t cal[4];
    uint32_t crc;
} Config_t;

extern Config_t g_config;

void Config_Load(void);
void Config_Save(void);
void Config_SetDefault(void);

#endif
