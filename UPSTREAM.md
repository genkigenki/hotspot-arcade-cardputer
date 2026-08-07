# Upstream

Everything under `vendor/` is copied verbatim from
[tarikbc/hotspot-arcade](https://github.com/tarikbc/hotspot-arcade) (MIT,
Tarik Caramanico). Nothing in `vendor/` is edited here -- see README.

| | |
| --- | --- |
| commit | `77fbc82393d79e72592bd01accebd9b8623d2d84` |
| describe | `v1.6.0-25-g77fbc82` |
| engine | `vendor/engine/` -- ha_proto.h, ha_json.h, ha_games.h |
| web bundle | `vendor/web/` -- 1 file(s) |
| content packs | `vendor/packs/` -- 76 pack(s) |
| async libs | `vendor/libs/` -- AsyncTCP, ESPAsyncWebServer (third-party, own LICENSE files) |

**This pin is two commits ahead of tarikbc/master**, both of them fixes waiting in a
pull request there:

- the phone client no longer pings every 2s and no longer closes a working socket after
  15s of quiet -- that self-diagnosis dismissed the game-change vote for anyone whose
  link went briefly silent
- audio unlocks on the first gesture of any kind, not only on Play, so a returning
  player (auto-rejoined from a saved nickname) is not silent for the whole session

Everything else under `vendor/` is upstream master verbatim. When the PR lands, re-sync
and this note goes away.

Refresh with:

```sh
node tools/sync-upstream.mjs [path-to-upstream-clone]
node tools/gen-assets.mjs
```

`git diff vendor/` after a sync is exactly the upstream change.
