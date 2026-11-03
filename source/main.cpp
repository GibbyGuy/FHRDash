/*
 * Go-Kart Gauge Cluster
 * Waveshare ESP32-S3-Touch-LCD-3.5  (ST7796 480x320 landscape, FT6336 touch)
 *
 * Build flag needed in platformio.ini (so Serial goes over USB and GPIO43/44 stay free for CAN):
 *   -DARDUINO_USB_CDC_ON_BOOT=1
 *
 * Cores: the display loop runs on core 1. CAN runs in its own task on core 0.
 */

#include <Arduino.h>
#include "controls.h"
#include "debug.h"
#include "display.h"
#include "sensors.h"
#include "can.h"
#include "wireless.h"

/**
 * @brief Arduino entry point: brings up serial, display, gauges, Wi-Fi and the CAN task, in that order.
 *
 * CAN starts last so the dash is on screen before autobaud (which can take
 * several seconds) begins.
 *
 * @return None.
 */
void setup() {
  debugInit();
  displayInit();         // I2C, TCA9554 reset, panel, backlight, last-reset notice
  sensorsInit();         // prefs, widget layout, static tach/speed drawing
#if BOOT_SWEEP
  bootSweep();
#endif
  wirelessBegin();       // Wi-Fi AP + server, registers the CAN frame hook
  startCanTask();        // CAN starts after the dash is on screen
}

/**
 * @brief Arduino main loop (core 1): polls touch continuously and redraws the gauges at about 30 fps.
 *
 * @return None.
 */
void loop() {
  static uint32_t lastFrame = 0;

  handleTouch();

  if (millis() - lastFrame < 33) return;   // ~30 fps
  lastFrame = millis();

  sensorsUpdate();
}