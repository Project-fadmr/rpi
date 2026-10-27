// ESP-NOW test sender for test/espnow_rx.py: broadcasts "hello N" every
// second, and every 5th message as 1000 bytes ("big N" + padding) to test
// ESP-NOW v2.0 reassembly. Arduino-ESP32 core 3.x.
// CHANNEL must equal ESPNOW_CHANNEL in /etc/conf.d/espnow on the Pi.
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define CHANNEL 1

static const uint8_t bcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static uint8_t msg[1000];

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  WiFi.setTxPower(WIFI_POWER_15dBm);  // 100% received at 3-4 m with 11-19.5 dBm (C3 SuperMini)
  if (esp_now_init() != ESP_OK) Serial.println("esp_now_init failed");
  esp_now_peer_info_t p = {};
  memcpy(p.peer_addr, bcast, 6);
  p.channel = CHANNEL;
  esp_now_add_peer(&p);
  Serial.printf("sending on channel %d from %s\n", CHANNEL, WiFi.macAddress().c_str());
}

void loop() {
  static uint32_t n;
  int len;
  if (n % 5 == 4) {
    memset(msg, 'x', sizeof msg);
    len = sizeof msg;
    memcpy(msg, "big ", 4);
    snprintf((char *)msg + 4, 12, "%lu", (unsigned long)n);
  } else {
    len = snprintf((char *)msg, sizeof msg, "hello %lu", (unsigned long)n);
  }
  esp_err_t err = esp_now_send(bcast, msg, len);
  if (err != ESP_OK) Serial.printf("send %lu (%d bytes) failed: %s\n", (unsigned long)n, len, esp_err_to_name(err));
  n++;
  delay(1000);
}
