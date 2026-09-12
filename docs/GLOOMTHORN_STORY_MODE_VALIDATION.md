# Gloomthorn Story Mode Validation Record

> Historical narrative record: this document describes the earlier source-led adaptation. The public-audience rewrite follows [Writing Story Mode for new players](GLOOMTHORN_STORY_MODE_WRITING_GUIDE.md). Dialogue, scene counts, route order, practice requirements, lore detail, and menu wording below are superseded where they differ from the active catalog. The September 11 clarity loop cuts story detours, moves extra drills after the endings, and uses familiar Mirewatch teams. Printed card rules and ordinary game commands still apply.

Date: 2026-09-10

## Non-negotiable gameplay boundary

Every required board input in Story Mode must be an ordinary game command executed by the ordinary engine from an authoritative card definition. A story panel may show dialogue, cargo, evidence, chains, votes, custody, or an autonomous character choice, but none of those events may become a substitute card, invisible target, special click, free damage source, or mastery rule.

## Delivered scope

- 114 chronological trilogy nodes: 30 Mirewatch, 27 Blackthorn, and 57 Seelie.
- 37 tactical missions: 12 Mirewatch, 10 Blackthorn, and 15 Seelie.
- 77 StoryOnly scenes for events that advance the novels but do not have an honest, transferable rules lesson. Seelie's 42 scenes use three to six panels so one scene can complete one setup-choice-consequence-reaction beat without a synopsis-forcing quota.
- Seven non-canon Blackthorn practice levels are recommended and skippable, and a skipped level awards no mastery. The earliest open 3v3 check appears immediately after `bt05_freight_office`; it has no prescribed move and no mastery award. If any of the seven entries lacks a completed-by-play stamp, a 74-step guided synthesis proves the exact 12-card starter roster before the required 4v4 depth-two bridge. If all seven stamps exist, the normal Continue action advances directly to that bridge and `Replay Synthesis` remains an explicit optional practice choice. A recommended depth-three ordinary-match exam then adds legal 20-card decks, hidden Hero placement, hands, economy, clocks, and victory by opposing-Hero elimination or opposing match-clock expiry. Its explicit pre-clock choice can continue the canonical route without mastery. All practice precedes the uninterrupted World Tree sequence: Birdie's clean shot, Victor's surrender/breach/death, and Thaeron's public deed.
- Revised Book One chronology is authoritative. The former standalone `mw12_road_reaches` node is retired because its surviving material moved into revised Chapter 14; both routes now include the revised Chapter 29 **The Deed in His Own Hand** coda.
- The Seelie route follows revised Books Two and Three from Vesper's Chapter 16 Mirror choice, through the Chapter 17 Cathedral battle, to the ordinary-work epilogue. Future Unseelie and mixed-court routes retain their own point-of-view gates; their revelations are not backfilled into Book One.
- Active legal use and mastery coverage for all 25 previously covered Mirewatch/Blackthorn starter titles and all 13 Seelie starter titles.
- Cross-faction and non-starter characters appear tactically only through real catalog cards.
- Connected Story Mode accepts only the complete authoritative schema-v11 card catalog. Before a mission starts, it validates the full dependency closure for board pieces, hands, draw piles, objectives, guided plays, mastery references, and every Summon, Rebirth, or Infest destination. Packaged definitions are available only through an explicit UI-capture/test fixture mode and can never silently rescue a connected session.
- Stable V2 progress remains ID-based after the prologue removal, and Seelie progress is stored independently under its own campaign suffix. Retired position-only saves restart a substantially different tutorial at Entry 1 rather than silently granting unrelated lessons.

The removed tutorial-only systems are:

- StoryActionKind::Interact
- bespoke Story interaction damage
- required clicks on rigging, cargo, chains, evidence, ledgers, gates, screens, witnesses, or other scenario props
- Story fixture cards and Rule Lab cards
- mastery credit for narrative concepts rather than game rules

Mechanics the live catalog cannot currently demonstrate remain StoryOnly and award no gameplay mastery. They can become tactical lessons only after an ordinary catalog card exposes them and a normal-engine replay verifies them.

## River Teeth opening

River Teeth is now the first campaign entry. The theatre/mentor prologue panels and all prologue-only capture routes have been removed from both playable paths. Its necessary information is deferred to later chapters where the key, Lash, and pearl become relevant.

The first Mirewatch battle has one causal line composed entirely of existing rules:

1. Reed, Donella, Erevan, and Telos begin surrounded by three Bull Gators.
2. Reed uses his printed one-square Step on each of two separate turns.
3. Donella uses Spark and Erevan uses Shadow Blade against real gators.
4. Telos uses his printed Travel from B4 to A5. B4 had blocked Reed's B6-to-B3 line; moving Telos therefore creates a real tactical benefit.
5. Reed uses his printed Bow through the newly clear column to defeat the final gator.

Cargo may remain visible in the story art, but no active panel, objective, or step asks the player to manipulate it. There is no rigging target. Board instructions name the real printed action rather than one input device; the first Seelie guided mission and persistent keyboard rail explicitly teach both mouse drag and keyboard focus/confirm controls.

The interface keeps three concepts distinct: `Guided Action` is campaign progress, `Move 1 of 2` is Reed's local objective, and `"Step"` is the printed name of Reed's card action. Briefings and multi-page in-mission story beats provide Previous navigation, and the opening no longer inserts a redundant popup before the first action.

## Automated verification

| Check | Result |
|---|---|
| Debug client build | PASS |
| storymodetest deterministic suite | PASS — 160 before/after mechanic checks, 146 authored mastery claims, 117 active art masters, 0 failures |
| Every active tactical golden replay | PASS |
| River Teeth blocked-line / Telos Travel / Reed Bow regression | PASS |
| Rejection of fabricated Story fixtures and non-game mastery | PASS |
| Campaign chronology, direct River Teeth entry, 114-node split, and all starter masteries | PASS |
| Blackthorn 27-entry order, seven optional practice levels including an early open 3v3 check, full-roster synthesis required only after a skipped drill, exact card/rule-equivalent earned bypass, required open 4v4 bridge, optional timed ordinary-match exam, split poisoned-road/factory chapters, post-training five-beat Victor reckoning, and depth-two-to-three escalation | PASS |
| Seelie 57-entry order, 15-tactical/42-StoryOnly split, focused 3-6-panel scenes, routed Court Behind the Curtain/city-at-night chapters, split Chapter 27/28 rescue and burning-root scenes, recommended drills, and role/objective resolution | PASS |
| Seelie open-board difficulty structure: 3v2 -> mixed 4v4 -> diverse 5v5 -> 4v4 withdrawal -> 8v8 Pump Four -> 4v4 gate route -> 6v7 witness control | PASS |
| Seelie opponent search-depth escalation: 1 -> 2 -> 3 -> 4 | PASS |
| Exact deployed schema-v11 traits, costs, Health, actions, passives, and status durations for all Seelie tutorial units and enemies | PASS |
| Live schema-11 gate: 97 cards, 55 unique tactical dependencies, 60/60 exact packaged/live fixtures, 24 exact-profile scripted golden paths, and 13 open setups/objectives with repeated configured-depth opponent turns | PASS — 0 failures, 0 unproven witnesses |
| Sylvara no-Summon boundary | PASS |
| Ashenfang identity boundary: one Ashenfang body, no separate Sylvara or unused spectators, and one neutral Veteran timing mark in the isolated rules demonstration | PASS |
| Mounted Nettle Rebirth identity handoff and unmounted Punch replay | PASS |
| Engine-owned scenario objectives and terminal results for defeat-all, defeat-role, reach-square, and control-square missions | PASS |
| `DefeatAllEnemies` counts authoritative original-owner identities, including reinforcements and Rebirth replacements, so temporary Control cannot erase a live enemy | PASS |
| `DefeatRole`, reach targets, and required survivors preserve identity through genuine Rebirth, while Infest remains a distinct replacement rule | PASS |
| Simultaneous survivor/force loss takes precedence over objective success, and invalid objective configurations fail closed | PASS |
| Connected card resolution rejects packaged fallback and recursively validates board, hand, pile, guided-play, Summon, Rebirth, and Infest dependencies | PASS |
| Story-wrong and engine-illegal guided inputs produce a visible correction plate | PASS |
| Every required survivor is named in both objective views, marked `PROTECT`, and named on failure; Pump Four requires Sylvara and the final witness objective requires Reed and Rowan | PASS |
| Open-objective setup, two-sided play state, normal End Turn flow, and no unverified mastery awards | PASS |
| Hollis debtor warning, Reed moonfruit bargain, Victor's five-scene motive, and the Seelie Baalzapub/Recall/concordance/ascent/withdrawal causal chain | PASS |
| Starter-deck database tests | PASS |
| Conquest regression groups | PASS |
| Ordinary engine standalone rules suite | PASS |
| Current 1920x1080 trilogy structure, briefing, art, and late-board captures | PASS |
| Current responsive 800x600 trilogy captures | PASS |
| Compact HUD leaves A8 and H8 selectable and visible; capture fixtures exercise both corner squares | PASS |
| UI-capture fixtures depict only legal live actions: Foreman Summon, End Turn, and Bristlejack attack redirected by Bodyguard | PASS |
| SE02 teaches the Bodyguard positive-damage/attached-Disable boundary in plain language without dumping later mechanic names; six authoritative live actions separately verify zero-damage Disable, Push, Pull, Infest, and Control targeting | PASS |
| Revised four-image River Teeth crop/provenance audit | PASS |
| git diff --check | PASS |

The service-backed tail of gametest still requires BAYOU_TEST_PASSWORD or BAYOU_SEED_PASSWORD. Its complete standalone rules portion passed; the missing credential is an environment prerequisite, not a Story Mode result.

## Visual evidence

The revised route structure is captured in:

- `output/story-mode-review/trilogy-structure-r1/` — both selectors and every tactical board at 1920x1080.
- `output/story-mode-review/trilogy-structure-r2/` — both final route pages and both Chapter 29 nodes at 1920x1080.
- `output/story-mode-review/trilogy-structure-r2-800x600/` — route, opening, capstone, and Chapter 29 compact-layout coverage.
- `output/story-mode-review/trilogy-structure-r3/` and `trilogy-structure-r3-800x600/` — corrected Story Intro metadata/header layout.
- `output/story-mode-review/river-teeth-art-r2/` and `river-teeth-art-r2-800x600/` — all four replacement illustrations and final Telos copy in briefing and in-mission panel crops.
- `output/story-mode-review/final-acceptance-r1/` and `final-acceptance-r1-800x600/` — current-build acceptance coverage after the opposition-dossier selector correction: both final route pages, both Chapter 29 nodes, and every River Teeth illustration at 1920x1080 and 800x600.
- `output/story-mode-review/tutorial-perfect-r18-1920/` and `tutorial-perfect-r18-800/` — matching 38-screen acceptance sets covering all three selectors, optional-drill choices, the rewritten Book One scenes, both Seelie Act VI definitions, the separated Silkmaw and Pallid chapters, visible Draw/Discard piles, Grask targeting, the new Blackthorn open exam, and both late Seelie objectives at desktop and minimum supported size.
- `output/story-mode-review/tutorial-perfect-r19-800/` — final compact-HUD spot checks with punctuated Resources/Control/Units labels and live `18 of 28` control progress in the mission plaque.
- `output/story-mode-review/tutorial-perfect-r21-800/` and `tutorial-perfect-r21-1920/` — historical focused acceptance from before the 8v8 board was replaced by the ordinary-match capstone; retained only as comparison evidence.
- `output/story-mode-review/tutorial-perfect-r21-full-800/` and `tutorial-perfect-r21-full-1920/` — complete matching 131-screen inventories after all route and HUD changes.
- `output/ui-review/tutorial-perfect-r54-full-800/` and `output/ui-review/tutorial-perfect-r54-full-1920/` — exhaustive pre-split 343-screen baseline retained for comparison. Each manifest completed with an exact registered set and all 686 PNGs decoded at their requested dimensions. The later final capture must additionally include the focused Poisoned Root Road, Lower Wells, First Prison, and Nima chapters.
- `output/ui-review/tutorial-perfect-r55-focused-800/` and `output/ui-review/tutorial-perfect-r55-focused-1920/` — 15-screen revision review covering the split Poisoned Root Road / False Factory Heaven sequence, the separated Cinderworks / Lower Wells / First Prison / Nima / receiver sequence, wingless-Nettle continuity, optional drills, field judgment, ordinary-match clocks, timeouts, and the explicit pre-clock choice. All 30 images decode at their requested dimensions.
- `output/ui-review/tutorial-perfect-r55-label-800/` and `output/ui-review/tutorial-perfect-r55-label-1920/` — final crop check proving `Nettle - WINGLESS ENGINEER` remains fully visible at both supported layouts.
- `output/ui-review/tutorial-perfect-r55-full-800/` and `output/ui-review/tutorial-perfect-r55-full-1920/` — the earlier post-split 347-screen inventories, retained as comparison evidence.
- `output/ui-review/r62-rail-800/` and `output/ui-review/r62-rail-1920/` — six-screen focused acceptance sets covering the placement keyboard rail and all four Intercept coaching pages after their final copy and fit corrections. Both sets render from the same client binary and pass strict rules, accessibility, and young-adult readability review.
- `output/ui-review/tutorial-perfect-r65-full-800/` and `output/ui-review/tutorial-perfect-r65-full-1920/` — historical exhaustive 360-screen inventories retained for comparison.
- `output/ui-review/tutorial-perfect-r83-full-800/` and `output/ui-review/tutorial-perfect-r83-full-1920/` — current final exhaustive 1,379-screen inventories. Both schema-v3 manifests report `completed`, the `ui-capture-complete` marker, 1,379 requested/registered/successful screens, exact registry equality, identical ordered screen lists, 350 Story action screens split 111 Mirewatch / 155 Blackthorn / 84 Seelie, matching build/capture source snapshots, stable inputs, and zero mismatch groups. Both use executable SHA-256 `bb93aefd4076f9eac841f3bcbea3458ee92afd0d39631f33e52392c1dfb0a971`; all 2,758 PNGs decode at exactly 800x600 or 1920x1080 as requested.

All earlier inventories are retained as historical evidence; r83 is the active final baseline. It includes dedicated 1024x1536 custom masters for `se05_court_behind_curtain`, `se22b_city_spends_at_night`, and `se26a_hunger_has_no_face`, plus their registered briefing pages at both supported layouts.

The River Teeth opening shows Telos as a normal unit at B4, three real Bull Gators, an input-neutral instruction for Reed's printed Step, visible ACT/TARGET labels, and no rigging or invisible interaction target. The Seelie captures distinguish guided actions from open objectives, identify `Seelie (YOU)` and `Hostiles`, show a normal End Turn control on open boards, and mark the G5 withdrawal destination directly on the board. The River Teeth replacement sequence is recorded in `GLOOMTHORN_STORY_ART_MANIFEST.md`; the misleading `03_drop_cargo.png` asset has been removed from runtime and from the repository.

## Independent blind review

Fresh no-context reviews were repeated after material corrections. Reviewers were instructed to reject any non-transferable Story-only input, unclear control, invisible target, or mismatch with an authoritative card.

- A rules-naive review caught the misleading `Player Inputs` counter. It is now `Actions Complete`, because one completed game action can involve a drag rather than one mouse input.
- A systems review caught mouse-only wording and verified the exact hidden-collision result in the Blackthorn route. Active movement, attack, deployment, draw, discard, and inspection coaching now uses action verbs plus explicit mouse and keyboard alternatives, and the engine-state regression checks the complete Ambush resolution.
- An accessibility review caught three competing uses of `Step`, forward-only narrative panels, redundant pre-action pacing, and developer jargon. The interface now separates `Guided Action`, `Move 1 of 2`, and Reed's printed `"Step"` action; provides Previous navigation; starts board control sooner; and uses tutorial-facing language. The reviewer accepted the corrected pass.
- A casual review caught ambiguity in the blue and red number badges. The briefing now explicitly identifies both as current Health and maps their colors to player and enemy pieces. The reviewer accepted the corrected pass.
- A mechanics-focused review independently confirmed that MW01 prompts only normal Move, Attack, and End Turn commands; that its named actions match their packaged card profiles; and that the blocked Bow fails before Telos travels and succeeds afterward. The reviewer accepted the corrected pass.
- A final fresh no-context casual/story-first review audited the completed r83 800x600 manifest and every referenced file, then sampled all three routes: the boat-gator opening, BT02 Relentless and both Sharpshooter states, BT15's single Ashenfang body, BT17's earned bypass, late Seelie escalation, and dense goal panels. It found no missing, extra, duplicate, corrupt, or mis-sized frame and returned GO with no blockers.

The first Seelie r10 blind pass correctly rejected an invented Chapter 35 elimination battle, synopsis compression, hidden act goals, compulsory guided inputs before free play, and clipped Mosswake legality copy. The current route restores focused causal scenes and ends its Book One handoff with a three-name orientation for Vesper, Caltheriel, and Lash. It defines COMPLETE and the empty place before Act VI's Rowan objective, then names and removes the concordance while explaining why ten regulator replicas keep Lash's ascent running. It shows why the Emperor turns toward Baalzapub for his first Recall. Vesper's bounded Mirror choice, the Chapter 12 grain terms, Ebbin and Ruvan's separate relay limits, Silkmaw, Pallid, Lash's ascent, the workers' refusal, Zippy's first release, the separate court/host withdrawals, and the final Total Recall are distinct beats. Two late canonical objectives remain ordinary game tasks: Rowan reaches board ground while the civilian gate stays narrative, then Reed and Rowan hold a 28-square witness position without making surrender or Recall a kill condition. Blackthorn now places seven optional practice levels, conditionally requires the all-path 12-card synthesis only when one lacks a play stamp, defaults fully trained players into the required unscripted 4v4 bridge, and offers a complete ordinary-match exam before the canonical climax. It then performs Victor's surrender, testimony, hidden contingency, second attack, and death as a focused scene.

## Release boundary

This record verifies the local implementation, deterministic replays, source guards, adjacent regression suites, and rendered screens. Service-backed integration and instrumented full-campaign human play remain release gates; screenshot acceptance does not replace those activities.
