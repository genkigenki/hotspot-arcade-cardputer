// Hotspot Arcade firmware for the M5Stack Cardputer (ESP32-S3).
//
// Same game engine as esp32/hotspot-arcade-fw, collapsed onto one device: the
// Cardputer runs the open AP + captive portal + WebSocket referee AND is its own
// host, so there is no Flipper, no UART link, and nothing to flash a second board
// with. The web bundle and the content packs are baked into flash by
// tools/gen-cardputer-assets.mjs instead of being streamed in at session start.
//
// The engine (ha_games.h) is untouched. It reports to its host through the same
// six haUart* sinks; here they write into the on-screen mirror (ha_host.h) rather
// than framing UART bytes. docs/PROTOCOL.md still describes the message set --
// this build just delivers it by function call.
//
// For education/fun on your own hardware. It runs an OPEN access point and a
// catch-all captive page; only operate it where that is allowed.

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>
#include <esp_ota_ops.h>
#include <SD.h>
#include <SPI.h>
#include <M5Cardputer.h>

#include "ha_proto.h"
#include "ha_json.h"
#include "ha_bundle.h"
#include "ha_games.h"
#include "ha_host.h"
#include "ha_history.h"
#include "ha_content.h"
#include "ha_ui.h"

#define WS_MSG_MAX 512
// 10 is the ESP32-S3 softAP hardware maximum (ESP_WIFI_MAX_CONN_NUM). More phones
// than this cannot associate no matter what -- the chip, not the code, is the cap.
#define AP_MAX_CONN 10

// ---- host speaker: short jingles, respecting the audio level set in the UI ----
// 0 = off, 1 = low, 2 = high. Stored here; the UI settings screen changes it.
uint8_t haAudioLevel = 1;

// Content language (see ha_ui.h): 0 English, 1 Deutsch. Persisted in NVS. The Settings
// screen changes it and sets haLangDirty; loop() then re-streams the packs.
uint8_t haLang = 0;
bool haLangDirty = false;

static void haBeep(uint16_t freq, uint16_t ms) {
    if(haAudioLevel == 0) return;
    M5Cardputer.Speaker.setVolume(haAudioLevel == 2 ? 200 : 80);
    M5Cardputer.Speaker.tone(freq, ms);
}
// Single notes: consecutive tone() calls replace each other rather than queue, and
// the join/leave sinks run on the async task where a blocking delay is unwelcome.
static void haJingleUp() { haBeep(1319, 160); }   // AP came up: clear high note
static void haJingleJoin() { haBeep(1568, 90); }  // a phone joined: bright blip up
static void haJingleLeave() { haBeep(523, 130); } // a phone left: low blip

static DNSServer dnsServer;
static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static IPAddress apIP(192, 168, 4, 1);
// A game-pad emoji in the default SSID makes the network jump out in a phone's Wi-Fi
// list. SSIDs are UTF-8 up to 32 bytes; the emoji is 4, so this fits with room to
// spare. (The host's own screen font has no emoji glyph, so it shows a placeholder
// there -- cosmetic; the phones that matter render it fine.)
static char apName[33] = "\xF0\x9F\x8E\xAE Hotspot Arcade";
static bool portalRunning = false;

static Engine engine;

// Engine state is touched from the loop task (tick, host actions) and from the
// AsyncTCP task (WebSocket events), so it is guarded exactly as in the two-device
// firmware. The host mirror is written only from inside sinks, which are only ever
// reached from an engine call, so this one lock covers both.
static SemaphoreHandle_t engineMutex = nullptr;
#define ENGINE_LOCK() xSemaphoreTakeRecursive(engineMutex, portMAX_DELAY)
#define ENGINE_UNLOCK() xSemaphoreGiveRecursive(engineMutex)

// ---------------- sinks used by the engine ----------------

void haWsSendWs(uint32_t wsId, const String& msg) {
    if(!wsId) return;
    ws.text(wsId, msg);
}
void haWsBroadcast(const String& msg) {
    ws.textAll(msg);
}
void haUartJoin(uint8_t pid, const char* nick) {
    if(haHostJoin(pid, nick)) haJingleJoin(); // jingle on a new join, not a rename
}
void haUartLeave(uint8_t pid) {
    haHostLeave(pid);
    haJingleLeave();
}
void haUartScore(uint8_t pid, int delta, const char* reason) {
    (void)reason;
    haHostScore(pid, delta);
}
void haUartEvent(const String& json) {
    // Same keys the Flipper's console picks out of the event feed.
    char ev[HA_EV_LEN];
    if(ha_json_str(json.c_str(), "duel", ev, sizeof(ev)) ||
       ha_json_str(json.c_str(), "pong", ev, sizeof(ev)) ||
       ha_json_str(json.c_str(), "draw", ev, sizeof(ev))) {
        haHostSetEvent(ev);
    } else if(ha_json_str(json.c_str(), "chat", ev, sizeof(ev))) {
        haHostLog(ev); // lobby chatter, not a game status line
    }
}
static const char* haNick(int pid) {
    if(pid >= 1 && pid <= HA_MAX_PLAYERS && haHost.p[pid].used) return haHost.p[pid].nick;
    return "?";
}

// Round results are pid-shaped on the wire ({"win":2,"lose":3}), which the Flipper
// prints raw because its console is four lines of 5x7. There is room here, and the
// host is the only screen that can name the players, so resolve them.
void haUartRoundResult(const String& json) {
    const char* j = json.c_str();
    char buf[HA_EV_LEN];
    char s[HA_EV_LEN];
    int win = 0, lose = 0;
    if(ha_json_int(j, "win", &win)) {
        ha_json_int(j, "lose", &lose);
        snprintf(buf, sizeof(buf), "%s beat %s", haNick(win), haNick(lose));
    } else if(
        ha_json_str(j, "trivia", s, sizeof(s)) || ha_json_str(j, "draw", s, sizeof(s)) ||
        ha_json_str(j, "scramble", s, sizeof(s)) || ha_json_str(j, "react", s, sizeof(s)) ||
        ha_json_str(j, "wyr", s, sizeof(s))) {
        snprintf(buf, sizeof(buf), "%s", s); // "final", or "ALICE got it"
    } else {
        const char* d = ha_json_find(j, "draw");
        if(d && *d == '[') strlcpy(buf, "round drawn", sizeof(buf)); // {"draw":[a,b]}
        else strlcpy(buf, j, sizeof(buf));
    }
    haHostSetEvent(buf);
}

// ---------------- HTTP (captive) ----------------

// Serve the baked web bundle for every host/path so the captive portal always
// resolves. GET "/" (and every OS captive-probe URL) gets the app; other bundled
// paths are served by exact match. Identical policy to the streamed build, just
// reading from flash instead of a heap buffer.
static const HaBakedFile* haFindFile(const char* path) {
    for(size_t i = 0; i < HA_BAKED_FILE_COUNT; i++)
        if(strcmp(HA_BAKED_FILES[i].path, path) == 0) return &HA_BAKED_FILES[i];
    return nullptr;
}

class ArcadeHandler : public AsyncWebHandler {
public:
    bool canHandle(AsyncWebServerRequest* request) const override {
        (void)request;
        return true;
    }
    void handleRequest(AsyncWebServerRequest* request) override {
        const HaBakedFile* a = haFindFile(request->url().c_str());
        if(!a && HA_BAKED_FILE_COUNT) a = &HA_BAKED_FILES[0]; // captive probes -> the app
        if(!a) {
            request->send(200, "text/html", "<h1>Hotspot Arcade</h1><p>No bundle baked in.</p>");
            return;
        }
        AsyncWebServerResponse* res = request->beginResponse(200, a->mime, a->data, a->len);
        if(a->gzip) res->addHeader("Content-Encoding", "gzip");
        res->addHeader("Cache-Control", "no-store");
        request->send(res);
    }
};

// ---------------- WebSocket ----------------

static void onWsEvent(
    AsyncWebSocket* srv,
    AsyncWebSocketClient* client,
    AwsEventType type,
    void* arg,
    uint8_t* data,
    size_t len) {
    (void)srv;
    if(type == WS_EVT_DISCONNECT) {
        ENGINE_LOCK();
        engine.onWsDisconnect(client->id());
        ENGINE_UNLOCK();
    } else if(type == WS_EVT_DATA) {
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT &&
           len < WS_MSG_MAX) {
            char buf[WS_MSG_MAX];
            memcpy(buf, data, len);
            buf[len] = '\0';
            ENGINE_LOCK();
            engine.onInput(client->id(), buf);
            ENGINE_UNLOCK();
        }
    }
}

// ---------------- AP lifecycle ----------------

// Handlers are registered once, not per start: the SSID editor stops and restarts
// the portal, and addHandler() has no matching remove, so re-registering on every
// start would stack a new ArcadeHandler (and leak it) each time the host renames
// the AP. The Flipper build never noticed because it re-flashed state instead.
static void installHandlers() {
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.addHandler(new ArcadeHandler()).setFilter(ON_AP_FILTER);
}

static void startPortal() {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(apName, nullptr, 1, 0, AP_MAX_CONN); // open AP
    delay(100);

    dnsServer.start(53, "*", apIP);
    server.begin();
    portalRunning = true;

    ENGINE_LOCK();
    haHost.portalRunning = true;
    haHostLog(hu("AP up", "AP an"));
    ENGINE_UNLOCK();
    haJingleUp();
    Serial.printf("[ha] AP \"%s\" up at %s\n", apName, WiFi.softAPIP().toString().c_str());
}

static void stopPortal() {
    if(portalRunning) {
        ws.closeAll();
        server.end();
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        portalRunning = false;
    }
    ENGINE_LOCK();
    engine.reset();
    haHostReset();
    haHost.portalRunning = false;
    haHostLog(hu("AP stopped", "AP aus"));
    ENGINE_UNLOCK();
    Serial.println("[ha] AP stopped");
}

// ---------------- host actions (called from the UI, on the loop task) ----------

void haHostSelectGame(uint8_t game) {
    ENGINE_LOCK();
    engine.selectGame(game);
    haHost.activeGame = game;
    haHostLog(hu("game changed", "Spiel gewechselt"));
    ENGINE_UNLOCK();
}

void haHostResetScores() {
    ENGINE_LOCK();
    engine.resetScores();
    for(int i = 1; i <= HA_MAX_PLAYERS; i++)
        if(haHost.p[i].used) haHost.p[i].score = 0;
    haHostLog(hu("scores reset", "Punkte zurueckgesetzt"));
    ENGINE_UNLOCK();
}

void haHostRoundEnd() {
    ENGINE_LOCK();
    engine.roundEnd();
    haHostLog(hu("round ended", "Runde beendet"));
    ENGINE_UNLOCK();
}

void haHostApplySsid(const char* ssid) {
    strlcpy(apName, ssid, sizeof(apName));
    haCfgSave();
    bool wasUp = portalRunning;
    if(wasUp) stopPortal();
    if(wasUp) startPortal();
}

void haHostTogglePortal() {
    if(portalRunning) stopPortal();
    else startPortal();
}

const char* haHostSsid() {
    return apName;
}

String haHostIp() {
    return portalRunning ? WiFi.softAPIP().toString() : String("--");
}

// Copy the mirror under the lock so the UI can spend milliseconds drawing without
// holding up the WebSocket task.
void haHostSnapshot(HaHost& dst) {
    ENGINE_LOCK();
    memcpy(&dst, &haHost, sizeof(HaHost));
    ENGINE_UNLOCK();
}

// ---------------- Arduino entry ----------------

// Cardputer v1 microSD is on its own SPI bus: SCK=40, MISO=39, MOSI=14, CS=12.
// This is separate from the display bus, so mounting it here doesn't disturb the UI.
static SPIClass haSdSpi(FSPI);
bool haSdOk = false; // non-static: ha_history.h reads it via `extern`
static void haSdBegin() {
    haSdSpi.begin(40, 39, 14, 12);
    haSdOk = SD.begin(12, haSdSpi, 20000000);
    if(haSdOk)
        Serial.printf(
            "[ha] SD ok: %lluMB, type %d\n",
            SD.cardSize() / (1024ULL * 1024ULL),
            (int)SD.cardType());
    else
        Serial.println("[ha] SD: no card or mount failed");
}

// Settings (SSID, audio, language) live on the SD card next to the leaderboard, so
// they survive not just a reboot but a full-chip reflash that wipes NVS. The card is
// the durable store; NVS is only a fallback for a board with no SD. Format is plain
// key=value the user can read or edit:  /hotspot-arcade/config.txt
static const char* HA_CFG_PATH = "/hotspot-arcade/config.txt";

void haCfgSave() { // non-static: the UI (ha_ui.h) calls it on every settings change
    if(!haSdOk) return;
    SD.mkdir("/hotspot-arcade");
    SD.remove(HA_CFG_PATH); // truncate by removing first
    File f = SD.open(HA_CFG_PATH, FILE_WRITE);
    if(!f) return;
    f.printf("ssid=%s\n", apName);
    f.printf("audio=%u\n", (unsigned)haAudioLevel);
    f.printf("lang=%u\n", (unsigned)haLang);
    f.close();
}

static void haCfgLoad() { // overrides NVS/defaults when the card has a config
    if(!haSdOk) return;
    File f = SD.open(HA_CFG_PATH, FILE_READ);
    if(!f) return;
    while(f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        int eq = line.indexOf('=');
        if(eq <= 0) continue;
        String k = line.substring(0, eq), v = line.substring(eq + 1);
        if(k == "ssid") { if(v.length()) strlcpy(apName, v.c_str(), sizeof(apName)); }
        else if(k == "audio") { int a = v.toInt(); if(a >= 0 && a <= 2) haAudioLevel = (uint8_t)a; }
        else if(k == "lang") { int l = v.toInt(); if(l >= 0 && l < HA_LANG_COUNT) haLang = (uint8_t)l; }
    }
    f.close();
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    Serial.begin(115200);
    haSdBegin();

    // M5Launcher installs apps with the ESP-IDF OTA rollback flag set: an app that
    // boots but never confirms itself gets rolled back to the launcher on the next
    // reset. This firmware is healthy the moment it reaches here, so confirm the
    // image -- otherwise any reset (incl. a host toggling USB DTR/RTS) bounces us
    // back to the launcher in an endless launcher->app->reset loop. No-op on a
    // plain esptool flash where there's nothing pending to confirm.
    esp_ota_mark_app_valid_cancel_rollback();

    engineMutex = xSemaphoreCreateRecursiveMutex();
    haUiBegin();
    installHandlers();

    { // restore settings: NVS as a fallback, then the SD card (durable) overrides
        Preferences p;
        if(p.begin("ha_cfg", true)) { haLang = p.getUChar("lang", 0); p.end(); }
        if(haLang >= HA_LANG_COUNT) haLang = 0;
    }
    haCfgLoad(); // SSID, audio and language off the SD card, if present

    ENGINE_LOCK();
    engine.reset();
    haHostReset();
    haContentLoadAll(engine, HA_LANG_CODE[haLang]); // baked packs for the chosen language
    engine.setLang(HA_LANG_CODE[haLang]);           // relay the phone-UI language to joiners
    haHostLog(hu("packs loaded", "Packs geladen"));
    ENGINE_UNLOCK();
    Serial.printf(
        "[ha] %u web file(s), %u pack(s), free heap %u\n",
        (unsigned)HA_BAKED_FILE_COUNT,
        (unsigned)HA_BAKED_PACK_COUNT,
        (unsigned)ESP.getFreeHeap());

    startPortal();
    haUiDraw();
}

void loop() {
    M5Cardputer.update();
    haUiPumpKeys();

    if(haLangDirty) { // Settings changed the language -> re-stream that language's packs
        haLangDirty = false;
        ENGINE_LOCK();
        haContentLoadAll(engine, HA_LANG_CODE[haLang]);
        engine.setLang(HA_LANG_CODE[haLang]); // new joiners get the localized phone UI
        ENGINE_UNLOCK();
        // Restart the active game so the phones get the new language right away: a
        // fresh lobby with the new pack names instead of waiting for the next round.
        if(haHost.activeGame != HA_GAME_NONE) haHostSelectGame(haHost.activeGame);
        haHostLog(haLang ? "Sprache: Deutsch" : "Language: English");
    }

    if(portalRunning) {
        dnsServer.processNextRequest();
        ws.cleanupClients();
        ENGINE_LOCK();
        engine.tick(millis());
        ENGINE_UNLOCK();
    }

    haUiTick();
}
