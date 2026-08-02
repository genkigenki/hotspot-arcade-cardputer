### v0.5.0 — German language + settings overhaul

- 🌍 **Play in German** — a language switch in Settings (English / Deutsch). All six content games are fully translated (Trivia, Would You Rather, Spectrum, Kiss Marry Kill, Word Scramble, Draw). Pick a language once and the host streams it to every phone, falling back to English per game where a language has none.
- 🔤 **UTF-8-safe games** — Word Scramble and Draw now handle accented letters and ß correctly (an upstream fix, vendored in).
- 💾 **Settings on the SD card** — SSID, audio and language survive a reboot and even a full-chip reflash (they weren't persisted before).
- 🎛️ **Redesigned settings screen** — values sit in pills that turn orange when editable, off/low/high and on/off as option pills, a language switch with `‹ ›` arrows, and `,`/`/` to change a value in place.
- 🎨 **New look** — the host screen matches the phone client's black / orange / white palette.
- 🎮 **The default network** carries a game-pad icon so it stands out in the Wi-Fi list.

Same fourteen games as v0.4.

### Install

Search for **"Hotspot Arcade"** in the **M5Burner** app or the **M5Launcher** catalog — one tap, no cables.

**Or flash by hand** (keeps M5Launcher):
```
esptool --chip esp32s3 --port <PORT> --baud 921600 write_flash 0x170000 hotspot-arcade-cardputer.ino.bin
```

Cardputer v1 (StampS3, 8MB). Full image and recovery are in the README.
