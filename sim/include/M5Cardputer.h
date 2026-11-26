// M5Cardputer.h (simulator) - the Cardputer ADV on a Mac: the real M5GFX screen library
// draws into an SDL window, the Mac keyboard is the Cardputer keyboard, the Mac speakers
// are its speaker, and the LED colour shows in the window title.
#pragma once
#include "Arduino.h"
#include <M5GFX.h>
#include <vector>

#define KEY_BACKSPACE 0x2a
#define KEY_TAB 0x2b
#define KEY_ENTER 0x28

namespace m5 {
using board_t = lgfx::boards::board_t;

struct rtc_date_t { int16_t year = 2000; int8_t month = 1, date = 1, weekDay = 0; };
struct rtc_time_t { int8_t hours = 0, minutes = 0, seconds = 0; };
struct rtc_datetime_t { rtc_date_t date; rtc_time_t time; };

struct Rtc_Class {          // no RTC module plugged in (the default Cardputer ADV)
    bool isEnabled() const { return false; }
    bool getVoltLow() { return true; }
    bool getDateTime(rtc_datetime_t*) const { return false; }
    bool setDateTime(const tm*) { return false; }
};
struct Power_Class { int32_t getBatteryLevel() { return 80; } };
struct Led_Class {
    bool isEnabled() const { return true; }
    void setBrightness(uint8_t b);
    void setAllColor(uint8_t r, uint8_t g, uint8_t b);
    void display();
};

struct Speaker_Class {
    void setVolume(uint8_t v);
    void setChannelVolume(uint8_t ch, uint8_t v);
    bool tone(float freq, uint32_t ms, int ch, bool stop, const uint8_t* wave, size_t len, bool stereo = false);
    bool tone(float freq, uint32_t ms = 0xFFFFFFFF, int ch = -1, bool stop = true);
    bool playRaw(const int16_t* data, size_t len, uint32_t rate = 44100, bool stereo = false, uint32_t repeat = 1, int ch = -1, bool stop = false);
    bool playRaw(const uint8_t* data, size_t len, uint32_t rate = 44100, bool stereo = false, uint32_t repeat = 1, int ch = -1, bool stop = false);
    size_t isPlaying(uint8_t ch) const;
    void stop(uint8_t ch);
    void stop();
};

struct config_t { bool external_rtc = false; };

struct M5Unified_Sim {
    Rtc_Class Rtc;
    Power_Class Power;
    Led_Class Led;
    Speaker_Class Speaker;
    config_t config() { return config_t(); }
    board_t getBoard() const { return board_t::board_M5CardputerADV; }
    void delay(uint32_t ms) { ::delay(ms); }
};
}  // namespace m5

extern m5::M5Unified_Sim M5;

// ---- keyboard: the Mac keyboard, mapped to the Cardputer's keys ----------------------
class Keyboard_Class {
public:
    struct KeysState {
        bool tab = false, fn = false, shift = false, ctrl = false, opt = false, alt = false;
        bool del = false, enter = false, space = false;
        std::vector<char> word;
    };
    bool isChange();
    uint8_t isPressed();
    KeysState& keysState() { return _state; }
    void update();                    // reads the Mac keyboard (called by M5Cardputer.update())
                                      // or, with the bridge, the real Cardputer's keys
private:
    KeysState _state;
    uint8_t _count = 0, _lastCount = 0;
};

struct Button_Class { bool wasPressed() { return false; } bool isPressed() { return false; } };

class M5_CARDPUTER {
public:
    Button_Class BtnA;
    void begin(m5::config_t cfg, bool enableKeyboard = true);
    void update();
    M5GFX Display;
    M5GFX& Lcd = Display;
    m5::Speaker_Class& Speaker = M5.Speaker;
    Keyboard_Class Keyboard;
};
extern M5_CARDPUTER M5Cardputer;
