// main.cpp - starts the hardware and shows the home screen: a grid of app icons.
//
// Arduino programs have two functions:
//   setup() runs once at power-on,
//   loop()  runs over and over forever.
#include <M5Cardputer.h>
#include "ui.h"
#include "apps.h"
#include "config.h"
#include "sound.h"
#include "status.h"

bool sdMount();     // app_sd.cpp

enum Glyph : uint8_t { G_WIFI, G_CLOCK, G_SD, G_GEAR, G_REMOTE, G_FOCUS, G_MUSIC, G_STATUS, G_BREATHE };

struct App {
    const char* name;
    const uint16_t* color;   // points at a col:: role, so it follows the chosen theme
    Glyph glyph;
    void (*run)();           // nullptr = coming soon
};

static const App APPS[] = {
    {"Wi-Fi",    &col::trace,  G_WIFI,   [] { appWifi(); }},
    {"Clock",    &col::accent, G_CLOCK,  appClock},
    {"SD Card",  &col::lilac,  G_SD,     appSd},
    {"Settings", &col::dim,    G_GEAR,   appSettings},
    {"Flow",     &col::bad,    G_FOCUS,   appFlow},
    {"Status",   &col::good,   G_STATUS,  appStatus},
    {"Breathe",  &col::trace,  G_BREATHE, appBreathe},
    // {"Music",    &col::warn,   G_MUSIC,   nullptr},   // hidden until the Music app is ready
};
static const int APP_COUNT = sizeof(APPS) / sizeof(APPS[0]);
static const int COLS = 4;
static int selected = 0;

void setup() {
    auto cfg = M5.config();
    cfg.external_rtc = true;        // use an RTC module on the Grove port if one is plugged in
    M5Cardputer.begin(cfg, true);   // true = also start the keyboard
    Serial.begin(115200);
    M5Cardputer.Speaker.setVolume(140);  // master volume; each sound sets its own level on top
    soundBegin();
    statusBegin();
    uiBegin();
    clockBoot();         // RTC module if plugged in; else the last known time until the internet answers
    wifiAutoConnect();   // Wi-Fi on; joins the strongest saved network (if any) in the background
    sdMount();           // SD card in? Make the /milliWorks folders (and tidy older ones) now.
                         // No card: nothing happens; apps try again when you open them.

    if (!onboardingDone()) {
        // Very first start: settings saved from another Cardputer? Offer them; else the tour.
        if (!(settingsFileOnCard() && settingsLoadFromCard(true))) runOnboarding();
    } else {
        showSplash();
    }
    appClock();          // the Clock is the first screen; Back from it opens the home screen
    // Leaving the clock (Back) lands on the home screen below.
}

// Mixes colour a towards b (amount 0..1). Used to grey out apps that are coming soon.
static uint16_t blend565(uint16_t a, uint16_t b, float amount) {
    int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
    int br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
    int r = ar + (br - ar) * amount, g = ag + (bg - ag) * amount, bl = ab + (bb - ab) * amount;
    return (r << 11) | (g << 5) | bl;
}

// Simple line-art symbols, drawn in the ground colour on the icon's hue.
// Angles are in degrees: 0 = right, 90 = down, 270 = up.
static void drawGlyph(Glyph g, int cx, int cy, uint16_t ink) {
    auto line = [&](int x0, int y0, int x1, int y1) { canvas.drawWideLine(x0, y0, x1, y1, 1.0f, ink); };
    switch (g) {
        case G_WIFI:
            for (int r : {5, 10, 15}) canvas.fillArc(cx, cy + 6, r - 1, r + 1, 225, 315, ink);
            canvas.fillCircle(cx, cy + 6, 2, ink);
            break;
        case G_CLOCK:
            canvas.fillArc(cx, cy, 10, 12, 0, 360, ink);
            line(cx, cy, cx, cy - 7);
            line(cx, cy, cx + 5, cy + 2);
            break;
        case G_SD:
            line(cx - 8, cy - 12, cx + 4, cy - 12);
            line(cx + 4, cy - 12, cx + 9, cy - 7);
            line(cx + 9, cy - 7, cx + 9, cy + 12);
            line(cx + 9, cy + 12, cx - 8, cy + 12);
            line(cx - 8, cy + 12, cx - 8, cy - 12);
            for (int i = 0; i < 4; i++) line(cx - 4 + i * 3, cy - 8, cx - 4 + i * 3, cy - 3);
            break;
        case G_GEAR:
            canvas.fillArc(cx, cy, 6, 8, 0, 360, ink);
            canvas.fillCircle(cx, cy, 2, ink);
            for (int k = 0; k < 8; k++) {
                float a = k * PI / 4;
                line(cx + cos(a) * 8, cy + sin(a) * 8, cx + cos(a) * 12, cy + sin(a) * 12);
            }
            break;
        case G_REMOTE:
            canvas.drawRoundRect(cx - 6, cy - 8, 12, 21, 3, ink);
            canvas.drawRoundRect(cx - 5, cy - 7, 10, 19, 2, ink);
            canvas.fillCircle(cx, cy - 2, 2, ink);
            for (int dy : {4, 8}) { canvas.fillCircle(cx - 2, cy + dy, 1, ink); canvas.fillCircle(cx + 2, cy + dy, 1, ink); }
            canvas.fillArc(cx, cy - 10, 3, 5, 215, 325, ink);
            break;
        case G_FOCUS:
            canvas.fillArc(cx, cy + 1, 9, 11, 0, 360, ink);
            canvas.fillCircle(cx, cy + 1, 4, ink);
            line(cx, cy - 9, cx, cy - 13);
            break;
        case G_STATUS:                                  // a round "no entry" style badge
            canvas.fillArc(cx, cy, 10, 12, 0, 360, ink);
            canvas.fillRoundRect(cx - 6, cy - 2, 12, 4, 2, ink);
            break;
        case G_BREATHE:                                 // rings, like a breath spreading out
            canvas.fillArc(cx, cy, 11, 12, 0, 360, ink);
            canvas.fillArc(cx, cy, 6, 7, 0, 360, ink);
            canvas.fillCircle(cx, cy, 2, ink);
            break;
        case G_MUSIC:
            line(cx - 3, cy + 8, cx - 3, cy - 10);
            line(cx - 3, cy - 10, cx + 8, cy - 12);
            line(cx + 8, cy - 12, cx + 8, cy + 6);
            canvas.fillEllipse(cx - 6, cy + 8, 4, 3, ink);
            canvas.fillEllipse(cx + 5, cy + 6, 4, 3, ink);
            break;
    }
}

static void drawHome() {
    drawHeader("milliWorks");
    const int S = 34, R = 9;                       // icon size and corner radius
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(middle_center);
    for (int i = 0; i < APP_COUNT; i++) {
        const App& a = APPS[i];
        int cx = 30 + (i % COLS) * 60, x = cx - S / 2, y = 21 + (i / COLS) * 50;
        bool soon = a.run == nullptr, sel = i == selected;
        uint16_t hue = soon ? blend565(*a.color, col::bg, 0.65f) : *a.color;
        canvas.fillRoundRect(x, y, S, S, R, hue);
        drawGlyph(a.glyph, cx, y + S / 2, col::bg);
        if (sel) {                                 // copper ring: "you are here"
            canvas.drawRoundRect(x - 3, y - 3, S + 6, S + 6, R + 3, col::accent);
            canvas.drawRoundRect(x - 2, y - 2, S + 4, S + 4, R + 2, col::accent);
        }
        canvas.setTextColor(sel ? col::text : col::dim);
        canvas.drawString(a.name, cx, y + S + 8);
    }
    drawHints({{ARROWS_UD ARROWS_LR, "move"}, {"OK", "open"}, {"1-7", "jump"}, {"R", "flip"}});
    pushScreen();
}

static void open(int i) {
    if (APPS[i].run) APPS[i].run();               // runs until the app returns (Back key)
    else toast("Coming soon", col::warn, 900);
}

void loop() {
    M5Cardputer.update();   // reads keys and buttons - call this every loop
    Input in = readInput();
    int row = selected / COLS, rows = (APP_COUNT + COLS - 1) / COLS;
    if (in.left)  selected = (selected + APP_COUNT - 1) % APP_COUNT;
    if (in.right) selected = (selected + 1) % APP_COUNT;
    if (in.up || in.down) {                       // same column, other row (if there is an app there)
        int target = ((row + (in.down ? 1 : rows - 1)) % rows) * COLS + selected % COLS;
        if (target < APP_COUNT) selected = target;
    }
    // Number keys jump straight to an app.
    if (in.ch >= '1' && in.ch < '1' + APP_COUNT) { selected = in.ch - '1'; playSfx(SFX_SELECT); in.enter = true; }
    if (in.ch == 'r') flipScreen();   // r = rotate the screen 180 degrees
    if (in.enter) open(selected);
    drawHome();
    delay(15);
}
