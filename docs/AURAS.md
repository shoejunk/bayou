# Rot Aura

Add `rot aura` to a Unit or Hero's Keywords (case insensitive).
Adjacent enemies cannot regain Health through actions, spells, healing auras or Lifedrain.
Diagonals and full footprints count. Allies and the bearer are exempt. The passive remains
active while its living bearer is hidden, disabled or Silenced; removal or moving away ends it.
Healing cannot resurrect defeated units.

The unused Silence Field active ability has been removed. Use `silence` for the separate
negative status documented in SILENCE.md. Legacy packet slots are emitted as inert constants
and consumed only for wire alignment; no field range, active duration or source state exists.
Rot Aura view facts and generic Silence duration still use the compatible optional trailer.
