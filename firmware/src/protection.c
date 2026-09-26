#include "protection.h"
#include "config.h"
#include "error_codes.h"
#include "protocol.h"
#include "servo.h"
#include "adc.h"
#include "usb_pd.h"
#include <string.h>

#ifdef HOST_TEST /* x86 unit tests: no RISC-V interrupt control */
#define IRQ_OFF() ((void)0)
#define IRQ_ON()  ((void)0)
#else
#define IRQ_OFF() __disable_irq()
#define IRQ_ON()  __enable_irq()
#endif

/* Scale factors: firmware/docs/constants.md */
uint32_t Prot_RawToMillivolt(uint16_t raw) {
    return (uint32_t)raw * 3300u * 61u / (4095u * 10u);       // 1/6.1 divider
}

uint32_t Prot_RawToMilliamp(uint16_t raw) {
    return (uint32_t)raw * 330000u / (4095u * 32u);           // PGA x32, 10 mOhm
}

/* NTC 22k B=4050 low side, 5.1k pull-up: ADC raw at each 5 degC step
 * (-20 .. 125 degC). Linear interpolation avoids libm on the MCU. */
static const uint16_t ntc_raw[] = {
    4012, 3982, 3945, 3897, 3838, 3766, 3680, 3577, 3459, 3324,
    3174, 3010, 2835, 2651, 2462, 2272, 2083, 1899, 1723, 1557,
    1401, 1257, 1126, 1007,  899,  802,  716,  639,  571,  510,
};
#define NTC_T0    (-20)
#define NTC_STEP  5
#define NTC_N     (sizeof(ntc_raw) / sizeof(ntc_raw[0]))

int16_t Prot_RawToCelsius(uint16_t raw) {
    if (raw < 50 || raw > 4045) return INT16_MIN; // NTC shorted / open
    if (raw >= ntc_raw[0]) return NTC_T0;
    for (unsigned i = 1; i < NTC_N; i++) {
        if (raw >= ntc_raw[i]) {
            int32_t hi = ntc_raw[i - 1], lo = ntc_raw[i];
            return (int16_t)(NTC_T0 + NTC_STEP * (int32_t)(i - 1) + NTC_STEP * (hi - raw) / (hi - lo));
        }
    }
    return NTC_T0 + NTC_STEP * (NTC_N - 1);
}

typedef struct {
    uint8_t  count;   // consecutive periods the condition held
    uint8_t  active;  // latched until the condition clears
} FaultState_t;

enum { F_OVERCURRENT, F_UNDERVOLTAGE, F_OVERVOLTAGE, F_OVERHEAT, F_COUNT };

static FaultState_t s_fault[F_COUNT];
static uint32_t s_last_ms;
static uint8_t s_started;

typedef struct {
    uint32_t start_ms;  // start of the current "not moving" window
    uint16_t start_pos; // position (us) at window start
} StallState_t;
static StallState_t s_stall[4];

void Protection_Reset(void) {
    memset(s_fault, 0, sizeof(s_fault));
    memset(s_stall, 0, sizeof(s_stall));
    s_started = 0;
}

static void Notify(uint8_t code, uint8_t ch_mask) {
    uint8_t d[3] = { 0x00, code, ch_mask };
    Send_Packet(IF_USB, HOST_ID, g_config.device.device_id, RESP_ERROR, d, 3);
    if (g_config.device.role == ROLE_DEVICE) {
        // UART2 is the upstream side of a ring device (toward the host board)
        Send_Packet(IF_UART2, HOST_ID, g_config.device.device_id, RESP_ERROR, d, 3);
    }
}

/* Debounce + latch. Returns 1 on the period the fault trips. */
static int Update_Fault(FaultState_t *f, int enabled, int condition) {
    if (!enabled || !condition) {
        f->count = 0;
        f->active = 0;
        return 0;
    }
    if (f->active) return 0;
    if (++f->count >= PROT_DEBOUNCE) {
        f->active = 1;
        return 1;
    }
    return 0;
}

static uint16_t Estimated_Pulse(uint8_t ch) {
    float p = (float)g_servo_feedback[ch] * g_config.servo[ch].slope + g_config.servo[ch].intercept;
    if (p < 0) p = 0;
    if (p > 65535.0f) p = 65535.0f;
    return (uint16_t)p;
}

static uint16_t Abs_Diff(uint16_t a, uint16_t b) { return a > b ? a - b : b - a; }

/* #28: current above threshold while a calibrated, driven channel is away
 * from its target and not moving, for stall_time_ms -> free that channel.
 * Current is measured for all servos together, so both position conditions
 * are needed to single out the stalled channel. */
static void Check_Stall(uint32_t now_ms, uint32_t current_ma) {
    const ProtectionConfig_t *p = &g_config.prot;
    for (uint8_t ch = 0; ch < 4; ch++) {
        StallState_t *st = &s_stall[ch];
        uint16_t target = g_servo_target[ch];
        uint16_t pos = Estimated_Pulse(ch);
        int watching = (p->feature_mask & PROT_STALL) && g_config.servo[ch].calibrated &&
                       target != 0 && current_ma > p->stall_current_ma &&
                       Abs_Diff(target, pos) > p->stall_pos_error;
        if (!watching || Abs_Diff(pos, st->start_pos) >= p->stall_fb_delta) {
            st->start_ms = now_ms;   // moving, or nothing to watch: restart window
            st->start_pos = pos;
            continue;
        }
        if (now_ms - st->start_ms >= p->stall_time_ms) {
            Servo_Free(1u << ch);
            Notify(ERR_STALL, 1u << ch);
            st->start_ms = now_ms;
            st->start_pos = pos;
        }
    }
}

void Protection_Tick(uint32_t now_ms) {
    if (s_started && now_ms - s_last_ms < PROT_PERIOD_MS) return;
    s_started = 1;
    s_last_ms = now_ms;

    // Sample with interrupts off: a command handled from the USB interrupt
    // may also use the ADC (0x02).
    IRQ_OFF();
    uint16_t v_raw = Get_ADC_Val(ADC_CH_VSENSE);
    uint16_t i_raw = Get_ADC_Val(ADC_CH_CURSENSE);
    uint16_t t_raw = Get_ADC_Val(ADC_CH_TEMPSENSE);
    IRQ_ON();

    const ProtectionConfig_t *p = &g_config.prot;
    uint32_t mv = Prot_RawToMillivolt(v_raw);
    uint32_t ma = Prot_RawToMilliamp(i_raw);
    int16_t  tc = Prot_RawToCelsius(t_raw);

    if (Update_Fault(&s_fault[F_OVERCURRENT], p->feature_mask & PROT_OVERCURRENT, ma > p->max_current_ma)) {
        Servo_Free(0x0F);
        Notify(ERR_OVERCURRENT, 0x0F);
    }
    if (Update_Fault(&s_fault[F_UNDERVOLTAGE], p->feature_mask & PROT_UNDERVOLTAGE, mv < p->min_voltage_mv)) {
        Servo_Free(0x0F);
        Notify(ERR_UNDERVOLTAGE, 0x0F);
    }
    if (Update_Fault(&s_fault[F_OVERVOLTAGE], p->feature_mask & PROT_OVERVOLTAGE, mv > p->max_voltage_mv)) {
        Servo_Free(0x0F);
        USB_PD_Request_Voltage(PD_MIN_MV); // fall back to 5 V
        Notify(ERR_OVERVOLTAGE, 0x0F);
    }
    if (Update_Fault(&s_fault[F_OVERHEAT], p->feature_mask & PROT_OVERHEAT,
                     tc != INT16_MIN && tc > p->max_temp_c)) {
        Servo_Free(0x0F);
        Notify(ERR_OVERHEAT, 0x0F);
    }

    Check_Stall(now_ms, ma);
}

uint8_t Protection_Fault(void) {
    if (s_fault[F_OVERHEAT].active)     return ERR_OVERHEAT;
    if (s_fault[F_OVERVOLTAGE].active)  return ERR_OVERVOLTAGE;
    if (s_fault[F_UNDERVOLTAGE].active) return ERR_UNDERVOLTAGE;
    return 0;
}
