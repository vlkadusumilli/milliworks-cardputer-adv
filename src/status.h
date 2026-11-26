// status.h - your "do not disturb" status, shown by the RGB LED (and the Status app's sign).
#pragma once
#include <stdint.h>

enum StatusKind : uint8_t { ST_OFF, ST_DND, ST_FOCUS, ST_MEETING, ST_FREE, ST_COUNT };

void statusBegin();                    // call once in setup()
void setStatus(StatusKind s);          // LED colour (+ breathing for Do not disturb)
StatusKind currentStatus();
const char* statusName(StatusKind s);
void statusLedRgb(StatusKind s, uint8_t& r, uint8_t& g, uint8_t& b);

// For the Breathe app: drive the LED directly (0..1 brightness). Pass < 0 to give it back.
void statusLedOverride(float level, uint8_t r = 60, uint8_t g = 140, uint8_t b = 255);

void statusService();                  // keeps the LED breathing; called from serviceTick()
