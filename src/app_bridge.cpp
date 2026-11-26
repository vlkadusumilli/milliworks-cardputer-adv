// app_bridge.cpp - Keyboard bridge: the Cardputer becomes the keyboard for the milliWorks
// simulator on a computer. Every change of the keys is sent over the USB cable as one line:
//
//     KB:<keys held>:<flags>:<characters in hex>      e.g.  KB:2:s:41   (Shift + a = "A")
//     flags: f fn  s shift  c ctrl  o opt  a alt  e enter  d del  t tab
//
// The simulator (make -C sim run-keys) reads these lines. All keys go to the computer, so
// the G0 button on top of the Cardputer ends the bridge.
#include "ui.h"
#include "apps.h"
#include "sound.h"

static void sendState() {
    auto& kb = M5Cardputer.Keyboard;
    auto& st = kb.keysState();
    String flags;
    if (st.fn) flags += 'f';
    if (st.shift) flags += 's';
    if (st.ctrl) flags += 'c';
    if (st.opt) flags += 'o';
    if (st.alt) flags += 'a';
    if (st.enter) flags += 'e';
    if (st.del) flags += 'd';
    if (st.tab) flags += 't';
    String hex;
    char b[3];
    for (char c : st.word) { snprintf(b, sizeof(b), "%02x", (uint8_t)c); hex += b; }
    Serial.printf("KB:%d:%s:%s\n", (int)kb.isPressed(), flags.c_str(), hex.c_str());
}

static void draw(uint32_t sent, const String& last) {
    drawHeader("Keyboard bridge");
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(col::text);
    canvas.drawString("Typing goes to the simulator", SCREEN_W / 2, 34);
    canvas.setFont(&fonts::Font0);
    canvas.setTextColor(col::dim);
    canvas.drawString("on the computer, over the USB cable.", SCREEN_W / 2, 50);
    canvas.drawString("Start it with:  make -C sim run-keys", SCREEN_W / 2, 64);
    canvas.fillRoundRect(60, 76, 120, 22, 5, col::panel);
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(last.length() ? col::accent : col::dim);
    canvas.drawString(last.length() ? last : String("press a key"), SCREEN_W / 2, 87);
    canvas.setFont(&fonts::Font0);
    canvas.setTextColor(col::dim);
    canvas.drawString(String(sent) + " key changes sent", SCREEN_W / 2, 108);
    canvas.setTextColor(col::warn);
    canvas.drawString("G0 button (on top): stop", SCREEN_W / 2, 124);
    pushScreen();
}

void appKeyboardBridge() {
    uint32_t sent = 0, lastBeat = 0, lastDraw = 0;
    String last;
    bool wasSoundOn = soundsOn();
    setSoundsOn(false);                       // the keys make no sound here; the simulator plays them
    draw(0, last);
    while (true) {
        M5Cardputer.update();                 // read keys directly: no menus, no sounds
        if (M5Cardputer.BtnA.wasPressed()) break;
        auto& kb = M5Cardputer.Keyboard;
        bool changed = kb.isChange();
        if (changed || millis() - lastBeat > 500) {   // and twice a second, in case a line got lost
            sendState();
            lastBeat = millis();
            if (changed) {
                sent++;
                auto& st = kb.keysState();
                if (st.enter) last = "OK";
                else if (st.del) last = "Del";
                else if (st.tab) last = "Tab";
                else if (!st.word.empty()) last = String(st.word.back());
                else last = kb.isPressed() ? "..." : "";
                if (st.fn && !st.word.empty()) last = "Fn + " + last;
            }
        }
        if (millis() - lastDraw > 120) { draw(sent, last); lastDraw = millis(); }
        delay(5);
    }
    Serial.printf("KB:0::\n");                // let go of every key on the computer
    setSoundsOn(wasSoundOn);
    toast("Keyboard bridge off", col::accent, 900);
}
