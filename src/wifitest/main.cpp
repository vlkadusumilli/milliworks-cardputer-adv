// Wi-Fi diagnostic firmware (separate build: `pio run -e wifitest`).
//
// Why: Launcher only says "wrong password" for every failure. This shows the
// router's real disconnect reason code, tries each access point (mesh node)
// with that name separately, and takes the password raw over USB - no quote
// parsing - so special characters can't be mangled.
//
// Protocol over serial (115200, LF):  SSID:<name>   PASS:<password>   GO
// tools/wifi_test_send.py sends these for you without printing the password.
#include <M5Cardputer.h>
#include <WiFi.h>

static String ssid, pass;
static volatile int lastReason = 0;
static volatile bool gotIp = false;
static String screenLines[7];

static const char* reasonName(int r) {
    switch (r) {
        case 2:   return "AUTH_EXPIRE";
        case 3:   return "AUTH_LEAVE";
        case 4:   return "ASSOC_EXPIRE";
        case 6:   return "NOT_AUTHED";
        case 7:   return "NOT_ASSOCED";
        case 8:   return "ASSOC_LEAVE";
        case 15:  return "4WAY_HANDSHAKE_TIMEOUT (usually wrong password)";
        case 16:  return "GROUP_KEY_UPDATE_TIMEOUT";
        case 23:  return "802_1X_AUTH_FAILED";
        case 39:  return "TIMEOUT";
        case 200: return "BEACON_TIMEOUT";
        case 201: return "NO_AP_FOUND";
        case 202: return "AUTH_FAIL";
        case 203: return "ASSOC_FAIL";
        case 204: return "HANDSHAKE_TIMEOUT (usually wrong password)";
        case 205: return "CONNECTION_FAIL";
        case 210: return "NO_AP_FOUND_COMPATIBLE_SECURITY";
        case 211: return "NO_AP_FOUND_AUTHMODE_THRESHOLD";
        case 212: return "NO_AP_FOUND_RSSI_THRESHOLD";
        default:  return "other";
    }
}

static const char* authName(wifi_auth_mode_t a) {
    switch (a) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
        default:                        return "?";
    }
}

// Prints to USB serial and keeps the last lines on the screen.
static void say(const String& s) {
    Serial.println(s);
    for (int i = 0; i < 6; i++) screenLines[i] = screenLines[i + 1];
    screenLines[6] = s;
    auto& d = M5Cardputer.Display;
    d.fillScreen(BLACK);
    d.setTextColor(GREEN);
    d.setCursor(0, 0);
    d.setFont(&fonts::Font2);
    d.println("Wi-Fi diagnostic");
    d.setFont(&fonts::Font0);
    d.setTextColor(WHITE);
    for (auto& l : screenLines) d.println(l.substring(0, 39));
}

static bool attempt(const uint8_t* bssid, int channel, uint32_t timeoutMs) {
    WiFi.disconnect(true, true);
    delay(300);
    lastReason = 0;
    gotIp = false;
    if (bssid) WiFi.begin(ssid.c_str(), pass.c_str(), channel, bssid);
    else WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t start = millis();
    while (millis() - start < timeoutMs && !gotIp) delay(50);
    if (gotIp) {
        say("  CONNECTED  IP " + WiFi.localIP().toString());
        return true;
    }
    say("  FAILED  reason " + String(lastReason) + " " + reasonName(lastReason));
    return false;
}

static void runTest() {
    say("SSID '" + ssid + "'  password " + String(pass.length()) + " chars");
    say("Scanning...");
    int n = WiFi.scanNetworks(false, true);
    struct Ap { uint8_t bssid[6]; int ch; int rssi; };
    std::vector<Ap> aps;
    for (int i = 0; i < n; i++) {
        if (WiFi.SSID(i) != ssid) continue;
        Ap ap;
        memcpy(ap.bssid, WiFi.BSSID(i), 6);
        ap.ch = WiFi.channel(i);
        ap.rssi = WiFi.RSSI(i);
        aps.push_back(ap);
        say("AP " + WiFi.BSSIDstr(i) + " ch" + String(ap.ch) + " " + String(ap.rssi) + "dB " +
            authName(WiFi.encryptionType(i)));
    }
    WiFi.scanDelete();
    if (aps.empty()) { say("No AP with that name found"); say("DONE"); return; }

    say("Try 1: normal connect");
    bool ok = attempt(nullptr, 0, 20000);
    for (size_t i = 0; i < aps.size() && !ok; i++) {
        char mac[18];
        snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", aps[i].bssid[0], aps[i].bssid[1],
                 aps[i].bssid[2], aps[i].bssid[3], aps[i].bssid[4], aps[i].bssid[5]);
        say("Try " + String(i + 2) + ": AP " + mac);
        ok = attempt(aps[i].bssid, aps[i].ch, 20000);
    }
    say(ok ? "RESULT: works with this password" : "RESULT: all attempts failed");
    say("DONE");
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, false);
    M5Cardputer.Display.setRotation(1);
    Serial.begin(115200);
    WiFi.persistent(false);          // don't store anything in flash
    WiFi.mode(WIFI_STA);
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
        lastReason = info.wifi_sta_disconnected.reason;
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) { gotIp = true; }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    say("Waiting for SSID/PASS over USB");
}

void loop() {
    static String line;
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r') continue;
        if (c != '\n') { if (line.length() < 200) line += c; continue; }
        if (line.startsWith("SSID:")) { ssid = line.substring(5); say("Got SSID"); }
        else if (line.startsWith("PASS:")) { pass = line.substring(5); say("Got password (" + String(pass.length()) + " chars)"); }
        else if (line == "GO") runTest();
        line = "";
    }
    delay(5);
}
