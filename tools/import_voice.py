"""Turn three recorded or generated clips into a Breathe voice for the SD card.

    python3 tools/import_voice.py <name> <in.wav> <hold.wav> <out.wav>
    python3 tools/import_voice.py <name> 2=<two.wav> 3=<three.wav> ...   (counting, 2 to 8)
    python3 tools/import_voice.py <name> --relevel     (level the clips already there again)

Takes any WAV (mono or stereo, 16-bit, any rate), mixes it to mono, resamples to 16 kHz,
trims the silence, adds 8 ms fades (no clicks), levels every clip to the same loudness
and writes:
  - design/sd/milliWorks/breathe/voices/<name>/{in,hold,out}.wav   copy this folder to the SD card
  - design/sounds/voice_<name>_{in,hold,out}.wav          to listen to on the Mac
Clips must stay under 3 s (the Cardputer's limit per clip, see src/voice.cpp).
"""
import math
import os
import sys
import wave
from array import array

SR = 16000
MAX_S = 3.0
TARGET_DB = -12.0      # loudness of the spoken part
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read_mono(path):
    with wave.open(path) as w:
        ch, sw, rate = w.getnchannels(), w.getsampwidth(), w.getframerate()
        assert sw == 2, f"{path}: needs 16-bit audio"
        raw = array("h", w.readframes(w.getnframes()))
    mono = [sum(raw[i:i + ch]) / ch for i in range(0, len(raw), ch)]
    if rate == SR:
        return mono
    n = int(len(mono) * SR / rate)                     # linear resample
    out = []
    for i in range(n):
        p = i * rate / SR
        a = int(p); f = p - a
        b = min(a + 1, len(mono) - 1)
        out.append(mono[a] * (1 - f) + mono[b] * f)
    return out


def tidy(s):
    # Find the speech by loudness in 20 ms windows. Sounds more than 0.3 s apart are separate
    # groups; the word is the group with the most energy, so a lone click or breath before or
    # after it is cut.
    win = SR // 50
    rms = [(sum(x * x for x in s[i:i + win]) / win) ** 0.5 for i in range(0, len(s), win)]
    thr = max(rms) * 0.06
    loud = [i for i, r in enumerate(rms) if r > thr]
    groups = [[loud[0], loud[0]]]
    for i in loud[1:]:
        if (i - groups[-1][1]) * win > SR * 0.3:
            groups.append([i, i])
        else:
            groups[-1][1] = i
    first, last = max(groups, key=lambda g: sum(rms[j] ** 2 for j in range(g[0], g[1] + 1)))
    a, b = first * win, min(len(s), (last + 1) * win)
    a, b = max(0, a - SR // 50), min(len(s), b + SR // 10)  # keep 20 ms before, 100 ms after
    return level(s[a:b])


def level(s):
    """Same loudness for every clip: the spoken part is set to TARGET_DB (RMS), and the
    rare peaks that would then go past -1 dBFS are rounded off gently (no hard clipping)."""
    win = SR // 50
    rms = [(sum(x * x for x in s[i:i + win]) / win) ** 0.5 for i in range(0, len(s), win)]
    thr = max(rms) * 0.25
    spoken = [r for r in rms if r > thr]
    loud = (sum(r * r for r in spoken) / len(spoken)) ** 0.5 or 1
    gain = 32767 * 10 ** (TARGET_DB / 20) / loud
    ceil = 32767 * 0.89                                 # -1 dBFS
    fade = SR * 8 // 1000
    out = array("h")
    for i, x in enumerate(s):
        y = x * gain
        if abs(y) > ceil * 0.7:                         # soft knee above -4 dBFS
            k = ceil * 0.7
            y = math.copysign(k + (ceil - k) * math.tanh((abs(y) - k) / (ceil - k)), y)
        f = min(1.0, i / fade, (len(s) - 1 - i) / fade)
        out.append(int(y * f))
    return out


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    name = sys.argv[1].lower()
    if sys.argv[2] == "--relevel":
        d = os.path.join(ROOT, "design", "sd", "milliWorks", "breathe", "voices", name)
        pairs = [(f[:-4], os.path.join(d, f)) for f in sorted(os.listdir(d)) if f.endswith(".wav")]
    elif "=" in sys.argv[2]:                             # key=file pairs, e.g. 2=two.wav
        pairs = [a.split("=", 1) for a in sys.argv[2:]]
    elif len(sys.argv) == 5:
        pairs = list(zip(("in", "hold", "out"), sys.argv[2:]))
    else:
        sys.exit(__doc__)
    sd_dir = os.path.join(ROOT, "design", "sd", "milliWorks", "breathe", "voices", name)
    snd_dir = os.path.join(ROOT, "design", "sounds")
    os.makedirs(sd_dir, exist_ok=True)
    for key, src in pairs:
        s = tidy(read_mono(src))
        secs = len(s) / SR
        assert secs < MAX_S, f"{src}: {secs:.2f} s is longer than {MAX_S} s"
        for path in (os.path.join(sd_dir, f"{key}.wav"), os.path.join(snd_dir, f"voice_{name}_{key}.wav")):
            with wave.open(path, "wb") as w:
                w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(s.tobytes())
        print(f"{name}/{key}.wav  {secs:.2f} s  {len(s) * 2 // 1024} KB")


if __name__ == "__main__":
    main()
