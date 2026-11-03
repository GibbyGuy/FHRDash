#include "debug.h"
#include "sensors.h"

#if CAN_DEBUG
struct DbgId { uint32_t id; uint32_t t; };
static DbgId   dbgIds[48];
static uint8_t dbgN = 0;

// Print each distinct ID at most every 250 ms so serial can't flood
/**
 * @brief Rate limiter for CAN frame printing: allows one print per ID every 250 ms.
 *
 * Tracks up to 48 distinct IDs; a new ID always prints. Only built when CAN_DEBUG is 1.
 *
 * @param id   CAN identifier being considered.
 * @param now  Current time in ms.
 * @return     true if this frame should be printed now, false if it was printed recently.
 *
 * @code
 * if (dbgDue(id, millis())) Serial.printf("0x%X\n", (unsigned)id);
 * @endcode
 */
static bool dbgDue(uint32_t id, uint32_t now) {
  for (uint8_t i = 0; i < dbgN; i++) {
    if (dbgIds[i].id == id) {
      if (now - dbgIds[i].t < 250) return false;
      dbgIds[i].t = now;
      return true;
    }
  }
  if (dbgN < 48) { dbgIds[dbgN].id = id; dbgIds[dbgN].t = now; dbgN++; }
  return true;
}
#endif

// Why did we (re)boot? Brownout / watchdog / crash are shown on the display for a moment.
const char *resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "POWER-ON";
    case ESP_RST_EXT:       return "RESET PIN";
    case ESP_RST_SW:        return "SOFTWARE";
    case ESP_RST_PANIC:     return "CRASH (PANIC)";
    case ESP_RST_INT_WDT:   return "INTERRUPT WDT";
    case ESP_RST_TASK_WDT:  return "TASK WDT";
    case ESP_RST_WDT:       return "OTHER WDT";
    case ESP_RST_DEEPSLEEP: return "DEEP SLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT (POWER)";
    default:                return "UNKNOWN";
  }
}

void debugInit() {
  Serial.end();                    // the core already started Serial with a tiny buffer
  Serial.setTxBufferSize(4096);    // now it takes effect
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);        // never block the dash if no PC is attached
#endif
  Serial.printf("Reset reason: %s\n", resetReasonName(esp_reset_reason()));
}

void debugMsg(const char *msg) {
  Serial.println(msg);
}

void debugAutobaudTry(const char *rateName, const char *clkName) {
  Serial.printf("Autobaud: trying %s with %s crystal...\n", rateName, clkName);
}

void debugAutobaudFound(const char *rateName, const char *clkName) {
  Serial.printf("Autobaud: TRAFFIC FOUND at %s / %s crystal (saved)\n", rateName, clkName);
}

void debugCanFrame(uint32_t id, bool ext, uint8_t len, const uint8_t *buf, uint32_t now) {
#if CAN_DEBUG
  if (!dbgDue(id, now)) return;
  Serial.printf("%s 0x%X [%d]:", ext ? "EXT" : "STD", (unsigned)id, len);
  for (uint8_t i = 0; i < len; i++) Serial.printf(" %02X", buf[i]);
  Serial.println();
#else
  (void)id; (void)ext; (void)len; (void)buf; (void)now;
#endif
}

bool debugStatusDue(uint32_t now) {
#if CAN_STATUS
  static uint32_t lastReport = 0;
  if (now - lastReport > 1000) {
    lastReport = now;
    return true;
  }
#else
  (void)now;
#endif
  return false;
}

void debugCanStatus(uint32_t frames, uint8_t eflg, uint8_t rxErr, uint8_t txErr, int intPin) {
#if CAN_STATUS
  Serial.printf("[CAN] frames: %u  EFLG 0x%02X  RXerr %d  TXerr %d  INT pin %d%s\n",
                (unsigned)frames, eflg, rxErr, txErr, intPin,
                frames ? "" : "  (no traffic yet)");
  debugDash();
#else
  (void)frames; (void)eflg; (void)rxErr; (void)txErr; (void)intPin;
#endif
}

// Once-a-second decoded view, to check the numbers against M1 Tune
void debugDash() {
#if CAN_STATUS
  static const char *SN[S_COUNT] = {"OILT", "CLT", "ADV", "KNK", "LCH", "OILP",
                                    "BATT", "AFR", "BST", "EGT", "IAT", "FP"};
  Serial.printf("[DASH] rpm %.0f  gear %d  mph %.1f |", rpm, gear, speedMph);
  for (int i = 0; i < S_COUNT; i++) {
    if (sensSeen[i]) Serial.printf(" %s %.1f", SN[i], sens[i]);
    else             Serial.printf(" %s --", SN[i]);
  }
  Serial.println();
#endif
}

void debugTouch(int16_t rx, int16_t ry, int16_t sx, int16_t sy) {
#if TOUCH_DEBUG
  Serial.printf("touch raw(%d,%d) -> (%d,%d)\n", rx, ry, sx, sy);
#else
  (void)rx; (void)ry; (void)sx; (void)sy;
#endif
}