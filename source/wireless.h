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

/**
 * @brief Starts the Wi-Fi access point, web server and WebSocket stream of raw CAN frames.
 *
 * Creates the frame queue, registers the CAN frame hook, and launches the "wifi"
 * task on core 0. The task waits 1.5 s, starts the AP (SSID/password from this
 * file, max 2 clients), serves the dashboard page at http://192.168.4.1/ and
 * streams frames on WebSocket port ::WS_PORT. Does nothing when ENABLE_WIRELESS is 0.
 *
 * @return None.
 *
 * @code
 * sensorsInit();
 * wirelessBegin();
 * startCanTask();
 * @endcode
 */
void     wirelessBegin();             // no-op when ENABLE_WIRELESS is 0
/**
 * @brief Returns the number of WebSocket clients currently connected.
 *
 * @return Client count, or 0 when ENABLE_WIRELESS is 0.
 *
 * @code
 * if (wirelessClients() > 0) debugMsg("laptop connected");
 * @endcode
 */
uint8_t  wirelessClients();
/**
 * @brief Returns how many CAN frames were lost because the send queue was full.
 *
 * @return Dropped frame count since boot, or 0 when ENABLE_WIRELESS is 0.
 *
 * @code
 * Serial.printf("dropped: %u\n", (unsigned)wirelessDropped());
 * @endcode
 */
uint32_t wirelessDropped();           // frames lost because the queue was full