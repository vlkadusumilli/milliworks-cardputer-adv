// app_clock.cpp - a big clock using internet time (NTP), with time zones,
// 12/24 hour format and setting the time by hand.
//
// The Cardputer has no battery-backed clock chip, so it asks a time server on
// the internet once Wi-Fi is connected. Without internet you can set it by hand
// (it's lost at power-off and replaced by internet time when Wi-Fi comes back).
#include <WiFi.h>
#include <Preferences.h>
#include <sys/time.h>
#include <time.h>
#include <esp_sntp.h>
#include "status.h"
#include "ui.h"
#include "apps.h"
#include "config.h"

// Time zones as POSIX TZ strings, including daylight-saving rules.
struct Zone { const char* name; const char* tz; };
static const Zone ZONES[] = {
    {"US Pacific (Los Angeles)",  "PST8PDT,M3.2.0,M11.1.0"},
    {"US Mountain (Denver)",      "MST7MDT,M3.2.0,M11.1.0"},
    {"US Arizona (Phoenix)",      "MST7"},
    {"US Central (Chicago)",      "CST6CDT,M3.2.0,M11.1.0"},
    {"US Eastern (New York)",     "EST5EDT,M3.2.0,M11.1.0"},
    {"Alaska (Anchorage)",        "AKST9AKDT,M3.2.0,M11.1.0"},
    {"Hawaii (Honolulu)",         "HST10"},
    {"UTC",                       "UTC0"},
    {"UK (London)",               "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Central Europe (Berlin)",   "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"India (Kolkata)",           "IST-5:30"},
    {"China (Shanghai)",          "CST-8"},
    {"Japan (Tokyo)",             "JST-9"},
    {"Australia East (Sydney)",   "AEST-10AEDT,M10.1.0,M4.1.0/3"},
};
static const int ZONE_COUNT = sizeof(ZONES) / sizeof(ZONES[0]);
static const int DEFAULT_ZONE = 3;   // US Central

static int savedZone() {
    Preferences p;
    p.begin(PREFS_NS, true);
    int z = p.getInt("tz", DEFAULT_ZONE);
    p.end();
    return (z >= 0 && z < ZONE_COUNT) ? z : DEFAULT_ZONE;
}

// ---- knowing the time ------------------------------------------------------------
// "verified" = the time came from the internet, from you (set by hand), or survived a
// restart (the chip's timer keeps running through a reset, just not through power-off).
// Otherwise we may show the LAST KNOWN time, saved in flash, dimmed until it's verified.
static volatile bool sntpSynced = false;
static volatile bool rtcNeedsWrite = false;
static bool verified = false;
static uint32_t lastSave = 0;

// ---- RTC module (optional, e.g. M5Stack Unit RTC on the Grove port) -------------
// A clock chip with its own coin cell keeps time while the Cardputer is off, so the
// time is right the instant it starts. We keep it in UTC; the time zone is applied on top.
static bool rtcValid = false;

static time_t utcEpoch(int y, int mo, int d, int h, int mi, int s) {   // calendar -> seconds, no time zone
    y -= mo <= 2;
    int era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400;
    int doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long days = era * 146097L + doe - 719468L;
    return (time_t)days * 86400 + h * 3600 + mi * 60 + s;
}

static bool readRtc() {
    if (!M5.Rtc.isEnabled() || M5.Rtc.getVoltLow()) return false;   // no module, or its battery ran out
    m5::rtc_datetime_t dt;
    if (!M5.Rtc.getDateTime(&dt) || dt.date.year < 2024 || dt.date.year > 2099) return false;
    struct timeval tv = {utcEpoch(dt.date.year, dt.date.month, dt.date.date, dt.time.hours, dt.time.minutes, dt.time.seconds), 0};
    settimeofday(&tv, nullptr);
    return true;
}

static void writeRtc() {
    if (!M5.Rtc.isEnabled()) return;
    time_t now = time(nullptr);
    struct tm utc;
    gmtime_r(&now, &utc);
    M5.Rtc.setDateTime(&utc);
}

bool clockHasRtc() { return M5.Rtc.isEnabled(); }

static void saveLastKnownTime() {
    Preferences p;
    p.begin(PREFS_NS, false);
    p.putULong64("last_time", (uint64_t)time(nullptr));
    p.end();
    lastSave = millis();
}

void clockBoot() {
    if (readRtc()) { rtcValid = verified = true; return; }          // RTC module: exact time, instantly
    if (time(nullptr) > 1700000000) { verified = true; return; }   // survived a restart
    Preferences p;
    p.begin(PREFS_NS, true);
    uint64_t last = p.getULong64("last_time", 0);
    p.end();
    if (last > 1700000000) {                 // show the last known time until the internet answers
        struct timeval tv = {(time_t)last, 0};
        settimeofday(&tv, nullptr);
    }
}

bool clockIsVerified() { return verified || sntpSynced; }

// Called often from every screen (via readInput): keeps Wi-Fi going and saves the
// time now and then - every 10 minutes, so the flash isn't worn out.
void serviceTick() {
    wifiService();
    statusService();                                                  // status LED
    if (sntpSynced && !verified) { verified = true; saveLastKnownTime(); }
    if (rtcNeedsWrite) { rtcNeedsWrite = false; writeRtc(); }      // keep the RTC module exact
    if (verified && millis() - lastSave > 10UL * 60 * 1000) saveLastKnownTime();
}

void clockApplySavedTimeZone() {
    // configTzTime sets the zone AND starts syncing with the internet time server.
    configTzTime(ZONES[savedZone()].tz, NTP_SERVER, "time.google.com", "time.cloudflare.com");
    sntp_set_time_sync_notification_cb([](struct timeval*) { sntpSynced = true; rtcNeedsWrite = true; });
}

bool clockIsSet() { return time(nullptr) > 1700000000; }   // before sync it reads 1970

int clockZoneCount() { return ZONE_COUNT; }
const char* clockZoneName(int i) { return ZONES[constrain(i, 0, ZONE_COUNT - 1)].name; }
int clockSavedZone() { return savedZone(); }
void clockSetZone(int i) {
    Preferences p;
    p.begin(PREFS_NS, false);
    p.putInt("tz", constrain(i, 0, ZONE_COUNT - 1));
    p.end();
    clockApplySavedTimeZone();
}

// Pick a time zone from a list.
static void chooseZone() {
    std::vector<String> items;
    for (auto& z : ZONES) items.push_back(z.name);
    int pick = pickFromList("Time zone", items, savedZone());
    if (pick < 0) return;
    Preferences p;
    p.begin(PREFS_NS, false);
    p.putInt("tz", pick);
    p.end();
    clockApplySavedTimeZone();
    toast(ZONES[pick].name, col::good, 900);
}

static int daysInMonth(int year, int month) {      // month 1..12
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return month == 2 && leap ? 29 : d[month - 1];
}

// Set date and time by hand: up/down changes the highlighted part,
// left/right moves between year, month, day, hour and minute.
static void setManually() {
    int v[5] = {2026, 1, 1, 12, 0};                  // year, month, day, hour, minute
    if (clockIsSet()) {
        time_t now = time(nullptr);
        struct tm t;
        localtime_r(&now, &t);
        v[0] = t.tm_year + 1900; v[1] = t.tm_mon + 1; v[2] = t.tm_mday;
        v[3] = t.tm_hour; v[4] = t.tm_min;
    }
    const int lo[5] = {2024, 1, 1, 0, 0};
    const char* names[5] = {"year", "month", "day", "hour", "minute"};
    int field = 3;                                   // start on the hour
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        if (in.back) { if (clockIsVerified()) saveLastKnownTime(); return; }
        if (in.left) field = (field + 4) % 5;
        if (in.right) field = (field + 1) % 5;
        int hi[5] = {2099, 12, daysInMonth(v[0], v[1]), 23, 59};
        int span = hi[field] - lo[field] + 1;
        if (in.up) v[field] = lo[field] + (v[field] - lo[field] + 1) % span;
        if (in.down) v[field] = lo[field] + (v[field] - lo[field] + span - 1) % span;
        v[2] = min(v[2], daysInMonth(v[0], v[1]));  // e.g. 31 Jan -> Feb
        if (in.enter) {
            struct tm t = {};
            t.tm_year = v[0] - 1900; t.tm_mon = v[1] - 1; t.tm_mday = v[2];
            t.tm_hour = v[3]; t.tm_min = v[4];
            t.tm_isdst = -1;                         // let the time zone decide summer time
            struct timeval tv = {mktime(&t), 0};
            settimeofday(&tv, nullptr);
            verified = true;                 // you set it: trust it
            saveLastKnownTime();
            writeRtc();
            toast("Time set", col::good, 900);
            return;
        }

        drawHeader("Set time");
        // Draw "2026-11-26  14:05" part by part so the chosen part can be highlighted.
        char parts[5][6];
        snprintf(parts[0], 6, "%04d", v[0]);
        for (int i = 1; i < 5; i++) snprintf(parts[i], 6, "%02d", v[i]);
        const char* seps[5] = {"-", "-", "  ", ":", ""};
        canvas.setFont(&fonts::Font4);
        canvas.setTextDatum(middle_left);
        int total = 0;
        for (int i = 0; i < 5; i++) total += canvas.textWidth(parts[i]) + canvas.textWidth(seps[i]);
        int x = (SCREEN_W - total) / 2, y = 62;
        for (int i = 0; i < 5; i++) {
            int w = canvas.textWidth(parts[i]);
            if (i == field) canvas.fillRoundRect(x - 2, y - 15, w + 4, 30, 4, col::accent);
            canvas.setTextColor(i == field ? col::onAccent : col::text);
            canvas.drawString(parts[i], x, y);
            x += w;
            canvas.setTextColor(col::dim);
            canvas.drawString(seps[i], x, y);
            x += canvas.textWidth(seps[i]);
        }
        canvas.setFont(&fonts::Font2);
        canvas.setTextDatum(middle_center);
        canvas.setTextColor(col::dim);
        canvas.drawString(String("changing the ") + names[field], SCREEN_W / 2, 96);
        drawHints({{ARROWS_UD, "change"}, {ARROWS_LR, "next"}, {"OK", "save"}, {"ESC", "cancel"}});
        pushScreen();
        delay(15);
    }
}

// Settings list behind the OK key - everything the letter shortcuts do, but visible.
static void clockSettings(bool& h24, int& brightness) {
    int sel = 0;
    while (true) {
        std::vector<String> items = {
            String("Time zone: ") + ZONES[savedZone()].name,
            String("Format: ") + (h24 ? "24-hour" : "12-hour"),
            "Set time by hand",
            String("Brightness: ") + String(brightness * 100 / 255) + "%",
            String("Wi-Fi: ") + (WiFi.isConnected() ? WiFi.SSID() : String("not connected")),
        };
        sel = pickFromList("Clock settings", items, sel);
        if (sel < 0) return;
        if (sel == 0) chooseZone();
        if (sel == 1) {
            h24 = !h24;
            Preferences p;
            p.begin(PREFS_NS, false);
            p.putBool("h24", h24);
            p.end();
        }
        if (sel == 2) setManually();
        if (sel == 3) {
            brightness = nextBrightness();
        }
        if (sel == 4) appWifi();
    }
}

void appClock() {
    Preferences p;
    p.begin(PREFS_NS, true);
    bool h24 = p.getBool("h24", false);
    p.end();
    int brightness = screenBrightness();

    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        if (in.back) return;
        if (in.ch == 't') {                       // toggle 12/24 hour and remember it
            h24 = !h24;
            p.begin(PREFS_NS, false);
            p.putBool("h24", h24);
            p.end();
        }
        if (in.enter) clockSettings(h24, brightness);
        if (in.ch == 'z') chooseZone();
        if (in.ch == 's') setManually();
        if (in.ch == 'w') appWifi();
        if (in.ch == 'b') {                       // cycle screen brightness
            brightness = nextBrightness();
        }

        drawHeader("Clock", false);   // the face shows the time; no small clock twice
        canvas.setFont(&fonts::Font0);                 // small time zone label
        canvas.setTextDatum(top_center);
        canvas.setTextColor(col::dim);
        canvas.drawString(ZONES[savedZone()].name, SCREEN_W / 2, HEADER_H + 4);
        time_t now = time(nullptr);
        bool synced = clockIsSet();                 // we have SOME time (maybe last-known)
        bool trusted = synced && clockIsVerified();
        struct tm t;
        localtime_r(&now, &t);

        char hm[8], sec[4], ampm[4] = "", date[32];
        if (synced) {
            strftime(hm, sizeof(hm), h24 ? "%H:%M" : "%I:%M", &t);
            if (!h24 && hm[0] == '0') memmove(hm, hm + 1, strlen(hm));   // "09:30" -> "9:30"
            strftime(sec, sizeof(sec), "%S", &t);
            if (!h24) strftime(ampm, sizeof(ampm), "%p", &t);
            strftime(date, sizeof(date), "%A, %b %d", &t);
        } else {
            strcpy(hm, "--:--");
            strcpy(sec, "--");
        }

        // Font7 is a 7-segment "digital clock" font, 48 px tall.
        canvas.setFont(&fonts::Font7);
        canvas.setTextColor(trusted ? col::text : col::dim);
        int hmW = canvas.textWidth(hm);
        int x = (SCREEN_W - hmW - 36) / 2;
        canvas.setTextDatum(middle_left);
        canvas.drawString(hm, x, 62);

        canvas.setFont(&fonts::Font4);            // 26 px font for seconds
        canvas.setTextColor(trusted ? col::accent : col::dim);
        canvas.drawString(sec, x + hmW + 6, 72);
        canvas.setFont(&fonts::Font2);
        canvas.setTextColor(col::dim);
        canvas.drawString(ampm, x + hmW + 8, 46);

        // Seconds progress bar.
        if (trusted) canvas.fillRect(0, 92, SCREEN_W * t.tm_sec / 59, 2, col::accent);

        canvas.setFont(&fonts::Font2);
        canvas.setTextDatum(middle_center);
        canvas.setTextColor(col::text);
        if (trusted) {
            canvas.drawString(date, SCREEN_W / 2, 107);
        } else if (synced) {
            canvas.setTextColor(col::warn);
            canvas.drawString(wifiHasSaved() ? "Last known time - updating..." : "Last known time: OK > Set time",
                              SCREEN_W / 2, 107);
        } else if (WiFi.isConnected()) {
            canvas.drawString("Getting time from internet...", SCREEN_W / 2, 107);
        } else {
            canvas.setTextColor(col::warn);
            canvas.drawString(wifiHasSaved() ? "No Wi-Fi: OK > Set time by hand" : "No Wi-Fi: OK > Wi-Fi to set up",
                              SCREEN_W / 2, 107);
        }
        drawHints({{"OK", "settings"}, {"ESC", "menu"}});
        pushScreen();
        delay(50);
    }
}
