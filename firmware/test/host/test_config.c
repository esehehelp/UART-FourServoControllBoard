/* Host-side tests for firmware/src/config.c (layout, defaults, CRC,
 * validation, version-1 migration). */
#include "config.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* config.c links against the flash driver; Config_Load/Save touch the real
 * flash address and are not exercised here (Config_LoadFrom is). */
FLASH_Status FLASH_ROM_ERASE(uint32_t a, uint32_t l) { (void)a; (void)l; return FLASH_COMPLETE; }
FLASH_Status FLASH_ROM_WRITE(uint32_t a, uint32_t *p, uint32_t l) { (void)a; (void)p; (void)l; return FLASH_COMPLETE; }

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

/* Version-1 image as written by firmware V0.8.0 */
typedef struct {
    uint32_t magic; uint8_t device_id, role, pad[2];
    struct { float slope, intercept; uint16_t min_pulse, max_pulse; } cal[4];
    uint32_t crc;
} V1_t;

static uint32_t crc32(const void *d, size_t n) {
    const uint8_t *p = d; uint32_t c = 0xFFFFFFFF;
    for (size_t i = 0; i < n; i++) { c ^= p[i]; for (int j = 0; j < 8; j++) c = (c & 1) ? (c >> 1) ^ 0xEDB88320 : c >> 1; }
    return ~c;
}

int main(void) {
    Config_t img;

    /* layout: fixed sizes, fits the 256-byte flash page */
    CHECK(sizeof(DeviceConfig_t) == 20);
    CHECK(sizeof(ServoConfig_t) == 16);
    CHECK(sizeof(ProtectionConfig_t) == 20);
    CHECK(sizeof(Config_t) <= 256);

    /* defaults */
    Config_SetDefault();
    CHECK(g_config.magic == CONFIG_MAGIC && g_config.version == CONFIG_VERSION);
    CHECK(g_config.device.device_id == 1 && g_config.device.role == ROLE_DEVICE && g_config.device.name[0] == 0);
    CHECK(g_config.servo[0].min_pulse == 500 && g_config.servo[0].max_pulse == 2500 && g_config.servo[0].default_pulse == 0);
    CHECK(g_config.prot.feature_mask == (PROT_OVERCURRENT | PROT_OVERVOLTAGE | PROT_OVERHEAT));
    CHECK(Config_IsValid(&g_config));
    CHECK(g_config.crc == Config_CalcCRC(&g_config));
    CHECK(g_config.crc == crc32(&g_config, offsetof(Config_t, crc)));

    /* a valid current image loads unchanged */
    strcpy(g_config.device.name, "ArmLeft");
    g_config.servo[2].default_pulse = 1500;
    g_config.crc = Config_CalcCRC(&g_config);
    img = g_config;
    Config_SetDefault();
    CHECK(Config_LoadFrom(&img) == 1);
    CHECK(strcmp(g_config.device.name, "ArmLeft") == 0 && g_config.servo[2].default_pulse == 1500);

    /* corrupted CRC -> defaults */
    img.servo[1].max_pulse = 2400;
    CHECK(Config_LoadFrom(&img) == 0 && g_config.device.name[0] == 0);

    /* invalid content with a correct CRC -> defaults */
    Config_SetDefault(); img = g_config;
    img.servo[0].default_pulse = 3000; img.crc = Config_CalcCRC(&img);
    CHECK(Config_LoadFrom(&img) == 0 && g_config.servo[0].default_pulse == 0);
    Config_SetDefault(); img = g_config;
    img.device.device_id = 0xFF; img.crc = Config_CalcCRC(&img);
    CHECK(Config_LoadFrom(&img) == 0);
    Config_SetDefault(); img = g_config;
    memset(img.device.name, 'x', DEVICE_NAME_LEN); img.crc = Config_CalcCRC(&img); // no NUL
    CHECK(Config_LoadFrom(&img) == 0);

    /* version-1 image is migrated: id, role and calibration are kept */
    uint8_t raw[256];
    memset(raw, 0xFF, sizeof raw);
    V1_t v1 = { .magic = CONFIG_MAGIC, .device_id = 3, .role = ROLE_HOST };
    for (int i = 0; i < 4; i++) { v1.cal[i].slope = 2.0f; v1.cal[i].intercept = -10.0f; v1.cal[i].min_pulse = 600; v1.cal[i].max_pulse = 2400; }
    v1.crc = crc32(&v1, offsetof(V1_t, crc));
    memcpy(raw, &v1, sizeof v1);
    CHECK(Config_LoadFrom(raw) == 1);
    CHECK(g_config.version == CONFIG_VERSION && g_config.device.device_id == 3 && g_config.device.role == ROLE_HOST);
    CHECK(g_config.servo[3].slope == 2.0f && g_config.servo[3].min_pulse == 600 && g_config.servo[3].max_pulse == 2400);
    CHECK(g_config.prot.max_current_ma == 6000 && g_config.crc == Config_CalcCRC(&g_config));

    /* erased flash -> defaults */
    memset(raw, 0xFF, sizeof raw);
    CHECK(Config_LoadFrom(raw) == 0 && g_config.device.device_id == 1);

    printf(fails ? "FAILED (%d)\n" : "ALL OK\n", fails);
    return fails != 0;
}
