// config.h - settings you might want to change.
#pragma once

// Firmware name and version, shown on the start-up screen and in the menu.
#define FW_NAME    "milliWorks"
// Set by tools/release.sh: "19-beta.1" for a test build (pre-release), "19" when stable.
#define FW_VERSION "18"
// Original author, shown on the About screen (vlka = vlkadusumilli, see NOTICE).
// Apache 2.0: keep this credit and the NOTICE file when you build on it.
#define FW_AUTHOR  "vlka"

// The time zone is chosen on the device (Clock -> z); see ZONES in app_clock.cpp.
#define NTP_SERVER "pool.ntp.org"

// Hardware pins (from the Cardputer ADV docs).
#define IR_TX_PIN 44

// Everything milliWorks keeps on the microSD card lives in one folder, one subfolder per app:
//   /milliWorks/settings/settings.json      Settings > Save settings to SD card
//   /milliWorks/breathe/voices/<name>/      Breathe voices (in.wav, hold.wav, out.wav)
//   /milliWorks/flow/parked.txt             thoughts parked during a Flow session
//   /milliWorks/music/                      the music player (coming)
// Older versions used /milliworks/settings.json, /milliworks/voice and /flow; app_sd.cpp
// moves those into place the first time the card is read.
#define MW_DIR       "/milliWorks"
#define SETTINGS_DIR MW_DIR "/settings"
#define BREATHE_DIR  MW_DIR "/breathe"
#define VOICE_DIR    BREATHE_DIR "/voices"
#define FLOW_DIR     MW_DIR "/flow"
#define MUSIC_DIR    MW_DIR "/music"

// Name used for saved settings in flash (the ESP32 "Preferences" storage).
#define PREFS_NS "cardputer"
