# Gloomthorn Trilogy Story Mode Structure

> Historical narrative record: this document describes the earlier source-led adaptation. The public-audience rewrite follows [Writing Story Mode for new players](GLOOMTHORN_STORY_MODE_WRITING_GUIDE.md). Dialogue, scene counts, route order, practice requirements, lore detail, and menu wording below are superseded where they differ from the active catalog. The September 11 clarity loop cuts story detours, moves extra drills after the endings, and uses familiar Mirewatch teams. Printed card rules and ordinary game commands still apply.

Status: canonical implementation map for the revised trilogy

Date: 2026-09-09

## Source hierarchy

Story Mode is adapted from the revised manuscripts in the primary project's `design/` folder:

1. `Gloomthorn - Book One - Revised Opus 2026-09-05.docx`
2. `Gloomthorn - Book Two - Revised Opus 2026-09-05.docx`
3. `Gloomthorn - Book Three - Revised Opus 2026-09-05.docx`

Book One is the authority for the Mirewatch Resistance and Blackthorn campaigns. Books Two and Three are the authority for the implemented Seelie campaign and reserve the later Unseelie and mixed-court campaigns. Older literary-edition chapter maps and tutorial dossiers are background material only when they conflict with this document or the revised manuscripts.

The complete Seelie route, mechanical coverage, and narrative boundaries are recorded in `GLOOMTHORN_SEELIE_STORY_MODE_STRUCTURE.md`.

## Adaptation contract

Story Mode has two simultaneous jobs: tell a coherent version of the novels and teach the actual game. Neither job may counterfeit the other.

- A playable requirement must be an existing engine action backed by an authoritative card: move, attack, play a card, use a printed ability, or end a turn.
- Cargo, rigging, chains, evidence, votes, custody, consent, dialogue, measurements, artifacts, and autonomous character choices may be depicted or narrated, but are never invented inputs or mastery rules.
- A scene-faithful battle may dramatize the prose with the user's approved cast and outcome. A nonliteral lesson must identify itself as a rehearsal, reconstruction, simulation, dramatization, or isolated card demonstration before control begins.
- StoryOnly nodes preserve plot, causality, institutional work, refusals, losses, and character agency without manufacturing a fight.
- A rules simulation can teach a card without asserting that its represented characters fought in that chapter. Its aftermath must return to the novel's fixed result.
- Rescue never implies ownership. Consent, custody, memory sacrifice, root cuts, and bearer systems remain narrative facts unless ordinary game cards later implement them.
- The theatre prologue is not a route node. Both campaigns begin with the gator attack or its post-attack Blackthorn reconstruction.

## Book One story spine

| Act | Revised chapters | Story function |
|---|---|---|
| Arrival and recognition | 1-3 | Gator attack, Gilded Hold, customs pressure, Joni's sanctuary, and the watcher who chooses the children. |
| Resistance learns its cost | 4-8 | Hospitality, office evidence, wagon rescue, distributed responsibility, retaliation, and the first public record. |
| Rescue, loss, and the manufactured mystery | 9-14 | Debtor prison, Bluewater, invitations, auction, Vault, Juniper's death, missing captives, Lash's architecture, and Briar's escape with Mirror and Root Key. |
| The Society becomes public | 15-18 | Clinic ethics, the vote, Victor's bill, corroborated evidence, the public hearing, the charter ruling, bombing, surrender, and abduction toward Feyward. |
| Feyward and bounded alliance | 19-23 | Gossiping trees, the false factory heaven, memory testimony, allies with terms, the toll rescue, the writ, and the refugee road. |
| The grove reckoning | 24-28 | Ashenfang/Sylvara, four simultaneous losses, Birdie's clean shot, choice rather than cure, and Victor Greyshard's natural order. |
| Public custody and town coda | 29-30 and Epilogue | Thaeron's conditional grain offer is brought to witnesses; Mirewatch operates without heroic ownership; Pearl answers. |

The revised Chapter 14 absorbs the surviving road material that once occupied a standalone Chapter 15. Therefore `mw12_road_reaches` is retired. Revised Chapter 29 is represented by `mw25_deed_own_hand` and `bt18_deed_own_hand` immediately before the town/epilogue coda.

## Mirewatch Resistance route

The route contains 30 nodes: 12 tactical levels and 18 StoryOnly chapters.

| Order | ID | Type | Story/rules purpose |
|---:|---|---|---|
| 1 | `mw01_river_teeth` | Tactical | Begin on the attacked skiff. Reed, Donella, Erevan, and Telos defeat three gators; Reed makes two separate one-square moves; Telos uses Travel to clear Reed's Bow line. |
| 2 | `mw02_gilded_hold` | StoryOnly | The clearing, the Hold's self-release, and customs accounting. No invented guard fight; the Smuggler is mastered later. |
| 3 | `mw03_town_under_company` | Tactical rehearsal | Control, resources, deployment, Tax, aura timing, and arrival exhaustion. |
| 4 | `mw04_watcher_protects` | Tactical reconstruction | Juniper's range and turn cadence; the watcher's autonomous choice remains narrative. |
| 5 | `s01_hospitality` | StoryOnly | Hospitality and sanctuary without possession. |
| 6 | `mw05_watched_office` | Tactical simulation | Erevan's hidden movement and Hidden Shove; Hara's evidence and choice remain fixed. |
| 7 | `mw06_cost_seen` | Tactical | Six starting-deck characters clear the wagon route using printed actions; the wagon opens in aftermath only. |
| 8 | `s02_no_one_alone` | Optional open tactical check | All eight canonical defection/rescue panels remain intact, followed by a clearly labeled non-canon 3v3 check with no prescribed move, no mastery award, and ordinary printed actions only. |
| 9 | `mw07_twenty_debtors` | StoryOnly | Garrett, the 16-3 vote, twenty crossings, and nineteen living survivors. |
| 10 | `mw08_lesson_night` | StoryOnly | Reed refuses an unsealed bargain; his flare exposes Bluewater and costs the refuge. |
| 11 | `mw09_invitations` | StoryOnly | Four manufactured routes and the approach to the auction. |
| 12 | `mw10_beautiful_plan` | Tactical | Vanya's Blade Dance and Repeat; Nima opens her own lock in story. |
| 13 | `mw11_no_plan_saves_all` | Tactical reconstruction | Birdie and Reed clear real ranged lanes, then Juniper uses Sprint to position for her automatic Intercept. The board teaches the printed trigger while the aftermath preserves sixteen survivors, three missing people, Juniper's canonical death, and Reed's injured hand. |
| 14 | `s03_published_mystery` | StoryOnly | Burial, Erevan's failed private tail, Remy's escape, paper reconstruction, Lash reveal, and Briar's theft. |
| 15 | `s04_wounds_that_vote` | StoryOnly | Fever-bark ethics, Delphine's unasked risk, the 43-person vote, and the Society's duties. |
| 16 | `mw13_making_credit` | Optional tactical reconstruction | The canonical meeting, separate testimony, Gearjaw's rhythm, and distributed record remain in panels. A clearly non-canon real-card drill teaches Dematerialize, Frogback Leap, and the Swamp Tracker's passive end-turn Reveal without claiming an Ambusher entered the meeting. |
| 17 | `mw14_public_lie` | StoryOnly | Selective truths, Vanya's surrender, and public custody of the charter dispute. |
| 18 | `mw15_title_follows_burden` | Tactical dramatization | Starter-deck combined-arms exam framed around the Charter Day escape. Its three-part aftermath carries the causal bridge from the concealed bombing through Braun's refusal and death, Mog's surrender and evidence, Victor's three-case/crew abduction, and the resulting writ and pursuit toward the poisoned root road; none becomes an invented board input. |
| 19 | `mw16_gossiping_trees` | StoryOnly | Feyward's relational voices and the bounded stair bargain. |
| 20 | `mw17_factory_heaven` | StoryOnly | False kitchen, moth harvester, and Vesper's release. |
| 21 | `s05_memory_contradicts` | StoryOnly | Thaeron's choices are tested through memory without turning sacrifice into an input. |
| 22 | `mw18_allies_dishonestly` | Open tactical | A 4v4 toll-patrol fight introduces Pavo and Nettle through their printed cards; the badge, detainees, and crossing remain story facts. |
| 23-27 | `mw19_road_keeps_one` through `mw23_choice_not_cure` | StoryOnly | The refugee road, Sylvara, four simultaneous losses, Birdie's clean shot, and chosen restoration remain fixed character decisions rather than invented inputs. |
| 28 | `mw24_agent_not_heir` | Open tactical finale | A 7v7 formation asks the player to break Grask's command position while preserving Reed, Pavo, and Nettle; Victor's surrender and broken terms resolve only in the aftermath. |
| 29 | `mw25_deed_own_hand` | StoryOnly | Thaeron's Chapter 29 offer enters witnessed Society custody without a private acceptance. |
| 30 | `s06_town_owns_itself` | StoryOnly | Chapter 30 and epilogue: the town functions, the record remains public, and Pearl answers. |

## Blackthorn route

The route contains 27 nodes: 10 tactical levels and 17 StoryOnly chapters. It is an opposition dossier, not an alternate canon in which Blackthorn wins scenes it loses in the novel.
Seven tactical practice levels are explicitly non-canon and skippable: five early guided reconstructions, the first 3v3 open-choice check at `bt06_receipt_book`, and the later isolated Ashenfang demonstration. Skipping any of them grants no mastery. `bt17_natural_order` is a required catch-up only when at least one of those seven entries lacks a completed-by-play stamp. If all seven stamps exist, its normal Continue action advances to the required unscripted 4v4 depth-two bridge, while `Replay Synthesis` remains an explicit optional practice choice. The tenth tactical level is a recommended depth-three ordinary match with legal 20-card decks, hidden Hero placement, hands, economy, clocks, open turns, and victory by opposing-Hero elimination or opposing match-clock expiry. Its pre-clock choice may continue the story without mastery. All training ends before the canonical World Tree climax begins, so Birdie's clean shot leads directly into Victor's surrender, testimony, breach, death, and the public coda without an exam interrupting the ending.

Canonical order:

`bt01_harness_hunger` -> `bt02_customs_bell` -> `bt03_sanctuary_debt` -> `bt04_terms_conditions` -> `s01_hospitality` -> `bt05_freight_office` -> `bt06_receipt_book` -> `s02_no_one_alone` -> `bt07_debtor_prison` -> `bt08_north_lock` -> `bt09_published_mystery` -> `s03_published_mystery` -> `bt10_stolen_road` -> `bt11_public_lie` -> `bt12_break_charter` -> `bt13a_poisoned_root_road` -> `bt13_feyward_transit` -> `s05_memory_contradicts` -> `bt14_move_boundary` -> `bt15_monster_rules` -> `bt17_natural_order` -> `bt17a_field_judgment` -> `bt17b_open_mastery` -> `bt16_clean_shot` -> `bt17c_victor_reckoning` -> `bt18_deed_own_hand` -> `s06_town_owns_itself`

The Feyward transition is deliberately two chapters: `bt13a_poisoned_root_road` completes the brood/pipe decision and Briar's bounded stair terms; `bt13_feyward_transit`, displayed as **False Factory Heaven**, begins only after that stair reaches the moth-harvester factory. This prevents the road, bargain, false kitchen, and Vesper's release from collapsing into a synopsis montage.

The tactical lessons are:

| ID | Framing | Existing rules taught |
|---|---|---|
| `bt01_harness_hunger` | Recommended post-attack reconstruction | Movement, healing, Disable application and its first skipped activation, and damage. The full two-owner-turn Disable lifecycle is reserved for `bt15`; scent timber is pre-existing context, never an input. |
| `bt02_customs_bell` | Recommended patrol reconstruction | Victor's optional Relentless second action and explicit decline, Braun movement, Sharpshooter transform/line of sight, Tax, and health. |
| `bt03_sanctuary_debt` | Recommended office economy simulation | Income, Gather, Tax, deployment, Command preserving normal activation, exhausted Summon arrival, Trail, and both Sapling profiles including Heal, Entangle, cooldown, and the full Disable duration. |
| `bt04_terms_conditions` | Recommended isolated Ambusher drill | Dematerialize, pass-through, collision reveal, Ambush damage, and one-activation Disable. |
| `bt05_freight_office` | Recommended operational reconstruction | Grask's Capture staging and Charge; Mog's damage. |
| `bt06_receipt_book` | Recommended early open check after the canonical receipt panels | A mastery-free 3v3 battle with no glowing move or prescribed order. The recorder, wagon escape, retaliation, and Torren's disappearance remain fixed story events. |
| `bt15_monster_rules` | Recommended isolated Ashenfang demonstration | A two-piece board uses one Ashenfang body and one neutral surviving Veteran to teach diagonal range and two-owner-turn Disable. Ashenfang is Sylvara's coerced form, never a separate or willing Blackthorn hero. |
| `bt17_natural_order` | Conditional non-canon guided catch-up / optional replay | Exact all-path proof of the 12-card Blackthorn starter roster when any earlier drill lacks play evidence: every claimed printed profile, dependent, passive, transform, hidden state, cooldown, and two-turn status is executed through the ordinary engine. Routine scripted turn transitions auto-resolve; the player supplies the 26 real card actions and abilities across 74 total steps. Completing all seven prior drills proves the same card/rule set and makes this an optional replay. |
| `bt17a_field_judgment` | Required non-canon open bridge | A 4v4 depth-two `DefeatAllEnemies` position with no script or mastery award. Thaeron must survive while the player selects the first fully independent unit and action. |
| `bt17b_open_mastery` | Recommended non-canon ordinary-match exam | Two legal decks each contain 20 non-Hero cards plus two legal Heroes. Each side has one non-resetting 2:00 Hero-placement deadline before its 15:00 match clock begins; ordinary turns begin at 2:00. The player manages hands, control income, draw/discard, increments, and timeouts, then wins by eliminating the opposing starting Heroes or exhausting the opposing match clock. Continuing before clocks start advances the story without mastery. |

`s04_wounds_that_vote` is not duplicated on this route. Revised Chapters 15-16 are folded into `bt10_stolen_road` (displayed as **The Bill Comes Due**) so Victor's bill, the clinic vote, Society rules, and evidence chronology remain contiguous.

## Books Two and Three: Seelie implementation and future handoffs

The Book One routes may foreshadow later courts only through what Book One characters know. The Seelie route starts with Vesper's bounded Mirror choice in Book Two, Chapter 16, reaches its first tactical battle at the Cathedral of Last Lights in Chapter 17, and then carries one causal story through the end of Book Three. It teaches the entire Seelie starter without turning contracts, memory, custody, consent, or the bearer system into invented game commands.

| Gate | Status | Campaign purpose |
|---|---|---|
| Book Two, Chapter 16, **What the Mirror Remembers** | Implemented Seelie opening | Establish why Vesper accepts only the bounded Cathedral road and closes older claims before any tactical input, then finish with a three-name orientation anchor for Vesper, Caltheriel, and Lash. |
| Book Two, Chapter 17, **Cathedral of Last Lights** | Implemented first Seelie tactical | Begin board play with a real attack, four legal starter actions, and the question that drives the court investigation. |
| Book Two, Chapter 18, **Road of Small Lights** | Implemented Seelie lesson | Teach Rebirth, flight, pass-through, ranged attacks, and survivor-aware positioning inside a real rescue. |
| Book Two, Chapters 19-26 | Implemented Seelie story spine | Make witness, exit, withdrawal, Caltheriel's choice, the Dusk Crown, and distributed release understandable without counterfeit inputs. |
| Book Three, Chapters 1-35 | Implemented Seelie continuation | Finish the spaced advanced-rule rehearsals, then escalate through a forgiving open drill, literal battles, a withdrawal objective, a full-formation defense, the seven-part answer, and an ordinary-work epilogue. |
| Book Two, Chapter 20 | Reserved Unseelie point of view | Revisit Nyxara's refuge-versus-custody terms from inside the Unseelie route before the player commands that deck. |
| Book Two, Chapter 23, **Clockwork Domain** | Reserved first Unseelie tactical | Introduce the Unseelie starter inside its own revealed stakes. |
| Book Two, Chapters 25-26 | Reserved mixed-court application | Combine court knowledge only after both factions have independent narrative and mechanical grounding. |
| Book Three, Chapters 8, 15, 19, 21, 22, and 28 | Reserved cross-court and advanced Unseelie | Teach interactions from the relevant characters' own point of view while preserving fixed costs and choices. |
| Book Three, Chapters 30-35 | Reserved mixed-court capstone | Revisit the shared climax only after independent Seelie and Unseelie mastery; never replace the canonical withdrawals, vacancy, Recall, or Lash's arrow wound with a boss-kill objective. |

## Art lock

The following gates were applied before final tutorial illustration generation:

1. Route IDs, node order, chapter sources, and tactical/StoryOnly classifications are locked.
2. Every required tactical input succeeds through the ordinary engine and an authoritative card.
3. Every nonliteral board is labeled before control, and its aftermath returns to canon.
4. Final panel copy fits the supported layouts at 1920x1080 and 800x600 without clipping or ambiguity.
5. A capture audit identifies which existing images are user-supplied, project-created, reusable, misleading, or missing.

All active nodes now have versioned custom scenario art tied to their locked panel copy. The final capture pass re-renders every registered briefing, action, popup, and aftermath view at both supported layouts so future story edits cannot silently invalidate an illustration.
