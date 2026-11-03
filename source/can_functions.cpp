#include <Preferences.h>
#include "can.h"
#include "debug.h"

// Receive mode. MCP_ANY is what the working .ino uses. MCP_STDEXT makes the chip reject
// frames with errors in hardware; try it once everything works.
#ifndef CAN_RX_MODE
#define CAN_RX_MODE MCP_ANY
#endif

// 1 = erase the saved bitrate/crystal at boot (flash once, then set back to 0)
#ifndef CAN_CLEAR_SAVED
#define CAN_CLEAR_SAVED 0
#endif

// The display driver uses HSPI (SPI3) on the ESP32-S3, so CAN must use FSPI (SPI2).
// Both on one peripheral = CAN traffic lands in the display (and the display breaks CAN).
SPIClass canSPI(FSPI);
MCP_CAN CAN0(&canSPI, CAN_CS);

bool     canOk = false;
uint32_t frameCount = 0;
static uint32_t canLastGood = 0;   // last time a frame with a known ID and legal length arrived
static uint32_t badFrames = 0;   // frames rejected as corrupt (bad length / remote request)

uint8_t canRate = CAN_500KBPS;   // used if autobaud is off or finds nothing
uint8_t canClk  = MCP_8MHZ;

static CanFrameHook frameHook = nullptr;

void canSetFrameHook(CanFrameHook cb) { frameHook = cb; }

// ----------------------------------------------------
// CAN signal map (see can.h for the notes)
//   Raw all-ones (0xFFFF / 0xFF) or signed-max (0x7FFF / 0x7F) is what the tune
//   sends for channels it has not configured, so those frames are ignored.
//   Flag / nibble fields (gear, launch switch) number bits from the LSB of each
//   byte. That is NOT verified yet: compare the [DASH] line with M1 Tune.
// ----------------------------------------------------
const CanSig SIGS[] = {
  // id     bit len signed scale                    offset target   sensor
  {0x640,    0, 16, false, 1.0f,                     0,   T_RPM,   0},         // Engine Speed (rpm)
  {0x640,   32, 16, true,  DEGC01_TO_F,             32,   T_SENS,  S_IAT},     // Inlet Manifold Temperature
  {0x641,   16, 16, false, 0.01f * AFR_PER_LAMBDA,   0,   T_SENS,  S_AFR},     // Fuel Mixture AIM (target, not measured)
  {0x641,   32, 16, false, KPA01_TO_PSI,             0,   T_SENS,  S_FUEL_P},  // Fuel Pressure Sensor
  {0x642,   32, 16, true,  -0.1f,                    0,   T_SENS,  S_ADV},     // Ignition Timing (-ve = BTDC, so flipped)
  {0x644,   48, 16, false, KPA01_TO_PSI,             0,   T_SENS,  S_OIL_P},   // Engine Oil Pressure
  {0x645,    0, 16, true,  KPA01_TO_PSI,             0,   T_SENS,  S_BOOST},   // Boost Pressure
  {0x648,    0, 16, false, KMH01_TO_MPH,             0,   T_SPEED, 0},         // Wheel Speed Front Left (bit 16/32/48 = FR/RL/RR)
  {0x649,    0,  8, false, 1.8f,                   -40,   T_SENS,  S_CLT},     // Coolant Temperature (raw - 40 degC)
  {0x649,    8,  8, false, 1.8f,                   -40,   T_SENS,  S_OIL_T},   // Engine Oil Temperature
  {0x649,   40,  8, false, 0.1f,                     0,   T_SENS,  S_BATT},    // ECU Battery Voltage
  {0x64A,    0, 16, false, DEGC01_TO_F,             32,   T_SENS,  S_EGT},     // Exhaust Temperature
  {0x64B,    0,  8, false, 0.1f,                     0,   T_SENS,  S_KNOCK},   // Cylinder 1 Trim Knock (retard, deg)
  {0x64D,   52,  4, true,  1.0f,                     0,   T_GEAR,  0},         // Gear (0 = neutral)
  {0x64E,   26,  1, false, 1.0f,                     0,   T_SENS,  S_LAUNCH},  // Launch Enable Switch -> OFF / ARM
};
const int NUM_SIGS = sizeof(SIGS) / sizeof(SIGS[0]);

// Decodes one signal from a frame. Returns false if the channel is "not configured".
bool decodeSignal(const CanSig &s, const byte *buf, float &out) {
  uint32_t raw;
  if (s.len < 8) {                                  // flags / nibbles: LSB-first inside the byte
    raw = (buf[s.bit / 8] >> (s.bit % 8)) & ((1u << s.len) - 1);
  } else {                                          // whole bytes, big-endian
    raw = 0;
    for (uint8_t i = 0; i < s.len / 8; i++) raw = (raw << 8) | buf[s.bit / 8 + i];
    uint32_t allOnes   = (1UL << s.len) - 1;
    uint32_t signedMax = allOnes >> 1;
    if (s.isSigned ? (raw == signedMax) : (raw == allOnes)) return false;
  }

  float v;
  if (s.isSigned && (raw & (1UL << (s.len - 1))))   // sign-extend
    v = (float)((int32_t)raw - (int32_t)(1UL << s.len));
  else
    v = (float)raw;

  out = v * s.scale + s.offset;
  return true;
}

void applySignal(const CanSig &s, const byte *buf) {
  float v;
  if (!decodeSignal(s, buf, v)) return;

  switch (s.target) {
    case T_RPM:   rpm = v; break;
    case T_SPEED: speedMph = v; break;
    case T_GEAR:  gear = constrain((int)(v + 0.5f), 0, 6); break;   // 0 = N, reverse clamps to N
    case T_SENS:  sens[s.sensor] = v; sensSeen[s.sensor] = true; break;
  }
}

// ----------------------------------------------------
// Autobaud
// ----------------------------------------------------
/**
 * @brief Converts a bitrate constant to a short label for log output.
 *
 * @param r  mcp_can bitrate constant (CAN_1000KBPS, CAN_500KBPS, ...).
 * @return   "1M", "500k", "250k", "125k", or "other".
 *
 * @code
 * debugMsg(rateName(CAN_500KBPS));   // "500k"
 * @endcode
 */
static const char *rateName(uint8_t r) {
  switch (r) {
    case CAN_1000KBPS: return "1M";
    case CAN_500KBPS:  return "500k";
    case CAN_250KBPS:  return "250k";
    case CAN_125KBPS:  return "125k";
    default:           return "other";
  }
}
/**
 * @brief Converts a crystal constant to a short label for log output.
 *
 * @param c  MCP_8MHZ or MCP_16MHZ.
 * @return   "8MHz", "16MHz", or "other".
 *
 * @code
 * debugMsg(clkName(MCP_8MHZ));   // "8MHz"
 * @endcode
 */
static const char *clkName(uint8_t c) {
  return c == MCP_16MHZ ? "16MHz" : (c == MCP_8MHZ ? "8MHz" : "other");
}

// Reads one frame straight out of the MCP2515 receive buffers over SPI.
// The library's readMsgBuf() trusts the chip's 4-bit length field (0-15) and copies that many
// bytes into its own 8-byte buffer. One corrupt frame (wrong bitrate, noise) overflows it and
// overwrites the library object's SPI pointer, and the next SPI access crashes with a
// LoadProhibited at a garbage address. Reading the 13 register bytes ourselves cannot overflow.
// Returns false if no frame is waiting. 'len' is the raw length field; the caller rejects > 8.
/**
 * @brief Reads one frame directly from the MCP2515 receive buffers over SPI.
 *
 * Replaces the library's readMsgBuf(), which trusts the chip's 4-bit length field
 * and can overflow its 8-byte buffer on a corrupt frame. Here exactly 13 register
 * bytes are read, so overflow is impossible. Chip-select going high clears the
 * receive flag.
 *
 * @param[out] id   Decoded 11- or 29-bit identifier.
 * @param[out] ext  true if the frame uses an extended ID.
 * @param[out] rtr  true if the frame is a remote request.
 * @param[out] len  Raw length field (0..15). The caller must reject values above 8.
 * @param[out] buf  Receives 8 data bytes (caller provides 8 bytes; only the first
 *                  @p len are meaningful).
 * @return          true if a frame was read, false if no frame is waiting.
 *
 * @code
 * uint32_t id; bool ext, rtr; uint8_t len, buf[8];
 * if (canReadFrame(id, ext, rtr, len, buf) && len <= 8 && !rtr) {
 *   // use id and buf[0..len-1]
 * }
 * @endcode
 */
static bool canReadFrame(uint32_t &id, bool &ext, bool &rtr, uint8_t &len, uint8_t *buf /* 8 bytes */) {
  const SPISettings cfg(10000000, MSBFIRST, SPI_MODE0);

  canSPI.beginTransaction(cfg);
  digitalWrite(CAN_CS, LOW);
  canSPI.transfer(0xA0);                       // READ STATUS
  uint8_t st = canSPI.transfer(0x00);
  digitalWrite(CAN_CS, HIGH);
  canSPI.endTransaction();

  uint8_t cmd;
  if (st & 0x01)      cmd = 0x90;              // READ RX BUFFER 0, starting at SIDH
  else if (st & 0x02) cmd = 0x94;              // READ RX BUFFER 1, starting at SIDH
  else return false;

  uint8_t r[13];                               // SIDH SIDL EID8 EID0 DLC D0..D7
  canSPI.beginTransaction(cfg);
  digitalWrite(CAN_CS, LOW);
  canSPI.transfer(cmd);
  for (int i = 0; i < 13; i++) r[i] = canSPI.transfer(0x00);
  digitalWrite(CAN_CS, HIGH);                  // CS going high clears the receive flag
  canSPI.endTransaction();

  ext = r[1] & 0x08;
  if (ext) {
    id = ((uint32_t)r[0] << 21) | (((uint32_t)r[1] & 0xE0) << 13) |
         (((uint32_t)r[1] & 0x03) << 16) | ((uint32_t)r[2] << 8) | r[3];
    rtr = r[4] & 0x40;
  } else {
    id = ((uint32_t)r[0] << 3) | (r[1] >> 5);
    rtr = r[1] & 0x10;
  }
  len = r[4] & 0x0F;
  memcpy(buf, &r[5], 8);
  return true;
}

// A frame only counts as real traffic if it looks like one: standard ID that is in our
// signal table, and a legal length. Noise at the wrong bitrate produces junk frames
// (with MCP_ANY the chip passes them on); those must not be mistaken for the bus.
/**
 * @brief Checks whether a frame looks like real traffic from this ECU.
 *
 * True only for a standard-ID frame with a legal, non-zero length whose ID is in
 * ::SIGS. Used to tell a working bus from noise at the wrong bitrate.
 *
 * @param id   Frame identifier.
 * @param ext  true if the ID is extended.
 * @param len  Data length.
 * @return     true if the frame is plausible, false otherwise.
 *
 * @code
 * if (plausibleFrame(0x640, false, 8)) canLastGood = millis();
 * @endcode
 */
static bool plausibleFrame(uint32_t id, bool ext, byte len) {
  if (ext || len == 0 || len > 8) return false;
  for (int k = 0; k < NUM_SIGS; k++)
    if (SIGS[k].id == id) return true;
  return false;
}

static uint8_t canReadReg(uint8_t reg);

// Listen-only attempt: true if 3 plausible frames show up within ms.
// Prints what it saw, so a silent bus can be told apart from a wrong bitrate:
//   0 frames, CANINTF/EFLG 0x00  -> nothing on the bus at all (ECU off, wires, termination)
//   0 frames, CANINTF/EFLG bits set (0x80/0x20) -> activity, but at a different bitrate
//   frames seen, 0 valid         -> traffic with other IDs / junk (wrong bitrate or crystal)
bool canTry(uint8_t rate, uint8_t clk, uint32_t ms) {
  if (CAN0.begin(CAN_RX_MODE, rate, clk) != CAN_OK) { debugMsg("  begin() failed"); return false; }
  CAN0.setMode(MCP_LISTENONLY);
  uint8_t good = 0;
  uint16_t raw = 0;
  bool ok = false;
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    uint32_t id; bool ext, rtr; uint8_t len; uint8_t buf[8];
    if (canReadFrame(id, ext, rtr, len, buf)) {
      raw++;
      if (!rtr && plausibleFrame(id, ext, len) && ++good >= 3) { ok = true; break; }
    } else {
      delay(2);
    }
  }
  char m[80];
  snprintf(m, sizeof(m), "  -> %u frames seen, %u valid, CANINTF 0x%02X EFLG 0x%02X",
           (unsigned)raw, (unsigned)good, canReadReg(0x2C), canReadReg(0x2D));
  debugMsg(m);
  return ok;
}

// Reads one MCP2515 register (slow clock, used for recovery diagnostics)
/**
 * @brief Reads one MCP2515 register over SPI at a slow 1 MHz clock.
 *
 * Used for diagnostics such as CANSTAT, CANCTRL, CANINTF and EFLG.
 *
 * @param reg  Register address (e.g. 0x0E = CANSTAT, 0x2D = EFLG).
 * @return     Register value. 0x00 or 0xFF usually means no SPI response.
 *
 * @code
 * uint8_t stat = canReadReg(0x0E);   // 0x80 = configuration mode
 * @endcode
 */
static uint8_t canReadReg(uint8_t reg) {
  canSPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(CAN_CS, LOW);
  canSPI.transfer(0x03);                       // READ
  canSPI.transfer(reg);
  uint8_t v = canSPI.transfer(0x00);
  digitalWrite(CAN_CS, HIGH);
  canSPI.endTransaction();
  return v;
}

// The MCP2515 keeps running when only the ESP32 resets, and the SPI lines float while the
// ESP32 boots, so the chip can be left mid-command or in a odd mode. Send RESET (0xC0)
// a few times at a slow clock, then check CANSTAT: a freshly reset chip reads 0x80.
//   0x80      -> chip answered and is in configuration mode (good)
//   0x00/0xFF -> no SPI response: check power, CS/MISO wiring, 3.3 V level on MISO
//   other     -> chip answers but is not in config mode, or the clock/level is marginal
/**
 * @brief Resets the MCP2515 and prints CANSTAT/CANCTRL to confirm it answers.
 *
 * Sends the RESET command three times at a slow clock, because the chip keeps
 * running when only the ESP32 resets and the SPI lines float during boot.
 * Expected readback: CANSTAT 0x80 and CANCTRL 0x87.
 *
 * @return None. Result is printed with debugMsg().
 *
 * @code
 * canSPI.begin(CAN_SCK, CAN_MISO, CAN_MOSI, CAN_CS);
 * canChipReset();
 * @endcode
 */
static void canChipReset() {
  canSPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  for (int i = 0; i < 3; i++) {
    digitalWrite(CAN_CS, LOW);
    canSPI.transfer(0xC0);                     // RESET
    digitalWrite(CAN_CS, HIGH);
    delay(10);
  }
  canSPI.endTransaction();

  uint8_t stat = canReadReg(0x0E);             // CANSTAT
  uint8_t ctrl = canReadReg(0x0F);             // CANCTRL (reset value 0x87)
  char m[72];
  snprintf(m, sizeof(m), "MCP2515 after reset: CANSTAT 0x%02X (want 0x80), CANCTRL 0x%02X (want 0x87)", stat, ctrl);
  debugMsg(m);
}

void canSetup() {
  canOk = false;
  pinMode(CAN_CS, OUTPUT);
  digitalWrite(CAN_CS, HIGH);                  // keep the chip deselected
  pinMode(CAN_INT, INPUT_PULLUP);
  canSPI.begin(CAN_SCK, CAN_MISO, CAN_MOSI, CAN_CS);
  canChipReset();

  // The bitrate / crystal that worked last time is remembered across power cycles,
  // but only settings that were verified with real frames are ever saved.
  Preferences cp;
  cp.begin("kartcan", false);
#if CAN_CLEAR_SAVED
  static bool cleared = false;                 // once per boot, not on every rescan
  if (!cleared) {
    cleared = true;
    if (cp.isKey("rate")) cp.remove("rate");
    if (cp.isKey("clk"))  cp.remove("clk");
    debugMsg("CAN: saved bitrate/crystal erased");
  }
#endif
  canRate = cp.getUChar("rate", canRate);
  canClk  = cp.getUChar("clk",  canClk);
  const char *src = "saved/default";

#if CAN_AUTOBAUD
  if (canTry(canRate, canClk, 1500)) {
    src = "saved, verified";
  } else {
    static const uint8_t rates[4] = {CAN_1000KBPS, CAN_500KBPS, CAN_250KBPS, CAN_125KBPS};
    static const uint8_t clks[2]  = {MCP_8MHZ, MCP_16MHZ};
    bool found = false;
    for (int c = 0; c < 2 && !found; c++) {
      for (int r = 0; r < 4 && !found; r++) {
        debugAutobaudTry(rateName(rates[r]), clkName(clks[c]));
        if (canTry(rates[r], clks[c], 1500)) {
          canRate = rates[r]; canClk = clks[c]; found = true;
          cp.putUChar("rate", canRate);
          cp.putUChar("clk", canClk);
          debugAutobaudFound(rateName(canRate), clkName(canClk));
          src = "autobaud";
        }
      }
    }
    if (!found) {
      debugMsg("Autobaud: no valid traffic on any setting (is the ECU on and the bus terminated?)");
      src = "saved/default, UNVERIFIED";
    }
  }
#endif
  cp.end();

  char msg[96];
  snprintf(msg, sizeof(msg), "CAN: using %s / %s crystal (%s)", rateName(canRate), clkName(canClk), src);
  debugMsg(msg);

  if (CAN0.begin(CAN_RX_MODE, canRate, canClk) == CAN_OK) {
    CAN0.setMode(CAN_LISTEN_ONLY ? MCP_LISTENONLY : MCP_NORMAL);
    canOk = true;
    debugMsg("MCP2515 init OK (SPI link good)");
  } else {
    debugMsg("MCP2515 init FAILED - check wiring/power/crystal setting");
  }
  canLastGood = millis();            // grace period before the "no data, rescan" check
}

void canPoll() {
  if (!canOk) return;
  uint32_t now = millis();
  (void)now;                         // only used by the debug output

  // poll the chip (doesn't depend on the INT wire); capped so one pass always returns
  for (uint8_t n = 0; n < 32; n++) {
    uint32_t id; bool ext, rtr; uint8_t len; uint8_t buf[8];
    if (!canReadFrame(id, ext, rtr, len, buf)) break;
    frameCount++;

    if (len > 8 || rtr) { badFrames++; continue; }   // corrupt / remote request

    if (plausibleFrame(id, ext, len)) canLastGood = now;

    debugCanFrame(id, ext, len, buf, now);
    if (frameHook) frameHook(id, len, buf);
    if (ext) continue;               // all signals use standard IDs

    for (int k = 0; k < NUM_SIGS; k++)
      if (SIGS[k].id == id && (SIGS[k].bit + SIGS[k].len + 7) / 8 <= len)
        applySignal(SIGS[k], buf);
  }

#if CAN_STATUS
  if (debugStatusDue(now)) {
    debugCanStatus(frameCount, CAN0.getError(), CAN0.errorCountRX(),
                   CAN0.errorCountTX(), digitalRead(CAN_INT));
    char m[48];
    snprintf(m, sizeof(m), "[CAN] rejected corrupt frames: %u", (unsigned)badFrames);
    debugMsg(m);
  }
#endif
}

// ----------------------------------------------------
// CAN runs in its own task on core 0; the display loop stays on core 1.
// Starting CAN (autobaud can take several seconds) never delays the dash,
// and drawing never makes the MCP2515's two receive buffers overflow.
// ----------------------------------------------------
void canTask(void *) {
  canSetup();
  uint32_t lastYield = millis();
  for (;;) {
    if (!canOk) {                    // init failed (wiring / power): keep retrying
      vTaskDelay(pdMS_TO_TICKS(2000));
      canSetup();
      continue;
    }
    canPoll();                       // drains everything the MCP2515 is holding
    delayMicroseconds(100);
#if CAN_AUTOBAUD
    // The ECU may be switched on after the dash, or the saved settings may be wrong:
    // if nothing valid arrives for 10 s, scan again instead of listening forever.
    if (millis() - canLastGood > 10000UL) {
      debugMsg("CAN: no valid frames for 10 s, rescanning");
      canSetup();
    }
#endif
    if (millis() - lastYield >= 250) {   // let the idle task run so the watchdog stays fed
      lastYield = millis();
      vTaskDelay(1);
    }
  }
}

void startCanTask() {
#if !SIMULATE
  xTaskCreatePinnedToCore(canTask, "can", 8192, NULL, 1, NULL, 0);
#endif
}