// status.cpp - the RGB LED as a status light. On the Cardputer ADV it sits on the
// StampS3A module (GPIO 21); M5Unified drives it as M5.Led.
#include <M5Cardputer.h>
#include <math.h>
#include "status.h"

static StatusKind status = ST_OFF;
static float overrideLevel = -1;
static uint8_t ovR, ovG, ovB;
static uint32_t lastLed = 0;

static const char* NAMES[ST_COUNT] = {"Off", "Do not disturb", "Focusing", "In a meeting", "Free, come in"};
const char* statusName(StatusKind s) { return NAMES[s < ST_COUNT ? s : 0]; }

void statusLedRgb(StatusKind s, uint8_t& r, uint8_t& g, uint8_t& b) {
    switch (s) {
        case ST_DND:     r = 255; g = 40;  b = 30;  break;   // red
        case ST_FOCUS:   r = 255; g = 150; b = 20;  break;   // amber
        case ST_MEETING: r = 40;  g = 110; b = 255; break;   // blue
        case ST_FREE:    r = 40;  g = 210; b = 80;  break;   // green
        default:         r = g = b = 0;
    }
}

static void showLed(float level, uint8_t r, uint8_t g, uint8_t b) {
    if (!M5.Led.isEnabled()) return;
    level = constrain(level, 0.0f, 1.0f);
    M5.Led.setAllColor((uint8_t)(r * level), (uint8_t)(g * level), (uint8_t)(b * level));
    M5.Led.display();
}

void statusBegin() {
    if (M5.Led.isEnabled()) M5.Led.setBrightness(80);   // gentle: it's a status light, not a torch
    showLed(0, 0, 0, 0);
}

void setStatus(StatusKind s) { status = s; lastLed = 0; }
StatusKind currentStatus() { return status; }

void statusLedOverride(float level, uint8_t r, uint8_t g, uint8_t b) {
    overrideLevel = level; ovR = r; ovG = g; ovB = b;
    if (level >= 0) showLed(level, r, g, b);
    lastLed = 0;
}

void statusService() {
    if (overrideLevel >= 0) return;                    // the Breathe app is in charge
    uint32_t now = millis();
    if (now - lastLed < 40) return;                    // ~25 updates a second is plenty
    lastLed = now;
    uint8_t r, g, b;
    statusLedRgb(status, r, g, b);
    float level = 1.0f;
    if (status == ST_DND) level = 0.25f + 0.75f * (0.5f - 0.5f * cosf(2 * PI * (now % 3200) / 3200.0f));  // slow breathing
    showLed(status == ST_OFF ? 0 : level, r, g, b);
}
