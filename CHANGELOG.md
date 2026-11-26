# Changelog

What changed in each milliWorks release. Write new changes under **Unreleased**:
`tools/release.sh` copies them into the release notes, and gives them the version number
when the release becomes stable.

## Unreleased

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
