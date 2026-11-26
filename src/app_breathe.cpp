// app_breathe.cpp - guided breathing. A circle grows as you breathe in and shrinks
// as you breathe out; the LED glows brighter with each in-breath. Flow's break uses
// the same breathing guide.
#include <Preferences.h>
#include <math.h>
#include "ui.h"
#include "apps.h"
#include "config.h"
#include "sound.h"
#include "status.h"
#include "focus_sound.h"
#include "voice.h"

struct Pattern { const char* name; const char* about; uint16_t in, hold1, out, hold2; };   // seconds
static const Pattern PATTERNS[] = {
    {"Calm",       "in 4, hold 4, out 6",        4, 4, 6, 0},
    {"Box",        "in 4, hold 4, out 4, hold 4", 4, 4, 4, 4},
    {"4-7-8",      "in 4, hold 7, out 8",        4, 7, 8, 0},
    {"Even",       "in 5, out 5",                5, 0, 5, 0},
};
static const int PATTERN_COUNT = sizeof(PATTERNS) / sizeof(PATTERNS[0]);
static const int MINUTES[] = {1, 3, 5, 10};

// Where in the breath are we? size 0..1 (circle), the phase's name and seconds left in it.
static float breathAt(const Pattern& p, uint32_t ms, const char*& phase, int& secsLeft, int& phaseIndex) {
    uint32_t cycle = (p.in + p.hold1 + p.out + p.hold2) * 1000UL;
    uint32_t t = ms % cycle;
    uint32_t a = p.in * 1000UL, b = a + p.hold1 * 1000UL, c = b + p.out * 1000UL;
    float ease;
    if (t < a)      { phase = "Breathe in";  phaseIndex = 0; secsLeft = (a - t + 999) / 1000; ease = (float)t / a; ease = 0.5f - 0.5f * cosf(ease * PI); return ease; }
    if (t < b)      { phase = "Hold";        phaseIndex = 1; secsLeft = (b - t + 999) / 1000; return 1; }
    if (t < c)      { phase = "Breathe out"; phaseIndex = 2; secsLeft = (c - t + 999) / 1000; ease = (float)(t - b) / (c - b); return 0.5f + 0.5f * cosf(ease * PI); }
    phase = "Hold"; phaseIndex = 3; secsLeft = (cycle - t + 999) / 1000; return 0;
}

// The breathing circle: a faint ring for the full size, a filled circle for now.
static void drawBreathCircle(int cx, int cy, int rMin, int rMax, float size, uint16_t color) {
    canvas.fillCircle(cx, cy, rMax + 2, col::panel);
    canvas.fillCircle(cx, cy, rMin + (int)((rMax - rMin) * size), color);
}

// Shared with Flow's break: draws one frame of the calm (4-4-6) breathing guide.
void drawBreathGuide(int cx, int cy, int rMin, int rMax, uint32_t ms) {
    const char* phase; int left, idx;
    float size = breathAt(PATTERNS[0], ms, phase, left, idx);
    drawBreathCircle(cx, cy, rMin, rMax, size, col::good);
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(col::dim);
    canvas.drawString(phase, cx, cy + rMax + 10);
}

// Breathing music, per phase (pitches from a calm G major chord):
//   in   - the chord rises from G3 to D4 and swells over the whole in-breath
//   hold - a soft bell (A5) and the chord stays put, a little quieter
//   out  - a lower bell (E5), then the chord falls back to G3 and fades with the out-breath
// The bells only play when Guidance is Silent; with Voice or Tones, those are the cues.
static void musicCue(const Pattern& p, int idx, bool bells) {
    switch (idx) {
        case 0: focusBreathSet(293.7f, 0.50f, p.in); break;
        case 1: if (bells) focusBreathBell(880.0f); focusBreathSet(293.7f, 0.36f, 1.0f); break;
        case 2: if (bells) focusBreathBell(659.3f); focusBreathSet(196.0f, 0.035f, p.out); break;
        default: if (bells) focusBreathBell(587.3f); focusBreathSet(196.0f, 0.035f, 1.0f); break;
    }
}

// ---- Guidance: how you know the phase with your eyes closed ----------------------------
// Saved by number (br_gd), so new choices go at the end; ORDER is how they appear.
enum Guidance { G_VOICE, G_TONES, G_SILENT, G_VCOUNT, G_COUNT };
static const char* GUIDANCE_NAME[G_COUNT] = {"Voice", "Tones", "Silent", "Count"};
static const Guidance ORDER[G_COUNT] = {G_VCOUNT, G_VOICE, G_TONES, G_SILENT};
static bool usesVoice(int g) { return g == G_VOICE || g == G_VCOUNT; }
static uint32_t duckUntil = 0;

// A calm voice says "Breathe in" / "Hold" / "Breathe out" on its own speaker channel (2),
// and the music dips for a moment so the words stay clear.
static void speak(int idx, bool music) {
    int phase = idx == 0 ? 0 : idx == 2 ? 2 : 1;
    if (!voicePlay(phase)) return;
    if (music) { focusSoundDuck(true); duckUntil = millis() + voiceMillis(phase) + 150; }
}

// The chosen voice comes from the SD card; if it isn't there, the small built-in voice
// plays instead (said once), so voice guidance always works.
static bool toldBuiltin = false;
static bool ensureVoice() {
    String want = voiceSaved();
    if (voiceLoaded() || want == VOICE_BUILTIN_NAME || voiceLoad(want)) return true;
    if (!toldBuiltin) { toast("Using the built-in voice", col::accent, 1300); toldBuiltin = true; }
    return true;
}

static void cue(Guidance g, const Pattern& p, int idx, bool music) {
    if (music) musicCue(p, idx, g == G_SILENT);
    if (usesVoice(g)) speak(idx, music);
    else if (g == G_TONES) playBreathCue(idx == 0 ? 0 : idx == 2 ? 2 : 1);
}

static void session(const Pattern& p, int minutes, bool music, Guidance guide) {
    uint32_t start = millis(), total = minutes * 60000UL;
    int lastPhase = -1;
    // Count: "In" is the first second, then "two", "three"... one per second
    // to the end of the phase, so the count always matches the pattern (4-7-8 counts to 8).
    uint32_t phaseStart = 0;
    int nextNum = 0;
    bool counting = guide == G_VCOUNT;
    if (music) { focusSoundStart(FS_BREATH); focusBreathSet(196.0f, 0.06f, 0.5f); }
    // The word the voice says, and the same number the voice counts (1 = the word itself).
    static const char* WORD[] = {"In", "Hold", "Out", "Hold"};
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        uint32_t ms = millis() - start;
        if (in.back || ms >= total) break;
        if (in.ch == 'm' && music) focusSoundSetMuted(!focusSoundMuted());

        const char* phase; int left, idx;
        float size = breathAt(p, ms, phase, left, idx);
        if (idx != lastPhase) {                         // each phase begins with its cue
            cue(guide, p, idx, music);
            lastPhase = idx;
            phaseStart = ms;
            nextNum = 2;
        }
        if (counting) {
            const uint16_t secs[] = {p.in, p.hold1, p.out, p.hold2};
            while (nextNum <= secs[idx] && ms >= phaseStart + nextNum * 1000UL) nextNum++;  // too late: skip
            if (nextNum <= secs[idx] && ms >= phaseStart + (nextNum - 1) * 1000UL && !voiceBusy()) {
                if (!voiceCount(nextNum)) counting = false;          // no numbers in this voice
                else if (music) { focusSoundDuck(true); duckUntil = millis() + voiceCountMillis() + 150; }
                nextNum++;
            }
        }
        if (duckUntil && millis() > duckUntil) { focusSoundDuck(false); duckUntil = 0; }
        statusLedOverride(0.08f + 0.92f * size, 60, 140, 255);   // the LED breathes with you

        canvas.fillScreen(col::bg);
        drawBreathCircle(52, 62, 8, 40, size, col::trace);
        // Text column: from x 104 to the right edge (132 px). drawTextFit keeps it inside.
        const int TX = 104, TW = SCREEN_W - TX - 4;
        canvas.setTextDatum(middle_left);
        canvas.setTextColor(col::text);
        drawTextFit(WORD[idx], TX, 36, TW, &fonts::Font4, &fonts::Font2);
        const uint16_t phaseSecs[] = {p.in, p.hold1, p.out, p.hold2};
        int num = phaseSecs[idx] - left + 1;              // counts up: 1 2 3 4, like the voice
        canvas.setFont(&fonts::Font4);
        canvas.setTextColor(col::accent);
        String n = String(num);
        canvas.drawString(n, TX, 64);
        canvas.setFont(&fonts::Font2);
        canvas.setTextColor(col::dim);
        canvas.drawString(String("of ") + phaseSecs[idx], TX + canvas.textWidth(n.c_str(), &fonts::Font4) + 6, 66);
        uint32_t remain = (total - ms) / 1000;
        char buf[16];
        snprintf(buf, sizeof(buf), "%lu:%02lu left", (unsigned long)remain / 60, (unsigned long)remain % 60);
        canvas.setTextColor(col::dim);
        drawTextFit(String(p.name) + "  -  " + buf, TX, 86, TW, &fonts::Font0);
        if (music && focusSoundMuted()) drawTextFit("music muted", TX, 98, TW, &fonts::Font0);
        if (music) drawHints({{"M", focusSoundMuted() ? "unmute" : "mute"}, {"ESC", "stop"}});
        else drawHints({{"ESC", "stop"}});
        pushScreen();
        delay(20);
    }
    if (music) { focusSoundDuck(false); focusSoundStop(true); }
    duckUntil = 0;
    statusLedOverride(-1);                               // give the LED back to Status
    if (millis() - start >= total) {
        playSfx(SFX_SUCCESS);
        toast((String("Well done - ") + minutes + " min").c_str(), col::good, 1800);
    }
}

void appBreathe() {
    Preferences pr;
    pr.begin(PREFS_NS, true);
    int pat = constrain(pr.getInt("br_pat", 0), 0, PATTERN_COUNT - 1);
    int len = constrain(pr.getInt("br_len", 1), 0, 3);
    bool music = pr.getBool("br_mus", true);
    int guide = constrain(pr.getInt("br_gd", G_VCOUNT), 0, G_COUNT - 1);
    pr.end();
    int sel = 0;
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        if (in.back) { voiceUnload(); toldBuiltin = false; return; }
        if (in.up) sel = (sel + 3) % 4;
        if (in.down) sel = (sel + 1) % 4;
        if (in.left || in.right) {
            int step = in.right ? 1 : -1;
            if (sel == 0) pat = (pat + step + PATTERN_COUNT) % PATTERN_COUNT;
            else if (sel == 1) len = (len + step + 4) % 4;
            else if (sel == 2) {                        // hear the choice straight away
                int at = 0;
                while (ORDER[at] != guide) at++;
                guide = ORDER[(at + step + G_COUNT) % G_COUNT];
                if (usesVoice(guide)) { if (ensureVoice()) speak(0, false); }
                else if (guide == G_TONES) playBreathCue(0);
            }
            else music = !music;
        }
        if (in.enter) {
            pr.begin(PREFS_NS, false);
            pr.putInt("br_pat", pat);
            pr.putInt("br_len", len);
            pr.putBool("br_mus", music);
            pr.putInt("br_gd", guide);
            pr.end();
            Guidance g = (Guidance)guide;
            if (usesVoice(g)) ensureVoice();
            session(PATTERNS[pat], MINUTES[len], music, g);
            continue;
        }
        drawHeader("Breathe");
        // A still preview of the circle on the left, settings on the right.
        drawBreathCircle(46, 58, 10, 32, 0.6f, col::trace);
        const char* labels[] = {"Pattern", "Length", "Guidance", "Music"};
        String values[] = {PATTERNS[pat].name, String(MINUTES[len]) + " min", GUIDANCE_NAME[guide], music ? "On" : "Off"};
        for (int i = 0; i < 4; i++) {
            int y = 21 + i * 19;
            if (i == sel) canvas.fillRoundRect(94, y, 142, 17, 4, col::accent);
            canvas.setFont(&fonts::Font2);
            canvas.setTextDatum(middle_left);
            canvas.setTextColor(i == sel ? col::onAccent : col::dim);
            canvas.drawString(labels[i], 100, y + 9);
            canvas.setTextDatum(middle_right);
            canvas.setTextColor(i == sel ? col::onAccent : col::text);
            canvas.drawString(values[i], 230, y + 9);
        }
        canvas.setFont(&fonts::Font0);
        canvas.setTextDatum(middle_left);
        canvas.setTextColor(col::dim);
        // Below the circle the line has the whole screen width (228 px = 38 small characters).
        drawTextFit(sel == 2 ? String(guide == G_VCOUNT ? "Voice says in, hold, out and counts" : guide == G_VOICE ? "Voice says: in, hold, out" : guide == G_TONES ? "Tones: up=in, 2 taps=hold, down=out" : "Silent: no cues (bells if music on)")
                             : String(PATTERNS[pat].name) + ": " + PATTERNS[pat].about, 6, 110, SCREEN_W - 12, &fonts::Font0);
        drawHints({{"OK", "start"}, {ARROWS_LR, "change"}, {ARROWS_UD, "move"}, {"ESC", "home"}});
        pushScreen();
        delay(15);
    }
}
