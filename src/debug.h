#pragma once
/*
 * debug.h - everything that writes to the serial port.
 * Each function checks its own flag from controls.h and compiles to
 * (almost) nothing when that flag is off.
 */

#include <Arduino.h>
#include <esp_system.h>
#include "controls.h"


void debugInit();                                   // Serial.begin + reset reason
void debugMsg(const char *msg);                     // one plain line

const char *resetReasonName(esp_reset_reason_t r);

// CAN
void debugAutobaudTry(const char *rateName, const char *clkName);
void debugAutobaudFound(const char *rateName, const char *clkName);
void debugCanFrame(uint32_t id, bool ext, uint8_t len, const uint8_t *buf, uint32_t now);
bool debugStatusDue(uint32_t now);                  // true about once a second
void debugCanStatus(uint32_t frames, uint8_t eflg, uint8_t rxErr, uint8_t txErr, int intPin);
void debugDash();                                   // decoded [DASH] line

// Touch
void debugTouch(int16_t rx, int16_t ry, int16_t sx, int16_t sy);