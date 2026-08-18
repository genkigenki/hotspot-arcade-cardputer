### v0.8.0 — upstream v1.8.0: far more memory headroom

- **Much more room to breathe.** Upstream reworked how game state is held in
  memory, and the effect on the Cardputer is large: static memory use drops by
  19 KB, and the low point during startup — the moment WiFi comes up, where
  phones used to fail to get an address — rises from 23 KB free to 88 KB. Free
  memory during play goes from 99 KB to 119 KB, and the largest single block
  from 36 KB to 48 KB, so there is less fragmentation as well.
- **Reconnecting works on modern iPhones.** iOS randomises its WiFi address,
  which could make a returning phone look like a stranger. Clients are now
  recognised by a stable id instead.
- **Spyfall no longer leaks the location.** When a player left and their slot
  was reused, the next player could inherit the location — in the one game
  whose entire point is that one person does not know it.
- **Werewolf reads correctly again** in English and German; the strings were
  double-encoded. The Fill the Blank and Spyfall packs were refreshed upstream.
- Twenty games, unchanged. This release is about what happens underneath them.

### Install

Search for **"Hotspot Arcade"** in the **M5Burner** app or the **M5Launcher**
catalog — one tap, no cables.

**Or flash by hand.** Note that the address depends on your device: if
M5Launcher manages the app, flash into the slot it created for Hotspot Arcade
rather than assuming a fixed address, and read the partition table first.

```
esptool --chip esp32s3 --port <PORT> --baud 921600 read_flash 0x8000 0xc00 pt.bin
esptool --chip esp32s3 --port <PORT> --baud 921600 write_flash <SLOT> hotspot-arcade-cardputer.ino.bin
```

Cardputer v1 (StampS3, 8MB). Full image and recovery are in the README.

### v0.7.0 — four new games, connection fixes, more cards

- **Fill the Blank** (game 17) — a judge picks the funniest card. Playable with two: the hand is topped up from the deck.
- **Werewolf** (game 18) — hidden roles, night and day phases. Five players or more.
- **Spyfall** (game 19) — one player does not know the location. Three players or more.
- **Draw a Monster** (game 20) — head, torso and legs from three separate hands, nobody sees the whole. Three players or more.
- **Phones connect again.** Twenty games and ninety card packs no longer fit in RAM at the same time. The Cardputer ran out of memory while WiFi was still coming up, so DHCP had nothing left to hand out leases with and phones sat at "connecting". Only the active game's packs are resident now: content grows in flash from here on, not in RAM. A further 17 KB of static memory was freed.
- **Phones stay connected.** The keep-alive was strict enough to drop live iPhones that had gone into power saving; it is more patient now. Socket headroom was raised as well — each phone holds two connections, the captive sheet and the browser, which put five players over the previous limit. Event lines are written to the SD card with timestamps.
- **No more ghost voters.** A phone that disappears without saying so (reload, dead spot, lock screen) now frees its seat: the host probes at TCP level, without the old client heartbeat. Such a ghost could previously block the game-change vote.
- **More cards, English and German:** Draw, Scramble, Spectrum and Secrets to 32 entries per pack, Would You Rather 12→24, Kiss Marry Kill 24→32, plus two new packs — animals and mythical creatures.
- **Upstream v1.7.0** is merged in full: web bundle in LittleFS flash, captive render, Android dark-mode fix, the remaining Portuguese strings, CRC-32 check of the bundle.
- **Cardputer:** the four new games are in the host menu (EN/DE), and a counter overflow is fixed that wrote one byte past the array on every "most played" sort since Secrets.
- Bot players added, for testing.

Twenty games now.

### Install

Search for **"Hotspot Arcade"** in the **M5Burner** app or the **M5Launcher** catalog — one tap, no cables.

**Or flash by hand** (keeps M5Launcher):
```
esptool --chip esp32s3 --port <PORT> --baud 921600 write_flash 0x170000 hotspot-arcade-cardputer.ino.bin
```

Cardputer v1 (StampS3, 8MB). Full image and recovery are in the README.

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
