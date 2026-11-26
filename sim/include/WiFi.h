// WiFi.h (simulator) - pretend networks around the Cardputer:
//   HomeWiFi      WPA2, strong   any password connects, except "wrong" (to try the error path)
//   iPhone        WPA2           any password connects
//   Coffee House  open           connects without a password
//   OldRouter     WPA2, weak     never connects (to try "Could not connect")
#pragma once
#include "Arduino.h"
#include <functional>

enum wifi_auth_mode_t { WIFI_AUTH_OPEN = 0, WIFI_AUTH_WPA2_PSK = 3 };
enum { WIFI_STA = 1 };
enum { WIFI_ALL_CHANNEL_SCAN = 1, WIFI_CONNECT_AP_BY_SIGNAL = 0 };
enum WiFiEvent_t { ARDUINO_EVENT_WIFI_STA_GOT_IP = 7, ARDUINO_EVENT_WIFI_STA_DISCONNECTED = 5 };
struct WiFiEventInfo_t { struct { uint8_t reason; } wifi_sta_disconnected; };

struct IPAddress {
    uint8_t a = 0, b = 0, c = 0, d = 0;
    String toString() const { char s[20]; snprintf(s, sizeof(s), "%d.%d.%d.%d", a, b, c, d); return s; }
};

class SimWiFi {
public:
    void mode(int) {}
    void setAutoReconnect(bool) {}
    void setScanMethod(int) {}
    void setSortMethod(int) {}
    void begin(const char* ssid, const char* pass, int channel = 0, const uint8_t* bssid = nullptr);
    void disconnect(bool = false, bool = false);
    bool isConnected();
    String SSID();
    int32_t RSSI();
    IPAddress localIP();
    uint8_t* BSSID();
    int32_t channel();

    int16_t scanNetworks(bool async = false, bool hidden = false);
    String SSID(int i);
    int32_t RSSI(int i);
    wifi_auth_mode_t encryptionType(int i);
    void scanDelete() {}

    void onEvent(std::function<void(WiFiEvent_t, WiFiEventInfo_t)> cb, WiFiEvent_t ev);
};
extern SimWiFi WiFi;
