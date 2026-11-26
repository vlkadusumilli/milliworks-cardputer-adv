// app_settings.cpp - Settings: theme, brightness, screen flip, about.
#include "ui.h"
#include "apps.h"
#include "config.h"
#include "sound.h"
#include "voice.h"

// Theme picker: moving through the list re-colours the whole screen right away,
// so you see each theme for real. OK keeps it, ESC goes back to the old one.
static void chooseTheme() {
    const int before = savedTheme();
    int sel = before, top = 0;
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        if (in.back) { applyTheme(before); return; }
        // Each theme also brings its own sounds: play a sample of the new pack.
        if (in.up)   { sel = (sel + THEME_COUNT - 1) % THEME_COUNT; applyTheme(sel); playSfx(SFX_SELECT); }
        if (in.down) { sel = (sel + 1) % THEME_COUNT; applyTheme(sel); playSfx(SFX_SELECT); }
        if (in.enter) {
            saveTheme(sel);
            toast(THEMES[sel].name, col::good, 800);
            return;
        }

        const int rows = 5;
        if (sel < top) top = sel;
        if (sel >= top + rows) top = sel - rows + 1;
        drawHeader("Theme");
        std::vector<String> names;
        for (int i = 0; i < THEME_COUNT; i++) names.push_back(THEMES[i].name);
        drawList(names, sel, top, rows);
        // A strip of each theme's own colours at the right of its row:
        // ground, panel, accent, network, ok.
        for (int r = 0; r < rows && top + r < THEME_COUNT; r++) {
            const uint16_t* c = THEMES[top + r].c;
            const int idx[] = {0, 1, 5, 7, 9};
            int y = HEADER_H + 4 + r * 18 + 4;
            for (int k = 0; k < 5; k++) {
                int x = SCREEN_W - 70 + k * 12;
                canvas.fillRect(x, y, 10, 9, c[idx[k]]);
                canvas.drawRect(x, y, 10, 9, col::edge);
            }
        }
        drawHints({{ARROWS_UD, "try"}, {"OK", "keep"}, {"ESC", "cancel"}});
        pushScreen();
        delay(15);
    }
}

static void about() {
    while (true) {
        M5Cardputer.update();
        if (readInput().back) return;
        drawHeader("About");
        canvas.fillRoundRect(4, HEADER_H + 6, SCREEN_W - 8, 84, 6, col::panel);
        canvas.setTextDatum(top_left);
        canvas.setFont(&fonts::Font4);
        canvas.setTextColor(col::text);
        canvas.drawString(FW_NAME, 12, HEADER_H + 12);
        canvas.setFont(&fonts::Font2);
        canvas.setTextColor(col::accent);
        canvas.setTextDatum(top_right);
        canvas.drawString("by " FW_AUTHOR, SCREEN_W - 12, HEADER_H + 18);
        canvas.setTextDatum(top_left);
        canvas.setTextColor(col::dim);
        drawTextFit(String("Version ") + FW_VERSION + "  for " + deviceName(), 12, HEADER_H + 42, SCREEN_W - 24,
                    &fonts::Font2, &fonts::Font0);           // a test build reads e.g. "19-beta.1"
        canvas.setFont(&fonts::Font2);
        canvas.drawString(String("Theme: ") + THEMES[savedTheme()].name, 12, HEADER_H + 58);
        canvas.drawString(String("Sounds: ") + (soundsOn() ? soundPackName() : "off") +
                          (clockHasRtc() ? "   RTC: yes" : "   RTC: none"), 12, HEADER_H + 74);
        drawHints({{"ESC", "back"}});
        pushScreen();
        delay(30);
    }
}

// The Breathe voice: "Built-in" or a folder from /milliWorks/breathe/voices/ on the SD card.
static void chooseVoice() {
    std::vector<String> voices = voiceList();
    int cur = 0;
    String now = voiceChosen();
    for (size_t i = 0; i < voices.size(); i++) if (voices[i] == now) cur = i;
    int pick = pickFromList("Breathe voice", voices, cur);
    if (pick < 0) return;
    voiceSetSaved(voices[pick]);
    if (!voiceLoad(voices[pick]) && voices[pick] != VOICE_BUILTIN_NAME)   // hear it: "In"
        toast((String("Can't read ") + voices[pick]).c_str(), col::warn, 1500);
    voicePlay(0);
    delay(voiceMillis(0) + 150);
    voiceUnload();
    if (voices.size() == 1) toast("More voices: /milliWorks/breathe/voices/", col::accent, 1600);
}

void appSettings() {
    int sel = 0;
    while (true) {
        std::vector<String> items = {
            String("Theme: ") + THEMES[savedTheme()].name,
            String("Brightness: ") + String(screenBrightness() * 100 / 255) + "%",
            String("Sounds: ") + (soundsOn() ? soundPackName() : "off"),
            String("Sound volume: ") + String(soundVolume() * 100 / 255) + "%",
            String("Breathe voice: ") + voiceChosen(),
            "Save settings to SD card",
            "Load settings from SD card",
            "Flip screen",
            "Show welcome again",
            "Keyboard bridge (simulator)",
            "About",
        };
        sel = pickFromList("Settings", items, sel);
        if (sel < 0) return;
        if (sel == 0) chooseTheme();
        if (sel == 1) nextBrightness();
        if (sel == 2) { setSoundsOn(!soundsOn()); playSfx(SFX_SELECT); }
        if (sel == 3) { nextSoundVolume(); playSfx(SFX_SUCCESS); }   // hear the new level
        if (sel == 4) chooseVoice();
        if (sel == 5) settingsSaveToCard();
        if (sel == 6) settingsLoadFromCard(false);
        if (sel == 7) flipScreen();
        if (sel == 8) runOnboarding();
        if (sel == 9) appKeyboardBridge();
        if (sel == 10) about();
    }
}
