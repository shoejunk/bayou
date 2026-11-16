# Silence negative status

Set a card's `ability` String Field to `silence`. Optional `silenceTurns` Integer Field defaults to 1; nonpositive values normalize to 1. The Choir uses 4 and the button label Dissonant Chorus.

Activation consumes the ordinary ability action and attaches a negative Silence status to every living enemy currently on the board, at any distance. Allies and later entrants are unaffected. The status prevents activated ability and Sacrifice buttons only: normal movement, drag attacks, passive keywords (including Chrysalis), and other passive effects remain intact.

The status records remaining turns and the affected enemy player at application. It decrements at that player's turn end, not per action and not at turn start. Four means four complete enemy turns. Caster removal and later control changes do not cancel or advance the clock. Reapplication uses the maximum remaining/configured duration, not addition. `clearSilence` removes this negative modifier for future cleansing mechanics. Transformation preserves the modifier; a newly spawned replacement does not inherit it.

The obsolete source-bound Silence Field ability has been removed. Its former wire slots remain inert solely to preserve packet compatibility. Silence never suppresses passive Chrysalis.

Client/server capability SIL1 adds optional status metadata after HEA1. Legacy peers omit this extension; server ability gating remains authoritative. Updated clients show remaining enemy turns as a negative status.
