// app_status.cpp - Status: pick how you want to be seen; the screen becomes a big sign
// and the LED glows in the matching colour. Sign colours are fixed (not the theme):
// they have to mean the same thing to everyone around you.
#include "ui.h"
#include "apps.h"
#include "status.h"
#include "sound.h"
#include <time.h>

static void showSign(StatusKind s) {
    setStatus(s);
    uint16_t bg, title, line, small;
    const char* big = statusName(s);
    String sub;
    switch (s) {
        case ST_DND:     bg = 0x2820; title = 0xFFFF; line = 0xFD08; small = 0x8A28; sub = "Please come back later"; break;
        case ST_FOCUS:   bg = 0x28C0; title = 0xFFFF; line = 0xFD84; small = 0x8C42; sub = "Only if it's important"; big = "Focusing"; break;
        case ST_MEETING: bg = 0x0105; title = 0xFFFF; line = 0x6C7F; small = 0x4A6F; sub = "Back soon"; break;
        default:         bg = 0x0101; title = 0xFFFF; line = 0x3E8B; small = 0x3369; sub = "Come in, say hi"; big = "Free"; break;
    }
    int bright = screenBrightness();
    M5Cardputer.Display.setBrightness(min(bright, 140));   // calm, not glaring
    while (true) {
        M5Cardputer.update();
        if (readInput().pressed) break;                    // any key takes the sign down
        canvas.fillScreen(bg);
        if (s == ST_DND) {                                 // a "no entry" sign
            canvas.fillCircle(SCREEN_W / 2, 30, 14, 0xF9C6);
            canvas.fillRect(SCREEN_W / 2 - 10, 27, 20, 6, bg);
        }
        canvas.setTextDatum(middle_center);
        canvas.setFont(&fonts::Font4);
        canvas.setTextColor(title);
        canvas.drawString(big, SCREEN_W / 2, s == ST_DND ? 66 : 52);
        canvas.setFont(&fonts::Font2);
        canvas.setTextColor(line);
        canvas.drawString(sub, SCREEN_W / 2, s == ST_DND ? 92 : 80);
        canvas.setFont(&fonts::Font0);
        canvas.setTextColor(small);
        canvas.drawString("any key  take the sign down", SCREEN_W / 2, 124);
        pushScreen();
        delay(30);
    }
    M5Cardputer.Display.setBrightness(bright);
}

void appStatus() {
    static const StatusKind ORDER[] = {ST_DND, ST_FOCUS, ST_MEETING, ST_FREE, ST_OFF};
    int sel = 0;
    while (true) {
        std::vector<String> items;
        for (auto s : ORDER) items.push_back(String("     ") + statusName(s) + (s == currentStatus() && s != ST_OFF ? "  (on)" : ""));
        // pickFromList draws the list; we add the coloured dots by drawing our own loop here.
        int top = 0;
        bool chosen = false;
        while (!chosen) {
            M5Cardputer.update();
            Input in = readInput();
            if (in.back) return;
            if (in.up) sel = (sel + 4) % 5;
            if (in.down) sel = (sel + 1) % 5;
            if (in.enter) chosen = true;
            drawHeader("Status");
            drawList(items, sel, top, 5);
            for (int i = 0; i < 5; i++) {
                uint8_t r, g, b;
                statusLedRgb(ORDER[i], r, g, b);
                int y = HEADER_H + 4 + i * 18 + 9;
                if (ORDER[i] == ST_OFF) canvas.drawCircle(16, y, 4, col::dim);
                else canvas.fillCircle(16, y, 4, canvas.color565(r, g, b));
            }
            drawHints({{ARROWS_UD, "choose"}, {"OK", "show sign"}, {"ESC", "home"}});
            pushScreen();
            delay(15);
        }
        if (ORDER[sel] == ST_OFF) { setStatus(ST_OFF); toast("Status off", col::warn, 700); }
        else showSign(ORDER[sel]);
    }
}
