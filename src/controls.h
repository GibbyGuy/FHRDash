#pragma once
/*
 * controls.h - compile-time mode selects.
 * Included by every other file. Includes nothing itself.
 */

#define SIMULATE         0   // 1 = run demo data, 0 = read real data from the CAN bus
#define BOOT_SWEEP       1   // 1 = sweep the tach and speed bar at startup
#define CAN_CLEAR_SAVED  1


#define CAN_DEBUG        1   // 1 = print every distinct CAN ID with raw bytes (rate-limited)
#define CAN_STATUS       0   // 1 = once a second: [CAN] bus counters and [DASH] decoded values
#define CAN_LISTEN_ONLY  0   // 1 = never ACK (only if other nodes ACK the ECU)
#define CAN_AUTOBAUD     1   // 1 = at boot, scan bitrates/crystals for traffic (adds up to ~12 s)

#define TOUCH_DEBUG      0   // 1 = print raw and mapped touch coordinates

#define ENABLE_WIRELESS  1   // reserved for wireless.h / wireless_functions.cpp