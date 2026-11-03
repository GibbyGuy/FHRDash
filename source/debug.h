#pragma once
/*
 * debug.h - everything that writes to the serial port.
 * Each function checks its own flag from controls.h and compiles to
 * (almost) nothing when that flag is off.
 */

#include <Arduino.h>
#include <esp_system.h>
#include "controls.h"


/**
 * @brief Starts the serial port and prints why the chip last reset.
 *
 * Restarts Serial with a 4 KB TX buffer at 115200 baud. With USB CDC on boot,
 * the TX timeout is set to 0 so the dash never blocks when no PC is attached.
 *
 * @return None.
 *
 * @code
 * void setup() { debugInit(); }   // prints "Reset reason: POWER-ON"
 * @endcode
 */
void debugInit();                                   // Serial.begin + reset reason
/**
 * @brief Prints one line of plain text to the serial port.
 *
 * @param msg  Null-terminated string; a newline is appended.
 * @return     None.
 *
 * @code
 * debugMsg("MCP2515 init OK");
 * @endcode
 */
void debugMsg(const char *msg);                     // one plain line

/**
 * @brief Converts an ESP32 reset reason code to a short readable name.
 *
 * @param r  Reset reason, e.g. from esp_reset_reason().
 * @return   Static string such as "BROWNOUT (POWER)" or "TASK WDT";
 *           "UNKNOWN" for unlisted codes. Never nullptr.
 *
 * @code
 * Serial.println(resetReasonName(esp_reset_reason()));
 * @endcode
 */
const char *resetReasonName(esp_reset_reason_t r);

// CAN
/**
 * @brief Prints which bitrate/crystal combination autobaud is about to try.
 *
 * @param rateName  Bitrate label, e.g. "500k".
 * @param clkName   Crystal label, e.g. "8MHz".
 * @return          None.
 *
 * @code
 * debugAutobaudTry("500k", "8MHz");
 * @endcode
 */
void debugAutobaudTry(const char *rateName, const char *clkName);
/**
 * @brief Prints that autobaud found traffic and saved the setting.
 *
 * @param rateName  Bitrate label that worked.
 * @param clkName   Crystal label that worked.
 * @return          None.
 *
 * @code
 * debugAutobaudFound("500k", "8MHz");
 * @endcode
 */
void debugAutobaudFound(const char *rateName, const char *clkName);
/**
 * @brief Prints a received CAN frame (ID and raw bytes), rate-limited per ID.
 *
 * Only active when CAN_DEBUG is 1. Each distinct ID prints at most once every
 * 250 ms, and up to 48 distinct IDs are tracked, so the serial port cannot flood.
 *
 * @param id   CAN identifier.
 * @param ext  true for a 29-bit extended ID, false for standard 11-bit.
 * @param len  Number of data bytes to print (0..8).
 * @param buf  Frame payload.
 * @param now  Current time in ms (millis()).
 * @return     None.
 *
 * @code
 * debugCanFrame(id, ext, len, buf, millis());   // "STD 0x640 [8]: 1F 40 ..."
 * @endcode
 */
void debugCanFrame(uint32_t id, bool ext, uint8_t len, const uint8_t *buf, uint32_t now);
/**
 * @brief Tells the caller whether it is time for the once-a-second status print.
 *
 * Always false when CAN_STATUS is 0.
 *
 * @param now  Current time in ms (millis()).
 * @return     true about once per second, false otherwise.
 *
 * @code
 * if (debugStatusDue(millis())) debugCanStatus(frameCount, CAN0.getError(),
 *     CAN0.errorCountRX(), CAN0.errorCountTX(), digitalRead(CAN_INT));
 * @endcode
 */
bool debugStatusDue(uint32_t now);                  // true about once a second
/**
 * @brief Prints the CAN bus counters followed by the decoded [DASH] line.
 *
 * Only active when CAN_STATUS is 1.
 *
 * @param frames  Total frames received so far.
 * @param eflg    MCP2515 error flag register.
 * @param rxErr   Receive error counter.
 * @param txErr   Transmit error counter.
 * @param intPin  Level read from the MCP2515 INT pin.
 * @return        None.
 *
 * @code
 * debugCanStatus(frameCount, CAN0.getError(), CAN0.errorCountRX(),
 *                CAN0.errorCountTX(), digitalRead(CAN_INT));
 * @endcode
 */
void debugCanStatus(uint32_t frames, uint8_t eflg, uint8_t rxErr, uint8_t txErr, int intPin);
/**
 * @brief Prints the decoded dash values (rpm, gear, speed and every sensor) on one line.
 *
 * Sensors not yet received print as "--". Use it to compare the decoded numbers
 * with M1 Tune. Only active when CAN_STATUS is 1.
 *
 * @return None.
 *
 * @code
 * debugDash();   // [DASH] rpm 8000  gear 3  mph 54.2 | OILT 212.0 CLT 190.4 ...
 * @endcode
 */
void debugDash();                                   // decoded [DASH] line

// Touch
/**
 * @brief Prints raw and mapped touch coordinates (only when TOUCH_DEBUG is 1).
 *
 * @param rx  Raw X from the FT6336 (portrait).
 * @param ry  Raw Y from the FT6336 (portrait).
 * @param sx  Mapped screen X (landscape).
 * @param sy  Mapped screen Y (landscape).
 * @return    None.
 *
 * @code
 * debugTouch(rx, ry, sx, sy);   // "touch raw(120,300) -> (300,19)"
 * @endcode
 */
void debugTouch(int16_t rx, int16_t ry, int16_t sx, int16_t sy);