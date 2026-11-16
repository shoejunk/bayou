# Chrysalis

`Chrysalis` is a passive Unit keyword, separate from the existing immediate Rebirth mechanic. It adds no activated-ability button and does not change drag/action selection.

- Once per physical card copy per match, an eligible defeat leaves a nonblocking Husk on the original footprint. The use is spent immediately.
- Any living unit, friendly or enemy, occupying any part of that footprint crushes the Husk. This includes a killing attack landing, interception, and each forced-movement step. Moving away later does not restore it.
- At the original owner's next turn start, an intact Husk returns its Unit at 1 Health if the full footprint is empty. Occupation by either side destroys the Husk immediately; leaving later cannot restore the return attempt.
- The card is held only by the Husk while pending, preventing duplicate Raise Undead or hand copies. Failed/crushed Husks enter the graveyard (or exile when applicable). Spent state persists through card zones and transformations.
- Existing successful Infestation/immediate Rebirth, raised-Undead exile and successful Evacuate retain precedence. Heroes cannot start Chrysalis; Silence does not suppress this passive.
- Husks do not count as living replacements: defeat and kill-trigger accounting are unchanged.

The server and local sandbox share the lifecycle helpers. Public Husk markers and spent flags use the negotiated `CHR1` snapshot extension; older peers continue receiving their older payload format. Deploy matching client/server versions before expecting the new keyword to work in online play. Updating catalog data or pushing source alone does not deploy runtime binaries.

## Checks

Build `gametest`, `client`, and `gameserver`; run `gametest.exe --movement-only` (no account/network integration). Focused tests live in `src/gametest/chrysalis_tests.hpp`. Run `cppcheck.bat` for the project baseline. Offline UI fixture: `--ui-capture-screens=game-chrysalis,game-selected,game-action-choice`.
