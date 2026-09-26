/* Host-side tests for firmware/src/protection.c (#47, #28). */
#include "protection.h"
#include "config.h"
#include "error_codes.h"
#include "protocol.h"
#include "adc.h"
#include <stdio.h>
#include <string.h>

/* ---- stubs ---- */
uint16_t adc_v, adc_i, adc_t;
volatile uint16_t g_servo_feedback[4];
volatile uint16_t g_servo_target[4];
uint8_t freed_mask; int pd_mv = -1;
uint8_t notes[16][3]; int n_notes;

uint16_t Get_ADC_Val(uint8_t ch) {
    if (ch == ADC_CH_VSENSE) return adc_v;
    if (ch == ADC_CH_CURSENSE) return adc_i;
    if (ch == ADC_CH_TEMPSENSE) return adc_t;
    return 0;
}
void Servo_Free(uint8_t m) {
    freed_mask |= m;
    for (int c = 0; c < 4; c++) if (m & (1 << c)) g_servo_target[c] = 0;
}
void USB_PD_Request_Voltage(uint16_t mv) { pd_mv = mv; }
void Send_Packet(Interface_t iface, uint8_t target, uint8_t source, uint8_t cmd, uint8_t *data, uint8_t len) {
    if (iface == IF_USB && cmd == RESP_ERROR && len == 3 && n_notes < 16) memcpy(notes[n_notes++], data, 3);
}
FLASH_Status FLASH_ROM_ERASE(uint32_t a, uint32_t l) { return FLASH_COMPLETE; }
FLASH_Status FLASH_ROM_WRITE(uint32_t a, uint32_t *p, uint32_t l) { return FLASH_COMPLETE; }

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static uint32_t now;
static void run_ms(uint32_t ms) { for (uint32_t i = 0; i < ms; i++) Protection_Tick(now++); }
static void reset(void) {
    Config_SetDefault(); Protection_Reset();
    adc_v = 1016; /* ~5.0 V */ adc_i = 40; /* ~100 mA */ adc_t = 3324; /* 25 C */
    freed_mask = 0; pd_mv = -1; n_notes = 0; now = 1000;
    memset((void *)g_servo_target, 0, sizeof g_servo_target);
}
static uint16_t ma_to_raw(uint32_t ma) { return (uint16_t)(ma * 4095u * 32u / 330000u + 1); }

int main(void) {
    /* conversions */
    CHECK(Prot_RawToMillivolt(1016) >= 4990 && Prot_RawToMillivolt(1016) <= 5010);
    CHECK(Prot_RawToMilliamp(1000) == 2518);
    CHECK(Prot_RawToCelsius(3324) == 25);
    CHECK(Prot_RawToCelsius(1401) == 80);
    CHECK(Prot_RawToCelsius(1480) >= 77 && Prot_RawToCelsius(1480) <= 78);
    CHECK(Prot_RawToCelsius(0) == INT16_MIN && Prot_RawToCelsius(4095) == INT16_MIN);

    /* normal conditions: nothing happens */
    reset(); run_ms(1000);
    CHECK(freed_mask == 0 && n_notes == 0 && Protection_Fault() == 0);

    /* overcurrent: debounced (needs 3 periods), notified once, not blocking */
    reset(); adc_i = ma_to_raw(7000);
    run_ms(15); CHECK(freed_mask == 0);          /* 2 periods: not yet */
    run_ms(20); CHECK(freed_mask == 0x0F && n_notes == 1);
    CHECK(notes[0][0] == 0x00 && notes[0][1] == ERR_OVERCURRENT && notes[0][2] == 0x0F);
    run_ms(200); CHECK(n_notes == 1);            /* latched: no repeat */
    CHECK(Protection_Fault() == 0);
    adc_i = 40; run_ms(20); adc_i = ma_to_raw(7000); run_ms(40);
    CHECK(n_notes == 2);                          /* re-armed after clearing */

    /* disabled feature does nothing */
    reset(); g_config.prot.feature_mask = 0; adc_i = ma_to_raw(9000); run_ms(100);
    CHECK(freed_mask == 0 && n_notes == 0);

    /* overheat blocks servo commands while present */
    reset(); adc_t = 1300; /* ~84 C */ run_ms(50);
    CHECK(freed_mask == 0x0F && notes[0][1] == ERR_OVERHEAT && Protection_Fault() == ERR_OVERHEAT);
    adc_t = 3324; run_ms(20); CHECK(Protection_Fault() == 0);
    /* open NTC is not an overheat */
    reset(); adc_t = 4095; run_ms(100); CHECK(n_notes == 0);

    /* overvoltage: servos freed, PD back to 5 V */
    reset(); adc_v = 3600; /* ~17.9 V */ run_ms(50);
    CHECK(freed_mask == 0x0F && pd_mv == 5000 && notes[0][1] == ERR_OVERVOLTAGE && Protection_Fault() == ERR_OVERVOLTAGE);

    /* undervoltage is opt-in */
    reset(); adc_v = 800; /* ~3.9 V */ run_ms(100); CHECK(n_notes == 0);
    g_config.prot.feature_mask |= PROT_UNDERVOLTAGE; run_ms(50);
    CHECK(notes[0][1] == ERR_UNDERVOLTAGE && Protection_Fault() == ERR_UNDERVOLTAGE);

    /* stall (#28): calibrated CH1 driven to 2000 us, stuck at 1500 us, current high */
    reset();
    g_config.prot.feature_mask |= PROT_STALL;
    for (int c = 0; c < 4; c++) { g_config.servo[c].slope = 1.0f; g_config.servo[c].intercept = 0; }
    g_config.servo[1].calibrated = 1;
    g_servo_target[1] = 2000; g_servo_feedback[1] = 1500;
    g_servo_target[2] = 1500; g_servo_feedback[2] = 1500; /* at target: never a stall */
    g_config.servo[2].calibrated = 1;
    adc_i = ma_to_raw(2000);
    run_ms(400); CHECK(freed_mask == 0);
    run_ms(150); CHECK(freed_mask == 0x02 && notes[0][1] == ERR_STALL && notes[0][2] == 0x02);
    /* moving servo is not a stall */
    reset(); g_config.prot.feature_mask |= PROT_STALL; g_config.servo[1].calibrated = 1;
    g_config.servo[1].slope = 1.0f;
    g_servo_target[1] = 2000; adc_i = ma_to_raw(2000);
    for (int k = 0; k < 100; k++) { g_servo_feedback[1] = 1000 + k * 10; run_ms(10); }
    CHECK(freed_mask == 0);
    /* uncalibrated channel is ignored */
    reset(); g_config.prot.feature_mask |= PROT_STALL;
    g_servo_target[0] = 2000; g_servo_feedback[0] = 1500; adc_i = ma_to_raw(2000); run_ms(1000);
    CHECK(freed_mask == 0);

    printf(fails ? "FAILED (%d)\n" : "ALL OK\n", fails);
    return fails != 0;
}
