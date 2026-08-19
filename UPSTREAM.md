# Upstream

Everything under `vendor/` is copied verbatim from
[tarikbc/hotspot-arcade](https://github.com/tarikbc/hotspot-arcade) (MIT,
Tarik Caramanico). Nothing in `vendor/` is edited here -- see README.

| | |
| --- | --- |
| commit | `9d62fc7413dc2f39772438964490b0695b84c8b1` |
| describe | `v1.8.0-20-g9d62fc7` |
| engine | `vendor/engine/` -- ha_proto.h, ha_json.h, ha_games.h |
| web bundle | `vendor/web/` -- 1 file(s) |
| content packs | `vendor/packs/` -- 90 pack(s) |
| async libs | `vendor/libs/` -- AsyncTCP, ESPAsyncWebServer (third-party, own LICENSE files) |

Refresh with:

```sh
node tools/sync-upstream.mjs [path-to-upstream-clone]
node tools/gen-assets.mjs
```

`git diff vendor/` after a sync is exactly the upstream change.
