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

// Inflate one baked pack and hand it to the parser above. Packs are stored raw-DEFLATE'd
// (see tools/gen-assets.mjs): the text is ~44% of its size in flash, which bought ~57 KB
// in an app slot that had 11.8 KB spare.
//
// The decompressor is free. tinfl_decompress_mem_to_mem() lives in the ESP32-S3's ROM
// (esp32s3.rom.ld exports it at 0x4000084c), so there is no library to vendor and no
// flash spent on the inflater -- only the ~1-3 KB heap buffer one pack needs while it is
// being parsed. flags = 0 means a bare deflate stream: no zlib header, no adler32.
//
// What comes out is the pack file BYTE FOR BYTE, so haContentLoadPack() and the grammar
// it implements are untouched. That is the point: the parse stays identical to the
// Flipper's content_stream_pack(), and compression stays an envelope rather than a
// second dialect only this host can read.
extern "C" size_t tinfl_decompress_mem_to_mem(
    void* pOut_buf, size_t out_buf_len, const void* pSrc_buf, size_t src_buf_len, int flags);

static void haContentLoadPackZ(
    Engine& engine,
    uint8_t game,
    const uint8_t* z,
    uint32_t zlen,
    uint32_t rawlen,
    const char* fallback) {
    // +1 for the NUL the parser's strchr()/strncmp() walk needs.
    char* buf = (char*)malloc((size_t)rawlen + 1);
    if(!buf) {
        Serial.printf("[ha] pack %s: no heap for %u bytes\n", fallback, (unsigned)rawlen);
        return;
    }
    size_t got = tinfl_decompress_mem_to_mem(buf, (size_t)rawlen, z, (size_t)zlen, 0);
    if(got != (size_t)rawlen) {
        // Only reachable if the generator and this loader disagree, so say so loudly
        // rather than feeding the parser a truncated pack it would silently half-read.
        Serial.printf(
            "[ha] pack %s: inflate gave %u of %u bytes\n", fallback, (unsigned)got, (unsigned)rawlen);
        free(buf);
        return;
    }
    buf[rawlen] = 0;
    haContentLoadPack(engine, game, buf, fallback);
    free(buf);
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
        haContentLoadPackZ(engine, bp.game, bp.z, bp.zlen, bp.rawlen, bp.fallback);
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
    uint32_t t0 = micros();
    unsigned h0 = (unsigned)ESP.getFreeHeap();
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
        haContentLoadPackZ(engine, bp.game, bp.z, bp.zlen, bp.rawlen, bp.fallback);
    }
    // Logged because this used to run on the AsyncTCP callback and cost the room its
    // countdown frames. If the number ever creeps back up, that is why it is here.
    Serial.printf(
        "[ha] packs game=%u in %lu us, heap %u -> %u, largest %u\n",
        (unsigned)game,
        (unsigned long)(micros() - t0),
        h0,
        (unsigned)ESP.getFreeHeap(),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}
