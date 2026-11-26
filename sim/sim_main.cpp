// sim_main.cpp - runs the real milliWorks firmware (src/) in a Mac window.
//
//   make -C sim run            the simulator, as your Cardputer is now (settings kept)
//   make -C sim run-new        like a brand-new Cardputer: no settings, no time (welcome tour)
//
// Options for ./milliworks-sim:  --cold   start without the time, like a real power-on
//                                --fresh  forget all settings first (sim/data/prefs.txt)
#include "Arduino.h"
#include "M5Cardputer.h"
#include <SDL2/SDL.h>
#include <cstring>
#include <sys/stat.h>
#include <dispatch/dispatch.h>
#undef time

void setup();                         // the firmware's own setup() and loop() (src/main.cpp)
void loop();

extern std::string simRoot;
extern bool simCold;
void simInitTime();
const char* simLedName();
int simLedLevel();

// ---- script mode: --script "down down enter shot:menu wait:500 type:hello esc" ----
// Each word: a key (enter esc up down left right del tab space, or one character; capitals
// and symbols press Shift), shot:<name> saves sim/shots/<name>.bmp, wait:<ms> pauses,
// type:<text> types a word. The simulator quits at the end.
extern std::atomic<int> simScriptKey;
extern std::atomic<bool> simScriptShift, simScripted;
static std::vector<std::string> script;
static std::atomic<bool> shotRequested{false};
static std::string shotName;
static std::atomic<bool> scriptDone{false};

static void saveShot(const std::string& name) {
    const int W = 240, H = 135;
    std::vector<uint16_t> px(W * H);
    M5Cardputer.Display.readRect(0, 0, W, H, px.data());
    std::string dir = simRoot + "/shots";
    ::mkdir(dir.c_str(), 0755);
    FILE* f = fopen((dir + "/" + name + ".bmp").c_str(), "wb");
    if (!f) return;
    int row = W * 3, size = 54 + row * H;
    uint8_t h[54] = {'B', 'M'};
    auto le = [&](int at, uint32_t v) { for (int i = 0; i < 4; i++) h[at + i] = v >> (8 * i); };
    le(2, size); le(10, 54); le(14, 40); le(18, W); le(22, H); h[26] = 1; h[28] = 24; le(34, row * H);
    fwrite(h, 1, 54, f);
    for (int y = H - 1; y >= 0; y--)
        for (int x = 0; x < W; x++) {
            uint16_t c = px[y * W + x];
            c = (c >> 8) | (c << 8);                                  // the panel stores RGB565 big-endian
            uint8_t r = (c >> 11) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, b = (c & 31) * 255 / 31;
            uint8_t bgr[3] = {b, g, r};
            fwrite(bgr, 1, 3, f);
        }
    fclose(f);
    printf("[shot] %s/%s.bmp\n", dir.c_str(), name.c_str());
    fflush(stdout);
}

static int scancodeFor(char c, bool& shift) {
    static const char* lower = "abcdefghijklmnopqrstuvwxyz1234567890-=[]\\;',./` ";
    static const char* upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()_+{}|:\"<>?~ ";
    const SDL_Scancode codes[] = {
        SDL_SCANCODE_A, SDL_SCANCODE_B, SDL_SCANCODE_C, SDL_SCANCODE_D, SDL_SCANCODE_E, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H,
        SDL_SCANCODE_I, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_M, SDL_SCANCODE_N, SDL_SCANCODE_O, SDL_SCANCODE_P,
        SDL_SCANCODE_Q, SDL_SCANCODE_R, SDL_SCANCODE_S, SDL_SCANCODE_T, SDL_SCANCODE_U, SDL_SCANCODE_V, SDL_SCANCODE_W, SDL_SCANCODE_X,
        SDL_SCANCODE_Y, SDL_SCANCODE_Z, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6,
        SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_0, SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS, SDL_SCANCODE_LEFTBRACKET,
        SDL_SCANCODE_RIGHTBRACKET, SDL_SCANCODE_BACKSLASH, SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_APOSTROPHE,
        SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD, SDL_SCANCODE_SLASH, SDL_SCANCODE_GRAVE, SDL_SCANCODE_SPACE};
    const char* p = strchr(lower, c);
    if (p) { shift = false; return codes[p - lower]; }
    p = strchr(upper, c);
    if (p) { shift = true; return codes[p - upper]; }
    return -1;
}

static void press(int sc, bool shift) {
    simScriptShift = shift; simScriptKey = sc; delay(90);
    simScriptKey = -1; simScriptShift = false; delay(140);
}

static void runScript() {
    delay(1500);                                                      // let the start-up finish
    for (auto& w : script) {
        if (w.rfind("shot:", 0) == 0) { shotName = w.substr(5); shotRequested = true; while (shotRequested) delay(10); continue; }
        if (w.rfind("wait:", 0) == 0) { delay(atoi(w.c_str() + 5)); continue; }
        if (w.rfind("type:", 0) == 0) { for (char c : w.substr(5)) { bool sh; int sc = scancodeFor(c, sh); if (sc >= 0) press(sc, sh); } continue; }
        struct { const char* n; SDL_Scancode sc; } named[] = {{"enter", SDL_SCANCODE_RETURN}, {"esc", SDL_SCANCODE_ESCAPE}, {"up", SDL_SCANCODE_UP},
            {"down", SDL_SCANCODE_DOWN}, {"left", SDL_SCANCODE_LEFT}, {"right", SDL_SCANCODE_RIGHT}, {"del", SDL_SCANCODE_BACKSPACE},
            {"tab", SDL_SCANCODE_TAB}, {"space", SDL_SCANCODE_SPACE}};
        bool done = false;
        for (auto& n : named) if (w == n.n) { press(n.sc, false); done = true; }
        if (!done && w.size() == 1) { bool sh; int sc = scancodeFor(w[0], sh); if (sc >= 0) press(sc, sh); }
        delay(120);
    }
    delay(300);
    scriptDone = true;
}

extern void (*simFrameHook)();
static bool* runningFlag = nullptr;
static std::string lastTitle;
static uint32_t lastCheck = 0;

// macOS only lets the main thread touch the window, and the firmware runs on its own
// thread: hand the new title to the main thread.
static void applyTitle(void* text) {
    static std::string shown;                         // the panel keeps the pointer: keep it alive
    shown = (char*)text;
    free(text);
    if (auto* p = (lgfx::Panel_sdl*)M5Cardputer.Display.panel()) p->setWindowTitle(shown.c_str());
}

static void frame() {
    if (shotRequested) { saveShot(shotName); shotRequested = false; }
    if (scriptDone || (runningFlag && !*runningFlag)) {
        SDL_Event e; e.type = SDL_QUIT; SDL_PushEvent(&e);
        delay(200);
        exit(0);                                      // the apps run their own loops: leave from here
    }
    if (millis() - lastCheck > 250) {                 // the LED shows in the window title
        lastCheck = millis();
        std::string title = std::string("milliWorks - Cardputer ADV simulator   LED: ") + simLedName();
        if (title != lastTitle) {
            lastTitle = title;
            dispatch_async_f(dispatch_get_main_queue(), strdup(lastTitle.c_str()), applyTitle);
            printf("[LED] %s\n", simLedName());
            fflush(stdout);
        }
    }
}

static int firmware(bool* running) {
    runningFlag = running;
    simFrameHook = frame;
    simInitTime();
    if (!script.empty()) { simScripted = true; std::thread(runScript).detach(); }
    setup();
    while (*running) loop();
    return 0;
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--cold")) simCold = true;
        if (!strcmp(argv[i], "--fresh")) { ::remove((simRoot + "/data/prefs.txt").c_str()); simCold = true; }
        if (!strcmp(argv[i], "--root") && i + 1 < argc) simRoot = argv[++i];
        if (!strcmp(argv[i], "--cardputer-keys")) {
            std::string port = (i + 1 < argc && argv[i + 1][0] == '/') ? argv[++i] : "";
            extern void simStartBridge(std::string);
            simStartBridge(port);
        }
        if (!strcmp(argv[i], "--script") && i + 1 < argc) {
            std::string all = argv[++i], w;
            for (char c : all + " ") { if (c == ' ') { if (!w.empty()) script.push_back(w); w.clear(); } else w += c; }
        }
    }
    printf("milliWorks simulator. Keys: as printed on the Cardputer; Esc = back, arrows = arrows,\n"
           "Enter = OK, Backspace = Del. SD card = sim/sdcard, settings = sim/data/prefs.txt.\n"
           "Wi-Fi: 'HomeWiFi' takes any password except 'wrong'; 'OldRouter' never connects.\n");
    return lgfx::Panel_sdl::main(firmware);
}
