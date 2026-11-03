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

/**
 * @brief Clears a TextField so the next updateText() call draws unconditionally.
 *
 * Call after the area behind the text has been wiped.
 *
 * @param[in,out] f  Field to reset (string, box and colour are zeroed).
 * @return           None.
 *
 * @code
 * TextField rpmF;
 * resetField(rpmF);
 * @endcode
 */
void resetField(TextField &f);
/**
 * @brief Draws centred text only if it changed, clearing leftovers of the old text.
 *
 * Returns immediately when the string and colour match what is already on screen.
 * Otherwise the new text is drawn opaque and any part of the old text's bounding
 * box not covered by the new text is erased. Uses the 6x8 built-in font scaled
 * by @p size.
 *
 * @param[in,out] f      Field remembering what is currently drawn.
 * @param areaX          Left edge of the area to centre in (px).
 * @param areaW          Width of that area (px).
 * @param y              Top of the text row (px).
 * @param size           Font scale (1 = 6x8 px per character).
 * @param s              New text; at most 11 characters are remembered in the field.
 * @param col            RGB565 text colour.
 * @return               None.
 *
 * @code
 * static TextField gearF;
 * updateText(gearF, 210, 60, 110, 8, "3", WHITE);
 * @endcode
 */
void updateText(TextField &f, int16_t areaX, int16_t areaW, int16_t y,
                uint8_t size, const char *s, uint16_t col);
/**
 * @brief Draws centred text unconditionally (no change tracking).
 *
 * Intended for static labels that are drawn once.
 *
 * @param s      Text to draw.
 * @param areaX  Left edge of the area to centre in (px).
 * @param areaW  Width of that area (px).
 * @param y      Top of the text row (px).
 * @param size   Font scale (1 = 6x8 px per character).
 * @param col    RGB565 text colour.
 * @return       None.
 *
 * @code
 * drawCentered("RPM", 210, 60, 226, 2, C_LABEL);
 * @endcode
 */
void drawCentered(const char *s, int16_t areaX, int16_t areaW, int16_t y,
                  uint8_t size, uint16_t col);

// ----------------------------------------------------
// Setup and touch
// ----------------------------------------------------
/**
 * @brief Initialises I2C, the TCA9554 expander, the ST7796 panel and the backlight.
 *
 * Pulses the LCD and touch reset lines through the expander, starts the display
 * at 40 MHz, clears the screen, turns the backlight on, then calls
 * showResetReason(). Prints an error message if the expander or panel fails.
 *
 * @return None.
 *
 * @code
 * void setup() {
 *   debugInit();
 *   displayInit();
 * }
 * @endcode
 */
void displayInit();        // I2C, TCA9554 reset, panel, backlight, last-reset notice
/**
 * @brief Shows "LAST RESET: <reason>" for 2.5 s if the last reset was abnormal.
 *
 * Shown for brownout, panic, and the interrupt/task/other watchdogs. Normal
 * resets (power-on, software, reset pin) show nothing. Blocks while displayed,
 * then clears the screen.
 *
 * @return None.
 *
 * @code
 * showResetReason();   // called at the end of displayInit()
 * @endcode
 */
void showResetReason();    // shows brownout / watchdog / crash on screen for a moment
/**
 * @brief Reads the FT6336 touch controller and maps the point to screen coordinates.
 *
 * Applies the TOUCH_SWAP_XY / TOUCH_FLIP_X / TOUCH_FLIP_Y settings so the result
 * matches the 480x320 landscape layout.
 *
 * @param[out] sx  Screen X (0..479). Only valid when true is returned.
 * @param[out] sy  Screen Y (0..319). Only valid when true is returned.
 * @return         true if 1 or 2 fingers are down and the read succeeded; false if
 *                 there is no touch or the I2C read failed.
 *
 * @code
 * int16_t x, y;
 * if (readTouch(x, y)) Serial.printf("touch at %d,%d\n", x, y);
 * @endcode
 */
bool readTouch(int16_t &sx, int16_t &sy);
/**
 * @brief Polls touch every 15 ms and forwards each new tap to slotHandleTap().
 *
 * Edge-triggered: one press produces one tap. The finger counts as lifted after
 * 3 consecutive polls with no touch. Call from loop() as often as possible.
 *
 * @return None.
 *
 * @code
 * void loop() {
 *   handleTouch();
 *   // ...
 * }
 * @endcode
 */
void handleTouch();        // polls touch and forwards taps to the widgets