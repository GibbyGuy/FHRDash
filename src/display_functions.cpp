#include "display.h"
#include "debug.h"
#include "sensors.h"

TCA9554 TCA(0x20);

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, SPI_SCLK, SPI_MOSI, SPI_MISO, HSPI);
Arduino_GFX *gfx = new Arduino_ST7796(bus, LCD_RST, 1 /* rotation */, true);

// ----------------------------------------------------
// Delta-drawn text
// ----------------------------------------------------
void resetField(TextField &f) { f.s[0] = 0; f.x = 0; f.w = 0; f.col = 0; }

// Draws string s (only if it changed) centred in areaX..areaX+areaW at row y.
// New text is drawn opaque, then the part of the OLD text's bounding box that
// the new text does not cover is cleared explicitly.
void updateText(TextField &f, int16_t areaX, int16_t areaW, int16_t y,
                uint8_t size, const char *s, uint16_t col) {
  if (f.w > 0 && f.col == col && strcmp(f.s, s) == 0) return;

  int16_t w  = strlen(s) * 6 * size;
  int16_t h  = 8 * size;
  int16_t nx = areaX + (areaW - w) / 2;

  gfx->setTextSize(size);
  gfx->setTextColor(col, C_BG);
  gfx->setCursor(nx, y);
  gfx->print(s);

  if (f.w > 0) {
    int16_t ox = f.x, oe = f.x + f.w, ne = nx + w;
    if (nx > ox) gfx->fillRect(ox, y, nx - ox, h, C_BG);
    if (oe > ne) gfx->fillRect(ne, y, oe - ne, h, C_BG);
  }

  strncpy(f.s, s, sizeof(f.s) - 1);
  f.s[sizeof(f.s) - 1] = 0;
  f.x = nx; f.w = w; f.col = col;
}

void drawCentered(const char *s, int16_t areaX, int16_t areaW, int16_t y,
                  uint8_t size, uint16_t col) {
  int16_t w = strlen(s) * 6 * size;
  gfx->setTextSize(size);
  gfx->setTextColor(col, C_BG);
  gfx->setCursor(areaX + (areaW - w) / 2, y);
  gfx->print(s);
}

// ----------------------------------------------------
// Setup
// ----------------------------------------------------
void displayInit() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  if (!TCA.begin()) debugMsg("TCA9554 not found!");
  TCA.pinMode1(EXIO_LCD_RST, OUTPUT);
  TCA.pinMode1(EXIO_TP_RST, OUTPUT);
  TCA.write1(EXIO_LCD_RST, LOW);
  TCA.write1(EXIO_TP_RST, LOW);
  delay(20);
  TCA.write1(EXIO_LCD_RST, HIGH);
  TCA.write1(EXIO_TP_RST, HIGH);
  delay(150);

  if (!gfx->begin(40000000)) debugMsg("gfx->begin() failed!");
  gfx->fillScreen(C_BG);

  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);

  showResetReason();
}

void showResetReason() {
  esp_reset_reason_t rr = esp_reset_reason();
  if (rr == ESP_RST_BROWNOUT || rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT ||
      rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT) {
    gfx->setTextSize(2);
    gfx->setTextColor(RED, C_BG);
    gfx->setCursor(8, 8);
    gfx->print("LAST RESET: ");
    gfx->print(resetReasonName(rr));
    delay(2500);
    gfx->fillScreen(C_BG);
  }
}

// ----------------------------------------------------
// Touch
// ----------------------------------------------------
bool readTouch(int16_t &sx, int16_t &sy) {
  Wire.beginTransmission(TOUCH_I2C_ADD);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)TOUCH_I2C_ADD, (uint8_t)5) != 5) return false;

  uint8_t n  = Wire.read() & 0x0F;
  uint8_t xh = Wire.read(), xl = Wire.read();
  uint8_t yh = Wire.read(), yl = Wire.read();
  if (n == 0 || n > 2) return false;

  int16_t rx = ((xh & 0x0F) << 8) | xl;
  int16_t ry = ((yh & 0x0F) << 8) | yl;

#if TOUCH_SWAP_XY
  sx = ry; sy = rx;
#else
  sx = rx; sy = ry;
#endif
#if TOUCH_FLIP_X
  sx = SCR_W - 1 - sx;
#endif
#if TOUCH_FLIP_Y
  sy = SCR_H - 1 - sy;
#endif
  debugTouch(rx, ry, sx, sy);
  return true;
}

void handleTouch() {
  static uint32_t lastPoll = 0;
  static bool down = false;
  static uint8_t misses = 0;
  if (millis() - lastPoll < 15) return;
  lastPoll = millis();

  int16_t x, y;
  if (readTouch(x, y)) {
    misses = 0;
    if (!down) {
      down = true;
      slotHandleTap(x, y);
    }
  } else if (++misses >= 3) {
    down = false;
    misses = 0;
  }
}