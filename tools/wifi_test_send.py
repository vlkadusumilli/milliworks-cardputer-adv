"""Send SSID + password to the Wi-Fi diagnostic firmware and show its results.

Run with PlatformIO's Python (it has pyserial), with no serial monitor open:
    <platformio python> tools/wifi_test_send.py "Your Wi-Fi name"

The password is typed hidden and sent raw (no quoting needed). It is never printed.
"""
import getpass
import sys
import time

import serial

import glob
PORT = (glob.glob("/dev/cu.usbmodem*") or ["/dev/cu.usbmodem2101"])[0]
ssid = sys.argv[1] if len(sys.argv) > 1 else input("Wi-Fi name (SSID): ").strip()
pwd = getpass.getpass(f"Type the Wi-Fi password for {ssid} (hidden), then Enter: ")
if not pwd or "\n" in pwd:
    sys.exit("No usable password - nothing was sent.")
print(f"Sending password ({len(pwd)} characters)...")

with serial.Serial(PORT, 115200, timeout=0.2) as ser:
    time.sleep(1.0)
    ser.reset_input_buffer()
    for line in (f"SSID:{ssid}", f"PASS:{pwd}", "GO"):
        ser.write((line + "\n").encode())
        ser.flush()
        time.sleep(0.3)
    buf = ""
    deadline = time.time() + 180          # up to 4 attempts x 20 s, plus scanning
    while time.time() < deadline:
        buf += ser.read(ser.in_waiting or 1).decode(errors="replace")
        while "\n" in buf:
            row, buf = buf.split("\n", 1)
            row = row.strip().replace(pwd, "********")
            if row:
                print("  ", row)
            if row == "DONE":
                sys.exit(0)
    print("Timed out waiting for the test to finish.")
