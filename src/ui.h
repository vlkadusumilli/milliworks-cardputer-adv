// ui.h - shared helpers every app uses: drawing, keyboard input, sounds.
#pragma once
#include <M5Cardputer.h>

// ---- Screen -----------------------------------------------------------------
// We draw everything into an off-screen "canvas" (a sprite in RAM) and then push
// it to the LCD in one go. That avoids flicker.
extern M5Canvas canvas;
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;
constexpr int HEADER_H = 16;

// A small colour palette so all apps look alike (RGB565 colours).
// The colour roles every screen uses. They are variables, not constants, so the
// Settings app can switch the whole UI to another theme (see THEMES in ui.cpp).
namespace col {
extern uint16_t bg;        // board-900  the ground
extern uint16_t panel;     // board-800  status bar, key caps, cards
extern uint16_t edge;      // board-700  outlines, dividers (never text)
extern uint16_t text;      // silk-100   primary text
extern uint16_t dim;       // silk-400   secondary text
extern uint16_t accent;    // copper-500 focus and selection ONLY ("you are here")
extern uint16_t onAccent;  // copper-ink text on copper (and on toasts)
extern uint16_t trace;     // trace-400  network: Wi-Fi icon, signal, progress
extern uint16_t lilac;     // lilac-300  storage: SD icon, usage bar
extern uint16_t good;      // ok-400     connected, saved
extern uint16_t warn;      // warn-400   needs attention
extern uint16_t bad;       // alert-400  failed, destructive
}  // namespace col

// ---- Themes (milliWorks design system) -----------------------------------------
struct Theme { const char* name; uint16_t c[12]; };   // c[] in the order of col:: above
extern const Theme THEMES[];
extern const int THEME_COUNT;
void applyTheme(int index);              // switch the colours now (not saved)
int  currentTheme();                     // the theme shown right now (sounds follow it)
int  savedTheme();                       // the theme stored in flash
void saveTheme(int index);

void uiBegin();                          // call once in setup()
void flipScreen();                       // turn the screen 180 degrees (remembered)
const char* deviceName();                // the board we run on, e.g. "Cardputer ADV"
void showSplash();                       // start-up screen: name, device, version
int  screenBrightness();                 // 0..255, remembered in flash
int  nextBrightness();                   // step 25% -> 50% -> 75% -> 100%, save it, return the new level
void drawHeader(const char* title, bool showClock = true);  // title bar with clock + battery
void drawFooter(const char* hint);       // plain help line at the bottom

// Key hints as little key caps, e.g. drawHints({{ARROWS_UD, "move"}, {"OK", "open"}});
struct Hint { const char* key; const char* label; };
void drawHints(std::initializer_list<Hint> hints);
#define ARROWS_UD "\x18\x19"      // up/down arrow characters in the small font
#define ARROWS_LR "\x1b\x1a"      // left/right arrow characters
void pushScreen();                       // copy canvas -> LCD

// ---- Keyboard ---------------------------------------------------------------
// One key press, already decoded into the things apps care about.
struct Input {
    bool pressed = false;   // true only on the frame a key went down
    char ch = 0;            // printable character (with Shift applied), 0 if none
    bool enter = false, del = false, tab = false, back = false;
    bool up = false, down = false, left = false, right = false;
    bool fn = false, shift = false, ctrl = false;
};

// Reads the keyboard. Call once per loop AFTER M5Cardputer.update().
//  textMode = false: ` (ESC) and Del are Back; ; . , / are arrows (good for menus).
//  textMode = true : every key types its character; Fn+` is Back, Fn+arrows move.
Input readInput(bool textMode = false);

// Returns true while any key is held down (for "hold to repeat").
bool anyKeyHeld();

// ---- Sound ------------------------------------------------------------------
void beep(uint16_t freq = 2000, uint32_t ms = 40);
void chime();                            // a short three-note melody

// ---- Little widgets ---------------------------------------------------------
// Shows a full-screen text box and lets the user type. Returns false if cancelled.
bool textPrompt(const char* title, const char* question, String& value, bool hidden = false);

// Shows a message for `ms` milliseconds (or until a key is pressed).
void toast(const char* msg, uint16_t color = col::accent, uint32_t ms = 1200);

// A full-screen list to choose from. Returns the chosen index, or -1 for Back.
int pickFromList(const char* title, const std::vector<String>& items, int selected = 0);

// "Are you sure?" box. OK = true, ESC/Del = false.
bool confirm(const char* question);

// Draws text that never runs past maxW pixels: it tries `font`, then `smaller` (if given),
// and only then shortens the text with "~". Uses the current datum and colour.
void drawTextFit(const String& text, int x, int y, int maxW, const lgfx::IFont* font, const lgfx::IFont* smaller = nullptr);

// Draws a list with a highlighted row and scrolls to keep it visible.
void drawList(const std::vector<String>& items, int selected, int top, int rows);
