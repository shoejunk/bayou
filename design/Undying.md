# Undying

Joe approved **Undying** for Sir Chitin the Undying (authoritative catalog title: **Sir Chitinous Vale**). It is a passive keyword with explanatory card text, not an activated ability or a separate ability label.

The first time this Unit would be defeated, it stays on the board with 1 Health. Its piece ID, square, controller, original owner, attachments, conditions, readiness, action state and printed maximum Health remain intact. The spent flag belongs only to that board piece. Passing turns, Control and an in-place timed transformation do not refresh it. A genuinely new piece starts unused.

Undying prevents that defeat before Infest, named-card Rebirth, raised-card exile, Evacuate or Chrysalis can resolve. It creates no graveyard card or Husk, does not grant a kill or Relentless, and its occupied footprint blocks attack landing normally. Later defeat uses the existing rules unchanged. A separate lethal event, including collision damage following the initial hit in the same action, can defeat the spent unit immediately. Sacrifice is a defeat path: the first sacrifice consumes Undying and clears the pending sacrifice, while retaining the ability's normal action cost.

The server and sandbox call the same revival rule from their central defeat handlers. Those handlers cover attack and Bodyguard damage, push collisions, Counterstrike/Bristle retaliation, damage spells, Shriek and sacrifice. No Poison damage mechanic exists in the current implementation. Heroes do not receive this Unit passive.

`UND1` negotiates the spent flag in snapshots after `ABP1`, for both ordinary matches and Conquest battles. New readers accept snapshots without this extension and default to unused. Servers omit it for clients that do not opt in. Use the updated client for the passive explanation and spent-state display; legacy wire parsing alone does not establish full UI compatibility.

Verification is automated and offline. No live match is required or performed. Catalog activation must preserve Sir Chitinous Vale's existing Bodyguard keyword, printed actions, stats, traits and assets, adding only Undying after the runtime is deployed and verified.

Echo Lantern remains in progress and on hold pending Joe's activated-ability architecture decision. This change does not implement Echo.
