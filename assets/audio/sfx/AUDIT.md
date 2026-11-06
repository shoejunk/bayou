# Bayou Bonanza sound audit and compact library

41 master recordings cover the current game and the repository's ten effect-card artworks. 38 are new ElevenLabs Sound Effects v2 recordings; three reuse the Bull Gator, Crystal Unicorn and Thaeron previews. This is a practical production minimum, not a mathematical lower bound: fewer files are possible if weapon, material, movement or notification identities are collapsed. The sounds and usage recipes are integrated into the native client. See SOUND_EFFECTS_INTEGRATION.md and SOUND_EFFECTS_INTERACTION_COVERAGE.md for runtime coverage and verification.

## Evidence and scope

- Read-only live catalog snapshot: game.gloomthorn.com:55004 CardListRequest, 2026-11-05T05:12:06.284Z, schema 12; 97 cards (13 Hero / 84 Unit), 160 action assignments and 56 unique internal action definitions. Full snapshot: output/sfx-audit/catalog.json. Local cards.db has zero rows and was not used as the gameplay truth.
- All 20 GameState values, reusable controls, input events, screen fragments, shop, deck editor, story, Conquest, admin and card editor were inventoried from source. 163 static control call sites are indexed in output/sfx-audit/ui-control-inventory.csv; dynamic list rows and keyboard/drag behavior are covered by the interaction families below. This is a source and asset audit, not a claim to have manually clicked every possible online state.
- 392 animation strips indexed in output/sfx-audit/animation-coverage.csv, including walking, attack, damage, death, idle, fidget and aim/lower variants. Six representative frames from all 99 attack strips were visually reviewed in attack-atlas-1..4.png; other states are mapped from their actual file binding, material and animation role.
- src/client/main.cpp:70 currently defines nine AudioCue values. The snapshot-diff audio route at ~5225-5450 coalesces generic place/move/attack/death, so simply adding files cannot deliver the per-action variety in this plan. Current playback supports volume but needs future event routing and pitch/offset selection.
- src/client/client_board_layout.hpp:42-43: attack 0.42 seconds, damage/death reaction 0.55 seconds. src/client/main.cpp:529: move 0.95 seconds. Fidgets are explicitly disabled at main.cpp:528. Full generated masters may ring beyond the animation; short attack/contact slices are exported for timing.
- src/shared/game_data.hpp, src/shared/game_rules.hpp and src/gameserver/game_engine.hpp define the supported ability, spell, enchantment and passive mechanics. docs/AURAS.md explicitly notes no current catalog assignments for Rot Aura/Silence Field.
- design/GT Card Abilities.docx and design/Card Abilities, traits, etc.docx contain alternatives/proposals, sometimes different from live rules. They inform reusable future coverage, not claims of implementation. Design ability text was extracted to output/sfx-audit/design-abilities.txt.

## Minimum sound list

| # | ID | Sound | Seconds | Reuse |
| --- | --- | --- | --- | --- |
| 1 | ui_tap | Soft interface tap | 0.5 | Buttons, tabs, checkboxes, focus changes, dropdowns, slider detents; very quiet hover variant |
| 2 | card_paper | Card and page swish | 0.8 | Draw, discard, drag pickup/drop, hand fan, deck add/remove, story page, popup transitions |
| 3 | ui_confirm | Positive confirmation | 0.8 | Save/login/claim success, match found, ready, objective complete, turn ready |
| 4 | ui_reject | Invalid action / cancellation | 0.6 | Illegal target, failed validation, insufficient funds, server error; quieter cancellation variant |
| 5 | alert_bell | Attention bell | 1.2 | Match ready, clock thresholds, connection loss, story alarm/bell, conquest deadline |
| 6 | resource_coins | Resource / reward coins | 1.2 | Resource gain/spend/tax/drain, shop purchase, rewards, conquest supply |
| 7 | victory | Victory flourish | 3 | Win, mission complete, campaign milestone, conquest victory |
| 8 | defeat | Defeat flourish | 2.5 | Loss, mission failure, defeat/timeout/resignation result |
| 9 | foot_soft | Light footstep pair | 1.2 | Humans, goblins, light creatures; crop alternating steps; cloth fidgets |
| 10 | foot_heavy | Heavy step / earth thump | 1.5 | Giants, tree folk, gator mount, heavy landing, blunt charge body layer |
| 11 | hoof_crystal | Crystalline hoof walk | 4 | Crystal Unicorn and Glimmer Stag movement; identity sparkle layer |
| 12 | wing_feather | Feather / leathery wingbeat | 1.4 | Griffin, batlike stalker, birdlike movement, dives, soft air swooshes |
| 13 | wing_insect | Insect / fey wing flutter | 1.6 | Fairies, moth/bee royalty, tiny flyers; swarm/infestation layer |
| 14 | swamp_squelch | Wet step / slither | 1.4 | Bull Gator, Scooter, frog mount, slugs/roots, muddy step overlays |
| 15 | mechanism | Clockwork ratchet / clank | 1.8 | Construct movement, repair, build, aim/weapon raise/lower, clockwork fidget |
| 16 | blade | Blade / spear / axe swish | 1 | Knives, swords, axes, spears, scythes; pitch/weight changes by weapon |
| 17 | blunt | Punch / shield / heavy strike | 0.8 | Punches, shield bash, staff, horn/antler collision, charge impacts |
| 18 | bow | Bow / slingshot release | 0.9 | Reed, Birdie, bows, sling, dart/thorn launch, projectile travel |
| 19 | gun | Black powder shot | 1.2 | Goblin Sharpshooter, spectral volley; explosion donor for story firearms |
| 20 | magic_bright | Bright magic bolt | 1.1 | Spark, Star Dart, Sun Sprite, staff light, solar/arcane attacks |
| 21 | magic_dark | Dark magic strike | 2.48 | Hexes, dark bolts, spectral attacks, curses/drain layers; original Thaeron preview |
| 22 | beast_bite | Beast growl / bite | 2.48 | Bull Gator and giant mount, beast claw/bite; extract growl and jaw snap separately |
| 23 | body_hit | Flesh / cloth damage hit | 0.7 | Generic damage, creature collisions, death body fall, wooden UI drop low layer |
| 24 | metal_hit | Metal hit / armor / cage | 1.1 | Construct damage/death, block/intercept/bodyguard, shields, cage lock, repair accent |
| 25 | wood_hit | Wood / bone crack | 1.1 | Tree/plant/undead damage and death, wood structures, broken plank, bone joints |
| 26 | crystal | Crystal / mirror shimmer | 1.4 | Unicorn, mirror magic, rare reveal, magical ward, cleansing accent, identity layer |
| 27 | heal | Healing / restoration bloom | 1.6 | All healing actions/auras, cleanse, rebirth layer, growth, medical success |
| 28 | roots | Roots / web / binding fibers | 1.4 | Entangle, thorns, webs, growth, plant movement, infestation physical layer |
| 29 | teleport | Phase / summon / vanish | 1.5 | Dematerialize, teleport, swap, summon, reveal, evacuation, transformation |
| 30 | power | Power / rally / command pulse | 1.3 | Command, Power Boost, relentless readiness, swarm gain, enchant/last stand |
| 31 | potion | Potion / venom splash | 1.2 | Alchemist, venom, marsh remedies, liquid magic, honey/wax/acid future effects |
| 32 | steam | Steam / hush / null pressure | 1.4 | Steam power, silence field, mechanical pressure, rot aura onset, fuse hiss |
| 33 | explosion | Compact powder explosion | 1.7 | Keg, detonation, bombs, chain reaction, destruction; scale by size |
| 34 | shriek | Creature / sonic shriek | 1.3 | Shriek, bat sonic dart, fear, aggressive creature vocalization |
| 35 | charm_pipes | Enchanting pipes / control | 1.5 | Pavo pipes; charm, lull/sleep, lure, mind control as pitched versions |
| 36 | death | Spirit / death dissolve | 1.6 | Unit removal, exile, undead death, sacrifice, enchant expiry, spirit rise reversed |
| 37 | water | Water splash / ripple | 2 | River movement, boat/raft, frog landing, floodgate, wet projectile impact |
| 38 | fire | Fire / fuse crackle | 4 | Burn, fuse, camp fire, burning boats, flame magic accents |
| 39 | creak | Gate / timber / rope creak | 1.8 | Doors, gates, cage opening, wagons, raft timber, structural strain |
| 40 | bayou | Quiet bayou atmosphere | 8 | Optional menu/story/map environmental bed, river/forest scene variation |
| 41 | ghost_chorus | Spectral chorus pulse | 1.8 | The Choir sizes, spirit voices, fear/mimic/echo, Unseelie identity |

## UI and system interactions

| Surface | Interaction | Sound | Trigger policy |
| --- | --- | --- | --- |
| All menus/forms | Button, keyboard activation, checkbox, tab, dropdown | ui_tap | Audible quiet pitched hover on target entry; firmer activation. No hover repeat while stationary; click takes precedence in the same frame. |
| Forms / text | Focus, typing, caret, selection, password visibility | ui_tap for focus/toggle; otherwise silence | No keystroke chatter or sound exposing password length. |
| Options | Sliders, resolution, apply, mute, save | ui_tap at release/detents; ui_confirm on saved | No continuous drag noise; use preview cue only if audio is enabled. |
| Login/account | Login, create account, change password, logout | ui_tap + ui_confirm/ui_reject | Acknowledge server success or failure once. |
| Navigation | Back/cancel, popup open/close, scroll, tooltips | pitched/reversed card_paper for panels; quiet ui_tap for changed list offsets | Open/close and card inspection announce transitions. Redraws and scroll limits stay silent; cancel is not an error. |
| Deck/collection | New/edit/delete, search/filter, inspect, add/remove, drag/drop, unsaved prompt | card_paper + ui_tap; ui_confirm on save; ui_reject invalid | Reorder/remove can use reversed paper, no separate recordings. |
| Shop/starter decks | Purchase, coin pack, claim deck, reveal, rarity | resource_coins + card_paper + crystal; ui_confirm | Only verified purchase completion. Rarity changes layering/timing, not a new master per rarity. |
| Matchmaking/sandbox | Find/cancel, loading, match found, connect/disconnect | ui_tap; alert_bell + ui_confirm when ready; ui_reject on failure | No looping search beep; ready cue must remain recognizable. |
| Board selection | Piece/card hover, inspect, select/deselect, action choice | quiet ui_tap/card_paper | No attack sound until accepted action resolves. |
| Board gestures | Legal move/deploy, drag cancel, invalid target, power/sacrifice confirmation | Movement/deploy/ability recipe; ui_reject for invalid | Avoid button + generic attack + ability triple triggering. |
| Card systems | Hand fan, draw/discard, foresight choice, graveyard, raise undead | card_paper + appropriate magic cue | Draw/reveal should not leak concealed cards to another player. |
| Battle status | Start/end turn, command selection, timer 120/60/30s, result | ui_confirm/power; alert_bell; victory/defeat | Use existing clock-warning tracker; at most one warning per crossed threshold. |
| Story menus/dialogue | Campaign/mission select, next/back/skip, choices, instructions | card_paper + ui_tap; ui_confirm at completed objective | Narrator text is silent; no generated speech needed. |
| Story world beats | Gate/cage, raft/boat/flood, fire/fuse, collapse/explosion, signal bell | creak/metal_hit; water; fire; wood_hit/explosion; alert_bell | Authored one-shot at visible story beat; no automatic keyword-triggered narration effects. |
| Conquest | Event list/create/join/ready/start/end, army setup, placement, orders/undo/exit | ui_tap + card_paper; ui_confirm; ui_reject | Strategic movement reuses foot_soft/foot_heavy; capture power; casualties death; awards resource_coins. |
| Conquest timing | Deadline, battle becomes available, eliminated, final winner | alert_bell; ui_confirm; defeat/victory | Only meaningful new state once; suppress repeated refresh notices. |
| Admin/card editor | New/copy/save/delete cards/actions, filters, lists, asset pick, grants | ui_tap + card_paper; ui_confirm/ui_reject | All CRUD and confirmations reuse player UI vocabulary; no admin-only sound pack. |
| Network recovery | Disconnected/rejected request, reconnect, resumed state | one alert_bell or ui_reject; ui_confirm on recovered | Snapshot resync is silent: do not replay all old summons/damage/deaths. |
| Ambient/idle | Menu/map/story environmental bed, idle and fidget | optional bayou/fire; idle silent | Current fidgets disabled. If enabled later, use sparse material/cloth/wing accents only for visible actor; do not add a sound every loop. |

All game states: Menu, SandboxLoading, Options, Login, CreateAccount, ChangePassword, Authenticated, StorySelect, StoryMissionSelect, StoryIntro, DeckSelect, Matchmaking, DeckEditor, Shop, StarterDecks, AdminUsers, AdminTools, CardEditor, Conquest, Game.

## Rules, abilities and statuses

| Event | Reusable sound recipe | Timing/conditions |
| --- | --- | --- |
| deploy | card_paper + teleport + material accent | Confirmed visible placement; one event per batch, never on failed drag. |
| draw/discard/shuffle/foresight | card_paper; crystal only when a choice is revealed | Paper one-shot per actual card transition, rate limited on multi-draw. |
| turn start / extra action / relentless | ui_confirm; power for a granted extra action | Turn cue only for local player. No sound for every ready flag update. |
| resources / tax / gather / resourceDrain | resource_coins | Aggregate deltas once per turn/event; reverse or lower pitch for spend/drain, distinct from real-money purchases. |
| damage / counterstrike / bristle / collision | body_hit, metal_hit or wood_hit according to victim | Impact once per visible hit, counterattack uses attacker cue once; cap simultaneous victims. |
| healing aura / lifedrain / cleanse | heal; potion or mechanism overlay if physical | Only positive actual healing; suppress healed-zero/no-op and rot-blocked blooms. |
| bodyguard / intercept / shelter | metal_hit + quiet power; teleport for an actual swap | Only when protection changes the result; no sound on passive adjacency alone. |
| reveal / foresight / mark | crystal + card_paper or teleport | Only information that client is allowed to know. Hidden actions cannot leak location or identity. |
| swarm / Power Boost / enchant damage | power + identity accent | Play only activation/threshold change, not continuous derived recomputation. |
| entangle / stun / sleep / grow wait | roots or magic_dark; charm_pipes for sleep | Apply once; no loop on disabled pieces; removal uses a soft heal/crystal tail. |
| control / charm / return ownership | charm_pipes + card_paper; ui_confirm on local regained control | Distinguish control expiration with softer reversed/tail variant. |
| infestation / rebirth / raised exile | wing_insect + roots; death + teleport; death only on exile | Replacement is one death plus one spawn; avoid playing a second generic kill. |
| dig / tunnel / jump / hop / push / pull | swamp_squelch or teleport + landing material | No loud takeoff for ordinary slide; collision impact only if a collision occurs. |
| transform / aim / lower weapon | mechanism for a physical stance; teleport + identity for magical form | Aim is a ratchet/string creak, not a gunshot. |
| sacrifice | death + payload cue (power/heal/explosion/teleport) | Payload at resolution, death once. Do not play an ordinary hostile hit on activation. |
| silence field / rot aura | steam + low magic_dark tail | Supported reusable rules, no live catalog bearer currently; one onset/expiry, no permanent hum required. |
| enchant player/square/piece | power + resource_coins or magic_dark | Covers runtime damage/resources/resourceDrain enchantment types; expiration uses death tail. |
| spell damage/heal/resources/steam | magic_bright or magic_dark / heal / resource_coins / steam | Only four supported spell effect names. Undefined spells receive ui_reject and no cast/impact. |
| timeout / resignation / match end | alert_bell then victory/defeat | Outcome once after authoritative result, cancel queued combat sound tails. |
| resurrection to hand / evacuate to hand | death reversed + card_paper + optional heal | Raise Undead returns one-health card to hand in current rules; do not play a board spawn until deployment. |

Every action on every live card has its own semantic recipe in cue-map.json and the table below. This includes heal actions that share an attack gesture, status-only actions, action repeats, control, Lifedrain, push/pull, infestation and form changes. Healing is not treated as damage just because canAttack is true.

## Effect cards

The following ten artworks are present but no Spell/Enchantment is returned by this live snapshot. Sound recipes are ready; undefined effects must remain silent except for rejection until supported mechanics are assigned. Heartshoot is currently a live Plant Unit, not a Spell; the table below follows the server.

| Effect card | Recipe | Status / use |
| --- | --- | --- |
| Ambush | card_paper + blade + beast_bite growl | Artwork/design present; absent from live 97-card snapshot. Design/art only; resolve via eventual effect definition, not title matching. |
| Rapid Assembly | mechanism + teleport | Artwork/design present; absent from live 97-card snapshot. Mechanical build/summon texture. |
| Supply Shortage | resource_coins lowered/reversed + ui_reject | Artwork/design present; absent from live 97-card snapshot. Resource-denial identity. |
| Survival Instinct | beast_bite growl + power + heal | Artwork/design present; absent from live 97-card snapshot. Short protective instinct response. |
| Hidden Path | teleport + roots | Artwork/design present; absent from live 97-card snapshot. Concealment/relocation, no ordinary damage impact. |
| Spell Surge | power + magic_bright | Artwork/design present; absent from live 97-card snapshot. Brief arcane empowerment pulse. |
| Ancient Awakening | roots + foot_heavy + heal | Artwork/design present; absent from live 97-card snapshot. Old wood stirs into life. |
| Cursed Ground | magic_dark + roots | Artwork/design present; absent from live 97-card snapshot. Curse onset; damage only when it actually resolves. |
| Raise the Fallen | death reversed + teleport + heal | Artwork/design present; absent from live 97-card snapshot. Resurrection identity, actual destination follows rules. |
| Last Stand | metal_hit + power | Artwork/design present; absent from live 97-card snapshot. Defiant protection/rally, no separate fanfare. |

Other proposed effects require no new source recordings: barriers use crystal+metal_hit; time warp uses mechanism+teleport; ice/mirror uses crystal+teleport; fear uses shriek+magic_dark; poison uses potion+magic_dark; terrain uses roots/water; mines use fire+explosion; information effects use crystal+card_paper; healing and protective effects use heal+power; construct repair uses mechanism+heal.

## Per-character coverage

| Character | Movement | Actions | Ability | Damage / death |
| --- | --- | --- | --- | --- |
| Archduchess Vespara | wing_insect | Winged Advance: movement only; Bloodless Kiss: magic_dark + wing_insect + death + heal | passives / none | body_hit / body_hit + death |
| Archivist Mosswake | roots | Rootwalk: roots + wood_hit | passives / none | wood_hit / wood_hit + death |
| Ashenfang | roots | Entangling Lunge: roots + magic_dark | passives / none | wood_hit / wood_hit + death |
| Birdie the Wise | foot_soft | Scout: movement only; Longbow Shot: bow | passives / none | body_hit / body_hit + death |
| Black Powder Keg | stationary/silent | Fuse Strike: fire + explosion | passives / none | body_hit / body_hit + death |
| Blackthorn Alchemist | foot_soft | Healing Elixer: heal + potion; Paralysis Potion: potion | passives / none | body_hit / body_hit + death |
| Blackthorn Debt Collector | foot_soft | Knife Stab: blade | passives / none | body_hit / body_hit + death |
| Blackthorn Engineer | foot_soft | Advance: mechanism + blunt; Repair: mechanism + heal | passives / none | body_hit / body_hit + death |
| Blackthorn Foreman | foot_soft | Slide: blade + mechanism | summon: teleport + power | body_hit / body_hit + death |
| Blackthorn Lumberjack | foot_soft | Axe Swing: blade | passives / none | body_hit / body_hit + death |
| Blightling | roots | Blighted Lunge: roots + wood_hit | passives / none | wood_hit / wood_hit + death |
| Bog Spearman | foot_soft | Hooked Spear: blade; Spear Thrust: blade | passives / none | body_hit / body_hit + death |
| Braun Stonefist | foot_soft | Charge: blade + blunt | passives / none | body_hit / body_hit + death |
| Briar Whisperthorn | roots | Thorn Strike: magic_dark + roots; Briar Step: teleport | passives / none | wood_hit / wood_hit + death |
| Bristlejack | swamp_squelch | Bristle Charge: beast_bite + blunt | passives / none | body_hit / body_hit + death |
| Bull Gator | swamp_squelch | Bite: beast_bite | passives / none | body_hit / body_hit + death |
| Caltheriel | wing_insect | Fey Flight: movement only; Charm: charm_pipes + magic_bright | passives / none | body_hit / body_hit + death |
| Camp Physician | foot_soft | March: movement only; Field Dressing: heal + potion | passives / none | body_hit / body_hit + death |
| Clockwork Guardian | mechanism | Guard: mechanism + metal_hit; Self-Repair: mechanism + heal | passives / none | metal_hit / metal_hit + death |
| Crystal Unicorn | hoof_crystal | Prismatic Charge: magic_bright + crystal | passives / none | body_hit / body_hit + death |
| Delphine Nettle | foot_soft | Swamp Step: movement only; Nettle Hex: magic_dark | passives / none | body_hit / body_hit + death |
| Donella of the Marsh | foot_soft | Sprint: movement only; Marshlight Mend: heal; Spark: magic_bright | passives / none | body_hit / body_hit + death |
| Duchess Dewbell | wing_insect | Dewbell's Decree: magic_bright | passives / none | body_hit / body_hit + death |
| Duke Lanternwing | wing_insect | Flight: movement only; Mesmerizing Lantern: magic_bright + fire + teleport; Burn: magic_bright + fire | passives / none | body_hit / body_hit + death |
| Dusk Harvester | foot_soft | Scythe Advance: blade; Harvest: magic_dark + death + heal | passives / none | body_hit / body_hit + death |
| Echo Lantern | foot_soft | KingMove0: movement only; RangedHex1: magic_dark | passives / none | body_hit / body_hit + death |
| Elliot Greentide | teleport | Spectral Passage: movement only; Ghostly Volley: gun + magic_dark | passives / none | death / death + death |
| Erevan the Shadow | foot_soft | Shadow Blade: blade; Fade Through Shadow: movement only; Hidden Shove: blade + teleport | dematerialize: teleport | body_hit / body_hit + death |
| Eyeblight | roots | Creeping Roots: movement only; Blighting Gaze: roots + magic_dark | passives / none | wood_hit / wood_hit + death |
| Fairy Ring | stationary/silent | Enchanted Circle: heal; Fairy Snare: magic_dark | passives / none | body_hit / body_hit + death |
| Father Grub | swamp_squelch | KingMove1: blunt + swamp_squelch; KingHeal2: heal | passives / none | body_hit / body_hit + death |
| Fey Messenger | wing_insect | Courier Flight: movement only; Star Dart: magic_bright | passives / none | body_hit / body_hit + death |
| Fizzlewick Gearwright | foot_soft + mechanism | Knife Stab: blade; Repair: mechanism + heal | summon: teleport + power | body_hit / body_hit + death |
| Gearjaw | mechanism | Bite: mechanism + metal_hit; Rebuild: mechanism + heal | passives / none | metal_hit / metal_hit + death |
| Gilded Cage | stationary/silent | Rattle: movement only; Golden Bind: roots + magic_dark | passives / none | metal_hit / metal_hit + death |
| Glimmer Stag | hoof_crystal | Antler Vault: blunt + crystal; Glimmer Call: charm_pipes + crystal | passives / none | body_hit / body_hit + death |
| Gloom Fairy | wing_insect | Fly: movement only; Shoot Spark: magic_dark | passives / none | body_hit / body_hit + death |
| Goblin Ambusher | foot_soft | Stab: blade; Sneak Around: movement only; Ambush: blade | dematerialize: teleport | body_hit / body_hit + death |
| Goblin Sharpshooter | foot_soft | Advance: movement only; Fire: gun | transform: mechanism + teleport | body_hit / body_hit + death |
| Grask | foot_heavy | Charge: blade; Swing Axes: blade | passives / none | body_hit / body_hit + death |
| Grove Sister | roots | Glide: roots + wood_hit | passives / none | wood_hit / wood_hit + death |
| Heartshoot | roots | Rootstep: movement only; Thorn Bolt: bow + roots | passives / none | wood_hit / wood_hit + death |
| Heartwood | foot_heavy | Rootbound Crush: roots + wood_hit | passives / none | wood_hit / wood_hit + death |
| Heartwood Sister | roots | Grovewalk: movement only | passives / none | wood_hit / wood_hit + death |
| Hidden Camp | stationary/silent | no authored actions | passives / none | body_hit / body_hit + death |
| Hushkeeper | wing_insect | QueenFly0: movement only; ShortRangedAttack1: magic_dark | passives / none | body_hit / body_hit + death |
| Joni Pumpernickel | foot_soft | Walking Stick: magic_bright | passives / none | body_hit / body_hit + death |
| Juniper Flash | foot_soft | Spark: magic_bright; Sprint: movement only | passives / none | body_hit / body_hit + death |
| Lady Mirrorglace | foot_soft + crystal | Mirror Step: crystal + teleport; Shattering Reflection: crystal + teleport | passives / none | body_hit / body_hit + death |
| Lady Silkmaw | swamp_squelch | Silkstep: roots + magic_dark; Venomous Thread: roots + magic_dark | passives / none | body_hit / body_hit + death |
| Lord Pallid | foot_soft | QueenMoveD1R2: blade; RangedHex1: magic_dark | passives / none | body_hit / body_hit + death |
| Lumber Automaton | mechanism | March: mechanism + blunt; Maintenance: mechanism + heal | passives / none | metal_hit / metal_hit + death |
| Maggie Mudroot | foot_heavy + creak | Gator-House Charge: beast_bite + creak | raise undead: death + heal + teleport | body_hit / body_hit + death |
| Maggie Mudroot Unmounted | foot_soft | Marsh Step: movement only; Mudroot Hex: magic_dark | passives / none | body_hit / body_hit + death |
| Maggie's Recruit | foot_soft | Axe Swing: blade | passives / none | wood_hit / wood_hit + death |
| Marrowind | foot_heavy | Rooted Advance: wood_hit + blunt; Old Growth: wood_hit + blunt | passives / none | wood_hit / wood_hit + death |
| Marshland Veteran | foot_soft | Advance: blade | passives / none | body_hit / body_hit + death |
| Mirewatch Informant | foot_soft | Lunge: blade | passives / none | body_hit / body_hit + death |
| Mog | foot_heavy | Axe Swing: blade | passives / none | body_hit / body_hit + death |
| Moonshade Stalker | wing_feather | Moonshade: movement only; Ambush: beast_bite + wing_feather | passives / none | body_hit / body_hit + death |
| Mossy Giant | foot_heavy | Rooted Advance: blunt + wood_hit; Stonefist: blunt + wood_hit | passives / none | body_hit / body_hit + death |
| Mourning Light | teleport | Drift: movement only; Pale Comfort: heal | passives / none | death / death + death |
| Nettle Starbright | wing_insect | Fly: movement only; Slingshot: bow | passives / none | body_hit / body_hit + death |
| Nettle Starbright Unmounted | foot_soft | Punch: blunt | passives / none | body_hit / body_hit + death |
| Nibsy | foot_soft | Shuffle: movement only; Lantern Bolt: magic_dark | passives / none | body_hit / body_hit + death |
| Pavo Quickstep | foot_soft | Quickstep: blade; Enchanting Pipes: charm_pipes | passives / none | body_hit / body_hit + death |
| Pipistrel Vane | wing_feather | Swoop: movement only; Sonic Dart: shriek | passives / none | body_hit / body_hit + death |
| Prince Vesper | wing_insect | Royal Flight: movement only; Vesper Spark: magic_bright | passives / none | body_hit / body_hit + death |
| Queen Nyxara | wing_insect | Royal Advance: movement only; Nyxaran Infest: wing_insect + magic_dark + roots | passives / none | body_hit / body_hit + death |
| Quinberry Lark | wing_feather | Swoop: blade; Sneak Around: movement only | dematerialize: teleport | body_hit / body_hit + death |
| Reed Baelstone | foot_soft | Step: movement only; Bow: bow | passives / none | body_hit / body_hit + death |
| Remy Croche | foot_soft | Croche Haymaker: blunt | passives / none | body_hit / body_hit + death |
| Resistance Cache | stationary/silent | no authored actions | passives / none | body_hit / body_hit + death |
| Resistance Smuggler | foot_soft | Step: movement only; Swashbuckle Blade: blade | passives / none | body_hit / body_hit + death |
| Rowan Leafbound | foot_soft | Shield Bash: blunt + metal_hit | passives / none | body_hit / body_hit + death |
| Sapling | stationary/silent | Heal: heal; Entangle: roots + magic_dark | passives / none | wood_hit / wood_hit + death |
| Scooter | swamp_squelch | River Rush: blunt + swamp_squelch; River Dash: movement only | passives / none | body_hit / body_hit + death |
| Sir Chitinous Vale | foot_soft | QueenMoveD1R2: blade + magic_dark; KingHex: magic_dark | passives / none | body_hit / body_hit + death |
| Sister Honeygrave | wing_insect | QueenFly0: movement only; ShortRangedAttack1: magic_bright | passives / none | body_hit / body_hit + death |
| Starbloom Knight | foot_soft | Starbloom Charge: blade | passives / none | body_hit / body_hit + death |
| Sun Sprite | teleport | Sunrise: movement only; Warmth: heal | passives / none | death / death + death |
| Swamp Tracker | swamp_squelch | Frogback Leap: blade | passives / none | body_hit / body_hit + death |
| Swamp Tracker Unmounted | foot_soft | Tracker's Spear: blade | passives / none | body_hit / body_hit + death |
| Sylvan Champion | foot_soft | Sunlit Advance: blade | passives / none | body_hit / body_hit + death |
| Sylvan Enchantress | foot_soft | Binding Glamour: roots + magic_bright | passives / none | body_hit / body_hit + death |
| Sylvara | roots | Sovereign Thorns: roots + magic_dark | passives / none | wood_hit / wood_hit + death |
| Telos the Merchant | foot_soft | Travel: movement only | passives / none | body_hit / body_hit + death |
| Thaeron Baelstone | foot_soft | Knife Stab: magic_dark + blade | command: power + ui_tap | body_hit / body_hit + death |
| The Choir | teleport + ghost_chorus | GhostQueenMove0: movement only; RangedHex1: magic_dark | passives / none | death / death + death |
| The Tockman | mechanism | KingMove1: blade; KingHealConstruct2: mechanism + heal | passives / none | metal_hit / metal_hit + death |
| The Weaver | mechanism | QueenMoveD1R2: mechanism + roots; RangedHex1: magic_dark | passives / none | metal_hit / metal_hit + death |
| Thorn Griffin | wing_feather | Griffin Flight: beast_bite + wing_feather | passives / none | body_hit / body_hit + death |
| Vaelthorn | roots | Thornstep: roots + wood_hit; Entangle: roots + magic_dark | passives / none | wood_hit / wood_hit + death |
| Vanya Bluewater | foot_soft | Sidestep: movement only; Blade Dance: blade | passives / none | body_hit / body_hit + death |
| Victor Greyshard | foot_soft | Axe Dance: blade | passives / none | body_hit / body_hit + death |
| Warden | foot_soft | RookMoveD1R3: blade + metal_hit; KingAttack1: blade + metal_hit | passives / none | body_hit / body_hit + death |
| Widowroot | foot_heavy | Rooted Strike: wood_hit + blunt; Widow's Bind: roots + magic_dark | passives / none | wood_hit / wood_hit + death |

Notable source differences: Thaeron has Knife Stab in data but visible hand/cane magic; the recipe keeps his dark cast signature with a low blade accent. Crystal Unicorn's Prismatic Charge animation also casts from its horn, so it uses bright bolt plus crystal rather than an assumed hoof collision. Braun carries an axe in the reviewed sprite. The Choir has three sizes sharing one spectral source. Nettle's mounted sprite has wings; movement follows that current sprite rather than an older snail description. Some generic detail prose disagrees with structured actions; structured actions govern mechanics.

## Variety without more recordings

1. Use a stable per-character pitch offset (recorded in cue-map.json) with only ±0.25 semitone random variation and ±1.2 dB gain variation. Keep UI warnings/outcomes recognizable and unrandomized. Pitch alone does not make every unit unique; material and identity layers do most of that work.
2. Light/medium/heavy weapon variants share blade with different gain and blunt/metal/wood layers. Constructs combine ratchet with metal, trees combine roots with wood, ghosts combine phase/death, flyers use insect versus broad wingbeats. Signature characters retain a recognizable layer.
3. Movement uses alternating contacts cut from the two-step masters, not the same whole clip restarted every frame. Shared action recipes select bright/dark, physical/magical, and source/target phases separately.
4. Recordings remain dry. Use tiny delays or controlled short reverb for spirits/mirrors; reverse phase/card/death tails for inverse actions, not whole reverse gunshots or reversing semantic success into failure.
5. Gate optional identity accents with a cooldown; at most one primary attack, one victim material hit and one key status accent at once. Mass healing/resource changes aggregate into one cue.
6. Preserve each master and derived slice metadata. Avoid announcing gameplay knowledge from hidden units, opponent secret draws or concealed locations. Spatialize only visible permitted events.

## Timing and integration contract

The current runtime uses accepted intents and visible snapshot deltas; stable event IDs would improve reconstruction of multiple remote actions combined into one update. Deduplicate snapshots/reconnects. Prioritize result/timer alerts over combat, combat over UI, ambient last. Runtime bus levels before cue gain and user volume: hover 18%, UI 32%, combat 40%, accents 24%, ambience 10%, alerts 46%. Hover uses a quieter, higher pitched activation sample. The WAV exports are 44.1 kHz 16-bit PCM mono for SFML SoundBuffer use. The profiles are wired into the native client; accepted local intents and visible snapshot changes drive playback.

No new recordings are needed for passive idle loops, movement within the same hover target, text typing, repeated redraws, stationary selection outlines, floating health/resource numerals, or continuously recomputed auras. These stay silent or share an existing cue only when a meaningful event occurs. Background music and dialogue voice acting are separate from this sound-effect minimum.

## Deliverables and verification

- assets/audio/sfx/manifest.json: all 41 masters, prompts, durations, provenance and completion status.
- assets/audio/sfx/cue-map.json: 97 per-card profiles, rule events, UI policy and complete animation inventory.
- assets/audio/sfx/masters/: original ElevenLabs MP3 recordings; assets/audio/sfx/wav/: normalized PCM exports.
- assets/audio/sfx/slices/: shorter timed event slices; file count here is not additional source recordings.
- output/sfx-audit/: live snapshot, coverage CSVs, progress and technical audio QA.
- assets/audio/sfx/preview.html: labeled audio browser; audition-reel.mp3: ordered whole-library preview.

Technical validation checks completion, decoder success, non-silent audio, finite samples, peak headroom, durations, file hashes and complete cue references. It does not substitute for a human listening pass or prove perceptual quality of every AI-generated recording. Runtime verification uses the Windows client build, offline audio/game/story tests and SFML audio-device startup checks. See SOUND_EFFECTS_INTEGRATION.md for results.
