// voice.cpp - loads the Breathe voice from the SD card. The clips are read once when
// Breathe opens (about 20 KB each) and freed when it closes, so they cost no firmware
// space and only borrow memory while needed.
#include <M5Cardputer.h>
#include <Preferences.h>
#include <SD.h>
#include "voice.h"
#include "config.h"
#include "sound.h"
#include "voice_builtin.h"
#include "wav_parse.h"

bool sdMount();                                     // app_sd.cpp

static const char* DIR = VOICE_DIR;
static const char* FILES[3] = {"in.wav", "hold.wav", "out.wav"};
static const uint32_t MAX_SAMPLES = 16000 * 3;      // at most ~3 s per clip, to protect memory

static int16_t* clip[3] = {nullptr, nullptr, nullptr};
static String loadedName;                          // the SD voice in memory ("" = none)
static int16_t* numClip = nullptr;                 // the number being said (one at a time)
static uint32_t numLen = 0, numRate = 0;
static uint32_t clipLen[3] = {0, 0, 0}, clipRate[3] = {0, 0, 0};

const char* VOICE_BUILTIN_NAME = "Built-in";

std::vector<String> voiceList() {
    std::vector<String> out;
    out.push_back(VOICE_BUILTIN_NAME);               // always there, even without a card
    if (!sdMount()) return out;
    File dir = SD.open(DIR);
    if (!dir) return out;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        if (f.isDirectory()) {
            String name = f.name();
            int slash = name.lastIndexOf('/');
            if (slash >= 0) name = name.substring(slash + 1);
            if (!name.startsWith(".")) out.push_back(name);
        }
        f.close();
    }
    dir.close();
    std::sort(out.begin() + 1, out.end());
    return out;
}

String voiceSaved() {
    Preferences p;
    p.begin(PREFS_NS, true);
    String v = p.getString("voice", "");
    p.end();
    return v;
}

String voiceChosen() {
    std::vector<String> list = voiceList();
    String want = voiceSaved();
    for (auto& v : list) if (v == want) return v;
    for (auto& v : list) if (v == VOICE_DEFAULT) return v;
    return list.size() > 1 ? list[1] : String(VOICE_BUILTIN_NAME);
}

void voiceSetSaved(const String& name) {
    Preferences p;
    p.begin(PREFS_NS, false);
    p.putString("voice", name);
    p.end();
}

// Reads a WAV file: the header is checked by wav_parse.h (shared with the Mac tests),
// then the samples are read; stereo is mixed down to mono.
static uint8_t header[4096];
static bool loadWav(const String& path, int16_t*& data, uint32_t& len, uint32_t& rate) {
    File f = SD.open(path);
    if (!f) { Serial.printf("voice: can't open %s\n", path.c_str()); return false; }
    size_t n = f.read(header, sizeof(header));
    WavInfo w;
    if (!parseWavHeader(header, n, w)) { f.close(); Serial.printf("voice: not a 16-bit WAV: %s\n", path.c_str()); return false; }
    len = min((uint32_t)(w.dataBytes / (2 * w.channels)), MAX_SAMPLES);
    rate = w.rate;
    data = (int16_t*)malloc(len * 2);
    if (!data) { f.close(); Serial.printf("voice: no memory for %s\n", path.c_str()); return false; }
    f.seek(w.dataOffset);
    for (uint32_t i = 0; i < len; i++) {
        int16_t s = 0, r = 0;
        if (f.read((uint8_t*)&s, 2) != 2) { len = i; break; }       // file shorter than it says
        if (w.channels == 2) { f.read((uint8_t*)&r, 2); s = s / 2 + r / 2; }
        data[i] = s;
    }
    f.close();
    return len > 0;
}

void voiceUnload() {
    M5Cardputer.Speaker.stop(2);                       // nothing may still be playing from these buffers
    for (int i = 0; i < 3; i++) { free(clip[i]); clip[i] = nullptr; clipLen[i] = 0; }
    free(numClip); numClip = nullptr; numLen = 0;
    loadedName = "";
}

bool voiceLoaded() { return clip[0] && clip[1] && clip[2]; }

bool voiceLoad(const String& name) {
    voiceUnload();
    if (name == VOICE_BUILTIN_NAME || !sdMount()) return false;
    for (int i = 0; i < 3; i++) {
        if (!loadWav(String(DIR) + "/" + name + "/" + FILES[i], clip[i], clipLen[i], clipRate[i])) {
            voiceUnload();
            return false;
        }
    }
    loadedName = name;
    return true;
}

bool voiceBusy() { return M5Cardputer.Speaker.isPlaying(2); }

bool voiceCount(int n) {
    if (!voiceLoaded() || n < 2 || n > 8) return false;   // the built-in voice has no numbers
    M5Cardputer.Speaker.stop(2);                         // the old number's memory is reused
    free(numClip); numClip = nullptr; numLen = 0;
    if (!loadWav(String(DIR) + "/" + loadedName + "/" + n + ".wav", numClip, numLen, numRate)) return false;
    M5Cardputer.Speaker.setChannelVolume(2, soundVolume());
    return M5Cardputer.Speaker.playRaw(numClip, numLen, numRate, false, 1, 2, true);
}

uint32_t voiceCountMillis() { return numRate ? numLen * 1000 / numRate : 0; }

// The SD voice if it's loaded, otherwise the small built-in one: voice guidance always works.
static const uint8_t* BUILTIN[3] = {VOICE_BUILTIN_IN, VOICE_BUILTIN_HOLD, VOICE_BUILTIN_OUT};
static const uint32_t BUILTIN_LEN[3] = {sizeof(VOICE_BUILTIN_IN), sizeof(VOICE_BUILTIN_HOLD), sizeof(VOICE_BUILTIN_OUT)};

bool voicePlay(int phase) {
    if (phase < 0 || phase > 2) return false;
    M5Cardputer.Speaker.setChannelVolume(2, soundVolume());
    if (voiceLoaded()) return M5Cardputer.Speaker.playRaw(clip[phase], clipLen[phase], clipRate[phase], false, 1, 2, true);
    return M5Cardputer.Speaker.playRaw(BUILTIN[phase], BUILTIN_LEN[phase], VOICE_BUILTIN_RATE, false, 1, 2, true);
}

uint32_t voiceMillis(int phase) {
    if (phase < 0 || phase > 2) return 0;
    if (voiceLoaded() && clipRate[phase]) return clipLen[phase] * 1000 / clipRate[phase];
    return BUILTIN_LEN[phase] * 1000 / VOICE_BUILTIN_RATE;
}
