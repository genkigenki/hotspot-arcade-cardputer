# Upstream

Everything under `vendor/` is copied verbatim from
[tarikbc/hotspot-arcade](https://github.com/tarikbc/hotspot-arcade) (MIT,
Tarik Caramanico). Nothing in `vendor/` is edited here -- see README.

| | |
| --- | --- |
| commit | `9a31a254cc38e40dd174dcd98840399d7279f30c` |
| describe | `v1.8.0-16-g9a31a25` |
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
