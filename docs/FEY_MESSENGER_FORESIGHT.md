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

Live assignment verified on 2026-11-27 after Joseph explicitly approved it.
The existing single-card schema-14 editor request added `foresight` to Fey
Messenger, matching Mosswake's legacy keyword representation. No binary
rollout, restart, client publication or other live card assignment was needed.

Independent CA/hostname-validated public TLS readback from
`game.gloomthorn.com:55004` compared all 97 cards: only Messenger's Foresight
assignment changed. Courier Flight, Star Dart, cost 25, health 1, Fey trait,
starter metadata, and all other fields/cards are unchanged. The readback also
passed the application's compiled catalog parser/conversion checks: 538 checks,
zero failures. No live match was played.

Before the editor save, a consistent protected rollback backup was verified at
`/var/lib/bayou/shared/cards-pre-fey-foresight-20261127T182747Z.db`.
A read-only comparison of all 12 authoritative database tables after the save
found exactly one added row: `card_keywords('Fey Messenger','foresight')`.
Database integrity and foreign-key checks passed. The normal server installer
still does not copy catalog databases; no rehearsal database was installed.
Detailed public feed and DB verification artifacts are retained locally under
`output/fey-messenger-live/` and are not committed.

Card Progress advances to Starbloom Knight, the next card in the local ability
suggestions document. Its listed suggestions remain undecided.
