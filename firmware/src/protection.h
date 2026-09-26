#ifndef PROTECTION_H
#define PROTECTION_H

#include <stdint.h>

/* Built-in protection (#47) and stall detection (#28).
 *
 * Runs from the main loop regardless of any application code. Features and
 * thresholds come from g_config.prot (settable with CMD 0x20, #49).
 * On a fault the servos are freed and the host is notified once with
 * 0xEE [0x00, error_code, ch_mask] (orig_cmd 0x00 = raised by the board). */

#define PROT_PERIOD_MS   10 // evaluation period
#define PROT_DEBOUNCE     3 // consecutive periods a fault must persist (30 ms)

/* Call every millisecond from the main loop. */
void Protection_Tick(uint32_t now_ms);

/* Error code of a fault that is still present and blocks servo commands
 * (overheat, under/overvoltage), or 0. */
uint8_t Protection_Fault(void);

/* Conversions used by the checks (exposed for tests) */
uint32_t Prot_RawToMillivolt(uint16_t raw);
uint32_t Prot_RawToMilliamp(uint16_t raw);
int16_t  Prot_RawToCelsius(uint16_t raw); /* INT16_MIN if the NTC reads open/short */

void Protection_Reset(void); /* clear internal state (tests) */

#endif
