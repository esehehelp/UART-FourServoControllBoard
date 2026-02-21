#include "dlm_jump.h"

// Keys for unlocking BOOT_MODEKEYR
#define FLASH_KEY1 0x45670123
#define FLASH_KEY2 0xCDEF89AB

// Bit 14 in STATR controls boot mode after reset
#define BOOT_MODE_BIT (1<<14)

// Key for PFIC system reset
#define PFIC_KEY3  0xBEEF
#define PFIC_SYSRST (1<<7)

void Jump_To_DLM(void) {
    // 1. Unlock BOOT configuration
    FLASH->BOOT_MODEKEYR = FLASH_KEY1;
    FLASH->BOOT_MODEKEYR = FLASH_KEY2;

    // 2. Set BOOT_MODE bit in FLASH_STATR
    // This tells the chip to boot from System Memory (ISP) after reset
    FLASH->STATR |= BOOT_MODE_BIT;

    // 3. Issue Software Reset via PFIC
    // Write KEY3 to upper 16 bits and set SYSRST (bit 7)
    PFIC->CFGR = (PFIC_KEY3 << 16) | PFIC_SYSRST;

    // Wait for reset
    while(1) {
        __asm__ volatile("nop");
    }
}
