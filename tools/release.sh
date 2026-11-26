#!/usr/bin/env bash
# release.sh - builds, checks and publishes a milliWorks release on GitHub.
#
#   tools/release.sh beta 19            test build: v19-beta.1, then v19-beta.2, ... (pre-release)
#   tools/release.sh stable 19          the tested beta becomes v19 (marked Latest)
#   add --dry-run to build and check everything without committing or publishing
#
# The path:  code changes -> beta (pre-release) -> test it on the Cardputer (RELEASING.md)
#            -> fix and make another beta if needed -> stable.
# A stable release must be the same code as the last beta: only the version number in
# src/config.h and CHANGELOG.md may differ. Nothing is ever flashed to a device.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
REPO="vlkadusumilli/milliworks-cardputer"
SLOT_KB=1088                       # the Launcher slot milliWorks fits today (rounded to 64 KB)
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

kind="${1:-}"; num="${2:-}"; dry=""
[[ "${3:-}" == "--dry-run" || "${1:-}" == "--dry-run" ]] && dry=1
if [[ "$kind" != "beta" && "$kind" != "stable" ]] || ! [[ "$num" =~ ^[0-9]+$ ]]; then
    sed -n 2,11p "$0" | sed 's/^# \{0,1\}//'; exit 1
fi
say()  { printf '\n\033[1m== %s\033[0m\n' "$*"; }
fail() { printf '\n\033[31mSTOP: %s\033[0m\n' "$*"; exit 1; }

# ---- 1. Where are we? ---------------------------------------------------------------
say "Checking the code is committed and up to date"
[[ -z "$(git status --porcelain)" ]] || fail "there are uncommitted changes. Commit them first."
[[ "$(git branch --show-current)" == "main" ]] || fail "releases are made from main."
git fetch -q --tags origin
[[ "$(git rev-parse HEAD)" == "$(git rev-parse origin/main)" ]] || fail "main is not the same as GitHub (push or pull first)."
git rev-parse -q --verify "refs/tags/v$num" >/dev/null && fail "v$num is already released as stable."

last_beta="$(git tag -l "v$num-beta.*" | sort -t. -k2 -n | tail -1)"
if [[ "$kind" == "beta" ]]; then
    n=$(( ${last_beta##*.} + 0 + 1 )); [[ -z "$last_beta" ]] && n=1
    version="$num-beta.$n"
else
    [[ -n "$last_beta" ]] || fail "there is no beta of v$num yet. Make one and test it: tools/release.sh beta $num"
    if ! git diff --quiet "$last_beta" HEAD -- . ':!src/config.h' ':!CHANGELOG.md'; then
        git diff --stat "$last_beta" HEAD -- . ':!src/config.h' ':!CHANGELOG.md'
        fail "the code changed since $last_beta (above). Make another beta and test that first."
    fi
    version="$num"
fi
tag="v$version"
echo "Making $tag ${dry:+(dry run: nothing is committed or published)}"

# ---- 2. Version number ----------------------------------------------------------------
sed -i '' "s/^#define FW_VERSION \".*\"/#define FW_VERSION \"$version\"/" src/config.h
grep -q "#define FW_VERSION \"$version\"" src/config.h || fail "could not set the version in src/config.h"
restore() { [[ -n "$dry" ]] && git checkout -q -- src/config.h CHANGELOG.md 2>/dev/null || true; }
trap 'restore; rm -rf "$TMP"' EXIT

# ---- 3. Checks --------------------------------------------------------------------------
say "Sound checks (levels, clicks, lengths, voices)"
c++ -std=c++17 -O2 -I src tools/render_sounds.cpp -o "$TMP/rs" && "$TMP/rs" | tail -1
"$TMP/rs" | grep -q "no problems" || fail "a sound check failed: run the line above to see which."

say "SD card and settings-file tests"
c++ -std=c++17 -O2 -I src tools/test_sd_features.cpp -o "$TMP/sdtest" && "$TMP/sdtest" | tail -1
"$TMP/sdtest" | grep -q " 0 failed" || fail "an SD test failed."

say "Each voice clip says the right word (speech recognition)"
swiftc -O tools/check_voice.swift -o "$TMP/cv" 2>/dev/null && "$TMP/cv" luna julian | tail -1 || fail "a voice clip says the wrong word."

say "Mac simulator builds"
make -s -C sim >/dev/null || fail "the simulator does not build."
echo "ok"

say "Firmware for the Cardputer ADV"
pio run -e cardputer-adv > "$TMP/pio.log" 2>&1 || { tail -30 "$TMP/pio.log"; fail "the firmware does not build."; }
mkdir -p dist
bin="dist/milliworks-cardputer-adv-$tag.bin"
cp .pio/build/cardputer-adv/firmware.bin "$bin"
kb=$(( ($(stat -f%z "$bin") + 1023) / 1024 ))
slot=$(( (kb + 63) / 64 * 64 ))
echo "$bin: $kb KB, Launcher slot $slot KB"
(( slot <= SLOT_KB )) || echo "NOTE: bigger than the $SLOT_KB KB slot. People updating must have $slot KB free in Launcher."

# ---- 4. SD card files ---------------------------------------------------------------------
say "SD card zip (voices)"
mkdir -p "$TMP/sd/milliWorks/breathe/voices" "$TMP/sd/milliWorks/settings" "$TMP/sd/milliWorks/flow" "$TMP/sd/milliWorks/music"
cp -R design/sd/milliWorks/breathe/voices/luna design/sd/milliWorks/breathe/voices/julian "$TMP/sd/milliWorks/breathe/voices/"
find "$TMP/sd" -name .DS_Store -delete
zip="$TMP/milliWorks-sd-card.zip"
(cd "$TMP/sd" && zip -qr -X "$zip" milliWorks)
echo "milliWorks-sd-card.zip: $(unzip -l "$zip" | tail -1 | awk '{print $2}') files"

# ---- 5. Release notes (from CHANGELOG.md) -------------------------------------------------
changes="$(awk '/^## Unreleased/{f=1;next} /^## /{f=0} f' CHANGELOG.md | sed '/^[[:space:]]*$/N;/^\n$/D')"
[[ -n "$(echo "$changes" | tr -d '[:space:]')" ]] || fail "CHANGELOG.md has nothing under '## Unreleased'. Write what changed first."
{
    if [[ "$kind" == "beta" ]]; then
        echo "> **Test build.** This is a pre-release for testing v$num before it becomes stable."
        echo "> For everyday use, install the [latest stable release](https://github.com/$REPO/releases/latest)."
        echo
    fi
    echo "## Install"
    echo
    echo "1. **Firmware:** copy \`milliworks-cardputer-adv-$tag.bin\` to the \`bins\` folder on your SD card, then in [Launcher](https://bmorcelli.github.io/Launcher/webflasher.html) go to **SD → bins → Install**. Updating? Remove the old version first in Launcher's **PM**."
    echo "2. **Voices:** unzip \`milliWorks-sd-card.zip\` and copy the \`milliWorks\` folder to the top of the SD card (skip this if your voices are already there)."
    echo "3. **Settings → About** shows the version: **$version**."
    echo
    echo "## What's new"
    echo
    echo "$changes"
    echo
    echo "For the M5Stack Cardputer **ADV** (ESP32-S3)."
} > "$TMP/notes.md"

if [[ "$kind" == "stable" ]]; then
    today="$(date +%Y-%m-%d)"
    awk -v v="v$num" -v d="$today" '/^## Unreleased/ && !done {print "## Unreleased\n\n## " v " - " d; done=1; next} {print}' CHANGELOG.md > "$TMP/cl" && cp "$TMP/cl" CHANGELOG.md
fi

if [[ -n "$dry" ]]; then
    say "Dry run finished: $tag would be published with these notes"
    cat "$TMP/notes.md"
    exit 0
fi

# ---- 6. Publish ---------------------------------------------------------------------------
say "Publishing $tag"
git add src/config.h CHANGELOG.md
git commit -q -m "Release $tag"
git tag -a "$tag" -m "milliWorks $tag"
git push -q origin main "$tag"
if [[ "$kind" == "beta" ]]; then
    gh release create "$tag" "$bin" "$zip" --repo "$REPO" --prerelease --title "milliWorks $tag (test build)" --notes-file "$TMP/notes.md"
else
    gh release create "$tag" "$bin" "$zip" --repo "$REPO" --latest --title "milliWorks $tag" --notes-file "$TMP/notes.md"
fi
say "Done: https://github.com/$REPO/releases/tag/$tag"
[[ "$kind" == "beta" ]] && echo "Next: test it on the Cardputer with the checklist in RELEASING.md, then: tools/release.sh stable $num"
exit 0
