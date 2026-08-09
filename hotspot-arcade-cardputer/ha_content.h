// Load the baked content packs into the engine at boot.
//
// This is a straight port of content_stream_pack() in
// flipper/hotspot-arcade/helpers/ha_session.c, minus the UART: the grammar is the
// contract ("Key: value" lines, a "---" or blank line ends a block, a "Pack:" key
// names the pack and is not part of an item), and the engine still receives each
// block as a JSON object of the file's own lowercased keys. Keeping the parse
// identical is the point -- pack files stay portable between the Flipper build and
// this one, and all the game semantics stay where they already live, in ha_games.h.
#pragma once
#include <Arduino.h>
#include "ha_games.h"
#include "ha_json.h"
#include "ha_bundle.h"

// copy_trim(): leading and trailing blanks off a [start,end) slice. `lower`
// case-folds ASCII only, matching the Flipper's byte loop -- a UTF-8 lead or
// continuation byte must not be touched by a locale-aware tolower().
//
// Only String operations the sim's off-target shim (sim/engine/Arduino.h) also
// provides are used here, so this loader can be built and tested on a desktop
// against the real engine, not just on the board.
static void haTrimTo(const char* s, const char* e, String& out, bool lower = false) {
    while(s < e && (*s == ' ' || *s == '\t' || *s == '\r')) s++;
    while(e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) e--;
    out = "";
    out.reserve((size_t)(e - s) + 1);
    for(const char* p = s; p < e; p++) {
        char c = *p;
        if(lower && c >= 'A' && c <= 'Z') c = (char)(c + 32);
        out += c;
    }
}

static void haContentLoadPack(Engine& engine, uint8_t game, const char* text, const char* fallback) {
    // Pass one: the pack name, so contentPack() goes first (it opens the pack the
    // items then attach to).
    String name = fallback;
    for(const char* p = text; p && *p;) {
        const char* eol = strchr(p, '\n');
        if(!eol) eol = p + strlen(p);
        if(strncmp(p, "Pack:", 5) == 0) {
            String v;
            haTrimTo(p + 5, eol, v);
            if(v.length()) name = v;
            break;
        }
        p = (*eol) ? eol + 1 : eol;
    }
    engine.contentPack(game, name.c_str());

    // Pass two: blocks.
    String obj = "{";
    String key, val;
    bool any = false;
    for(const char* p = text; p && *p;) {
        const char* eol = strchr(p, '\n');
        if(!eol) eol = p + strlen(p);

        const char* s = p;
        const char* e = eol;
        while(s < e && (*s == ' ' || *s == '\t' || *s == '\r')) s++;
        while(e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) e--;

        bool sep = (s == e) || (e - s == 3 && strncmp(s, "---", 3) == 0);
        if(sep) {
            if(any) {
                obj += "}";
                engine.contentItem(obj.c_str());
            }
            obj = "{";
            any = false;
        } else {
            const char* colon = (const char*)memchr(s, ':', (size_t)(e - s));
            if(colon) {
                haTrimTo(s, colon, key, true);
                haTrimTo(colon + 1, e, val);
                if(key.length() && strcmp(key.c_str(), "pack") != 0) {
                    if(any) obj += ",";
                    obj += "\"";
                    obj += ha_json_escape(key.c_str());
                    obj += "\":\"";
                    obj += ha_json_escape(val.c_str());
                    obj += "\"";
                    any = true;
                }
            }
        }
        p = (*eol) ? eol + 1 : eol;
    }
    if(any) { // a file that ends without a trailing separator
        obj += "}";
        engine.contentItem(obj.c_str());
    }
}

// Stream the baked packs for one language into the engine. The generator caps each
// game at the engine's TRIVIA_MAX_TOPICS packs PER LANGUAGE, and only one language is
// ever loaded at a time, so the cap is never exceeded.
//
// Fallback is per game: a game whose selected language has no packs (an untranslated
// game, or lang="en" which every game has) streams its English packs instead. So a
// partially translated language still plays -- translated games come up localized,
// the rest stay English. Called at boot and again whenever Settings changes language.
static void haContentLoadAll(Engine& engine, const char* lang) {
    engine.contentClear();
    bool hasLang[64] = {false}; // game id -> does the selected language cover it?
    for(size_t i = 0; i < HA_BAKED_PACK_COUNT; i++) {
        const HaBakedPack& bp = HA_BAKED_PACKS[i];
        if(bp.game < 64 && strcmp(bp.lang, lang) == 0) hasLang[bp.game] = true;
    }
    for(size_t i = 0; i < HA_BAKED_PACK_COUNT; i++) {
        const HaBakedPack& bp = HA_BAKED_PACKS[i];
        const char* want = (bp.game < 64 && hasLang[bp.game]) ? lang : "en";
        if(strcmp(bp.lang, want) != 0) continue;
        haContentLoadPack(engine, bp.game, bp.text, bp.fallback);
    }
}

// Stream ONE game's packs, dropping every other game's parsed copy. Only one game is
// ever played at a time, but the parsed Strings of all of them used to sit in the
// heap together -- ~76 KB with the packs filled to their caps, on top of the four new
// games' state, which starved lwIP: the AP still beaconed but DHCP had nothing left
// to answer with, and phones hung at "connecting" forever. Resident content is now
// bounded by the LARGEST single game (~14 KB) no matter how much the flash carries,
// so the packs can keep growing for free. Re-parsing on a game switch is a few ms of
// memory-mapped flash reads.
//
// The caller must push fresh state afterwards (the lobby's pack list changes), and
// every path that changes the active game must come through here -- a game whose
// packs are not loaded shows an empty list and never starts, the exact failure class
// the gen-assets guard exists for.
static void haContentLoadGame(Engine& engine, const char* lang, uint8_t game) {
    engine.contentClear();
    bool hasLang = false;
    for(size_t i = 0; i < HA_BAKED_PACK_COUNT; i++) {
        const HaBakedPack& bp = HA_BAKED_PACKS[i];
        if(bp.game == game && strcmp(bp.lang, lang) == 0) hasLang = true;
    }
    const char* want = hasLang ? lang : "en";
    for(size_t i = 0; i < HA_BAKED_PACK_COUNT; i++) {
        const HaBakedPack& bp = HA_BAKED_PACKS[i];
        if(bp.game != game || strcmp(bp.lang, want) != 0) continue;
        haContentLoadPack(engine, bp.game, bp.text, bp.fallback);
    }
}
