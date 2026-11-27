# Fey Messenger: Foresight

Joseph selected the existing Foresight passive used by Archivist Mosswake.
Fey Messenger keeps Courier Flight and Star Dart, cost 25, health 1, Fey trait,
starter metadata, and all unrelated fields. This is a passive assignment;
there is no new activated or sacrifice ability and no new Foresight rule.

The ordinary Draw interaction costs 50 Resources. Each owned Foresight piece
adds one option to the normal one-card draw; with Messenger alone, choose one
of two cards. The unchosen card goes to the deck bottom. Options are private,
and resolving the choice does not consume the piece's regular action.

## Isolated catalog rehearsal

`tools/prepare_fey_messenger_foresight.py --source <existing-catalog.db> --output <new-isolated.db>`
opens the source read-only and refuses an existing output. It copies the exact
Foresight spelling/storage from Archivist Mosswake, using `card_passives` on
native catalogs or the legacy `card_keywords` representation. It verifies all
tables against the source: only the single Fey Messenger assignment may differ.
It also checks integrity and foreign keys. Rehearsing an already assigned
catalog adds zero rows. This tool never installs or replaces a live catalog.

`build/Debug/abilityobjecttest.exe --verify-fey-foresight <before.db> <after.db> <fixture.bin>`
loads both catalogs through the application database reader, compares every
card field/action, checks Messenger's paid draw/choice through GameEngine, and
exports that actual catalog card for the normal Windows client renderer:

```
build/Debug/SteamTactics.exe --ui-capture=<new-output-directory> --ui-capture-screens=game-fey-messenger-foresight,game-fey-messenger-foresight-chosen --ui-capture-game-card=<fixture.bin>
```

Also run `gametest --movement-only` and `storymodetest` for the existing
Foresight and packaged Story regressions. Inspect the screenshots and capture
manifest. The rehearsal source may contain other unpublished work; never
replace production with this copy.

## Live status

Locally verified; no production mutation, deployment or publishing performed.
The supplied live baseline is schema 14 with no Messenger passive. The normal
server installer deliberately does not copy card databases, so deploying code
alone will not assign this passive. A separately authorized catalog rollout
must add only Foresight to Fey Messenger using the current authoritative Card
Editor/catalog path, retain Courier Flight and Star Dart, and read back the
result. Native taxonomy uses Passives; the current schema-14 server represents
implemented passives in its legacy keyword channel, as it does for Mosswake.
Live gameplay verification was waived, not performed.

Card Progress advances to Starbloom Knight, the next card in the local ability
suggestions document. Its listed suggestions remain undecided.
