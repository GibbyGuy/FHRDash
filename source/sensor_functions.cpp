#include <Preferences.h>
#include "sensors.h"
#include "display.h"

// ----------------------------------------------------
// Sensor definitions
// ----------------------------------------------------
const SensorDef DEFS[S_COUNT] = {
  {"OIL TEMP",   "\xF7" "F", 100, 300, 160, 260, 0, false},
  {"COOLANT",    "\xF7" "F", 100, 260, 150, 220, 0, false},
  {"ADVANCE",    "deg",       -5,  45,   5,  38, 1, false},
  {"KNOCK",      "deg RET",    0,  10,   0, 1.5f, 1, false},
  {"LAUNCH",     "",           0,   2,   0,   2, 0, true },
  {"OIL PRESS",  "psi",        0, 100,  25,  80, 0, false},
  {"BATTERY",    "volts",      8,  16,  12, 14.8f, 1, false},
  {"AFR",        "a/f",       10,  18, 11.5f, 15.2f, 1, false},
  {"BOOST",      "psi",      -10,  20, -10,  15, 1, false},
  {"EGT",        "\xF7" "F", 800, 1800, 900, 1500, 0, false},
  {"INTAKE T",   "\xF7" "F",  30, 200,  30, 140, 0, false},
  {"FUEL PRESS", "psi",        0, 80,  35,  60, 0, false}
};

const char *LAUNCH_STR[3] = {"OFF", "ARM", "GO!"};

// Labels for text-style notification icons (sensors with a drawn symbol ignore these)
const char *ALERT_L1[S_COUNT] = {"OIL", "",  "ADV", "", "LC",     "",      "", "AFR", "BST", "EGT", "IAT", "FP"};
const char *ALERT_L2[S_COUNT] = {"TEMP", "", "",    "", "LAUNCH", "",      "", "",    "",    "",    "",    "PRESS"};

// Live data
volatile float   rpm = 0;
volatile uint8_t gear = 0;
volatile float   speedMph = 0;
volatile float   sens[S_COUNT];
volatile bool    sensSeen[S_COUNT];

// ----------------------------------------------------
// Private types and state
// ----------------------------------------------------
struct Slot {
  int16_t   x, y;
  uint8_t   type;
  bool      full;
  bool      bad;
  int16_t   markX;
  TextField val;
};

enum { MODE_STEADY = 0, MODE_WARN = 1, MODE_CRIT = 2 };

struct AlertIcon {
  uint8_t  sensor;
  uint16_t col;
  uint8_t  mode;
  bool     vis;
};

static Preferences prefs;
static Slot slots[NUM_SLOTS];

// ----------------------------------------------------
// Side widgets
// ----------------------------------------------------
/**
 * @brief Converts a sensor value to the X position of the marker on a widget's bar.
 *
 * The value is clamped to the sensor's min..max range first.
 *
 * @param s  Widget the bar belongs to.
 * @param v  Sensor value in display units.
 * @return   Marker left edge in screen pixels.
 *
 * @code
 * int16_t mx = barXFor(slots[0], sens[slots[0].type]);
 * @endcode
 */
static int16_t barXFor(const Slot &s, float v) {
  const SensorDef &d = DEFS[s.type];
  float f = (v - d.minV) / (d.maxV - d.minV);
  if (f < 0) f = 0;
  if (f > 1) f = 1;
  return s.x + BAR_X_OFF + (int16_t)(f * (BAR_W - MARK_W));
}

/**
 * @brief Fills the overlap of two horizontal ranges with a colour.
 *
 * Draws the part of [a, b) that lies inside [x0, x1); nothing if they do not overlap.
 *
 * @param a    Start of the range to repaint (px).
 * @param b    End of the range to repaint (px).
 * @param x0   Start of the zone (px).
 * @param x1   End of the zone (px).
 * @param y    Top of the strip (px).
 * @param h    Height of the strip (px).
 * @param col  RGB565 fill colour.
 * @return     None.
 *
 * @code
 * fillSpan(100, 140, 120, 200, by, BAR_H, C_ZONE_GOOD);   // fills 120..140
 * @endcode
 */
static void fillSpan(int16_t a, int16_t b, int16_t x0, int16_t x1, int16_t y, int16_t h, uint16_t col) {
  int16_t lo = max(a, x0), hi = min(b, x1);
  if (hi > lo) gfx->fillRect(lo, y, hi - lo, h, col);
}

/**
 * @brief Paints the bad/good/bad colour zones of a widget bar, limited to an X range.
 *
 * Used for the whole bar and to repair the area under a moving marker.
 *
 * @param s  Widget whose sensor defines the zones (goodLo..goodHi).
 * @param a  Left X of the region to repaint (px).
 * @param b  Right X of the region to repaint (px).
 * @return   None.
 *
 * @code
 * fillZones(s, s.x + BAR_X_OFF, s.x + BAR_X_OFF + BAR_W);   // whole bar
 * @endcode
 */
static void fillZones(const Slot &s, int16_t a, int16_t b) {
  const SensorDef &d = DEFS[s.type];
  int16_t bx = s.x + BAR_X_OFF, by = s.y + BAR_Y_OFF;
  float span = d.maxV - d.minV;
  float fl = (d.goodLo - d.minV) / span, fh = (d.goodHi - d.minV) / span;
  if (fl < 0) fl = 0;
  if (fh > 1) fh = 1;
  int16_t gl = bx + (int16_t)(fl * BAR_W);
  int16_t gh = bx + (int16_t)(fh * BAR_W);
  fillSpan(a, b, bx, gl, by, BAR_H, C_ZONE_BAD);
  fillSpan(a, b, gl, gh, by, BAR_H, C_ZONE_GOOD);
  fillSpan(a, b, gh, bx + BAR_W, by, BAR_H, C_ZONE_BAD);
}

/**
 * @brief Clears a widget and draws its fixed parts: name, unit and zoned bar.
 *
 * Also resets the value text field and marker so the next updateSlot() redraws them.
 *
 * @param[in,out] s  Widget to draw.
 * @return           None.
 *
 * @code
 * drawSlotStatic(slots[2]);
 * @endcode
 */
static void drawSlotStatic(Slot &s) {
  const SensorDef &d = DEFS[s.type];
  gfx->fillRect(s.x, s.y, SLOT_W, SLOT_H, C_BG);
  drawCentered(d.name, s.x, SLOT_W, s.y + 5, 2, C_LABEL);
  if (d.unit[0]) drawCentered(d.unit, s.x, SLOT_W, s.y + 62, 2, C_LABEL);
  fillZones(s, s.x + BAR_X_OFF, s.x + BAR_X_OFF + BAR_W);
  resetField(s.val);
  s.markX = -1;
}

/**
 * @brief Redraws one side widget: border, value text and bar marker, only where something changed.
 *
 * A widget flagged "full" (after load or a tap) is redrawn from scratch first.
 * Out-of-range values show in red with a red border. Launch control shows
 * OFF / ARM / GO! instead of a number.
 *
 * @param[in,out] s  Widget to update.
 * @return           None.
 *
 * @code
 * for (int i = 0; i < NUM_SLOTS; i++) updateSlot(slots[i]);
 * @endcode
 */
static void updateSlot(Slot &s) {
  const SensorDef &d = DEFS[s.type];
  float v = sens[s.type];
  bool bad = !d.isState && (v < d.goodLo || v > d.goodHi);
  uint16_t col = bad ? RED : WHITE;
  bool wasFull = s.full;

  if (s.full) {
    drawSlotStatic(s);
    s.full = false;
  }

  if (wasFull || bad != s.bad) {
    gfx->drawRoundRect(s.x, s.y, SLOT_W, SLOT_H, 6, bad ? RED : C_BORDER);
  }

  char buf[12];
  if (d.isState) {
    int idx = constrain((int)(v + 0.5f), 0, 2);
    snprintf(buf, sizeof(buf), "%s", LAUNCH_STR[idx]);
  } else {
    snprintf(buf, sizeof(buf), "%.*f", d.dec, v);
  }
  updateText(s.val, s.x, SLOT_W, s.y + 24, 4, buf, col);

  int16_t mx = barXFor(s, v);
  if (mx != s.markX || bad != s.bad) {
    int16_t by = s.y + BAR_Y_OFF;
    if (s.markX >= 0) {
      gfx->fillRect(s.markX, by - 3, MARK_W, 3, C_BG);
      gfx->fillRect(s.markX, by + BAR_H, MARK_W, 3, C_BG);
      fillZones(s, s.markX, s.markX + MARK_W);
    }
    gfx->fillRect(mx, by - 3, MARK_W, BAR_H + 6, col);
    s.markX = mx;
  }
  s.bad = bad;
}

/**
 * @brief Checks whether any widget already shows a given sensor.
 *
 * @param t  Sensor index (S_OIL_T ... S_FUEL_P).
 * @return   true if one of the NUM_SLOTS widgets uses it.
 *
 * @code
 * if (!typeInUse(S_BOOST)) slots[0].type = S_BOOST;
 * @endcode
 */
static bool typeInUse(uint8_t t) {
  for (int i = 0; i < NUM_SLOTS; i++) if (slots[i].type == t) return true;
  return false;
}

/**
 * @brief Saves the sensor shown in each widget to flash ("slot0".."slot5").
 *
 * @return None.
 *
 * @code
 * slots[1].type = S_AFR;
 * saveSlots();
 * @endcode
 */
static void saveSlots() {
  char key[8];
  for (int i = 0; i < NUM_SLOTS; i++) {
    snprintf(key, sizeof(key), "slot%d", i);
    prefs.putUChar(key, slots[i].type);
  }
}

/**
 * @brief Switches a widget to the next sensor not shown in any other widget, and saves the layout.
 *
 * Marks the widget for a full redraw. Does nothing if no free sensor exists.
 *
 * @param i  Widget index (0..NUM_SLOTS-1).
 * @return   None.
 *
 * @code
 * cycleSlot(0);   // top-left widget moves to the next free sensor
 * @endcode
 */
static void cycleSlot(int i) {
  uint8_t t = slots[i].type;
  for (int k = 1; k < S_COUNT; k++) {
    uint8_t cand = (t + k) % S_COUNT;
    if (!typeInUse(cand)) {
      slots[i].type = cand;
      slots[i].full = true;
      slots[i].bad = false;
      saveSlots();
      return;
    }
  }
}

/**
 * @brief Loads the saved widget assignments and sets up each widget's position.
 *
 * Saved values that are out of range or duplicated are replaced by the defaults
 * (oil temp, coolant, oil pressure, advance, knock, launch). Slots 0-2 go on the
 * left, 3-5 on the right.
 *
 * @return None.
 *
 * @code
 * prefs.begin("kartdash", false);
 * loadSlots();
 * @endcode
 */
static void loadSlots() {
  const uint8_t defaults[NUM_SLOTS] = {S_OIL_T, S_CLT, S_OIL_P, S_ADV, S_KNOCK, S_LAUNCH};
  bool ok = true;
  uint8_t t[NUM_SLOTS];
  char key[8];
  for (int i = 0; i < NUM_SLOTS; i++) {
    snprintf(key, sizeof(key), "slot%d", i);
    t[i] = prefs.getUChar(key, defaults[i]);
    if (t[i] >= S_COUNT) ok = false;
    for (int j = 0; j < i; j++) if (t[j] == t[i]) ok = false;
  }
  for (int i = 0; i < NUM_SLOTS; i++) {
    slots[i].type = ok ? t[i] : defaults[i];
    slots[i].x = (i < 3) ? 3 : 351;
    slots[i].y = 4 + (i % 3) * 105;
    slots[i].full = true;
    slots[i].bad = false;
    slots[i].markX = -1;
    resetField(slots[i].val);
  }
}

// Called from handleTouch(): cycles the tapped widget to the next unused sensor
bool slotHandleTap(int16_t x, int16_t y) {
  for (int i = 0; i < NUM_SLOTS; i++) {
    Slot &s = slots[i];
    if (x >= s.x && x < s.x + SLOT_W && y >= s.y && y < s.y + SLOT_H) {
      cycleSlot(i);
      return true;
    }
  }
  return false;
}

// ----------------------------------------------------
// Tachometer
// ----------------------------------------------------
static int16_t segPts[TACH_N][8];
static int tachPrevLit = 0;
static TextField gearF, rpmF;

/**
 * @brief Pre-computes the corner points of every tach arc segment.
 *
 * Fills ::segPts with four (x, y) corners per segment so drawing needs no trigonometry.
 *
 * @return None.
 *
 * @code
 * buildSegments();   // once, in sensorsInit()
 * @endcode
 */
static void buildSegments() {
  float step = TACH_SWEEP / TACH_N;
  for (int i = 0; i < TACH_N; i++) {
    float a0 = (TACH_START - i * step) * DEG_TO_RAD;
    float a1 = (TACH_START - (i + 1) * step + 0.8f) * DEG_TO_RAD;
    segPts[i][0] = TACH_CX + TACH_RO * cosf(a0);  segPts[i][1] = TACH_CY - TACH_RO * sinf(a0);
    segPts[i][2] = TACH_CX + TACH_RO * cosf(a1);  segPts[i][3] = TACH_CY - TACH_RO * sinf(a1);
    segPts[i][4] = TACH_CX + TACH_RI * cosf(a1);  segPts[i][5] = TACH_CY - TACH_RI * sinf(a1);
    segPts[i][6] = TACH_CX + TACH_RI * cosf(a0);  segPts[i][7] = TACH_CY - TACH_RI * sinf(a0);
  }
}

/**
 * @brief Draws one tach arc segment as two filled triangles.
 *
 * @param i    Segment index (0..TACH_N-1).
 * @param col  RGB565 fill colour.
 * @return     None.
 *
 * @code
 * drawSeg(10, C_ARC_OFF);   // turn segment 10 off
 * @endcode
 */
static void drawSeg(int i, uint16_t col) {
  int16_t *p = segPts[i];
  gfx->fillTriangle(p[0], p[1], p[2], p[3], p[4], p[5], col);
  gfx->fillTriangle(p[0], p[1], p[4], p[5], p[6], p[7], col);
}

/**
 * @brief Returns the lit colour of a tach segment: white, yellow from SHIFT_RPM, red from REDLINE_RPM.
 *
 * @param i  Segment index (0..TACH_N-1).
 * @return   RGB565 colour.
 *
 * @code
 * drawSeg(i, segColor(i));
 * @endcode
 */
static uint16_t segColor(int i) {
  float segRpm = (float)(i + 1) / TACH_N * MAX_RPM;
  if (segRpm >= REDLINE_RPM) return RED;
  if (segRpm >= SHIFT_RPM)   return C_YELLOW;
  return WHITE;
}

/**
 * @brief Draws the tach in its idle state: all segments off, "RPM" label, text fields reset.
 *
 * @return None.
 *
 * @code
 * drawTachStatic();   // once, in sensorsInit()
 * @endcode
 */
static void drawTachStatic() {
  for (int i = 0; i < TACH_N; i++) drawSeg(i, C_ARC_OFF);
  drawCentered("RPM", TACH_CX - 30, 60, TACH_CY + 68, 2, C_LABEL);
  tachPrevLit = 0;
  resetField(gearF);
  resetField(rpmF);
}

/**
 * @brief Updates the tachometer from the live rpm and gear values.
 *
 * Lights or clears only the segments between the previous and new level, and
 * redraws the gear letter/number and RPM text (rounded to 10) if they changed.
 * Text turns red at or above SHIFT_RPM.
 *
 * @return None.
 *
 * @code
 * rpm = 9000;
 * updateTach();
 * @endcode
 */
static void updateTach() {
  int lit = (int)(rpm / MAX_RPM * TACH_N + 0.5f);
  lit = constrain(lit, 0, TACH_N);
  if (lit > tachPrevLit) {
    for (int i = tachPrevLit; i < lit; i++) drawSeg(i, segColor(i));
  } else if (lit < tachPrevLit) {
    for (int i = lit; i < tachPrevLit; i++) drawSeg(i, C_ARC_OFF);
  }
  tachPrevLit = lit;

  bool shift = rpm >= SHIFT_RPM;
  char g[2] = { gear == 0 ? 'N' : (char)('0' + gear), 0 };
  updateText(gearF, TACH_CX - 30, 60, TACH_CY - 48, 8, g, shift ? RED : WHITE);

  char r[8];
  snprintf(r, sizeof(r), "%d", ((int)rpm / 10) * 10);
  updateText(rpmF, TACH_CX - 60, 120, TACH_CY + 38, 3, r, shift ? RED : WHITE);
}

// ----------------------------------------------------
// Speedometer
//   - displayed speed eases toward the real speed (smooth number + bar)
//   - icy blue -> white gradient across the bar
//   - leading segment shimmers
//   - yellow peak-hold marker that lingers, then slides back down
//   All drawn by per-segment state, so only changed segments are repainted.
// ----------------------------------------------------
enum { SS_UNKNOWN = 255, SS_OFF = 0, SS_LIT = 1, SS_BRIGHT = 2, SS_PEAK = 3 };

static uint8_t  spdState[SPD_N];
static uint16_t spdCol[SPD_N], spdColBright[SPD_N];
static float    speedDisp = 0;
static int      spdPeak = -1;
static uint32_t spdPeakHold = 0, spdPeakStep = 0, spdShimmerT = 0;
static bool     spdShimmer = false;
static TextField spdF;

/**
 * @brief Pre-computes the icy-blue to white gradient for the speed bar.
 *
 * Fills ::spdCol (normal) and ::spdColBright (shimmer) for each segment.
 *
 * @return None.
 *
 * @code
 * buildSpeedColors();   // once, in sensorsInit()
 * @endcode
 */
static void buildSpeedColors() {
  for (int i = 0; i < SPD_N; i++) {
    float t = (float)i / (SPD_N - 1);
    float r = 255.0f * t;
    float g = 110.0f + 145.0f * t;
    float b = 255.0f;
    spdCol[i] = gfx->color565((uint8_t)r, (uint8_t)g, (uint8_t)b);
    spdColBright[i] = gfx->color565((uint8_t)(r + (255 - r) * 0.6f),
                                    (uint8_t)(g + (255 - g) * 0.6f), 255);
  }
}

/**
 * @brief Sets one speed-bar segment to a state and repaints it only if the state changed.
 *
 * @param i   Segment index (0..SPD_N-1).
 * @param st  SS_OFF, SS_LIT, SS_BRIGHT or SS_PEAK.
 * @return    None.
 *
 * @code
 * setSpdSeg(12, SS_PEAK);   // yellow peak marker
 * @endcode
 */
static void setSpdSeg(int i, uint8_t st) {
  if (spdState[i] == st) return;
  spdState[i] = st;
  uint16_t col = C_ARC_OFF;
  if (st == SS_LIT) col = spdCol[i];
  else if (st == SS_BRIGHT) col = spdColBright[i];
  else if (st == SS_PEAK) col = C_YELLOW;
  gfx->fillRect(SPD_BAR_X + i * SPD_PITCH, SPD_BAR_Y, SPD_SEG_W, SPD_SEG_H, col);
}

/**
 * @brief Draws the speedometer in its idle state: all segments off, "MPH" label, peak cleared.
 *
 * @return None.
 *
 * @code
 * drawSpeedStatic();   // once, in sensorsInit()
 * @endcode
 */
static void drawSpeedStatic() {
  for (int i = 0; i < SPD_N; i++) { spdState[i] = SS_UNKNOWN; setSpdSeg(i, SS_OFF); }
  drawCentered("MPH", SPD_NUM_AREA_X + SPD_NUM_AREA_W + 6, 36, SPD_NUM_Y + 16, 2, C_LABEL);
  resetField(spdF);
  spdPeak = -1;
}

/**
 * @brief Animates the speedometer toward the live speed.
 *
 * The shown speed eases toward ::speedMph, the lead segment shimmers every 90 ms,
 * and a yellow peak marker holds for 1.2 s before sliding down. Only changed
 * segments and text are repainted.
 *
 * @param now  Current time in ms (millis()).
 * @return     None.
 *
 * @code
 * updateSpeed(millis());
 * @endcode
 */
static void updateSpeed(uint32_t now) {
  speedDisp += (speedMph - speedDisp) * 0.3f;
  if (fabsf(speedMph - speedDisp) < 0.05f) speedDisp = speedMph;

  int lit = constrain((int)(speedDisp / SPEED_MAX * SPD_N + 0.5f), 0, SPD_N);

  if (now - spdShimmerT > 90) { spdShimmerT = now; spdShimmer = !spdShimmer; }

  if (lit - 1 > spdPeak) {
    spdPeak = lit - 1;
    spdPeakHold = now + 1200;
  } else if (spdPeak > lit - 1 && now > spdPeakHold && now - spdPeakStep > 45) {
    spdPeak--;
    spdPeakStep = now;
  }

  for (int i = 0; i < SPD_N; i++) {
    uint8_t st = SS_OFF;
    if (i < lit - 1) st = SS_LIT;
    else if (i == lit - 1) st = spdShimmer ? SS_BRIGHT : SS_LIT;
    else if (i == spdPeak) st = SS_PEAK;
    setSpdSeg(i, st);
  }

  char buf[8];
  snprintf(buf, sizeof(buf), "%d", (int)(speedDisp + 0.5f));
  updateText(spdF, SPD_NUM_AREA_X, SPD_NUM_AREA_W, SPD_NUM_Y, 4, buf, WHITE);
}

// Startup gauge sweep: tach + speed bar sweep up and back down
void bootSweep() {
  gear = 0;
  for (int pass = 0; pass < 2; pass++) {
    for (int k = 0; k <= 30; k++) {
      float f = (pass == 0) ? k / 30.0f : 1.0f - k / 30.0f;
      float e = f * f * (3.0f - 2.0f * f);          // smoothstep
      rpm = e * MAX_RPM;
      speedMph = speedDisp = e * SPEED_MAX;
      updateTach();
      updateSpeed(millis());
      delay(75);
    }
  }
  rpm = 0; speedMph = 0; speedDisp = 0;
  spdPeak = -1;
  updateTach();
  updateSpeed(millis());
}

// ----------------------------------------------------
// Notification icons (top centre)
// ----------------------------------------------------
static AlertIcon shownAlerts[MAX_ALERTS];
static uint8_t   shownCount = 0;
static bool      alertActive[S_COUNT];

/**
 * @brief Draws one notification icon at a position.
 *
 * Coolant, oil pressure, battery and knock have drawn symbols; every other
 * sensor gets a labelled box using ::ALERT_L1 / ::ALERT_L2.
 *
 * @param sensor  Sensor index the icon represents.
 * @param ox      Left edge (px).
 * @param oy      Top edge (px).
 * @param col     RGB565 colour (orange = warning, red = critical).
 * @return        None.
 *
 * @code
 * drawIcon(S_CLT, iconX(0), ALERT_Y, RED);
 * @endcode
 */
static void drawIcon(uint8_t sensor, int16_t ox, int16_t oy, uint16_t col) {
  switch (sensor) {
    case S_CLT: {   // thermometer + heat waves
      gfx->fillRoundRect(ox + 9, oy + 2, 10, 26, 5, col);
      gfx->fillCircle(ox + 14, oy + 30, 8, col);
      gfx->fillRoundRect(ox + 12, oy + 6, 4, 22, 2, C_BG);
      gfx->fillCircle(ox + 14, oy + 30, 4, C_BG);
      gfx->fillRect(ox + 13, oy + 16, 2, 14, col);
      gfx->fillCircle(ox + 14, oy + 30, 3, col);
      for (int k = 0; k < 2; k++) {
        int wy = oy + 10 + k * 13;
        for (int j = 0; j < 4; j++) {
          int x0 = ox + 22 + j * 4;
          int ya = wy + ((j & 1) ? 4 : 0);
          int yb = wy + ((j & 1) ? 0 : 4);
          gfx->drawLine(x0, ya, x0 + 4, yb, col);
          gfx->drawLine(x0, ya + 1, x0 + 4, yb + 1, col);
        }
      }
      break;
    }
    case S_OIL_P: { // oil can + drop
      gfx->fillRoundRect(ox + 5, oy + 16, 22, 16, 3, col);
      gfx->fillRect(ox + 9, oy + 12, 10, 4, col);
      gfx->drawLine(ox + 5, oy + 18, ox + 2, oy + 10, col);
      gfx->drawLine(ox + 6, oy + 18, ox + 3, oy + 10, col);
      gfx->drawLine(ox + 2, oy + 10, ox + 10, oy + 10, col);
      gfx->drawLine(ox + 2, oy + 11, ox + 10, oy + 11, col);
      for (int k = 0; k < 3; k++) gfx->drawLine(ox + 26, oy + 20 + k, ox + 37, oy + 12 + k, col);
      gfx->fillTriangle(ox + 35, oy + 20, ox + 31, oy + 29, ox + 39, oy + 29, col);
      gfx->fillCircle(ox + 35, oy + 31, 4, col);
      break;
    }
    case S_BATT: {  // battery
      gfx->drawRect(ox + 3, oy + 10, 34, 24, col);
      gfx->drawRect(ox + 4, oy + 11, 32, 22, col);
      gfx->fillRect(ox + 8, oy + 6, 6, 4, col);
      gfx->fillRect(ox + 26, oy + 6, 6, 4, col);
      gfx->fillRect(ox + 8, oy + 21, 8, 2, col);      // +
      gfx->fillRect(ox + 11, oy + 18, 2, 8, col);
      gfx->fillRect(ox + 24, oy + 21, 8, 2, col);     // -
      break;
    }
    case S_KNOCK: { // lightning bolt
      gfx->fillTriangle(ox + 26, oy + 2,  ox + 10, oy + 22, ox + 23, oy + 16, col);
      gfx->fillTriangle(ox + 10, oy + 22, ox + 19, oy + 22, ox + 23, oy + 16, col);
      gfx->fillTriangle(ox + 19, oy + 22, ox + 14, oy + 38, ox + 32, oy + 16, col);
      gfx->fillTriangle(ox + 19, oy + 22, ox + 32, oy + 16, ox + 23, oy + 16, col);
      break;
    }
    default: {      // labelled box
      gfx->drawRoundRect(ox, oy + 2, ICON_SZ, 36, 5, col);
      gfx->drawRoundRect(ox + 1, oy + 3, ICON_SZ - 2, 34, 4, col);
      const char *a = ALERT_L1[sensor];
      const char *b = ALERT_L2[sensor];
      if (b[0]) {
        drawCentered(a, ox, ICON_SZ, oy + 6, 2, col);
        drawCentered(b, ox, ICON_SZ, oy + 26, 1, col);
      } else {
        drawCentered(a, ox, ICON_SZ, oy + 12, 2, col);
      }
      break;
    }
  }
}

/**
 * @brief Erases an icon area (used for the "off" phase of a flashing icon).
 *
 * @param ox  Left edge (px).
 * @param oy  Top edge (px).
 * @return    None.
 *
 * @code
 * clearIcon(iconX(k), ALERT_Y);
 * @endcode
 */
static void clearIcon(int16_t ox, int16_t oy) {
  gfx->fillRect(ox, oy, ICON_SZ, ICON_SZ, C_BG);
}

/**
 * @brief Says whether a flashing icon should be visible right now.
 *
 * Warning flashes at 400 ms per phase, critical at 180 ms; steady is always on.
 *
 * @param mode  MODE_STEADY, MODE_WARN or MODE_CRIT.
 * @return      true during the "on" phase.
 *
 * @code
 * if (phaseOn(MODE_CRIT)) drawIcon(S_OIL_P, x, ALERT_Y, RED);
 * @endcode
 */
static bool phaseOn(uint8_t mode) {
  uint32_t now = millis();
  if (mode == MODE_WARN) return ((now / 400) & 1) == 0;
  if (mode == MODE_CRIT) return ((now / 180) & 1) == 0;
  return true;
}

/**
 * @brief Returns the X position of the k-th notification icon so the group is centred on the tach.
 *
 * @param k  Icon index (0..shownCount-1).
 * @return   Left edge in screen pixels.
 *
 * @code
 * drawIcon(s, iconX(k), ALERT_Y, col);
 * @endcode
 */
static int16_t iconX(int k) {
  return TACH_CX - (shownCount * ICON_PITCH - (ICON_PITCH - ICON_SZ)) / 2 + k * ICON_PITCH;
}

/**
 * @brief Decides which warning icons to show and flashes them.
 *
 * A sensor alerts when its value leaves goodLo..goodHi (with a 3% hysteresis
 * before it clears); critical is more than 25% of the range beyond the limit.
 * Launch control shows a steady white icon when armed and a fast green flash
 * when active. Sensors with no data yet never alert. At most MAX_ALERTS icons
 * are shown, highest priority first. The strip is redrawn only when the set
 * changes; otherwise only icons whose flash phase changed are repainted.
 *
 * @return None.
 *
 * @code
 * updateAlerts();   // every frame from sensorsUpdate()
 * @endcode
 */
static void updateAlerts() {
  uint8_t  cSensor[S_COUNT], cPrio[S_COUNT], cMode[S_COUNT];
  uint16_t cCol[S_COUNT];
  int nc = 0;

  for (int i = 0; i < S_COUNT; i++) {
    const SensorDef &d = DEFS[i];
    float v = sens[i];
    if (!sensSeen[i]) continue;            // no data received yet -> no alert

    if (d.isState) {                       // launch control
      int st = (int)(v + 0.5f);
      if (st >= 1) {
        cSensor[nc] = i;
        if (st == 1) { cCol[nc] = WHITE;   cMode[nc] = MODE_STEADY; cPrio[nc] = 0; }
        else         { cCol[nc] = C_GREEN; cMode[nc] = MODE_CRIT;   cPrio[nc] = 2; }
        nc++;
      }
      continue;
    }

    float lo = d.goodLo, hi = d.goodHi, span = hi - lo, h = span * 0.03f;
    bool out  = (v < lo || v > hi);
    bool back = (v >= lo + h && v <= hi - h);
    if (!alertActive[i]) { if (out) alertActive[i] = true; }
    else if (back)       { alertActive[i] = false; }
    if (!alertActive[i]) continue;

    bool crit = (v < lo - span * 0.25f || v > hi + span * 0.25f);
    cSensor[nc] = i;
    cCol[nc]  = crit ? RED : C_ORANGE;
    cMode[nc] = crit ? MODE_CRIT : MODE_WARN;
    cPrio[nc] = crit ? 3 : 1;
    nc++;
  }

  // pick the highest-priority MAX_ALERTS, keep them in sensor order
  bool pick[S_COUNT] = {false};
  for (int k = 0; k < MAX_ALERTS; k++) {
    int best = -1;
    for (int j = 0; j < nc; j++)
      if (!pick[j] && (best < 0 || cPrio[j] > cPrio[best])) best = j;
    if (best < 0) break;
    pick[best] = true;
  }
  AlertIcon nu[MAX_ALERTS];
  int n = 0;
  for (int j = 0; j < nc && n < MAX_ALERTS; j++) {
    if (!pick[j]) continue;
    nu[n].sensor = cSensor[j];
    nu[n].col    = cCol[j];
    nu[n].mode   = cMode[j];
    nu[n].vis    = false;
    n++;
  }

  bool changed = (n != shownCount);
  for (int k = 0; !changed && k < n; k++) {
    if (nu[k].sensor != shownAlerts[k].sensor || nu[k].col != shownAlerts[k].col ||
        nu[k].mode != shownAlerts[k].mode) changed = true;
  }

  if (changed) {
    gfx->fillRect(STRIP_X, STRIP_Y, STRIP_W, STRIP_H, C_BG);
    shownCount = n;
    for (int k = 0; k < n; k++) {
      shownAlerts[k] = nu[k];
      shownAlerts[k].vis = phaseOn(nu[k].mode);
      if (shownAlerts[k].vis) drawIcon(nu[k].sensor, iconX(k), ALERT_Y, nu[k].col);
    }
  } else {
    for (int k = 0; k < shownCount; k++) {
      bool vis = phaseOn(shownAlerts[k].mode);
      if (vis != shownAlerts[k].vis) {
        shownAlerts[k].vis = vis;
        if (vis) drawIcon(shownAlerts[k].sensor, iconX(k), ALERT_Y, shownAlerts[k].col);
        else     clearIcon(iconX(k), ALERT_Y);
      }
    }
  }
}

// ----------------------------------------------------
// Data source
// ----------------------------------------------------
/**
 * @brief Supplies the live data for this frame.
 *
 * With SIMULATE 1 it generates demo rpm, gear, speed and sensor values. With
 * SIMULATE 0 it does nothing, because canTask() fills the values on core 0.
 *
 * @return None.
 *
 * @code
 * readSensors();
 * updateTach();
 * @endcode
 */
static void readSensors() {
#if SIMULATE
  static const uint8_t SIM_SHIFT[7] = {1, 0, 2, 3, 4, 5, 6};   // 1 -> N -> 2 -> 3 -> 4 -> 5 -> 6
  static const float   RATIO[7]     = {0, 3.5f, 5.0f, 6.5f, 8.0f, 9.5f, 11.0f};  // mph per 1000 rpm
  static float simSpeed = 0;

  float t = millis() / 1000.0f;
  const float STAGE = 3.5f;
  int stage = ((int)(t / STAGE)) % 7;
  float cyc = fmodf(t, STAGE) / STAGE;

  gear = SIM_SHIFT[stage];
  rpm = 2500 + cyc * cyc * 5800;

  float target = (gear > 0) ? rpm / 1000.0f * RATIO[gear] : simSpeed * 0.995f;
  simSpeed += (target - simSpeed) * 0.06f;
  speedMph = simSpeed;

  for (int i = 0; i < S_COUNT; i++) {
    const SensorDef &d = DEFS[i];
    if (d.isState) { sens[i] = (int)(t / 3.0f) % 3; continue; }
    float center = (d.goodLo + d.goodHi) * 0.5f;
    float amp = (d.goodHi - d.goodLo) * 0.5f * 1.3f;
    float v = center + amp * sinf(t * (0.15f + 0.05f * i) + i);
    sens[i] = constrain(v, d.minV, d.maxV);
  }
#else
  // CAN data is filled in by canTask() on core 0
#endif
}

// ----------------------------------------------------
// Public entry points
// ----------------------------------------------------
void sensorsInit() {
  prefs.begin("kartdash", false);
  loadSlots();

  for (int i = 0; i < S_COUNT; i++) sens[i] = DEFS[i].minV;
  for (int i = 0; i < S_COUNT; i++) sensSeen[i] = SIMULATE;

  buildSegments();
  buildSpeedColors();
  drawTachStatic();
  drawSpeedStatic();
}

void sensorsUpdate() {
  uint32_t now = millis();
  readSensors();
  updateTach();
  updateSpeed(now);
  updateAlerts();
  for (int i = 0; i < NUM_SLOTS; i++) updateSlot(slots[i]);
}