#ifndef USB_PD_H
#define USB_PD_H

#include <stdint.h>

void USB_PD_Init(void);
void USB_PD_Process(void);
void USB_PD_Request_Voltage(uint16_t mv);

#endif
