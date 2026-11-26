// voice.h - spoken Breathe cues read from the SD card:
//   /milliWorks/breathe/voices/<name>/in.wav, hold.wav, out.wav, 2.wav .. 8.wav (16-bit WAV)
// Each folder is a voice; copy in another voice, language or your own recordings.
#pragma once
#include <Arduino.h>
#include <vector>

extern const char* VOICE_BUILTIN_NAME; // "Built-in": the small voice inside the firmware
std::vector<String> voiceList();      // "Built-in" first, then the folders found on the card
String voiceSaved();                  // what was picked in Settings ("" = nothing yet)
String voiceChosen();                 // the voice to use: the pick if it's on the card, else
                                      // VOICE_DEFAULT, else the first voice there, else Built-in
void voiceSetSaved(const String& name);

bool voiceLoad(const String& name);   // reads an SD voice into memory; false = the built-in one plays
void voiceUnload();                   // frees the memory again
bool voiceLoaded();
bool voicePlay(int phase);            // 0 = "Breathe in", 1 = "Hold", 2 = "Breathe out" (SD or built-in)
uint32_t voiceMillis(int phase);      // length of a clip
// Counting: 2.wav .. 8.wav in the same folder say "two" .. "eight". Read from the card when
// needed (one at a time, so they cost little memory). false = this voice has no numbers.
bool voiceCount(int n);
bool voiceBusy();                     // still speaking?
uint32_t voiceCountMillis();          // length of the number just started
