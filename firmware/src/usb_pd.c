#include "usb_pd.h"
#include "ch32x035_usbpd.h"
#include "debug.h"
#include <string.h>
#include <stdio.h>

/* PD Internal definitions from SDK */
#define DEF_PD_TX_OK               0x00
#define DEF_PD_TX_FAIL             0x01

#define UPD_TMR_TX_48M    (80-1)
#define UPD_TMR_RX_48M    (120-1)

#define UPD_SOP0          ( TX_SEL1_SYNC1 | TX_SEL2_SYNC1 | TX_SEL3_SYNC1 | TX_SEL4_SYNC2 )
#define UPD_HARD_RESET    ( TX_SEL1_RST1  | TX_SEL2_RST1  | TX_SEL3_RST1  | TX_SEL4_RST2  )

/* PD Global variables */
__attribute__ ((aligned(4))) uint8_t PD_Rx_Buf[34];
__attribute__ ((aligned(4))) uint8_t PD_Tx_Buf[34];
uint8_t PD_Ack_Buf[2];
PD_CONTROL PD_Ctl;
uint8_t Adapter_SrcCap[30]; // Index 0: Length, then 4-byte PDOs
uint8_t PDO_Len = 0;

static uint16_t g_requested_mv = 5000;
static uint8_t g_pending_request = 0;

void PD_Phy_SendPack(uint8_t mode, uint8_t *pbuf, uint8_t len, uint8_t sop);
void PD_Rx_Mode(void);
void PD_SINK_Init(void);
void PD_Load_Header(uint8_t ex, uint8_t msg_type);
uint8_t PD_Send_Handle(uint8_t *pbuf, uint8_t len);

void USBPD_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USBPD_IRQHandler(void) {
    if(USBPD->STATUS & IF_RX_ACT) {
        USBPD->STATUS |= IF_RX_ACT;
        if((USBPD->STATUS & MASK_PD_STAT) == PD_RX_SOP0) {
            if(USBPD->BMC_BYTE_CNT >= 6) {
                if((USBPD->BMC_BYTE_CNT != 6) || ((PD_Rx_Buf[0] & 0x1F) != DEF_TYPE_GOODCRC)) {
                    Delay_Us(30);
                    PD_Ack_Buf[0] = 0x41;
                    PD_Ack_Buf[1] = (PD_Rx_Buf[1] & 0x0E) | PD_Ctl.Flag.Bit.Auto_Ack_PRRole;
                    USBPD->CONFIG |= IE_TX_END;
                    PD_Phy_SendPack(0, PD_Ack_Buf, 2, UPD_SOP0);
                }
            }
        }
    }
    if(USBPD->STATUS & IF_TX_END) {
        USBPD->PORT_CC1 &= ~CC_LVE;
        USBPD->PORT_CC2 &= ~CC_LVE;
        NVIC_DisableIRQ(USBPD_IRQn);
        PD_Ctl.Flag.Bit.Msg_Recvd = 1;
        USBPD->STATUS |= IF_TX_END;
    }
    if(USBPD->STATUS & IF_RX_RESET) {
        USBPD->STATUS |= IF_RX_RESET;
        PD_SINK_Init();
    }
}

void PD_Rx_Mode(void) {
    USBPD->CONFIG |= PD_ALL_CLR;
    USBPD->CONFIG &= ~PD_ALL_CLR;
    USBPD->CONFIG |= IE_RX_ACT | IE_RX_RESET | PD_DMA_EN;
    USBPD->DMA = (uint32_t)(uint8_t *)PD_Rx_Buf;
    USBPD->CONTROL &= ~PD_TX_EN;
    USBPD->BMC_CLK_CNT = UPD_TMR_RX_48M;
    USBPD->CONTROL |= BMC_START;
    NVIC_EnableIRQ(USBPD_IRQn);
}

void PD_SINK_Init(void) {
    PD_Ctl.Flag.Bit.PR_Role = 0;
    PD_Ctl.Flag.Bit.Auto_Ack_PRRole = 0;
    USBPD->PORT_CC1 = CC_CMP_66 | CC_PD;
    USBPD->PORT_CC2 = CC_CMP_66 | CC_PD;
}

void USB_PD_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBPD, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    
    AFIO->CTLR |= USBPD_IN_HVT | USBPD_PHY_V33;
    USBPD->CONFIG = PD_DMA_EN;
    USBPD->STATUS = BUF_ERR | IF_RX_BIT | IF_RX_BYTE | IF_RX_ACT | IF_RX_RESET | IF_TX_END;
    
    memset(&PD_Ctl, 0, sizeof(PD_CONTROL));
    PD_SINK_Init();
    PD_Rx_Mode();
}

void PD_Phy_SendPack(uint8_t mode, uint8_t *pbuf, uint8_t len, uint8_t sop) {
    if (USBPD->CONFIG & CC_SEL) USBPD->PORT_CC2 |= CC_LVE;
    else USBPD->PORT_CC1 |= CC_LVE;

    USBPD->BMC_CLK_CNT = UPD_TMR_TX_48M;
    USBPD->DMA = (uint32_t)(uint8_t *)pbuf;
    USBPD->TX_SEL = sop;
    USBPD->BMC_TX_SZ = len;
    USBPD->CONTROL |= PD_TX_EN;
    USBPD->STATUS &= BMC_AUX_INVALID;
    USBPD->CONTROL |= BMC_START;

    if(mode) {
        while(!(USBPD->STATUS & IF_TX_END));
        USBPD->STATUS |= IF_TX_END;
        USBPD->PORT_CC1 &= ~CC_LVE;
        USBPD->PORT_CC2 &= ~CC_LVE;
        PD_Rx_Mode();
    }
}

void PD_Load_Header(uint8_t ex, uint8_t msg_type) {
    PD_Tx_Buf[0] = msg_type;
    if(PD_Ctl.Flag.Bit.PD_Version) PD_Tx_Buf[0] |= 0x80;
    else PD_Tx_Buf[0] |= 0x40;
    PD_Tx_Buf[1] = PD_Ctl.Msg_ID & 0x0E;
    if(ex) PD_Tx_Buf[1] |= 0x80;
}

uint8_t PD_Send_Handle(uint8_t *pbuf, uint8_t len) {
    uint8_t try_cnt = 3;
    while(try_cnt--) {
        NVIC_DisableIRQ(USBPD_IRQn);
        PD_Phy_SendPack(1, PD_Tx_Buf, len + 2, UPD_SOP0);
        uint16_t timeout = 1000;
        while(timeout--) {
            if(USBPD->STATUS & IF_RX_ACT) {
                USBPD->STATUS |= IF_RX_ACT;
                if(USBPD->BMC_BYTE_CNT == 6 && (PD_Rx_Buf[0] & 0x1F) == DEF_TYPE_GOODCRC) {
                    PD_Ctl.Msg_ID += 2;
                    PD_Rx_Mode();
                    return DEF_PD_TX_OK;
                }
            }
            Delay_Us(2);
        }
        PD_Rx_Mode();
    }
    return DEF_PD_TX_FAIL;
}

static void Request_PDO(uint8_t index, uint16_t mv, uint16_t ma) {
    uint8_t payload[4] = {0};
    uint8_t is_pps = (Adapter_SrcCap[1 + (index-1)*4 + 3] & 0xC0) == 0xC0;

    if (is_pps) {
        // PPS Request: Volts in 20mV units, Current in 50mA units
        uint16_t v_unit = mv / 20;
        uint16_t i_unit = ma / 50;
        payload[0] = i_unit & 0x7F;
        payload[1] = ((v_unit & 0x07) << 1) | (i_unit >> 7);
        payload[2] = (v_unit >> 3);
        payload[3] = (index << 4) | 0x01; // No capability mismatch
    } else {
        // Fixed Request: Current in 10mA units
        uint16_t i_unit = ma / 10;
        payload[0] = i_unit & 0xFF;
        payload[1] = (i_unit >> 8) & 0x03;
        payload[2] = (i_unit >> 10); // Should be 0
        payload[3] = (index << 4) | 0x02; // GiveBack = 0, Capability Mismatch = 0, USB Comm = 1 (0x02)
    }

    PD_Load_Header(0, DEF_TYPE_REQUEST);
    PD_Tx_Buf[1] |= (1 << 4); // 1 Data Object
    memcpy(&PD_Tx_Buf[2], payload, 4);
    
    if (PD_Send_Handle(&PD_Tx_Buf[2], 4) == DEF_PD_TX_OK) {
        PD_Ctl.PD_State = STA_RX_ACCEPT_WAIT;
    } else {
        PD_Ctl.PD_State = STA_TX_SOFTRST;
    }
}

void USB_PD_Request_Voltage(uint16_t mv) {
    g_requested_mv = mv;
    g_pending_request = 1;
}

static void Select_Best_PDO(void) {
    uint8_t best_idx = 0;
    uint16_t best_mv = 0;

    // Search for PPS first
    for (int i = 0; i < PDO_Len; i++) {
        uint8_t *pdo = &Adapter_SrcCap[1 + i*4];
        if ((pdo[3] & 0xC0) == 0xC0) { // Augmented PDO (PPS)
            uint16_t min_v = (pdo[0]) * 100;
            uint16_t max_v = ((pdo[2] & 0x01) << 7 | (pdo[1] >> 1)) * 100;
            if (g_requested_mv >= min_v && g_requested_mv <= max_v) {
                Request_PDO(i + 1, g_requested_mv, 1000); // 1A request
                return;
            }
        }
    }

    // Fallback to Fixed PDO
    for (int i = 0; i < PDO_Len; i++) {
        uint8_t *pdo = &Adapter_SrcCap[1 + i*4];
        if ((pdo[3] & 0xC0) == 0x00) { // Fixed
            uint16_t v = ((pdo[2] & 0x03) << 8 | pdo[1] >> 2) * 50;
            if (v <= g_requested_mv && v > best_mv) {
                best_mv = v;
                best_idx = i + 1;
            }
        }
    }

    if (best_idx) Request_PDO(best_idx, best_mv, 1000);
}

void USB_PD_Process(void) {
    static uint32_t last_tick = 0;
    uint32_t now = SysTick->CNT;
    uint32_t elapsed = (now - last_tick) / (SystemCoreClock / 1000);
    last_tick = now;

    if (elapsed > 100) elapsed = 1;

    switch(PD_Ctl.PD_State) {
        case STA_IDLE:
            if (!PD_Ctl.Flag.Bit.Connected) {
                // Detect connection
                USBPD->PORT_CC1 &= ~CC_CMP_Mask; USBPD->PORT_CC1 |= CC_CMP_22;
                USBPD->PORT_CC2 &= ~CC_CMP_Mask; USBPD->PORT_CC2 |= CC_CMP_22;
                Delay_Us(5);
                if (USBPD->PORT_CC1 & PA_CC_AI) {
                    USBPD->CONFIG &= ~CC_SEL; PD_Ctl.Flag.Bit.Connected = 1; PD_Ctl.PD_State = STA_SRC_CONNECT;
                } else if (USBPD->PORT_CC2 & PA_CC_AI) {
                    USBPD->CONFIG |= CC_SEL; PD_Ctl.Flag.Bit.Connected = 1; PD_Ctl.PD_State = STA_SRC_CONNECT;
                }
            } else if (g_pending_request) {
                g_pending_request = 0;
                Select_Best_PDO();
            }
            break;
        case STA_SRC_CONNECT:
            PD_Ctl.PD_Comm_Timer += elapsed;
            if (PD_Ctl.PD_Comm_Timer > 1000) { PD_Ctl.PD_State = STA_IDLE; PD_Ctl.Flag.Bit.Connected = 0; }
            break;
        case STA_RX_ACCEPT_WAIT:
        case STA_RX_PS_RDY_WAIT:
            PD_Ctl.PD_Comm_Timer += elapsed;
            if (PD_Ctl.PD_Comm_Timer > 500) PD_Ctl.PD_State = STA_TX_SOFTRST;
            break;
        case STA_TX_SOFTRST:
            PD_Load_Header(0, DEF_TYPE_SOFT_RESET);
            if (PD_Send_Handle(NULL, 0) == DEF_PD_TX_OK) PD_Ctl.PD_State = STA_IDLE;
            else PD_Ctl.PD_State = STA_IDLE; // Simplified
            break;
        default: break;
    }

    if (PD_Ctl.Flag.Bit.Msg_Recvd) {
        PD_Ctl.Flag.Bit.Msg_Recvd = 0;
        uint8_t type = PD_Rx_Buf[0] & 0x1F;
        uint8_t n_obj = (PD_Rx_Buf[1] >> 4) & 0x07;

        switch(type) {
            case DEF_TYPE_SRC_CAP:
                PDO_Len = n_obj;
                Adapter_SrcCap[0] = n_obj;
                memcpy(&Adapter_SrcCap[1], &PD_Rx_Buf[2], n_obj * 4);
                // On power on or reconnection, request 5V (PDO 1 is always 5V Fixed)
                Request_PDO(1, 5000, 1000);
                break;
            case DEF_TYPE_ACCEPT:
                PD_Ctl.PD_State = STA_RX_PS_RDY_WAIT;
                PD_Ctl.PD_Comm_Timer = 0;
                break;
            case DEF_TYPE_PS_RDY:
                PD_Ctl.PD_State = STA_IDLE;
                PD_Ctl.PD_Comm_Timer = 0;
                break;
            case DEF_TYPE_SOFT_RESET:
                PD_Load_Header(0, DEF_TYPE_ACCEPT);
                PD_Send_Handle(NULL, 0);
                break;
        }
        PD_Rx_Mode();
    }
}
