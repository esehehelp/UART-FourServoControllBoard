/* Host-side tests for firmware/src/config.c (CRC and defaults). */
#include "config.h"
#include <stdio.h>
#include <string.h>

/* config.c links against the flash driver; Load/Save touch the real flash
 * address and are not exercised here. */
FLASH_Status FLASH_ROM_ERASE(uint32_t a, uint32_t l) { (void)a; (void)l; return FLASH_COMPLETE; }
FLASH_Status FLASH_ROM_WRITE(uint32_t a, uint32_t *p, uint32_t l) { (void)a; (void)p; (void)l; return FLASH_COMPLETE; }

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    Config_SetDefault();
    CHECK(g_config.magic == CONFIG_MAGIC);
    CHECK(g_config.cal[0].min_pulse == 500 && g_config.cal[0].max_pulse == 2500);
    /* CRC-32 (IEEE) of the default config bytes, cross-checked with zlib.crc32 */
    CHECK(g_config.crc == 0x48c65cbbu);
    CHECK(Config_CalcCRC(&g_config) == g_config.crc);
    /* any change to a covered field changes the CRC */
    g_config.cal[3].max_pulse = 2400;
    CHECK(Config_CalcCRC(&g_config) != 0x48c65cbbu);
    /* the crc field itself is not covered */
    Config_SetDefault();
    g_config.crc = 0;
    CHECK(Config_CalcCRC(&g_config) == 0x48c65cbbu);
    printf(fails ? "FAILED (%d)\n" : "ALL OK\n", fails);
    return fails != 0;
}
