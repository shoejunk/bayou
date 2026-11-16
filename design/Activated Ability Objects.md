# Activated ability objects

Joe approved this redesign on 2026-11-16. The Echo copying passive was subsequently canceled; Echo Lantern uses the existing Reveal passive. No Echo copying or Eyeblight trap implementation is included.

## Authoring and runtime

`ability_data::Definition` is a reusable named catalog object with its kind, optional range/strength/duration, optional Heal filter and Command restriction, Summon card, initial Dig uses, and state transitions/button labels. Cards reference separate normal and optional sacrifice definitions. A shared edit affects every assigned card; Copy creates an independent definition. Assigned definitions cannot be deleted until their references are removed. Renaming retargets references in the same transaction.

State rows contain the current state ID, next state ID, and button label. Transform and Dematerialize use these transitions and retain the unit's existing movement/attack action states. They do not copy or replace actions. Trail keeps its passive summon target on the unit; a normal or sacrifice Summon definition owns an independent target, and Story dependency loading includes both. The piece retains its current action state and its own remaining-use counters, including separate normal and sacrifice counts. Newly spawned pieces initialize their counters from the definitions. Existing matches retain the definitions already loaded into their pieces, as they do with printed actions.

The Abilities editor tab manages the reusable library. Cards have dedicated Ability reference and Sacrifice reference fields. Ability settings are absent from generic card fields in native catalog responses. Blank numeric values retain the established ability defaults. Heal filter and Command restriction distinguish unrestricted (`*`) from explicitly empty (matches nothing). The editor validates transitions, keeps both directions of a normal two-state Transform, and guards unsaved edits on navigation.

## Migration and compatibility

The card server creates `activated_abilities`, `card_activated_abilities`, and `card_ability_object_migrations` in a transaction. The definition blob has the versioned `ABD1` marker and bounded serialization. Existing cards receive definitions derived from their effective legacy settings, including existing default values, filters, labels, state cycles, restrictions and summon targets. Initial migration retains the original card fields for rollback. An explicit migrated-none marker prevents cleared assignments, including historical hard-coded abilities, from being recreated on restart.

Catalog schema 14 includes references and complete resolved definitions. Schema 12/13 clients receive legacy projections; their unchanged card metadata saves retain native assignments. Attempts to change ability settings through the old editor are rejected with a request to use the updated editor. Native saves resolve definitions from authoritative library references rather than accepting an embedded definition from the client. Deck submissions keep their established referenced-card format. Conquest event archives read older formats, retain the legacy model for legacy source cards, and write native objects for native cards (including explicit unassigned slots).

`ABO1` negotiates complete definitions and sacrifice use counters after `UND1` for ordinary and Conquest snapshots. Standalone cards/pieces and nested Chrysalis cards also preserve definitions. Older snapshot readers receive the existing layout when they do not opt in. Runtime compatibility fields remain as projections for legacy packets and authored fixtures; native definitions govern execution.

Back up the authoritative card database before deploying the migration. The initial conversion is transactional and leaves legacy fields intact. Later shared-definition edits refresh their legacy projections, but older formats cannot represent every independent sacrifice setting or custom transition. Rollback after such edits requires the paired database backup rather than assuming an old binary can reproduce every new configuration. Updated clients are required to author and inspect the complete native model.

## Automated verification

- `abilityobjecttest`: in-memory migration, idempotency, explicit assignment clearing, rename/delete restrictions, serialization bounds, optional filter distinctions, state transitions, runtime effects and independent use counters. An archived schema-13 packet can be supplied to compare the complete catalog's normalized gameplay before and after migration.
- `tools/test_activated_ability_service.py`: a temporary localhost card server with its own database and short-lived test PKI. It refuses an occupied port, validates the certificate chain and hostname, exercises native/legacy CRUD, and stops only its own process. It does not contact production.
- Existing offline gametest, Conquest and Story Mode suites cover legacy mechanics. Card-editor captures run native SFML interaction checks for tab navigation, definition edits, unsaved guards, filters, transitions and card references.

No live match is required or performed. Sir Chitin's Undying and Echo Lantern's Reveal were separately deployed and verified on runtime `227ace968c3b49f203c22ce4a5456202db3afc74`; this ability-object redesign is a separate rollout.

Verification on 2026-11-16: complete Debug and Release builds passed. Each configuration passed 1,121 offline game checks, 337 ability-object checks (including all 97 archived catalog cards), Conquest and Story Mode. The isolated TLS service passed 10 CRUD cases with database integrity `ok`. Offline editor interaction checks passed and the Abilities/card-reference captures were visually reviewed. Local reports are under `output/ability-objects-verification/{Debug,Release}` and captures under `output/ability-editor-tested`.

This redesign has not been pushed or deployed. Rollout still needs a source commit on `origin/main`, a verified fresh authoritative database backup, cached deployment of that exact commit, runtime/TLS checks, and a complete native/legacy catalog comparison after the transactional migration. Include the updated client when distributing support for native authoring.
