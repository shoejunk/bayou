# Shared activated ability parameters

Canonical optional integer fields: `ability range`, `ability strength`, `ability duration`.
Magnitude is ability-specific. Range uses nearest-footprint Chebyshev distance, including diagonals. Zero range/strength is explicit; negative values normalize to zero. Duration is at least one.

Defaults are range 1, strength 1, duration 1 except compatibility fallbacks: Area Heal honors legacy `heal range`; Shriek honors `shriek range`/`shriekRange` and `shriek damage`/`shriekDamage` (historically zero damage); Silence honors `silenceTurns` and remains global when no explicit canonical range is supplied. Do not narrow existing Silence cards implicitly. Canonical fields override legacy fields.

Range consumers: Area Heal, Shriek, Power Boost, Command, Silence, Weaken. Strength consumers: Area Heal, Shriek damage, Power Boost, Weaken. Duration consumer: Silence. Passive aura ranges, action attack/movement ranges, activation-use counts, and growth/transformation timers are not these fields.

## Weaken
`ability=weaken`; Lord Pallid's card-specific `abilityLabels` should contain `Pallid Curse`.
A normal activated button adds a permanent stacking negative Power status to each living enemy currently in range. Default magnitude is one. Allies/self are not affected. It remains after the caster dies or the target leaves range; it does not tick down. Positive and negative Power are summed before final damage is floored at zero. Non-damaging actions remain non-damaging. A dedicated clear helper removes only this negative status, preserving positive Power. The status is separate from the underlying card's base Power.

## Compatibility and rollout
ABP1 is explicitly negotiated for the shared runtime parameters and negative status. Older packet layouts remain readable, and canonical values mirror into legacy range/damage/duration slots where those exist. Snapshot Power is already effective; a marker prevents applying modifiers twice on a new client.

Packet compatibility is not full gameplay compatibility. An older client cannot activate Weaken, displays negative Power using its old clamp, and restricts Command selection to adjacency. Deploy a matching updated client with the updated server before relying on these features. No claim of played-through live validation is made.

Live catalog writes are separate from this source change. Preserve existing values/labels/actions, inspect actual authoritative definitions, update only intended fields, and verify readback. Global Silence range migration remains a separate owner decision. Legacy aliases remain supported so deployment order does not silently change old cards.

Verification: Debug client, gameserver, gametest, and audiotest builds; 1088 standalone gameplay checks and 1472 audio checks passed. Live gameplay testing waived by Joe. Authenticated network end-to-end/live matches were not run.
