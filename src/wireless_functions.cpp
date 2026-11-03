#include "wireless.h"
#include "debug.h"

#if ENABLE_WIRELESS
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <esp_wifi.h>
#include "can.h"
#include "dash_page.h"

// 17 bytes per frame on the wire (little-endian): time ms, CAN id, length, 8 data bytes
#pragma pack(push, 1)
struct WFrame { uint32_t t; uint32_t id; uint8_t len; uint8_t data[8]; };
#pragma pack(pop)

static const int QUEUE_LEN = 512;
static const int BATCH     = 48;

static QueueHandle_t     q = nullptr;
static volatile uint32_t dropped = 0;
static WebServer         http(80);
static WebSocketsServer  ws(WS_PORT);

// Called from the CAN task: must never block.
static void frameHook(uint32_t id, uint8_t len, const uint8_t *buf) {
  WFrame f;
  f.t   = millis();
  f.id  = id;
  f.len = len > 8 ? 8 : len;
  memset(f.data, 0, 8);
  memcpy(f.data, buf, f.len);
  if (xQueueSend(q, &f, 0) != pdTRUE) dropped++;
}

static void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_CONNECTED)    debugMsg("WS: client connected");
  if (type == WStype_DISCONNECTED) debugMsg("WS: client disconnected");
}

// Twice a second: Wi-Fi signal strength of each connected device (as heard by the kart)
// plus the dropped-frame counter, as a small JSON text message.
//   {"t":"s","rssi":[-61],"drop":0,"up":123}
static void sendStatus() {
  if (ws.connectedClients() == 0) return;

  char buf[160];
  int n = snprintf(buf, sizeof(buf), "{\"t\":\"s\",\"rssi\":[");

  wifi_sta_list_t list;
  if (esp_wifi_ap_get_sta_list(&list) == ESP_OK) {
    for (int i = 0; i < list.num && i < 4; i++)
      n += snprintf(buf + n, sizeof(buf) - n, "%s%d", i ? "," : "", (int)list.sta[i].rssi);
  }
  n += snprintf(buf + n, sizeof(buf) - n, "],\"drop\":%u,\"up\":%u}",
                (unsigned)dropped, (unsigned)(millis() / 1000));

  if (n > 0 && n < (int)sizeof(buf)) ws.broadcastTXT(buf, n);
}

static void wirelessTask(void *) {
  vTaskDelay(pdMS_TO_TICKS(1500));            // let the supply rail settle after the sweep
  WiFi.mode(WIFI_AP);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);        // set before the AP starts transmitting
  WiFi.softAP(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL, 0, 2);   // max 2 clients
  debugMsg("Wi-Fi AP up: http://192.168.4.1/");

  http.on("/", []() { http.send_P(200, "text/html", DASH_PAGE); });
  http.begin();
  ws.begin();
  ws.onEvent(onWsEvent);

  static WFrame batch[BATCH];
  uint32_t lastStatus = 0;
  for (;;) {
    http.handleClient();
    ws.loop();

    int n = 0;
    while (n < BATCH && xQueueReceive(q, &batch[n], 0) == pdTRUE) n++;   // always drain
    if (n && ws.connectedClients() > 0)
      ws.broadcastBIN((uint8_t *)batch, n * sizeof(WFrame));

    if (millis() - lastStatus >= 500) {
      lastStatus = millis();
      sendStatus();
    }

    vTaskDelay(pdMS_TO_TICKS(n == BATCH ? 1 : 15));
  }
}

void wirelessBegin() {
  q = xQueueCreate(QUEUE_LEN, sizeof(WFrame));
  canSetFrameHook(frameHook);
  xTaskCreatePinnedToCore(wirelessTask, "wifi", 8192, NULL, 2, NULL, 0);
}

uint8_t  wirelessClients() { return ws.connectedClients(); }
uint32_t wirelessDropped() { return dropped; }

#else
void     wirelessBegin()   {}
uint8_t  wirelessClients() { return 0; }
uint32_t wirelessDropped() { return 0; }
#endif