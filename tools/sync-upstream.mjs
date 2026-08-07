#!/usr/bin/env node
// Refresh vendor/ from an upstream hotspot-arcade checkout and record the exact
// commit it came from.
//
//   node tools/sync-upstream.mjs [path-to-upstream-clone]   (default: ../hotspot-arcade)
//
// This repo does not fork upstream; it vendors four things and pins the commit:
//   vendor/engine/  the game engine + protocol + json helpers  (never edited here)
//   vendor/web/     the built phone client
//   vendor/packs/   the content packs
//   vendor/libs/    upstream's vendored AsyncTCP + ESPAsyncWebServer (own licenses)
//
// After syncing, run `node tools/gen-assets.mjs` and check `git diff vendor/` --
// that diff IS the upstream change, and it is the only place upstream and this
// repo can drift.

import { readFileSync, writeFileSync, readdirSync, mkdirSync, copyFileSync, cpSync, existsSync, rmSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const vendor = join(root, 'vendor');
const up = process.argv[2] ? process.argv[2] : join(root, '..', 'hotspot-arcade');

if (!existsSync(join(up, 'esp32', 'hotspot-arcade-fw', 'ha_games.h'))) {
  console.error(`not an upstream hotspot-arcade checkout: ${up}`);
  console.error('usage: node tools/sync-upstream.mjs [path-to-upstream-clone]');
  process.exit(1);
}

const git = (...args) => execFileSync('git', ['-C', up, ...args], { encoding: 'utf8' }).trim();
const commit = git('rev-parse', 'HEAD');
const describe = (() => {
  try {
    return git('describe', '--tags', '--always');
  } catch {
    return commit.slice(0, 7);
  }
})();
const dirty = git('status', '--porcelain').length > 0;

const copy = (from, to) => {
  mkdirSync(dirname(to), { recursive: true });
  copyFileSync(from, to);
};

// engine
for (const n of ['ha_proto.h', 'ha_json.h', 'ha_games.h']) {
  copy(join(up, 'esp32', 'hotspot-arcade-fw', n), join(vendor, 'engine', n));
}
// web bundle (whatever the manifest names, so a multi-file bundle still works)
const manifest = JSON.parse(readFileSync(join(up, 'web', 'dist', 'manifest.json'), 'utf8'));
copy(join(up, 'web', 'dist', 'manifest.json'), join(vendor, 'web', 'manifest.json'));
for (const m of manifest) copy(join(up, 'web', 'dist', m.file), join(vendor, 'web', m.file));
// packs
rmSync(join(vendor, 'packs'), { recursive: true, force: true });
let packCount = 0;
// Two levels deep, because upstream keeps translations in packs/<game>/<lang>/ (de/,
// pt-br/, ...) with English flat at the game root. Copying only the top level silently
// dropped every translation -- German Secrets exists upstream and simply never arrived
// here, which is exactly the kind of gap a "verbatim copy" is supposed to make impossible.
const langsSeen = new Set();
for (const sub of readdirSync(join(up, 'packs'), { withFileTypes: true })) {
  if (!sub.isDirectory()) continue;
  for (const e of readdirSync(join(up, 'packs', sub.name), { withFileTypes: true })) {
    if (e.isDirectory()) {
      for (const f of readdirSync(join(up, 'packs', sub.name, e.name))) {
        if (!f.toLowerCase().endsWith('.txt')) continue;
        copy(
          join(up, 'packs', sub.name, e.name, f),
          join(vendor, 'packs', sub.name, e.name, f),
        );
        langsSeen.add(e.name.toLowerCase());
        packCount++;
      }
      continue;
    }
    if (!e.name.toLowerCase().endsWith('.txt')) continue;
    copy(join(up, 'packs', sub.name, e.name), join(vendor, 'packs', sub.name, e.name));
    langsSeen.add('en');
    packCount++;
  }
}

// vendored async libs: pinned copies rather than library-manager versions, so a
// build here compiles against exactly what upstream ships and tests.
rmSync(join(vendor, 'libs'), { recursive: true, force: true });
cpSync(join(up, 'esp32', 'libs'), join(vendor, 'libs'), { recursive: true });

writeFileSync(
  join(root, 'UPSTREAM.md'),
  `# Upstream

Everything under \`vendor/\` is copied verbatim from
[tarikbc/hotspot-arcade](https://github.com/tarikbc/hotspot-arcade) (MIT,
Tarik Caramanico). Nothing in \`vendor/\` is edited here -- see README.

| | |
| --- | --- |
| commit | \`${commit}\` |
| describe | \`${describe}\`${dirty ? ' (working tree was dirty at sync time)' : ''} |
| engine | \`vendor/engine/\` -- ha_proto.h, ha_json.h, ha_games.h |
| web bundle | \`vendor/web/\` -- ${manifest.length} file(s) |
| content packs | \`vendor/packs/\` -- ${packCount} pack(s) |
| async libs | \`vendor/libs/\` -- AsyncTCP, ESPAsyncWebServer (third-party, own LICENSE files) |

Refresh with:

\`\`\`sh
node tools/sync-upstream.mjs [path-to-upstream-clone]
node tools/gen-assets.mjs
\`\`\`

\`git diff vendor/\` after a sync is exactly the upstream change.
`,
);

console.log(`synced vendor/ from ${up}`);
console.log(`  commit ${describe} (${commit})${dirty ? '  WARNING: upstream working tree is dirty' : ''}`);
console.log(`  ${manifest.length} web file(s), ${packCount} pack(s)`);
console.log('next: node tools/gen-assets.mjs');
