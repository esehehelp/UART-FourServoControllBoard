#include "config.h"
#include <stddef.h>
#include <string.h>

Config_t g_config;

/* FLASH_ROM_ERASE / FLASH_ROM_WRITE only accept 256-byte aligned address
 * and length, so the config is written as one full 256-byte page. */
#define CONFIG_FLASH_PAGE  256

typedef char config_fits_in_page[(sizeof(Config_t) <= CONFIG_FLASH_PAGE) ? 1 : -1];

/* Version-1 layout (before #48), kept only to migrate existing boards */
typedef struct {
    float    slope;
    float    intercept;
    uint16_t min_pulse;
    uint16_t max_pulse;
} ConfigV1Cal_t;

typedef struct {
    uint32_t      magic;
    uint8_t       device_id;
    uint8_t       role;
    uint8_t       _pad[2];
    ConfigV1Cal_t cal[4];
    uint32_t      crc;
} ConfigV1_t;

/* CRC-32 (IEEE 802.3, reflected poly 0xEDB88320) */
static uint32_t crc32(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : (crc >> 1);
        }
    }
    return ~crc;
}

/* CRC over every field before crc */
uint32_t Config_CalcCRC(const Config_t *cfg) {
    return crc32(cfg, offsetof(Config_t, crc));
}

/* Reject values that would make Set_Servo() clamp in the wrong direction or
 * drive a channel outside its range at power-up */
int Config_IsValid(const Config_t *cfg) {
    for (int i = 0; i < 4; i++) {
        const ServoConfig_t *s = &cfg->servo[i];
        if (s->min_pulse >= s->max_pulse) return 0;
        if (s->default_pulse != 0 &&
            (s->default_pulse < s->min_pulse || s->default_pulse > s->max_pulse)) return 0;
    }
    if (cfg->device.device_id == 0x00 || cfg->device.device_id == 0xFF) return 0;
    if (cfg->device.role != ROLE_DEVICE && cfg->device.role != ROLE_HOST) return 0;
    if (memchr(cfg->device.name, 0, DEVICE_NAME_LEN) == NULL) return 0;
    return 1;
}

void Config_SetDefault(void) {
    memset(&g_config, 0, sizeof(Config_t));
    g_config.magic = CONFIG_MAGIC;
    g_config.version = CONFIG_VERSION;
    g_config.device.device_id = 0x01;
    g_config.device.role = ROLE_DEVICE;
    for (int i = 0; i < 4; i++) {
        g_config.servo[i].slope = 1.0f;
        g_config.servo[i].intercept = 0.0f;
        g_config.servo[i].min_pulse = 500;
        g_config.servo[i].max_pulse = 2500;
        g_config.servo[i].default_pulse = 0; // PWM off at power-up (#27)
    }
    /* Protection defaults (#47): on by default for faults that cannot occur in
     * normal use. Undervoltage (USB 5 V sags under servo load) and stall
     * detection (needs FB wiring + calibration) are opt-in. */
    g_config.prot.feature_mask     = PROT_OVERCURRENT | PROT_OVERVOLTAGE | PROT_OVERHEAT;
    g_config.prot.max_current_ma   = 6000;  // README: current sense full scale ~6.8 A
    g_config.prot.min_voltage_mv   = 4500;
    g_config.prot.max_voltage_mv   = 17500; // above the 16.8 V PD limit (#38)
    g_config.prot.max_temp_c       = 80;
    g_config.prot.stall_current_ma = 1500;
    g_config.prot.stall_time_ms    = 500;   // #28
    g_config.prot.stall_fb_delta   = 20;
    g_config.prot.stall_pos_error  = 100;
    g_config.crc = Config_CalcCRC(&g_config);
}

/* Build a current config from a valid version-1 image */
static void Config_MigrateV1(const ConfigV1_t *old) {
    Config_SetDefault();
    g_config.device.device_id = old->device_id;
    g_config.device.role = old->role;
    for (int i = 0; i < 4; i++) {
        g_config.servo[i].slope = old->cal[i].slope;
        g_config.servo[i].intercept = old->cal[i].intercept;
        g_config.servo[i].min_pulse = old->cal[i].min_pulse;
        g_config.servo[i].max_pulse = old->cal[i].max_pulse;
    }
    g_config.crc = Config_CalcCRC(&g_config);
}

int Config_LoadFrom(const void *src) {
    const Config_t *cur = (const Config_t *)src;
    const ConfigV1_t *v1 = (const ConfigV1_t *)src;

    if (cur->magic == CONFIG_MAGIC && cur->version == CONFIG_VERSION &&
        cur->crc == Config_CalcCRC(cur) && Config_IsValid(cur)) {
        memcpy(&g_config, cur, sizeof(Config_t));
        return 1;
    }
    if (v1->magic == CONFIG_MAGIC && v1->crc == crc32(v1, offsetof(ConfigV1_t, crc))) {
        Config_MigrateV1(v1);
        if (Config_IsValid(&g_config)) return 1;
    }
    Config_SetDefault();
    return 0;
}

void Config_Load(void) {
    int ok = Config_LoadFrom((const void *)CONFIG_FLASH_ADDR);
    // Rewrite when defaults were loaded or a version-1 image was migrated
    if (!ok || memcmp((const void *)CONFIG_FLASH_ADDR, &g_config, sizeof(Config_t)) != 0) {
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
