// settings_sd.cpp - copy milliWorks settings to and from the SD card, to move them to
// another Cardputer:  /milliWorks/settings/settings.json  (readable, editable on a computer).
//
// The Cardputer's internal memory stays the main store (always there, safe against power
// cuts); the card file is a copy. Wi-Fi passwords are only included if you say so, and the
// Claude API key never leaves the device.
#include <Preferences.h>
#include <SD.h>
#include <map>
#include <time.h>
#include "ui.h"
#include "apps.h"
#include "config.h"
#include "sound.h"
#include "status.h"
#include "settings_json.h"

bool sdMount();                                     // app_sd.cpp
void sdMakeDirs(const char* path);
void soundReloadSettings();                         // sound.cpp
void uiReloadSettings();                            // ui.cpp

static const char* FILE_PATH = SETTINGS_DIR "/settings.json";
static const char* TMP_PATH = SETTINGS_DIR "/settings.tmp";

enum Kind { K_INT, K_BOOL, K_STR };
struct Key { const char* name; Kind kind; };
// Everything worth carrying to another Cardputer. Not here on purpose: Wi-Fi passwords
// (optional, below), the time, the Wi-Fi access point shortcut, today's Flow totals.
static const Key KEYS[] = {
    {"theme", K_INT}, {"bright", K_INT}, {"flip", K_BOOL},
    {"snd_on", K_BOOL}, {"snd_vol", K_INT},
    {"tz", K_INT}, {"h24", K_BOOL},
    {"fl_int", K_STR}, {"fl_len", K_INT}, {"fl_snd", K_INT}, {"fl_brk", K_INT},
    {"br_pat", K_INT}, {"br_len", K_INT}, {"br_gd", K_INT}, {"br_cnt", K_BOOL}, {"br_mus", K_BOOL}, {"voice", K_STR},
    {"s0", K_STR}, {"s1", K_STR}, {"s2", K_STR}, {"s3", K_STR}, {"s4", K_STR},
};

bool settingsFileOnCard() { return sdMount() && SD.exists(FILE_PATH); }

static bool writeFile(bool withPasswords) {
    if (!sdMount()) return false;
    sdMakeDirs(SETTINGS_DIR);
    char when[24] = "unknown";
    time_t now = time(nullptr);
    if (now > 1700000000) { struct tm t; localtime_r(&now, &t); strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &t); }
    // The text is built with settings_json.h, the same code the Mac tests use.
    std::string body = std::string("{\n  \"app\": \"") + jsonEscape(FW_NAME) + "\"";
    body += jsonText("version", FW_VERSION);
    body += jsonText("device", deviceName());
    body += jsonText("saved", when);
    body += jsonBool("wifi_passwords", withPasswords);
    Preferences p;
    p.begin(PREFS_NS, true);
    auto put = [&](const char* k, Kind kind) {
        if (!p.isKey(k)) return;
        if (kind == K_INT) body += jsonInt(k, p.getInt(k, 0));
        else if (kind == K_BOOL) body += jsonBool(k, p.getBool(k, false));
        else body += jsonText(k, p.getString(k, "").c_str());
    };
    for (auto& k : KEYS) put(k.name, k.kind);
    if (withPasswords) { const char* pw[] = {"p0", "p1", "p2", "p3", "p4"}; for (auto k : pw) put(k, K_STR); }
    p.end();
    body += "\n}\n";
    File f = SD.open(TMP_PATH, FILE_WRITE);
    if (!f) return false;
    size_t written = f.write((const uint8_t*)body.data(), body.size());
    f.close();
    if (written != body.size()) { SD.remove(TMP_PATH); return false; }   // card full: keep the old file
    // Swap the new file in only once it's complete: a power cut can't leave half a file.
    SD.remove(FILE_PATH);
    return SD.rename(TMP_PATH, FILE_PATH);
}

// Reads the file with settings_json.h (shared with the Mac tests). Strings keep a leading
// '"' marker in the map so they can't be mistaken for numbers or true/false.
static bool readFile(std::map<String, String>& out) {
    if (!settingsFileOnCard()) return false;
    File f = SD.open(FILE_PATH);
    if (!f) return false;
    std::string text;
    while (f.available()) { char buf[256]; int n = f.read((uint8_t*)buf, sizeof(buf)); if (n <= 0) break; text.append(buf, n); }
    f.close();
    std::map<std::string, std::string> m;
    if (!parseFlatJson(text, m)) return false;
    for (auto& kv : m) out[String(kv.first.c_str())] = String(kv.second.c_str());
    return out.count("app") && out["app"] == String("\"") + FW_NAME;
}

static void apply(std::map<String, String>& m) {
    Preferences p;
    p.begin(PREFS_NS, false);
    // Keep this device's Wi-Fi passwords for networks it already knows, if the file has none.
    String oldPw[5], oldSsid[5];
    for (int i = 0; i < 5; i++) { oldSsid[i] = p.getString(("s" + String(i)).c_str(), ""); oldPw[i] = p.getString(("p" + String(i)).c_str(), ""); }
    for (auto& k : KEYS) {
        if (!m.count(k.name)) { if (k.name[0] == 's' && isdigit((unsigned char)k.name[1])) p.remove(k.name); continue; }
        const String& v = m[k.name];
        if (k.kind == K_INT) p.putInt(k.name, v.toInt());
        else if (k.kind == K_BOOL) p.putBool(k.name, v == "true");
        else p.putString(k.name, v.startsWith("\"") ? v.substring(1) : v);
    }
    for (int i = 0; i < 5; i++) {
        String pk = "p" + String(i), sk = "s" + String(i);
        if (m.count(pk)) { p.putString(pk.c_str(), m[pk].substring(1)); continue; }
        String ssid = m.count(sk) ? m[sk].substring(1) : "";
        String keep;
        for (int j = 0; j < 5; j++) if (ssid.length() && oldSsid[j] == ssid) keep = oldPw[j];
        if (ssid.length()) p.putString(pk.c_str(), keep); else p.remove(pk.c_str());
    }
    p.putBool("onboarded", true);
    p.end();
    // Use the new settings straight away.
    uiReloadSettings();
    soundReloadSettings();
    clockApplySavedTimeZone();
    wifiAutoConnect();
}

static String val(std::map<String, String>& m, const char* k) {
    if (!m.count(k)) return "-";
    String v = m[k];
    return v.startsWith("\"") ? v.substring(1) : v;
}

// ---- screens ------------------------------------------------------------------------
void settingsSaveToCard() {
    if (!sdMount()) { toast("No SD card found", col::bad, 1500); return; }
    bool pw = confirm("Include Wi-Fi passwords?");      // OK = yes, ESC = no (the safe default)
    if (writeFile(pw)) toast("Saved to the SD card", col::good, 1400);
    else toast("Could not write the card", col::bad, 1600);
}

// Shows what's in the file and asks first. Returns true if the settings were loaded.
bool settingsLoadFromCard(bool firstStart) {
    std::map<String, String> m;
    if (!readFile(m)) { if (!firstStart) toast("No settings file on the card", col::warn, 1500); return false; }
    int theme = constrain(val(m, "theme").toInt(), 0, THEME_COUNT - 1);
    int zone = val(m, "tz").toInt();
    String nets;
    for (int i = 0; i < 5; i++) { String s = val(m, ("s" + String(i)).c_str()); if (s != "-") nets += (nets.length() ? ", " : "") + s; }
    bool pw = val(m, "wifi_passwords") == "true";
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        if (in.back) return false;
        if (in.enter) break;
        drawHeader(firstStart ? "Your settings" : "Load settings");
        canvas.setFont(&fonts::Font0);
        canvas.setTextDatum(middle_left);
        canvas.setTextColor(col::dim);
        canvas.drawString("Found on the SD card:", 6, 24);
        String rows[][2] = {{"Saved", val(m, "saved")}, {"From", val(m, "device") + ", " + val(m, "app") + " v" + val(m, "version")},
                            {"Theme", THEMES[theme].name}, {"Time zone", m.count("tz") ? clockZoneName(zone) : "-"},
                            {"Wi-Fi", (nets.length() ? nets : String("none")) + (pw ? "" : "  (no passwords)")}};
        for (int i = 0; i < 5; i++) {
            canvas.setTextColor(col::dim);
            canvas.drawString(rows[i][0], 6, 38 + i * 13);
            canvas.setTextColor(col::text);
            drawTextFit(rows[i][1], 66, 38 + i * 13, SCREEN_W - 72, &fonts::Font0);
        }
        canvas.setTextColor(col::warn);
        canvas.drawString(firstStart ? "OK uses them and skips the welcome tour." : "Replaces this Cardputer's settings.", 6, 108);
        drawHints({{"OK", firstStart ? "use them" : "load"}, {"ESC", firstStart ? "start fresh" : "cancel"}});
        pushScreen();
        delay(20);
    }
    apply(m);
    toast("Settings loaded", col::good, 1200);
    return true;
}
