# Board Markers and Traces

Traces is a reusable passive, not an activated ability. It uses the same
trigger as Trail: after a piece survives a regular action that actually changed
its position, it leaves its selected Board Marker at the action's origin.
Commanded moves and moving attacks can qualify; stationary attacks and forced
relocation by another piece do not place a marker.

In Card Editor, open Board Markers to create a named definition. Choose
Friendly, Enemy, or Any eligibility, Heal or Damage, and a positive strength.
The optional Marker Art field accepts an asset path relative to `assets/`.
For Bloom, set `characters/bloomMarker.png` on the **Bloom Board Marker**;
Traces continues selecting the saved marker and has no art assignment of its own.
Saving or clearing this field affects the reusable definition.
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
- The game renderer draws the image stored in the marker definition. Empty or
  unavailable art uses the existing placeholder. The owner outline stays visible,
  and its offset does not depend on hidden Blight, including on shared squares.

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

Schema 17 adds optional object art while schema 16 keeps its exact layout.
BMA1 is advertised after BMR1 and is sent only after explicit client opt-in.
New readers accept the older marker records and saved pieces. Existing catalogs
can be read without the new column; initialization adds a blank `image_path`
column without assigning any art. Legacy marker edits preserve assigned art.

## Local verification

Build the Windows client and services with CMake. Run abilityobjecttest,
gametest --movement-only, conquesttest, and offline storymodetest. Run the
isolated loopback card-service test with tools/test_activated_ability_service.py
--server build/Release/cardserver.exe. Invoke the Release client from the repo
root with --ui-capture=output/dewbell-markers/ui and
--ui-capture-screens=game-bloom,card-editor-board-markers,card-editor-traces,card-editor-interactions.
Check the capture manifest and screenshots. For a catalog-to-render check, pass
`--ui-capture-marker-definition=<isolatedDirectory>/marker-art.fixture` from the
loopback service test to `game-bloom`. That fixture contains the object read back
from the isolated catalog under a different name, exercising the regular renderer
without relying on a Bloom or card-name art lookup.
