# Board Markers and Traces

Traces is a reusable passive, not an activated ability. It uses the same
trigger as Trail: after a piece survives a regular action that actually changed
its position, it leaves its selected Board Marker at the action's origin.
Commanded moves and moving attacks can qualify; stationary attacks and forced
relocation by another piece do not place a marker.

In Card Editor, open Board Markers to create a named definition. Choose
Friendly, Enemy, or Any eligibility, Heal or Damage, and a positive strength.
A Unit or Hero with Traces selects one existing definition on its card. Editing
a definition updates its users; renaming preserves links; an assigned
definition cannot be deleted. The catalog server resolves definitions by name.

## Contact rules

- Authored markers are visible to both players, do not occupy or block a square,
  and stay until an eligible entry. Different names can coexist; on the same
  square an existing case-insensitive name wins and is not overwritten.
- Entry means a newly occupied square of the piece's footprint on a completed
  landing, a forced relocation step, or a spawn. Merely crossing squares during
  voluntary movement does not trigger authored markers. Remaining on a square
  already occupied by the piece does not count as entry.
- Bloom targets the first friendly entrant, including the piece that placed
  it, heals 1, and is consumed even at full health or while Rot Aura blocks
  healing. Enemies do not spend it. Damage uses normal defeat rules, including
  Undying, without crediting it as an attack.
- Existing hidden Blight remains separate, with its existing per-owner
  placement and movement/contact rules. Its name is reserved. When both are
  applicable to an entry, Blight resolves first. Remaining applicable authored
  markers resolve in placement order while the entrant is alive.
- Bloom uses a distinct board-scale placeholder, including when it shares a
  square with Blight. Final art is not needed for the mechanics.

## Catalog and release

The local rehearsal assigns Traces -> Bloom to Duchess Dewbell only, leaving
her existing card fields and actions unchanged. No catalog database or
generated capture belongs in Git. The rehearsal database is not a production
replacement. For a separately approved rollout, coordinate the card,
accounts, and game services and updated client, back up current catalog data,
then create Bloom and assign it in the current authoritative editor. Verify
native readback and check unrelated cards.

Schema 16 carries typed marker links in the catalog and frozen Conquest data.
The older card lists omit Traces instead of sending unsupported names; a
legacy edit cannot remove an assigned marker. Match snapshots negotiate BMR1
after DRS1; older clients do not see authored markers, so update them together
for accurate play.

## Local verification

Build the Windows client and services with CMake. Run abilityobjecttest,
gametest --movement-only, conquesttest, and offline storymodetest. Run the
isolated loopback card-service test with tools/test_activated_ability_service.py
--server build/Release/cardserver.exe. Invoke the Release client from the repo
root with --ui-capture=output/dewbell-markers/ui and
--ui-capture-screens=game-bloom,card-editor-board-markers,card-editor-traces,card-editor-interactions.
Check the capture manifest and screenshots.
