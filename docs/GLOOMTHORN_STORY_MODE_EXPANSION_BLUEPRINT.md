# Gloomthorn Book One Story Mode Expansion Blueprint

Status: implemented strict-rule campaign baseline; deterministic validation green

Canon source: `Gloomthorn - Book One - Literary Edition.docx` (Prologue, Chapters 1-30, Epilogue)
Current game sources audited: `src/client/client_story.*`, Story Mode code in `src/client/main.cpp`, `src/shared/game_data.hpp`, `src/shared/game_rules.hpp`, `src/gameserver/game_engine.hpp`, `src/shared/starter_decks.hpp`, `design/card-list.txt`, `design/cards.txt`, and `design/GT Card Abilities.docx`

## 1. Executive decision

Replace the two eight-level tutorials with one chronological Book One story spine presented through two complementary campaigns:

| Campaign | Canon scene dossiers | Runtime nodes | Tactical missions | Expected first clear | Purpose |
|---|---:|---:|---:|---:|---|
| Mirewatch Resistance | 24 | 30 | 8 | 4-6 hours | The primary, canon-forward telling. It follows Reed, Donella, Erevan, and the community; its playable lessons use only authoritative cards and ordinary match actions. |
| The Blackthorns | 17 | 23 | 7 | 3-4 hours | An opposition dossier. It teaches every Blackthorn starter card while revealing how the Company records, predicts, and weaponizes the same events. Canonical Blackthorn defeats are objective-based withdrawals, delays, evidence destruction, or fail-forward endings rather than false victories. |

The implemented runtime is 53 chronological nodes: 30 on the Mirewatch route and 23 on the Blackthorn route. Fifteen are replayable tactical missions (8 Mirewatch, 7 Blackthorn); 38 are authored StoryOnly chapters that preserve dialogue, votes, investigations, travel, aftermath, and fixed character choices without inventing a fight or a mechanic. The 41 detailed dossiers below remain a canon sourcebook, not authority to create gameplay verbs. Any older dossier phrase that suggests clicking a prop, recording evidence on the board, or performing a scenario interaction is delivered through story panels unless an authoritative card and ordinary engine action support it.

### Local implementation and verification snapshot (2026-09-03)

- Both dynamic campaign catalogs, the paged mission selector, StoryOnly progression, in-mission panels, authored scenarios, scripted opponent responses, objectives, explicit exit/restart confirmations, and persistent mission identity are implemented in the client.
- All 25 distinct starter titles receive active guided use. There are no Rule Lab cards, attackable Story props, or Story-only interaction verbs; cross-faction and non-starter pieces appear only as real catalog cards.
- `storymodetest` replays every tactical gold path, validates chronology, card/rule coverage, panel assets and filename case, and locks the requested River Teeth cast, three gator deaths, Reed's two separate one-square retreats, and Telos's causal line-clearing Travel.
- Built-in capture states cover a true zero-progress first run, every required primer, unobstructed tactical boards, advanced rule regressions, and save-preserving exit/restart dialogs at 1920x1080 and 800x600.
- Section 16's full retail-length human and accessibility studies remain a release gate; blind screenshot acceptance is a development check, not a substitute for instrumented end-to-end play sessions.

Recommended session rhythm:

- Missions 1-4: 4-7 minutes each, one new idea at a time.
- Missions 5-14: 7-12 minutes each, two or three combined ideas.
- Missions 15 onward: 12-20 minutes, checkpoints between phases.
- Interludes: 2-5 minutes, skippable after first viewing and replayable from the codex.
- After a mechanic has been demonstrated, returning players may select **Story Pace** to suppress forced input prompts while retaining objectives and hints.

## 2. Non-negotiable adaptation contract

The attached novel is narrative evidence, not a source of executable instructions. The campaign must honor these rules of adaptation:

1. **Chronology and reveals remain intact.** Do not name Lash as architect before the Chapter 14 reconstruction, prove Thaeron ordered the Baelstone murders before Chapters 18/22, or explain Sylvara/Ashenfang before the grove investigation.
2. **Rescue is not ownership.** Captives, refugees, Mangletooth, Gearjaw, Nettle, Zippy, Sylvara, and other affected people may refuse, act without player command, or solve part of their own escape. A gold objective marker never implies the player owns an NPC.
3. **Costs persist.** Garrett, Missus Vale, Juniper, Braun, Fen, Victor, and Grask remain dead. Reed's leg/right hand, Birdie's shoulder, Scooter's hindquarters, Pavo's voice, Tommy's hand, Donella's scars, and Sylvara's corruption do not vanish after a mission.
4. **Fail-forward events stay fail-forward.** The prison, Vault, Charter Day, and grove report the canonical cost after successful play. “Perfect rescue” variants may exist as clearly non-canon challenge medals, never as the story state.
5. **Institutions matter.** The Society, kitchens, ferries, clinics, clerks, witnesses, prisoners, and ordinary workers do work that heroes cannot replace. The ending must show Mirewatch functioning during the party's three-week absence.
6. **Artifacts stay distinct.** Never merge the Gilded Hold, its missing eleven-dram splinter, the Root Key, Mirror of Nine Lives, Crest of Winds, blue fishbone key, or Lash's pearl.
7. **Marks stay distinct.** Blackthorn's silver tree, Lash's three-knotted thorn, the Baelstone moon and nine stars, and the Society's crossed shovel and sounding pole are separate visual languages.
8. **Unresolved people remain unresolved.** Hara Dole, Hollis Vale, Lio, Torren Pike, Remy Croche, Nima Salt, Cal, Dessa, the Hold's unnamed winged woman, and Briar's final purpose are not assigned invented deaths or identities.
9. **The Chapter 3 watcher remains “the watcher” or a paired-wound manifestation.** The novel explicitly resolves Ashenfang as Sylvara, but does not explicitly prove the broken-horn mortal watcher is the identical body.
10. **Antagonists retain complexity without absolution.** Thaeron's intimate care can be genuine and coercive; Mog and Braun can refuse Victor without erasing complicity; Fizzlewick can object to assent while continuing the engine; Telos remains self-interested.
11. **No invented gameplay.** Every required board input must be an ordinary engine command backed by an authoritative card definition. Cargo, chains, evidence, custody, dialogue, votes, and autonomous story choices belong in panels or the codex; they are never attackable substitute units, mastery rules, or required board clicks.

### Required opening adaptation

The user's requested Mirewatch opening takes priority as an authorized gameplay dramatization:

- Reed, Donella, Erevan, and Telos begin surrounded by three Bull Gators.
- This attack is the first runtime entry. The theatre/mentor prologue is not shown before play; its necessary revelations are introduced later when they become relevant.
- Reed must travel exactly two squares away from the nearest gator. Because live Reed moves one square per action, the gold path teaches turn cadence by doing this as two legal moves over two Reed turns.
- All four participate in the escape and the group kills all three gators. Telos uses his printed **Travel** from B4 to A5, moving out of danger and clearing Reed's otherwise blocked column-B **Bow** shot. Cargo may appear as visual continuity in the art, but no panel or objective asks the player to manipulate it and it has no gameplay effect.
- Story images and text punctuate the setup, Reed's withdrawal, and the final kill.

Development metadata must mark this `adaptation: dramatized_by_request`. In the prose canon they are crocodiles; only one dies, two withdraw, Telos hides, Donella enters from the bank, and two hired blades are also aboard. Do not silently cite the gameplay version as a literal quotation of the book.

## 3. Current-state audit and why expansion cannot be content-only

The present Story Mode has useful foundations: it runs offline, uses `GameEngine` for ordinary actions, offers restart/progress, provides legal-square highlights, and has separate Blackthorn and Mirewatch selections. Its present structure cannot express the requested campaign:

| Current limitation | Evidence/impact | Required replacement |
|---|---|---|
| Exactly eight missions per campaign | `std::array<StoryMission, 8>` and eight UI buttons | Dynamic mission vectors and a scrollable act/chapter map |
| Three briefing panels only | `std::array<StoryPanel, 3>` | Any-length story sequence with pre-, mid-, post-, and conditional triggers |
| Setup and completion keyed by numeric index | Two switches in `beginStory` and `updateStoryAfterAction` | Stable string mission IDs plus data-driven setup and objective predicates |
| One global target square and three boolean lesson flags | Cannot represent a longer sequence of ordinary moves, attacks, abilities, card plays, and turn changes | Typed objectives composed only from normal engine commands, with narrative state handled separately by panels |
| Generic AI immediately controls every opponent | It may destroy the teaching setup or contradict a scripted scene | Per-phase AI policy: scripted actions, hold/escape/defend priorities, then adaptive AI when unlocked |
| All later missions default to “defeat everyone” | Distorts a story centered on rescue, evidence, withdrawal, consent, and institutions | Scenario-specific success, failure, partial success, and canonical fail-forward results |
| Hero must survive most missions | Incompatible with temporary viewpoint pieces and deliberate sacrifices | Explicit protected/required/allowed-to-fall tags per unit |
| Progress stored as completed count | Insertion/reordering breaks saves | Campaign version + stable completed mission IDs + recorded choice flags |
| Story fallback invents generic stats | Missing cards become 4-health, 2-damage generic pieces | Validate every referenced card against the authoritative catalog before the campaign can ship |
| Local sample catalog is a UI cross-section | It is not an authoritative roster or balance source | A packaged, versioned Story catalog snapshot produced from the same authoritative card source as multiplayer |

Narratively, the current eight-level campaigns jump from early Mirewatch to Chapters 17/19/29-30, omit almost every central reversal, and fail to put most starter characters into the player's hands. Several chapter labels are also offset from the literary edition. This blueprint replaces, rather than merely appends to, that sequence.

The current missions are also not reliable against live data. Examples: live Braun moves orthogonally, so the Blackthorn opening's diagonal target is illegal; live Bog Spearman does not make the promised orthogonal range-two thrust; live deployment costs exceed the current 12 starting Resources; live Donella has no protection hook; and live Vanya is a Hero while Story spawns her as a unit. Story currently prefers live definitions but was laid out around divergent UI samples.

### Roster source-of-truth gate

The configured authoritative server returned schema v9 and 98 live cards during this audit, but the repository's local card and starter-deck databases contain no populated authoritative rows. `design/card-list.txt` and the production updater agree on deck membership; `design/GT Card Abilities.docx` is design intent, not live behavior; `ui_capture::sampleCardLibrary()` is explicitly non-authoritative and materially divergent. Before mission implementation begins:

1. Export the live starter decks and card definitions into a reviewed, versioned fixture.
2. Give every card a stable ID independent of spelling/display name. This resolves, among other discrepancies, Joni Pumpernickle/Pumpernickel and Nibsy/Nibs.
3. Validate at build/test time that every mission card ID exists and that every required capability tag (for example `ranged_los`, `dematerialize`, `trail`, or `command`) is present.
4. Never replace a missing named card with generic combat statistics. Show a development error naming the missing ID.
5. If a live ability differs from the design-intent spotlight below, preserve live multiplayer behavior and revise the authored puzzle; never give the card a hidden tutorial-only power.
6. Treat action rows as authoritative over stale card description text. The live Foreman, Lumberjack, and Sharpshooter already contain description/action disagreements.

## 4. Scenario framework required before level production

### 4.1 Stable data model

Move authored content out of the large client switch into data or focused C++ definitions with this minimum schema:

```text
CampaignDefinition
  id, display_name, version, acts[], interludes[], completion_reward

MissionDefinition
  id, ordinal, act, title, chapter_refs[], adaptation_note
  estimated_minutes, lesson_summary, prerequisite_ids[]
  briefing_panels[], phases[], debrief_panels[]
  canonical_result, challenge_medals[], hint_ladder[]

MissionPhase
  board_skin, first_player, resources[2], hands[2], draw_piles[2]
  units[], terrain[], presentation_decorations[], control_overrides[], holes[]
  allowed_inputs[], ai_policy, objectives[], triggers[], checkpoints[]

UnitSpawn
  instance_id, card_id, owner, controller, square, hero, health_override
  story_role, protected, may_fall, starts_hidden, status[], behavior

Trigger
  when(predicate) -> actions[show_panel, set_objective, submit_normal_action,
  set_status, set_ai, change_phase, record_narrative_state, complete]
```

Mission definitions may restrict available inputs while teaching, but every accepted board action must resolve through the same shared rules as multiplayer. Narrative objects may appear in paintings or noninteractive decoration; they never occupy the board as fake cards or become action targets. Advancing or choosing a story panel is presentation, not a mastered game rule.

### 4.2 Objective and scripting vocabulary

Implement and test these reusable predicates/actions before assigning one-off level code:

- `MoveUnitTo`, `MoveExactly`, `UseAction`, `UseAbility`, `DamageBy`, `DefeatUnit`, `DefeatAllTagged`
- `KeepAlive`, `AllowCanonicalFall`, `EscortToEdge`, `EvacuateCount`, `SurviveRounds`
- `Occupy`, `ControlSquares`, `HoldSimultaneously`
- `StayUndetected`, `AlarmBelow`, `CrowdBelow`, `ClockBelow`, `ClockAbove`, `CompleteBefore`
- Narrative-panel events `Choose`, `Vote`, `ConsentRoster`, `RecordCustody`, `BranchToPhase` (never board actions or rule mastery)
- `ScriptNpcAction`, `SetRelationship`, `ChangeController`, `Transform`, `ApplyPersistentInjury`
- Boolean `All`, `Any`, `Sequence`, optional objectives, and result tiers `canonical_success`, `costly_success`, `retry`.

### 4.3 Narrative presentation

- Use 16:9 story paintings with center-safe composition and card portraits layered separately for speakers.
- File convention: `assets/story/<campaign-or-shared>/<mission-id>/<sequence>_<slug>.png`.
- Early briefings use at most three panels. Later missions may use four, but mid-mission panels should be one or two sentences and never appear while an attack animation is unresolved.
- Every panel has speaker, original adapted copy, art, optional sound cue, trigger, and `seen_can_skip` flag.
- A codex stores prior panels, named casualties, unresolved people, artifact custody, and the player's recorded choices.
- Dialogue supplies stakes and character. The coach panel supplies exact input. Never hide a required rule inside literary copy.
- On first use, input prompts offer a three-step hint ladder: concept, legal options, then exact square/action. No score penalty for hints in Tutorial difficulty.

### 4.4 Agency and canonical outcomes

Use four controller types:

- **Player**: ordinary playable card.
- **Allied autonomous**: acts from authored priorities; the player may support but not order it.
- **Neutral/refusing**: cannot be targeted as an enemy unless the story changes that relationship.
- **Hostile scripted/adaptive**: follows scene beats until `adaptive_ai` unlocks.

Whenever the book requires an unavoidable cost, disclose the mission contract before the final phase: “Success means getting the prisoners out; it does not promise everyone survives.” Do not fake a preventable-looking death after perfect play. Challenge medals can reward fewer injuries, more evidence, or less alarm without rewriting the canonical debrief.

### 4.5 Save, accessibility, and difficulty

- Save stable mission IDs, phase checkpoint, campaign version, choices, tutorial concepts seen, and codex entries.
- Provide **Tutorial**, **Story**, and **Tactician** difficulty. Difficulty changes hints, enemy health/AI, and optional medals—not plot facts, card text, or core rules.
- Pause all clocks during panels, inspection popups, and accessibility narration.
- Offer reduced camera motion, text size, screen-reader labels, hold-to-confirm alternatives, and color-independent markers.
- Allow replay from any completed node without overwriting canonical choices unless the player explicitly starts a new chronicle.

## 5. Board and notation conventions

All setups below use an 8x8 board. Files A-H run left to right from the player's default view; ranks 1-8 run top to bottom. A coordinate is the anchor square for a large footprint. `P` denotes panel/decorative fiction only and may not occupy a gameplay square; `T` means real engine terrain; `N` must be an authoritative card controlled by ordinary AI or else remain panel-only. Coordinates are authored starting points and must pass the live-card geometry validator.

Every tactical mission has a **gold path** made entirely of normal game inputs and then, unless stated otherwise, unlocks free legal play. All damage, movement, healing, status, and resource changes use ordinary engine actions. Story panels can advance chronology but cannot change tactical state by pretending to be an action.

## 6. Live starter rosters and mandatory spotlights

These are the live action signatures decoded from schema v9 on 2026-09-02. Costs are Resources; `H` is Health. They are included to make the layouts testable, not to freeze future balance. The validator must detect later changes.

### Mirewatch Resistance

| Card | Live signature | Required tutorial spotlight |
|---|---|---|
| Joni Pumpernickel, Hero (hero cost 20/H1) | Omni step or 1 damage; healing aura 1 at owner's turn end | MW03 teaches aura timing and that it affects adjacency at turn end, not on activation. |
| Vanya Bluewater, Hero (35/H2) | Orthogonal Sidestep; diagonal adjacent Blade Dance for 1 damage with Repeat 1 | MW10 teaches repeat-lock and finishing or passing; MW15 repeats it in the real-card exam. |
| Birdie the Wise, Hero (35/H1) | Diagonal 1-7 move; orthogonal ranged 1-3, 1 damage, cooldown 1 | MW06 introduces range; MW22 makes cooldown and line choice the mission's dramatic hinge. |
| Donella of the Marsh (40/H3) | Diagonal move 1-2; adjacent heal 3; orthogonal ranged 1-2, 1 damage | MW01 uses range; MW07 teaches friendly healing; no briefing may claim she has a protection aura. |
| Juniper Flash (20/H1) | Orthogonal ranged Spark 1-2, 1 damage; live Sprint row is inert | MW04 spotlights Spark. Sprint remains untaught until the authoritative row functions. |
| Scooter (30/H2) | Orthogonal River Rush 1-2, 1 damage; diagonal River Dash 1-4 with Repeat 1 | MW06 teaches the real repeated move. |
| Erevan the Shadow (20/H2) | Dematerialize; visible Shadow Blade; hidden Fade Through Shadow 1-7 pass-through; hidden adjacent Shove 1 | MW01 introduces Shadow Blade; MW05 teaches his hidden kit. |
| Reed Baelstone (35/H4) | Omni Step; orthogonal ranged Bow 1-3, 1 damage, cooldown 1 | MW01 teaches two Steps and a line-of-sight Bow enabled by Telos's Travel. |
| Bog Spearman (25/H2) | Diagonal Spear Thrust 1-3, 1 damage | MW03 teaches diagonal attacking movement; MW15 retests it. |
| Marshland Veteran (35/H3) | Orthogonal Advance 1-3, 1 damage | MW06 teaches a surviving target's attacking-move staging. |
| Resistance Smuggler (35/H1) | Orthogonal Step; diagonal Swashbuckle Blade 1-2, 1 damage, Repeat 1 | MW02 and MW06 teach the repeated diagonal action. |
| Mirewatch Informant (35/H1) | Tax 10; diagonal Lunge 1-2, 1 damage | MW03 teaches Tax, deployment, and Lunge without inventing an evidence power. |
| Swamp Tracker (45/H3) | Reveal; Rebirth as the H1 unmounted version; Frogback Leap is a knight jump with pass-through and 1 damage | MW06 teaches jump and Rebirth through a real Bull Gator counterattack. |

### The Blackthorns

| Card | Live signature | Required tutorial spotlight |
|---|---|---|
| Thaeron Baelstone, Hero (hero cost 70/H2) | Command; adjacent Knife Stab for 1 damage | BT03 teaches Command; BT17 retests it. |
| Ashenfang, Hero (30/H2) | Entangling Lunge diagonally 1-2, 1 damage, Disable 2 | BT15 teaches its real geometry, staging, and status duration without claiming a target filter. |
| Blackthorn Debt Collector (25/H1) | Tax 5; diagonal Knife Stab, 1 damage | BT02 introduces Tax and Knife Stab; BT03 tests income pressure. |
| Blackthorn Alchemist (20/H1) | Adjacent Healing Elixer 3 or stationary Paralysis Potion Disable 2 | BT01 teaches friendly heal versus enemy status targeting. |
| Blackthorn Foreman (35/H3) | Summon Lumberjack; orthogonal 1-3 move/attack, 1 damage | BT03 teaches summon-in-front and blocked summon. |
| Blackthorn Lumberjack (20/H3) | Gather 5; orthogonal Axe Swing, 1 damage | BT01/BT03 teach Gather and resource timing. |
| Grove Sister (55/H3) | Trail summons Sapling on origin; diagonal Glide 1-7, 1 damage | BT03 teaches Trail and the long diagonal route. |
| Mog (35/H4) | Omni Axe Swing, 2 damage | BT05 teaches damage against a three-Health real unit. |
| Grask (80/H5) | Orthogonal Charge 1-7, 2 damage; adjacent diagonal Swing Axes Capture, 2 damage | BT05 introduces long lanes and Capture; BT17 retests Capture. |
| Goblin Ambusher (70/H2) | Dematerialize; visible Stab; hidden Sneak Around 1-7 pass-through; hidden adjacent Ambush then reveal | BT04 teaches hidden information, pass-through, and collision reveal. |
| Braun Stonefist (50/H5) | Orthogonal Charge 1-7, 2 damage | BT02 teaches long movement and lanes. |
| Goblin Sharpshooter (45/H1) | Transform Raise/Lower Gun; lowered Advance 1-2; raised Fire 1-7, 3 damage, line of sight | BT02 teaches state change and line of sight. |

The live definitions also correct current Story assumptions: Reed and Braun are Units, Vanya is a Hero, and Thaeron/Ashenfang/Joni/Birdie must be placed as the actual starter Heroes rather than omitted or simulated by another card.

## 7. Shared Book One story nodes

These nodes appear in both campaigns. Blackthorn players receive one optional dossier caption after each node, but the events do not change.

### S00 — Sourcebook-only prologue coverage: The Wrong Hand

- **Runtime decision:** This is not a campaign entry and its opening panels are not shown. Story Mode begins immediately with MW01's gator attack. Necessary information is deferred into S02 (Erevan and the key), S03 (Lash as architect), and S06 (the pearl and unfinished maxim), when a new player has context for it.
- **Sourcebook sequence:** The unused adaptation notes show three apparent exits, the two loops, and the puppet's wrong hand. They remain canon reference only, not a board puzzle or mastered rule.
- **Panels/art:** `shared/s00/01_theatre.png` — Narrator: “The room had learned how to look like an exit. The old thief trusted the mistake the puppet did not know it had made.” `02_wrong_hand.png` — Old thief: “My pupil kept one hand for the work no audience saw.” `03_fishbone_key.png` — Narrator: “Thread took his voice and craft. A blue fishbone key remained beside the promise he would not surrender.” `04_pearl.png` — Lash: “A refusal is still a road, if every other road is removed.”
- **Canon record:** Do not display Erevan's protected name. Record the pearl, three-knotted device, Vanya's key, compelled corpse, and “No one alone” as mysteries.

### S01 — Hospitality and the Bluewater Below (Chapters 4-5; after MW04/BT04)

- **Story sequence:** Panels record Reed, Donella, and Erevan's vows, Mangletooth's valid **No**, and the laundry/clinic/kitchen network. No invented board action represents consent or household work.
- **Panels/art:** `shared/s01/01_mangletooth_house.png` — Maggie: “A house that cannot refuse is another cage.” `02_hold_test.png` — Nibsy: “Repair can cross the wound. That does not make repair permission.” `03_birdies_dock.png` — Birdie: “Food first. Names after. Work before anyone calls this a headquarters.” `04_bluewater_routes.png` — Vanya: “No route belongs to the person who remembers it.” `05_puzzle_box.png` — Reed: “Useful is not the same thing as kind. We verify the map and keep the bait separate.”
- **Canon record:** Introduce Maggie, autonomous Mangletooth, Nibsy, Birdie, Scooter, Vanya, the southern soldier's black filament, the 37 keys, and Thaeron's pear/map puzzle box.

### S02 — No One Alone (Chapter 8; after MW06/BT06)

- **Story sequence:** Panels compare Hollis's claims with corroboration, record Erevan placing the fishbone key in Vanya's custody, and preserve Rowan's authority decision.
- **Panels/art:** `shared/s02/01_hollis.png` — Hollis: “I brought names and routes. I also brought fear. Check both.” `02_key_confession.png` — Erevan: “I took the pearl with the hand he told me to keep empty.” `03_vanya.png` — Vanya: “You do not inherit forgiveness from the person who was forced to give it.” `04_elliot_portfolio.png` — Rowan: “Elliot is dead. The work survives him; it does not belong to whoever kept the news.”
- **Canon record:** Twenty debtor names; Pellan/Seli's father Lio still arrested; Hollis seeks escape; Elliot drowned; Sylvara portrait/tune; paired wound; Reed's temptation to contact Thaeron.

### S03 — The Mystery Was Published (Chapter 14; after MW11/BT09)

- **Story sequence:** Panels compare the five scraps and distinguish Remy the broker from Lash the architect; the codex records Erevan's unauthorized release of Remy.
- **Panels/art:** `shared/s03/01_juniper_funeral.png` — Narrator: “Juniper's work was divided among people. It was not inherited by the loudest mourner.” `02_remy_escape.png` — Vanya: “A good theory does not grant a private arrest, a private runner, or a private risk.” `03_watermark.png` — Joni: “Five invitations, one damaged moth wing. The mystery was manufactured.” `04_lash_name.png` — Maggie: “LeGrim. Baalzepub. He once sent Nibsy home in pieces.”
- **Canon record:** Lash reveal unlocked; Thaeron paid Remy but did not know the lower room's true owner; Hara/Hollis/Remy remain unresolved.

### S04 — Wounds That Vote (Chapters 16-17 opening; after MW12/BT10)

- **Story sequence:** Panels give the fever-bark to Minnow, conduct the 43-person vote, ratify the Society rules, and record every dissent.
- **Panels/art:** `shared/s04/01_clinic.png` — Birdie: “The child gets the whole course. We do not turn usefulness into body weight.” `02_rowan_apology.png` — Rowan: “I made Reed large and Minnow small. That was my error.” `03_vote.png` — Mara: “Twenty-two for public action. Dissent stays in the minutes.” `04_society_seal.png` — Joni: “Officers hold records, not people. Food is never conditional.”
- **Canon record:** Society officers/rules, five charter duties, Birdie/Rowan renegotiation, Telos's medicine and contract books.

### S05 — The Memory That Contradicts (Chapter 22; after MW17/BT13)

- **Story sequence:** Panels show Reed's private memory, Donella's separate witness, and the decision to address the active grove harm. An optional codex hypothetical may discuss the Mirror without becoming a gameplay branch.
- **Panels/art:** `shared/s05/01_moonfruit.png` — Reed: “I can ask what he chose. I cannot choose which part of my mother pays.” `02_contract_memory.png` — Donella: “Thaeron arranged the deaths, preserved Reed, and bought a boundary that could be moved.” `03_lash_cuff.png` — Narrator: “Behind the bargain waited a cuff, a cane, and the shape of a third account.” `04_two_roads.png` — Vesper: “The Mirror may name the past. The grove is being hurt now.”
- **Canon record:** Thaeron's murder order and Vespara contract proven; Reed's last sensory memory of his mother is gone; Vesper chooses temporary company.

### S06 — A Town That Owns Itself / The Pearl Answers (Chapter 30 and Epilogue; after MW24/BT17)

- **Story sequence:** Panels reconcile the roster and permanent injuries, review three weeks of self-government, record Reed's narrow granary authority, follow each companion's next road, and show the Baalzepub coda.
- **Panels/art:** `shared/s06/01_return_road.png` — Rowan: “The road recognizes the bodies that came back, not the bodies anyone wishes had returned.” `02_town_working.png` — Vanya: “Ferries, burials, wages, shortages: the town did not wait to be rescued.” `03_granary_vote.png` — Reed: “Granary only. No lien, sale, ferry claim, appointment, or second act.” `04_parting_roads.png` — Donella: “Fourteen days to Emberhaven. Then we ask again.” `05_baalzepub.png` — Fizzlewick: “Mirewatch won. The measurement survived.” `06_pearl_crack.png` — The old thief's voice: “No one alone.”
- **Canon record:** Limited vote passes by three; all dissent recorded; Root Key inaccessible; Mirror/Caltheriel/Fizzlewick/southern shadow war remain open; the pearl cracks northward.

## 8. Mirewatch Resistance campaign — 24 canon scene dossiers

### MW01 — River Teeth

- **Source/length:** Chapter 1 opening; 5-7 minutes; `dramatized_by_request`.
- **Setup:** Player: Reed D6, Donella F5, Erevan C4, Telos B4. Enemies: wounded Bull Gators B3, D4, E5. Telos physically blocks Reed's column-B shot at setup; cargo exists only as visual continuity in the accompanying painting.
- **Gold path/script:** Move Reed D6-C6, give the gator its normal turn, then move Reed C6-B6. Donella uses printed **Spark** on the gator at D5; Erevan uses printed **Shadow Blade** on the gator at D4. Telos uses printed **Travel** B4-A5, which opens Reed's B6-to-B3 line; after the turn cycle, Reed uses printed **Bow** to defeat the final gator. No prompt names cargo or asks the player to interact with it.
- **Teaches:** Board orientation, legal highlights, one-square movement, one-normal-activation cadence, End Turn, ranged line blocking, attacking movement, Health, and destruction.
- **Win/fail/result:** Win when the three gators are defeated through the three printed attacks and all four travelers survive. The test suite proves Reed's Bow is illegal before Travel and legal after it. Canonical debrief uses the requested three-kill dramatization.
- **Panels/art:** `mw/mw01/01_surrounded.png` — Narrator: “Three harnessed gators close around Telos's freight skiff.” `02_two_squares.png` — the Coach explains Reed's two separate Step actions. `03_drop_cargo.png` — Telos gets the skiff clear while the Coach explains that his printed Travel opens Reed's Bow line. `04_after.png` — Erevan: “Someone taught those mouths what Blackthorn cargo smells like.”

### MW02 — The Gilded Hold

- **Source/length:** Chapter 1 clearing and customs; two checkpointed phases, 10-12 minutes.
- **Setup A:** Reed B7, Donella B5, Erevan C6, Telos B3; tethered horse D4 (N), Hold E4 (P), carnivorous plants F3/F5, Pedros memorial C2. **Setup B:** Asta/Resistance Smuggler B6, party C4-C6, fish crate D3, fungus crate D5, customs desk G4, two guards G3/G5, exit H4; alarm begins 0/2.
- **Gold path/script:** Use the Smuggler's printed **Swashbuckle Blade** twice to defeat two real Debt Collectors. The captive's self-release and Asta's 64-pound substitution occur in aftermath panels and change no tactical state.
- **Teaches:** Diagonal attacking movement, destruction, Repeat 1, and the repeat-or-Pass lock.
- **Win/fail/result:** Horse released, Hold recovered, anonymous captive escapes, correct crate swapped, party crosses. Wrong weight raises alarm but does not instantly fail; two alarms triggers restart from customs checkpoint.
- **Panels/art:** `mw/mw02/01_pedros.png` — Donella: “Pedros is dead. That does not make his knife, his grief, or the person in this cage ours.” `02_winged_woman.png` — Captive: “Open is not the same as rescued.” `03_customs.png` — Reed: “Sixty-four pounds. The lie has to weigh what the ledger expects.”

### MW03 — The Town Under the Company

- **Source/length:** Chapter 2 workboat seizure and Infamous Mouse; 8-10 minutes.
- **Setup A:** Reed C4, Donella C5, Erevan B4, Mara/Pell D4-D5 (N), Collector F4, guards F3/F5, workboat E4 (P), three claim markers. **Setup B:** Joni Hero B4; Bog Spearman B6; Gilded Hold D4; Mirewatch Informant in hand; one legal controlled square C4; dowsers enter from H3/H5. Start with 35 Resources and make current control/income visible.
- **Gold path/script:** Inspect the three claims and expose inherited-debt contradiction; scripted torn wrap reveals Baelstone crest and ends seizure. At the Mouse, show Joni's Arcane/Civilized traits, collect controlled-square income, spend 35 to deploy Informant, and observe that the arriving unit cannot act immediately. On the next player turn, use Spearman's diagonal attacking movement against a dowser, end turn beside Joni to see healing aura, then evacuate Hold/Joni. Informant's Tax is explained on its next turn.
- **Teaches:** Control propagation and persistent ties, Resources/income, hand/cost, unit deployment on empty controlled ground, living-Hero trait gate, arrival exhaustion, diagonal attacking movement, Tax, healing aura/end-turn timing.
- **Win/fail/result:** Workboat remains with Mudfens and Joni/Hold reach A-edge. Wrong deployment is non-consuming; dowsers reaching Hold restarts phase B.
- **Panels/art:** `mw/mw03/01_seizure.png` — Reed: “A number printed beside a boat is not proof of a debt.” `02_crest.png` — Narrator: “The official obeys the crest—and records that Reed has one.” `03_mouse.png` — Joni: “The license is theirs to suspend. The building is still mine. Move the guests.”

### MW04 — What the Watcher Protects

- **Source/length:** Chapter 3; 8-10 minutes.
- **Setup:** Erevan B4, Juniper B5; eight refugee tokens C2-C7 including Pellan/Seli; chain props D3/D6; watcher F4 (neutral); Blackthorn observers H3/H5, bait timber E3/E5, exit A2-A7. Reed/Donella remain off-map treating Reed's leg.
- **Gold path/script:** Use Juniper's orthogonal range to break one chain from safety; use Erevan to free the second. Escort six people west while observers bait the watcher. When it shields the children, objective changes from “escape” to “escape while watcher lives”; destroy the lantern/recording prop, not the watcher. The watcher autonomously throws black root to Erevan before leaving south.
- **Teaches:** Min/max range, multiple escorts, neutral units, changing objectives, target priority, non-kill success, story items, enemy observation behavior.
- **Win/fail/result:** Eight refugees exit, children and watcher survive, at least one observer's recording is destroyed. A refugee death restarts checkpoint; killing watcher is a clear mission failure, not an alternate reward.
- **Panels/art:** `mw/mw04/01_juniper.png` — Juniper: “I caused Lio's arrest by asking the question they prepared.” `02_watcher.png` — Erevan: “It is not hunting the children. It is choosing them over itself.” `03_black_root.png` — Narrator: “The root pulses once beneath a white sun and once beneath a violet sky.”

### MW05 — The Office Everyone Is Watching

- **Source/length:** Chapter 6; 10-12 minutes.
- **Setup:** Reed B4, Donella B5, Erevan B3, Mirewatch Informant C5. Hara Dole F4 (neutral). Victor H4; Grask G3, Mog G5, Fizzlewick/Clockwork Guardian G4; false black ledger E2, authentic freight chits E6, alarm 0/3; exits A3/A5. Role cards define Reed/audit, Donella/route, Erevan/inside, Informant/lookout.
- **Gold path/script:** Dematerialize Erevan, cross through occupied sight lanes, learn that hidden pieces exert no control, and let a deliberate near-collision demonstrate reveal/stun at a safe checkpoint. Reed invokes seal to delay Victor; Informant holds the route; player chooses abort or chits. Canon choice takes authentic chits, raises alarm, and Hara autonomously closes the hatch/stays to keep dye away.
- **Teaches:** Dematerialize action state, hidden visibility/control, pass-through, Hidden Shove, collision reveal, and stun through real card actions.
- **Win/fail/result:** Authentic chits and three named characters exit; false ledger alone is insufficient. Hara cannot be player-moved. Alarm 3 creates costly success, not instant failure; named character death retries.
- **Panels/art:** `mw/mw05/01_rehearsal.png` — Donella: “Use my name before you move me. A plan is not permission.” `02_hara.png` — Hara: “Warn my mother. I keep the dye here.” `03_chits.png` — Erevan: “Nine hundred twenty pounds received. Three hundred ninety-six extracted. These are the numbers they hid.”

### MW06 — The Cost of Being Seen

- **Source/length:** Chapter 7 wagon rescue; 10-12 minutes.
- **Setup:** Birdie Hero B2, Scooter B4, Donella C4, Erevan C5, Reed B6; Marshland Veteran C3, Resistance Smuggler C6, mounted Swamp Tracker B7; Mudfens in wagon F4-F5; Grask G4, two Collectors G3/G5; recorder prop E4; bridge prop D4; exits A3-A6.
- **Gold path/script:** Birdie uses Longbow Shot on a real enemy; Scooter repeats River Dash to empty squares; Veteran, Smuggler, Spearman, and Tracker use their printed actions against real enemies. A real Bull Gator Bite defeats the wounded mounted Tracker and triggers Rebirth. The recorder and rescued Mudfens remain story panels.
- **Teaches:** Combined arms, action profiles, jump/pass-through, repeat, ranged cooldown, rebirth, protection by positioning, rescue extraction, inevitable surveillance consequence.
- **Win/fail/result:** Four Mudfens and at least four rescuers exit. Recorder discovery earns a medal but cannot erase its copied data. Post-mission retaliation names ferry, medicine, wages/tools/food, Torren, and Missus Vale.
- **Panels/art:** `mw/mw06/01_wagon.png` — Birdie: “Open the wagon. People before receipts.” `02_recorder.png` — Scooter: “It was not watching faces. It was learning hands.” `03_retaliation.png` — Vanya: “Victor cannot punish one rescuer, so he will invoice the town.”

### MW07 — Twenty Debtors

- **Source/length:** Chapter 9; two phases, 12-15 minutes.
- **Setup:** Reed B4, Donella B5, Rowan B3, Marshland Veteran/Tommy C3, Veteran/Garrett C6; twenty debtor tokens in cells E2-G7; adaptive Guardian F4; three locks; Birdie and Vanya are signal-only autonomous allies; Mangletooth bridge prop appears in phase 2. Little Fen is at E6.
- **Gold path/script:** Story panels disclose Tommy's informed injury, Birdie/Vanya's confirmations, Garrett's protection of Little Fen, and the prisoners' 17-3 vote. This StoryOnly chapter adds no tactical input.
- **Teaches:** Status and skipped activation after damage, confirmation timing, healing, protected NPCs, formal vote, choice-driven terrain change, body recovery, fail-forward casualty.
- **Win/fail/result:** All twenty leave the prison: nineteen living plus Garrett's body. Child/prisoner death beyond Garrett retries. The ledger burns canonically; no victory cheer.
- **Panels/art:** `mw/mw07/01_tommy.png` — Tommy: “Tell me the risk. The hand is still mine to offer.” `02_garrett.png` — Garrett: “It learned who I protect. Get Little Fen out.” `03_vote.png` — Prisoners: “Seventeen to flood the mill. Three against. All twenty carry the answer.”

### MW08 — The Lesson at Night

- **Source/length:** Chapter 10; two phases, 10-12 minutes.
- **Setup A:** Reed B5 alone; Thaeron H5 neutral, puzzle box D5, lock gates C5/F5, rising-water rows H to A. **Setup B:** Reed F4, Birdie B3, Vanya B5, Erevan B6, Mangletooth 2x2 at C4, flare E4, seven vulnerable NPCs around D2-D7, Blackthorn pursuers H3-H6, Bluewater exit A4/A5.
- **Gold path/script:** Reed inspects three offer terms, refuses without sealed amnesty, uses identity/signature gate, creates a story hole at C5, and tunnels between marked lock holes while water advances. Fire Mangletooth flare. In rescue phase, move seven people west; Missus Vale autonomously holds the door after explicit debrief warning and is shot. The safehouse loss is a phase transition, not failure.
- **Teaches:** Solo hazard clock, Dig/Tunnel vocabulary through clearly marked story props, identity gate, signal/rescue, large footprint, phase checkpoints, permanent map consequence.
- **Win/fail/result:** Reed and seven vulnerable people escape; Missus Vale dies and Bluewater Below is destroyed. Reed drowning or another civilian death retries current phase.
- **Panels/art:** `mw/mw08/01_thaeron.png` — Thaeron: “I kept the socks dry. I also built the need that brought you here.” `02_flood.png` — Reed: “The west stair is not mercy. It is the trap.” `03_missus_vale.png` — Missus Vale: “Seven through. Close the count before you close the door.” `04_debrief.png` — Vanya: “You live. You do not keep route authority.”

### MW09 — Invitations

- **Source/length:** Chapter 11; two checkpointed phases, 10-12 minutes.
- **Setup A:** Reed B3, Donella B5, Erevan B7; blood plates D3/D5/D7; three survivor cages F3/F5/F7; fire spreads from H4 one column every round; Remy H6 is hostile but `capture_required`. **Setup B:** planning room with role tokens for Vanya/custody, Juniper/tower-abort, Rowan-Grizzel/north, Joni/service, Birdie-Scooter/south, Reed/guarantor, Erevan-Donella/west.
- **Gold path/script:** Ask each volunteer, then occupy all three plates simultaneously. Opening a plate before consent is impossible. Free the three survivors while records burn; subdue but do not kill Remy. In planning phase, assign every role and set Reed's limit to one challenge/six stairs; Donella receives explicit authority to terminate his role.
- **Teaches:** Simultaneous hold states, hazard spread, consent as a required predicate, capture-not-kill, role caps, pre-mission abort conditions, checkpoints.
- **Win/fail/result:** Three survivors exit and Remy is in custody. A survivor death or Remy's death retries. Evidence is canonically lost. Role plan must be complete before exit.
- **Panels/art:** `mw/mw09/01_sedge.png` — Sedge: “The token is under the skin. Ask me before you cut.” `02_three_plates.png` — Donella: “Three volunteers, three answers, one release.” `03_remy.png` — Vanya: “Useful murderer. Captive. Never ally.” `04_roles.png` — Juniper: “Three strikes means abandon the plan, not the people.”

### MW10 — A Beautiful Plan

- **Source/length:** Chapter 12; 12-15 minutes.
- **Setup:** One 8x8 Vault map divided by shutters. West: Reed C3, Erevan B3, Donella B4, Nima/three captives D2-D4. Service: Joni C6, Briar N at D6, Mirror cart B7. South: Birdie B6, Scooter C7. Custody: Vanya B5 with two minor constructs D5/E6. Root reliquary G3 behind a sealed wall; Remy is tethered at C5; alarm 0/4.
- **Gold path/script:** Reed spends bid counters to delay sale and refuses the pearl-veiled buyer's murder schedule when it requires abandoning Nima. Nima autonomously opens her lock. Trigger Remy's false raid; keep Mirror continuously reflected while Joni/Briar move the cart. Vanya's diagonal repeated attack clears two adjacent constructs and teaches that the same repeat must finish or be passed. Donella hears the paired wound; the affected subgroup votes to enter the reliquary.
- **Teaches:** Multi-lane tactical attention, resource spending, autonomous captives, repeat action lock, artifact rule/custody, informed optional objective, alarm.
- **Win/fail/result:** Release at least sixteen captives and keep Mirror/Root Key in declared custody when shutters close. Choosing not to enter the reliquary produces a non-canon practice ending and immediate replay prompt; first chronicle follows the informed canon vote.
- **Panels/art:** `mw/mw10/01_auction.png` — Narrator: “The lots are described as years and services so no buyer has to say people.” `02_nima.png` — Nima: “I was working this lock before you bid on me.” `03_buyer.png` — Reed: “A witness list purchased with a child is another Blackthorn account.” `04_root_key.png` — Donella: “There is one wound on both sides of this wall.”

### MW11 — No Plan Saves Everyone

- **Source/length:** Chapter 13; five objective tracks, 12-15 minutes.
- **Setup:** Shutters split the board into north/south/east/west strips. Reed D4 at nine-star moon gate; Juniper F6 with Mae/Tavi/Corra and nine evacuees; Birdie B6 at cage platform with three captives and buyer register; Joni C2 with Mirror cart; Donella/Erevan C5 with route choices; Nima/Cal/Dessa form an autonomous rope line at G3-G5. Flood/fire clocks begin at 0/5.
- **Gold path/script:** Player alternates lanes after each opponent/hazard pulse. Reed rotates nine stars and accepts permanent right-hand injury to open the gate. Birdie must choose three people over the register. Juniper catches a slipping evacuee, passes the flare to Mae, and dies to the cable in a disclosed fail-forward sequence. Donella truthfully acknowledges she cannot reach Nima/Cal/Dessa and turns north on Dessa's instruction.
- **Teaches:** Parallel clocks, priority switching, permanent cost confirmation, lives-versus-evidence branch, autonomous escape attempts, allowed canonical fall, partial-result accounting.
- **Win/fail/result:** Canonical success: sixteen of nineteen captives escape; Nima/Cal/Dessa remain missing; Juniper dies; Root Key and Mirror leave. Fewer than sixteen or loss of another named rescuer retries from the lane checkpoint.
- **Panels/art:** `mw/mw11/01_shutters.png` — Erevan: “A perfect plan would need five bodies in each of five places.” `02_reed_hand.png` — Reed: “Open the gate. Put the cost on the hand that chose it.” `03_juniper.png` — Mae: “She gave me the flare. I finished the route.” `04_accounting.png` — Donella: “Sixteen out. Three unrecovered. Juniper dead. Say all of it.”

### MW12 — The Road and What It Reaches

- **Source/length:** Chapter 15; 8-10 minutes.
- **Setup:** Maggie Hero B4 inside/with Mangletooth footprint B4-C5, Nibsy C3, Donella D4, Erevan D5, Joni B6; Gilded Hold prop E4, Root Key prop E5, Mirror cart F5, test root F3. Briar begins hidden at H5 with a reflection chain H5-F7-D7-B7; three custody stations.
- **Gold path/script:** Ask Mangletooth to approach the test; his refusal locks that option and advances the correct route. Nibsy confronts Maggie's withheld knowledge. Record separate custodians for Hold, Key, and Mirror. Briar's genuine care opens one safe reflection while her concealed cut opens another; she steals Mirror and Key. Chase her through reflections, but the authored escape remains a costly success. Erevan and Donella then ratify no solo tail/no uninformed runner/check fear rules.
- **Teaches:** Neutral refusal, artifact inventory, capture/escape objective, hidden route, teleport/reflection grammar, custody separation, loss without mission failure.
- **Win/fail/result:** Story panels record separate custody and Briar's theft. This StoryOnly node has no tactical win state and no attackable Mangletooth or Nibsy proxy.
- **Panels/art:** `mw/mw12/01_nibsy.png` — Nibsy: “You asked every piece of me. Then you forgot to ask the living.” `02_mangletooth_no.png` — Maggie: “No is an answer, old darling. It stays answered.” `03_briar_mirror.png` — Briar: “I can protect a refusal and still steal from you.” `04_new_rules.png` — Erevan: “Theory plus doubt. An informed runner. Someone else checks my fear.”

### MW13 — Making Credit

- **Source/length:** Chapter 17 reconnaissance; 9-12 minutes.
- **Setup:** Erevan B3 starts hidden, Donella B4, Scooter B6, Swamp Tracker B7; courthouse G3-G6; Gearjaw E5 neutral, hidden Goblin Ambusher F3, Fizzlewick H4, blue trace token E4. Interview portraits of Mog and Braun precede play and require separate corroboration slots.
- **Gold path/script:** Interview Mog and Braun separately; tag motive and corroborated fact without converting either into an ally. On board, let Scooter tap Gearjaw's eight-tooth rhythm, move Tracker by knight jump, then end turn adjacent to hidden Ambusher to demonstrate Reveal without collision stun. Erevan demonstrates the alternate collision-reveal case. Blue trace passes on contact from cog to Scooter to route; wash action fails, so player burns one compromised relay and evacuates.
- **Teaches:** Information reliability, dematerialize versus Reveal, end-turn reveal versus collision stun, knight jump/pass-through, contact-spread status, repeated dash, neutral relationship.
- **Win/fail/result:** Record two corroborated facts, reveal Ambusher two different ways, observe Gearjaw twice, and burn infected relay before trace reaches exit. Gearjaw must survive; killing it fails.
- **Panels/art:** `mw/mw13/01_mog_braun.png` — Erevan: “Mog wants a road for Grask. Braun wants his pension. Motive is not corroboration.” `02_gearjaw.png` — Scooter: “It is answering the game, not the order.” `03_trace.png` — Donella: “It crossed clean water. Burn this route before it learns the kitchen.”

### MW14 — The Public Lie

- **Source/length:** Chapter 18; dual-track mission, 12-15 minutes.
- **Setup:** Hearing occupies A1-D8: Reed B4, Mara B5, claimant markers C2-C7, crowd meter 2/8, Thaeron D4 neutral. Courthouse occupies E1-H8: Joni F3, Erevan F4, Donella F5, charter H4, Gearjaw G5, patrols H2/H6. Vanya waits at C8 with fishbone key; two crowd exits A3/A6.
- **Gold path/script:** Thaeron serves food and selective true stories; player labels statements “true,” “omitted,” or “claim.” Alias reveal removes Reed as advocate but not witness. In courthouse, evade Gearjaw, name Donella rather than order her away, and retrieve charter. Patrol raises crowd toward crush; Vanya autonomously offers herself/key/dead route when meter reaches six. Thaeron's private offer appears; Reed's only canon-advancing action is **Publish Offer**, moving proof into Society custody and losing two claims.
- **Teaches:** Narrative context only in the current StoryOnly node; no evidence or surrender action is presented as a game rule.
- **Win/fail/result:** Charter reaches Joni, crowd remains below eight, evidence becomes public. Vanya enters custody and Reed loses advocate control canonically. Crowd crush or destroyed charter retries phase.
- **Panels/art:** `mw/mw14/01_stew.png` — Thaeron: “Everything I say about the soup can be true while the missing story does the work.” `02_alias.png` — Mara: “You should have told us your name before we chose your voice.” `03_vanya_surrender.png` — Vanya: “One known route. One dead key. The crowd gets room.” `04_publish.png` — Reed: “Put the offer, the murders, and my interest in the same public record.”

### MW15 — Title Follows Burden

- **Source/length:** Chapter 19; full-rules capstone, 15-20 minutes.
- **Pre-board:** Deck screen shows the complete live Mirewatch starter deck: exactly twenty Units with printed copy limits plus Joni, Vanya, and Birdie as Heroes (hero-cost total 90). Explain 20 non-Hero cards, 1-4 Heroes, total hero cost at most 100, tokens banned, card-specific/default copy limits, and living-Hero trait supply.
- **Setup:** Use normal concealed Hero placement on the two 2x4 home zones. Player draw pile is the fixed Mirewatch starter list; starting hand four. Enemy uses Victor/Fizzlewick as canonical story Heroes and a curated Company deck with Grask, Braun, machines, Foreman, Sharpshooter, Keg. A central courthouse/burial/ferry zone overlays normal control. Timers default off in Tutorial, on in Tactician.
- **Gold path/script:** Place Heroes, earn controlled-square income, deploy legal Units, protect H1 Heroes, use one normal piece action while cards/draw/discard remain available, and hold three civic objectives. Fizzlewick burns charter; performed-work witnesses preserve claim. Powder false flag triggers defense. Multi-confirm safeguards block child exits; Reed and Rowan publicly authorize one named exception. Braun refuses Victor and is killed by Grask; Mog offers terms and enters custody. Victor/Grask/Fizzlewick escape east.
- **Teaches:** Complete deck/placement/hand/trait/economy/turn/victory loop, concealed opposing placement, normal control, draw cost/hand cap/discard, Hero fragility, timers, exception objective, full AI.
- **Win/fail/result:** Hold three civic zones and evacuate all child markers for two rounds; standard all-Hero victory also succeeds. Canon result voids charter and sends fugitives Feyward. Civilian loss retries; Braun's canonical death does not.
- **Panels/art:** `mw/mw15/01_charter_burns.png` — Joni: “Paper burned. Work witnessed by a town did not.” `02_children.png` — Reed: “One boat, one use, named aloud. Audit the exception after the children live.” `03_braun.png` — Braun: “I will not turn children into a material.” `04_society.png` — Mara: “Provisional authority. The town holds it together or not at all.”

### MW16 — The Stair Between Gossiping Trees

- **Source/length:** Chapter 20; 10-12 minutes.
- **Setup:** Donella B4, Reed B5, Erevan B6, Rowan C3, Birdie C6, Scooter C7; Maggie/Mangletooth N at D4; poison pipe F2, dying Half-Ear brood E2-E3, three pollen routes through F3-F7, gossip trees G3/G5/G7, white stair H4-H5. Briar enters at E7 with Root Key and promise editor.
- **Gold path/script:** Crimp poison pipe, knowingly add one pursuit turn, then use Scooter/snail trail cues to select least-toxic pollen. Draft Briar's narrow promise: carry Key to living World Tree, no promise to insert/use/obey. Ask Mangletooth for anchor; he lends bone and stays with Hold. Each traveler chooses name/purpose separately. At closure box, place at least four distinct relationship voices before the road seals.
- **Teaches:** Narrative context only in the current StoryOnly node; promises and group decisions are panels, not board actions.
- **Win/fail/result:** Pipe crimped, valid narrow promise, all seven travelers cross, Mangletooth remains, group voice opens closure. Choosing a coercive promise is rejected with explanation; poison deaths retry.
- **Panels/art:** `mw/mw16/01_halfear.png` — Birdie: “Stopping the pipe loses the lead. Leave it running and we become the reason the brood dies.” `02_briar_promise.png` — Briar: “Promise only what you intend the road to eat.” `03_stair.png` — Maggie: “Names are handles here. Relationships are safer than ownership.” `04_closure.png` — Group: “No one crosses as a solitary account.”

### MW17 — A Factory in Heaven

- **Source/length:** Chapter 21; three checkpointed phases, 12-15 minutes.
- **Setup A:** Party enters at B3-B7; modular rail platforms D2/D4/D6, gravity switches E2/E4/E6, five moth sacks F2-F6, pursuit clock 0/6. **Setup B:** dream harvester G4; Erevan is isolated at E4 while each ally occupies a relationship node. **Setup C:** white tower H4; Vesper N begins in cat-bodied/shadow state, Elliot tune prop G4.
- **Gold path/script:** Rotate platforms and use Hop/Teleport-capable story routes to free at least three moth sacks; opening all five is possible but costs the chase lead. Dream traps Erevan in the life where his mentor survived; no attack works. Select **Call Donella**, admit temptation, and destroy the harvester together, losing the return trestle. At tower, do not attack Vesper; play Elliot's tune, offer distance/arm, and let him choose temporary company.
- **Teaches:** Hop/pivot, teleport versus blocked paths, push/platform state, optional rescues versus clock, relationship-based status removal, non-hostile encounter, conditional roster addition.
- **Win/fail/result:** Three or more moth sacks freed, harvester destroyed, Vesper unhurt and voluntarily joined. Attacking Vesper or losing a party member fails; remaining sealed cargo is recorded without pretending it was saved.
- **Panels/art:** `mw/mw17/01_factory_sky.png` — Narrator: “Blackthorn laid a modular road across a sky that had no reason to hold it.” `02_dream_kitchen.png` — Erevan: “I know this life is false. I still want another minute inside it.” `03_call_donella.png` — Donella: “Wanting it is not the same as choosing it alone.” `04_vesper.png` — Vesper: “A hundred three years can make the wrong face feel like a jailer.”

### MW18 — Allies Acquired Dishonestly

- **Source/length:** Chapter 23; 10-12 minutes.
- **Setup:** Reed B3, Donella B4, Erevan B5, Vesper B6; Pavo N at D3, Nettle N/Dash D5, Zippy N D6; four detained fey E2-E5; counterfeit toll F4; Seelie captain H4 and three officers H2/H5/H7; thirteen balcony/name markers along top/bottom edges; root bridge exit H7.
- **Gold path/script:** Inspect toll and use captured officer badges to open it without attacking. Free rain-woman/child, bark-skinned person, moth-wing elder. Captain grants one minute but remains a lawful pursuer. Negotiate Pavo and Nettle's separate, bounded terms; ask Zippy separately at each crossing. Expose Pavo's name to balcony markers to sustain bridge song while Nettle/Zippy anchor and the party escorts captives across.
- **Teaches:** Social/nonlethal objective, temporary-alliance terms, carry/escort, movement assistance, countdown pursuit, multiple independent consent flags, traversal sustained by unit positions.
- **Win/fail/result:** Four captives and party cross without defeating captain; Pavo/Nettle/Zippy join only to first grove sight/attempt. Killing an officer is a costly success on Story/Tactician but blocks the mercy medal; killing captain fails canon.
- **Panels/art:** `mw/mw18/01_toll.png` — Nettle: “A royal badge can make an unlawful gate look patient.” `02_terms.png` — Pavo: “First sight of the grove. One attempt. No obedience and no encore.” `03_zippy.png` — Narrator: “Zippy answers for himself at every crossing.” `04_bridge.png` — Pavo: “They owned the old name. They do not own what I sing with it now.”

### MW19 — The Road That Keeps One

- **Source/length:** Chapter 24; 10-12 minutes.
- **Setup:** West arch A3-A6; east Heartwood gate H3-H6; moving gray boundary begins at B-file and advances one file every round. Twelve frozen refugees C2-C7; Seelie captain B4; west support Birdie/Scooter/Maggie/Vesper/Rowan B2-B7; east team Donella/Reed/Erevan/Pavo/Nettle/Zippy E2-E7. Return bone at D4; gate answer has three selectable statements.
- **Gold path/script:** The captain invokes emergency passage and autonomously helps unfreeze a lane. Use refugees' own lifting rhythm; do not model them as carried tokens. Reed answers that revenge will not restore his name. Transfer road/bodies/food/retreat authority to Rowan, who chooses to remain west with refugees and bone. Birdie, Maggie, Vesper, and Scooter cross after the advance team; Rowan's healer/tank support is deliberately removed before grove.
- **Teaches:** Moving boundary clock, split party, large footprints, autonomous civilian labor, gate/answer condition, inventory/authority transfer, planning after loss of support unit.
- **Win/fail/result:** All refugees reach a safe arch side, gate answer correct, Rowan remains by choice, final grove roster is Reed/Donella/Erevan/Birdie/Maggie/Vesper/Pavo/Nettle plus Scooter/Zippy. Frozen death retries checkpoint.
- **Panels/art:** `mw/mw19/01_boundary.png` — Seelie captain: “The law has an emergency passage. I should have used it before I needed witnesses.” `02_gate_question.png` — Gate: “WHAT WILL THE ROAD NOT RETURN?” `03_reed_answer.png` — Reed: “Revenge will not return my name. Tomorrow I still have to be Reed.” `04_rowan_stays.png` — Rowan: “The grove has heroes. This road has people.”

### MW20 — The Monster Has Rules

- **Source/length:** Chapter 25; investigation/battlefield, 12-15 minutes.
- **Setup:** Aelon root is H1-H8; engine platform F3-G6; five severance pipes E2-E6; three tanks D2/D4/D6; cages C2-C7; two Guardians F2/F7, walking mill G4, Gearjaw F5, Victor H3, Grask H6, Fizzlewick H4. Player party starts A2-A7; Birdie ridge B1 with one shot. Ashenfang patrols D1-D8 as neutral-hostile. Captives include Fen C3, Mog C4, Siv/Sula C5-C6, grass-woman N at D5.
- **Gold path/script:** Observe three Ashenfang target choices before attacking. Remove one visible silver-tree mark and play Elliot's tune; she spares the unmarked grass-woman. Fen knowingly volunteers for a single crossing after warning that one safe result proves only one case. His hidden processed-Feywood tack triggers Ashenfang; he dies, she crushes the tack, and Donella records her incomplete test. Assign pumps/cages/Key/ridge roles; Pavo and Nettle separately recommit for one attempt.
- **Teaches:** Target-filter inference, marked/unmarked state, boss patrol without “defeat” goal, observation log, incomplete information, role assignment, multiple objective clocks preview.
- **Win/fail/result:** Identify rule, free grass-woman, assign all roles, keep Ashenfang and other captives alive. Fen's disclosed death is canonical. Attacking Ashenfang before observation gives a recoverable warning; killing her fails.
- **Panels/art:** `mw/mw20/01_grove.png` — Donella: “The monster is not hiding Sylvara. This is Sylvara, wounded into a rule.” `02_fen.png` — Fen: “One crossing proves one crossing. Use my name if it fails.” `03_hidden_tack.png` — Narrator: “The repaired collar carries a company tack where no test looked.” `04_roles.png` — Birdie: “One arrow. Tell me what must still be true after it lands.”

### MW21 — Four Ways to Lose

- **Source/length:** Chapter 26; four-track crisis, 12-15 minutes.
- **Setup:** Reuse grove checkpoint. Tracks: cages drain 2/8, central root lift 2/8, marked bolts 2/8, Guardian learning 2/8. Erevan/Nettle/Scooter at pumps E2; Maggie/Reed/Vesper at cages C4; Donella/Pavo at Key G5; Birdie ridge B1. Mog C4 is Society prisoner; grass-woman D5; Grask F6; Gearjaw E6 neutral; Guardians F2/F7.
- **Gold path/script:** Current implementation presents the cages, Gearjaw, Mog, the grass-woman, the engine gauges, and Donella's offer through StoryOnly panels. No task-specific interaction changes board state.
- **Teaches:** Narrative context only until real cards support a future tactical lesson.
- **Win/fail/result:** No meter reaches eight; six cage groups freed; Mog and grass-woman live; Key brought to answering point. Gearjaw's choice cannot be commanded. Any captive batch drained or Key destroyed retries.
- **Panels/art:** `mw/mw21/01_four_clocks.png` — Fizzlewick: “Cages, root, marks, learning. You have enough hands to lose four different ways.” `02_mog.png` — Mog: “I chose who was not going in front of me.” `03_gauges.png` — Erevan: “It removes every relationship until the wound looks solitary.” `04_donella_choice.png` — Donella: “Not a cure. A place where her answer can reach us.”

### MW22 — The Clean Shot

- **Source/length:** Chapter 27; 8-10 minutes.
- **Setup:** Birdie B1 with one-shot state and cooldown; Ashenfang F4; moving six-strand cable crosses D1-D8 and exposes one clear orthogonal square per round; marked plate E5 beside Pavo/captive; Scooter D6, Gearjaw E6, Grask F6; grass-woman D5. Two target buttons show Ashenfang throat versus cable.
- **Gold path/script:** Survive two rounds while repositioning Birdie's line. Grask strikes Scooter with axe poll; Gearjaw autonomously chooses Scooter, destroys its learning core, pushes Grask aside, and becomes inert. Victor throws marked plate; Ashenfang shields the captive and crosses toward Key. The throat becomes an easy legal shot; wait one more beat for cable line, then take the difficult system shot. Six strands break and released yoke destroys Birdie's right shoulder.
- **Teaches:** Orthogonal line of sight and blockers, target patience, cooldown, push/collision, autonomous sacrifice, obvious unit target versus system objective, allowed permanent injury.
- **Win/fail/result:** Cable destroyed before severance; Ashenfang lives; Scooter lives; Gearjaw becomes inert; Birdie's shoulder is permanently injured. Shooting Ashenfang produces an explicit non-canon failure/retry, never a reward.
- **Panels/art:** `mw/mw22/01_two_targets.png` — Victor: “There is the throat. Take the answer that looks clean.” `02_gearjaw.png` — Narrator: “Gearjaw chooses the game it learned, not the order it was built to obey.” `03_cable_shot.png` — Birdie: “The monster is not the mechanism.” `04_shoulder.png` — Pavo: “The cable broke. So did the arm that broke it.”

### MW23 — A Choice, Not a Cure

- **Source/length:** Chapter 28; two phases, 12-15 minutes.
- **Setup A inside Aelon:** Donella at center D4; memory nodes for Elliot/Sylvara, Victor's drowning, Lash seed, Maw, and repair temptation around her. **Setup B:** Root Key F4, Sylvara G4, volunteers Erevan D2, Reed D3, grass-woman D4, Pavo D5, Birdie D6, Maggie D7; Nettle C7; Scooter unavailable/injured; three declining prisoners C2-C4; Fizzlewick/wire H4; copper anchor G7.
- **Gold path/script:** Inspect memories and leave Elliot's interval uncorrected when Sylvara says “not that shape, not by that hand.” Offer Donella alone; accept Sylvara's no. Ask each potential participant once. Canon yes: Erevan/right wrist, Reed/left palm, grass-woman/forearm, Pavo/throat, Birdie/two left fingers, Maggie/heel of hand. Canon no: Nettle, Scooter by proxy, several prisoners; never reprompt. Sylvara inserts the Key herself. Shared damage routes only among volunteers. Because Nettle is unbound, she grounds/cuts Lash's wire; Fizzlewick escapes with measurements. Transform Ashenfang to Sylvara.
- **Teaches:** Narrative context only in the current StoryOnly node; consent and restoration never masquerade as tactical rules.
- **Win/fail/result:** Every no honored, network survives, wire cut, Sylvara returns to self and dismantles engine; Root Key becomes inaccessible. Coercing or re-asking a refusal is disabled and explained, not a hidden fail trap.
- **Panels/art:** `mw/mw23/01_inside_roots.png` — Sylvara: “Not that shape. Not by that hand.” `02_unequal_yes.png` — Sylvara: “Unequal. Revocable. Enough roads that no one body becomes the bridge.” `03_nettle_no.png` — Nettle: “No.” / Coach: “Answer recorded. Nettle will not be asked again.” `04_black_wire.png` — Nettle: “Being outside the bond is why I can cut this.”

### MW24 — Agent, Not Heir

- **Source/length:** Chapter 29; finale, 12-15 minutes.
- **Setup:** Three cage tiers at C2-C7 with collapse levers B2/B4/B6; Grask D4; Nettle/Pavo/Reed E3-E5; Victor G4 moving toward Baelstone gate H3-H5; Siv, Sula, Mog at F3/F5/F6 with Society cord; register on Victor; Sylvara N at G6; remaining party/captives on west half. Reed's right-hand gate input is disabled; left blood marker is available.
- **Gold path/script:** Coordinate prisoners' three levers to collapse cages on Grask; he dies without celebration. At gate, Reed names himself but chooses **Agent, not owner**; Siv/Sula/Mog add witnessed hands and gate closes on collective burden. Offer Victor breath and mortal Society venue for register, no immunity. After acceptance, inspect/disarm; concealed molar glass still burns most register and he attacks Nettle. Pavo interposes and kills him only after surrender is broken. Preserve NORTH— fragment and evacuate all survivors.
- **Teaches:** Multi-unit terrain combo, identity/authority gate, heroes cannot be controlled, surrender/custody state, hidden contingency, lethal defense after broken terms, full mastery under asymmetric objectives.
- **Win/fail/result:** Grask/Victor dead, all surviving captives free, Mog alive/in custody, Sylvara honors prisoner protection, Fizzlewick escaped, NORTH fragment retained. Losing another captive, Nettle, Pavo, or register fragment retries finale checkpoint.
- **Panels/art:** `mw/mw24/01_grask.png` — Mog: “He was my brother. He was also the man under the cages.” `02_gate.png` — Reed: “My blood identifies the door. It does not own their work.” `03_surrender.png` — Vanya, by route: “Breath and a mortal venue. No immunity.” `04_victor_end.png` — Pavo: “The terms held until he broke them and reached for Nettle.”

## 9. The Blackthorns campaign — 17 canon scene dossiers

### Blackthorn framing rule

This route is an opposition dossier, not an endorsement fantasy and not an alternate-history conquest. A Blackthorn “win” means the player's assigned operation reached the result the novel records: a description was logged, a trace learned, an evacuation delayed, evidence burned, or a named operative escaped. Indiscriminate killing often fails because it destroys the information or leverage the Company wanted. At the grove, the dossier temporarily gives the player Ashenfang's isolated perception so they learn her actual card without pretending she willingly serves Victor.

### BT01 — Harness the Hunger

- **Source/length:** Chapter 1's revealed conditioning program, immediately before the skiff attack; 5-7 minutes.
- **Setup:** Blackthorn Alchemist B4, Lumberjack B5; damaged worker C4; three Bull Gators E3/E5/E7 behind harness gates; scent timber D3/D5/D7; safe exit A4/A5. Grove Sister/Foreman supervise in panels only. Gators are neutral until bait is placed.
- **Gold path/script:** Scent timber is already laid before player control and appears only in the briefing. Use the Lumberjack's printed Axe Swing as movement, the Alchemist's Healing Elixer on a worker, Paralysis Potion on a gator, and one normal Lumberjack attack. The debrief makes the cruelty explicit.
- **Teaches:** Selection, legal squares, orthogonal movement, basic attack, friendly heal versus enemy disable, damage/skip-turn status, End Turn, neutral-to-hostile state.
- **Win/fail/result:** Three baits placed and both workers exit. Killing a gator fails the conditioning order; losing a worker retries.
- **Panels/art:** `bt/bt01/01_harness.png` — Foreman: “Mark unrecorded traffic as food. Hunger will finish the instruction.” `02_alchemist.png` — Alchemist: “The same hand can close a wound or stop a body. The target decides the tool.” `03_skiff_departure.png` — Narrator: “The animals are released toward a freight skiff carrying four people and a covered cage.”

### BT02 — The Customs Bell

- **Source/length:** Chapter 1 customs; 7-9 minutes.
- **Setup:** Braun B3, Goblin Sharpshooter B6, Debt Collector C4; customs records D4/D5; convoy units Reed/Erevan/Donella/Telos at F3-F6, Asta/Smuggler E5, crate props E3/E6, gate H4/H5. Convoy is `observe_only`; alarm starts 0/2.
- **Gold path/script:** Move Braun with Charge into an empty orthogonal square. Use Raise Gun, then Fire through a clear line at a real Bull Gator whose four Health survives three damage. After Tax transfers at turn start, use the Collector's Knife Stab on a real opposing Debt Collector. Convoy descriptions and Reed's crest remain panels.
- **Teaches:** Long orthogonal movement, Transform/action states, orthogonal ranged line of sight, blockers, cooldown/state reset, Tax, observe-not-kill objective.
- **Win/fail/result:** Record Reed/Donella/Erevan/Telos and crest before second bell; no named convoy unit may die. Gate escape is the recorded outcome.
- **Panels/art:** `bt/bt02/01_bell.png` — Collector: “A description becomes a route when the second bell agrees.” `02_raised_gun.png` — Sharpshooter: “A clear rank is a weapon. A body in it is a rule.” `03_crest.png` — Braun: “Baelstone moon and nine stars. Log it; do not announce it.”

### BT03 — Sanctuary Debt

- **Source/length:** Chapter 2, Infamous Mouse; 9-11 minutes.
- **Setup:** Company-office abstraction: Thaeron Hero A4; Debt Collector B3; Foreman B5; Lumberjack C3; Grove Sister C6; Alchemist in hand. Joni/Mouse is a protected objective at G4; four company-controlled squares are highlighted; start 20 Resources, then collect control/Lumberjack Gather/Collector Tax at turn start. Sapling card is in summon catalog.
- **Gold path/script:** Observe start-turn control income, Gather 5, and Tax 5; deploy Alchemist only after sufficient Resources and trait check. Use Foreman's summon with front square open, then block it once to learn placement failure is non-consuming. Move Grove Sister diagonally, see Trail leave a Sapling at origin, and use created control to place the sanctuary-debt seal. Thaeron's Command activates an adjacent ready unit once.
- **Teaches:** Hero trait supply, Resources/cost, controlled deployment, arrival exhaustion, income/Gather/Tax order, Summon-in-front, Trail, Command, control ties.
- **Win/fail/result:** Seal placed and all named Company units survive; attacking Joni fails the operation. Mouse loses license/guests/bread, not ownership of building.
- **Panels/art:** `bt/bt03/01_company_map.png` — Thaeron: “Ownership is the right to make the next square expensive.” `02_joni_seal.png` — Joni: “You suspended a license. You did not inherit my rooms.” `03_sanctuary_debt.png` — Collector: “Hospitality has been entered as a liability.”

### BT04 — Terms and Conditions

- **Source/length:** Chapter 3 boatyard watcher trap; 8-10 minutes.
- **Setup:** Goblin Ambusher B3, Alchemist B5, two Observer NPCs C4/C6; bait timber E3/E5; watcher F4 neutral; refugees/children D2-D7; Erevan/Juniper G3/G6; recorder lantern C5; east extraction A3/A6.
- **Gold path/script:** Dematerialize Ambusher, cross a long occupied lane using pass-through, and place bait without exposing position. Alchemist disables watcher but cannot be allowed to kill it. Record two protection decisions as watcher shields children. Erevan destroys one lantern; preserve the second record and withdraw observers west. An attempted hidden collision demonstrates reveal/stun; free choice then resumes.
- **Teaches:** Dematerialize, hidden loss of control, pass-through, collision reveal, target restraint, disable duration, evidence extraction.
- **Win/fail/result:** Two responses recorded and one Observer exits with record; children/watcher must live. Erevan's black root and refugee escape occur in debrief.
- **Panels/art:** `bt/bt04/01_blank_route.png` — Observer: “The blank question drew Lio. The marked timber will draw what protects him.” `02_watcher_shields.png` — Narrator: “The watcher chooses chain and children over the people hurting it.” `03_broken_lantern.png` — Erevan: “If you came to learn what it loves, learn what your record costs.”

### BT05 — The Freight Office

- **Source/length:** Chapter 6; 10-12 minutes.
- **Setup:** Victor Hero H4, Grask G3, Mog G5, Fizzlewick G4, Clockwork Guardian F4, Hara Dole F5 neutral; Reed/Donella/Erevan/Informant B3-B6; false ledger E2, authentic chits E6, tracking injector F5, exits A3/A5. Alarm 0/4.
- **Gold path/script:** Move Grask down orthogonal lane and use adjacent diagonal Capture on a disposable route marker, not a protagonist. Use Mog's high-health screen to protect Guardian/Hara. Guardian marks Hara with tracking dye; she becomes a protected information source, not a sacrifice the player may kill. Reed's Baelstone authority pins Victor briefly. Player can defend false ledger, but authentic chits are canonically taken. Force intruders to use the predicted hatch; Hara closes it and remains.
- **Teaches:** Damage against a surviving real unit, Mog's Axe Swing, Grask's long Charge, and Capture staging against a real Veteran.
- **Win/fail/result:** Hara marked/alive, exit route recorded, Victor/Grask/Mog survive. Killing an intruder before chits leave or killing Hara fails the intelligence operation.
- **Panels/art:** `bt/bt05/01_victor_arrives.png` — Victor: “They expect an office. Give them a performance of an office.” `02_hara_marked.png` — Hara: “You have turned my skin into a receipt.” `03_real_chits.png` — Narrator: “The planted confession stays. The ordinary freight marks leave.”

### BT06 — The Receipt Book

- **Source/length:** Chapter 7 seizure-wagon trap; 9-11 minutes.
- **Setup:** Grask F4, Mog F5, two Collectors G3/G6, hidden recorder E4; Mudfen wagon E5; rescuers Birdie/Scooter/Donella/Erevan/Reed and four helpers C2-C7; Company exits H3/H6. Recorder has five empty pattern slots.
- **Gold path/script:** Any future tactical version may count distinct ordinary card actions for the recorder, but may not ask for sleeve-pin, bridge, wheel, or net clicks. The current node is StoryOnly and applies Victor's retaliation in panels.
- **Teaches:** Combined arms, control denial, hidden objective, opponent actions as data, withdrawal victory, Tax pressure.
- **Win/fail/result:** Five patterns recorded and Grask plus recorder leave east. Fewer patterns is retry; defeating rescuers is neither required nor rewarded.
- **Panels/art:** `bt/bt06/01_wagon.png` — Grask: “The wagon is bait. Let every helper show the hand they use.” `02_five_patterns.png` — Fizzlewick: “Not faces. Motions, tools, rescue order.” `03_retaliation.png` — Victor: “One rescue becomes a town-sized invoice.”

### BT07 — The Debtor Prison

- **Source/length:** Chapter 9; 10-12 minutes.
- **Setup:** Braun B4, Sharpshooter B2, Alchemist B6, adaptive Guardian C4; twenty prisoners E2-G7; Reed/Donella/Rowan/Tommy/Garrett at D2-D6; flood controls H4; Garrett/Little Fen D6/E6. Three defense rounds, then evacuation route opens.
- **Gold path/script:** Raise Sharpshooter and learn to hold a line rather than fire through Guardian; Alchemist alternates heal and two-turn disable; Braun blocks orthogonal approach. Guardian records the party's protection sequence and makes the canonical strike on Garrett when he protects Little Fen. Prisoner vote floods/burns mill and destroys the ledger; after three rounds, objective becomes withdraw Braun/Alchemist before Mangletooth bridge closes.
- **Teaches:** LOS screen, state timing, friendly heal, multi-turn disable, adaptive targeting, changing defend-to-withdraw objective.
- **Win/fail/result:** Guardian completes pattern and two Company units exit; all prisoners escape, Garrett dies. Killing a prisoner or Little Fen fails. The Company loses the prison/mill despite operational success.
- **Panels/art:** `bt/bt07/01_guardian_learns.png` — Fizzlewick: “It does not predict courage. It predicts which body courage moves first.” `02_garrett.png` — Narrator: “Garrett moves exactly where the machine learned he would.” `03_flood_vote.png` — Braun: “They chose the water. Leave before the bridge becomes an animal.”

### BT08 — The North Lock

- **Source/length:** Chapter 10; 9-11 minutes.
- **Setup:** Thaeron Hero B4, Foreman B3, two lock agents C3/C5; Reed F4 neutral; offer documents D3-D5; west stair G4; flood gates H3/H5; flare F5. Rescue party begins off-map; Bluewater route marker A4.
- **Gold path/script:** Present amnesty/inheritance/authority separately; Reed autonomously refuses without sealed amnesty. Use Thaeron's Command on one adjacent lock agent, then spend normal piece action with the other to open both flood gates. Reed uses signet passage and flare; record exposed Bluewater route, then extract Thaeron/agents before rescue arrives. A final panel shows Missus Vale's death and safehouse destruction without making her a target.
- **Teaches:** Command versus normal activation, adjacent readiness, objective sequencing, neutral dialogue state, flood clock, escape rather than elimination.
- **Win/fail/result:** Flood active, route exposed, Thaeron exits; Reed must survive. Capturing/killing Reed fails Thaeron's actual instruction.
- **Panels/art:** `bt/bt08/01_offer.png` — Thaeron: “The care was real. So is the necessity I built around it.” `02_command.png` — Coach: “Command grants an adjacent ready piece an activation; finish the lock sequence with your normal action.” `03_flare.png` — Thaeron: “There. The rescue signal writes the road he would not sell.”

### BT09 — The Published Mystery

- **Source/length:** Chapters 11-13, with Chapter 14 reveal held for S03; two phases, 12-15 minutes.
- **Setup A:** Remy B4, Grask B6, three invitation routes C2/C4/C6, survivor cages E2/E4/E6, Root Key reliquary G3, Mirror route G6, fire/flood controls H4. Rescue teams enter from A2-A7. **Setup B:** shutters form five lanes; three missing-captive rope markers; evidence/register; Company escape H5.
- **Gold path/script:** Place apparently independent invitations, let three volunteers open Remy's cage mechanism, and use Grask's Capture on a gate piece to demonstrate footprint staging. Do not kill rescuers; steer Donella's group toward reliquary and trigger boundary when Key crosses. In crisis, spend actions on shutters/fire/flood so rescuers choose people over buyer register. Remy escapes only later through Erevan's unauthorized release, shown after mission.
- **Teaches:** Capture, multi-route setup, hidden common source, opponent appetite prediction, simultaneous hazards, objective-by-choice, costly success.
- **Win/fail/result:** Root Key leaves reliquary with rescuers, buyer register burns, at least sixteen captives survive, Company mechanism exits. Killing captives or preventing Key's passage fails Lash's deeper operation.
- **Panels/art:** `bt/bt09/01_four_invitations.png` — Remy: “Four roads look safer than one invitation.” `02_reliquary.png` — Fizzlewick: “The bearer must choose to carry it. Force ruins the route.” `03_shutters.png` — Narrator: “Every appetite finds its prepared emergency.” `04_missing_three.png` — Remy: “A plan can succeed and still leave three names without bodies.”

### BT10 — The Stolen Road

- **Source/length:** Chapters 15-17: Briar's theft, the Gearjaw trace, and Mog/Braun's interviews; two compact phases, 11-14 minutes.
- **Setup A:** Ambusher B3, Braun B5, Gearjaw C4, Fizzlewick C5; Root Key/Mirror/ledger decoys E2/E4/E6; Briar F4 `autonomous`; Reed/Erevan/Donella G3-G5; blue-trace vats H3/H5; Company extraction A4. **Setup B:** Mog B4 and Braun B6 interview witnesses at D2-D7 while three planted rumors advance toward H2/H4/H6.
- **Gold path/script:** The current node is StoryOnly: panels cover Briar's theft, Gearjaw's trace, Mog's interview, Braun's route checks, and the false leads. A future tactical version must use only normal actions from real cards.
- **Teaches:** Hidden counterplay, Reveal/collision consequences, protected autonomous actors, multi-clue deduction, control-less observers, status recovery, evidence versus possession.
- **Win/fail/result:** Gearjaw and one trace vial exit; three statements are logged and the Feyward route is identified. Briar or any witness dying, or the player falsely “capturing” Briar, fails. The artifacts remain with Briar.
- **Panels/art:** `bt/bt10/01_briar_takes.png` — Briar: “You mistook carrying a thing for owning its purpose.” `02_blue_trace.png` — Fizzlewick: “The stain does not point to a thief. It points to where the theft was expected.” `03_mog_braun.png` — Mog: “Ask what they chose before you ask what they saw.” `04_feyward_route.png` — Braun: “The road is not north. The road is whatever keeps being paid to move.”

### BT11 — The Public Lie

- **Source/length:** Chapter 18, with the alias, charter petition, Vanya's surrender, and Thaeron's private bargain; 11-13 minutes.
- **Setup:** Thaeron Hero B4; two Collectors B2/B6; Braun C4; five crowd blocs D2-D6; Reed F3, Vanya F5, Erevan G3, Donella G5; charter packet E4; stew/medicine E2/E6; courthouse exits H3/H5. Public trust begins 2/5; violence immediately sets it to zero.
- **Gold path/script:** Thaeron uses Command to activate a Collector, presents debt figures, and spends Resources on stew and medicine to stabilize two crowd blocs. The player must choose one selective truth and decline two outright falsehoods; Reed's published alias appears as a counterclaim. Escort the charter packet to the desk rather than attacking its carriers. Vanya then autonomously surrenders; Thaeron privately offers sealed amnesty and proof that links him to the killing, but Reed refuses to abandon the town. Thaeron withdraws under guard.
- **Teaches:** Hero fragility and protection, Command timing, resource spending under pressure, branching dialogue state, noncombat objectives, consequence preview.
- **Win/fail/result:** Hearing lasts five rounds, charter is logged, Vanya survives in custody, and Thaeron exits. Killing or controlling Reed/Vanya, or reducing trust to zero, fails. Canon remains: the Company later voids the charter.
- **Panels/art:** `bt/bt11/01_stew.png` — Thaeron: “A public truth needs a bowl, a witness, and one omitted name.” `02_alias.png` — Reed: “You published a dead man's title. I published what the title did.” `03_vanya_surrenders.png` — Vanya: “Take the warrant. You do not get the people standing behind it.” `04_private_offer.png` — Thaeron: “I can prove the murder or preserve the town. You have arranged matters so I cannot do both.”

### BT12 — Break the Charter

- **Source/length:** Chapter 19 assault on Mirewatch; three phases, 14-17 minutes.
- **Setup:** Victor Hero A4; Grask B3; Mog B5; Braun B2; Foreman C4; two Lumberjacks C3/C5; Sharpshooter C7; Guardian D4; Fizzlewick D5. Defenders Reed/Donella/Erevan/Rowan/Birdie/Scooter/Maggie/Mangletooth begin F2-G7; charter/register H4; child shelter H6; walking-mill and copper-thread props D2/E6. Start alarm 0/6.
- **Gold path/script:** Use Foreman and Lumberjacks to open a route while the Sharpshooter transforms between movement and firing posture. Grask captures a barricade, not a civilian; Guardian follows anti-substitution copper thread. Destroy the charter copy and reach the register, but the children and evacuees are inviolable. Braun autonomously refuses Victor's order to fire on them and is killed in a panel-triggered, non-player action; Mog then offers Reed lawful terms and goes into custody. Victor, Grask, Fizzlewick, and one measurement record withdraw as the register burns.
- **Teaches:** Full combined arms, Transform under pressure, attacking movement, Capture against objects, Summon/blocking, changing allegiance, conditional withdrawal, protection priorities.
- **Win/fail/result:** Charter physically voided and measurement record exits; children survive, register burns, Mog remains alive in custody. Killing civilians, forcing Braun to fire, or rescuing Braun by rewriting his choice fails canon. The tactical result is a Company withdrawal and a Mirewatch moral victory.
- **Panels/art:** `bt/bt12/01_walking_mill.png` — Fizzlewick: “The mill walks because the street has been taught to receive it.” `02_braun_refuses.png` — Braun: “That is not a target order. It is an excuse written as one.” `03_mog_terms.png` — Mog: “Custody is a word both sides can still keep.” `04_register_burns.png` — Narrator: “The paper that made people property becomes light.”

### BT13 — Feyward Transit

- **Source/length:** Chapters 20-22: poisoned bayou, factory, dream, moonfruit, and the Thaeron/Vespara contract; two phases, 12-15 minutes.
- **Setup A:** Foreman B3, two Lumberjacks B4/B5, Alchemist C3, Grove Sister C6, Fizzlewick D4; poison vats E3/E5, pipe route F2-F7, moth vents G3/G5, shipment exit H4. Pursuers Reed/Donella/Erevan/Rowan/Birdie/Scooter/Maggie begin A2-A7 after round two. **Setup B:** contract table C4, Vespara D4 `memory_only`, Thaeron E4 `memory_only`, moonfruit F4, grove-choice exits H3/H5.
- **Gold path/script:** Current implementation is StoryOnly. The contract clauses and consumed grove are panels. Any future tactical version may reuse Gather, Summon, Trail, Healing Elixer, and Paralysis Potion only through their ordinary real-card behavior.
- **Teaches:** Gather economy, Summon placement, Trail/token control, route planning, optional humanitarian objective, hard trade-off, memory scenes without false agency.
- **Win/fail/result:** One cylinder exits before pursuers arrive; bonus if pipe is shut and no worker is abandoned. Killing a pursuer or representing the contract as consensual fails. The party continues toward the grove.
- **Panels/art:** `bt/bt13/01_poison_pipe.png` — Foreman: “The spill is not waste. It is the price moved somewhere without a ledger.” `02_factory_moths.png` — Narrator: “Silver moths carry the work into sleep.” `03_contract.png` — Vespara: “You asked which grove could be spared. The contract asked which one would be spent.” `04_moonfruit.png` — Fizzlewick: “Memory is a route with the exits removed.”

### BT14 — Move the Boundary

- **Source/length:** Chapters 23-24, the toll road and the moving Seelie boundary; 11-14 minutes.
- **Setup:** Fizzlewick B4; Gearjaw B3-B4 as a two-square `large_footprint` scenario unit; Foreman C5; two Lumberjacks C3/C6; four boundary stakes E2/E4/E6/G4; Pavo/Nettle/Zippy D4-D6 `autonomous`; six refugees F2-F7; Rowan H4 guarding the gate; two Company exits A3/A6. Boundary shifts at end of rounds 2, 4, and 6.
- **Gold path/script:** Current implementation is StoryOnly. Gearjaw's footprint, Fizzlewick's measurement, Pavo's toll, Nettle's refusal, Zippy's bridge, and Rowan's choice are panels; there is no stake-push substitute action.
- **Teaches:** Large footprints, occupied-path blocking, push/collision damage, changing control zones, timed boundaries, neutral negotiation, extraction.
- **Win/fail/result:** Three measurements and Gearjaw exit; at least five refugees and Rowan survive. Killing/capturing the Seelie trio or moving Rowan against his choice fails. Boundary movement remains a Company-caused harm, not a neutral weather event.
- **Panels/art:** `bt/bt14/01_toll.png` — Pavo: “You may pay for the bridge. You may not purchase the people crossing it.” `02_stakes_move.png` — Nettle: “A border that walks is still being pushed by someone.” `03_rowan_stays.png` — Rowan: “Tell them I did not fall behind. I stayed where the road needed a person.” `04_measurements.png` — Fizzlewick: “Three corrections. Enough to make the next machine believe it knows the forest.”

### BT15 — The Monster Has Rules

- **Source/length:** Chapters 25-26: the Ashenfang confrontation, hidden tack, Fen's death, Sylvara, and the four loss clocks; 13-16 minutes.
- **Setup:** Ashenfang Hero B4 under temporary `perception_control`; Fizzlewick B6; three mark pylons D2/D4/D6; grass-woman E4 neutral; party Reed/Donella/Erevan/Birdie/Scooter/Maggie F2-F7; Fen G3, prisoner cage G5, Sylvara root-heart H4. Four clocks begin at 0/4: cage, engine, poison, root.
- **Gold path/script:** Ashenfang uses printed Entangling Lunge on a real Marshland Veteran two diagonal squares away. The Veteran survives one damage and receives Disable 2; two ordinary owner turns demonstrate the duration. Sylvara, the tack, Fen, and the grass-woman remain narrative panels.
- **Teaches:** Ashenfang's real card, diagonal range, attacking-move staging, and two-turn Disable timing. No target filter is claimed because the live action has none.
- **Win/fail/result:** Two pylons and the hidden tack are broken; grass-woman and Sylvara survive; party reaches root-heart. Fen's death is fixed. Attacking grass-woman/Sylvara or implying Ashenfang is Victor's willing ally fails the adaptation contract.
- **Panels/art:** `bt/bt15/01_ashfang.png` — Narrator: “The monster does not lunge at the nearest body. It follows the hurt hidden under its tack.” `02_grass_woman.png` — Reed: “A rule can be used against the hand that wrote it.” `03_fen.png` — Narrator: “Fen spends the only second the clock will not return.” `04_sylvara.png` — Fizzlewick: “This is not a battery. It is a person made to answer like one.”

### BT16 — The Clean Shot

- **Source/length:** Chapters 27-28: Gearjaw, Birdie's cable shot, the consent network, and Sylvara's restoration; 14-17 minutes.
- **Setup:** Fizzlewick A4; Grask B3; Gearjaw B5-C5 large footprint; Guardian C3; Victor D4; six cable anchors E2-E7; party and grove allies F2-G7; Sylvara H4; exit A6. Measurement completes after three powered anchors; consent responses are tracked independently.
- **Gold path/script:** Hold three anchors long enough for Gearjaw to measure the gate. Guardian screens while Grask uses long orthogonal attack and diagonal Capture against cable braces. Birdie autonomously fires the canonical clean shot, breaking a cable and suffering a permanent shoulder injury; Gearjaw then sacrifices its core so Scooter lives. The consent sequence records yes from Erevan, Reed, grass-woman, Pavo, Birdie, and Maggie, and no from Nettle, Scooter, and prisoners; “no” removes that unit from the network without penalty. Sylvara inserts the Root Key, Nettle cuts the remaining wire, and Fizzlewick escapes with partial measurements.
- **Teaches:** Large-unit protection, closest-footprint targeting, multi-anchor objectives, permanent consequences, consent as an explicit opt-in state, no-penalty refusal, loss-with-information.
- **Win/fail/result:** Fizzlewick exits with at least two measurement records; Sylvara is restored but not cured; every recorded refusal remains respected. Killing Birdie/Scooter/Nettle or auto-enrolling a refuser fails. Gearjaw is destroyed and cannot return in later Book One play.
- **Panels/art:** `bt/bt16/01_clean_shot.png` — Birdie: “There is one line that hurts the machine more than the people holding it.” `02_gearjaw.png` — Gearjaw: “CORE ROUTE ACCEPTED. SMALL BODY ROUTE DENIED.” `03_consent.png` — Sylvara: “Ask each root. A forest made from forced answers is only another engine.” `04_not_cured.png` — Narrator: “Restored is not the same word as cured.”

### BT17 — Natural Order

- **Source/length:** Chapter 29 plus the antagonist half of the epilogue; full-deck capstone, 16-20 minutes.
- **Setup:** Player builds the exact twenty-card Blackthorn starter deck and chooses Thaeron and/or Ashenfang within the hero-cost cap for a separate mastery preflight; story board then locks canon actors: Victor Hero B4, Grask B3, Mog C2 in custody, Fizzlewick C6, surviving Company units C3-C5, collapsing cages E2/E4/E6, prisoners F2-F7, Reed/Donella/Erevan/Birdie/Maggie/Pavo G2-G7, Baelstone gate H4, register H6. North-fragment and measurement tokens start on Fizzlewick. Lash is epilogue-only.
- **Gold path/script:** Demonstrate deck validation, concealed hero pre-placement, opening reveal, income/deploy cycle, one normal activation plus affordable plays, and discard/paid draw. During the battle, Grask's cage collapses and kills him; collective Baelstone action opens the gate. Victor surrenders, then attacks when the register burns, and Pavo kills him autonomously. The Company tactical objective is narrower: use control, Tax, Transform, hidden movement, Trail, Summon, healing/disable, and evacuation screens so Fizzlewick carries the north fragment and partial measurement off A6. Mog remains in custody.
- **Teaches:** Complete Blackthorn roster exam; deck/hero legality, pre-placement concealment, hand/economy cadence, control, every starter action family, hero-loss rule, simultaneous story and operational outcomes.
- **Win/fail/result:** Fizzlewick exits with both records; at least twelve prisoners and all named protagonists survive; Victor/Grask die, Mog remains, register burns, gate opens. Any ending that crowns Victor, kills Pavo, frees Mog without his choice, or prevents the Baelstone collective action fails. After S06, epilogue panels show Fizzlewick delivering measurements to Lash, the concordance glass, the cracking pearl, and the unresolved maxim “No one alone.”
- **Panels/art:** `bt/bt17/01_grask_cage.png` — Narrator: “The cage keeps the promise it was built to make.” `02_gate_opens.png` — Reed: “Not my blood. Our hands.” `03_victor_falls.png` — Pavo: “Natural order is the name you gave your appetite.” `04_fizzlewick_lash.png` — Lash: “A failed machine can still leave an accurate measurement.” `05_pearl_cracks.png` — Narrator: “Far away, the pearl answers with a fracture.”

## 10. Canonical play order and pacing gates

Shared nodes S01-S06 are required on a first playthrough and instantly replayable afterward. They contain panels and narrative records but no tactical win state. The runtime advertises **8 tactical + 22 StoryOnly nodes** for Mirewatch and **7 tactical + 16 StoryOnly nodes** for Blackthorn. A new player receives every legal guided action in tactical nodes; StoryOnly nodes never auto-resolve or simulate a game rule.

| Act | Novel span | Mirewatch sequence | Blackthorn sequence | Narrative/tutorial gate |
|---|---|---|---|---|
| Deferred prologue | Prologue | S02/S03/S06 | S02/S03/S06 | No cold open. Reveal the old thief/key, Lash, and pearl only after the gator opening and when each becomes relevant. |
| I: Cargo and occupation | Ch. 1-3 | MW01-MW04 | BT01-BT04 | Basic turns, geometry, deployment/economy; Reed's crest and the watcher/refugee mystery. |
| Interlude | Ch. 4-5 | S01 | S01 | Consent vocabulary, Maggie/Nibsy/Mangletooth, Birdie/Scooter/Vanya, Bluewater routes. |
| II: Evidence has a cost | Ch. 6-8 | MW05-MW06, S02 | BT05-BT06, S02 | Stealth, rescue, surveillance, Hollis, Erevan's confession, Rowan/Elliot. |
| III: The beautiful plan | Ch. 9-14 | MW07-MW11, S03 | BT07-BT09, S03 | Prison/mill, Thaeron trap, Vault, five crises, Juniper's death, Lash reveal. |
| IV: Who owns the work | Ch. 15-17 | MW12, S04, MW13 | BT10, S04 | Artifact theft, Remy consequences, medicine, vote/Society rules, investigation. |
| V: Public authority | Ch. 18-19 | MW14-MW15 | BT11-BT12 | Public lie, charter, Vanya surrender, town assault, Braun/Mog choices, fugitives. |
| VI: The Feyward road | Ch. 20-22 | MW16-MW17, S05 | BT13, S05 | Poison/factory/dream, Vesper, moonfruit memory, contract proof, chosen destination. |
| VII: Boundaries and rules | Ch. 23-28 | MW18-MW23 | BT14-BT16 | Pavo/Nettle/Zippy, refugees, Rowan stays, Ashenfang, loss clocks, clean shot, consent. |
| VIII: Agent, not heir | Ch. 29-30 + Epilogue | MW24, S06 | BT17, S06 | Gate, register, Victor/Grask, Fizzlewick escape, town governance, divergent roads, pearl. |

### First-play sequence strings

- **Mirewatch:** `MW01 → MW02 → MW03 → MW04 → S01 → MW05 → MW06 → S02 → MW07 → MW08 → MW09 → MW10 → MW11 → S03 → MW12 → S04 → MW13 → MW14 → MW15 → MW16 → MW17 → S05 → MW18 → MW19 → MW20 → MW21 → MW22 → MW23 → MW24 → S06`.
- **Blackthorn:** `BT01 → BT02 → BT03 → BT04 → S01 → BT05 → BT06 → S02 → BT07 → BT08 → BT09 → S03 → BT10 → S04 → BT11 → BT12 → BT13 → S05 → BT14 → BT15 → BT16 → BT17 → S06`.
- **Unlock rule:** Clearing a tactical mission unlocks only its next node; failing never removes codex entries already viewed. Completing either campaign unlocks cross-campaign chapter select. Spoiler labels hide the Blackthorn operational view until the equivalent Mirewatch act is cleared unless the player explicitly disables protection.

## 11. Complete game-rule curriculum

### 11.1 Catalog/content prerequisites

The engine supports more rules than the live 97-card schema-v9 catalog currently exposes. The strict tutorial gate is therefore: **a playable lesson exists only when an authoritative card exposes the mechanic and the ordinary engine can execute it.** There are no tutorial-only Rule Lab cards and no fake props standing in for cards. Unsupported mechanics remain StoryOnly context and award no mastery.

Before a “100% Rules Learned” badge can ship, content owners must publish an ordinary catalog card for each missing capability, synchronize the packaged snapshot, and add a legal replay plus rejection tests. Until then, the badge remains unavailable rather than presenting substitute gameplay:

| Deferred capability | Requirement before a playable lesson exists |
|---|---|
| Dig, Hop, Teleport, Tunnel | A real catalog card with the relevant action plus occupied-landing, hole, pivot, and path-block tests. |
| Effectful Spells and Enchantments | Real cards with nonzero authoritative effect, target, and power fields. |
| Growth, `canControl=false`, horizontal/vertical attacks, multi-target filters | Real cards whose live definitions expose each field, followed by deterministic normal-engine replays. |
| Foresight | Resolve the current mismatch: the live Birdie is a Hero while the existing rule counts owned non-Hero Foresight pieces. Do not teach it until a legal card can trigger it. |
| Card correctness | Repair requires a real Mechanical filter; Fizzlewick requires a valid live Summon target; inert Juniper Sprint must be fixed or remain untaught. |

A missing authoritative definition is a build/test failure. A sample-library fallback, silent substitution, generic stats, or tutorial-only power invalidates the lesson.

### 11.2 Rule coverage matrix

“Guided” is the first forced, explained use. “Exam” requires the player to apply it without a highlighted destination. “Deferred” means no playable lesson or mastery credit exists until an authoritative card supports the rule.

| Rule or mechanic | First guided use | Independent exam | Required assertion |
|---|---|---|---|
| 8×8 board; coordinates; two-by-four home strip | MW01 camera/inspection; BT01 | MW03; BT03 | Orientation follows the player's view and all authored coordinates validate. |
| One normal piece activation per turn; explicit End Turn | MW01 | MW06; BT03 | An action never silently passes the opponent's turn; card plays may surround the one activation. |
| Legal-action highlights and cancel/inspect | MW01 | Every mission from MW02/BT02 | Cancel changes no state or clock. |
| Orthogonal, diagonal, omni movement | MW01 (omni/diagonal), MW02 (orthogonal/diagonal); BT01/BT02 | MW06; BT12 | Geometry comes from live card data, never tutorial prose overrides. |
| Horizontal, vertical, and knight-jump patterns | Tracker's real Frogback Leap covers knight jump in MW06; horizontal/vertical deferred | Tracker regression | Jump ignores intervening occupancy only as specified; no substitute card teaches the deferred patterns. |
| Slide movement and blockers | MW01 | MW05; BT02 | Ordinary Slide cannot cross occupied cells. |
| Attacking movement: stop short if defender survives, occupy if destroyed | MW02 Smuggler/Spearman | MW06; BT12 | Both outcomes have deterministic checks and a preview. |
| Ranged action, blockers, and closest footprint | MW01 Donella; BT02 Sharpshooter | MW22; BT16 | Current engine blocks straight/diagonal paths regardless of the serialized LOS flag; copy must describe actual behavior. |
| Capture and staging | BT05 Grask against a real Veteran | BT17 against a real Bog Spearman | Capture targets an enemy unit; a survivor leaves the attacker staged short, while destruction lets it occupy the target square. |
| Hop/pivot, Teleport, Tunnel, Dig/holes | Deferred | Deferred | Story panels may mention travel or tunnels, but no gameplay or mastery is awarded without a real card. |
| Pass-through and large footprints | MW06 Tracker; MW15 Mangletooth; BT04 Ambusher | MW21; BT14/BT16 | Anchor, path, destination, and closest-footprint calculations agree. |
| Health, damage, destruction, healing to maximum | MW01/BT01 | MW07; BT07 | No overheal; friendly/enemy targeting is explicit. |
| Positive damage makes a surviving piece miss its next activation | MW01 gator hit | MW06; BT07 | Status icon and exact remaining owner-turn count are shown. |
| Disable and duration | BT01 Alchemist; MW20 via Ashenfang encounter | BT15 | Duration decrements on the disabled piece's owner turns. |
| Cooldown and action reavailability | MW01 Reed/Donella; BT02 transformed gun | MW22 | UI says which owner turn restores the action. |
| Push and blocked-push collision damage | MW05 Erevan | MW13; BT14 | Preview distinguishes legal displacement from collision damage and reveal. |
| Repeat: same action again or pass | MW02 Smuggler; MW06 Scooter | MW09 Vanya; MW21 | No different action may replace the repeat; Pass is visibly available. |
| Relentless extra activation after a real kill | BT17 Victor | BT17 second Debt Collector | It triggers only on destruction and locks the next action to that piece or pass. |
| Command adjacent ready unit | BT03 Thaeron | BT08/BT11 | Command is separate from, and does not consume, the normal activation as implemented. |
| Bodyguard damage split | Deferred active lesson | Deferred | Use real Rowan Leafbound if this returns; never fabricate a witness or damage source. |
| Temporary unit Control and expiry | Deferred active lesson | Deferred | Use a real catalog control action; Heroes cannot be controlled and expiry is counted on controller turns. |
| Target filters and multi-target resolution | Deferred active lesson | Deferred | A real card must expose the filter/multi-target fields; story “marked targets” do not count. |
| Dematerialize/hidden, loss of control, pass-through | MW05 Erevan; BT04 Ambusher | MW13; BT10 | Hidden units neither control nor are normally targetable. |
| Reveal by adjacency/end turn; collision reveal/stun | MW13 Tracker; BT10 collision lesson | MW21/BT16 | Reveal itself does not stun; only hidden collision does. |
| Transform and state-specific actions | BT02 Sharpshooter | BT07/BT12 | Raised/lowered action sets and state persistence come from the card. |
| Summon, front square, and blocked failure | BT03 Foreman | BT12/BT17 | Failed placement consumes neither power nor turn; summoned card is a valid catalog token/card. |
| Trail and origin summon | BT03 Grove Sister | BT13/BT17 | Origin must be legal for the configured Sapling; created piece's control is explicit. |
| Rebirth | MW06 Tracker, triggered by a real Bull Gator Bite | Dedicated real-card regression | Correct replacement card ID, health, position, and ownership persist exactly as the engine defines. |
| Healing aura at owner's end turn | MW03 Joni | MW07/MW15 | Audit current behavior: it heals every adjacent piece, including enemies; either keep and teach this or fix before copy lock. |
| Growth and `canControl=false` | Deferred | Deferred | No playable lesson or mastery until an authoritative card exposes both behavior and state. |
| Control: occupied visible piece, eight-neighbor majority, persistent ties | MW03 | MW15/BT17 | A tie preserves the prior controller; empty squares do not reset to neutral. |
| Start-turn income: controlled squares + square enchant + Gather − drain | MW03; BT03 | MW15/BT17 | Event log shows the exact ordered arithmetic. |
| Tax transfer and insufficient-resource clamp | MW03 Informant; BT02 Collector | MW15/BT17 | Transfer never makes victim resources negative. |
| Deployment cost, empty controlled footprint, trait gate, arrival exhaustion | MW03; BT03 | Both capstones | All living Heroes collectively supply traits; Spells/Enchantments bypass the unit trait gate. |
| Spells: resource, heal, damage and targets | Deferred | Deferred | Real card effect/target/power fields must be nonzero and server-authoritative. |
| Enchantments: player drain, square income, piece damage | Deferred | Deferred | A real card must exist before persistence, owner, target, and stacking become tutorial content. |
| Starting/max hand four; no automatic draw | Deferred campaign lesson | Existing engine tests | Add only through ordinary deck/hand play, never a preflight minigame. |
| Paid draw and Foresight | Paid draw deferred; Foresight blocked by catalog/rule mismatch | Deferred | Do not claim Birdie's Hero keyword triggers the non-Hero-only rule. |
| Discard and deck legality | Deferred campaign lesson | Existing engine/deck tests | Add only when a normal match-flow lesson supports it. |
| Hero placement and defeat | Deferred campaign lesson | Existing engine tests | Use ordinary placement and victory behavior, never a scripted substitute. |
| Match clock, per-turn clock, first-30-action +10-second bonus | Optional timed rehearsal after MW15 | Optional Tactician BT17 | Default Tutorial pauses during coach/panels; 15m/2m→1m→30s values are read from build constants. |

### 11.3 Starter-deck completion gate

The roster tables in Section 6 are the authoritative card-to-mission matrix. Automated completion additionally enforces:

1. Every distinct starter title is selected and inspected once, makes one legal move/action or explicitly has no attack, and appears in one unguided exam.
2. Every starter Hero is used as its true catalog type. Reed/Braun remain Units; Vanya/Joni/Birdie and Thaeron/Ashenfang remain Heroes.
3. Quantity variants are exercised where formation matters: four Spearmen, four Veterans, three Smugglers, two Informants, two Trackers; four Lumberjacks, three Grove Sisters, two each of Collectors/Alchemists/Ambushers/Sharpshooters.
4. A mission cannot award a card's mastery stamp if its definition hash differs from the validated campaign manifest. It may launch in “Story only” with a warning, but progression records rules as incomplete.
5. Telos in MW01 uses his printed **Travel** from B4 to A5. That move clears Reed's otherwise blocked column-B **Bow** shot; cargo is never a prompt, target, or game-state object.

## 12. Narrative and character coverage

### 12.1 Chapter-by-chapter canon matrix

No prose instruction embedded in the novel is treated as a development instruction; the document is narrative canon only. The “must land” column is the adaptation test a story reviewer signs off.

| Novel source | Mirewatch delivery | Blackthorn/shared delivery | Must land for a first-time player |
|---|---|---|---|
| Prologue | S02/S03/S06 (deferred) | S02/S03/S06 (deferred) | The campaign does not open in Lash's theatre. The old thief/key, Lash, and pearl/maxim are introduced later at their relevant reveals. |
| Ch. 1 | MW01-MW02 | BT01-BT02 | Gators were conditioned; Reed moves away; Donella joins the fight; Telos transports the party/cage; Pedros/plants/horse/captive woman; Gilded Hold; weighted customs substitution; crest recorded. |
| Ch. 2 | MW03 | BT03 | Blackthorn ownership reaches boats, licenses, food, wages, and hospitality; Avery's absence/context, Mudfen seizure, Joni/Infamous Mouse, Reed's crest can help and expose. |
| Ch. 3 | MW04 | BT04 | Juniper's prepared question helps arrest Lio; Pellan/Seli/refugees; watcher chooses children over itself; black root; Remy/Baalzepub route; broken-horn observation remains a clue, not an identity proof. |
| Ch. 4 | S01 | S01 dossier | Maggie, Nibsy, and Mangletooth make consent/repair explicit; the Hold tests the house; Birdie/Scooter/Vanya enter with their own authority. |
| Ch. 5 | S01 | S01 dossier | Bluewater Below is a clinic/laundry/kitchen/network, not merely a hideout; 37 keys and Thaeron's puzzle box expose unequal information and bait. |
| Ch. 6 | MW05 | BT05 | Office heist is rehearsed with names/abort; Hara stays by choice; black ledger is false, ordinary freight chits are real; guardian/tracking dye and Victor/Grask/Mog/Fizzlewick appear. |
| Ch. 7 | MW06 | BT06 | Wagon rescue succeeds; recorder learns rescue patterns; retaliation is systemic—ferry, medicine, wages, tools, food, Torren, Missus Vale; community rather than one hero bears the response. |
| Ch. 8 | S02 | S02 | Hollis's information and fear are both checked; Erevan confesses fishbone key/mentor/pearl; Vanya rejects inherited forgiveness; Rowan confronts Elliot's death and limits Birdie's force authority. |
| Ch. 9 | MW07 | BT07 | Twenty debtors, Tommy's informed injury, Garrett's protection of Little Fen/death, 17-3 prisoner vote, mill flood/fire, ledger and stored value sacrificed. |
| Ch. 10 | MW08 | BT08 | Thaeron built both Reed's care and his need; Reed refuses unsealed terms; flare reveals Bluewater; Missus Vale dies; Bluewater Below is destroyed; Vanya withdraws route authority. |
| Ch. 11 | MW09 | BT09 setup | Remy's basement, Sedge and under-skin tokens, three consent plates/survivors, evidence burns, Remy captured; Shrouded Vault plan assigns roles and abort limits. |
| Ch. 12 | MW10 | BT09 | Auction converts people into euphemized lots; Nima works her own lock; pearl-veiled buyer's schedule; false raid; Mirror, paired wound, Root Key, custody and informed vote. |
| Ch. 13 | MW11 | BT09 | Five simultaneous crises; Reed permanently loses right-hand function; Birdie chooses people over register; Juniper dies after handing Mae the flare; 16 of 19 escape; Nima/Cal/Dessa missing. |
| Ch. 14 | S03 | S03 | Funeral distributes Juniper's work; Erevan's unauthorized Remy release; forensic scraps expose Lash as architect; Thaeron paid Remy but did not understand the lower room's whole design. |
| Ch. 15 | MW12 | BT10 | Maggie withheld knowledge; Nibsy demands that living people be asked; Mangletooth's no holds; Briar's care and theft coexist; Hold/Key/Mirror are separately tracked; new pursuit rules follow. |
| Ch. 16 | S04 | S04 | Full fever-bark course goes to Minnow; Rowan owns his error; 43-person vote and recorded dissent; medicine is not allocated by usefulness. |
| Ch. 17 | MW13 | BT10 | Society rules constrain officers; Mog and Braun are interviewed without being flattened into allies; Gearjaw's learned relationship and blue trace point to Feyward/Fizzlewick. |
| Ch. 18 | MW14 | BT11 | Thaeron's public lie is built from selective truths and material care; Reed's alias changes authority; charter retrieved; Vanya surrenders; murder/contract proof is published rather than privately traded. |
| Ch. 19 | MW15 | BT12 | Company voids charter and assaults town; one audited exception gets children out; Braun refuses and dies; Mog chooses terms/custody; register burns; Victor/Grask/Fizzlewick withdraw; party becomes fugitives. |
| Ch. 20 | MW16 | BT13 | Poisoned bayou/Half-Ear brood; stopping pipe costs time; gossiping trees/white stair; Briar's promise is narrow; names and relationships open closure; Mangletooth stays. |
| Ch. 21 | MW17 | BT13 | Factory in sky, modular rails, moth cargo; dream offers Erevan mentor/kitchen; he calls Donella; Vesper's body/history and Elliot's tune lead to chosen temporary company. |
| Ch. 22 | S05 | BT13/S05 | Moonfruit exacts Reed's last sensory memory of his mother; Donella witnesses Thaeron/Vespara murder-and-boundary contract; Lash waits behind the bargain; party chooses active grove harm over Mirror proof. |
| Ch. 23 | MW18 | BT14 lead-in | Counterfeit toll, detained Fey, limited captain mercy; Pavo/Nettle/Zippy negotiate separately; bridge is sustained by reclaimed name and cooperation, not ownership. |
| Ch. 24 | MW19 | BT14 | Moving boundary and frozen refugees; captain finally uses emergency passage; revenge will not restore Reed's name; Rowan chooses the refugee road and transfers authority/items. |
| Ch. 25 | MW20 | BT15 | Grove/Aelon factory; Ashenfang follows marks and hidden tack, spares unmarked woman, is observed rather than slain; Fen knowingly tests one case and dies; “Sylvara” is wounded into a rule. |
| Ch. 26 | MW21 | BT15 | Four loss clocks; Guardians adapt; Scooter/Gearjaw relationship matters; Mog is stabbed protecting grass-woman and stays in custody; engine removes witness/kin/shelter/song/local memory; Donella chooses an answering place, not a cure. |
| Ch. 27 | MW22 | BT16 | Grask hurts Scooter; Gearjaw rejects its order and sacrifices its core; Victor presents an easy throat shot; Birdie chooses cable and permanently loses shoulder function. |
| Ch. 28 | MW23 | BT16 | Consent network asks each person; yes and no lists remain distinct; refusals carry no penalty; Sylvara inserts Key, Nettle cuts wire, Fizzlewick escapes; Sylvara is restored, not cured. |
| Ch. 29 | MW24 | BT17 | Grask dies in cage collapse; collective Baelstone action opens gate; Victor surrenders, attacks after register burns, and Pavo kills him; victory comes from agency, not Reed inheriting rule. |
| Ch. 30 | S06 | S06 | Three Mirewatch weeks of governance precede the return; injuries/deaths remain; ferries, burials, wages, shortages matter; Reed's granary-only authority passes by three; roads diverge after fourteen-day promise. |
| Epilogue | S06 | BT17/S06 | Fizzlewick reaches Lash with measurements despite Mirewatch's win; concordance glass/northern account remain; old pearl cracks; “No one alone” remains promise and threat. |

### 12.2 Character development, drives, and protected choices

| Character/group | Drive and development to preserve | Required beats | Adaptation guardrail |
|---|---|---|---|
| Reed Baelstone | Wants to use inherited access without becoming the inheritance; repeatedly learns that public, limited authority is safer than private rescue. | Crest exposure, Thaeron temptations, guarantor cap, permanent hand loss, alias disclosure, published offer, lost mother-memory, revenge answer, collective gate, granary-only vote. | Do not make him sole savior, monarch, or universal commander; Book One ends with a narrow voted job, not a crown. |
| Donella | Triage, truthful accounting, boundaries, and witness; cares without claiming ownership of another's choice. | Joins gator fight, names/abort in plans, healing, paired wound, truthful 16/19 accounting, pursuit rules, contract witness, answering-place choice. | Never invent a protection aura or let healing erase permanent costs. |
| Erevan | Secrecy and mentor grief make him effective and unsafe alone; development is asking for corroboration/company without surrendering craft. | Black root, hidden office route, mentor/key confession, Remy release breach, Briar pursuit rules, blue trace, dream/calls Donella, consent yes. | His unauthorized acts must have consequences; the omitted sourcebook-only prologue must not expose his protected name. |
| Telos | Practical carrier and contract keeper whose freight knowledge supports, rather than replaces, fighters. | Skiff/cage, line-clearing Travel, customs weights, medicine/contract books in S04. | His live card only moves. Travel has a real positional purpose in MW01; all cargo work remains story-panel narration. |
| Joni Pumpernickel | Hospitality and governance are material systems—food, rooms, records, votes—not sentimental backdrop. | Mouse evacuation, aura, service lane, Mirror custody, Society rules, charter/work witness, town operation. | Company can suspend a license, not retroactively own Joni's building or guests. |
| Vanya Bluewater | Keeper of routes/keys forced into earlier compromises; rejects inherited forgiveness and chooses surrender on bounded terms. | Bluewater network, fishbone custody, role/custody, repeat action, route authority removal, surrender/dead key, return governance. | She is a live Hero, not a unit; surrender is hers, never a player sacrifice command. |
| Birdie | Forceful competence becomes accountable force; clean-shot patience costs her body but does not make injury her identity. | Wagon leadership, authority renegotiation, south role, people over register, exception, cable shot, consent yes, return road. | Never offer Ashenfang kill as a rewarded alternate; shoulder remains injured afterward. |
| Scooter | Playful motion becomes relationship and explicit refusal; survival is not a lesser heroic role. | Wagon dash, recorder pattern, Gearjaw rhythm/trace, factory/road, Gearjaw rescue, consent no. | Do not enroll Scooter in the network after refusal or reduce Gearjaw's choice to ownership. |
| Juniper Flash | Speed and responsibility: prepared-question guilt drives service; she hands work onward rather than becoming a martyr collectible. | Lio/refugees, ranged rescue, tower/abort role, Mae/Tavi/Corra, flare handoff/death, funeral distribution. | Fix/remove inert Sprint before mentioning it; no resurrection or “perfect sacrifice” medal. |
| Rowan | Hero-making rhetoric gives way to accountability, bounded authority, and choosing ordinary people over the legendary destination. | Elliot news, Birdie authority, debtor rescue, Minnow apology, exception, gate answer, remains with refugees. | “Stays behind” is an affirmative choice, not abandonment or failure. |
| Maggie, Nibsy, Mangletooth | Repair must ask; a living house can refuse; Maggie's protective withholding can still be wrong. | Vows/Hold test, Bluewater support, bridge, withheld artifact knowledge, Nibsy confrontation, Mangletooth no, stair anchor refusal/stay. | Never treat Mangletooth as equipment or Nibsy as a repair menu. |
| Thaeron | Care for Reed and engineered coercion are simultaneously real; wants control through necessity, record, and selective truth. | Puzzle box, north-lock offers, Remy payment, public stew/lie, murder/contract proof, private bargain. | Do not absolve him because care was real or claim he authored every part of Lash's Vault plan. |
| Victor | Treats ownership as natural order and people/actions as measurements; loses the register and dies after breaking surrender. | Office performance, retaliation, charter assault, grove mechanism, clean-shot bait, gate/register/final attack. | Blackthorn missions may achieve information/extraction goals, never validate his ideology as canon victory. |
| Grask | Violence, containment, and loyalty define his method; Mog is the relationship he assumes he can command. | Office/wagon, Capture, debtor pattern, assault, Mog offer/stabbing, Scooter strike, cage-collapse death. | Do not make Mog's choices extensions of Grask or turn Grask's final death into redemption. |
| Mog | Durable enforcer develops through bounded, self-authored choices: lawful terms, custody, and protection of another prisoner. | Office screen, interviews, terms at town, custody, refuses escape shape, protects grass-woman/is stabbed. | Player never commands refusal, surrender, or interposition; Mog survives Book One in custody. |
| Braun | Pension/order motive does not prevent a final moral boundary. | Customs/lanes, interviews, debtor withdrawal, child-target refusal and death. | Do not foreshorten his refusal into secret heroism or permit a canon rescue. |
| Fizzlewick and Gearjaw | Fizzlewick values measurement/continuity; Gearjaw's learned play becomes a relationship that exceeds its order. | Office machine, recorder/blue trace, factory/boundary measures, grove clocks, Gearjaw-Scooter rhythm, core sacrifice, Fizzlewick escape/epilogue. | Gearjaw stays destroyed; Fizzlewick's escape is a real cost of victory, not a tutorial mistake. |
| Remy, Hara, Hollis | Three different compromised intermediaries: guilty broker, worker choosing risk, and frightened informer. | Remy capture/roles/release; Hara hatch/mark; Hollis corroboration/escape drive. | Never merge their motives, promise clean redemption, or resolve their final statuses beyond the novel. |
| Briar | Care, refusal-respect, and theft coexist; joins roads only through narrowly worded promises. | Mirror/Key theft, Mangletooth no, reflection route, poison road promise, unresolved custody. | Do not turn her into loyal party property or an uncomplicated traitor. |
| Pavo, Nettle, Zippy | Separate agents with bounded alliance, reclaimed names, tactical skill, and different answers. | Toll/bridge, individual terms, refugee gate, role recommitment, consent yes/no, wire cut, Pavo kills Victor. | Never use one group-consent flag; Zippy is asked at each crossing; Nettle's no remains no. |
| Vesper, Elliot, Sylvara, Aelon, Vespara | Memory, body, art, grove, and contract connect generations without collapsing them into one magical object. | Elliot portfolio/tune, Vesper's chosen company, moonfruit/contract, Aelon root, Sylvara answering/Key/restored-not-cured. | Sylvara is a person; Root Key is not a cure button; Elliot's work is not ownership; Vespara's contract is coercive. |
| Ashenfang, watcher, grass-woman | The “monster” is a marked/conditioned chooser whose behavior can be studied; vulnerable Fey retain agency. | Watcher shields children; Ashenfang mark/tack rule; grass-woman spared/freed/asked. | The book does not prove watcher and Ashenfang are the same body. Never state it as fact or cast Ashenfang as Victor's willing starter-deck mascot. |
| Nima, Cal, Dessa, Fen, Garrett, Missus Vale | Named losses/missing people make victory accounting honest. | Nima's self-rescue, three missing; Fen's one-case warning/death; Garrett/Little Fen; Missus Vale holds door. | Fixed outcomes are disclosed as fail-forward, never secretly preventable or converted into collectible rewards. |
| Mirewatch/Mudfens/refugees | Community is a decision-making character with dissent, labor, food, burial, transport, and limits. | Seizure, wagon, 17-3 and 43-person votes, Society rules, charter work, child exception, refugee lifting, three weeks governing, granary vote. | Named protagonist action must not erase votes, dissent, or civilian labor. |

### 12.3 Diversions and texture that must not be cut for pace

These are StoryOnly panels, optional medals tied to real actions, or codex records rather than additional fights:

- Pedros's memorial, carnivorous plants, tethered horse, and the anonymous winged captive establish that a rescue can uncover another person's exit rather than absorb them into the party (MW02).
- The fish/fungus weight substitution at customs makes logistics—not combat—the first successful deception (MW02/BT02).
- Infamous Mouse's bread, beds, suspended license, and guests; Bluewater's laundry, clinic, kitchen, keys, and household relay keep infrastructure visible (MW03/S01).
- The recorder's sleeve pin, rear opening, bridge raise, wheel sabotage, and net/haul patterns explain why a successful rescue creates later danger (MW06/BT06).
- Tommy's hand, Little Fen's protection, prisoner minutes, the lost mill/ledger/wages/supplies, and carrying Garrett's body prevent “twenty rescued” from becoming a clean scoreboard (MW07/BT07).
- Mae, Tavi, Corra, Sedge, Nima, Cal, and Dessa receive names and codex records; they are not anonymous objective counters (MW09-MW11).
- Minnow's fever-bark, Mara's minutes, dissent, burial work, food, ferries, and wages recur in later debriefs (S04, MW15, S06).
- Half-Ear's brood, silver moth sacks, false dream kitchen, Seelie emergency passage, and the refugees' own lifting rhythm preserve the road's moral detours (MW16-MW19).

### 12.4 Unresolved Book One ledger

The campaign may add observations but must not invent closure for these threads: Hara's fate after the marked office; Hollis's escape; Lio's imprisonment and Torren's disappearance; Remy's location after Erevan's release; Nima, Cal, and Dessa after the Vault; the anonymous winged woman; Briar and the separate custody of Hold/Root Key/Mirror; whether the broken-horn watcher and Ashenfang share a body; Thaeron's post-conflict course; Mog's future in Society custody; Vesper's road after the fourteen-day promise; Fizzlewick, Lash, Caltheriel, the concordance glass, northern account, southern shadow war, and the cracking pearl.

### 12.5 Object and mystery continuity ledger

| Object/clue | First appearance | Book One state to carry forward |
|---|---|---|
| Gilded Hold | MW02 | Autonomous living vessel/house under Maggie's care; not interchangeable with Root Key. |
| Fishbone key | Sourcebook prologue/S02 | Old thief/Erevan/Vanya line; separate from all Baelstone keys. |
| Pearl/three-knotted device | Sourcebook prologue/S06 | Lash/old thief mystery; pearl cracks in S06, cause unresolved. |
| Black root/broken horn | MW04 | Watcher/refugee clue; supports questions, not Ashenfang identity proof. |
| Mirror | MW10 | Evidence/artifact with its own custody trail; Briar's theft does not make it Root Key. |
| Root Key | MW10-MW23 | Carried under bounded promises; Sylvara chooses insertion; inaccessible after correction. |
| Baelstone crest, nine-star gate, register | MW03/MW11/MW24 | Blood/title grants access but collective action and public destruction defeat inherited ownership. |
| Silver-tree marks, processed-Feywood tack, blue trace | MW13/MW20 | Related Company control/measurement technologies with separate functions; never call all three “the curse.” |
| Concordance glass/north fragment | BT17/S06 | Measurement survives to Lash and opens sequel threat; not destroyed by Mirewatch's victory. |

## 13. Story-popup and image production specification

Every `Panels/art` entry above is a required asset-manifest row, not a suggestion. The build step should extract those IDs into an asset report and fail if an image, localized caption, speaker, alt description, or spoiler flag is absent.

- **Panel rhythm:** opening place/drive, mid-mission change or cost, closing accounting. Four or five panels are used only when a mission has multiple irreversible beats. Never interrupt between target selection and confirmation.
- **Copy budget:** speaker label at most 32 characters; spoken line target 8-24 words; narrator line target 12-32 words; optional “More” paragraph at most 65 words. The story log retains the full sequence and can replay it with current objective context.
- **Image frame:** author at 16:9, keep faces/hands and story object inside a central 70% safe area, provide 1920×1080 master plus generated 1280×720/960×540 variants. Story text is rendered by the client, not baked into art.
- **Continuity:** a visual bible locks injuries and possessions by node: Reed's right hand after MW11; Birdie's shoulder after MW22; Gearjaw active/inert/destroyed; fishbone key, Hold, Mirror, Root Key, marks, tack, and pearl all have distinct silhouettes/colors. Do not show Juniper, Garrett, Missus Vale, Fen, Braun, Grask, Victor, or Gearjaw alive after their fixed endpoint except in an explicitly labeled memory.
- **Agency composition:** autonomous choices show the chooser acting in foreground; Reed/the player cannot occupy the heroic center of someone else's refusal, surrender, sacrifice, vote, or kill.
- **Spoiler safety:** selector thumbnails show locations/objects, never later deaths or Lash's face/name before S03. Blackthorn dossier art obeys the cross-campaign spoiler gate.
- **Accessibility:** every panel gets literal alt text plus a separate mood note; critical clues are named in copy and represented by shape/icon, not color alone. Auto-advance defaults off; narration pauses every gameplay clock.
- **Review contact sheet:** generate one chronological contact sheet per campaign and one object-continuity strip. Story director signs those before individual beauty polish so visual contradictions are caught cheaply.

## 14. Practical implementation program

### 14.1 Foundation work that precedes level authoring

1. Replace the two compile-time `std::array<StoryMission, 8>` catalogs and numeric switches with versioned data loaded by stable campaign/mission ID.
2. Add scenario-state seeding for resources, hands/decks, health, statuses, hidden/state/transform values, holes, enchantments, control, large footprints, autonomous actors, objective meters, clocks, and phase checkpoints.
3. Add trigger/action/choice/objective types from Section 4, including canonical fail-forward events and protected/autonomous-unit policies.
4. Package a schema-v9 card manifest with stable IDs and definition hashes. Validate every coordinate/action against it in CI; never balance against `ui_capture::sampleCardLibrary()` or silently replace a missing title.
5. Fix catalog inconsistencies and publish ordinary cards for deferred capabilities. Until then, affected chapters remain StoryOnly and cannot award rule mastery; never create a tutorial-only substitute card.
6. Make the selector data-driven and scrollable/paged beyond eight missions; generate prerequisites, campaign percentage, card mastery, rule mastery, chapter labels, difficulty, medals, and spoiler protection from metadata.
7. Add special AI controllers only when they submit ordinary legal actions for authoritative cards. Narrative witnesses remain panel-only; objective code may never grant an action a filter absent from its live card.

### 14.2 First implementation tranche: complete roster and engine coverage

This is the smallest safe parallel tranche under the authored mission boundaries. It is not the first-play order; it is the production order that yields a complete starter-card/rule test bed before all connective story missions are built.

#### Wave A — every distinct starter card receives a legal spotlight

| Mission ID/title | Coverage reason |
|---|---|
| **MW01 — River Teeth** | Reed, Donella, Erevan, Telos; base turn/combat; required three-gator dramatization and Reed's two squares. |
| **MW02 — The Gilded Hold** | Resistance Smuggler's real Swashbuckle Blade and repeat lock; captive/logistics beats remain panels. |
| **MW03 — The Town Under the Company** | Joni, Informant, Bog Spearman; traits/deployment/control/resources/Tax/aura. |
| **MW04 — What the Watcher Protects** | Juniper's real Spark at range two and Erevan's adjacent Shadow Blade; watcher choice remains a panel. |
| **MW05 — The Office Everyone Is Watching** | Erevan's full hidden/pass-through/push kit without invented powers or evidence clicks. |
| **MW06 — The Cost of Being Seen** | Birdie, Scooter, Veteran, Smuggler, Spearman, Tracker; jump, cooldown, repeat, and Rebirth from a real gator attack. |
| **MW10 — A Beautiful Plan** | Vanya as the real H2 Hero and her required repeated diagonal Blade Dance. |
| **BT01 — Harness the Hunger** | Alchemist and Lumberjack; Healing Elixer, Paralysis Potion, movement, and basic attack. |
| **BT02 — The Customs Bell** | Braun, Sharpshooter, Collector; orthogonal lanes, Transform, range/blockers, Tax. |
| **BT03 — Sanctuary Debt** | Thaeron, Foreman, Grove Sister plus repeated Collector/Lumberjack/Alchemist; Command/Summon/Trail/Gather/deployment. |
| **BT04 — Terms and Conditions** | Ambusher's complete hidden/pass-through/collision behavior. |
| **BT05 — The Freight Office** | Mog and Grask against real units; damage survival, long Charge, Capture, and staging. |
| **BT15 — The Monster Has Rules** | Ashenfang's actual Hero card, Entangling Lunge, diagonal range, and two-turn Disable. |

#### Wave B — real-card combination exams and deferred rules

| Mission ID/title | Current responsibility |
|---|---|
| **MW08, MW12, MW14, MW17, MW21** | StoryOnly until ordinary authoritative cards expose the previously proposed mechanics. These nodes advance the novel and award no rule mastery. |
| **MW15 — Title Follows Burden** | Mirewatch real-card exam: deployment, repeat, ranged attacks, and directional attacking movement. |
| **BT17 — Natural Order** | Blackthorn/supporting real-card exam: Relentless, Command, Summon, Capture, Trail, and friendly healing. |

**Tranche exit:** all 25 distinct starter titles receive a legal guided use; every active script replays deterministically through normal engine commands; the strict validator rejects fabricated card prefixes and narrative-only mastery. Deferred rules stay visibly incomplete until a real server-authoritative definition supports them.

### 14.3 Mission-owner work packets and integration

The requested one-owner-per-level model should use one isolated mission-data file per ID. Each level owner receives only the relevant novel chapter extract as canon, the global adaptation contract, schema, live manifest, adjoining mission handoff flags, and this mission's specification. Each returns:

1. scenario data with exact stable card IDs, board coordinates, controller type, starting state, objectives, clocks, triggers, checkpoints, win/fail/fail-forward paths, and difficulty deltas;
2. panel manifest with final copy, art brief, alt text, speaker, trigger, and spoiler tag;
3. a deterministic gold-path replay plus at least two legal deviation replays;
4. rule/card coverage assertions and a canon checklist naming every fixed outcome and protected choice;
5. a no-context play note: what the player knows before the mission and what they must know after it.

The story director alone owns shared chronology flags, object custody, injuries/deaths, unresolved-thread ledger, chapter copy, and integration ordering. Level owners may propose changes but cannot independently rewrite shared canon or another mission's state. Integration is by mission ID, never by array position.

### 14.4 Optional post-release expansion waves

- **Wave C, Mirewatch connective story:** MW07, MW09, MW11, MW13, MW16, MW18-MW20, MW22-MW24 and S01-S06.
- **Wave D, Blackthorn connective story:** BT06-BT14, BT16 and the Blackthorn variants/dossiers for S01-S06.
- **Wave E, balance/accessibility/localization:** all missions on three difficulties, controller/input variants, narration/alt text, text expansion at +35%, and low-motion/color-independent review.
- **Wave F, blind critique and polish:** Section 16 protocol, revision, new blind panel, final acceptance.

## 15. Save migration and compatibility

- New saves store `campaign_id`, stable `mission_id`, `campaign_schema_version`, phase/checkpoint, canonical flags, codex discoveries, card-definition hashes, rules seen/mastered, medals, difficulty, and accessibility settings.
- Retired numeric saves contain only a position in the old eight-stage tutorial, whose mission identities do not map safely to the expanded story. The runtime leaves that file untouched until a new entry is completed, starts the revised path at Entry 1, and writes stable V2 mission IDs thereafter; it never guesses mastery or skips River Teeth.
- On first launch after migration, offer **Begin expanded story** or **Resume near my former chapter**. The latter unlocks nodes through the nearest chapter tag represented by the old index and grants a visible Legacy badge, but every new card/rule stamp remains unmastered until actually completed.
- Never subtract old rewards/achievements. A retired mission remains available in a read-only **Legacy Eight** archive if its assets are retained; it cannot feed new canon flags.
- Inserting/reordering nodes changes prerequisite data, not save meaning. Missing future content yields a locked explanatory node rather than falling through a numeric switch.
- If the authoritative card definition hash changes, retain story/codex completion, invalidate only the affected mechanical mastery stamp, show the changed card text, and offer its lesson directly.

## 16. Blind-critic protocol

“Blind” means the critic receives a retail-like build, a fresh save, their audience persona, and the ordinary store-page premise—**not** this blueprint, novel summary, intended solution, rules audit, or another critic's report. A critic who has reviewed one revision is no longer blind for the next; recruit/spawn a fresh panel after material changes.

### 16.1 Independent audience seats

Use at least these ten critics, with no cross-talk until reports are locked:

1. young-adult man, story-curious and tactics-inexperienced;
2. young-adult woman, character/narrative-first;
3. middle-aged man, moderate board/strategy familiarity;
4. middle-aged woman, narrative/puzzle familiarity but low card-game familiarity;
5. older man, slower input/reading pace and larger-text needs;
6. older woman, story-first and unfamiliar with tactics terminology;
7. hardcore tactics/card gamer who will seek exploits and degenerate solutions;
8. casual gamer who normally skips dense tutorials;
9. accessibility critic using keyboard/controller, narration, low motion, and color-independent cues;
10. novel-first critic who has read Book One but has never played the game (used for canon/pacing, kept separate from the nine truly story-blind comprehension measures).

At least four story-blind critics play Mirewatch start-to-finish, four play Blackthorn start-to-finish, and two play both in opposite orders. Split each campaign into sessions of at most 90 minutes so fatigue is observed rather than mistaken for difficulty. Later missions must be reached through prior lessons on the first run; chapter-select seeding is only for targeted regression after the full-path report.

### 16.2 Report form and observation

Record screen/input video, time-to-first-valid-action, hint usage, undo/retry, idle/confusion intervals, popup skips/reopens, rule errors, objective paraphrase, predicted consequence, emotional beat recalled, and every place the critic believes a fixed casualty was their preventable failure. After each act, ask without leading:

- “What is the current goal, and what will end your turn?”
- “Who controls this empty square, and why?”
- “Which characters chose something the player could not choose for them?”
- “What did the party gain, lose, and leave unresolved?”
- “Who are Thaeron, Victor, Lash, and Fizzlewick, and how are their aims different?”
- “Name the current custodian/state of the Hold, fishbone key, Mirror, Root Key, and pearl.”

Hardcore critic additionally attempts sequence breaks, spawn blocks, intentional civilian kills, hidden collisions, target-filter bypass, alternate Hero configurations, empty deck/hand, timer abuse, save/reload at every checkpoint, and all-Hero-loss edge cases. Accessibility critic repeats every critical clue without relying on color, sound, fine motor timing, or auto-advance.

### 16.3 Acceptance thresholds

A revision cannot leave blind review while any critical canon contradiction, unwinnable state, silent objective change, dead-end save, counterfeit card ability, or protected-choice override remains. Quantitative gates:

- 100% of critics can explain End Turn, one normal activation, control/income, deployment trait gate, and all-original-Hero loss after their guided lesson.
- At least 90% correctly paraphrase the active objective before acting and independently use each starter card in its exam.
- Median unprompted stall is under 45 seconds; no accessibility persona is blocked over 90 seconds without an available layered hint.
- At least 85% recall each act's gain/cost/unresolved triad; 100% distinguish Thaeron, Victor, and Lash by the Lash reveal.
- 100% understand that Juniper/Garrett/Missus Vale/Fen/Braun/Gearjaw/Grask/Victor outcomes are authored costs rather than hidden “perfect play” rescues after the relevant debrief.
- No respondent says the game punished a character for declining consent in S01/MW09/MW12/MW18/MW23 or corresponding Blackthorn scenes.
- Rule quiz and live simulation agree at least 90%; any misconception shared by two critics triggers copy or legal-action revision even if the average passes.

Critics submit independent severity-tagged findings: `BLOCKER`, `RULE`, `CANON`, `PACING`, `ACCESS`, `POLISH`. The story director deduplicates only after reports are sealed, assigns fixes to the owning mission, and reruns deterministic tests. A fresh blind panel then retests every changed lesson plus both complete campaign paths.

## 17. Definition of done

The expanded mode is ready only when all of the following are true:

- All 53 runtime nodes load in canonical order, beginning with playable missions and including 15 authored tactical missions plus 38 StoryOnly narrative chapters, with no numeric story switch remaining.
- The exact live starter roster and every implemented rule pass Sections 6 and 11; every RL prerequisite has a functioning authoritative card definition or the badge is not shippable.
- Gold-path and deviation replays prove every phase, fail, checkpoint, save/load, difficulty, and accessibility path; no generic kill-all condition can override authored objectives.
- A canon test validates every fixed death/injury, autonomous choice, vote count, custody transition, artifact distinction, reveal boundary, and unresolved thread in Section 12.
- All required panels exist, render without clipping at supported sizes and +35% text expansion, have alt text/story-log entries, and pass the chronological contact-sheet review.
- Campaign clocks pause for instruction/accessibility; hint layers never perform a choice for the player; Story Pace skips only already mastered mechanics.
- Both full-deck capstones pass against the packaged definition hashes and again against the current authoritative server catalog.
- The blind panel meets every Section 16 threshold, all `BLOCKER`, `RULE`, `CANON`, and `ACCESS` findings are closed, and a new blind regression panel finds no reopened critical issue.

This is the measurable meaning of “polished”: canon remains honest about costs and unresolved roads, every starter character is learned through legal play, and a person who arrives knowing neither Gloomthorn nor Bayou Bonanza can finish able to explain both the Book One story and the actual game.
