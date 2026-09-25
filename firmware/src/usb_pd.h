#ifndef USB_PD_H
#define USB_PD_H

#include <stdint.h>

/* Accepted USB-PD request range (#38). 16.8 V = 4S LiPo max, kept below the
 * 3.3 V LDO input rating; requests outside are rejected with ERR_BAD_VALUE. */
#define PD_MIN_MV        5000
#define PD_MAX_SAFE_MV   16800

void USB_PD_Init(void);
void USB_PD_Process(void);
void USB_PD_Request_Voltage(uint16_t mv);

#endif
