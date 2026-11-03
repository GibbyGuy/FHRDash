#pragma once
/*
 * sensors.h - sensor definitions, live data, layout constants, and the public
 * drawing entry points for the gauge cluster.
 *
 * Layout (480 x 320):
 *   top centre : notification icons (flash when something is out of range)
 *   centre     : tachometer arc + gear + RPM
 *   bottom ctr : speedometer (number + animated segment bar)
 *   both sides : 3 tappable widgets each
 *
 * Gear shift pattern: 1 -> N -> 2 -> 3 -> 4 -> 5 -> 6
 *   gear = 0 means Neutral (between 1st and 2nd), 1..6 = gears.
 */

#include <Arduino.h>
#include "controls.h"

// ----------------------------------------------------
// Sensors
// ----------------------------------------------------
enum {
  S_OIL_T, S_CLT, S_ADV, S_KNOCK, S_LAUNCH, S_OIL_P,
  S_BATT, S_AFR, S_BOOST, S_EGT, S_IAT, S_FUEL_P,
  S_COUNT
};

struct SensorDef {
  const char *name;
  const char *unit;
  float minV, maxV;
  float goodLo, goodHi;
  uint8_t dec;
  bool isState;
};

extern const SensorDef DEFS[S_COUNT];
extern const char *LAUNCH_STR[3];
extern const char *ALERT_L1[S_COUNT];
extern const char *ALERT_L2[S_COUNT];

// Live data (filled from the CAN bus, or the simulator)
// Written by the CAN task (core 0), read by the display loop (core 1). Plain 32-bit
// values, so single reads/writes are atomic and no locking is needed.
extern volatile float   rpm;
extern volatile uint8_t gear;            // 0 = Neutral, 1..6 = gear
extern volatile float   speedMph;
extern volatile float   sens[S_COUNT];
extern volatile bool    sensSeen[S_COUNT];   // true once a value has been received for that sensor

// ----------------------------------------------------
// Tachometer config
// ----------------------------------------------------
#define MAX_RPM      14000
#define SHIFT_RPM    11000
#define REDLINE_RPM  12500

const int TACH_CX = 240, TACH_CY = 158, TACH_RO = 106, TACH_RI = 80;
#define TACH_N 40
const float TACH_START = 210.0f;
const float TACH_SWEEP = 240.0f;

// ----------------------------------------------------
// Speedometer config
// ----------------------------------------------------
#define SPEED_MAX    100.0f   // mph at full bar
#define SPD_N        25
#define SPD_BAR_X    140
#define SPD_BAR_Y    296
#define SPD_SEG_W    7
#define SPD_SEG_H    14
#define SPD_PITCH    8
#define SPD_NUM_Y    248
#define SPD_NUM_AREA_X 180
#define SPD_NUM_AREA_W 72

// ----------------------------------------------------
// Side widgets
// ----------------------------------------------------
#define NUM_SLOTS 6
#define SLOT_W 126
#define SLOT_H 102
#define BAR_X_OFF 6
#define BAR_W 114
#define BAR_Y_OFF 84
#define BAR_H 12
#define MARK_W 4

// ----------------------------------------------------
// Notification icons (top centre)
//   Any sensor outside its acceptable range raises a flashing icon:
//     orange + slow flash = warning, red + fast flash = critical.
//   Launch control: armed = steady white "LC", active = fast-flashing green.
//   Up to 5 icons; highest priority wins if more are active.
// ----------------------------------------------------
#define MAX_ALERTS   5
#define ICON_SZ      40
#define ICON_PITCH   44
#define ALERT_Y      6
#define STRIP_X      132
#define STRIP_Y      4
#define STRIP_W      216
#define STRIP_H      44

// ----------------------------------------------------
// Public functions
// ----------------------------------------------------
/**
 * @brief Prepares the gauge cluster: loads saved widget layout and draws the static tach and speedometer.
 *
 * Opens the "kartdash" preferences, loads the six widget assignments (falling
 * back to defaults if invalid), sets every sensor to its minimum value, marks
 * sensors as seen only in SIMULATE mode, and draws the tach and speed bar.
 * Call after displayInit().
 *
 * @return None.
 *
 * @code
 * displayInit();
 * sensorsInit();
 * @endcode
 */
void sensorsInit();                          // prefs, widget layout, static tach/speed drawing
/**
 * @brief Runs one display frame: reads data, then redraws only what changed.
 *
 * Updates the simulator (if enabled), tach, speedometer, alert icons and all six
 * side widgets. Intended to be called about 30 times a second.
 *
 * @return None.
 *
 * @code
 * void loop() {
 *   static uint32_t last = 0;
 *   if (millis() - last >= 33) { last = millis(); sensorsUpdate(); }
 * }
 * @endcode
 */
void sensorsUpdate();                        // one frame: read data + redraw whatever changed
/**
 * @brief Startup animation: tach and speed bar sweep up to full and back to zero.
 *
 * Two smoothstep passes of 31 steps at 75 ms each (about 4.7 s). Blocks until
 * done and leaves rpm, speed and the speed peak marker at zero.
 *
 * @return None.
 *
 * @code
 * sensorsInit();
 * #if BOOT_SWEEP
 *   bootSweep();
 * #endif
 * @endcode
 */
void bootSweep();                            // startup sweep of tach + speed bar
/**
 * @brief Handles a tap: if it lands on a side widget, switches that widget to the next unused sensor.
 *
 * The new choice is saved to flash and the widget is fully redrawn on the next
 * frame. Sensors already shown in another widget are skipped.
 *
 * @param x  Tap X in screen pixels.
 * @param y  Tap Y in screen pixels.
 * @return   true if the tap hit a widget, false if it hit empty space.
 *
 * @code
 * if (slotHandleTap(x, y)) Serial.println("widget cycled");
 * @endcode
 */
bool slotHandleTap(int16_t x, int16_t y);    // true if the tap hit a widget (and cycled it)