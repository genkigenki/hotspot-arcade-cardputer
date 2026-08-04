// Host-side mirror of the session, for the Cardputer's own screen.
//
// On the Flipper build the engine reports the roster, scores and events over
// UART and flipper/helpers/ha_session.c keeps the mirror the host UI draws from.
// Here the same reports arrive as direct calls -- the .ino implements the engine's
// haUart* sinks by writing into this struct instead of framing bytes. The UI then
// draws from the mirror, never from the engine, which keeps the split (and the
// locking rule) identical to the two-device build.
//
// Every writer below runs under the .ino's ENGINE_LOCK, because every engine call
// that can reach a sink is made under it. Readers must snapshot under the same
// lock -- see haUiSnapshot().
#pragma once
#include <Arduino.h>
#include "ha_games.h"

#define HA_EV_MAX 24 // console scrollback
#define HA_EV_LEN 44 // fits the 240px screen at the 6px font

// Content/UI language, owned by the .ino (0 = en, 1 = de). Declared here so the
// roster event tags below can be logged in the selected language.
extern uint8_t haLang;

struct HaHostPlayer {
    bool used;
    char nick[HA_NICK_LEN];
    int32_t score;
};

struct HaHost {
    HaHostPlayer p[HA_MAX_PLAYERS + 1]; // 1-based, matching pids
    char ev[HA_EV_MAX][HA_EV_LEN];
    uint32_t evTotal; // events ever logged; ring slot = evTotal % HA_EV_MAX
    char lastEvent[HA_EV_LEN];
    uint8_t activeGame;
    bool portalRunning;
    uint32_t rev; // bumped on every change; the UI redraws when it moves
};

static HaHost haHost;

static inline void haHostTouch() {
    haHost.rev++;
}

static inline void haHostLog(const char* s) {
    strlcpy(haHost.ev[haHost.evTotal % HA_EV_MAX], s, HA_EV_LEN);
    haHost.evTotal++;
    haHostTouch();
}

static inline void haHostReset() {
    for(int i = 0; i <= HA_MAX_PLAYERS; i++) haHost.p[i] = HaHostPlayer{};
    haHost.lastEvent[0] = '\0';
    haHost.activeGame = HA_GAME_NONE;
    haHostTouch();
}

// A re-join for a known pid is a rename (the phone's header editor), exactly as
// player_join() treats it on the Flipper: update the nick, keep the score.
// Returns true only for a genuinely new player (not a mid-game rename), so the
// caller can jingle on joins but not on nick edits.
static inline bool haHostJoin(uint8_t pid, const char* nick) {
    if(pid < 1 || pid > HA_MAX_PLAYERS) return false;
    bool isNew = !haHost.p[pid].used;
    haHost.p[pid].used = true;
    strlcpy(haHost.p[pid].nick, nick, HA_NICK_LEN);
    if(isNew) haHost.p[pid].score = 0;
    char line[HA_EV_LEN];
    snprintf(line, sizeof(line), "%s %s",
             isNew ? (haLang ? "DA" : "JOIN") : "NAME", nick);
    haHostLog(line);
    return isNew;
}

static inline void haHostLeave(uint8_t pid) {
    if(pid < 1 || pid > HA_MAX_PLAYERS) return;
    char line[HA_EV_LEN];
    snprintf(line, sizeof(line), "%s %s", haLang ? "WEG" : "LEAVE", haHost.p[pid].nick);
    haHost.p[pid] = HaHostPlayer{};
    haHostLog(line);
}

static inline void haHostScore(uint8_t pid, int delta) {
    if(pid < 1 || pid > HA_MAX_PLAYERS || !haHost.p[pid].used) return;
    haHost.p[pid].score += delta;
    haHostTouch();
}

static inline void haHostSetEvent(const char* s) {
    strlcpy(haHost.lastEvent, s, HA_EV_LEN);
    haHostLog(s);
}

static inline int haHostPlayerCount() {
    int n = 0;
    for(int i = 1; i <= HA_MAX_PLAYERS; i++)
        if(haHost.p[i].used) n++;
    return n;
}
