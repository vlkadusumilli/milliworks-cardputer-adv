# milliWorks for the M5Stack Cardputer ADV

A small, friendly set of apps for the **M5Stack Cardputer ADV**: a clock that keeps time
without waiting for the internet, a focus timer with calming sounds, a status light for
"do not disturb", and a guided breathing app with a voice that counts with you.

| App | What it does |
|---|---|
| **Clock** | Internet time (NTP), shows the last known time straight away, time zones, 12/24 h, optional RTC module |
| **Wi-Fi** | Scan, join and remember up to 5 networks; reconnects quickly to the last one |
| **Flow** | Focus sessions with an intention, background sounds (rain, brown noise, drone, tick), a "park this thought" note, breaks and a summary |
| **Status** | The RGB LED shows Busy / Free / Focus so people around you know |
| **Breathe** | Calm, Box, 4-7-8 and Even patterns; a voice says *In / Hold / Out* and counts each second, matching the numbers on screen. Optional calm music |
| **SD Card** | See what's on the card and browse it |
| **Settings** | 6 themes with matching sound packs, brightness, volume, voice, flip screen, save/load all settings to the SD card |

The Music app is coming.

## Install

You need a Cardputer **ADV** with a microSD card (FAT32).

### 1. The firmware

The easiest way is **[Launcher](https://bmorcelli.github.io/Launcher/webflasher.html)**,
which lets you keep several firmwares on the Cardputer and switch between them.

1. Download `milliworks-cardputer-adv-vXX.bin` from the [latest release](../../releases/latest).
2. Copy it to the `bins` folder on the SD card. Use a card reader, or Launcher's **WUI** (web page) over Wi-Fi.
3. On the Cardputer, open Launcher → **SD** → `bins` → the `.bin` → **Install**.

When updating, first remove the old version in Launcher's **PM** (partition manager), then
choose **Apply Changes**. Launcher keeps each installed version in its own slot.

### 2. The voices (for Breathe)

1. Download `milliWorks-sd-card.zip` from the same release and unzip it.
2. Copy the `milliWorks` folder to the top level of the SD card. It holds `milliWorks/breathe/voices/luna` and `julian`.
   - If milliWorks has already started with the card inserted, the folders are already there. Copy the `luna` and `julian` folders into `milliWorks/breathe/voices/`.
3. On the Cardputer: **Settings → Breathe voice**, then pick **luna** (female) or **julian** (male).

There's also a small voice built into the firmware, so Breathe works without the card. It
doesn't count, though.

### What's on the SD card

milliWorks keeps everything in one folder and creates it when it starts:

```
/milliWorks/settings/settings.json     Settings → Save settings to SD card
/milliWorks/breathe/voices/<name>/     in.wav, hold.wav, out.wav, 2.wav … 8.wav
/milliWorks/flow/parked.txt            thoughts you parked during Flow
/milliWorks/music/                     for the Music app (coming)
```

`settings.json` lets you move your setup to another Cardputer. Wi-Fi passwords are only
included if you say yes when saving.

### Your own voice

A voice is a folder with 16-bit WAV files: `in.wav`, `hold.wav`, `out.wav` and `2.wav` to
`8.wav`. Each clip must be under 1 second, or it runs into the next count. Record your own
clips, then tidy them (trim, fade, even loudness) with:

```bash
python3 tools/import_voice.py myvoice in=in.wav hold=hold.wav out=out.wav 2=two.wav 3=three.wav
swiftc -O tools/check_voice.swift -o /tmp/check_voice && /tmp/check_voice myvoice   # does each clip say the right word?
```

## Keys

| Key | In menus | While typing text |
|---|---|---|
| `;` `.` `,` `/` | up, down, left, right | Fn + the key |
| `` ` `` (Esc) | back | Fn + `` ` `` |
| Del | back | delete a letter |
| Enter | OK | OK |
| G0 (top button) | stops the keyboard bridge | |

## Make it yours (template)

This repository is a **template**. Click **Use this template** at the top of the GitHub
page to get your own copy, then:

**Rename it.** Everything that carries the name lives in [`src/config.h`](src/config.h):

```cpp
#define FW_NAME    "milliWorks"     // start-up screen, welcome tour, settings file
#define FW_VERSION "17"
#define MW_DIR     "/milliWorks"    // the app's folder on the SD card
#define PREFS_NS   "cardputer"      // where settings are kept in flash
```

**Add an app.** An app is a function that draws its screen and runs until you press Back:

```cpp
// src/app_hello.cpp
#include "ui.h"
#include "apps.h"

void appHello() {
    while (true) {
        M5Cardputer.update();
        Input in = readInput();              // keys, with the theme's key sounds
        if (in.back) return;                 // ` or Del goes back to the home screen
        drawHeader("Hello");                 // title bar with clock and battery
        canvas.setFont(&fonts::Font4);
        canvas.setTextColor(col::accent);    // theme colours: col::text, dim, accent, good, warn...
        canvas.setTextDatum(middle_center);
        canvas.drawString("Hi there!", SCREEN_W / 2, 70);
        drawHints({{"ESC", "back"}});
        pushScreen();                        // everything is drawn off-screen, then shown at once
    }
}
```

Then declare it in [`src/apps.h`](src/apps.h) (`void appHello();`) and add it to the home
screen's `APPS` list in [`src/main.cpp`](src/main.cpp), with a colour and an icon. Icons
are drawn in `drawGlyph`, so reuse one or add your own.

**Helpers you get** (see [`src/ui.h`](src/ui.h)): `toast`, `confirm`, `pickFromList`,
`textPrompt` (with a show/hide for passwords), `drawTextFit` (shrinks text to fit), six
themes with matching sounds (`playSfx`), Wi-Fi, NTP time, the SD card (`sdMount`) and the
LED (`status.h`). Try every change in the Mac simulator before putting it on the device.

**Space.** Launcher gives each firmware a slot rounded up to 64 KB. milliWorks v17 is
1086 KB, which fits a 1088 KB slot; going over moves it to the next size up.

## Build it yourself

[PlatformIO](https://platformio.org):

```bash
pio run -e cardputer-adv                       # build
pio run -e cardputer-adv -t upload             # flash over USB (needs upload_flags = --no-stub, already set)
```

### Mac simulator

The `sim` folder runs the real firmware in a window on a Mac, using the same screen library.
It has the same colours, a working keyboard, sound, and a pretend SD card and Wi-Fi.

```bash
brew install sdl2
make -C sim run          # as your Cardputer is now
make -C sim run-new      # like a brand-new one (welcome tour)
make -C sim run-keys     # type on the real Cardputer: Settings → Keyboard bridge on it
```

### Checks

```bash
c++ -std=c++17 -O2 -I src tools/render_sounds.cpp -o /tmp/rs && /tmp/rs        # every sound: level, clicks, length
c++ -std=c++17 -O2 -I src tools/test_sd_features.cpp -o /tmp/sd && /tmp/sd     # settings file + voice loading
```

## Credits

- Built on [M5Unified](https://github.com/m5stack/M5Unified), [M5GFX](https://github.com/m5stack/M5GFX) and [M5Cardputer](https://github.com/m5stack/M5Cardputer).
- Voices Luna and Julian were generated with Seed Audio (ByteDance) through Higgsfield.
- Installed with [Launcher](https://github.com/bmorcelli/Launcher) by bmorcelli.
