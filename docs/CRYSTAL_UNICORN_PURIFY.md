# Crystal Unicorn: Purifying Horn

Joseph selected Purifying Horn as a second regular action. Prismatic Charge,
cost 50, Health 2, deck limit 2, starter rarity, Honorable/Wild traits and all
other card fields are preserved. This is not an activated or sacrifice ability.

The action uses the same `@purify` metadata and `applyPurify` status routine as
Caltheriel. It is ranged/omni, minimum 1 and maximum 2, following the existing
straight/diagonal friendly-piece targeting convention. It cannot target enemies,
empty squares or itself. Like Caltheriel's action it has no line-of-sight flag.
Its `can_attack` routing flag enables normal action targeting; damage, heal,
movement and status damage are all zero. The resolver and client description
now recognize the existing Purify flag independently of positive healing.

It removes only the established negative unit statuses: Disabled, Sleep,
Rooted, Slow, timed Silence and all Weaken. Health, position, positive Power,
transformations, spent readiness, ownership/control, infestation and existing
board markers stay intact. No Corrupt-marker removal rule was added.

## Local verification

Prepare a new isolated copy (never overwrite an existing catalog):

```
python tools/prepare_crystal_unicorn_purify.py --source <before.db> --output <new-isolated.db>
build/Debug/abilityobjecttest.exe --verify-unicorn-purify <before.db> <after.db> <fixture.bin>
build/Debug/SteamTactics.exe --ui-capture=<new-directory> --ui-capture-screens=game-unicorn-purify-actions,game-unicorn-purify-target,game-unicorn-purify-result --ui-capture-game-card=<fixture.bin>
```

The preparation compares every table, permitting only the new action, its
Purify flag and its second-action link. The application reader compares all
97 cards and runs the actual authoritative regular-action path using the
rehearsal card. Captures use that same catalog card through the normal client
renderer and sandbox action handler, checking cleanse, unchanged Health/Power,
spent readiness, position and existing Blight. Caltheriel and existing offline
game/Story regressions are also checked. The existing first Unicorn mastery
lesson now follows its real Bodyguard hit with Horn on the Knight. At that
turn boundary the one-turn Disable has already spent the Knight's readiness;
the lesson explicitly verifies Horn cannot restore that action or heal.
The Knight is placed within range, before the original Charge sequence. No generated database belongs in Git.

## Live status

No live assignment, deployment or publishing is performed here. Read-only
production inspection found game binary `/opt/bayou/releases/6d237ae-20261116170847/bin/gameserver`
(SHA-256 `bac0a7859a6bd1561a1de2c2df2597947c4a991ef83ee80962d9ec4c4860730a`)
and no `@purify` support string; that deployed source predates Purify. Thus an
editor assignment alone would not safely enable this action. A separately
approved compatible release and narrow live catalog assignment are needed.
The stale rehearsal copy must never replace the authoritative database.

Heartwood Sister follows Crystal Unicorn in the local suggestions document.
Live gameplay was not tested.
