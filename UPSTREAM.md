# Upstream

Everything under `vendor/` is copied verbatim from
[tarikbc/hotspot-arcade](https://github.com/tarikbc/hotspot-arcade) (MIT,
Tarik Caramanico). Nothing in `vendor/` is edited here -- see README.

| | |
| --- | --- |
| commit | `6d366675eaf0e1ac21b8ef277e8163bfd6b07aac` |
| describe | `v1.9.0-15-g6d36667` (working tree was dirty at sync time) |
| engine | `vendor/engine/` -- ha_proto.h, ha_json.h, ha_games.h |
| web bundle | `vendor/web/` -- 1 file(s) |
| content packs | `vendor/packs/` -- 90 pack(s) |
| async libs | `vendor/libs/` -- AsyncTCP, ESPAsyncWebServer (third-party, own LICENSE files) |

**This pin is upstream master (v1.9.0 + 2, PRs #26/#27/#28/#30/#31 merged) plus one
open PR**, on the branch
[`integration-cardputer-09`](https://github.com/genkigenki/hotspot-arcade/tree/integration-cardputer-09)
of our upstream clone: `pr/kmk-depth` (Kiss Marry Kill's own pack caps and 100-name
packs). When it lands, re-sync from master and this note goes away.

Refresh with:

```sh
node tools/sync-upstream.mjs [path-to-upstream-clone]
node tools/gen-assets.mjs
```

`git diff vendor/` after a sync is exactly the upstream change.
