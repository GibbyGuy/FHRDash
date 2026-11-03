#pragma once
/*
 * wireless.h - Wi-Fi access point + WebSocket stream of raw CAN frames.
 * The laptop dashboard (dash_page.h) is served from the ESP32 at http://192.168.4.1/
 */
#include <Arduino.h>
#include "controls.h"

#define WIFI_SSID     "KartDash"
#define WIFI_PASS     "changeme123"   // 8+ characters
#define WIFI_CHANNEL  1               // use 1, 6 or 11
#define WS_PORT       81

void     wirelessBegin();             // no-op when ENABLE_WIRELESS is 0
uint8_t  wirelessClients();
uint32_t wirelessDropped();           // frames lost because the queue was full