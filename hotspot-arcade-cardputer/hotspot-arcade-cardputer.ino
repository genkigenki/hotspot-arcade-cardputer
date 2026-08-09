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
#include <lwip/etharp.h>
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

// ---------------- connection-stage notes ----------------
// "Phone cannot connect" is undebuggable from a blank dashboard. Every stage of a
// phone's arrival leaves a line in the event log: radio association, the DHCP
// lease, the first page load from that address, the WebSocket. The join itself
// (nickname) was already logged. These fire on the WiFi-event and AsyncTCP tasks,
// which must not write the host mirror, so they queue here and loop() drains them.
static portMUX_TYPE haNoteMux = portMUX_INITIALIZER_UNLOCKED;
static char haNoteBuf[8][40];
static uint8_t haNoteW = 0, haNoteN = 0;

static void haNote(const char* fmt, ...) {
    char line[40];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    portENTER_CRITICAL(&haNoteMux);
    strlcpy(haNoteBuf[haNoteW], line, sizeof(haNoteBuf[0]));
    haNoteW = (uint8_t)((haNoteW + 1) % 8);
    if(haNoteN < 8) haNoteN++;
    portEXIT_CRITICAL(&haNoteMux);
}

static void haNoteDrain() {
    char out[8][40];
    int n = 0;
    portENTER_CRITICAL(&haNoteMux);
    while(haNoteN) {
        uint8_t r = (uint8_t)((haNoteW + 8 - haNoteN) % 8);
        strlcpy(out[n++], haNoteBuf[r], sizeof(out[0]));
        haNoteN--;
    }
    portEXIT_CRITICAL(&haNoteMux);
    for(int i = 0; i < n; i++) haHostLog(out[i]);
}

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
    // Upstream PR #18: the phones can vote the game away from under the host, without
    // haHostSelectGame() ever being called -- and the host screen would go on showing the
    // old one. The engine announces the winner here, and it names the game rather than
    // numbering it, so map the name back to an id and move the mirror with it.
    char gv[16], gname[20];
    if(ha_json_str(json.c_str(), "gamevote", gv, sizeof(gv)) && strcmp(gv, "approved") == 0 &&
       ha_json_str(json.c_str(), "game", gname, sizeof(gname))) {
        uint8_t id = haUiGameIdByName(gname);
        // The engine is about to selectGame(id) itself (this event fires just before).
        // Load the new game's packs NOW so the lobby it then pushes has its list --
        // with per-game loading the previous game's packs are all the engine holds.
        // Same task, same lock, and the content arrays are idle at this moment.
        haContentLoadGame(engine, HA_LANG_CODE[haLang], id);
        haHost.activeGame = id;
        haHostTouch();
        haUiForce = true; // redraw even while sitting on the picker
        char msg[HA_EV_LEN];
        snprintf(msg, sizeof(msg), hu("phones chose %s", "Handys wollen %s"), haUiGameLabel(id));
        haHostSetEvent(msg);
        return;
    }

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
// Finished artwork (Draw a Monster). On the Flipper build these frames cross the UART
// and the Flipper writes an SVG to its SD card; here the Cardputer IS the host, so it
// writes the SVG itself: /hotspot-arcade/art/fd-<seq>-<sheet>.svg. The engine streams
// a sheet one segment per call precisely so nobody buffers a drawing -- the file is
// the buffer. No SD card (or a failed open): every frame drops on the floor and the
// game plays on, exactly like the score history does.
static File haArtFile;
void haUartArt(uint8_t op, const String& json) {
    const char* j = json.c_str();
    if(op == HA_ART_BEGIN) {
        if(haArtFile) haArtFile.close(); // a lost END must not wedge the next sheet
        SD.mkdir(HA_HIST_DIR);
        SD.mkdir("/hotspot-arcade/art");
        // No RTC offline, so files are numbered by an NVS counter (like the session
        // history), plus the sheet id for a stable within-gallery order.
        Preferences pr;
        pr.begin("ha-art", false);
        uint32_t seq = pr.getUInt("seq", 0) + 1;
        pr.putUInt("seq", seq);
        pr.end();
        int id = 0;
        ha_json_int(j, "id", &id);
        char path[48];
        snprintf(path, sizeof(path), "/hotspot-arcade/art/fd-%03u-%d.svg", (unsigned)seq, id);
        haArtFile = SD.open(path, FILE_WRITE);
        if(!haArtFile) return;
        char w0[HA_NICK_LEN] = "", w1[HA_NICK_LEN] = "", w2[HA_NICK_LEN] = "";
        ha_json_str(j, "w0", w0, sizeof(w0));
        ha_json_str(j, "w1", w1, sizeof(w1));
        ha_json_str(j, "w2", w2, sizeof(w2));
        haArtFile.print("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 255 255\" "
                        "stroke=\"#111\" stroke-width=\"3\" stroke-linecap=\"round\" "
                        "fill=\"none\">\n<!-- ");
        haArtFile.print(w0);
        haArtFile.print(" / ");
        haArtFile.print(w1);
        haArtFile.print(" / ");
        haArtFile.print(w2);
        haArtFile.print(" -->\n");
    } else if(op == HA_ART_STROKE) {
        if(!haArtFile) return;
        int x0, y0, x1, y1;
        if(!ha_json_int(j, "x0", &x0) || !ha_json_int(j, "y0", &y0) ||
           !ha_json_int(j, "x1", &x1) || !ha_json_int(j, "y1", &y1))
            return;
        char line[80];
        snprintf(line, sizeof(line), "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\"/>\n", x0, y0,
                 x1, y1);
        haArtFile.print(line);
    } else if(op == HA_ART_END) {
        if(!haArtFile) return;
        haArtFile.print("</svg>\n");
        haArtFile.close();
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

// The IP -> MAC mapping comes from the AP's own DHCP server: ip_event_ap_staipassigned_t
// carries the assigned address *and* the client MAC, so one event handler can keep a
// small table (the AP caps stations well below HA_STA_MAX). A station that got its lease
// before the handler was installed is missing from it; for those, read the MAC out of
// lwIP's ARP cache instead, and if even that misses, key on the IP.

#define HA_STA_MAX 10 // >= ESP_WIFI_MAX_CONN_NUM: the AP cannot hold more leases
#define HA_KEY_MAC 0x01 // key tag: low 48 bits are a station MAC
#define HA_KEY_IP 0x02 // key tag: low 32 bits are an IPv4 address

struct StaLease {
    uint32_t ip; // 0 = free slot
    uint8_t mac[6];
};
static StaLease staLeases[HA_STA_MAX];
// Written from the WiFi event task, read from the async WS task; the critical section
// is a handful of instructions over a 10-entry array.
static portMUX_TYPE leaseMux = portMUX_INITIALIZER_UNLOCKED;

static uint64_t macDeviceKey(const uint8_t* mac) {
    uint64_t v = 0;
    for(int i = 0; i < 6; i++) v = (v << 8) | mac[i];
    return ((uint64_t)HA_KEY_MAC << 56) | v;
}

static void leaseNote(uint32_t ip, const uint8_t* mac) {
    portENTER_CRITICAL(&leaseMux);
    int slot = -1, freeSlot = -1;
    for(int i = 0; i < HA_STA_MAX; i++) {
        if(!staLeases[i].ip) {
            if(freeSlot < 0) freeSlot = i;
        } else if(memcmp(staLeases[i].mac, mac, 6) == 0) {
            slot = i; // same phone, (re)leased
            break;
        } else if(staLeases[i].ip == ip) {
            slot = i; // this address now belongs to a different station
        }
    }
    if(slot < 0) slot = freeSlot >= 0 ? freeSlot : 0;
    staLeases[slot].ip = ip;
    memcpy(staLeases[slot].mac, mac, 6);
    portEXIT_CRITICAL(&leaseMux);
}

static void leasesClear() {
    portENTER_CRITICAL(&leaseMux);
    memset(staLeases, 0, sizeof(staLeases));
    portEXIT_CRITICAL(&leaseMux);
}

static bool leaseMac(uint32_t ip, uint8_t* out) {
    bool found = false;
    portENTER_CRITICAL(&leaseMux);
    for(int i = 0; i < HA_STA_MAX && !found; i++)
        if(staLeases[i].ip == ip) {
            memcpy(out, staLeases[i].mac, 6);
            found = true;
        }
    portEXIT_CRITICAL(&leaseMux);
    return found;
}

// The address a MAC currently holds, for the log line only (0 if unknown).
static uint32_t leaseIp(const uint8_t* mac) {
    uint32_t ip = 0;
    portENTER_CRITICAL(&leaseMux);
    for(int i = 0; i < HA_STA_MAX && !ip; i++)
        if(staLeases[i].ip && memcmp(staLeases[i].mac, mac, 6) == 0) ip = staLeases[i].ip;
    portEXIT_CRITICAL(&leaseMux);
    return ip;
}

// Fallback for a station whose lease we never saw. Read without the lwIP core lock:
// the ARP table is a fixed static array, so the worst case is reading a half-updated
// entry (a wrong MAC, i.e. one extra player) rather than a bad pointer.
static bool arpMac(uint32_t ip, uint8_t* out) {
    for(size_t i = 0; i < ARP_TABLE_SIZE; i++) {
        ip4_addr_t* eip = nullptr;
        struct netif* nif = nullptr;
        struct eth_addr* eth = nullptr;
        if(etharp_get_entry(i, &eip, &nif, &eth) && eip && eth && eip->addr == ip) {
            memcpy(out, eth->addr, 6);
            return true;
        }
    }
    return false;
}

// Which phone a socket is on, as the opaque key the engine stores. 0 = unknown, which
// makes the engine fall back to one player per connection rather than merging clients.
static uint64_t peerDeviceKey(AsyncWebSocketClient* client) {
    uint32_t ip = (uint32_t)client->remoteIP();
    if(!ip || ip == (uint32_t)apIP) return 0; // not a joined station
    uint8_t mac[6];
    if(leaseMac(ip, mac) || arpMac(ip, mac)) return macDeviceKey(mac);
    return ((uint64_t)HA_KEY_IP << 56) | ip; // last resort: the address itself
}

// Render a key for the serial log: "ip=.. mac=.." when we know both.
static String deviceKeyText(uint64_t key) {
    char b[64];
    if((uint8_t)(key >> 56) == HA_KEY_MAC) {
        uint8_t mac[6];
        for(int i = 0; i < 6; i++) mac[i] = (uint8_t)(key >> (40 - 8 * i));
        uint32_t ip = leaseIp(mac);
        String where = ip ? String("ip=") + IPAddress(ip).toString() + " " : String("");
        snprintf(b, sizeof(b), "mac=%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
                 mac[3], mac[4], mac[5]);
        return where + b;
    }
    if((uint8_t)(key >> 56) == HA_KEY_IP)
        return String("ip=") + IPAddress((uint32_t)(key & 0xFFFFFFFFu)).toString() + " mac=?";
    return String("device=unknown");
}

// The AP's DHCP server is where a station's IP and MAC are seen together. Registered
// once in setup(), before any AP comes up, so no lease is missed (see peerDeviceKey).
static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    if(event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
        const uint8_t* m = info.wifi_ap_staconnected.mac;
        haNote("~ Funk: %02X%02X verbunden", m[4], m[5]);
    } else if(event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
        const uint8_t* m = info.wifi_ap_stadisconnected.mac;
        haNote("~ Funk: %02X%02X weg", m[4], m[5]);
    }
    if(event == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED) {
        leaseNote(info.wifi_ap_staipassigned.ip.addr, info.wifi_ap_staipassigned.mac);
        const uint8_t* m = info.wifi_ap_staipassigned.mac;
        haNote("~ DHCP: %02X%02X hat .%u", m[4], m[5],
               (unsigned)(info.wifi_ap_staipassigned.ip.addr >> 24)); // last octet (LE)
    }
    // Deliberately NOT forgetting the lease on STADISCONNECTED. A phone that blips off
    // the AP (screen lock, captive popup closing, walking to the fridge) and returns
    // before DHCP re-announces would look up empty here, fall back to the IP key, and
    // come back as a SECOND player -- one of the "duplicate user" shapes. Keeping the
    // entry is safe: leaseNote() already reclaims a slot when the same IP is handed to
    // a different station, and corrects the IP when the same station re-leases.
}

// Identity trace: the engine calls this for every hello, telling us whether it made a
// new player or recognised a phone that is already playing (a second browser context,
// e.g. the captive mini-browser alongside Safari) and consolidated it onto that player.
void haLogJoin(uint8_t pid, uint64_t deviceKey, const char* nick, bool recognised) {
    String where = deviceKeyText(deviceKey);
    if(recognised)
        Serial.printf(
            "[ha] SAME DEVICE %s -> pid=%u nick=\"%s\" (recognised)  players=%d\n",
            where.c_str(),
            (unsigned)pid,
            nick,
            haHostPlayerCount());
    else
        Serial.printf(
            "[ha] JOIN pid=%u %s nick=\"%s\"  players=%d\n",
            (unsigned)pid,
            where.c_str(),
            nick,
            haHostPlayerCount());
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
        { // First HTTP contact per address -> "the page loaded" stage in the log.
            static uint32_t seen[10] = {};
            static uint8_t seenW = 0;
            uint32_t ip = (uint32_t)request->client()->remoteIP();
            bool isNew = ip != 0;
            for(int i = 0; i < 10 && isNew; i++)
                if(seen[i] == ip) isNew = false;
            if(isNew) {
                seen[seenW] = ip;
                seenW = (uint8_t)((seenW + 1) % 10);
                haNote("~ Seite: .%u laedt", (unsigned)(ip >> 24));
            }
        }
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
    if(type == WS_EVT_CONNECT) {
        // Server-side liveness, so a silently broken link cannot leave a ghost slot.
        // A phone that reloads mid-game or walks out of range without a TCP FIN keeps
        // its player slot "connected" forever: the engine then counts it in the
        // game-change vote, sends its frames into the void, and the vote dies in the
        // timeout while the other phones see nothing (the play-test bug). With a
        // keep-alive period the SERVER pings the socket when idle; a dead peer never
        // ACKs, AsyncTCP's ack timeout closes the connection, WS_EVT_DISCONNECT fires
        // and the slot is parked like any clean leave. This is not the client-side
        // heartbeat we removed -- no JS timers, no self-diagnosis on the phone; the
        // pong comes from the phone's TCP/WS stack without waking the page at all.
        client->keepAlivePeriod(10);
        haNote("~ Socket: .%u offen", (unsigned)((uint32_t)client->remoteIP() >> 24));
    } else if(type == WS_EVT_DISCONNECT) {
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
            engine.onInput(client->id(), peerDeviceKey(client), buf);
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
    // A fresh session leases fresh addresses. Without this the IP -> MAC table
    // outlives the AP, and the next phone to be handed a recycled address resolves
    // to whoever held it last -- a wrong device identity, which is how ghost players
    // and stolen nicknames appear. Upstream's own host clears it in both places.
    leasesClear();
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
    leasesClear();
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
    // Packs for the NEW game first, then the switch: selectGame's first lobby push
    // already carries the pack list, and an empty one reads as "the game is broken".
    haContentLoadGame(engine, HA_LANG_CODE[haLang], game);
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
    // Registered before any AP comes up, so no DHCP lease is missed.
    WiFi.onEvent(onWiFiEvent, ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED);
    WiFi.onEvent(onWiFiEvent, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
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
    // Only the active game's packs live in RAM (per-game loading); at boot that is
    // the plain lobby, so nothing is parsed yet and the heap stays at its widest
    // exactly when the AP and lwIP are spinning up.
    haContentLoadGame(engine, HA_LANG_CODE[haLang], haHost.activeGame);
    engine.setLang(HA_LANG_CODE[haLang]); // relay the phone-UI language to joiners
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
        haContentLoadGame(engine, HA_LANG_CODE[haLang], haHost.activeGame);
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

    // Drain the connection-stage notes queued by the WiFi-event and AsyncTCP tasks
    // (they must not write the host mirror themselves) into the event log.
    haNoteDrain();

    // The header shows the live free heap; nudge a redraw every 2s so it does not
    // freeze on the value from the last state change.
    {
        static uint32_t hbAt = 0;
        if((int32_t)(millis() - hbAt) >= 2000) {
            hbAt = millis();
            haUiForce = true;
        }
    }

    haUiTick();
}
