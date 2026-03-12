#pragma once
#include <stdint.h>

void App_Init(void);
void App_Tick(uint32_t ms);
void App_On_Pong(uint8_t device_id);
void App_Trigger_Discovery(void);
