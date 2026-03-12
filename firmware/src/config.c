#include "config.h"
#include <string.h>

Config_t g_config;

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
}

void Config_Load(void) {
    memcpy(&g_config, (void*)CONFIG_FLASH_ADDR, sizeof(Config_t));
    if (g_config.magic != CONFIG_MAGIC) {
        Config_SetDefault();
        Config_Save();
    }
}

void Config_Save(void) {
    FLASH_Unlock();
    FLASH_ErasePage(CONFIG_FLASH_ADDR);
    FLASH_ROM_WRITE(CONFIG_FLASH_ADDR, (uint32_t *)&g_config, sizeof(Config_t));
    FLASH_Lock();
}
