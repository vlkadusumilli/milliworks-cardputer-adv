#include "ui.h"
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include "config.h"
#include "apps.h"
#include "sound.h"

M5Canvas canvas(&M5Cardputer.Display);

// ---- Themes: RGB565 values from the milliWorks design system --------------------
// Order: bg, panel, edge, text, dim, accent, onAccent, trace, lilac, good, warn, bad
const Theme THEMES[] = {
    {"Green mask", {0x0040, 0x1123, 0x2205, 0xF7FF, 0xA5F6, 0xFC47, 0x0040, 0x3E9D, 0xC53F, 0x3F0F, 0xFE07, 0xFAC9}},
    {"Daylight", {0xEF9D, 0xE73C, 0xCEB9, 0x10C2, 0x4B0B, 0x9A63, 0xFFFF, 0x138F, 0x5A16, 0x0A65, 0x7280, 0xA964}},
    {"Black mask", {0x0841, 0x18C3, 0x2966, 0xF79D, 0x9CD2, 0xDD89, 0x0841, 0x65BB, 0xBD1D, 0x5E31, 0xF447, 0xF34B}},
    {"Purple mask", {0x1884, 0x20C6, 0x394A, 0xF77E, 0xACD7, 0xE5CB, 0x1884, 0x5E5B, 0xF4D8, 0x6692, 0xF509, 0xFBCE}},
    {"Phosphor", {0x1060, 0x18A0, 0x3941, 0xFDA8, 0xBC06, 0xFDA8, 0x1060, 0xFE91, 0xED2B, 0xFF15, 0xFCA6, 0xFB47}},
    {"High contrast", {0x0000, 0x10A2, 0x5ACB, 0xFFFF, 0xD6BA, 0xFEA0, 0x0000, 0x5E9F, 0xD5DF, 0x4F91, 0xFD80, 0xFB0C}},
};
const int THEME_COUNT = sizeof(THEMES) / sizeof(THEMES[0]);

namespace col {
uint16_t bg, panel, edge, text, dim, accent, onAccent, trace, lilac, good, warn, bad;
}

static int themeNow = 0;
int currentTheme() { return themeNow; }

void applyTheme(int i) {
    if (i < 0 || i >= THEME_COUNT) i = 0;
    themeNow = i;
    const uint16_t* c = THEMES[i].c;
    col::bg = c[0]; col::panel = c[1]; col::edge = c[2]; col::text = c[3]; col::dim = c[4];
    col::accent = c[5]; col::onAccent = c[6]; col::trace = c[7]; col::lilac = c[8];
    col::good = c[9]; col::warn = c[10]; col::bad = c[11];
}

int savedTheme() {
    Preferences p;
    p.begin(PREFS_NS, true);
    int t = p.getInt("theme", 0);
    p.end();
    return (t >= 0 && t < THEME_COUNT) ? t : 0;
}

void saveTheme(int i) {
    Preferences p;
    p.begin(PREFS_NS, false);
    p.putInt("theme", i);
    p.end();
}

// Screen orientation. M5Unified already picks the right rotation for each board
// (the ADV's screen is mounted differently from the original Cardputer), so we
// start from its default. If that is ever portrait, turn it a quarter. The user
// can also flip it 180 degrees (saved in flash) with flipScreen().
static uint8_t baseRotation = 0;

static void applyRotation() {
    Preferences p;
    p.begin(PREFS_NS, true);
    bool flipped = p.getBool("flip", false);
    p.end();
    M5Cardputer.Display.setRotation((baseRotation + (flipped ? 2 : 0)) % 4);
}

void flipScreen() {
    Preferences p;
    p.begin(PREFS_NS, false);
    p.putBool("flip", !p.getBool("flip", false));
    p.end();
    applyRotation();
}

void uiBegin() {
    applyTheme(savedTheme());
    baseRotation = M5Cardputer.Display.getRotation();
    if (M5Cardputer.Display.width() < M5Cardputer.Display.height()) {
        baseRotation = (baseRotation + 1) % 4;    // portrait -> landscape
    }
    applyRotation();
    M5Cardputer.Display.setBrightness(screenBrightness());
    canvas.setColorDepth(16);
    canvas.createSprite(SCREEN_W, SCREEN_H);  // 240*135*2 bytes = ~64 KB of RAM
    canvas.setTextWrap(false);
}

void pushScreen() { canvas.pushSprite(0, 0); }

// M5Unified detects the board at start-up, so one firmware can tell an original
// Cardputer from a Cardputer ADV (they differ in keyboard chip, audio and screen mounting).
const char* deviceName() {
    switch (M5.getBoard()) {
        case m5::board_t::board_M5CardputerADV: return "Cardputer ADV";
        case m5::board_t::board_M5Cardputer:    return "Cardputer";
        default:                                return "M5Stack device";
    }
}

// Brightness is saved so it survives leaving an app and restarting. Matte screens
// need more light than glossy ones, so the default is about 70%.
int screenBrightness() {
    Preferences p;
    p.begin(PREFS_NS, true);
    int b = p.getInt("bright", 180);
    p.end();
    return constrain(b, 40, 255);
}

int nextBrightness() {
    const int steps[] = {64, 128, 180, 255};          // 25%, 50%, 70%, 100%
    int now = screenBrightness(), next = steps[0];
    for (int s : steps) if (s > now) { next = s; break; }
    Preferences p;
    p.begin(PREFS_NS, false);
    p.putInt("bright", next);
    p.end();
    M5Cardputer.Display.setBrightness(next);
    return next;
}

void showSplash() {
    canvas.fillScreen(col::bg);
    canvas.setFont(&fonts::Font4);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(col::text);
    canvas.drawString(FW_NAME, SCREEN_W / 2, 56);
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(col::dim);
    canvas.drawString(String("for ") + deviceName(), SCREEN_W / 2, 82);
    canvas.setFont(&fonts::Font0);
    canvas.drawString("v" FW_VERSION, SCREEN_W / 2, 100);
    pushScreen();
    playSfx(SFX_STARTUP);
    delay(900);
}

static void drawBattery(int x, int y) {
    int level = M5.Power.getBatteryLevel();    // 0..100, or <0 if unknown
    uint16_t c = level < 0 ? col::dim : level < 20 ? col::bad : level < 50 ? col::warn : col::good;
    canvas.drawRect(x, y, 20, 10, col::text);
    canvas.fillRect(x + 20, y + 3, 2, 4, col::text);
    if (level > 0) canvas.fillRect(x + 2, y + 2, 16 * level / 100, 6, c);
}

void drawHeader(const char* title, bool showClock) {
    canvas.fillScreen(col::bg);
    canvas.fillRect(0, 0, SCREEN_W, HEADER_H, col::panel);
    canvas.setFont(&fonts::Font2);              // built-in 16px font
    canvas.setTextSize(1);
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(col::text);
    canvas.drawString(title, 4, HEADER_H / 2);

    // Small clock, only once the time is known (after an internet sync).
    time_t now = time(nullptr);
    if (showClock && now > 1700000000 && clockIsVerified()) {
        struct tm t;
        localtime_r(&now, &t);
        char buf[8];
        strftime(buf, sizeof(buf), "%H:%M", &t);
        canvas.setTextDatum(middle_right);
        canvas.drawString(buf, SCREEN_W - 44, HEADER_H / 2);
    }
    // Wi-Fi dot: green when connected.
    canvas.fillCircle(SCREEN_W - 34, HEADER_H / 2, 3, WiFi.isConnected() ? col::good : col::dim);
    drawBattery(SCREEN_W - 26, 3);
}

// Key hints drawn as little key caps:  [OK] open  [ESC] back
// The small font has no arrow characters, so the arrow codes \x18-\x1b in a key label
// are drawn as little triangles (6 px each). Everything else is ordinary text.
static bool isArrow(char c) { return c >= 0x18 && c <= 0x1b; }
static int keyWidth(const char* k) {
    int w = 0;
    for (const char* p = k; *p; p++) {
        if (isArrow(*p)) w += 6;
        else { char s[2] = {*p, 0}; w += canvas.textWidth(s); }
    }
    return w;
}
static void drawKey(const char* k, int x, int y) {
    for (const char* p = k; *p; p++) {
        if (isArrow(*p)) {
            int cx = x + 2, cy = y + 3;                     // a 5x5 triangle in a 6 px cell
            switch (*p) {
                case 0x18: canvas.fillTriangle(cx, cy + 4, cx + 4, cy + 4, cx + 2, cy, col::text); break;   // up
                case 0x19: canvas.fillTriangle(cx, cy, cx + 4, cy, cx + 2, cy + 4, col::text); break;       // down
                case 0x1a: canvas.fillTriangle(cx, cy, cx, cy + 4, cx + 4, cy + 2, col::text); break;       // right
                case 0x1b: canvas.fillTriangle(cx + 4, cy, cx + 4, cy + 4, cx, cy + 2, col::text); break;   // left
            }
            x += 6;
        } else {
            char s[2] = {*p, 0};
            canvas.drawString(s, x, y);
            x += canvas.textWidth(s);
        }
    }
}

void drawHints(std::initializer_list<Hint> hints) {
    canvas.setFont(&fonts::Font0);
    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);
    const int y = SCREEN_H - 10, gap = 7;
    int total = 0;
    for (auto& h : hints) total += keyWidth(h.key) + 4 + 3 + canvas.textWidth(h.label) + gap;
    int x = max(0, (SCREEN_W - total + gap) / 2);
    for (auto& h : hints) {
        int kw = keyWidth(h.key) + 4;
        canvas.fillRoundRect(x, y - 1, kw, 10, 2, col::panel);
        canvas.setTextColor(col::text);
        drawKey(h.key, x + 2, y);
        x += kw + 3;
        canvas.setTextColor(col::dim);
        canvas.drawString(h.label, x, y);
        x += canvas.textWidth(h.label) + gap;
    }
}

void drawFooter(const char* hint) {
    canvas.setFont(&fonts::Font0);              // tiny 6x8 font
    canvas.setTextSize(1);
    canvas.setTextDatum(bottom_center);
    canvas.setTextColor(col::dim);
    canvas.drawString(hint, SCREEN_W / 2, SCREEN_H - 1);
}

Input readInput(bool textMode) {
    serviceTick();                     // keep Wi-Fi and the time ticking over on every screen
    Input in;
    auto& kb = M5Cardputer.Keyboard;
    // isChange() is true when the number of held keys changes; isPressed() > 0
    // means at least one is down - together: "a key was just pressed".
    if (!(kb.isChange() && kb.isPressed())) return in;

    auto st = kb.keysState();
    in.fn = st.fn;
    in.shift = st.shift;
    in.ctrl = st.ctrl;
    in.enter = st.enter;
    in.del = st.del;
    in.tab = st.tab;
    char c = st.word.empty() ? 0 : st.word.back();

    // The arrow symbols are printed on these four keys.
    auto arrow = [&](char k) {
        if (k == ';') { in.up = true; return true; }
        if (k == '.') { in.down = true; return true; }
        if (k == ',') { in.left = true; return true; }
        if (k == '/') { in.right = true; return true; }
        return false;
    };

    if (textMode) {
        if (in.fn && c == '`') { in.back = true; c = 0; }
        else if (in.fn && arrow(c)) { c = 0; }
    } else {
        // In menus both ESC (`) and Del go back - whichever your finger expects.
        if (c == '`' || in.del) { in.back = true; c = 0; }
        else if (arrow(c)) { c = 0; }
    }
    in.ch = c;
    in.pressed = in.ch || in.enter || in.del || in.tab || in.back ||
                 in.up || in.down || in.left || in.right;

    // Every screen gets its sounds from here: the current theme's pack.
    if (in.back) playSfx(SFX_BACK);
    else if (in.enter) playSfx(SFX_SELECT);
    else if (in.up || in.down || in.left || in.right) playSfx(SFX_MOVE);
    else if (textMode && (in.ch || in.del)) playSfx(SFX_TYPE);
    return in;
}

bool anyKeyHeld() { return M5Cardputer.Keyboard.isPressed() > 0; }

void beep(uint16_t freq, uint32_t ms) { M5Cardputer.Speaker.tone(freq, ms); }

void chime() {
    const uint16_t notes[] = {1319, 1568, 2093};   // E6, G6, C7
    for (auto n : notes) {
        M5Cardputer.Speaker.tone(n, 120);
        delay(150);
    }
}

void toast(const char* msg, uint16_t color, uint32_t ms) {
    if (color == col::good) playSfx(SFX_SUCCESS);
    else if (color == col::warn) playSfx(SFX_NOTICE);
    else if (color == col::bad) playSfx(SFX_ERROR);
    canvas.fillRoundRect(16, 45, SCREEN_W - 32, 44, 6, color);
    canvas.setFont(&fonts::Font2);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(col::onAccent);
    canvas.drawString(msg, SCREEN_W / 2, 67);
    pushScreen();
    uint32_t end = millis() + ms;
    while (millis() < end) {
        M5Cardputer.update();
        if (readInput().pressed) break;
        delay(10);
    }
}

void drawList(const std::vector<String>& items, int selected, int top, int rows) {
    canvas.setFont(&fonts::Font2);
    canvas.setTextDatum(middle_left);
    const int rowH = 18;
    for (int i = 0; i < rows && top + i < (int)items.size(); i++) {
        int idx = top + i;
        int y = HEADER_H + 4 + i * rowH;
        if (idx == selected) {
            canvas.fillRoundRect(2, y, SCREEN_W - 4, rowH - 1, 4, col::accent);
        }
        canvas.setTextColor(idx == selected ? col::onAccent : col::text);
        canvas.drawString(items[idx], 8, y + rowH / 2);
    }
    // Scroll bar when the list is longer than the screen.
    if ((int)items.size() > rows) {
        int barH = (SCREEN_H - HEADER_H - 14) * rows / items.size();
        int barY = HEADER_H + 4 + (SCREEN_H - HEADER_H - 14 - barH) * top / (items.size() - rows);
        canvas.fillRect(SCREEN_W - 3, barY, 2, barH, col::dim);
    }
}

bool textPrompt(const char* title, const char* question, String& value, bool hidden) {
    // Secret fields (passwords) start VISIBLE so typos are easy to spot on the
    // tiny keyboard; Tab hides them with * when someone is looking.
    bool show = true;
    while (true) {
        M5Cardputer.update();
        Input in = readInput(true);
        if (in.back) return false;
        if (in.enter) return true;
        if (in.tab) show = !show;                         // Tab shows/hides a password
        if (in.del && value.length()) value.remove(value.length() - 1);
        if (in.ch && value.length() < 63) value += in.ch;

        drawHeader(title);
        canvas.setFont(&fonts::Font2);
        canvas.setTextDatum(top_left);
        canvas.setTextColor(col::text);
        canvas.drawString(question, 6, 24);
        canvas.drawRoundRect(4, 46, SCREEN_W - 8, 26, 4, col::accent);
        String shown = show ? value : String();
        if (!show) for (size_t i = 0; i < value.length(); i++) shown += '*';
        // Keep the end of long text visible.
        while (canvas.textWidth(shown + "_") > SCREEN_W - 20) shown.remove(0, 1);
        canvas.setTextDatum(middle_left);
        canvas.drawString(shown, 10, 59);
        if ((millis() / 500) % 2) {                      // blinking copper cursor
            canvas.setTextColor(col::accent);
            canvas.drawString("_", 10 + canvas.textWidth(shown), 59);
        }
        if (hidden) drawHints({{"OK", "done"}, {"Tab", show ? "hide" : "show"}, {"Fn+ESC", "cancel"}});
        else drawHints({{"OK", "done"}, {"Del", "erase"}, {"Fn+ESC", "cancel"}});
        pushScreen();
        delay(10);
    }
}

int pickFromList(const char* title, const std::vector<String>& items, int selected) {
    int top = 0;
    selected = constrain(selected, 0, max(0, (int)items.size() - 1));
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        if (in.back) return -1;
        if (in.up) selected = (selected + items.size() - 1) % items.size();
        if (in.down) selected = (selected + 1) % items.size();
        if (in.enter && !items.empty()) return selected;
        const int rows = 5;
        if (selected < top) top = selected;
        if (selected >= top + rows) top = selected - rows + 1;
        drawHeader(title);
        drawList(items, selected, top, rows);
        drawHints({{ARROWS_UD, "move"}, {"OK", "select"}, {"ESC", "back"}});
        pushScreen();
        delay(15);
    }
}

bool confirm(const char* question) {
    playSfx(SFX_NOTICE);
    canvas.fillRoundRect(12, 36, SCREEN_W - 24, 62, 6, col::panel);
    canvas.drawRoundRect(12, 36, SCREEN_W - 24, 62, 6, col::warn);
    canvas.setFont(&fonts::Font2);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(col::text);
    canvas.drawString(question, SCREEN_W / 2, 56);
    canvas.setFont(&fonts::Font0);
    canvas.setTextColor(col::dim);
    canvas.drawString("OK = yes     ESC = no", SCREEN_W / 2, 82);
    pushScreen();
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        if (in.enter) return true;
        if (in.back) return false;
        delay(15);
    }
}

void drawTextFit(const String& text, int x, int y, int maxW, const lgfx::IFont* font, const lgfx::IFont* smaller) {
    canvas.setFont(font);
    if (canvas.textWidth(text) > maxW && smaller) canvas.setFont(smaller);
    String t = text;
    while (t.length() > 1 && canvas.textWidth(t) > maxW) t.remove(t.length() - 2, 1);   // keep the last char as "~"
    if (t != text && t.length()) t.setCharAt(t.length() - 1, '~');
    canvas.drawString(t, x, y);
}

// After settings are loaded from the SD card: theme, rotation and brightness at once.
void uiReloadSettings() {
    applyTheme(savedTheme());
    applyRotation();
    M5Cardputer.Display.setBrightness(screenBrightness());
}
