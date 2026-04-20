#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>

namespace {
constexpr char kApSsid[] = "MulticastLoRa";
constexpr char kApPassword[] = "multicastlora";
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

constexpr size_t kMaxPayloadSize = 255;

WiFiUDP udp;
uint8_t payload[kMaxPayloadSize];

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
    Serial.println("Failed to start multicast UDP listener");
  } else {
    Serial.printf("Listening multicast %u.%u.%u.%u:%u\n", kMulticastIp[0],
                  kMulticastIp[1], kMulticastIp[2], kMulticastIp[3],
                  kMulticastPort);
  }
}

void setupLora() {
  SPI.begin(kLoraSck, kLoraMiso, kLoraMosi, kLoraSs);
  LoRa.setPins(kLoraSs, kLoraRst, kLoraDio0);

  if (!LoRa.begin(kLoraFrequencyHz)) {
    Serial.println("LoRa init failed");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("LoRa init OK");
}

void handleUdpToLora() {
  int packetSize = udp.parsePacket();
  if (packetSize <= 0) {
    return;
  }

  const size_t bytesToRead =
      (static_cast<size_t>(packetSize) < kMaxPayloadSize)
          ? static_cast<size_t>(packetSize)
          : kMaxPayloadSize;
  const size_t len = udp.read(payload, bytesToRead);
  if (len == 0) {
    return;
  }

  printPayload("UDP", payload, len);
  forwardToLora(payload, len);
}

void handleLoraToUdp() {
  const int packetSize = LoRa.parsePacket();
  if (packetSize <= 0) {
    return;
  }

  size_t len = 0;
  while (LoRa.available() && len < kMaxPayloadSize) {
    payload[len++] = static_cast<uint8_t>(LoRa.read());
  }

  if (len == 0) {
    return;
  }

  printPayload("LoRa", payload, len);
  forwardToMulticast(payload, len);
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
