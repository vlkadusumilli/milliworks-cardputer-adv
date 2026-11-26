// sound.cpp - plays short note sequences on a background task, so a key press is
// never slowed down by its sound. A newer sound cuts an older one short.
#include <M5Cardputer.h>
#include <Preferences.h>
#include "sound.h"
#include "ui.h"
#include "config.h"

#include "sound_packs.h"   // waveforms, notes and packs (shared with tools/render_sounds.cpp)

static QueueHandle_t queue;
static bool enabled = true;
static int volume = 170;

static const Pack& pack() {
    int t = currentTheme();
    return PACKS[(t >= 0 && t < PACK_COUNT) ? t : 0];
}

static void soundTask(void*) {
    uint8_t id;
    for (;;) {
        if (xQueueReceive(queue, &id, portMAX_DELAY) != pdTRUE) continue;
        const Pack& p = pack();
        bool breath = id >= 200 && id < 203;                // Breathe cues: same in every theme
        const Phrase& ph = breath ? BREATH_CUES[id - 200] : p.ev[id];
        const uint8_t* wave = breath ? WAVE_TRIANGLE : p.wave;
        int v = breath ? volume : (int)(volume * p.gain * EVENT_GAIN[id]);
        M5Cardputer.Speaker.setChannelVolume(0, constrain(v, 0, 255));
        for (int i = 0; i < ph.count; i++) {
            if (uxQueueMessagesWaiting(queue)) break;          // a newer sound wins
            const Note& n = ph.n[i];
            if (n.hz) M5Cardputer.Speaker.tone(n.hz, n.ms, 0, true, wave, 16);
            vTaskDelay(pdMS_TO_TICKS(n.ms));
        }
    }
}

void soundBegin() {
    Preferences pr;
    pr.begin(PREFS_NS, true);
    enabled = pr.getBool("snd_on", true);
    volume = pr.getInt("snd_vol", 170);
    pr.end();
    queue = xQueueCreate(4, sizeof(uint8_t));
    xTaskCreatePinnedToCore(soundTask, "sound", 3072, nullptr, 2, nullptr, 0);
}

void playSfx(Sfx s) {
    if (!enabled || !queue || s >= SFX_COUNT) return;
    uint8_t id = s;
    xQueueSend(queue, &id, 0);                                // never wait
}

const char* soundPackName() { return pack().name; }

// The user chose tones in Breathe, so these play even when UI sounds are off.
void playBreathCue(int phase) {
    if (!queue || phase < 0 || phase > 2) return;
    uint8_t id = 200 + phase;
    xQueueSend(queue, &id, 0);
}

bool soundsOn() { return enabled; }
void setSoundsOn(bool on) {
    enabled = on;
    Preferences pr;
    pr.begin(PREFS_NS, false);
    pr.putBool("snd_on", on);
    pr.end();
}

int soundVolume() { return volume; }
int nextSoundVolume() {
    const int steps[] = {64, 128, 190, 255};
    int next = steps[0];
    for (int s : steps) if (s > volume) { next = s; break; }
    volume = next;
    Preferences pr;
    pr.begin(PREFS_NS, false);
    pr.putInt("snd_vol", volume);
    pr.end();
    return volume;
}

// After settings are loaded from the SD card.
void soundReloadSettings() {
    Preferences pr;
    pr.begin(PREFS_NS, true);
    enabled = pr.getBool("snd_on", true);
    volume = pr.getInt("snd_vol", 170);
    pr.end();
}
