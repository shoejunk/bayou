# Tutorial clarity loop

Rebuilt the tutorial around short rescue stories, personal choices, and visible consequences. The three routes now contain **67 entries through their endings**, followed by **13 optional practices**. The previous 114-entry version contained long story detours and drills before its endings.

Three independent builders completed two substantial revision rounds. Nine blind campaign reviews across three rounds read only the active player-facing text, without the builders' reasoning or the source novels. All three final readers could retell their route coherently and reported **no material comprehension blockers** for the sixth-grade, adult beginner, and international plain-English lenses. Their remaining small wording fixes were applied. A separate final read across all three routes caught two shared-scene inconsistencies involving Scooter and Gearjaw; the reader accepted the repairs, and the compiled export was checked against those accepted strings.

These are editorial model reviews, not human playtests or a guarantee of enjoyment for every audience.

## What changed

- Cut 38 story-only detours and merged overlapping helper roles. Removed bookkeeping, court procedure, research, spare magic devices, and repeated explanations that did not earn their place.
- Mirewatch now centers Reed's family betrayal, a single prison rescue, and his choice to save families before chasing Victor. Juniper survives injured; the child she saved returns with soup. Mog carries the prison-helper role. Gearjaw and Scooter's two taps develop their friendship and its cost.
- Blackthorn follows Mog's small kindnesses through Braun's refusal, Mog's public break with Victor, and the work of helping those he hurt. Skipping every practice still tells a complete story. Skipping no longer creates a mandatory 26-action review later.
- Seelie follows Caltheriel's rescue, the attack on the city, and one continuous theater evacuation. The honey bargain, shared light, tested harness, Liora's mistake, Emperor's refusal, and final song carry the emotional story. Fewer names and devices compete with those moments.
- Shortened two required Mirewatch lessons and Nettle's opening flight lesson. Their full demonstrations remain available after the endings. Mirewatch's first independent battle and finale now use familiar existing cards.
- Moved detailed optional coaching after Start Practice, made retry prompts name the required move, separated story completion from extra practice, and preserved saved completion evidence when stable mission IDs move.
- Replaced four mismatched planning-table illustrations with existing character and climax art. Printed card/action names still match the real game, including its current Healing Elixer spelling.

## Size of the main route

| Route | Entries through ending | Extra practices | Main-route dialogue/coaching words | Required guided inputs |
|---|---:|---:|---:|---:|
| Mirewatch | 30 → 22 | 4 | 4,680 → 2,406 | 73 → 48 |
| Blackthorn | 27 → 18 | 3 | 4,043 → 2,524 | 26 → 0 |
| Seelie | 57 → 27 | 6 | 5,965 → 2,559 | 28 → 22 |

Main-route dialogue and coaching fell from **14,688 to 7,489 words**, about **49%**. These counts include optional coaching that occurs before the ending and exclude headings, action instructions, and retry prompts. Required guided inputs exclude autonomous free-play actions. These are measures of density, not certified reading levels.

## Verification

The Windows Debug client and story tests build. The Story Mode suite reports **0 failures**, including **172 before/after outcome checks covering 156 authored mastery claims**. Original full lesson proofs remain in the optional workshops, with additional checks for the retained required moves. Card rules, action profiles, damage, timing, and commands remain authoritative. The structural audit accepts only the documented scene cuts, practice moves/splits, and familiar-team substitutions.

Saved-progress tests cover reordered and removed IDs, retained completed/skipped evidence beyond a new gap, and prevention of unearned mastery. The updated capture registry is generated from the current routes and checks optional-review behavior.

The final **669-screen pass at 800×600** passed with no validation failures, covering **375 active action states and all 40 tactical clean boards**. A further **34-screen pass at 1920×1080** also passed. Selected dialogue, mouse/keyboard instructions, practice choices, injury/recovery scenes, and final encounters were inspected visually. A **16-screen final pass at 800×600** rechecked every final continuity/goal-text change and the affected survivor displays. The first two broad batches preceded only that localized polish; the remaining broad batches and focused checks used the final dialogue build. The last artwork-only correction in the Emperor and engine scenes passed an additional eleven-page check at each resolution. Every successful capture manifest confirms that its source and binary matched during that run.

## Continuing the work

Use [Writing Story Mode for new players](GLOOMTHORN_STORY_MODE_WRITING_GUIDE.md). The earlier trilogy documents are historical; the active catalog and this record supersede their scene order and required-review assumptions. The complete source patch is based on `3c09d31c12d49bb1d36509f9a5d8d5495d53529b`.

Build the Windows checkout with `cmake --build build --config Debug --target client storymodetest --parallel 4`, then run `build/Debug/storymodetest.exe` from the repository root. UI captures use `build/Debug/SteamTactics.exe --ui-capture=<empty-output-directory> --ui-capture-size=800x600`; retain the default warmup. The review bundle includes the compiled-text exporter, exact snapshots, critics, builders, audit, tests, and successful manifests.
