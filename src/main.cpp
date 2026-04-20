#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <algorithm>

namespace {
constexpr char kApSsid[] = "MulticastLoRa";
constexpr char kApPassword[] = "MulticastLoRa123";
constexpr uint8_t kApChannel = 1;
constexpr uint8_t kApMaxClients = 2;

constexpr uint16_t kMulticastPort = 22501;
const IPAddress kMulticastIp(239, 0, 0, 1);

constexpr long kLoraFrequencyHz = 915E6;
constexpr int kLoraSck = 5;
constexpr int kLoraMiso = 19;
constexpr int kLoraMosi = 27;
constexpr int kLoraSs = 18;
constexpr int kLoraRst = 14;
constexpr int kLoraDio0 = 26;
constexpr uint8_t kLoraInitMaxRetries = 10;

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
  udp.beginPacketMulticast(kMulticastIp, kMulticastPort, WiFi.softAPIP());
  udp.write(data, len);
  udp.endPacket();
}

void setupSoftAp() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(kApSsid, kApPassword, kApChannel, 0, kApMaxClients);
  Serial.print("SoftAP started, IP: ");
  Serial.println(WiFi.softAPIP());
}

void setupMulticastUdp() {
  if (!udp.beginMulticast(WiFi.softAPIP(), kMulticastIp, kMulticastPort)) {
    Serial.println(
        "Failed to start multicast UDP listener; WiFi AP stays active but bridge is disabled");
  } else {
    Serial.printf("Listening multicast %u.%u.%u.%u:%u\n", kMulticastIp[0],
                  kMulticastIp[1], kMulticastIp[2], kMulticastIp[3],
                  kMulticastPort);
  }
}

void setupLora() {
  SPI.begin(kLoraSck, kLoraMiso, kLoraMosi, kLoraSs);
  LoRa.setPins(kLoraSs, kLoraRst, kLoraDio0);

  uint8_t attempts = 0;
  while (attempts < kLoraInitMaxRetries && !LoRa.begin(kLoraFrequencyHz)) {
    ++attempts;
    Serial.printf("LoRa init failed (attempt %u/%u), retrying...\n", attempts,
                  kLoraInitMaxRetries);
    delay(2000);
  }

  loraReady = attempts < kLoraInitMaxRetries;
  if (loraReady) {
    Serial.println("LoRa init OK");
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
