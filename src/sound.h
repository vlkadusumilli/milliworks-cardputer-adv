// sound.h - UI sounds. Each theme has its own sound pack (same moments, different
// notes and waveform), as previewed in design/cardputer-preview.html.
#pragma once
#include <stdint.h>

enum Sfx : uint8_t { SFX_MOVE, SFX_SELECT, SFX_BACK, SFX_TYPE, SFX_SUCCESS, SFX_NOTICE, SFX_ERROR, SFX_STARTUP, SFX_COUNT };

void soundBegin();              // start the background sound player (call once in setup)
void playSfx(Sfx s);            // queue a sound; returns at once
const char* soundPackName();    // the current theme's pack, e.g. "Copper chime"
void playBreathCue(int phase);  // Breathe app: 0 = in (up), 1 = hold (taps), 2 = out (down)

bool soundsOn();                // remembered in flash
void setSoundsOn(bool on);
int  soundVolume();             // 0..255, remembered
int  nextSoundVolume();         // 25% -> 50% -> 75% -> 100%
