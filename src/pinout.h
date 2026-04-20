#pragma once

namespace Pinout {
#if defined(BOARD_HELTEC_WIFI_LORA_32)
constexpr int kLoraSck = 5;
constexpr int kLoraMiso = 19;
constexpr int kLoraMosi = 27;
constexpr int kLoraSs = 18;
constexpr int kLoraRst = 14;
constexpr int kLoraDio0 = 26;
#elif defined(BOARD_LILYGO_T3_V161)
// LilyGO T3 1.6.1 (TTGO LoRa32-OLED V1 profile)
constexpr int kLoraSck = 5;
constexpr int kLoraMiso = 19;
constexpr int kLoraMosi = 27;
constexpr int kLoraSs = 18;
constexpr int kLoraRst = 14;
constexpr int kLoraDio0 = 26;
#elif defined(BOARD_TTGO_LORA32_V21)
// TTGO LoRa32 V2.1-1.6 (PlatformIO ttgo-lora32-v21 profile)
constexpr int kLoraSck = 5;
constexpr int kLoraMiso = 19;
constexpr int kLoraMosi = 27;
constexpr int kLoraSs = 18;
constexpr int kLoraRst = 23;
constexpr int kLoraDio0 = 26;
#else
#error "Unsupported board: define pin mapping in src/pinout.h"
#endif
} // namespace Pinout
