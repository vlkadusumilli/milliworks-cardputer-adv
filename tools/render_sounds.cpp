// render_sounds.cpp - renders every milliWorks sound to a WAV file with the SAME code the
// Cardputer runs (src/focus_gen.h, src/sound_packs.h), then checks each one.
//
//   c++ -std=c++17 -O2 -I src tools/render_sounds.cpp -o /tmp/render_sounds && /tmp/render_sounds design/sounds
//
// UI sounds: the firmware calls M5.Speaker.tone(freq, ms, ch, true, wave16), which plays
// the 16-step wave table at freq * 16 steps a second. Here we step through the table the
// same way at 48 kHz (no smoothing), so the WAV has the same slightly buzzy edges.
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <algorithm>
#include "focus_gen.h"
#include "sound_packs.h"
#include "voice_builtin.h"

static void writeWav(const std::string& path, const std::vector<int16_t>& pcm, int rate) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { perror(path.c_str()); return; }
    uint32_t data = pcm.size() * 2, riff = 36 + data, fmt = 16, rate32 = rate, byteRate = rate * 2;
    uint16_t pcmFmt = 1, ch = 1, align = 2, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fmt, 4, 1, f); fwrite(&pcmFmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate32, 4, 1, f);
    fwrite(&byteRate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f); fwrite(pcm.data(), 2, pcm.size(), f);
    fclose(f);
}

struct Stats { double peakDb, rmsDb, dc; int clipped; double seconds; };
static Stats measure(const std::vector<int16_t>& pcm, int rate, size_t from = 0) {
    double peak = 0, sum = 0, sum2 = 0; int clip = 0; size_t n = 0;
    for (size_t i = from; i < pcm.size(); i++, n++) {
        double x = pcm[i] / 32768.0;
        peak = std::max(peak, std::fabs(x)); sum += x; sum2 += x * x;
        if (std::abs(pcm[i]) >= 32000) clip++;
    }
    double rms = n ? std::sqrt(sum2 / n) : 0;
    return {20 * std::log10(peak + 1e-9), 20 * std::log10(rms + 1e-9), n ? sum / n : 0, clip, pcm.size() / (double)rate};
}

static int problems = 0;
static void report(const std::string& name, const Stats& s, double minRmsDb, double maxPeakDb) {
    std::string flag;
    if (s.clipped) flag += " CLIPPED";
    if (s.rmsDb < minRmsDb) flag += " TOO-QUIET";
    if (s.peakDb > maxPeakDb) flag += " TOO-LOUD";
    if (std::fabs(s.dc) > 0.02) flag += " DC-OFFSET";
    if (!flag.empty()) problems++;
    printf("  %-34s %5.2fs  peak %6.1f dBFS  rms %6.1f dBFS  dc %+.3f %s\n",
           name.c_str(), s.seconds, s.peakDb, s.rmsDb, s.dc, flag.empty() ? " ok" : flag.c_str());
}

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "design/sounds";
    mkdir(dir.c_str(), 0755);
    const char* packIds[] = {"device", "daylight", "blackmask", "purplemask", "phosphor", "contrast"};
    const char* eventIds[] = {"move", "select", "back", "type", "success", "notice", "error", "startup"};

    // ---- UI sounds ----
    const int R = 48000;
    const int defaultVolume = 170;                                   // sound.cpp's default
    printf("UI sounds (6 packs x 8 moments), at the default volume:\n");
    for (int p = 0; p < PACK_COUNT; p++) {
        for (int e = 0; e < SFX_COUNT; e++) {
            const Phrase& ph = PACKS[p].ev[e];
            int vol = (int)(defaultVolume * PACKS[p].gain * EVENT_GAIN[e]);
            if (vol > 255) vol = 255;
            double level = vol / 255.0 * 0.9;
            std::vector<int16_t> pcm;
            for (int i = 0; i < ph.count; i++) {
                const Note& n = ph.n[i];
                int len = R * n.ms / 1000;
                double pos = 0, step = n.hz * 16.0 / R;               // wave-table steps per output sample
                for (int k = 0; k < len; k++) {
                    double v = 0;
                    if (n.hz) { v = (PACKS[p].wave[(int)pos & 15] - 128) / 128.0; pos += step; }
                    pcm.push_back((int16_t)std::lround(v * level * 32000));
                }
            }
            for (int k = 0; k < R / 50; k++) pcm.push_back(0);       // 20 ms of silence at the end
            std::string name = std::string("sfx_") + packIds[p] + "_" + eventIds[e];
            writeWav(dir + "/" + name + ".wav", pcm, R);
            report(name, measure(pcm, R), e == SFX_MOVE || e == SFX_TYPE ? -40 : -30, -0.5);
        }
    }

    // ---- are the packs about equally loud? (the 'select' sound of each) ----
    printf("\nLoudness of each pack's Select sound (should be within about 6 dB of each other):\n");
    double lo = 0, hi = -200;
    for (int p = 0; p < PACK_COUNT; p++) {
        FILE* f = fopen((dir + "/sfx_" + packIds[p] + "_select.wav").c_str(), "rb");
        std::vector<int16_t> pcm; if (f) { fseek(f, 44, SEEK_SET); int16_t v; while (fread(&v, 2, 1, f)) pcm.push_back(v); fclose(f); }
        double db = measure(pcm, 48000).rmsDb; lo = std::min(lo, db); hi = std::max(hi, db);
        printf("  %-12s %6.1f dBFS\n", packIds[p], db);
    }
    printf("  spread: %.1f dB %s\n", hi - lo, hi - lo <= 6.5 ? "ok" : "UNEVEN");
    if (hi - lo > 6.5) problems++;

    // ---- focus sounds: 12 s each, seamless loop ----
    printf("\nFocus sounds (Flow), 12 s loops:\n");
    const char* focusIds[] = {"brown", "rain", "drone", "tick"};
    const int FR = FocusGen::RATE, LEN = FR * 12, XF = FR / 2;      // last 0.5 s fades into the start
    for (int s = 0; s < FS_SILENCE; s++) {
        FocusGen gen;
        std::vector<float> raw(LEN + XF);
        for (auto& v : raw) v = gen.sample(s);
        // Device check: the firmware fades in from silence, so measure after 2 s.
        std::vector<int16_t> pcm(LEN);
        for (int i = 0; i < LEN; i++) {
            float v = raw[i];
            if (i < XF) { float t = (float)i / XF; v = raw[i] * t + raw[LEN + i] * (1 - t); }   // loop seam
            pcm[i] = focusToPcm(v);
        }
        std::string name = std::string("focus_") + focusIds[s];
        writeWav(dir + "/" + name + ".wav", pcm, FR);
        Stats st = measure(pcm, FR, FR * 2);
        report(name, st, s == FS_TICK ? -45 : -32, -0.5);
        if (s == FS_TICK) {                                           // exactly one tick a second?
            int ticks = 0; bool in = false;
            for (int i = 0; i < LEN; i++) { bool loud = std::abs(pcm[i]) > 2000; if (loud && !in) ticks++; in = loud ? true : (i % 400 ? in : false); }
            printf("    ticks in 12 s: %d %s\n", ticks, ticks == 12 ? "ok" : "CHECK");
            if (ticks != 12) problems++;
        }
        if (s == FS_RAIN) {                                           // some drops, not a downpour
            FocusGen g; int drops = 0; float prevAmp = 0;
            for (int i = 0; i < FR * 60; i++) { g.sample(FS_RAIN); if (g.dropAmp > 0.2f && prevAmp < 0.001f) drops++; prevAmp = g.dropAmp; }
            printf("    raindrops per minute: %d %s\n", drops, drops >= 20 && drops <= 200 ? "ok" : "CHECK");
            if (drops < 20 || drops > 200) problems++;
        }
    }
    // ---- Breathe app music: two Calm cycles, steered exactly like app_breathe.cpp's musicCue() ----
    {
        printf("\nBreathe music (Calm: in 4, hold 4, out 6), 2 cycles:\n");
        FocusGen g;
        std::vector<int16_t> pcm;
        g.breathSet(196.0f, 0.06f, 0.5f);
        struct Ph { int idx; float secs; } cyc[] = {{0, 4}, {1, 4}, {2, 6}};
        float gain = 0;
        for (int c = 0; c < 2; c++) for (auto ph : cyc) {
            switch (ph.idx) {
                case 0: g.breathSet(293.7f, 0.50f, ph.secs); break;
                case 1: g.breathBell(880.0f); g.breathSet(293.7f, 0.36f, 1.0f); break;
                default: g.breathBell(659.3f); g.breathSet(196.0f, 0.035f, ph.secs); break;
            }
            for (int i = 0; i < (int)(ph.secs * FR); i++) { gain += (1 - gain) * 0.0004f; pcm.push_back(focusToPcm(g.sample(FS_BREATH) * gain)); }
        }
        writeWav(dir + "/focus_breath.wav", pcm, FR);
        report("focus_breath", measure(pcm, FR), -36, -0.5);
        // The in-breath should get louder and the out-breath quieter.
        auto rmsAt = [&](float from, float to) { double s2 = 0; int n = 0; for (int i = (int)(from * FR); i < (int)(to * FR); i++, n++) s2 += (pcm[i] / 32768.0) * (pcm[i] / 32768.0); return 20 * std::log10(std::sqrt(s2 / n) + 1e-9); };
        double inStart = rmsAt(14.2f, 15.0f), inEnd = rmsAt(17.2f, 18.0f), outEnd = rmsAt(27.0f, 28.0f);
        printf("    in-breath swells %.1f -> %.1f dBFS, out-breath fades to %.1f dBFS %s\n", inStart, inEnd, outEnd,
               inEnd > inStart + 6 && outEnd < inEnd - 10 ? "ok" : "CHECK");
        if (!(inEnd > inStart + 6 && outEnd < inEnd - 10)) problems++;
    }

    // ---- Breathe guidance: voice clips (as built into the firmware) and tone cues ----
    {
        printf("\nBreathe guidance - voices on the SD card (design/sd/milliWorks/breathe/voices):\n");
        const char* keys[] = {"in", "hold", "out", "2", "3", "4", "5", "6", "7", "8"};
        double lo = 0, hi = -99;                            // quietest and loudest clip (rms)
        for (std::string voice : {"luna", "julian"})
        for (auto k : keys) {
            std::string path = "design/sd/milliWorks/breathe/voices/" + voice + "/" + k + ".wav";
            FILE* f = fopen(path.c_str(), "rb");
            std::vector<int16_t> pcm;
            if (f) { fseek(f, 44, SEEK_SET); int16_t v; while (fread(&v, 2, 1, f)) pcm.push_back(v); fclose(f); }
            if (pcm.empty()) { printf("  %s missing CHECK\n", path.c_str()); problems++; continue; }
            Stats st = measure(pcm, 16000);
            report(voice + " " + k, st, -30, -0.5);
            // Loudness of the spoken part (20 ms windows above a quarter of the loudest), the
            // same measure tools/import_voice.py levels to: quiet consonants don't count.
            std::vector<double> w;
            for (size_t i = 0; i + 320 <= pcm.size(); i += 320) {
                double e = 0; for (size_t j = i; j < i + 320; j++) e += (double)pcm[j] * pcm[j];
                w.push_back(std::sqrt(e / 320));
            }
            double mx = *std::max_element(w.begin(), w.end()), e = 0; int n = 0;
            for (double r : w) if (r > mx * 0.25) { e += r * r; n++; }
            double spokenDb = 20 * std::log10(std::sqrt(e / n) / 32768);
            lo = std::min(lo, spokenDb); hi = std::max(hi, spokenDb);
            if (pcm.size() > 16000 * 1.0) { printf("    longer than 1 s: would run into the next count CHECK\n"); problems++; }
            if (std::abs(pcm.front()) > 300 || std::abs(pcm.back()) > 300) { printf("    starts/ends abruptly CHECK\n"); problems++; }
        }
        printf("    spoken loudness spread between clips: %.1f dB %s\n", hi - lo, hi - lo <= 6 ? "ok" : "CHECK");
        if (hi - lo > 6) problems++;
        printf("\nBreathe guidance - built-in fallback voice (6 kHz 8-bit, in the firmware):\n");
        struct { const char* name; const uint8_t* d; size_t n; } v[] = {
            {"builtin voice in", VOICE_BUILTIN_IN, sizeof(VOICE_BUILTIN_IN)}, {"builtin voice hold", VOICE_BUILTIN_HOLD, sizeof(VOICE_BUILTIN_HOLD)},
            {"builtin voice out", VOICE_BUILTIN_OUT, sizeof(VOICE_BUILTIN_OUT)}};
        size_t total = 0;
        for (auto& c : v) {
            std::vector<int16_t> pcm; for (size_t i = 0; i < c.n; i++) pcm.push_back((int16_t)((c.d[i] - 128) * 256));
            report(c.name, measure(pcm, VOICE_BUILTIN_RATE), -30, -2.0);
            total += c.n;
        }
        printf("    built-in voice size: %zu KB\n", total / 1024);
        printf("\nBreathe guidance - tones (same in every theme):\n");
        const char* names[] = {"breath_cue_in", "breath_cue_hold", "breath_cue_out"};
        for (int k = 0; k < 3; k++) {
            const Phrase& ph = BREATH_CUES[k];
            std::vector<int16_t> pcm;
            double level = 170 / 255.0 * 0.9;
            for (int i = 0; i < ph.count; i++) {
                const Note& n = ph.n[i];
                double pos = 0, step = n.hz * 16.0 / R;
                for (int j = 0; j < R * n.ms / 1000; j++) {
                    double x = 0; if (n.hz) { x = (WAVE_TRIANGLE[(int)pos & 15] - 128) / 128.0; pos += step; }
                    pcm.push_back((int16_t)std::lround(x * level * 32000));
                }
            }
            for (int j = 0; j < R / 50; j++) pcm.push_back(0);
            writeWav(dir + "/" + names[k] + ".wav", pcm, R);
            report(names[k], measure(pcm, R), -30, -0.5);
        }
        // Can you tell them apart with your eyes closed? in must rise, out must fall, hold must be two equal taps.
        auto notes = [](const Phrase& ph) { std::vector<int> f; for (int i = 0; i < ph.count; i++) if (ph.n[i].hz) f.push_back(ph.n[i].hz); return f; };
        auto in = notes(BREATH_CUES[0]), hold = notes(BREATH_CUES[1]), out = notes(BREATH_CUES[2]);
        bool rises = in.size() >= 2 && std::is_sorted(in.begin(), in.end()) && in.front() < in.back();
        bool falls = out.size() >= 2 && std::is_sorted(out.rbegin(), out.rend()) && out.front() > out.back();
        bool taps = hold.size() == 2 && hold[0] == hold[1] && BREATH_CUES[1].n[1].hz == 0;
        printf("    in rises: %s   hold = two equal taps: %s   out falls: %s\n", rises ? "ok" : "NO", taps ? "ok" : "NO", falls ? "ok" : "NO");
        if (!(rises && falls && taps)) problems++;
    }

    printf("\n%s\n", problems ? "PROBLEMS FOUND" : "All sounds rendered and checked: no problems.");
    return problems ? 1 : 0;
}
