// apps.h - every "app" is just a function that runs until you press Back (`).
#pragma once

void appClock();
bool appWifi();           // returns true if connected when you leave it
void appSd();
void appSettings();
void appFlow();
void appStatus();
void appBreathe();
void appKeyboardBridge();  // the Cardputer as keyboard for the Mac simulator
void runOnboarding();      // the first-start tour (app_onboarding.cpp)
bool settingsFileOnCard();                 // settings_sd.cpp
void settingsSaveToCard();
bool settingsLoadFromCard(bool firstStart);
bool onboardingDone();

// Wi-Fi helpers (app_wifi.cpp)
void wifiAutoConnect();   // join the strongest saved network, if any (doesn't wait)
bool wifiHasSaved();

// Clock helpers (app_clock.cpp)
void clockApplySavedTimeZone();   // set the saved time zone + start internet time sync
bool clockIsSet();                // true once there is any time (maybe last-known)
bool clockIsVerified();           // true once it came from the internet, by hand, or survived a restart
void clockBoot();                 // at power-on: restore the last known time if we have nothing better
bool clockHasRtc();               // an RTC module is plugged in (keeps time while off)
int  clockZoneCount();
const char* clockZoneName(int i);
int  clockSavedZone();
void clockSetZone(int i);
void serviceTick();               // background work; readInput() calls it on every screen
void wifiService();               // part of serviceTick: fast-reconnect fallback, remember the access point
