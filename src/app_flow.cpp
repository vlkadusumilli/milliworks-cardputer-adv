// app_flow.cpp - Flow: a concentration app.
//   Set up:  intention, length, focus sound, break length.
//   Focus:   big timer; M mutes the sound, N parks a thought, D counts a drift,
//            OK pauses. The screen dims after 10 s. The LED shows "Do not disturb".
//   Break:   breathing guide + your parked thoughts; LED green.
//   Summary: what you did today, without judgement.
#include <Preferences.h>
#include <SD.h>
#include <time.h>
#include "ui.h"
#include "apps.h"
#include "config.h"
#include "sound.h"
#include "status.h"
#include "focus_sound.h"

void drawBreathGuide(int cx, int cy, int rMin, int rMax, uint32_t ms);   // app_breathe.cpp
bool sdMount();                                                          // app_sd.cpp
void sdMakeDirs(const char* path);

static const int LENGTHS[] = {15, 25, 45, 50, 90};
static const int BREAKS[] = {5, 10, 15};

struct FlowSettings { String intention; int len = 1, sound = FS_BROWN, brk = 0; };
struct Session { int drifted = 0; std::vector<String> parked; uint32_t focusedMs = 0; };

static FlowSettings load() {
    FlowSettings s;
    Preferences p;
    p.begin(PREFS_NS, true);
    s.intention = p.getString("fl_int", "");
    s.len = constrain(p.getInt("fl_len", 1), 0, 4);
    s.sound = constrain(p.getInt("fl_snd", FS_BROWN), 0, FS_COUNT - 1);
    s.brk = constrain(p.getInt("fl_brk", 0), 0, 2);
    p.end();
    return s;
}

static void save(const FlowSettings& s) {
    Preferences p;
    p.begin(PREFS_NS, false);
    p.putString("fl_int", s.intention);
    p.putInt("fl_len", s.len);
    p.putInt("fl_snd", s.sound);
    p.putInt("fl_brk", s.brk);
    p.end();
}

// Today's totals (reset on a new day, when the date is known).
static int todayKey() {
    time_t now = time(nullptr);
    if (now < 1700000000) return -1;
    struct tm t;
    localtime_r(&now, &t);
    return t.tm_year * 1000 + t.tm_yday;
}
static void addToday(int minutes, int& totalMin, int& sessions) {
    Preferences p;
    p.begin(PREFS_NS, false);
    int day = todayKey();
    if (day != p.getInt("fl_day", -2)) { p.putInt("fl_day", day); p.putInt("fl_min", 0); p.putInt("fl_n", 0); }
    totalMin = p.getInt("fl_min", 0) + minutes;
    sessions = p.getInt("fl_n", 0) + (minutes > 0 ? 1 : 0);
    p.putInt("fl_min", totalMin);
    p.putInt("fl_n", sessions);
    p.end();
}

static void parkToCard(const String& note, const String& intention) {
    if (!sdMount()) return;                       // no card: the note still shows at the break
    sdMakeDirs(FLOW_DIR);
    File f = SD.open(FLOW_DIR "/parked.txt", FILE_APPEND);
    if (!f) return;
    char when[20] = "";
    time_t now = time(nullptr);
    if (now > 1700000000) { struct tm t; localtime_r(&now, &t); strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &t); }
    f.printf("%s  [%s]  %s\n", when, intention.c_str(), note.c_str());
    f.close();
}

static String clip(const String& s, int maxChars) {
    return (int)s.length() <= maxChars ? s : s.substring(0, maxChars - 1) + "~";
}

// ---- set up -----------------------------------------------------------------------
static bool setup(FlowSettings& s) {
    int sel = 1;
    int previewing = -1;                          // the sound playing as a preview, or -1
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        if (in.back) { focusSoundStop(true); return false; }
        if (in.ch == 'm') focusSoundSetMuted(!focusSoundMuted());
        if (in.up) sel = (sel + 3) % 4;
        if (in.down) sel = (sel + 1) % 4;
        if (in.left || in.right) {
            int d = in.right ? 1 : -1;
            if (sel == 1) s.len = (s.len + d + 5) % 5;
            if (sel == 2) s.sound = (s.sound + d + FS_COUNT) % FS_COUNT;
            if (sel == 3) s.brk = (s.brk + d + 3) % 3;
        }
        if (in.enter) {
            if (sel == 0) { String v = s.intention; if (textPrompt("Flow", "What are you working on?", v)) s.intention = v; }
            else { save(s); return true; }      // a previewed sound keeps playing into the session
        }

        // Preview: on the Sound row, play the highlighted sound; elsewhere, let it fade out.
        if (sel == 2 && previewing != s.sound) {
            if (s.sound == FS_SILENCE) focusSoundStop(true); else focusSoundStart(s.sound);
            previewing = s.sound;
        } else if (sel != 2 && previewing != -1) {
            focusSoundStop(true);
            previewing = -1;
        }

        drawHeader("Flow");
        canvas.setFont(&fonts::Font0);
        canvas.setTextDatum(middle_left);
        canvas.setTextColor(col::dim);
        canvas.drawString("What are you working on?", 6, 24);
        bool selInt = sel == 0;
        if (selInt) canvas.fillRoundRect(4, 30, SCREEN_W - 8, 19, 4, col::accent);
        else canvas.drawRoundRect(4, 30, SCREEN_W - 8, 19, 4, col::edge);
        canvas.setFont(&fonts::Font2);
        canvas.setTextColor(selInt ? col::onAccent : (s.intention.length() ? col::text : col::dim));
        canvas.drawString(s.intention.length() ? clip(s.intention, 30) : "OK to type it", 10, 40);
        String vals[] = {String(LENGTHS[s.len]) + " min", focusSoundName(s.sound), String(BREAKS[s.brk]) + " min"};
        const char* keys[] = {"Length", "Sound", "Break"};
        for (int i = 0; i < 3; i++) {
            int y = 54 + i * 19;
            bool on = sel == i + 1;
            if (on) canvas.fillRoundRect(2, y, SCREEN_W - 4, 17, 4, col::accent);
            canvas.setTextDatum(middle_left);
            canvas.setTextColor(on ? col::onAccent : col::dim);
            canvas.drawString(keys[i], 10, y + 9);
            canvas.setTextDatum(middle_right);
            canvas.setTextColor(on ? col::onAccent : col::text);
            canvas.drawString(String("< ") + vals[i] + " >", 230, y + 9);
            if (i == 1 && on && s.sound != FS_SILENCE) {       // little "playing" waves next to the name
                int x = 214 - canvas.textWidth(String("< ") + vals[i] + " >");
                uint16_t c = focusSoundMuted() ? col::dim : col::onAccent;
                for (int k = 0; k < 3; k++) canvas.fillRect(x + k * 4, y + 12 - (k + 1) * 3 + (int)((millis() / 180 + k) % 3), 2, (k + 1) * 3, c);
            }
        }
        if (sel == 2) drawHints({{"OK", "start"}, {ARROWS_LR, "listen"}, {"M", focusSoundMuted() ? "unmute" : "mute"}, {"ESC", "home"}});
        else drawHints({{"OK", sel == 0 ? "type" : "start"}, {ARROWS_LR, "change"}, {ARROWS_UD, "move"}, {"ESC", "home"}});
        pushScreen();
        delay(15);
    }
}

// ---- focus ------------------------------------------------------------------------
// Returns true when the time ran out, false when you ended it early.
static bool focus(const FlowSettings& s, Session& ss) {
    const uint32_t total = LENGTHS[s.len] * 60000UL;
    uint32_t left = total, last = millis(), lastKey = millis();
    bool paused = false, dimmed = false;
    int bright = screenBrightness();
    int sound = s.sound;                          // can be changed mid-session with left/right
    setStatus(ST_DND);
    focusSoundStart(sound);
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        uint32_t now = millis();
        // M mutes at any time - even on the dimmed screen, without waking it.
        if (in.ch == 'm') { focusSoundSetMuted(!focusSoundMuted()); if (dimmed) { lastKey = now; continue; } }
        else if (in.pressed) {
            lastKey = now;
            if (dimmed) { dimmed = false; M5Cardputer.Display.setBrightness(bright); continue; }   // first key just wakes
        }
        if ((in.left || in.right) && !paused) {       // try another sound without stopping the timer
            sound = (sound + (in.right ? 1 : -1) + FS_COUNT) % FS_COUNT;
            if (sound == FS_SILENCE) focusSoundStop(true); else focusSoundStart(sound);
        }
        if (in.ch == 'd') { ss.drifted++; toast("Noted. Back to it.", col::accent, 700); last = millis(); }
        if (in.ch == 'n') {
            String note;
            if (textPrompt("Park a thought", "Park it, get back to work", note) && note.length()) {
                ss.parked.push_back(note);
                parkToCard(note, s.intention);
                playSfx(SFX_SUCCESS);
            }
            last = millis();
        }
        if (in.enter) {
            paused = !paused;
            if (paused) focusSoundStop(true); else if (sound != FS_SILENCE) focusSoundStart(sound);
        }
        if (in.back) {
            if (confirm("End this session?")) { ss.focusedMs += total - left; focusSoundStop(true); M5Cardputer.Display.setBrightness(bright); return false; }
            last = millis();
        }

        uint32_t dt = millis() - last;
        last = millis();
        if (!paused) left = left > dt ? left - dt : 0;
        if (left == 0) { ss.focusedMs += total; focusSoundStop(true); M5Cardputer.Display.setBrightness(bright); return true; }

        if (!paused && !dimmed && now - lastKey > 10000) { dimmed = true; M5Cardputer.Display.setBrightness(25); }

        char t[8];
        snprintf(t, sizeof(t), "%02lu:%02lu", (unsigned long)(left / 60000), (unsigned long)(left / 1000 % 60));
        canvas.fillScreen(col::bg);
        canvas.setTextDatum(middle_center);
        int progress = (SCREEN_W - 40) * (total - left) / total;
        if (dimmed) {                                     // only the time and a thin line
            canvas.setFont(&fonts::Font7);
            canvas.setTextColor(col::dim);
            canvas.drawString(t, SCREEN_W / 2, 62);
            canvas.fillRect(20, 92, progress, 2, col::bad);
            if (focusSoundMuted()) {                      // a quiet hint that M worked
                canvas.setFont(&fonts::Font0);
                canvas.setTextColor(col::dim);
                canvas.drawString("muted", SCREEN_W / 2, 104);
            }
            pushScreen();
            delay(100);
            continue;
        }
        canvas.setFont(&fonts::Font2);
        canvas.setTextColor(col::dim);
        canvas.drawString(s.intention.length() ? clip(s.intention, 30) : "Flow", SCREEN_W / 2, 10);
        canvas.setFont(&fonts::Font7);
        canvas.setTextColor(paused ? col::dim : col::text);
        canvas.drawString(t, SCREEN_W / 2, 50);
        canvas.fillRoundRect(20, 80, SCREEN_W - 40, 4, 2, col::edge);
        canvas.fillRoundRect(20, 80, max(progress, 4), 4, 2, col::bad);
        // Sound chip.
        bool mute = focusSoundMuted() || sound == FS_SILENCE;
        canvas.fillRoundRect(6, 92, 104, 18, 9, col::panel);
        canvas.fillCircle(17, 101, 5, mute ? col::dim : col::trace);
        canvas.setFont(&fonts::Font0);
        canvas.setTextDatum(middle_left);
        canvas.setTextColor(mute ? col::dim : col::text);
        canvas.drawString(paused ? "Paused" : (focusSoundMuted() ? "Muted" : focusSoundName(sound)), 27, 101);
        canvas.setTextDatum(middle_right);
        canvas.setTextColor(col::dim);
        canvas.drawString(String("drifted ") + ss.drifted + "   parked " + ss.parked.size(), 232, 101);
        // Five keys only just fit across 240 px, so labels stay short; while paused the sound can't change.
        if (paused) drawHints({{"M", focusSoundMuted() ? "unmute" : "mute"}, {"N", "park"}, {"D", "drift"}, {"OK", "resume"}});
        else drawHints({{"M", focusSoundMuted() ? "unmute" : "mute"}, {ARROWS_LR, "sound"}, {"N", "park"}, {"D", "drift"}, {"OK", "pause"}});
        pushScreen();
        delay(30);
    }
}

// ---- break ------------------------------------------------------------------------
static void takeBreak(const FlowSettings& s, Session& ss) {
    const uint32_t total = BREAKS[s.brk] * 60000UL, start = millis();
    setStatus(ST_FREE);
    playSfx(SFX_SUCCESS);
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        uint32_t ms = millis() - start;
        if (in.ch == 's' || in.back || ms >= total) break;
        if (in.enter && ss.parked.size()) {
            std::vector<String> items(ss.parked.begin(), ss.parked.end());
            pickFromList("Parked thoughts", items);
        }
        uint32_t left = (total - ms) / 1000;
        char title[20];
        snprintf(title, sizeof(title), "Break  %lu:%02lu", (unsigned long)left / 60, (unsigned long)left % 60);
        drawHeader(title);
        drawBreathGuide(52, 64, 10, 34, ms);
        canvas.setFont(&fonts::Font0);
        canvas.setTextDatum(middle_left);
        canvas.setTextColor(col::dim);
        canvas.drawString(ss.parked.empty() ? "Nothing parked" : "Parked thoughts", 100, 26);
        for (int i = 0; i < 2 && i < (int)ss.parked.size(); i++) {
            canvas.fillRoundRect(98, 34 + i * 20, 138, 17, 4, col::panel);
            canvas.setTextColor(col::text);
            canvas.drawString(clip(ss.parked[ss.parked.size() - 1 - i], 21), 104, 43 + i * 20);
        }
        canvas.setTextColor(col::dim);
        canvas.drawString(String("drifted ") + ss.drifted + "x  -  that's fine", 100, 84);
        drawHints({{"OK", "notes"}, {"S", "skip break"}, {"ESC", "end"}});
        pushScreen();
        delay(20);
    }
    playSfx(SFX_NOTICE);
}

// ---- summary ----------------------------------------------------------------------
// Returns true for "another session".
static bool summary(const FlowSettings& s, Session& ss, int todayMin, int todayN) {
    setStatus(ST_OFF);
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        if (in.back) return false;
        if (in.enter) return true;
        if (in.ch == 'n' && ss.parked.size()) { std::vector<String> items(ss.parked.begin(), ss.parked.end()); pickFromList("Parked thoughts", items); }
        drawHeader("Session done");
        canvas.setTextDatum(middle_center);
        canvas.setFont(&fonts::Font7);
        canvas.setTextColor(col::text);
        canvas.drawString(String(ss.focusedMs / 60000), 40, 52);
        canvas.setFont(&fonts::Font0);
        canvas.setTextColor(col::dim);
        canvas.drawString("min in flow", 40, 82);
        canvas.setFont(&fonts::Font2);
        canvas.setTextDatum(middle_left);
        String lines[] = {s.intention.length() ? clip(s.intention, 18) : "Flow", String("drifted ") + ss.drifted + "x",
                          String("parked ") + ss.parked.size() + " thought" + (ss.parked.size() == 1 ? "" : "s"),
                          String("today: ") + todayMin + " min, " + todayN + " session" + (todayN == 1 ? "" : "s")};
        for (int i = 0; i < 4; i++) {
            canvas.setTextColor(i == 0 ? col::text : i == 3 ? col::good : col::dim);
            canvas.drawString(lines[i], 88, 32 + i * 16);
        }
        drawHints({{"OK", "another"}, {"N", "parked notes"}, {"ESC", "home"}});
        pushScreen();
        delay(30);
    }
}

void appFlow() {
    FlowSettings s = load();
    if (!setup(s)) return;
    while (true) {
        Session ss;
        bool finished = focus(s, ss);
        if (finished) takeBreak(s, ss);
        int todayMin, todayN;
        addToday(ss.focusedMs / 60000, todayMin, todayN);
        if (!summary(s, ss, todayMin, todayN)) break;
    }
    setStatus(ST_OFF);
    focusSoundStop(false);
}
