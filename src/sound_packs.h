// sound_packs.h - the UI sound packs as plain data (no Arduino), shared by the firmware
// (sound.cpp) and tools/render_sounds.cpp, which renders them to WAV files to check them.
#pragma once
#include <stdint.h>
#include "sound.h"

// ---- waveforms: one cycle of 16 samples, 128 = silence ---------------------------
// Every table starts at 128 (silence) so a note begins without a click.
static const uint8_t WAVE_SINE[16]     = {128, 177, 219, 246, 255, 246, 219, 177, 128, 79, 37, 10, 1, 10, 37, 79};
static const uint8_t WAVE_TRIANGLE[16] = {128, 160, 192, 224, 255, 224, 192, 160, 128, 96, 64, 32, 1, 32, 64, 96};
static const uint8_t WAVE_SQUARE[16]   = {200, 200, 200, 200, 200, 200, 200, 200, 56, 56, 56, 56, 56, 56, 56, 56};
static const uint8_t WAVE_PULSE[16]    = {236, 236, 236, 236, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92};   // centred: no DC offset

struct Note { uint16_t hz, ms; };            // hz 0 = a short rest
struct Phrase { uint8_t count; Note n[5]; };
struct Pack { const char* name; const uint8_t* wave; float gain; Phrase ev[SFX_COUNT]; };

// Order of ev[]: move, select, back, type, success, notice, error, startup.
// Order of packs = order of THEMES in ui.cpp.
static const Pack PACKS[] = {
  {"Copper chime", WAVE_TRIANGLE, 1.0f, {
    {1, {{2093, 7}}}, {2, {{1568, 28}, {2093, 40}}}, {2, {{2093, 28}, {1568, 40}}}, {1, {{3136, 5}}},
    {3, {{1319, 55}, {1568, 55}, {2093, 120}}}, {3, {{1568, 60}, {0, 40}, {1568, 60}}}, {2, {{659, 90}, {523, 170}}},
    {4, {{1047, 70}, {1568, 70}, {2093, 70}, {2637, 180}}}}},
  {"Glass", WAVE_SINE, 0.9f, {
    {1, {{2794, 6}}}, {2, {{1760, 26}, {2794, 38}}}, {2, {{2794, 26}, {1760, 38}}}, {1, {{3520, 5}}},
    {3, {{1397, 50}, {1760, 50}, {2794, 110}}}, {3, {{1760, 55}, {0, 35}, {1760, 55}}}, {2, {{880, 90}, {698, 170}}},
    {4, {{1397, 60}, {1760, 60}, {2093, 60}, {2794, 170}}}}},
  {"Gold", WAVE_SINE, 1.1f, {
    {1, {{1047, 8}}}, {2, {{784, 40}, {1047, 60}}}, {2, {{1047, 40}, {784, 60}}}, {1, {{1568, 6}}},
    {4, {{523, 80}, {659, 80}, {784, 80}, {1047, 180}}}, {3, {{784, 90}, {0, 50}, {784, 90}}}, {2, {{392, 110}, {330, 220}}},
    {4, {{262, 110}, {392, 110}, {523, 110}, {784, 260}}}}},
  {"Arcade", WAVE_PULSE, 1.0f, {
    {1, {{2349, 6}}}, {2, {{1480, 24}, {2349, 32}}}, {2, {{2349, 24}, {1480, 32}}}, {1, {{2960, 5}}},
    {4, {{1175, 40}, {1480, 40}, {1760, 40}, {2349, 100}}}, {3, {{1976, 45}, {0, 30}, {1976, 45}}}, {3, {{740, 70}, {587, 70}, {440, 140}}},
    {5, {{1175, 50}, {1319, 50}, {1480, 50}, {1760, 50}, {2349, 150}}}}},
  {"Terminal", WAVE_SQUARE, 1.0f, {
    {1, {{1200, 8}}}, {1, {{1200, 40}}}, {1, {{900, 40}}}, {1, {{1800, 4}}},
    {3, {{1200, 60}, {0, 30}, {1200, 60}}}, {1, {{1200, 150}}}, {1, {{220, 260}}},
    {3, {{1200, 120}, {0, 60}, {1200, 40}}}}},
  {"Clear", WAVE_SINE, 1.3f, {
    {1, {{1568, 10}}}, {2, {{1047, 40}, {2093, 60}}}, {2, {{2093, 40}, {1047, 60}}}, {1, {{2637, 6}}},
    {3, {{1047, 70}, {1568, 70}, {2093, 160}}}, {3, {{1568, 80}, {0, 60}, {1568, 80}}}, {2, {{523, 120}, {262, 220}}},
    {3, {{523, 90}, {1047, 90}, {2093, 200}}}}},
};
static const int PACK_COUNT = sizeof(PACKS) / sizeof(PACKS[0]);
static const float EVENT_GAIN[SFX_COUNT] = {0.35f, 1, 1, 0.25f, 1, 1, 1, 1};   // ticks are quieter


// ---- Breathe app tone cues: three shapes you can tell apart with your eyes closed ----
//   in   - three notes going UP      (G5  B5  D6)
//   hold - two soft TAPS, same note  (A5 .. A5)
//   out  - three notes going DOWN    (D6  B5  G5)
static const Phrase BREATH_CUES[3] = {
    {3, {{784, 110}, {988, 110}, {1175, 200}}},
    {3, {{880, 80}, {0, 110}, {880, 80}}},
    {3, {{1175, 110}, {988, 110}, {784, 200}}},
};
