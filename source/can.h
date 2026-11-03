#pragma once
/*
 * can.h - CAN bus (MCP2515) constants, signal table and function declarations.
 *
 * SPI:   the display uses SPI3 (HSPI); the CAN board uses SPI2 (FSPI). They must
 *        never share a peripheral, or every CAN read is also clocked into the display.
 * Cores: CAN runs in its own task on core 0; the display loop runs on core 1.
 *
 * Signal map: MoTeC M1 "General" CAN protocol, standard 11-bit IDs 0x640-0x65F.
 *   value = raw * scale + offset, already converted to the dash's units
 *   (temps degF, pressures psi, speed mph).
 */

#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>
#include "controls.h"
#include "sensors.h"

// GPIO pins for MCP2515 CAN-to-SPI board
#define CAN_CS     44
#define CAN_MISO   43
#define CAN_MOSI   48
#define CAN_SCK    47
#define CAN_INT    46

// Signal targets
enum { T_RPM, T_SPEED, T_GEAR, T_SENS };

#define KPA01_TO_PSI    0.0145038f   // 0.1 kPa units -> psi
#define DEGC01_TO_F     0.18f        // 0.1 degC units -> degF (then +32)
#define KMH01_TO_MPH    0.0621371f   // 0.1 km/h units -> mph
#define AFR_PER_LAMBDA  14.7f        // gasoline stoich; E85 = 9.76, methanol = 6.45

struct CanSig {
  uint32_t id;
  uint8_t  bit;        // start bit (spreadsheet "Start Bit")
  uint8_t  len;        // length in bits: 1, 4, 8 or 16
  bool     isSigned;
  float    scale, offset;
  uint8_t  target;     // T_*
  uint8_t  sensor;     // S_* when target == T_SENS
};

extern const CanSig SIGS[];
extern const int    NUM_SIGS;

// Bus objects and state
extern SPIClass canSPI;
extern MCP_CAN  CAN0;
extern bool     canOk;
extern uint32_t frameCount;
extern uint8_t  canRate;
extern uint8_t  canClk;

// Optional hook called for every received frame (from the CAN task on core 0).
// Intended for wireless.h to forward raw frames; leave unset to disable.
typedef void (*CanFrameHook)(uint32_t id, uint8_t len, const uint8_t *buf);
/**
 * @brief Registers a callback that is run for every CAN frame the dash receives.
 *
 * The hook is called from the CAN task on core 0, so it must be fast and must
 * never block. Used by wireless.h to forward raw frames to the laptop.
 *
 * @param cb  Function matching ::CanFrameHook, or nullptr to disable forwarding.
 * @return    None.
 *
 * @code
 * static void myHook(uint32_t id, uint8_t len, const uint8_t *buf) {
 *   if (id == 0x640) Serial.println("got RPM frame");
 * }
 * canSetFrameHook(myHook);
 * @endcode
 */
void canSetFrameHook(CanFrameHook cb);

/**
 * @brief Decodes one signal from a CAN payload into dash units.
 *
 * Fields of 8 bits or more are read whole-byte, big-endian. Smaller fields
 * (flags, nibbles) are read LSB-first inside their byte. Signed fields are
 * sign-extended, then value = raw * scale + offset. A 8/16-bit field whose raw
 * value is all-ones (unsigned) or signed-max (signed) is treated as "channel
 * not configured in the tune" and rejected.
 *
 * @param[in]  s    Signal definition (an entry of ::SIGS).
 * @param[in]  buf  Frame payload; must hold at least (s.bit + s.len + 7) / 8 bytes.
 * @param[out] out  Decoded value. Only written when the function returns true.
 * @return     true if a valid value was decoded, false if the channel is unconfigured.
 *
 * @code
 * const byte frame[8] = {0x1F, 0x40, 0, 0, 0, 0, 0, 0};   // 0x1F40 = 8000
 * float v;
 * if (decodeSignal(SIGS[0], frame, v)) {                  // SIGS[0] = engine speed
 *   Serial.println(v);                                    // 8000.0
 * }
 * @endcode
 */
bool decodeSignal(const CanSig &s, const byte *buf, float &out);
/**
 * @brief Decodes a signal and stores it in the live dash data.
 *
 * Writes ::rpm, ::speedMph, ::gear or ::sens[] (and sets ::sensSeen[]) depending
 * on CanSig::target. Gear is rounded and clamped to 0..6 (reverse becomes N).
 * Does nothing if the channel is unconfigured (see decodeSignal()).
 *
 * @param[in] s    Signal definition to apply.
 * @param[in] buf  Frame payload (at least (s.bit + s.len + 7) / 8 bytes).
 * @return    None.
 *
 * @code
 * for (int k = 0; k < NUM_SIGS; k++)
 *   if (SIGS[k].id == id) applySignal(SIGS[k], buf);
 * @endcode
 */
void applySignal(const CanSig &s, const byte *buf);
/**
 * @brief Listens on the bus with one bitrate/crystal setting and reports whether real traffic appears.
 *
 * Re-initialises the MCP2515 with the given settings in listen-only mode (so it
 * never ACKs or disturbs the bus), then counts frames for up to @p ms. Success
 * needs 3 plausible frames (standard ID in ::SIGS, legal length). Prints a
 * summary with CANINTF/EFLG so a silent bus can be told apart from a wrong bitrate.
 * The chip is left in listen-only mode; call CAN0.begin() afterwards to use it.
 *
 * @param rate  Bitrate constant from mcp_can, e.g. CAN_500KBPS.
 * @param clk   MCP2515 crystal constant, MCP_8MHZ or MCP_16MHZ.
 * @param ms    Maximum listening time in milliseconds.
 * @return      true if 3 plausible frames arrived in time, false otherwise
 *              (including when begin() fails).
 *
 * @code
 * if (canTry(CAN_500KBPS, MCP_8MHZ, 1500)) {
 *   debugMsg("bus found at 500k / 8 MHz");
 * }
 * @endcode
 */
bool canTry(uint8_t rate, uint8_t clk, uint32_t ms);
/**
 * @brief Brings up the CAN interface: SPI, chip reset, bitrate selection and MCP2515 init.
 *
 * Sets up the FSPI bus, resets the chip, loads the last verified bitrate/crystal
 * from flash (namespace "kartcan"), and (when CAN_AUTOBAUD is 1) verifies it,
 * scanning 1M/500k/250k/125k with 8 and 16 MHz crystals if it fails. A newly
 * found setting is saved. Finally starts the chip in normal or listen-only mode
 * (CAN_LISTEN_ONLY). Sets ::canOk to true on success. If CAN_CLEAR_SAVED is 1,
 * the saved setting is erased once per boot first.
 *
 * Takes up to ~12 s with autobaud, so it runs inside canTask(), not setup().
 *
 * @return None. Check ::canOk for the result.
 *
 * @code
 * canSetup();
 * if (!canOk) debugMsg("CAN init failed");
 * @endcode
 */
void canSetup();
/**
 * @brief Drains frames waiting in the MCP2515 and applies them to the live data.
 *
 * Reads up to 32 frames per call straight from the chip (no INT pin needed).
 * Corrupt frames (length > 8, remote requests) are counted and skipped. Valid
 * standard-ID frames are logged (CAN_DEBUG), passed to the frame hook, and
 * decoded with applySignal() for every matching ::SIGS entry. Does nothing
 * when ::canOk is false.
 *
 * @return None. Updates ::frameCount and the shared dash variables.
 *
 * @code
 * for (;;) {
 *   canPoll();
 *   delayMicroseconds(100);
 * }
 * @endcode
 */
void canPoll();
/**
 * @brief FreeRTOS task body that owns the CAN bus (runs on core 0, never returns).
 *
 * Calls canSetup(), then loops calling canPoll(). If init failed it retries
 * every 2 s. With CAN_AUTOBAUD it re-runs canSetup() when no valid frame has
 * arrived for 10 s. Yields for one tick every 250 ms so the watchdog stays fed.
 *
 * @param unused  Task argument, ignored.
 * @return        Never returns.
 *
 * @code
 * xTaskCreatePinnedToCore(canTask, "can", 8192, NULL, 1, NULL, 0);
 * @endcode
 */
void canTask(void *);
/**
 * @brief Creates the CAN task on core 0 (priority 1, 8 KB stack).
 *
 * Compiles to nothing when SIMULATE is 1, since there is no bus to read.
 *
 * @return None.
 *
 * @code
 * void setup() {
 *   displayInit();
 *   sensorsInit();
 *   startCanTask();   // CAN starts after the dash is on screen
 * }
 * @endcode
 */
void startCanTask();