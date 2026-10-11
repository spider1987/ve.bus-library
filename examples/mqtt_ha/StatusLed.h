#pragma once
#include <stdint.h>

enum class LedStatus : uint8_t { Startup, WifiDown, BusDown, WifiWeak, MqttDown, TestRunning, Ready, Updating };

inline LedStatus chooseLedStatus(bool updating, bool wifi, bool bus, int rssi, bool mqtt, bool test) {
    if (updating) return LedStatus::Updating;
    if (!wifi) return LedStatus::WifiDown;
    if (!bus) return LedStatus::BusDown;
    if (rssi <= -80) return LedStatus::WifiWeak;
    if (!mqtt) return LedStatus::MqttDown;
    if (test) return LedStatus::TestRunning;
    return LedStatus::Ready;
}

inline const char *ledStatusName(LedStatus s) {
    switch (s) {
    case LedStatus::Startup: return "startup";
    case LedStatus::WifiDown: return "wifi_connecting";
    case LedStatus::BusDown: return "vebus_disconnected";
    case LedStatus::WifiWeak: return "wifi_weak";
    case LedStatus::MqttDown: return "mqtt_disconnected";
    case LedStatus::TestRunning: return "test_running";
    case LedStatus::Ready: return "ready";
    case LedStatus::Updating: return "firmware_update";
    }
    return "unknown";
}

inline uint32_t statusLedRGB(LedStatus s, uint32_t elapsedMs) {
    // RGB, global brightness is reduced separately. No delays or IRQ masking.
    switch (s) {
    case LedStatus::Startup: return 0x0080FF;
    case LedStatus::WifiDown: return elapsedMs % 1000 < 500 ? 0x0080FF : 0;
    case LedStatus::BusDown: return elapsedMs % 500 < 250 ? 0xFF0000 : 0;
    case LedStatus::WifiWeak: return 0xFFA000;
    case LedStatus::MqttDown: return elapsedMs % 2000 < 1000 ? 0xFFA000 : 0;
    case LedStatus::TestRunning: return elapsedMs % 2000 < 1800 ? 0x00FF00 : 0;
    case LedStatus::Ready: return 0x00FF00;
    case LedStatus::Updating: return 0xA000FF;
    }
    return 0;
}

inline bool statusLedSelfCheck() {
    return chooseLedStatus(true, false, false, -90, false, true) == LedStatus::Updating &&
        chooseLedStatus(false, false, true, -55, true, true) == LedStatus::WifiDown &&
        chooseLedStatus(false, true, false, -90, false, true) == LedStatus::BusDown &&
        chooseLedStatus(false, true, true, -80, true, true) == LedStatus::WifiWeak &&
        chooseLedStatus(false, true, true, -79, false, true) == LedStatus::MqttDown &&
        chooseLedStatus(false, true, true, -55, true, true) == LedStatus::TestRunning &&
        chooseLedStatus(false, true, true, -55, true, false) == LedStatus::Ready &&
        statusLedRGB(LedStatus::BusDown, 0) == 0xFF0000 &&
        statusLedRGB(LedStatus::BusDown, 250) == 0 &&
        statusLedRGB(LedStatus::BusDown, 500) == 0xFF0000 &&
        statusLedRGB(LedStatus::TestRunning, 1800) == 0;
}
