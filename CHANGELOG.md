# Changelog

What changed in each milliWorks release. Write new changes under **Unreleased**:
`tools/release.sh` copies them into the release notes, and gives them the version number
when the release becomes stable.

## Unreleased

- The Music app is removed completely. It was only a placeholder and was never built. The empty `/milliWorks/music` folder that earlier versions made on the SD card is removed automatically.
## v19 - 2026-11-26

- About screen: the credit now reads *by Milli Labs*, the author's pen name (GitHub: vlkadusumilli).
- Flow: the **Soft drone** sound can now be heard on the Cardputer. Its tones were too low for the small speaker; they are now pitched higher.
- Breathe: counting is now an **On/Off** setting for the voice (Guidance: Voice, then Count: On or Off) instead of a separate "Count" guidance. If you chose Count in an earlier test build, you get Voice with Count On.
- Home: the Music app is hidden until it's ready.
- Breathe: uses the voice on your SD card straight away. Before, it looked for an old voice (Samantha) until you picked one in Settings, and quietly played the small built-in voice without counting. Now it uses **luna** if it's on the card, or else the first voice it finds.
- Breathe and Settings say so on screen if a voice on the card can't be read, instead of switching to the built-in voice without telling you.
- Release path: test builds (pre-releases such as `v19-beta.1`) come before each stable release. See [RELEASING.md](RELEASING.md).
- About screen: long version numbers (like `19-beta.1`) shrink to fit.

## v18 - 2026-11-26

- Apache License 2.0. The About screen credits the author: *by vlka* ([vlkadusumilli](https://github.com/vlkadusumilli)).
- Wi-Fi: the IP address and signal line no longer runs off the screen.

## v17 - 2026-11-26

- Breathe: new **Count** guidance. The voice says *In / Hold / Out* and counts each second, following the pattern you choose. The screen shows the same number.
- Two voices, Luna (female) and Julian (male), all at the same loudness and checked by speech recognition.
- Everything on the SD card lives in `/milliWorks`, with a folder for each app. Folders are created at start-up; files from older versions are moved there.
- Smaller built-in backup voice, so the firmware fits a 1088 KB Launcher slot.
