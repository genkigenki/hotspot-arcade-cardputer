# Upstream

Everything under `vendor/` is copied verbatim from
[tarikbc/hotspot-arcade](https://github.com/tarikbc/hotspot-arcade) (MIT,
Tarik Caramanico). Nothing in `vendor/` is edited here -- see README.

> **Ahead of upstream (temporary).** The pinned commit carries a small UTF-8 fix so
> non-English content works: Word Scramble shuffles whole characters (not bytes),
> Draw counts letters, and the scramble tiles upper-case ASCII only (so a German "ß"
> stays one letter instead of becoming "SS"). It's on the `utf8-content` branch of the
> [genkigenki/hotspot-arcade](https://github.com/genkigenki/hotspot-arcade) fork,
> offered upstream; once merged this re-pins to Tarik's official commit.

| | |
| --- | --- |
| commit | `66b64133421a89642618634283f2d1f6bc6dcf3e` |
| describe | `v1.4.0-1-g66b6413` |
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
