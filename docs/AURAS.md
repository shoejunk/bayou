# Rot Aura and Silence Field

These mechanics are reusable card data. No existing catalog card, starter deck,
or packaged story card is assigned either effect by this change.

## Card data

- Add `rot aura` to a Unit or Hero's Keywords. Matching is case insensitive.
- Set its `ability` String Field to `silence field` for the existing active-effect
  button. No additional target, direction, choice or input mode is used.
- `silenceRange` is an optional Integer Field, default **1**. It is clamped to
  **1–7**, the useful radii of an 8×8 board.
- `silenceTurns` is an optional Integer Field, default **1**. Values below 1
  become 1. All positive signed 32-bit values are supported; activation copies
  the value without adding to it, and timing only decrements it.

Configuration is normalized when converting catalog data, spawning a piece,
serializing, and reading runtime metadata. Existing generic integer/string/
keyword tables carry these definitions; no card database migration or schema
version change is required.

## Rules and timing

Rot Aura prevents adjacent enemies from regaining Health through healing
actions, spells, end-turn healing auras, or Lifedrain (including push-collision
damage). Diagonals and the full footprints of large units count. It excludes
the bearer and allies. Disabled, sleeping, growing, hidden or silenced bearers
still supply this passive while alive. Moving away or defeating/removing the
bearer ends the restriction immediately. Healing cannot resurrect dead pieces;
Rebirth, infestation and newly deployed units are new pieces, not healing.

Pressing Silence Field starts or refreshes the caster's single field. Every
OTHER unit within range, including allies and diagonals, loses the use of its
active ability and Sacrifice buttons. The caster is immune to its own field,
but another caster's field can silence it. Movement, drag attacks (including
named ranged/healing actions), card plays and passive effects are unaffected.
The radius is the Chebyshev distance between the nearest occupied squares of
both full footprints. The field moves with the living caster, updates as units
enter or leave, and disappears on removal; replacement pieces do not inherit it.

Duration counts the activating player's subsequent turn starts, rather than
actions, cards played, or both players' turns. Duration **1** expires at the
start of that player's next turn; **2** remains through two opposing turns and
expires at the activating player's second subsequent turn start. Expiry occurs
before control restoration and piece refresh. A later ownership change does
not change the activating player's clock. Reactivation refreshes to the
configured duration; it does not add durations together. A Sacrifice field
ends when its caster is sacrificed, consistent with all source-bound effects.

Authoritative snapshots carry whether each visible piece is silenced or has
healing blocked, including effects from concealed sources, without revealing
the concealed unit's identity or position. These are view facts; authoritative
rules and sandbox healing always recompute from the current complete board.

## Compatibility and verification

The existing snapshot core, message types, action requests and card-list schema
remain byte compatible. Optional `AUR1` metadata follows the existing final
snapshot status string and records field configuration/state and the two view
facts. Existing clients ignore this trailing data and can continue playing
existing cards. Updated clients also accept legacy snapshots with default
parameters and inactive fields. Standalone GameCard/Piece serializers have
optional trailing metadata; nested snapshot records explicitly use the legacy
core, and the snapshot trailer supplies their metadata in order.

The updated native client is required to author/display these parameters and
accurately disable buttons under new fields. The server remains authoritative
if an older client attempts a silenced ability. No live card is changed or new
card activated by deployment. Conquest stores the existing generic card data
and action requests; existing saved catalogs and actions retain their format.

`gametest --movement-only` runs the new deterministic aura regressions with the
existing offline rule, AI and sandbox suite. They cover defaults, validation,
custom radius/duration, diagonal/large footprints, ownership, entry/exit,
caster movement/death, active/Sacrifice buttons, normal attacks/movement,
passives, spell/action/aura/Lifedrain healing, turn expiry, snapshot metadata,
legacy compatibility and malformed metadata. No test changes the live catalog.
