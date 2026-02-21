#include "debug.h"
#include "ch32x035_usbfs_device.h"
#include <string.h>

/* Device ID Config */
#define DEVICE_ID 1
#define HOST_ID   0
#define PKT_HEADER 0xAA

/* PWM Servo Config */
#define PWM_PERIOD  20000
#define PWM_DEFAULT 1500

/* ADC Channels */
#define ADC_CH_VSENSE    ADC_Channel_6 // PA6
#define ADC_CH_TEMPSENSE ADC_Channel_9 // PB1

static volatile uint16_t g_servo_pos[4] = {PWM_DEFAULT, PWM_DEFAULT, PWM_DEFAULT, PWM_DEFAULT};

/* CRC8 */
uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x07;
            else crc <<= 1;
        }
    }
    return crc;
}

/* Servo Initialization - Nuclear Option for PA3 */
void Servo_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};

    // 1. Enable Clocks
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    
    // Enable OPA Clock directly (bit 22 of APB2PCENR)
    RCC->APB2PCENR |= (1 << 22);

    // 2. Explicitly DISABLE OPA1 (Outputs on PA3)
    // OPA_CTLR1: bit 0 is EN1.
    *((volatile uint32_t*)0x40026004) &= ~0x00000001; 

    // 3. Force USART2 into 1-wire mode and REMAP RX away from PA3
    USART2->CTLR3 |= USART_CTLR3_HDSEL;
    // USART2_RM [7:5] = 100 (RX to PC3)
    AFIO->PCFR1 = (AFIO->PCFR1 & ~(0x7 << 5)) | (0x04 << 5);
    // TIM1_RM [17:15] = 011 (Full remap)
    AFIO->PCFR1 = (AFIO->PCFR1 & ~(0x7 << 15)) | (0x03 << 15);

    // 4. GPIO Setup
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // 5. Timer Base Config
    TIM_TimeBaseStructure.TIM_Period = PWM_PERIOD - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = (SystemCoreClock / 1000000) - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    // 6. OC Setup
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = PWM_DEFAULT;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

    TIM_OC1Init(TIM2, &TIM_OCInitStructure);
    TIM_OC2Init(TIM2, &TIM_OCInitStructure);
    TIM_OC4Init(TIM2, &TIM_OCInitStructure);
    TIM_OC2Init(TIM1, &TIM_OCInitStructure);

    // 7. Enable Preload and MOE
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_OC4PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Enable);
    
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_CtrlPWMOutputs(TIM2, ENABLE);

    // 8. Start
    TIM_Cmd(TIM1, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
}

void Set_Servo(uint8_t idx, uint16_t pos) {
    if (pos < 500) pos = 500;
    if (pos > 2500) pos = 2500;
    switch(idx) {
        case 0: TIM_SetCompare1(TIM2, pos); break;
        case 1: TIM_SetCompare2(TIM2, pos); break;
        case 2: TIM_SetCompare4(TIM2, pos); break;
        case 3: TIM_SetCompare2(TIM1, pos); break;
    }
}

/* ADC Initialization */
void ADC_Init_Custom(void) {
    ADC_InitTypeDef ADC_InitStructure = {0};
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);
    ADC_Cmd(ADC1, ENABLE);
    ADC1->CTLR2 |= (1 << 3); while(ADC1->CTLR2 & (1 << 3));
    ADC1->CTLR2 |= (1 << 2); while(ADC1->CTLR2 & (1 << 2));
}

uint16_t Get_ADC_Val(uint8_t ch) {
    ADC_RegularChannelConfig(ADC1, ch, 1, ADC_SampleTime_11Cycles);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    return ADC_GetConversionValue(ADC1);
}

void Process_Packet(uint8_t *buf, uint8_t len) {
    uint8_t cmd = buf[2];
    uint8_t dlen = buf[3];
    uint8_t *data = &buf[4];
    if (cmd == 0x01 && dlen >= 3) {
        Set_Servo(data[0], (data[1] << 8) | data[2]);
    } else if (cmd == 0x02) {
        uint16_t v = Get_ADC_Val(ADC_CH_VSENSE);
        uint16_t t = Get_ADC_Val(ADC_CH_TEMPSENSE);
        uint8_t res[9];
        res[0] = PKT_HEADER; res[1] = HOST_ID; res[2] = 0x82; res[3] = 4;
        res[4] = v >> 8; res[5] = v & 0xFF;
        res[6] = t >> 8; res[7] = t & 0xFF;
        res[8] = crc8(res, 8);
        USBFS_Endp_DataUp(DEF_UEP3, res, 9, DEF_UEP_CPY_LOAD);
    }
}

int main(void) {
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);
    printf("System Startup PA3=T2C4 Final\r\n");

    USBFS_RCC_Init();
    USBFS_Device_Init(ENABLE, PWR_VDD_3V3);

    Servo_Init();
    ADC_Init_Custom();

    uint32_t last_tick = 0;
    while(1) {
        if (SysTick->CNT - last_tick > 12000000) {
            last_tick = SysTick->CNT;
        }
    }
}
