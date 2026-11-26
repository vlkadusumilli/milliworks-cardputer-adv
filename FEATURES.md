# Cardputer ADV: what's inside and what it can do

The Cardputer ADV is a tiny programmable computer: an **ESP32‑S3 microcontroller**
with a screen, a keyboard, audio and wireless radios, plus a battery. It doesn't run
an operating system like a laptop. It runs **one program (firmware) at a time**, and
that program has full control of the hardware.

## The hardware

| Part | What it is | What you can do with it | Learn it in this project |
|---|---|---|---|
| **Brain** | ESP32‑S3 (StampS3A), 2 cores at 240 MHz, 8 MB flash, ~320 KB RAM, no PSRAM | Run your C++ / MicroPython code | Everything |
| **Screen** | 1.14" color LCD, 240 × 135 pixels | Text, graphics, animations, games | Every app |
| **Keyboard** | 56 keys read by a TCA8418 chip | Typing, menus, game controls, music keys | Tour → Keys |
| **Wi‑Fi** | 2.4 GHz only (b/g/n) | Internet time, weather, web APIs, controlling smart devices | Wi‑Fi, Clock |
| **Bluetooth** | BLE 5 | Phone connections, BLE keyboards/sensors | (idea list) |
| **Speaker** | ES8311 codec + NS4150B amplifier | Beeps, tones, music | Tour → Piano, Music |
| **Headphone jack** | 3.5 mm | Listen privately | Music |
| **Microphone** | MEMS mic | Sound level meter, recording, voice | Tour → Mic |
| **IMU** | BMI270 accelerometer + gyroscope | Tilt/motion detection, spirit level, gestures, step counting | Tour → Tilt |
| **IR transmitter** | Infrared LED on pin G44 (send only, no receiver) | TV / AC remote control | LG Remote |
| **microSD slot** | SPI, FAT32 cards | Music files, saved data, apps for launchers | Music, Tour → SD |
| **Battery** | ~1500–1750 mAh LiPo (depends on production batch) | Use it unplugged | Header bar |
| **Grove port** | HY2.0‑4P (G1/G2, 5 V) | Plug in M5 "Units": sensors, GPS, IR receiver, relays… | (idea list) |
| **EXT header** | 2.54 mm 14‑pin | Bigger add‑ons such as LoRa modules | (idea list) |
| **Buttons** | G0 (top) + reset + power switch | G0 = download mode / user button | — |

### Things that surprise beginners
- **No clock chip (RTC).** The time is lost when it powers off, so the Clock app gets the time from the internet (NTP).
- **No IR receiver.** It can *send* remote codes but not *learn* them from your remote.
- **One program at a time.** "Apps" in this project are menu entries inside one firmware.
  Launchers like M5Launcher swap whole firmwares from the SD card.
- **Speaker and mic share the audio chip**, so the code turns one off before using the other.

## Keyboard cheat‑sheet (in this project's firmware)

| Key | Meaning |
|---|---|
| `` ` `` (top‑left, ESC) | Back / exit the app |
| `;` `.` `,` `/` | Up / Down / Left / Right (the arrows printed on those keys) |
| Enter | Select / OK / start |
| Shift (Aa) + key | Upper‑case / symbol. **`"` = Shift + `'`** |
| Fn + key | In text boxes, Fn + `` ` `` cancels; Fn + arrows move |

## How code gets onto it

1. You write C++ in `src/` (this project uses **PlatformIO** + the **Arduino** framework).
2. `pio run` compiles it into `firmware.bin`.
3. `pio run -t upload` sends it over USB‑C. If the upload fails, hold **G0** while plugging in the cable
   (download mode), then try again, then press reset.
4. `pio device monitor` shows `Serial.println(...)` messages from the device, which helps a lot with debugging.

## Key libraries
- **M5Cardputer**: keyboard + sets everything up (`M5Cardputer.begin`).
- **M5Unified / M5GFX**: screen, speaker, mic, IMU, battery (`M5.Display`, `M5.Speaker`, `M5.Imu`, `M5.Power`).
- **IRremote**: sends IR codes.
- **ESP8266Audio**: decodes MP3/WAV for the music player.
