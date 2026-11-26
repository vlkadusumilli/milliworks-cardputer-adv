# Releasing milliWorks

Every version goes through two steps: a **test build** first, then a **stable release** of
exactly the same code.

```
 change the code ──► beta (pre-release) ──► test on the Cardputer ──► stable (Latest)
                          ▲                          │
                          └──── fix, next beta ◄─────┘  something wrong
```

| | Tag | On GitHub | About screen | Who it's for |
|---|---|---|---|---|
| **Beta** | `v19-beta.1`, `v19-beta.2` … | marked *Pre-release*, never "Latest" | `Version 19-beta.1` | you and testers |
| **Stable** | `v19` | marked *Latest*; the README's install link points here | `Version 19` | everyone |

Versions are whole numbers (17, 18, 19…), one per stable release. A beta is numbered
for the version it's heading to: `v19-beta.1` is the first test of what will become `v19`.

## 1. Write down what changed

Add a line for each change under `## Unreleased` in [CHANGELOG.md](CHANGELOG.md), in plain
words for people using the Cardputer. Commit and push.

## 2. Make a test build

```bash
tools/release.sh beta 19 --dry-run     # optional: everything except publishing
tools/release.sh beta 19
```

The script stops if any of these fails:

- the code isn't committed, isn't on `main`, or differs from GitHub;
- the sound checks (levels, clicks, lengths, same loudness for every voice clip);
- the SD and settings-file tests;
- speech recognition confirming every voice clip says the right word;
- the Mac simulator and the firmware build;
- CHANGELOG has nothing under *Unreleased*.

It also prints the firmware size and the Launcher slot it needs. Then it sets the
version in `src/config.h`, commits, tags `v19-beta.N`, and publishes a **pre-release** with
the `.bin`, the SD zip and the notes. It never flashes a device.

## 3. Test it on the Cardputer

Install the beta through Launcher (remove the old version in **PM** first), then go
through this list. Anything wrong: fix it, push, and make the next beta (`beta 19` again
gives `v19-beta.2`).

**Start-up**
- [ ] The splash shows `v19-beta.N`. **Settings → About** says `Version 19-beta.N` and *by vlka*.
- [ ] With the SD card in: `/milliWorks` has `settings`, `breathe/voices`, `flow`, `music`.
- [ ] The Clock shows a time straight away, then the right time once Wi-Fi connects.

**Apps**
- [ ] Home: arrows, OK, number keys 1–7 and Back all work.
- [ ] Wi-Fi: connected network, IP and signal fit on one line; scan and join work.
- [ ] Flow: a short session starts, the sound plays and mutes (M), parking a thought saves to `/milliWorks/flow/parked.txt`.
- [ ] Status: each status changes the LED colour.
- [ ] Breathe (Guidance **Voice**, Count **On**, voice luna, then julian): *In, two, three, four* is in time with the screen's numbers for Calm, Box and 4-7-8. Music dips while the voice speaks.
- [ ] Breathe with the SD card out: the built-in voice says *In / Hold / Out* and nothing crashes.
- [ ] Settings: change the theme (sounds change too), save to SD, load from SD.
- [ ] Anything that changed in this version (see CHANGELOG).

**Device**
- [ ] Speaker volume is comfortable; no clicks or crackle.
- [ ] Unplugged on battery: runs and the battery icon looks right.

## 4. Make it stable

```bash
tools/release.sh stable 19
```

This only works if the code is **the same as the last beta**: only the version number and
CHANGELOG may differ. Otherwise it stops and asks for another beta. It builds `v19`,
dates the CHANGELOG section, and publishes it as **Latest**.

## Notes

- `dist/` (built `.bin` files) is not in git. Firmware lives in the GitHub releases.
- Firmware over 1088 KB moves to the next Launcher slot (1152 KB). The script warns about this.
- A beta that turned out bad can stay. It's marked as a pre-release, and the next beta or stable replaces it.
