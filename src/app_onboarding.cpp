// app_onboarding.cpp - the first-start tour: welcome, keys, look, sounds, Wi-Fi,
// time zone, done. Shown once; Settings > "Show welcome again" replays it.
// ESC always goes one step back. Matches the screens in design/cardputer-preview.html.
#include <Preferences.h>
#include <WiFi.h>
#include "ui.h"
#include "apps.h"
#include "config.h"
#include "sound.h"

static const int STEPS = 7;

bool onboardingDone() {
    Preferences p;
    p.begin(PREFS_NS, true);
    bool done = p.getBool("onboarded", false);
    p.end();
    return done;
}

static void markDone() {
    Preferences p;
    p.begin(PREFS_NS, false);
    p.putBool("onboarded", true);
    p.end();
}

// Progress dots just above the key hints; the current step is a copper pill.
static void dots(int step) {
    const int y = 119, gap = 8;
    int x = SCREEN_W / 2 - ((STEPS - 1) * gap + 8) / 2;
    for (int i = 0; i < STEPS; i++) {
        if (i == step) { canvas.fillRoundRect(x - 3, y - 2, 12, 4, 2, col::accent); x += 14; }
        else { canvas.fillRoundRect(x, y - 2, 4, 4, 2, i < step ? col::dim : col::edge); x += gap; }
    }
}

static void centerText(const char* s, int y, const lgfx::IFont* font, uint16_t color) {
    canvas.setFont(font);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(color);
    canvas.drawString(s, SCREEN_W / 2, y);
}

// A short list with an optional line above it. Returns the chosen row, or -1 for ESC.
// onMove (optional) runs whenever the highlighted row changes.
static int stepList(int step, const char* title, const char* intro, const std::vector<String>& items,
                    int sel, void (*onMove)(int) = nullptr, bool swatches = false) {
    int top = 0;
    const int listTop = intro ? 46 : 20, rows = intro ? 3 : 5;
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        if (in.back) return -1;
        int before = sel;
        if (in.up) sel = (sel + items.size() - 1) % items.size();
        if (in.down) sel = (sel + 1) % items.size();
        if (sel != before && onMove) onMove(sel);
        if (in.enter) return sel;

        if (sel < top) top = sel;
        if (sel >= top + rows) top = sel - rows + 1;
        drawHeader(title);
        if (intro) centerText(intro, 30, &fonts::Font2, col::dim);
        for (int r = 0; r < rows && top + r < (int)items.size(); r++) {
            int i = top + r, y = listTop + r * 18;
            if (i == sel) canvas.fillRoundRect(2, y, SCREEN_W - 4, 17, 4, col::accent);
            canvas.setFont(&fonts::Font2);
            canvas.setTextDatum(middle_left);
            canvas.setTextColor(i == sel ? col::onAccent : col::text);
            canvas.drawString(items[i], 8, y + 9);
            if (swatches) {                               // each theme's own colours
                const uint16_t* c = THEMES[i].c;
                const int idx[] = {0, 1, 5, 7, 9};
                for (int k = 0; k < 5; k++) {
                    int x = SCREEN_W - 70 + k * 12;
                    canvas.fillRect(x, y + 4, 10, 9, c[idx[k]]);
                    canvas.drawRect(x, y + 4, 10, 9, col::edge);
                }
            }
        }
        dots(step);
        drawHints({{ARROWS_UD, swatches ? "try" : "choose"}, {"OK", "next"}, {"ESC", "back"}});
        pushScreen();
        delay(15);
    }
}

// ---- the steps: each returns +1 (next) or -1 (back) --------------------------------
static int stepWelcome() {
    playSfx(SFX_STARTUP);
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        if (in.enter) return +1;
        canvas.fillScreen(col::bg);
        centerText("Welcome to", 30, &fonts::Font2, col::dim);
        centerText(FW_NAME, 54, &fonts::Font4, col::text);
        centerText((String("for ") + deviceName()).c_str(), 78, &fonts::Font2, col::accent);
        dots(0);
        drawHints({{"OK", "start"}});
        pushScreen();
        delay(15);
    }
}

static void keyCap(int x, int y, int w, int h, const char* k, const char* sub) {
    canvas.fillRoundRect(x, y, w, h, 3, col::panel);
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(col::text);
    canvas.drawString(k, x + w / 2, sub ? y + 8 : y + h / 2);
    if (sub && sub[0] >= 0x18 && sub[0] <= 0x1b) {      // an arrow: the small font has none, so draw it
        int cx = x + w / 2, cy = y + 18;
        switch (sub[0]) {
            case 0x18: canvas.fillTriangle(cx - 3, cy + 2, cx + 3, cy + 2, cx, cy - 2, col::accent); break;
            case 0x19: canvas.fillTriangle(cx - 3, cy - 2, cx + 3, cy - 2, cx, cy + 2, col::accent); break;
            case 0x1a: canvas.fillTriangle(cx - 2, cy - 3, cx - 2, cy + 3, cx + 2, cy, col::accent); break;
            case 0x1b: canvas.fillTriangle(cx + 2, cy - 3, cx + 2, cy + 3, cx - 2, cy, col::accent); break;
        }
    } else if (sub) {
        canvas.setFont(&fonts::Font0);
        canvas.setTextColor(col::accent);
        canvas.drawString(sub, x + w / 2, y + 19);
    }
}

static int stepKeys() {
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        if (in.back) return -1;
        if (in.down || in.enter) return +1;          // "press the down arrow": you've now used it once
        drawHeader("Your keys");
        keyCap(10, 22, 20, 24, ";", "\x18");
        keyCap(34, 22, 20, 24, ".", "\x19");
        keyCap(58, 22, 20, 24, ",", "\x1b");
        keyCap(82, 22, 20, 24, "/", "\x1a");
        canvas.setFont(&fonts::Font2);
        canvas.setTextDatum(middle_left);
        canvas.setTextColor(col::dim);
        canvas.drawString("move", 110, 34);
        keyCap(10, 52, 44, 18, "Enter", nullptr);
        canvas.setTextDatum(middle_left);
        canvas.setTextColor(col::dim);
        canvas.drawString("open, choose", 62, 61);
        keyCap(10, 74, 24, 24, "`", "esc");
        canvas.setFont(&fonts::Font0);
        canvas.setTextDatum(middle_left);
        canvas.setTextColor(col::dim);
        canvas.drawString("or", 38, 86);
        keyCap(52, 77, 28, 18, "Del", nullptr);
        canvas.setFont(&fonts::Font2);
        canvas.setTextDatum(middle_left);
        canvas.setTextColor(col::dim);
        canvas.drawString("back", 86, 86);
        canvas.fillRoundRect(150, 46, 82, 46, 6, col::accent);
        canvas.setTextDatum(middle_center);
        canvas.setTextColor(col::onAccent);
        canvas.drawString("Try it:", 191, 58);
        canvas.drawString("press .", 191, 76);
        canvas.fillTriangle(186, 84, 196, 84, 191, 89, col::onAccent);   // a small "down" arrow
        dots(1);
        drawHints({{"\x19", "continue"}, {"ESC", "back"}});
        pushScreen();
        delay(15);
    }
}

static int stepTheme() {
    const int before = savedTheme();
    std::vector<String> names;
    for (int i = 0; i < THEME_COUNT; i++) names.push_back(THEMES[i].name);
    int pick = stepList(2, "Pick a look", nullptr, names, before,
                        [](int i) { applyTheme(i); playSfx(SFX_SELECT); }, true);
    if (pick < 0) { applyTheme(before); return -1; }
    saveTheme(pick);
    applyTheme(pick);
    return +1;
}

static int stepSounds() {
    int pick = stepList(3, "Sounds", "Little sounds for keys and results.",
                        {String("On  -  ") + soundPackName(), "Off"}, soundsOn() ? 0 : 1,
                        [](int i) { if (i == 0) { setSoundsOn(true); playSfx(SFX_SUCCESS); } });
    if (pick < 0) return -1;
    setSoundsOn(pick == 0);
    return +1;
}

static int stepWifi() {
    int pick = stepList(4, "Wi-Fi", "Wi-Fi sets the clock for you.",
                        {WiFi.isConnected() ? String("Connected: ") + WiFi.SSID() : String("Choose a network"), "Skip for now"}, 0);
    if (pick < 0) return -1;
    if (pick == 0 && !WiFi.isConnected()) appWifi();   // the normal Wi-Fi setup; back here afterwards
    return +1;
}

static int stepZone() {
    std::vector<String> zones;
    for (int i = 0; i < clockZoneCount(); i++) zones.push_back(clockZoneName(i));
    int pick = stepList(5, "Time zone", nullptr, zones, clockSavedZone());
    if (pick < 0) return -1;
    clockSetZone(pick);
    return +1;
}

static int stepDone() {
    playSfx(SFX_SUCCESS);
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        if (in.back) return -1;
        if (in.enter) return +1;
        canvas.fillScreen(col::bg);
        // A green tick.
        canvas.drawWideLine(108, 30, 116, 38, 1.5f, col::good);
        canvas.drawWideLine(116, 38, 132, 22, 1.5f, col::good);
        centerText("All set", 54, &fonts::Font4, col::text);
        centerText("ESC from the Clock opens your apps.", 76, &fonts::Font2, col::dim);
        centerText("Theme, sounds and more: Settings.", 92, &fonts::Font2, col::dim);
        dots(6);
        drawHints({{"OK", "open the Clock"}});
        pushScreen();
        delay(15);
    }
}

void runOnboarding() {
    int (*steps[STEPS])() = {stepWelcome, stepKeys, stepTheme, stepSounds, stepWifi, stepZone, stepDone};
    int i = 0;
    while (i < STEPS) {
        int move = steps[i]();
        i = max(0, i + move);              // ESC on the first screen just stays there
    }
    markDone();
}
