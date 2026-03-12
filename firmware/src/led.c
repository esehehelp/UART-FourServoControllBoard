#include "led.h"
#include "protocol.h"   /* g_dlm_requested */
#include "ch32x035_opa.h" /* OPA_CMP_Cmd for CMP1 disable */

extern volatile uint32_t g_dlm_timer;  /* defined in main.c */

/* LED1: PB12, active-high */
#define LED1_PORT  GPIOB
#define LED1_PIN   GPIO_Pin_12
#define LED1_CLK   RCC_APB2Periph_GPIOB
#define LED1_ON()  (LED1_PORT->BSHR = LED1_PIN)
#define LED1_OFF() (LED1_PORT->BCR  = LED1_PIN)

/* LED2: PC3, active-low */
#define LED2_PORT  GPIOC
#define LED2_PIN   GPIO_Pin_3
#define LED2_CLK   RCC_APB2Periph_GPIOC
#define LED2_ON()  (LED2_PORT->BCR  = LED2_PIN)
#define LED2_OFF() (LED2_PORT->BSHR = LED2_PIN)

static volatile uint8_t s_led1_duty = 0;
static volatile uint8_t s_led2_duty = 0;

void LED_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    /* LED1: PB12 */
    RCC_APB2PeriphClockCmd(LED1_CLK, ENABLE);
    GPIO_InitStructure.GPIO_Pin   = LED1_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED1_PORT, &GPIO_InitStructure);
    LED1_OFF();

    /* LED2: PC3 (also NRST and CMP1_N0)
     * - RST_MOD=3 must be set in option bytes (confirmed via wchisp: RST_MOD=0x3)
     * - PC3 = CMP1 negative input (CMP1_N0); explicitly disable CMP1 before GPIO use
     * - CH32X035 has only GPIO_Speed_50MHz; NRST analog filter has no effect on PP output */
    OPA_CMP_Cmd(CMP1, DISABLE);  /* ensure CMP1 analog path is off */

    RCC_APB2PeriphClockCmd(LED2_CLK, ENABLE);
    GPIO_InitStructure.GPIO_Pin   = LED2_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED2_PORT, &GPIO_InitStructure);
    LED2_OFF();
}

void LED1_SetDuty(uint8_t duty) { s_led1_duty = duty; }
void LED2_SetDuty(uint8_t duty) { s_led2_duty = duty; }

void LED_Tick(void) {
    static uint8_t cnt = 0;
    cnt++;

    /* LED1: DLM中はblink override */
    uint8_t d1 = s_led1_duty;
    if (g_dlm_requested) {
        d1 = ((g_dlm_timer / 100) % 2) ? 255 : 0;
    }
    if (cnt < d1) LED1_ON(); else LED1_OFF();

    /* LED2: duty制御のみ (active-low、論理は同一) */
    if (cnt < s_led2_duty) LED2_ON(); else LED2_OFF();
}

void LED_StartupBlink(void) {
    for (int i = 0; i < 6; i++) {
        LED1_PORT->OUTDR ^= LED1_PIN;
        Delay_Ms(100);
    }
    LED1_OFF();
    s_led1_duty = 0;
    /* LED2は消灯のまま */
}
