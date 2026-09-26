/* Host-side stubs for the hardware functions protocol.c calls. They record
 * what the firmware would have done so the tests can check it. */
#include "protocol.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
int led1=-1, led2=-1, config_saves=0, flash_ok=1; uint16_t servo_pos[4]; int pd_mv=-1;
uint8_t tx[8][512]; int txn[8];
void LED1_SetDuty(uint8_t d){ led1=d; }
void LED2_SetDuty(uint8_t d){ led2=d; }
void Set_Servo(uint8_t i, uint16_t p){ if(i<4) servo_pos[i]=p; }
void Servo_Free(uint8_t m){ (void)m; }
uint16_t Get_ADC_Val(uint8_t c){ (void)c; return 0; }
volatile uint16_t g_servo_feedback[4];
void USB_PD_Request_Voltage(uint16_t mv){ pd_mv=mv; }
void App_Trigger_Discovery(void){}
void App_On_Pong(uint8_t id){ (void)id; }
/* Flash emulation for config.c (built with -DCONFIG_FLASH_PTR=host_flash) */
uint8_t host_flash[256];
FLASH_Status FLASH_ROM_ERASE(uint32_t a, uint32_t l){ (void)a; memset(host_flash, 0xFF, l); return FLASH_COMPLETE; }
FLASH_Status FLASH_ROM_WRITE(uint32_t a, uint32_t *p, uint32_t l){
  (void)a; config_saves++;
  if (!flash_ok) return FLASH_OP_RANGE_ERROR;
  memcpy(host_flash, p, l); return FLASH_COMPLETE; }
FlagStatus USART_GetFlagStatus(USART_TypeDef* u, uint16_t f){ (void)u;(void)f; return SET; }
void USART_SendData(USART_TypeDef* u, uint16_t d){ int k = (u==USART2)?IF_UART2:IF_UART4; tx[k][txn[k]++]=(uint8_t)d; }
uint8_t USBFS_Endp_DataUp(uint8_t e, uint8_t *b, uint16_t l, uint8_t m){ (void)e;(void)m; memcpy(tx[IF_USB]+txn[IF_USB],b,l); txn[IF_USB]+=l; return 0; }
