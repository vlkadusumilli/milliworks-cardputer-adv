// focus_gen.h - the focus-sound generators as plain C++ (no Arduino), so the same code
// runs on the Cardputer and in tools/render_sounds.cpp on a computer, which renders
// every sound to a WAV file and checks it.
#pragma once
#include <math.h>
#include <stdint.h>

enum FocusSound { FS_BROWN, FS_RAIN, FS_DRONE, FS_TICK, FS_SILENCE, FS_COUNT };
// Not a Flow choice: the Breathe app's music, steered by breathSet() / breathBell().
static constexpr int FS_BREATH = 100;

struct FocusGen {
    static constexpr int RATE = 16000;            // samples per second
    static constexpr float kTwoPi = 6.28318530718f;

    uint32_t rng = 0x12345678;
    float brownLast = 0, pinkB0 = 0, pinkB1 = 0;
    float dropPhase = 0, dropFreq = 0, dropAmp = 0;
    float ph1 = 0, ph2 = 0, ph3 = 0, lfo = 0;
    uint32_t tickPos = 0;
    float hpPrev = 0;

    // Breathing music: a soft chord (root + fifth) that glides to a target pitch and level,
    // so it rises as you breathe in and falls as you breathe out. Plus a bell for cues.
    float bFreq = 220, bAmp = 0, bTargetFreq = 220, bTargetAmp = 0, bRate = 0.0005f;
    float bPh1 = 0, bPh2 = 0, bellPh = 0, bellFreq = 880, bellAmp = 0;
    void breathSet(float freq, float amp, float seconds) {   // glide over about this long
        bTargetFreq = freq; bTargetAmp = amp;
        float n = seconds * RATE / 3.0f;                       // ~95% of the way by the end
        bRate = n > 1 ? 1.0f / n : 1.0f;
    }
    void breathBell(float freq) { bellFreq = freq; bellAmp = 0.35f; bellPh = 0; }

    inline float white() {                        // fast random numbers, -1..1
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (int32_t)rng / 2147483648.0f;
    }

    // One sample, roughly -1..1.
    float sample(int s) {
        switch (s) {
            case FS_BROWN:                          // deep and even: integrated noise
                brownLast = (brownLast + 0.02f * white()) / 1.02f;
                return brownLast * 3.2f;
            case FS_RAIN: {                         // soft noise plus the odd drop
                float w = white();
                pinkB0 = 0.97f * pinkB0 + w * 0.03f;
                pinkB1 = 0.6f * pinkB1 + w * 0.4f;
                float v = pinkB0 * 2.2f + (pinkB1 - pinkB0) * 0.25f;
                if (dropAmp < 0.001f && (rng & 0x3FFF) == 7) { dropFreq = 1800 + (rng % 2200); dropAmp = 0.25f; dropPhase = 0; }
                if (dropAmp > 0.001f) {
                    v += sinf(dropPhase) * dropAmp;
                    dropPhase += kTwoPi * dropFreq / RATE;
                    if (dropPhase > kTwoPi) dropPhase -= kTwoPi;
                    dropAmp *= 0.992f;
                }
                return v;
            }
            case FS_DRONE: {                        // three low tones that breathe slowly
                ph1 += kTwoPi * 110.0f / RATE; ph2 += kTwoPi * 164.8f / RATE; ph3 += kTwoPi * 220.5f / RATE;
                lfo += kTwoPi * 0.08f / RATE;
                if (ph1 > kTwoPi) ph1 -= kTwoPi;
                if (ph2 > kTwoPi) ph2 -= kTwoPi;
                if (ph3 > kTwoPi) ph3 -= kTwoPi;
                if (lfo > kTwoPi) lfo -= kTwoPi;
                return (sinf(ph1) * 0.4f + sinf(ph2) * 0.4f + sinf(ph3) * 0.15f) * (0.55f + 0.25f * sinf(lfo));
            }
            case FS_TICK: {                         // one quiet tick a second
                float v = 0;
                if (tickPos < RATE * 0.012f) v = white() * expf(-(float)tickPos / (RATE * 0.002f)) * 0.9f;
                if (++tickPos >= (uint32_t)RATE) tickPos = 0;
                float hp = v - hpPrev; hpPrev = v;  // keep it a crisp click, not a thump
                return hp;
            }
            case FS_BREATH: {
                bFreq += (bTargetFreq - bFreq) * bRate;
                bAmp += (bTargetAmp - bAmp) * bRate;
                bPh1 += kTwoPi * bFreq / RATE;        if (bPh1 > kTwoPi) bPh1 -= kTwoPi;
                bPh2 += kTwoPi * bFreq * 1.5f / RATE; if (bPh2 > kTwoPi) bPh2 -= kTwoPi;
                float v = (sinf(bPh1) * 0.6f + sinf(bPh2) * 0.3f) * bAmp;
                if (bellAmp > 0.0005f) {                // a soft bell: a sine that rings out
                    bellPh += kTwoPi * bellFreq / RATE; if (bellPh > kTwoPi) bellPh -= kTwoPi;
                    v += (sinf(bellPh) + 0.3f * sinf(bellPh * 2.0f)) * bellAmp;
                    bellAmp *= 0.99975f;                // rings for about a second
                }
                return v;
            }
            default: return 0;
        }
    }
};

// The level the speaker gets: sample * 9000, kept inside 16-bit range.
inline int16_t focusToPcm(float v) {
    float x = v * 9000.0f;
    if (x > 32000.0f) x = 32000.0f;
    if (x < -32000.0f) x = -32000.0f;
    return (int16_t)x;
}
