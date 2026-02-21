#include "uart_bus.h"
#include "crc8.h"
#include "servo.h"
#include "adc.h"
#include "usb_pd.h"

typedef struct {
    uint8_t buf[32];
    uint8_t idx;
} uart_pkt_t;

static uart_pkt_t pkt1, pkt2;

void setup_uart_bus() {
    RCC->APB2PCENR |= RCC_USART1EN | RCC_IOPAEN;
    RCC->APB1PCENR |= RCC_USART2EN;

    GPIOA->CFGLR &= ~((0xf << (4 * 2)) | (0xf << (4 * 5)));
    GPIOA->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (4 * 2);
    GPIOA->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (4 * 5);

    uint32_t brr = (48000000 + UART_BAUD / 2) / UART_BAUD;
    
    USART1->BRR = brr;
    USART1->CTLR3 |= USART_CTLR3_HDSEL;
    USART1->CTLR1 |= USART_CTLR1_TE | USART_CTLR1_RE | USART_CTLR1_UE;

    USART2->BRR = brr;
    USART2->CTLR3 |= USART_CTLR3_HDSEL;
    USART2->CTLR1 |= USART_CTLR1_TE | USART_CTLR1_RE | USART_CTLR1_UE;
}

static void uart_send_byte(USART_TypeDef *u, uint8_t b) {
    while (!(u->STATR & USART_STATR_TXE));
    u->DATAR = b;
}

void process_packet(uint8_t *buf, size_t len, USART_TypeDef *src_uart) {
    uint8_t cmd = buf[2];
    uint8_t datalen = buf[3];
    uint8_t *data = &buf[4];

    if (cmd == CMD_SERVO_SET && datalen >= 3) {
        uint8_t idx = data[0];
        uint16_t pos = (data[1] << 8) | data[2];
        set_servo_pos(idx, pos);
    } else if (cmd == CMD_SENSOR_REQ) {
        uint16_t v = read_adc(ADC_CH_VSENSE);
        uint16_t t = read_adc(ADC_CH_TEMPSENSE);
        uint8_t res[8];
        res[0] = PKT_HEADER; res[1] = HOST_ID; res[2] = RES_SENSOR_DATA; res[3] = 4;
        res[4] = v >> 8; res[5] = v & 0xFF; res[6] = t >> 8; res[7] = t & 0xFF;
        uint8_t crc = crc8(res, 8);
        if (src_uart) {
            for(int i=0; i<8; i++) uart_send_byte(src_uart, res[i]);
            uart_send_byte(src_uart, crc);
        }
    } else if (cmd == CMD_PD_SET_VOLT && datalen >= 2) {
        uint16_t mv = (data[0] << 8) | data[1];
        usb_pd_request_pps(mv, 50);
    }
}

static void poll_one_uart(USART_TypeDef *u, USART_TypeDef *other, uart_pkt_t *p) {
    if (u->STATR & USART_STATR_RXNE) {
        uint8_t b = u->DATAR;
        if (p->idx == 0 && b != PKT_HEADER) return;
        p->buf[p->idx++] = b;
        
        if (p->idx >= 4) {
            uint8_t dest = p->buf[1];
            uint8_t len = p->buf[3];
            if (p->idx >= (4 + len + 1)) {
                if (crc8(p->buf, p->idx - 1) == p->buf[p->idx - 1]) {
                    GPIOB->OUTDR ^= (1 << 12); // Visual feedback
                    if (dest == DEVICE_ID) {
                        process_packet(p->buf, p->idx, USART1);
                    } else {
                        for (int i = 0; i < p->idx; i++) uart_send_byte(USART1, p->buf[i]);
                    }
                }
                p->idx = 0;
            }
        }
        if (p->idx >= sizeof(p->buf)) p->idx = 0;
    }
}

void uart_bus_poll() {
    poll_one_uart(USART1, USART2, &pkt1);
    poll_one_uart(USART2, USART1, &pkt2);
}
