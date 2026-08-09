# Upstream

Everything under `vendor/` is copied verbatim from
[tarikbc/hotspot-arcade](https://github.com/tarikbc/hotspot-arcade) (MIT,
Tarik Caramanico). Nothing in `vendor/` is edited here -- see README.

| | |
| --- | --- |
| commit | `059232cfdf8e8bd88f2335aa09d67b47a31e2915` |
| describe | `v1.7.0-16-g059232c` |
| engine | `vendor/engine/` -- ha_proto.h, ha_json.h, ha_games.h |
| web bundle | `vendor/web/` -- 1 file(s) |
| content packs | `vendor/packs/` -- 90 pack(s) |
| async libs | `vendor/libs/` -- AsyncTCP, ESPAsyncWebServer (third-party, own LICENSE files) |

**This pin is upstream master (v1.7.0 + 3) plus our two open PRs and two additions**,
all together on the branch
[`integration-cardputer-07`](https://github.com/genkigenki/hotspot-arcade/tree/integration-cardputer-07)
of our upstream clone:

- PR #24 (play-test fixes: no client self-policing, audio unlock on any gesture,
  language kept across reload)
- PR #25 (four party games: Fill the Blank, Werewolf, Spyfall, Draw a Monster,
  ids 17-20)
- the expanded content packs (every pack filled to its cap, WYR 12->24, two
  kid-friendly Kiss Marry Kill packs -- Tierreich and Fabelwesen)
- the bot fill players: the lobby's testing switch now seats engine-run bots up
  to the active game's minimum instead of bypassing the check, and the
  game-change vote counts humans only

When PRs #24/#25 land upstream, re-sync from master and this note shrinks to the
leftovers.

Refresh with:

```sh
node tools/sync-upstream.mjs [path-to-upstream-clone]
node tools/gen-assets.mjs
```

`git diff vendor/` after a sync is exactly the upstream change.
