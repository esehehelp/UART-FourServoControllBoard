#ifndef CONFIG_H
#define CONFIG_H

#include "ch32x035.h"

/* Firmware version (#39). Major.Minor follow the hardware revision (V0.8);
 * compatibility between HW / FW / SW is recorded in PRs and release notes. */
#define FW_VERSION_MAJOR   0
#define FW_VERSION_MINOR   8
#define FW_VERSION_PATCH   1

#define CONFIG_FLASH_ADDR  0x0800F000 // Last 4KB of Flash for settings
#define CONFIG_MAGIC       0x43414C43 // "CALC"
#define CONFIG_VERSION     2          // layout of Config_t (1 = flat layout before #48)

#define ROLE_DEVICE  0x00
#define ROLE_HOST    0x01

#define DEVICE_NAME_LEN  16 // including the terminating NUL

/* Protection features (#47, #28), bits of ProtectionConfig_t.feature_mask */
#define PROT_OVERCURRENT   0x01
#define PROT_UNDERVOLTAGE  0x02
#define PROT_OVERVOLTAGE   0x04
#define PROT_OVERHEAT      0x08
#define PROT_STALL         0x10

/* Configuration split by purpose (#48). Every struct is 4-byte aligned so the
 * layout is identical on the MCU and in host-side tests. */

// Device identity / network (#29)
typedef struct {
    uint8_t  device_id;
    uint8_t  role;                 // ROLE_DEVICE or ROLE_HOST
    uint8_t  _pad[2];
    char     name[DEVICE_NAME_LEN]; // UI label, UTF-8, NUL-terminated
} DeviceConfig_t;

// Per servo channel
typedef struct {
    float    slope;          // FB ADC -> pulse (us)
    float    intercept;
    uint16_t min_pulse;      // commands outside [min, max] are rejected
    uint16_t max_pulse;
    uint16_t default_pulse;  // pulse at power-up, 0 = PWM off (#27)
    uint8_t  calibrated;     // 1 after a 0x07 calibration save (enables stall detection)
    uint8_t  _pad;
} ServoConfig_t;

// Protection thresholds (#47, #28)
typedef struct {
    uint8_t  feature_mask;      // PROT_* bits
    uint8_t  _pad;
    uint16_t max_current_ma;    // total servo current
    uint16_t min_voltage_mv;
    uint16_t max_voltage_mv;
    int16_t  max_temp_c;
    uint16_t stall_current_ma;  // stall: current above this ...
    uint16_t stall_time_ms;     // ... for this long ...
    uint16_t stall_fb_delta;    // ... while FB (us) moves less than this ...
    uint16_t stall_pos_error;   // ... and is further than this from the target (us)
    uint16_t _pad2;
} ProtectionConfig_t;

typedef struct {
    uint32_t           magic;
    uint8_t            version;   // CONFIG_VERSION
    uint8_t            _pad[3];
    DeviceConfig_t     device;
    ServoConfig_t      servo[4];
    ProtectionConfig_t prot;
    uint32_t           crc;       // CRC-32 of all fields above (see Config_CalcCRC)
} Config_t;

extern Config_t g_config;

void Config_Load(void);
/* Validate the config image at src (current or version-1 layout) into
 * g_config. Returns 1 if it was valid (migrated if needed), 0 if defaults
 * were loaded. Does not write the flash. */
int  Config_LoadFrom(const void *src);
int  Config_Save(void);  // updates crc; returns 1 on verified write, 0 on failure
void Config_SetDefault(void);
int  Config_IsValid(const Config_t *cfg);
uint32_t Config_CalcCRC(const Config_t *cfg);

#endif
