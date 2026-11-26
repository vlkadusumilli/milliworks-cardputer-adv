// focus_sound.cpp - streams generated sound to the speaker on its own channel (1),
// so the UI sounds (channel 0) still play on top. Three buffers take turns: while
// the speaker plays one, the next is filled - the pattern M5Unified recommends.
#include <M5Cardputer.h>
#include <math.h>
#include "focus_sound.h"
#include "focus_gen.h"
#include "sound.h"

static const int RATE = FocusGen::RATE, N = 1024, CH = 1;
static int16_t buf[3][N];
static int bufIndex = 0;
static volatile int current = FS_SILENCE;
static volatile bool running = false, muted = false, fading = false;
static float gain = 0, targetGain = 1;
static TaskHandle_t task = nullptr;

static const char* NAMES[FS_COUNT] = {"Brown noise", "Rain", "Soft drone", "Ticking clock", "Silence"};
const char* focusSoundName(int s) { return NAMES[(s >= 0 && s < FS_COUNT) ? s : FS_SILENCE]; }

// The generators live in focus_gen.h, shared with tools/render_sounds.cpp.
static FocusGen gen;

static void soundTask(void*) {
    for (;;) {
        if (!running) { vTaskDelay(pdMS_TO_TICKS(30)); continue; }
        // Wait while two buffers are already queued.
        while (M5Cardputer.Speaker.isPlaying(CH) >= 2) vTaskDelay(pdMS_TO_TICKS(4));
        int16_t* b = buf[bufIndex];
        int s = current;
        for (int i = 0; i < N; i++) {
            // Smooth volume changes (fade in, fade out) one sample at a time.
            gain += (targetGain - gain) * 0.0004f;
            b[i] = focusToPcm(gen.sample(s) * gain);
        }
        if (fading && gain < 0.01f) { running = false; fading = false; continue; }
        M5Cardputer.Speaker.playRaw(b, N, RATE, false, 1, CH, false);
        bufIndex = (bufIndex + 1) % 3;
    }
}

void focusBreathSet(float freq, float amp, float seconds) { gen.breathSet(freq, amp, seconds); }
void focusBreathBell(float freq) { gen.breathBell(freq); }

void focusSoundStart(int s) {
    if (!task) xTaskCreatePinnedToCore(soundTask, "focus", 3072, nullptr, 3, &task, 0);
    if (s == FS_SILENCE) { current = s; running = false; return; }
    bool same = running && current == s;
    current = s;
    fading = false;
    if (!same) gain = 0;                                // a new sound fades in gently
    targetGain = 1;                                     // the same one just carries on
    M5Cardputer.Speaker.setChannelVolume(CH, muted ? 0 : soundVolume());
    running = true;
}

void focusSoundStop(bool fade) {
    if (!running) return;
    if (fade) { fading = true; targetGain = 0; }
    else { running = false; M5Cardputer.Speaker.stop(CH); }
}

void focusSoundDuck(bool duck) {                       // quieter while a voice speaks
    if (muted) return;
    M5Cardputer.Speaker.setChannelVolume(CH, duck ? soundVolume() * 35 / 100 : soundVolume());
}

void focusSoundSetMuted(bool m) {
    muted = m;
    M5Cardputer.Speaker.setChannelVolume(CH, m ? 0 : soundVolume());   // instant
}

bool focusSoundMuted() { return muted; }
bool focusSoundPlaying() { return running; }
