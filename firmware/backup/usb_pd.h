#ifndef _USB_PD_H
#define _USB_PD_H

#include "config.h"

void setup_usb_pd();
void usb_pd_poll();
void usb_pd_request_pps(uint16_t mv, uint8_t ma_step); // mv: 3300-21000

#endif
