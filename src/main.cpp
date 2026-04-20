#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <algorithm>
#include <esp_efuse.h>
#include "pinout.h"

namespace {
// Last 4 hex digits of the base MAC address are appended at runtime.
constexpr char kApSsidPrefix[] = "MCLoRa";
constexpr char kApPassword[] = "MulticastLoRa123";
constexpr uint8_t kApChannel = 1;
constexpr uint8_t kApMaxClients = 2;

constexpr uint16_t kMulticastPort = 22501;
const IPAddress kMulticastIp(239, 225, 0, 1);

// LoRa radio configuration
constexpr long   kLoraFrequencyHz      = 869525000L; // 869.525 MHz
constexpr int    kLoraSpreadingFactor  = 7;          // SF7–SF12
constexpr long   kLoraBandwidthHz      = 250000L;    // 250 kHz
constexpr int    kLoraCodingRateDenom  = 5;          // 4/5
constexpr bool   kLoraCrcEnabled       = true;
constexpr uint8_t kLoraInitMaxRetries  = 10;

char apSsid[16]; // "MCLoRa" + '_' + 4 hex chars + '\0'

constexpr size_t kMaxPayloadSize = 255;

WiFiUDP udp;
uint8_t udpPayload[kMaxPayloadSize];
uint8_t loraPayload[kMaxPayloadSize];
bool loraReady = false;

void printPayload(const char *source, const uint8_t *data, size_t len) {
  Serial.printf("[%s] %u bytes: ", source, static_cast<unsigned>(len));
  for (size_t i = 0; i < len; ++i) {
    const uint8_t c = data[i];
    if (c >= 32 && c <= 126) {
      Serial.write(c);
    } else {
      Serial.print('.');
    }
  }
  Serial.println();
}

void forwardToLora(const uint8_t *data, size_t len) {
  if (!loraReady) {
    static bool warnedUnavailable = false;
    if (!warnedUnavailable) {
      Serial.println("LoRa unavailable: dropping UDP->LoRa payloads");
      warnedUnavailable = true;
    }
    return;
  }
  LoRa.beginPacket();
  LoRa.write(data, len);
  LoRa.endPacket();
}

void forwardToMulticast(const uint8_t *data, size_t len) {
  udp.beginPacket(kMulticastIp, kMulticastPort);
  udp.write(data, len);
  udp.endPacket();
}

void buildApSsid() {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  snprintf(apSsid, sizeof(apSsid), "%s_%02X%02X",
           kApSsidPrefix, mac[4], mac[5]);
}

void setupSoftAp() {
  buildApSsid();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid, kApPassword, kApChannel, 0, kApMaxClients);
  Serial.printf("SoftAP started, SSID: %s, IP: %s\n",
                apSsid, WiFi.softAPIP().toString().c_str());
}

void setupMulticastUdp() {
  if (!udp.beginMulticast(kMulticastIp, kMulticastPort)) {
    Serial.println(
        "Failed to start multicast UDP listener; WiFi AP stays active but bridge is disabled");
  } else {
    Serial.printf("Listening multicast %u.%u.%u.%u:%u\n", kMulticastIp[0],
                  kMulticastIp[1], kMulticastIp[2], kMulticastIp[3],
                  kMulticastPort);
  }
}

void setupLora() {
  SPI.begin(Pinout::kLoraSck, Pinout::kLoraMiso, Pinout::kLoraMosi,
            Pinout::kLoraSs);
  LoRa.setPins(Pinout::kLoraSs, Pinout::kLoraRst, Pinout::kLoraDio0);

  uint8_t attempts = 0;
  while (attempts < kLoraInitMaxRetries && !LoRa.begin(kLoraFrequencyHz)) {
    ++attempts;
    Serial.printf("LoRa init failed (attempt %u/%u), retrying...\n", attempts,
                  kLoraInitMaxRetries);
    delay(2000);
  }

  loraReady = attempts < kLoraInitMaxRetries;
  if (loraReady) {
    LoRa.setSpreadingFactor(kLoraSpreadingFactor);
    LoRa.setSignalBandwidth(kLoraBandwidthHz);
    LoRa.setCodingRate4(kLoraCodingRateDenom);
    if (kLoraCrcEnabled) LoRa.enableCrc(); else LoRa.disableCrc();
    Serial.printf("LoRa init OK — %.3f MHz SF%d BW%.0fkHz CR4/%d CRC:%s\n",
                  kLoraFrequencyHz / 1e6f, kLoraSpreadingFactor,
                  kLoraBandwidthHz / 1e3f, kLoraCodingRateDenom,
                  kLoraCrcEnabled ? "on" : "off");
  } else {
    Serial.println("LoRa unavailable after max retries; continuing without LoRa");
  }
}

void handleUdpToLora() {
  int packetSize = udp.parsePacket();
  if (packetSize <= 0) {
    return;
  }

  const size_t maxBytesToRead =
      std::min(static_cast<size_t>(packetSize), kMaxPayloadSize);
  if (static_cast<size_t>(packetSize) > kMaxPayloadSize) {
    Serial.printf("UDP packet truncated from %d to %u bytes\n", packetSize,
                  static_cast<unsigned>(kMaxPayloadSize));
  }
  const size_t len = udp.read(udpPayload, maxBytesToRead);
  if (len == 0) {
    return;
  }

  printPayload("UDP", udpPayload, len);
  forwardToLora(udpPayload, len);
}

void handleLoraToUdp() {
  if (!loraReady) {
    return;
  }

  const int packetSize = LoRa.parsePacket();
  if (packetSize <= 0) {
    return;
  }

  size_t len = 0;
  while (LoRa.available() && len < kMaxPayloadSize) {
    loraPayload[len++] = static_cast<uint8_t>(LoRa.read());
  }

  if (len == 0) {
    return;
  }

  printPayload("LoRa", loraPayload, len);
  forwardToMulticast(loraPayload, len);
}
} // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  setupSoftAp();
  setupMulticastUdp();
  setupLora();
}

void loop() {
  handleUdpToLora();
  handleLoraToUdp();
}
