"""Send a Wi-Fi password from the Mac clipboard to Launcher on the Cardputer.

Usage (close any `pio device monitor` first, it holds the port):
    1. Copy the Wi-Fi password (e.g. from the Passwords app).
    2. Run:  <platformio python> tools/wifi_from_clipboard.py "Your Wi-Fi name"

The password is never printed. Only the Cardputer's replies are shown,
and any reply containing the password is masked.
"""
import subprocess
import sys
import json
import time

import serial  # pyserial, bundled with PlatformIO

import glob
PORT = (glob.glob("/dev/cu.usbmodem*") or ["/dev/cu.usbmodem2101"])[0]
args = [a for a in sys.argv[1:] if not a.startswith("--")]
ssid = args[0] if args else input("Wi-Fi name (SSID): ").strip()

if "--clipboard" in sys.argv:
    pwd = subprocess.run(["pbpaste"], capture_output=True, text=True).stdout
    pwd = pwd.rstrip("\r\n")  # copying sometimes adds a newline at the end
else:
    import getpass
    pwd = getpass.getpass(f"Type the Wi-Fi password for {ssid} (hidden), then Enter: ")
if not pwd:
    sys.exit("No password given - nothing was sent.")
if "\n" in pwd or "\r" in pwd:
    sys.exit(f"Clipboard has {pwd.count(chr(10)) + 1} lines of text - that's not a password. "
             "Copy only the password and try again. Nothing was sent.")

print(f"Password from clipboard: {len(pwd)} characters "
      f"(starts with a {'letter' if pwd[0].isalpha() else 'digit' if pwd[0].isdigit() else 'symbol'}, "
      f"{'has' if ' ' in pwd else 'no'} spaces, "
      f"{'has' if any(c in pwd for c in chr(0x201c) + chr(0x201d)) else 'no'} curly quotes)")

# Launcher's parser: a word that starts with " is quoted text where \" and \\ are
# escapes; quotes anywhere else are literal. Quote only when we have to.
if " " in pwd or pwd.startswith('"'):
    arg = '"' + pwd.replace("\\", "\\\\").replace('"', '\\"') + '"'
else:
    arg = pwd


def send(ser, line, wait=1.0, quiet=False):
    ser.write((line + "\n").encode())
    ser.flush()
    end = time.time() + wait
    out = b""
    while time.time() < end:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            out += chunk
            end = max(end, time.time() + 0.5)
    raw = out.decode(errors="replace")
    if not quiet:
        # Mask the password, both as-is and in its JSON-escaped form.
        text = raw.replace(json.dumps(pwd)[1:-1], "********").replace(pwd, "********")
        for row in text.splitlines():
            if row.strip():
                print("  <", row)
    return raw


def kind(c):
    return "quote" if c == '"' else "letter" if c.isalpha() else "digit" if c.isdigit() else "symbol"


def verify(ser):
    """Read Launcher's settings back and compare the stored password - never print it."""
    raw = send(ser, "settings get", quiet=True)
    start, end = raw.find("{"), raw.rfind("}")
    try:
        cfg = json.loads(raw[start:end + 1])
    except Exception:
        print("  ? could not read settings back")
        return
    stored = next((w.get("pwd", "") for w in cfg.get("wifi", []) if w.get("ssid") == ssid), None)
    if stored is None:
        print(f"  ? {ssid} not found in saved settings")
    elif stored == pwd:
        print(f"  = stored password MATCHES what you typed ({len(stored)} characters)")
    else:
        i = next((i for i in range(min(len(stored), len(pwd))) if stored[i] != pwd[i]), min(len(stored), len(pwd)))
        print(f"  ! stored password is DIFFERENT: stored {len(stored)} chars vs typed {len(pwd)}; "
              f"first difference at position {i + 1} "
              f"(typed a {kind(pwd[i]) if i < len(pwd) else 'nothing'}, "
              f"stored a {kind(stored[i]) if i < len(stored) else 'nothing'})")


with serial.Serial(PORT, 115200, timeout=0.2) as ser:
    time.sleep(0.5)
    ser.reset_input_buffer()
    send(ser, "")  # clear any half-typed line
    print(f"> wifi add {ssid} ********")
    send(ser, f"wifi add {ssid} {arg}")
    print("> settings get   (checking what was stored)")
    verify(ser)
    if "--no-connect" not in sys.argv:
        print(f"> wifi connect {ssid}")
        send(ser, f"wifi connect {ssid}", wait=20)
