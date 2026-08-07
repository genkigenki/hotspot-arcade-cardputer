#!/usr/bin/env node
// Bake the vendored web bundle and content packs into a C header, and copy the
// vendored engine headers into the sketch folder.
//
//   node tools/gen-assets.mjs
//
// Reads  vendor/web/{manifest.json,*.gz}, vendor/packs/<game>/*.txt, vendor/engine/*.h
// Writes hotspot-arcade-cardputer/{ha_bundle.h, ha_proto.h, ha_json.h, ha_games.h}
//
// Why the copy: arduino-cli copies the sketch folder to a temp build dir, so an
// include reaching outside it would not resolve -- the sketch has to be
// self-contained. vendor/ is the single source; run tools/sync-upstream.mjs to
// refresh it, then this.

import { readFileSync, writeFileSync, readdirSync, existsSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const sketch = join(root, 'hotspot-arcade-cardputer');
const vendor = join(root, 'vendor');
const out = join(sketch, 'ha_bundle.h');

// ---- web bundle -------------------------------------------------------------
const manifest = JSON.parse(readFileSync(join(vendor, 'web', 'manifest.json'), 'utf8'));
const files = manifest.map((m, i) => ({
  ...m,
  bytes: readFileSync(join(vendor, 'web', m.file)),
  sym: `HA_WEB_${i}`,
}));

const hex = (buf) => {
  const lines = [];
  for (let i = 0; i < buf.length; i += 16) {
    lines.push(
      '    ' + [...buf.subarray(i, i + 16)].map((b) => '0x' + b.toString(16).padStart(2, '0')).join(','),
    );
  }
  return lines.join(',\n');
};

// ---- content packs ----------------------------------------------------------
// ha_content.h parses the same "Key: value" + "---" grammar the Flipper host
// implements, so pack files stay portable between the two hosts. The engine keeps
// at most TRIVIA_MAX_TOPICS (6) packs per game, so cap here rather than silently
// baking bytes that get dropped at runtime.
const GAMES = [
  ['trivia', 'HA_GAME_TRIVIA'],
  ['wyr', 'HA_GAME_WYR'],
  ['scramble', 'HA_GAME_SCRAMBLE'],
  ['draw', 'HA_GAME_DRAW'],
  ['spectrum', 'HA_GAME_SPECTRUM'],
  ['kmk', 'HA_GAME_KMK'],
  ['secrets', 'HA_GAME_SECRETS'], // upstream PR #17
];
const MAX_PER_GAME = 6; // per game AND per language: only one language is ever pushed

// Languages are DISCOVERED inside vendor/, not listed here, and they are upstream's own.
// Upstream's convention is English at the root of packs/<game>/ and one subdirectory per
// translation (de/, pt-br/, ...). This fork used to keep a second German tree of its own
// (packs-de/), which meant every upstream content change had to be copied by hand -- and it
// drifted: upstream's Secrets ships packs/secrets/de/ that the separate tree never had, so
// German Secrets simply had no content. Reading the subdirectories keeps vendor 1:1 and
// picks up any language upstream adds with no change here.
//
// The host bakes every language and streams only the one Settings selects, falling back to
// English per game where that language has no pack -- so the engine's per-game cap applies
// per language, not across all of them.
const langDirsFor = (gameDir) => {
  const out = [['en', gameDir]]; // flat .txt files at the root are English
  for (const e of readdirSync(gameDir, { withFileTypes: true }))
    if (e.isDirectory()) out.push([e.name.toLowerCase(), join(gameDir, e.name)]);
  return out;
};

// Stop the build if upstream ships packs for a game this table does not know. That is a
// one-line fix here, but as a silent omission it is expensive: the packs get baked with no
// game id, so the game appears in the picker, offers an empty pack list, and never starts a
// round -- with nothing anywhere saying why. This has cost real debugging time three times
// now (Secrets twice, Spyfall once), which is what a build-time error is for.
{
  const shipped = readdirSync(join(vendor, 'packs'), { withFileTypes: true })
    .filter((e) => e.isDirectory())
    .map((e) => e.name);
  const known = new Set(GAMES.map(([sub]) => sub));
  const orphans = shipped.filter((d) => !known.has(d));
  if (orphans.length) {
    throw new Error(
      `vendor/packs has ${orphans.length} directory(ies) this host cannot place: ${orphans.join(', ')}\n` +
        `Add each to the GAMES table in tools/gen-assets.mjs with its HA_GAME_* constant,\n` +
        `or the packs bake with no game id and the game silently has no content.`,
    );
  }
}

const packs = [];
const dropped = [];
const langs = new Set();
for (const [sub, gameConst] of GAMES) {
  const gameDir = join(vendor, 'packs', sub);
  if (!existsSync(gameDir)) continue;
  for (const [lang, dir] of langDirsFor(gameDir)) {
    const names = readdirSync(dir).filter((n) => n.toLowerCase().endsWith('.txt')).sort();
    if (names.length) langs.add(lang);
    for (const name of names.slice(0, MAX_PER_GAME)) {
      packs.push({
        gameConst,
        lang,
        fallback: name.slice(0, -4),
        text: readFileSync(join(dir, name), 'utf8').replace(/\r\n/g, '\n'),
      });
    }
    for (const name of names.slice(MAX_PER_GAME)) dropped.push(`${lang}:${sub}/${name}`);
  }
}

const DELIM = 'HAPACK';
for (const p of packs) {
  if (p.text.includes(`)${DELIM}"`)) {
    throw new Error(`pack ${p.fallback} contains the raw-string delimiter )${DELIM}"`);
  }
}

const totalWeb = files.reduce((n, f) => n + f.bytes.length, 0);
const totalPacks = packs.reduce((n, p) => n + p.text.length, 0);

let h = `// GENERATED by tools/gen-assets.mjs -- do not edit by hand.
//
// The Cardputer has no Flipper to stream assets from, so the web bundle and the
// content packs live in flash. Re-generate after changing anything under vendor/:
//     node tools/gen-assets.mjs
//
// web bundle: ${totalWeb} bytes in ${files.length} file(s); packs: ${totalPacks} bytes in ${packs.length} pack(s).
#pragma once
#include <Arduino.h>
#include "ha_proto.h"

// \`const\` matters: const data lands in .rodata, which is memory-mapped flash on
// the ESP32-S3 and readable with a plain pointer. Drop the const and all of this
// gets copied into RAM at boot, which will not fit.
`;

for (const f of files) h += `\nstatic const uint8_t ${f.sym}[] = {\n${hex(f.bytes)}};\n`;

h += `
struct HaBakedFile {
    const char* path;
    const char* mime;
    bool gzip;
    const uint8_t* data;
    size_t len;
};

static const HaBakedFile HA_BAKED_FILES[] = {
${files.map((f) => `    {"${f.path}", "${f.mime}", ${f.gzip ? 'true' : 'false'}, ${f.sym}, sizeof(${f.sym})},`).join('\n')}
};
static const size_t HA_BAKED_FILE_COUNT = sizeof(HA_BAKED_FILES) / sizeof(HA_BAKED_FILES[0]);

struct HaBakedPack {
    uint8_t game;
    const char* lang;     // "en", "de", ... -- the Settings language selects which stream
    const char* fallback; // pack name if the file has no "Pack:" line
    const char* text;
};

static const HaBakedPack HA_BAKED_PACKS[] = {
${packs.map((p) => `    {${p.gameConst}, "${p.lang}", "${p.fallback}", R"${DELIM}(${p.text})${DELIM}"},`).join('\n')}
};
static const size_t HA_BAKED_PACK_COUNT = sizeof(HA_BAKED_PACKS) / sizeof(HA_BAKED_PACKS[0]);
`;

writeFileSync(out, h);

// ---- engine headers ---------------------------------------------------------
const ENGINE = ['ha_proto.h', 'ha_json.h', 'ha_games.h'];
for (const name of ENGINE) {
  const banner =
    `// GENERATED COPY of vendor/engine/${name} (upstream hotspot-arcade) -- do not edit here.\n` +
    `// Change it upstream, re-run tools/sync-upstream.mjs, then tools/gen-assets.mjs.\n`;
  writeFileSync(join(sketch, name), banner + readFileSync(join(vendor, 'engine', name), 'utf8'));
}

console.log(
  `wrote ${out}\n  ${files.length} web file(s), ${totalWeb} bytes\n` +
    `  ${packs.length} pack(s), ${totalPacks} bytes\n` +
    `  copied ${ENGINE.join(', ')} into the sketch`,
);
// Silence here would read as "everything is baked in"; it would not be.
if (dropped.length) console.log(`  NOT baked (over the ${MAX_PER_GAME}-per-game cap): ${dropped.join(', ')}`);
