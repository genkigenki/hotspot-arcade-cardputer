# Upstream

Everything under `vendor/` is copied verbatim from
[tarikbc/hotspot-arcade](https://github.com/tarikbc/hotspot-arcade) (MIT,
Tarik Caramanico). Nothing in `vendor/` is edited here -- see README.

| | |
| --- | --- |
| commit | `9e139c9845b6539cb48abf9c7af2f3a41020f4af` |
| describe | `v1.8.0-4-g9e139c9` |
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
