// focus_sound.h - background sounds for the Flow app, generated live (no files needed).
#pragma once

#include "focus_gen.h"   // FocusSound: FS_BROWN ... FS_COUNT

const char* focusSoundName(int s);
void focusSoundStart(int s);       // start (or switch to) a sound
void focusSoundStop(bool fade);    // stop, optionally fading out over ~1.5 s
void focusSoundSetMuted(bool m);   // instant, keeps the sound running underneath
bool focusSoundMuted();
bool focusSoundPlaying();

// Breathe app: start with focusSoundStart(FS_BREATH), then steer it each phase.
void focusBreathSet(float freq, float amp, float seconds);   // glide the chord there over `seconds`
void focusBreathBell(float freq);                            // a soft bell as a cue
void focusSoundDuck(bool duck);                              // music quieter while the voice speaks
