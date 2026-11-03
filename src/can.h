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
void canSetFrameHook(CanFrameHook cb);

bool decodeSignal(const CanSig &s, const byte *buf, float &out);
void applySignal(const CanSig &s, const byte *buf);
bool canTry(uint8_t rate, uint8_t clk, uint32_t ms);
void canSetup();
void canPoll();
void canTask(void *);
void startCanTask();