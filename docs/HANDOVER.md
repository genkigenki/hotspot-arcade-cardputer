# Übergabe: Stand 2026-08-10, Build #26

Für eine lokale Claude-Sitzung auf dem Mac (Cardputer per USB-C). Vorgeschichte und
Regeln: siehe auch `UPSTREAM.md` und die Commit-Messages auf diesem Branch
(`claude/latest-version-check-7x7frj`) — sie sind ausführlich und erklären jede
Entscheidung.

## Was hier liegt

v0.7.0-Linie: 20 Spiele (vier neue: Fill the Blank 17, Werwolf 18, Spyfall 19,
Draw a Monster 20), erweiterte Karten (90 Packs EN/DE, Build-#21-Inhalt), Upstream
v1.7.0+3 komplett. Vendor-Pin = Branch `integration-cardputer-07` auf
`genkigenki/hotspot-arcade` (master + PR #24 + PR #25 + Content + Bots + Size-Knobs).

Build-Historie dieses Debug-Tags (Nummer steht auf dem Display neben dem Titel):

- **#22** v0.7.0-Basis. Auf Hardware: iPhones hingen ewig bei „Verbinden" — OOM
  (alle 90 Packs resident + neue Spiele, lwIP bekam kein DHCP mehr heraus).
- **#23** Packs werden nur noch fürs AKTIVE Spiel geparst (`haContentLoadGame`,
  `ha_content.h`) — Content wächst ab jetzt nur im Flash, nie im RAM. Heap-Anzeige
  im Titel (`#NN 47k`), Verbindungstreppe im Event-Log
  (`~ Funk → ~ DHCP → ~ Seite → ~ Socket → Join`).
- **#24** −17 KB statisch (`FD_PANEL_STROKES 96`, `TRIVIA_MAX_QS 15` — Overrides im
  .ino VOR dem ha_games.h-Include; die #ifndef-Guards liegen upstream). Sprite-Drop
  bei <90k nach Portal-Start. Serial-Telemetrie (`[ha] heap free/min/maxblock`,
  alle 10 s). Spielnamen im Handy-Switcher/Votebox lokalisiert. Gemessen: 91k frei.
- **#25** Flacker-Fix: Heap-Ticker zeichnet nur noch die Titelleiste, nur bei
  geänderter Zahl (`haUiHeaderRefresh`).
- **#26** Stabilität nach 5-Spieler-Test (ständige Drops, teils alle zugleich):
  1. Keepalive entschärft: `keepAlivePeriod(30)` + `setAckTimeout(15000)` statt
     10 s/5 s — die alte Kombi hat LEBENDE iPhones im Stromsparmodus gekillt.
  2. `ws.cleanupClients(AP_MAX_CONN * 2)` — der Bibliotheks-Default kappt ab dem
     9. Socket den ältesten; 5 Handys × (Captive + Browser) sind 10.
  3. SD-Debug-Log: jede Event-Zeile mit Zeitstempel nach
     `/hotspot-arcade/debug.log` (+ USB-Serial-Spiegel).

## Bauen / Flashen / Prüfen (auf dem Mac)

```sh
tools/build.sh --deps   # einmalig: arduino-cli + esp32 core 3.3.11 + M5Cardputer
tools/build.sh          # baut build/hotspot-arcade-cardputer.ino.bin
python3 -m esptool --chip esp32s3 --port /dev/cu.usbmodem* --baud 921600 \
  write_flash 0x170000 build/hotspot-arcade-cardputer.ino.bin
screen /dev/cu.usbmodem* 115200   # [ha]-Zeilen: Heap + Verbindungstreppe
```

NIEMALS `erase_flash`, niemals nach `0x0` (dort wohnt der M5Launcher). Bei jedem
geflashten Build `HA_BUILD_NO` in `ha_ui.h` um eins erhöhen. Web-Client-Änderungen
passieren im Upstream-Klon (`integration-cardputer-07`), dann
`node tools/sync-upstream.mjs <pfad>` + `node tools/gen-assets.mjs` — vendor/ nie
direkt editieren. Vor jedem Build gegentesten: upstream `sim/test/all.sh` (22 Tests)
und bei Client-Änderungen der Simulator im Browser (`sim/serve.sh`).

## Offen / als Nächstes

1. **#26-Feldtest**: 2+ Handys, mehrere Runden — fliegen noch Handys? SD-Log lesen.
2. **KMK-Reveal-Verdacht** (Wählerin-Eingabe „falsch angezeigt"): Engine-Mapping ist
   testgedeckt; vermutlich Drop-Artefakt. Nach #26 gezielt nachstellen.
3. **Captive-Splash**: Die Captive-Seite soll NICHT mehr der Spielclient sein,
   sondern eine Mini-Seite „Verbunden — öffne den Browser / http://arcade.page".
   Motivation: iOS-Sheet meldet sich sonst als eigener Spieler-Kontext an (Sockets
   verdoppeln sich; die MAC-Verschmelzung fängt die Identität, aber nicht den
   Socket). DNS fängt bereits alle Hostnamen; `arcade.page` funktioniert per http.
4. **Release**: NICHT taggen vor bestandenem Feldtest — ein `v*`-Tag published
   GitHub-Release UND M5Burner-Katalog automatisch. Wenn stabil: Branch nach main
   mergen, `docs/RELEASE_NOTES.md` prüfen (v0.7.0-Abschnitt liegt bereit), Tag.
5. **PR #24/#25 upstream rebasen** (tarikbc): Zutaten liegen in
   `integration-cardputer-07`; von Hand mergen, nie `-X theirs`.
6. Regeln aus dem Alt-Bericht gelten weiter: kein Client-Heartbeat, WLAN des
   Rechners nicht anfassen, M5Stack nicht löschen, `WiFi.softAPdisconnect(true)`
   bleibt, Tempo an tarik ausrichten.
