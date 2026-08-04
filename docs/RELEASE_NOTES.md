### v0.5.0 — German, top to bottom + settings overhaul

- 🌍 **Play fully in German** — a language switch in Settings (English / Deutsch). The **content** (all six content games, 32 packs), the **phone interface** and the **Cardputer's own screen** are all German — buttons, prompts, in-game text, the host menus, settings and event log, the lot. Pick a language once; the host streams the content, relays the UI language to every phone, and switches its own screen. English fallback for anything untranslated.
- ♟️ **Chess** — a 15th game (1v1, full FIDE rules with a blitz clock), from upstream.
- 🔤 **UTF-8-safe games** — Word Scramble and Draw handle umlauts and ß correctly.
- 💾 **Settings on the SD card** — SSID, audio and language survive a reboot and even a full-chip reflash.
- 🎛️ **Redesigned settings screen** — option pills, a language switch with `‹ ›` arrows, `,`/`/` to change a value in place.
- 🎨 **New look** — the host screen matches the phone client's black / orange / white palette.
- 🎮 **The default network** carries a game-pad icon so it stands out in the Wi-Fi list.

Fifteen games now.

### Install

Search for **"Hotspot Arcade"** in the **M5Burner** app or the **M5Launcher** catalog — one tap, no cables.

**Or flash by hand** (keeps M5Launcher):
```
esptool --chip esp32s3 --port <PORT> --baud 921600 write_flash 0x170000 hotspot-arcade-cardputer.ino.bin
```

Cardputer v1 (StampS3, 8MB). Full image and recovery are in the README.
