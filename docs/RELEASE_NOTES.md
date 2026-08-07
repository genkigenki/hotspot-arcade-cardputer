### v0.6.0 — the phones run the party

- 🎮 **Pick the game from your phone.** Tap the switcher, choose, and the others get a prompt: more than half have to agree. Nobody has to reach for the Cardputer any more — and its screen follows the vote, so the host still shows what is actually being played.
- 👤 **One phone, one player.** Identity is bound to the device rather than to the connection, so the captive-portal popup and your browser are the *same* player instead of two. Drop out and come back and your score is still there; a name you type yourself always wins over the one that was remembered for you.
- 🤫 **Secrets** — a 16th game. Everyone answers a question yes/no in secret, then guesses how many said yes. Only the total is ever revealed, never who said what. Three packs, from harmless to very spicy.
- 📊 **Would You Rather now tells you something.** The final screen charts how much the group actually agreed, with the average marked.
- 🇧🇷 **Portuguese** — a third content language beside English and German, straight from upstream. Settings cycles English / Deutsch / Portugues.
- 🔇 **Two fixes you will notice on your phone.** Sound now works on a phone that has played before (it was rejoined automatically, never pressed Play, and so never unlocked audio — silent all evening). And the client no longer polices its own connection every two seconds, which used to make the game-change prompt vanish a second after it appeared.
- 🖥️ **On the Cardputer**: the network name uses the full width of the settings row instead of being cut to "Hotspot Arc", and a small build number sits next to the title so a screenshot says which firmware it is.

Sixteen games now.

### Install

Search for **"Hotspot Arcade"** in the **M5Burner** app or the **M5Launcher** catalog — one tap, no cables.

**Or flash by hand** (keeps M5Launcher):
```
esptool --chip esp32s3 --port <PORT> --baud 921600 write_flash 0x170000 hotspot-arcade-cardputer.ino.bin
```

Cardputer v1 (StampS3, 8MB). Full image and recovery are in the README.

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
