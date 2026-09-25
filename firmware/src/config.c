#include "config.h"
#include <stddef.h>
#include <string.h>

Config_t g_config;

/* FLASH_ROM_ERASE / FLASH_ROM_WRITE only accept 256-byte aligned address
 * and length, so the config is written as one full 256-byte page. */
#define CONFIG_FLASH_PAGE  256

typedef char config_fits_in_page[(sizeof(Config_t) <= CONFIG_FLASH_PAGE) ? 1 : -1];

/* CRC-32 (IEEE 802.3, reflected poly 0xEDB88320) over every field before crc */
uint32_t Config_CalcCRC(const Config_t *cfg) {
    const uint8_t *p = (const uint8_t *)cfg;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < offsetof(Config_t, crc); i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : (crc >> 1);
        }
    }
    return ~crc;
}

/* Reject values that would make Set_Servo() clamp in the wrong direction */
static int Config_IsValid(const Config_t *cfg) {
    for (int i = 0; i < 4; i++) {
        if (cfg->cal[i].min_pulse >= cfg->cal[i].max_pulse) return 0;
    }
    return 1;
}

void Config_SetDefault(void) {
    memset(&g_config, 0, sizeof(Config_t));
    g_config.magic = CONFIG_MAGIC;
    g_config.device_id = 0x01;
    g_config.role = ROLE_DEVICE;
    for(int i=0; i<4; i++) {
        g_config.cal[i].slope = 1.0f;
        g_config.cal[i].intercept = 0.0f;
        g_config.cal[i].min_pulse = 500;
        g_config.cal[i].max_pulse = 2500;
    }
    g_config.crc = Config_CalcCRC(&g_config);
}

void Config_Load(void) {
    memcpy(&g_config, (void*)CONFIG_FLASH_ADDR, sizeof(Config_t));
    if (g_config.magic != CONFIG_MAGIC ||
        g_config.crc != Config_CalcCRC(&g_config) ||
        !Config_IsValid(&g_config)) {
        Config_SetDefault();
        Config_Save();
    }
}

int Config_Save(void) {
    static uint32_t page[CONFIG_FLASH_PAGE / 4];

    g_config.crc = Config_CalcCRC(&g_config);

    memset(page, 0xFF, sizeof(page));
    memcpy(page, &g_config, sizeof(Config_t));

    if (FLASH_ROM_ERASE(CONFIG_FLASH_ADDR, CONFIG_FLASH_PAGE) != FLASH_COMPLETE) return 0;
    if (FLASH_ROM_WRITE(CONFIG_FLASH_ADDR, page, CONFIG_FLASH_PAGE) != FLASH_COMPLETE) return 0;

    /* Read back to verify the write */
    return memcmp((const void *)CONFIG_FLASH_ADDR, &g_config, sizeof(Config_t)) == 0;
}
