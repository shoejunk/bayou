# Gloomthorn Story Mode Validation Record

Date: 2026-09-03

## Non-negotiable gameplay boundary

Every required board input in Story Mode must be an ordinary game command executed by the ordinary engine from an authoritative card definition. A story panel may show dialogue, cargo, evidence, chains, votes, custody, or an autonomous character choice, but none of those events may become a substitute card, invisible target, special click, free damage source, or mastery rule.

## Delivered scope

- 53 chronological Book One nodes: 30 Mirewatch and 23 Blackthorn.
- 15 tactical missions: 8 Mirewatch and 7 Blackthorn.
- 38 StoryOnly chapters for scenes that advance the novel but do not have an honest, transferable rules lesson.
- Active legal use and mastery coverage for all 25 distinct starter-deck titles.
- Cross-faction and non-starter characters appear tactically only through real catalog cards.
- The 37 packaged Story Mode card definitions used for offline play and UI capture mirror the live schema-v9 catalog; live catalog definitions take precedence in an ordinary connected session.
- Stable V2 progress remains ID-based after the prologue removal. Retired position-only eight-stage saves restart this substantially different tutorial at Entry 1 rather than silently granting unrelated lessons.

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

Cargo may remain visible in the story art, but no active panel, objective, or step asks the player to manipulate it. There is no rigging target. Board moves and attacks are described as drags, matching the real client input path.

The interface keeps three concepts distinct: `Guided Action` is campaign progress, `Move 1 of 2` is Reed's local objective, and `"Step"` is the printed name of Reed's card action. Briefings and multi-page in-mission story beats provide Previous navigation, and the opening no longer inserts a redundant popup before the first drag.

## Automated verification

| Check | Result |
|---|---|
| Debug client build | PASS |
| storymodetest deterministic suite | PASS — 0 failures |
| Every active tactical golden replay | PASS |
| River Teeth blocked-line / Telos Travel / Reed Bow regression | PASS |
| Rejection of fabricated Story fixtures and non-game mastery | PASS |
| Campaign chronology, direct River Teeth entry, 53-node split, and all 25 starter masteries | PASS |
| Starter-deck database tests | PASS |
| Conquest regression groups | PASS |
| Ordinary engine standalone rules suite | PASS |
| Current 1920x1080 Story captures | PASS |
| git diff --check | PASS |

The service-backed tail of gametest still requires BAYOU_TEST_PASSWORD or BAYOU_SEED_PASSWORD. Its complete standalone rules portion passed; the missing credential is an environment prerequisite, not a Story Mode result.

## Visual evidence

The current correction is captured in:

- output/story-mode-review/real-actions-fix-r4/01-story-select.png
- output/story-mode-review/real-actions-fix-r4/02-story-mirewatch-mission-select.png
- output/story-mode-review/real-actions-fix-r4/03-story-mirewatch-briefing.png
- output/story-mode-review/real-actions-fix-r4/04-story-mirewatch-briefing-actions.png
- output/story-mode-review/real-actions-fix-r4/05-story-mirewatch-briefing-control.png
- output/story-mode-review/real-actions-fix-r4/06-story-mirewatch-game-1.png
- output/story-mode-review/real-actions-fix-r4/07-story-mirewatch-game-1-board.png
- output/story-mode-review/real-actions-fix-r4/08-story-game-1-board.png

The opening board shows Telos as a normal unit at B4, three real Bull Gators, a drag instruction for Reed's ordinary move, visible ACT/TARGET labels, and no rigging or invisible interaction target.

## Independent blind review

Fresh no-context reviews were repeated after material corrections. Reviewers were instructed to reject any non-transferable Story-only input, unclear control, invisible target, or mismatch with an authoritative card.

- A rules-naive review caught the misleading `Player Inputs` counter. It is now `Actions Complete`, because one completed game action can involve a drag rather than one mouse input.
- A systems review caught that the client is drag-driven and verified the exact hidden-collision result in the Blackthorn route. All active movement, attack, and deployment coaching now matches the real input path, and the engine-state regression checks the complete Ambush resolution.
- An accessibility review caught three competing uses of `Step`, forward-only narrative panels, redundant pre-action pacing, and developer jargon. The interface now separates `Guided Action`, `Move 1 of 2`, and Reed's printed `"Step"` action; provides Previous navigation; starts board control sooner; and uses tutorial-facing language. The reviewer accepted the corrected pass.
- A casual review caught ambiguity in the blue and red number badges. The briefing now explicitly identifies both as current Health and maps their colors to player and enemy pieces. The reviewer accepted the corrected pass.
- A mechanics-focused review independently confirmed that MW01 prompts only normal Move, Attack, and End Turn commands; that its named actions match their packaged card profiles; and that the blocked Bow fails before Telos travels and succeeds afterward. The reviewer accepted the corrected pass.

No final blind reviewer found a remaining blocker or high-severity issue in the corrected entry flow.

## Release boundary

This record verifies the local implementation, deterministic replays, source guards, adjacent regression suites, and rendered screens. Service-backed integration and instrumented full-campaign human play remain release gates; screenshot acceptance does not replace those activities.
