# MulticastLoRa

PlatformIO Arduino firmware for ESP32 LoRa boards (`heltec_wifi_lora_32`) that bridges:

- WiFi SoftAP multicast UDP (`239.0.0.1:22501`) -> LoRa
- LoRa -> WiFi multicast UDP (`239.0.0.1:22501`)

## Behavior

- Starts a SoftAP (`MulticastLoRa`) with max 2 clients.
- Joins multicast group `239.0.0.1` on UDP port `22501`.
- Prints received payloads to serial (`115200`).
- Forwards WiFi multicast payloads over LoRa.
- Forwards LoRa payloads back out as WiFi multicast packets.

## Build

```bash
pio run
```
