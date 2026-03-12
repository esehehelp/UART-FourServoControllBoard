#include "app.h"
#include "protocol.h"
#include "config.h"

#define DISCOVERY_TIMEOUT_MS  100
#define MAX_DEVICES           16

extern volatile uint32_t g_ms_ticks;

static uint8_t  g_device_list[MAX_DEVICES];
static uint8_t  g_device_count;
static uint32_t g_discovery_start_ms;
static uint8_t  g_discovery_active;

void App_Init(void) {
    g_discovery_active = 0;
    g_device_count = 0;
}

void App_Trigger_Discovery(void) {
    g_device_count = 0;
    g_discovery_active = 1;
    g_discovery_start_ms = g_ms_ticks;
    // Broadcast Ping into ring via UART2
    Send_Packet(IF_UART2, BROADCAST_ID, g_config.device_id, 0xA0, NULL, 0);
}

void App_On_Pong(uint8_t device_id) {
    if (g_discovery_active && g_device_count < MAX_DEVICES) {
        g_device_list[g_device_count++] = device_id;
    }
}

void App_Tick(uint32_t ms) {
    if (!g_discovery_active) return;
    if ((ms - g_discovery_start_ms) >= DISCOVERY_TIMEOUT_MS) {
        g_discovery_active = 0;
        // Send collected device list back to USB host
        Send_Packet(IF_USB, HOST_ID, g_config.device_id, 0xA1,
                    g_device_list, g_device_count);
    }
}
