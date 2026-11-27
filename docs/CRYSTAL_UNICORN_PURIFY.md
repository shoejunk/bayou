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

Joseph explicitly approved the latest server build and the narrow Unicorn
assignment. On 2026-11-27, source `eb5955df03aadd0ca8b0ca2644ac357c8068b170`
was built with the managed cached Linux helper and activated at
`/opt/bayou/releases/eb5955d-20261127193441`. All four artifact, installed and
running-process binary hashes match. Accounts, cardserver, gameserver and
matchmaking are active/listening, with CA/hostname-validated public TLS on
55000/55001/55002/55004. Certificate expiry remains 2027-01-09.

The standard installer hit the documented accounts/cardserver readiness race
twice: accounts attempted its catalog fetch before cardserver was ready, and
the accounts-port TLS check failed. Each attempt restored the previous release,
protected catalog and TLS configuration. The helper-installed exact artifacts
then passed ordered activation: start cardserver, wait for a real schema-17
catalog response, then start accounts and the other services, checking each
TLS endpoint. No persistent service/configuration change was needed. All four
services have zero restarts and no journal errors since that ordered start.

The protected rollback directory is
`/var/lib/bayou/rollout-backups/eb5955d-20261127T190953Z`; it contains verified
pre-change cards/accounts/starter-deck databases and configuration, plus
consistent post-migration and post-Unicorn catalog snapshots. Original rows in
all 23 account tables and the starter-deck table are unchanged. Existing TLS,
game-server and certificate-renewal configuration hashes are unchanged.

Before assignment, all 97 public schema-14 and schema-17 cards retained the
same runtime fields/actions as the fresh production-copy rehearsal. Migration
moved exactly 23 existing behavior rows from keywords into `card_passives`,
including Messenger's already-approved Foresight. Every other original catalog
table was unchanged; new Board Marker tables remained empty. No rehearsal
database replaced production.

After compatibility/health verification, the normal single-action/single-card
editor requests created Purifying Horn and appended its reference only to
Crystal Unicorn. Independent public readbacks show Prismatic Charge followed
by Purifying Horn with `@purify`, friendly range 1-2, and zero healing, damage
or movement. All other fields and all 96 other cards match the pre-assignment
catalog, including Fey Messenger's Foresight. All 15 catalog tables exactly
match the fresh rehearsal: beyond the taxonomy migration, precisely one added
row each in `actions`, `action_target_filters` and `card_actions`. Integrity and
foreign-key checks pass. No other pending card, Dewbell, Bloom or marker art
was assigned.

The actual live catalog passed the compiled reader/conversion checks (538/0)
and the real regular-action resolver/preservation checks (350/0). Its Unicorn
game fixture is byte-identical to the one exercised through the normal Windows
Release renderer and sandbox action handler. Four Release action/target/result/
Story captures completed with exact source/assets provenance for `eb5955d`;
28 isolated TLS/editor compatibility checks passed. Evidence remains under
`output/unicorn-rollout/` and the local `unicorn-rollout-release-capture` folder.

The matching local Release client is `build/Release/SteamTactics.exe`
(SHA-256 `d487469f3dc475950784dac6901719bd89c9ae1bbc9ab5b5e3c5075d9c80f921`).
Use this build or newer for correct friendly Purify targeting/descriptions and
the updated passive/Conquest interfaces. Legacy catalog parsing alone does not
establish full gameplay/UI compatibility. No client package/storefront publish
or authenticated live match was performed. Other pending live card assignments
remain separate.

Heartwood Sister remains next in the local suggestions document.
