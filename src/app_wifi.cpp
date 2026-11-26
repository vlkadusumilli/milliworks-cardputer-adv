// app_wifi.cpp - Wi-Fi setup: connect, remember up to 5 networks, switch, forget.
//
// Networks are stored in flash with the ESP32 "Preferences" library (keys s0..s4 /
// p0..p4), so they survive power-off. At start-up the Cardputer scans and joins the
// strongest network it knows.
#include <WiFi.h>
#include <Preferences.h>
#include "ui.h"
#include "apps.h"
#include "config.h"

static const int MAX_SAVED = 5;

struct Saved { String ssid, pass; };

// ---- storage ----------------------------------------------------------------
static std::vector<Saved> loadSaved() {
    std::vector<Saved> out;
    Preferences p;
    p.begin(PREFS_NS, true);   // read-only
    for (int i = 0; i < MAX_SAVED; i++) {
        String s = p.getString(("s" + String(i)).c_str(), "");
        if (s.length()) out.push_back({s, p.getString(("p" + String(i)).c_str(), "")});
    }
    // Version 1-3 kept a single network under "ssid"/"pass" - still honour it.
    String old = p.getString("ssid", "");
    if (old.length() && out.empty()) out.push_back({old, p.getString("pass", "")});
    p.end();
    return out;
}

static void storeSaved(const std::vector<Saved>& list) {
    Preferences p;
    p.begin(PREFS_NS, false);
    for (int i = 0; i < MAX_SAVED; i++) {
        String k = String(i);
        if (i < (int)list.size()) {
            p.putString(("s" + k).c_str(), list[i].ssid);
            p.putString(("p" + k).c_str(), list[i].pass);
        } else {
            p.remove(("s" + k).c_str());
            p.remove(("p" + k).c_str());
        }
    }
    p.remove("ssid");   // old single-network keys
    p.remove("pass");
    p.end();
}

// Puts a network at the top of the list (newest first), replacing an old entry.
static void rememberNetwork(const String& ssid, const String& pass) {
    auto list = loadSaved();
    for (size_t i = 0; i < list.size(); i++)
        if (list[i].ssid == ssid) { list.erase(list.begin() + i); break; }
    list.insert(list.begin(), {ssid, pass});
    if ((int)list.size() > MAX_SAVED) list.resize(MAX_SAVED);
    storeSaved(list);
}

static void forgetNetwork(const String& ssid) {
    auto list = loadSaved();
    for (size_t i = 0; i < list.size(); i++)
        if (list[i].ssid == ssid) { list.erase(list.begin() + i); break; }
    storeSaved(list);
}

bool wifiHasSaved() { return !loadSaved().empty(); }

// By default the ESP32 joins the FIRST access point it finds with the right name
// (scanning from channel 1). If several devices broadcast the same name - like a
// mesh, or an old extender with an old password - that can be the wrong one.
// These two settings make it scan every channel and pick the strongest signal.
static void preferStrongestAccessPoint() {
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
}

// ---- fast reconnect -----------------------------------------------------------
// Connecting normally means scanning every channel first (2-3 s). After a successful
// connection we remember the access point's hardware address (BSSID) and channel, and
// next time go straight to it. If that hasn't worked after a few seconds (router
// moved channel, different place), we fall back to the normal scan.
static volatile bool gotIpEvent = false;
static bool fastPending = false;
static uint32_t fastStart = 0;

static void normalConnect();

static void rememberAccessPoint() {
    Preferences p;
    p.begin(PREFS_NS, false);
    p.putString("ap_ssid", WiFi.SSID());
    p.putBytes("ap_bssid", WiFi.BSSID(), 6);
    p.putInt("ap_ch", WiFi.channel());
    p.end();
}

void wifiService() {
    if (gotIpEvent) { gotIpEvent = false; fastPending = false; rememberAccessPoint(); }
    if (fastPending && !WiFi.isConnected() && millis() - fastStart > 6000) {
        fastPending = false;           // the shortcut didn't work: scan the normal way
        WiFi.disconnect();
        normalConnect();
    }
}

void wifiAutoConnect() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    preferStrongestAccessPoint();
    clockApplySavedTimeZone();   // also starts internet time sync (app_clock.cpp)
    static bool hooked = false;
    if (!hooked) {
        WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) { gotIpEvent = true; }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
        hooked = true;
    }

    auto list = loadSaved();
    if (list.empty()) return;

    // Shortcut: the access point that worked last time.
    Preferences p;
    p.begin(PREFS_NS, true);
    String apSsid = p.getString("ap_ssid", "");
    uint8_t bssid[6];
    bool haveAp = p.getBytes("ap_bssid", bssid, 6) == 6;
    int ch = p.getInt("ap_ch", 0);
    p.end();
    if (haveAp && ch > 0) {
        for (auto& s : list) if (s.ssid == apSsid) {
            WiFi.begin(s.ssid.c_str(), s.pass.c_str(), ch, bssid);
            fastPending = true;
            fastStart = millis();
            return;
        }
    }
    normalConnect();
}

static void normalConnect() {
    auto list = loadSaved();
    if (list.empty()) return;
    if (list.size() == 1) {                 // one network: no need to scan
        WiFi.begin(list[0].ssid.c_str(), list[0].pass.c_str());
        return;
    }
    // Several saved: join whichever known network is strongest right now.
    int n = WiFi.scanNetworks();
    int best = -1, bestRssi = -1000;
    for (int i = 0; i < n; i++)
        for (size_t k = 0; k < list.size(); k++)
            if (WiFi.SSID(i) == list[k].ssid && WiFi.RSSI(i) > bestRssi) { best = k; bestRssi = WiFi.RSSI(i); }
    WiFi.scanDelete();
    const Saved& s = list[best >= 0 ? best : 0];
    WiFi.begin(s.ssid.c_str(), s.pass.c_str());
}

// ---- connecting -------------------------------------------------------------
static bool connectWithProgress(const String& ssid, const String& pass) {
    WiFi.disconnect();
    preferStrongestAccessPoint();
    WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t start = millis();
    while (millis() - start < 15000) {
        if (WiFi.isConnected()) return true;
        M5Cardputer.update();
        if (readInput().back) break;
        drawHeader("Wi-Fi");
        canvas.setFont(&fonts::Font2);
        canvas.setTextDatum(middle_center);
        canvas.setTextColor(col::text);
        canvas.drawString("Connecting to", SCREEN_W / 2, 50);
        canvas.drawString(ssid, SCREEN_W / 2, 70);
        int w = (SCREEN_W - 40) * (millis() - start) / 15000;
        canvas.fillRect(20, 92, w, 6, col::trace);
        drawHints({{"ESC", "cancel"}});
        pushScreen();
        delay(50);
    }
    return WiFi.isConnected();
}

static void message(const char* line1, const String& line2) {
    drawHeader("Wi-Fi");
    canvas.setFont(&fonts::Font2);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(col::text);
    canvas.drawString(line1, SCREEN_W / 2, 60);
    canvas.drawString(line2, SCREEN_W / 2, 80);
    pushScreen();
}

// Scan -> pick -> password -> connect. Returns true when connected.
static bool addNetwork() {
    message("Scanning...", "");
    int n = WiFi.scanNetworks();
    std::vector<String> items, ssids;
    std::vector<bool> secure;
    for (int i = 0; i < n; i++) {
        String name = WiFi.SSID(i);
        if (name.isEmpty()) continue;                // hidden network
        bool dup = false;                            // mesh points repeat names
        for (auto& s : ssids) if (s == name) dup = true;
        if (dup) continue;
        bool lock = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        int r = WiFi.RSSI(i);   // signal strength in dBm: -50 is great, -80 is weak
        String bars = r > -60 ? "|||" : r > -70 ? "|| " : "|  ";
        items.push_back(bars + "  " + name + (lock ? "" : "  (open)"));
        ssids.push_back(name);
        secure.push_back(lock);
    }
    WiFi.scanDelete();
    if (items.empty()) { toast("No networks found", col::warn); return false; }

    int selected = 0, top = 0;
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        if (in.back) return false;
        if (in.up && selected > 0) selected--;
        if (in.down && selected < (int)items.size() - 1) selected++;
        if (in.ch == 'r') return addNetwork();       // rescan
        if (in.enter) {
            String ssid = ssids[selected], pass;
            // Pre-fill the password if we know this network already.
            for (auto& s : loadSaved()) if (s.ssid == ssid) pass = s.pass;
            if (secure[selected] &&
                !textPrompt("Wi-Fi", ("Password for " + ssid).c_str(), pass, true)) continue;
            if (connectWithProgress(ssid, pass)) {
                rememberNetwork(ssid, pass);
                toast("Connected & saved!", col::good);
                return true;
            }
            toast("Could not connect", col::bad, 2000);
            wifiAutoConnect();                       // back to a known network
        }
        const int rows = 5;
        if (selected < top) top = selected;
        if (selected >= top + rows) top = selected - rows + 1;
        drawHeader("Choose network");
        drawList(items, selected, top, rows);
        drawHints({{ARROWS_UD, "move"}, {"OK", "join"}, {"R", "rescan"}, {"ESC", "back"}});
        pushScreen();
        delay(15);
    }
}

// List of remembered networks: pick one, then Connect or Forget.
static void savedNetworks() {
    int sel = 0;
    while (true) {
        auto list = loadSaved();
        if (list.empty()) { toast("No saved networks", col::warn); return; }
        std::vector<String> items;
        for (auto& s : list)
            items.push_back((WiFi.isConnected() && WiFi.SSID() == s.ssid ? "* " : "  ") + s.ssid);
        sel = pickFromList("Saved networks", items, sel);
        if (sel < 0) return;
        const Saved net = list[sel];
        int action = pickFromList(net.ssid.c_str(), {"Connect", "Forget this network"});
        if (action == 0) {
            if (connectWithProgress(net.ssid, net.pass)) {
                rememberNetwork(net.ssid, net.pass);   // move to the top
                toast("Connected!", col::good);
                return;
            }
            toast("Could not connect", col::bad, 2000);
        } else if (action == 1 && confirm(("Forget " + net.ssid + "?").c_str())) {
            forgetNetwork(net.ssid);
            if (WiFi.SSID() == net.ssid) WiFi.disconnect();
            toast("Forgotten", col::warn);
            sel = 0;
        }
    }
}

// ---- main Wi-Fi screen --------------------------------------------------------
bool appWifi() {
    // First time (nothing saved): go straight to choosing a network.
    if (!wifiHasSaved()) return addNetwork();

    int selected = 0;
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        const int OPTIONS = 3;
        if (in.back) return WiFi.isConnected();
        if (in.up) selected = (selected + OPTIONS - 1) % OPTIONS;
        if (in.down) selected = (selected + 1) % OPTIONS;
        if (in.enter) {
            if (selected == 0) addNetwork();
            else if (selected == 1) savedNetworks();
            else { WiFi.disconnect(); toast("Disconnected", col::warn); }
        }

        drawHeader("Wi-Fi");
        // Status card.
        canvas.fillRoundRect(4, HEADER_H + 4, SCREEN_W - 8, 44, 5, col::panel);
        canvas.setFont(&fonts::Font2);
        canvas.setTextDatum(top_left);
        if (WiFi.isConnected()) {
            int rssi = WiFi.RSSI();
            canvas.setTextColor(col::good);
            canvas.drawString("Connected: " + WiFi.SSID(), 10, HEADER_H + 7);
            canvas.setTextColor(col::dim);
            drawTextFit(WiFi.localIP().toString() + "  signal " +
                        (rssi > -60 ? "great" : rssi > -70 ? "good" : "weak") +
                        " (" + String(rssi) + ")", 10, HEADER_H + 26, SCREEN_W - 24, &fonts::Font2, &fonts::Font0);
        } else {
            canvas.setTextColor(col::warn);
            canvas.drawString("Not connected", 10, HEADER_H + 7);
            canvas.setTextColor(col::dim);
            canvas.drawString(String(loadSaved().size()) + " saved network(s)", 10, HEADER_H + 26);
        }
        // Options.
        const char* opts[] = {"Connect to a network", "Saved networks", "Disconnect"};
        for (int i = 0; i < 3; i++) {
            int y = HEADER_H + 54 + i * 18;
            if (i == selected) canvas.fillRoundRect(2, y, SCREEN_W - 4, 17, 4, col::accent);
            canvas.setTextColor(i == selected ? col::onAccent : col::text);
            canvas.setTextDatum(middle_left);
            canvas.drawString(opts[i], 10, y + 9);
        }
        drawHints({{ARROWS_UD, "move"}, {"OK", "select"}, {"ESC", "back"}});
        pushScreen();
        delay(30);
    }
}
