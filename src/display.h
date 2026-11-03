#pragma once
/*
 * display.h - Waveshare ESP32-S3-Touch-LCD-3.5 (ST7796 480x320 landscape, FT6336 touch)
 * Pins, palette, touch orientation, delta-drawn text helpers, and display/touch setup.
 *
 * Text is only redrawn when the string/colour changes, and the leftover part of
 * the OLD text's bounding box is cleared explicitly.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include "TCA9554.h"
#include "controls.h"

// ----------------------------------------------------
// Pin definitions
// ----------------------------------------------------
#define GFX_BL 6  // default backlight pin

#define SPI_MISO 2
#define SPI_MOSI 1
#define SPI_SCLK 5

#define LCD_CS -1
#define LCD_DC 3
#define LCD_RST -1
#define LCD_HOR_RES 320
#define LCD_VER_RES 480

#define I2C_SDA 8
#define I2C_SCL 7

// I2C Address for FT6336 Touch Controller
#define TOUCH_I2C_ADD 0x38

// TCA9554 expander pin roles (edit here if your board revision differs)
#define EXIO_LCD_RST 1
#define EXIO_TP_RST  2

// ----------------------------------------------------
// Touch orientation. Raw FT6336 coords are portrait (320x480).
// ----------------------------------------------------
#define TOUCH_SWAP_XY 1
#define TOUCH_FLIP_X  0
#define TOUCH_FLIP_Y  1

// ----------------------------------------------------
// Screen / palette
// ----------------------------------------------------
#ifndef BLACK
#define BLACK 0x0000
#endif
#ifndef RED
#define RED   0xF800
#endif
#ifndef WHITE
#define WHITE 0xFFFF
#endif

#define SCR_W 480
#define SCR_H 320

#define C_BG        BLACK
#define C_LABEL     0xBDF7
#define C_BORDER    0x4208
#define C_ARC_OFF   0x2104
#define C_YELLOW    0xFFE0
#define C_ORANGE    0xFD20
#define C_GREEN     0x07E0
#define C_ZONE_GOOD 0x4A49
#define C_ZONE_BAD  0x5000

// ----------------------------------------------------
// Hardware interfaces (defined in display_functions.cpp)
// ----------------------------------------------------
extern TCA9554       TCA;
extern Arduino_GFX  *gfx;

// ----------------------------------------------------
// Delta-drawn text
// ----------------------------------------------------
struct TextField {
  char     s[12];
  int16_t  x, w;    // last drawn bounding box (x extent)
  uint16_t col;
};

void resetField(TextField &f);
void updateText(TextField &f, int16_t areaX, int16_t areaW, int16_t y,
                uint8_t size, const char *s, uint16_t col);
void drawCentered(const char *s, int16_t areaX, int16_t areaW, int16_t y,
                  uint8_t size, uint16_t col);

// ----------------------------------------------------
// Setup and touch
// ----------------------------------------------------
void displayInit();        // I2C, TCA9554 reset, panel, backlight, last-reset notice
void showResetReason();    // shows brownout / watchdog / crash on screen for a moment
bool readTouch(int16_t &sx, int16_t &sy);
void handleTouch();        // polls touch and forwards taps to the widgets