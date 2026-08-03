# Upstream

Everything under `vendor/` is copied verbatim from
[tarikbc/hotspot-arcade](https://github.com/tarikbc/hotspot-arcade) (MIT,
Tarik Caramanico). Nothing in `vendor/` is edited here -- see README.

| | |
| --- | --- |
| commit | `25ad21523c9a66eb712911545792ee8ebf6281ad` |
| describe | `v1.6.0-3-g25ad215` |
| engine | `vendor/engine/` -- ha_proto.h, ha_json.h, ha_games.h |
| web bundle | `vendor/web/` -- 1 file(s) |
| content packs | `vendor/packs/` -- 32 pack(s) |
| async libs | `vendor/libs/` -- AsyncTCP, ESPAsyncWebServer (third-party, own LICENSE files) |

Refresh with:

```sh
node tools/sync-upstream.mjs [path-to-upstream-clone]
node tools/gen-assets.mjs
```

`git diff vendor/` after a sync is exactly the upstream change.
