import fs from 'node:fs';
import path from 'node:path';

const root = process.cwd();
const out = path.join(root, 'output/sfx-audit');
const library = path.join(root, 'assets/audio/sfx');
const snapshot = JSON.parse(fs.readFileSync(path.join(out,'catalog.json'), 'utf8').replace(/^\uFEFF/,''));
const manifest = JSON.parse(fs.readFileSync(path.join(library,'manifest.json'), 'utf8').replace(/^\uFEFF/,''));
const ids = new Set(manifest.masters.map(m => m.id));
const group = s => new Set(s.split('|'));
const has = (set, title) => set.has(title);
const insects = group('Archduchess Vespara|Caltheriel|Duchess Dewbell|Duke Lanternwing|Gloom Fairy|Hushkeeper|Prince Vesper|Queen Nyxara|Sister Honeygrave|Fey Messenger');
const birds = group('Thorn Griffin|Moonshade Stalker|Quinberry Lark|Pipistrel Vane');
const trees = group('Archivist Mosswake|Ashenfang|Briar Whisperthorn|Blightling|Eyeblight|Grove Sister|Heartshoot|Heartwood|Heartwood Sister|Marrowind|Sylvara|Vaelthorn|Widowroot|Sapling');
const machines = group('Clockwork Guardian|Gearjaw|Lumber Automaton|The Tockman|The Weaver');
const spectral = group('Elliot Greentide|Mourning Light|The Choir|Sun Sprite');
const wet = group('Bull Gator|Scooter|Swamp Tracker|Father Grub|Lady Silkmaw|Bristlejack');
const immobile = group('Black Powder Keg|Gilded Cage|Fairy Ring|Hidden Camp|Resistance Cache|Sapling');
const heavy = group('Mog|Grask|Mossy Giant|Maggie Mudroot|Marrowind|Widowroot|Heartwood');
const bright = group('Archivist Mosswake|Caltheriel|Donella of the Marsh|Duchess Dewbell|Fey Messenger|Heartwood Sister|Joni Pumpernickel|Juniper Flash|Prince Vesper|Sylvan Enchantress|Sun Sprite|Telos the Merchant|Sister Honeygrave');
const crystal = group('Crystal Unicorn|Glimmer Stag|Lady Mirrorglace');
const attackOverrides = {
  'Black Powder Keg':['fire','explosion'], 'Bristlejack':['beast_bite','blunt'],
  'Bull Gator':['beast_bite'], 'Maggie Mudroot':['beast_bite','creak'],
  'Clockwork Guardian':['mechanism','metal_hit'], 'Gearjaw':['mechanism','metal_hit'],
  'Lumber Automaton':['mechanism','blunt'], 'Mossy Giant':['blunt','wood_hit'],
  'Braun Stonefist':['blade','blunt'], 'Remy Croche':['blunt'],
  'Nettle Starbright Unmounted':['blunt'], 'Rowan Leafbound':['blunt','metal_hit'],
  'Crystal Unicorn':['magic_bright','crystal'], 'Glimmer Stag':['blunt','crystal'],
  'Archduchess Vespara':['magic_dark','wing_insect'], 'The Choir':['ghost_chorus','magic_dark'],
  'Thorn Griffin':['beast_bite','wing_feather'], 'Moonshade Stalker':['beast_bite','wing_feather'],
  'Scooter':['blunt','swamp_squelch'], 'Duke Lanternwing':['magic_bright','fire'],
  'Lady Silkmaw':['roots','magic_dark'], 'The Weaver':['mechanism','roots'],
  'Blackthorn Engineer':['mechanism','blunt'], 'Pipistrel Vane':['shriek','wing_feather'],
  'Thaeron Baelstone':['magic_dark','blade'], 'Briar Whisperthorn':['magic_dark','roots'],
  'Father Grub':['blunt','swamp_squelch'], 'Blackthorn Foreman':['blade','mechanism'],
  'Marrowind':['wood_hit','blunt'], 'Widowroot':['wood_hit','blunt'],
  'Sir Chitinous Vale':['blade','magic_dark'], 'Warden':['blade','metal_hit']
};
const layer = (id, gain = 0.7, at = 0) => ({id,gain,at_seconds:at});
function movement(c) {
  const t = c.title;
  if (has(immobile,t)) return {layers:[],policy:'Silent while stationary; reuse creak/metal_hit/card_paper if a visible interaction moves this object.'};
  let sound = 'foot_soft';
  if (has(trees,t)) sound='roots';
  if (has(heavy,t)) sound='foot_heavy';
  if (has(wet,t)) sound='swamp_squelch';
  if (has(machines,t)) sound='mechanism';
  if (has(insects,t)) sound='wing_insect';
  if (has(birds,t)) sound='wing_feather';
  if (has(spectral,t)) sound='teleport';
  if (t==='Crystal Unicorn'||t==='Glimmer Stag') sound='hoof_crystal';
  if (t==='Nettle Starbright') sound='wing_insect'; // mounted animation has a winged mount
  const sounds=[layer(sound,0.48)];
  if(t==='Maggie Mudroot') sounds.push(layer('creak',0.22));
  if(t==='Fizzlewick Gearwright') sounds.push(layer('mechanism',0.2));
  if(t==='The Choir') sounds.push(layer('ghost_chorus',0.13));
  if(t==='Lady Mirrorglace') sounds.push(layer('crystal',0.2));
  return {layers:sounds,policy:'Movement only; schedule short contacts/wingbeats within the 0.95 second travel, do not loop the full master once per animation frame.'};
}
function attack(c,a,i) {
  const n = c.actionDisplayNames[i] || a.name, t=c.title;
  let sounds=[];
  if (a.heal>0) {
    sounds=/repair|rebuild|maintenance|construct/i.test(n+a.name)||has(machines,t)?['mechanism','heal']:['heal'];
    if(t==='Blackthorn Alchemist'||t==='Camp Physician') sounds.push('potion');
  } else if (a.control>0) sounds = t==='Pavo Quickstep'?['charm_pipes']:['charm_pipes',has(crystal,t)?'crystal':'magic_bright'];
  else if (/bow|slingshot|longbow/i.test(n)) sounds=['bow'];
  else if (/spear/i.test(n)) sounds=['blade'];
  else if (/thorn bolt/i.test(n)) sounds=['bow','roots'];
  else if (/star dart/i.test(n)) sounds=['magic_bright'];
  else if (t==='Goblin Sharpshooter'&&a.canAttack) sounds=['gun'];
  else if (/ghostly volley/i.test(n)) sounds=['gun','magic_dark'];
  else if (/sonic|shriek/i.test(n)) sounds=['shriek'];
  else if (a.infest) sounds=['wing_insect','magic_dark','roots'];
  else if (/potion|elixer|elixir/i.test(n)) sounds=['potion'];
  else if (/mirror|reflection/i.test(n)) sounds=['crystal','teleport'];
  else if (/burn/i.test(n)) sounds=['magic_bright','fire'];
  else if (a.statusTurns>0) {
    if(has(trees,t)||/bind|entang|thread|thorn/i.test(n)) sounds=['roots',has(bright,t)?'magic_bright':'magic_dark'];
    else if(t==='Gilded Cage') sounds=['metal_hit','magic_dark'];
    else sounds=[has(bright,t)?'magic_bright':'magic_dark'];
  } else if (!a.canAttack) {
    if(a.kind==='teleport'||a.kind==='assault step'||a.kind==='rally step'||a.kind==='tunnel') sounds=[a.kind==='tunnel'?'swamp_squelch':'teleport'];
    else return {name:n,internal_name:a.name,kind:a.kind,layers:[],policy:'Use movement profile only; no attack sound.'};
  } else if(attackOverrides[t]) sounds=attackOverrides[t];
  else if(a.kind==='ranged') sounds=[has(bright,t)?'magic_bright':'magic_dark'];
  else if(has(trees,t)) sounds=['roots','wood_hit'];
  else if(has(bright,t)) sounds=['magic_bright'];
  else sounds=['blade'];
  if(a.pull||a.push) sounds.push(t==='Bog Spearman'?'blade':'teleport');
  if(a.lifedrain) sounds.push('death','heal');
  sounds=[...new Set(sounds)];
  return {name:n,internal_name:a.name,kind:a.kind,layers:sounds.map((id,j)=>layer(id,j===0?0.72:0.25,j===0?0:0.06)),policy:a.heal?'Play restoration only for actual Health gain; an accepted cast with unchanged Health uses soft magic_bright casting feedback.':a.lifedrain?'Play heal layer at source only for actual restored health, plus one victim impact.':'Align primary transient to visible attack at ~0.18 seconds in the current 0.42 second animation; use attack slices, allow natural tail after animation.',extra:{repeat:a.repeat||0,push:a.push||0,pull:!!a.pull,infest:a.infest||'',control:a.control||0,lifedrain:!!a.lifedrain}};
}
const abilitySounds = {
  'dig':['swamp_squelch','wood_hit'], 'transform':['mechanism','teleport'],
  'dematerialize':['teleport'], 'summon':['teleport','power'], 'shriek':['shriek'],
  'silence field':['steam','magic_dark'], 'power boost':['power'], 'command':['power','ui_tap'],
  'swap':['teleport','card_paper'], 'raise undead':['death','heal','teleport']
};
const profiles = snapshot.cards.map(c=>{
  const t=c.title;
  let damage = has(machines,t)||t==='Gilded Cage'?'metal_hit':has(trees,t)||t==="Maggie's Recruit"?'wood_hit':has(spectral,t)?'death':'body_hit';
  const ability = (c.strings.ability||'').trim().toLowerCase();
  const sacrifice = (c.strings.Sacrifice||c.strings.sacrifice||'').trim().toLowerCase();
  const hash = [...t].reduce((a,v)=>((a*31+v.charCodeAt(0))>>>0),0);
  const notes=[];
  if(t==='Thaeron Baelstone')notes.push('Live action Knife Stab conflicts with magical sprite: use magic_dark primary and quiet blade detail. Existing preview remains available. Command is a separate power cue.');
  if(t==='Nettle Starbright')notes.push('Mounted and unmounted forms have different movement and attack profiles; mounted sprite has wings although older design prose describes a snail.');
  if(t==='Black Powder Keg')notes.push('Live Fuse Strike remains distinct from design-only area Detonate; current animation visibly explodes, so explosion is appropriate to sprite.');
  if(t==='The Choir')notes.push('Large/medium/small animation variants reuse ghost_chorus with stable lower/normal/higher pitch and different gain, not separate recordings.');
  if(t==='Gilded Cage')notes.push('Golden Bind is a disable in live data, not implemented capture-beneath-card from design prose.');
  if(!c.actions.length)notes.push('No authored action list: do not invent an attack cue; use visible deploy/passive/ability events only.');
  return {title:t,type:c.type,traits:c.traits,keywords:c.keywords,identity_pitch_semitones:((hash%7)-3)*0.45,random_pitch_semitones:0.25,random_gain_db:1.2,movement:movement(c),actions:c.actions.map((a,i)=>attack(c,a,i)),ability:ability?{name:ability,layers:(abilitySounds[ability]||['power']).map(id=>layer(id)),uses_fallback:!abilitySounds[ability]}:null,sacrifice:sacrifice?{name:sacrifice,layers:['death',...(abilitySounds[sacrifice]||['power'])].map(id=>layer(id))}:null,deploy:[layer('card_paper',0.25),layer('teleport',0.55),...(has(machines,t)?[layer('mechanism',0.2)]:has(trees,t)?[layer('roots',0.2)]:[])],damage:[layer(damage,0.6)],death:[layer(damage,0.4),layer('death',0.55)],animations:Object.fromEntries(Object.entries(c.strings).filter(([k])=>/^(Walk|Idle|Attack|Damaged|Killed|Fidget)Anim$/.test(k))),notes};
});

const rules = [
 ['deploy','card_paper + teleport + material accent','Confirmed visible placement; one event per batch, never on failed drag.'],
 ['draw/discard/shuffle/foresight','card_paper; crystal only when a choice is revealed','Paper one-shot per actual card transition, rate limited on multi-draw.'],
 ['turn start / extra action / relentless','ui_confirm; power for a granted extra action','Turn cue only for local player. No sound for every ready flag update.'],
 ['resources / tax / gather / resourceDrain','resource_coins','Aggregate deltas once per turn/event; reverse or lower pitch for spend/drain, distinct from real-money purchases.'],
 ['damage / counterstrike / bristle / collision','body_hit, metal_hit or wood_hit according to victim','Impact once per visible hit, counterattack uses attacker cue once; cap simultaneous victims.'],
 ['healing aura / lifedrain / cleanse','heal; potion or mechanism overlay if physical','Only positive actual healing; suppress healed-zero/no-op and rot-blocked blooms.'],
 ['bodyguard / intercept / shelter','metal_hit + quiet power; teleport for an actual swap','Only when protection changes the result; no sound on passive adjacency alone.'],
 ['reveal / foresight / mark','crystal + card_paper or teleport','Only information that client is allowed to know. Hidden actions cannot leak location or identity.'],
 ['swarm / Power Boost / enchant damage','power + identity accent','Play only activation/threshold change, not continuous derived recomputation.'],
 ['entangle / stun / sleep / grow wait','roots or magic_dark; charm_pipes for sleep','Apply once; no loop on disabled pieces; removal uses a soft heal/crystal tail.'],
 ['control / charm / return ownership','charm_pipes + card_paper; ui_confirm on local regained control','Distinguish control expiration with softer reversed/tail variant.'],
 ['infestation / rebirth / raised exile','wing_insect + roots; death + teleport; death only on exile','Replacement is one death plus one spawn; avoid playing a second generic kill.'],
 ['dig / tunnel / jump / hop / push / pull','swamp_squelch or teleport + landing material','No loud takeoff for ordinary slide; collision impact only if a collision occurs.'],
 ['transform / aim / lower weapon','mechanism for a physical stance; teleport + identity for magical form','Aim is a ratchet/string creak, not a gunshot.'],
 ['sacrifice','death + payload cue (power/heal/explosion/teleport)','Payload at resolution, death once. Do not play an ordinary hostile hit on activation.'],
 ['silence field / rot aura','steam + low magic_dark tail','Supported reusable rules, no live catalog bearer currently; one onset/expiry, no permanent hum required.'],
 ['enchant player/square/piece','power + resource_coins or magic_dark','Covers runtime damage/resources/resourceDrain enchantment types; expiration uses death tail.'],
 ['spell damage/heal/resources/steam','magic_bright or magic_dark / heal / resource_coins / steam','Only four supported spell effect names. Undefined spells receive ui_reject and no cast/impact.'],
 ['timeout / resignation / match end','alert_bell then victory/defeat','Outcome once after authoritative result, cancel queued combat sound tails.'],
 ['resurrection to hand / evacuate to hand','death reversed + card_paper + optional heal','Raise Undead returns one-health card to hand in current rules; do not play a board spawn until deployment.']
].map(([event,recipe,policy])=>({event,recipe,policy}));

const effects = [
 ['Ambush','ambush.png','card_paper + blade + beast_bite growl','Design/art only; resolve via eventual effect definition, not title matching.'],
 ['Rapid Assembly','rapidAssembly.png','mechanism + teleport','Mechanical build/summon texture.'],
 ['Supply Shortage','supplyShortage.png','resource_coins lowered/reversed + ui_reject','Resource-denial identity.'],
 ['Survival Instinct','survivalInstinct.png','beast_bite growl + power + heal','Short protective instinct response.'],
 ['Hidden Path','hiddenPath.png','teleport + roots','Concealment/relocation, no ordinary damage impact.'],
 ['Spell Surge','spellSurge.png','power + magic_bright','Brief arcane empowerment pulse.'],
 ['Ancient Awakening','ancientAwakening.png','roots + foot_heavy + heal','Old wood stirs into life.'],
 ['Cursed Ground','cursedGround.png','magic_dark + roots','Curse onset; damage only when it actually resolves.'],
 ['Raise the Fallen','raiseTheFallen.png','death reversed + teleport + heal','Resurrection identity, actual destination follows rules.'],
 ['Last Stand','lastStand.png','metal_hit + power','Defiant protection/rally, no separate fanfare.']
].map(([title,art,recipe,notes])=>({title,art:'assets/cards/'+art,status:'Artwork/design present; absent from live 97-card snapshot',recipe,notes}));

const ui = [
 ['All menus/forms','Button, keyboard activation, checkbox, tab, dropdown','ui_tap','Audible quiet pitched hover on target entry; firmer activation. No hover repeat while stationary; click takes precedence in the same frame.'],
 ['Forms / text','Focus, typing, caret, selection, password visibility','ui_tap for focus/toggle; otherwise silence','No keystroke chatter or sound exposing password length.'],
 ['Options','Sliders, resolution, apply, mute, save','ui_tap at release/detents; ui_confirm on saved','No continuous drag noise; use preview cue only if audio is enabled.'],
 ['Login/account','Login, create account, change password, logout','ui_tap + ui_confirm/ui_reject','Acknowledge server success or failure once.'],
 ['Navigation','Back/cancel, popup open/close, scroll, tooltips','pitched/reversed card_paper for panels; quiet ui_tap for changed list offsets','Open/close and card inspection announce transitions. Redraws and scroll limits stay silent; cancel is not an error.'],
 ['Deck/collection','New/edit/delete, search/filter, inspect, add/remove, drag/drop, unsaved prompt','card_paper + ui_tap; ui_confirm on save; ui_reject invalid','Reorder/remove can use reversed paper, no separate recordings.'],
 ['Shop/starter decks','Purchase, coin pack, claim deck, reveal, rarity','resource_coins + card_paper + crystal; ui_confirm','Only verified purchase completion. Rarity changes layering/timing, not a new master per rarity.'],
 ['Matchmaking/sandbox','Find/cancel, loading, match found, connect/disconnect','ui_tap; alert_bell + ui_confirm when ready; ui_reject on failure','No looping search beep; ready cue must remain recognizable.'],
 ['Board selection','Piece/card hover, inspect, select/deselect, action choice','quiet ui_tap/card_paper','No attack sound until accepted action resolves.'],
 ['Board gestures','Legal move/deploy, drag cancel, invalid target, power/sacrifice confirmation','Movement/deploy/ability recipe; ui_reject for invalid','Avoid button + generic attack + ability triple triggering.'],
 ['Card systems','Hand fan, draw/discard, foresight choice, graveyard, raise undead','card_paper + appropriate magic cue','Draw/reveal should not leak concealed cards to another player.'],
 ['Battle status','Start/end turn, command selection, timer 120/60/30s, result','ui_confirm/power; alert_bell; victory/defeat','Use existing clock-warning tracker; at most one warning per crossed threshold.'],
 ['Story menus/dialogue','Campaign/mission select, next/back/skip, choices, instructions','card_paper + ui_tap; ui_confirm at completed objective','Narrator text is silent; no generated speech needed.'],
 ['Story world beats','Gate/cage, raft/boat/flood, fire/fuse, collapse/explosion, signal bell','creak/metal_hit; water; fire; wood_hit/explosion; alert_bell','Authored one-shot at visible story beat; no automatic keyword-triggered narration effects.'],
 ['Conquest','Event list/create/join/ready/start/end, army setup, placement, orders/undo/exit','ui_tap + card_paper; ui_confirm; ui_reject','Strategic movement reuses foot_soft/foot_heavy; capture power; casualties death; awards resource_coins.'],
 ['Conquest timing','Deadline, battle becomes available, eliminated, final winner','alert_bell; ui_confirm; defeat/victory','Only meaningful new state once; suppress repeated refresh notices.'],
 ['Admin/card editor','New/copy/save/delete cards/actions, filters, lists, asset pick, grants','ui_tap + card_paper; ui_confirm/ui_reject','All CRUD and confirmations reuse player UI vocabulary; no admin-only sound pack.'],
 ['Network recovery','Disconnected/rejected request, reconnect, resumed state','one alert_bell or ui_reject; ui_confirm on recovered','Snapshot resync is silent: do not replay all old summons/damage/deaths.'],
 ['Ambient/idle','Menu/map/story environmental bed, idle and fidget','optional bayou/fire; idle silent','Current fidgets disabled. If enabled later, use sparse material/cloth/wing accents only for visible actor; do not add a sound every loop.']
].map(([surface,interaction,recipe,policy])=>({surface,interaction,recipe,policy}));

function filesIn(dir) { return fs.readdirSync(dir,{withFileTypes:true}).flatMap(e=>e.isDirectory()?filesIn(path.join(dir,e.name)):[path.join(dir,e.name)]); }
const animationFiles=fs.readdirSync(path.join(root,'assets/animations')).filter(f=>f.endsWith('.png'));
const aliases = {'dark-fairy':'Gloom Fairy','vaelorin':'Vaelthorn','heartwoodTree':'Heartwood','fairyCircle':'Fairy Ring','GT_animation_gloomFairy_idle':'Gloom Fairy','theChoir_small':'The Choir','theChoir_med':'The Choir','fizzlewickGearwright_mounted':'Fizzlewick Gearwright'};
const animations=animationFiles.map(f=>{
 const stem=f.replace(/\.png$/,'').replace(/-(?:walk|idle|attack\d*|damaged|killed|fidget|aim|lower-weapon|lowerWeapon)$/,'');
 const state=(f.match(/-(walk|idle|attack\d*|damaged|killed|fidget|aim|lower-weapon|lowerWeapon)\.png$/)||[])[1]||'idle';
 const matches=profiles.filter(p=>Object.values(p.animations).some(v=>path.basename(v).toLowerCase()===f.toLowerCase())||p.title===aliases[stem]);
 const p=matches[0];
 const fallback = /gear|clock|autom|tock|weaver/i.test(stem)?'mechanism':/wood|root|moss|sylvar|grove/i.test(stem)?'roots':/fairy|wing|vespar|nyxar/i.test(stem)?'wing_insect':'foot_soft';
 let recipe;
 if(state==='idle')recipe='silent';
 else if(state==='fidget')recipe='silent currently (disabled); optional '+fallback;
 else if(/aim|lower/i.test(state))recipe='mechanism + quiet bow string detail; never gunshot';
 else if(state==='walk')recipe=p?p.movement.layers.map(l=>l.id).join(' + ')||'silent':fallback;
 else if(state==='damaged')recipe=p?p.damage.map(l=>l.id).join(' + '):'body_hit/material variant';
 else if(state==='killed')recipe=p?p.death.map(l=>l.id).join(' + '):'death + material variant';
 else recipe=p?p.actions.filter(a=>a.layers.length).map(a=>a.layers.map(l=>l.id).join(' + ')).join(' / ')||'ability-only; no ordinary attack':'Use matching family attack; legacy art has no live action binding';
 return {file:'assets/animations/'+f,state,cards:matches.map(p=>p.title),status:matches.length?'catalog-bound or explicit art alias':'unbound/alternate repository animation',recipe};
});

const sourceFiles=filesIn(path.join(root,'src/client')).filter(p=>/\.(cpp|hpp|cppm|inl)$/.test(p));
const controls=[];
for(const file of sourceFiles){
 const lines=fs.readFileSync(file,'utf8').split(/\r?\n/);
 for(let i=0;i<lines.length;i++){
  const line=lines[i];
  if(/\bButton\s+\w+\s*\(|\bEditorButton\(|\bdrawButton\(|\b(?:TabStrip|SliderControl|CheckboxControl)\s+\w+\s*\(/.test(line))
   controls.push({source:path.relative(root,file).replaceAll('\\','/'),line:i+1,code:line.trim(),cue:'ui_tap; outcome may add ui_confirm/ui_reject; content motion may use card_paper'});
 }
}
const screensText=fs.readFileSync(path.join(root,'src/client/main.cpp'),'utf8');
const screens=screensText.match(/enum class GameState\s*\{([\s\S]*?)\}/)[1].split(',').map(v=>v.trim()).filter(Boolean);
const captureText=fs.readFileSync(path.join(root,'src/client/client_ui_capture.cpp'),'utf8');
const captureStatic=[...captureText.slice(captureText.indexOf('knownScreens()')).matchAll(/"([a-z][a-z0-9-]+)"/g)].map(m=>m[1]);
const references = [...new Set(profiles.flatMap(p=>[...p.movement.layers,...p.damage,...p.death,...p.deploy,...p.actions.flatMap(a=>a.layers),...(p.ability?.layers||[]),...(p.sacrifice?.layers||[])]).map(l=>l.id))];
for(const id of references)if(!ids.has(id))throw Error('Missing master '+id);
if(profiles.length!==snapshot.count||new Set(profiles.map(p=>p.title)).size!==snapshot.count)throw Error('Catalog coverage mismatch');
const result={version:1,scope:{snapshot_source:snapshot.source,snapshot_utc:snapshot.fetchedAt,live_cards:snapshot.count,live_actions:snapshot.cards.reduce((n,c)=>n+c.actions.length,0),unique_internal_actions:new Set(snapshot.cards.flatMap(c=>c.actions.map(a=>a.name))).size,local_database_cards:0,effect_artworks:effects.length,animation_files:animations.length,states:screens,static_capture_keys:[...new Set(captureStatic)],ui_control_call_sites:controls.length},profiles,rule_events:rules,effect_cards:effects,ui,animation_files:animations,ui_control_call_sites:controls};
fs.writeFileSync(path.join(library,'cue-map.json'),JSON.stringify(result,null,2));
const csv=(rows,fields)=>[fields.join(','),...rows.map(r=>fields.map(k=>'"'+String(r[k]??'').replaceAll('"','""')+'"').join(','))].join('\n');
fs.writeFileSync(path.join(out,'animation-coverage.csv'),csv(animations.map(a=>({...a,cards:a.cards.join('; ')})),['file','state','cards','status','recipe']));
fs.writeFileSync(path.join(out,'ui-control-inventory.csv'),csv(controls,['source','line','code','cue']));
fs.writeFileSync(path.join(out,'card-sound-map.csv'),csv(profiles.map(p=>({title:p.title,movement:p.movement.layers.map(l=>l.id).join(' + '),actions:p.actions.map(a=>a.name+': '+a.layers.map(l=>l.id).join(' + ')).join('; '),ability:p.ability?.name||'',ability_sounds:p.ability?.layers.map(l=>l.id).join(' + ')||'',sacrifice:p.sacrifice?.name||'',damage:p.damage.map(l=>l.id).join(' + '),death:p.death.map(l=>l.id).join(' + '),notes:p.notes.join(' ')})),['title','movement','actions','ability','ability_sounds','sacrifice','damage','death','notes']));

const table=(headers,rows)=>'| '+headers.join(' | ')+' |\n| '+headers.map(()=>'---').join(' | ')+' |\n'+rows.map(row=>'| '+row.map(v=>String(v??'').replaceAll('|','/').replaceAll('\n',' ')).join(' | ')+' |').join('\n');
const report=`# Bayou Bonanza sound audit and compact library\n\n41 master recordings cover the current game and the repository's ten effect-card artworks. 38 are new ElevenLabs Sound Effects v2 recordings; three reuse the Bull Gator, Crystal Unicorn and Thaeron previews. This is a practical production minimum, not a mathematical lower bound: fewer files are possible if weapon, material, movement or notification identities are collapsed. The sounds and usage recipes are integrated into the native client. See SOUND_EFFECTS_INTEGRATION.md and SOUND_EFFECTS_INTERACTION_COVERAGE.md for runtime coverage and verification.\n\n## Evidence and scope\n\n- Read-only live catalog snapshot: ${snapshot.source}, ${snapshot.fetchedAt}, schema ${snapshot.version}; ${snapshot.count} cards (${snapshot.cards.filter(c=>c.type==='Hero').length} Hero / ${snapshot.cards.filter(c=>c.type==='Unit').length} Unit), ${result.scope.live_actions} action assignments and ${result.scope.unique_internal_actions} unique internal action definitions. Full snapshot: output/sfx-audit/catalog.json. Local cards.db has zero rows and was not used as the gameplay truth.\n- All ${screens.length} GameState values, reusable controls, input events, screen fragments, shop, deck editor, story, Conquest, admin and card editor were inventoried from source. ${controls.length} static control call sites are indexed in output/sfx-audit/ui-control-inventory.csv; dynamic list rows and keyboard/drag behavior are covered by the interaction families below. This is a source and asset audit, not a claim to have manually clicked every possible online state.\n- ${animations.length} animation strips indexed in output/sfx-audit/animation-coverage.csv, including walking, attack, damage, death, idle, fidget and aim/lower variants. Six representative frames from all 99 attack strips were visually reviewed in attack-atlas-1..4.png; other states are mapped from their actual file binding, material and animation role.\n- src/client/main.cpp:70 currently defines nine AudioCue values. The snapshot-diff audio route at ~5225-5450 coalesces generic place/move/attack/death, so simply adding files cannot deliver the per-action variety in this plan. Current playback supports volume but needs future event routing and pitch/offset selection.\n- src/client/client_board_layout.hpp:42-43: attack 0.42 seconds, damage/death reaction 0.55 seconds. src/client/main.cpp:529: move 0.95 seconds. Fidgets are explicitly disabled at main.cpp:528. Full generated masters may ring beyond the animation; short attack/contact slices are exported for timing.\n- src/shared/game_data.hpp, src/shared/game_rules.hpp and src/gameserver/game_engine.hpp define the supported ability, spell, enchantment and passive mechanics. docs/AURAS.md explicitly notes no current catalog assignments for Rot Aura/Silence Field.\n- design/GT Card Abilities.docx and design/Card Abilities, traits, etc.docx contain alternatives/proposals, sometimes different from live rules. They inform reusable future coverage, not claims of implementation. Design ability text was extracted to output/sfx-audit/design-abilities.txt.\n\n## Minimum sound list\n\n${table(['#','ID','Sound','Seconds','Reuse'],manifest.masters.map(m=>[m.number,m.id,m.name,m.duration_seconds,m.uses]))}\n\n## UI and system interactions\n\n${table(['Surface','Interaction','Sound','Trigger policy'],ui.map(u=>[u.surface,u.interaction,u.recipe,u.policy]))}\n\nAll game states: ${screens.join(', ')}.\n\n## Rules, abilities and statuses\n\n${table(['Event','Reusable sound recipe','Timing/conditions'],rules.map(r=>[r.event,r.recipe,r.policy]))}\n\nEvery action on every live card has its own semantic recipe in cue-map.json and the table below. This includes heal actions that share an attack gesture, status-only actions, action repeats, control, Lifedrain, push/pull, infestation and form changes. Healing is not treated as damage just because canAttack is true.\n\n## Effect cards\n\nThe following ten artworks are present but no Spell/Enchantment is returned by this live snapshot. Sound recipes are ready; undefined effects must remain silent except for rejection until supported mechanics are assigned. Heartshoot is currently a live Plant Unit, not a Spell; the table below follows the server.\n\n${table(['Effect card','Recipe','Status / use'],effects.map(e=>[e.title,e.recipe,e.status+'. '+e.notes]))}\n\nOther proposed effects require no new source recordings: barriers use crystal+metal_hit; time warp uses mechanism+teleport; ice/mirror uses crystal+teleport; fear uses shriek+magic_dark; poison uses potion+magic_dark; terrain uses roots/water; mines use fire+explosion; information effects use crystal+card_paper; healing and protective effects use heal+power; construct repair uses mechanism+heal.\n\n## Per-character coverage\n\n${table(['Character','Movement','Actions','Ability','Damage / death'],profiles.map(p=>[p.title,p.movement.layers.map(l=>l.id).join(' + ')||'stationary/silent',p.actions.map(a=>a.name+': '+(a.layers.map(l=>l.id).join(' + ')||'movement only')).join('; ')||'no authored actions',p.ability?p.ability.name+': '+p.ability.layers.map(l=>l.id).join(' + '):'passives / none',p.damage[0].id+' / '+p.death.map(l=>l.id).join(' + ')]))}\n\nNotable source differences: Thaeron has Knife Stab in data but visible hand/cane magic; the recipe keeps his dark cast signature with a low blade accent. Crystal Unicorn's Prismatic Charge animation also casts from its horn, so it uses bright bolt plus crystal rather than an assumed hoof collision. Braun carries an axe in the reviewed sprite. The Choir has three sizes sharing one spectral source. Nettle's mounted sprite has wings; movement follows that current sprite rather than an older snail description. Some generic detail prose disagrees with structured actions; structured actions govern mechanics.\n\n## Variety without more recordings\n\n1. Use a stable per-character pitch offset (recorded in cue-map.json) with only ±0.25 semitone random variation and ±1.2 dB gain variation. Keep UI warnings/outcomes recognizable and unrandomized. Pitch alone does not make every unit unique; material and identity layers do most of that work.\n2. Light/medium/heavy weapon variants share blade with different gain and blunt/metal/wood layers. Constructs combine ratchet with metal, trees combine roots with wood, ghosts combine phase/death, flyers use insect versus broad wingbeats. Signature characters retain a recognizable layer.\n3. Movement uses alternating contacts cut from the two-step masters, not the same whole clip restarted every frame. Shared action recipes select bright/dark, physical/magical, and source/target phases separately.\n4. Recordings remain dry. Use tiny delays or controlled short reverb for spirits/mirrors; reverse phase/card/death tails for inverse actions, not whole reverse gunshots or reversing semantic success into failure.\n5. Gate optional identity accents with a cooldown; at most one primary attack, one victim material hit and one key status accent at once. Mass healing/resource changes aggregate into one cue.\n6. Preserve each master and derived slice metadata. Avoid announcing gameplay knowledge from hidden units, opponent secret draws or concealed locations. Spatialize only visible permitted events.\n\n## Timing and integration contract\n\nThe current runtime uses accepted intents and visible snapshot deltas; stable event IDs would improve reconstruction of multiple remote actions combined into one update. Deduplicate snapshots/reconnects. Prioritize result/timer alerts over combat, combat over UI, ambient last. Runtime bus levels before cue gain and user volume: hover 18%, UI 32%, combat 40%, accents 24%, ambience 10%, alerts 46%. Hover uses a quieter, higher pitched activation sample. The WAV exports are 44.1 kHz 16-bit PCM mono for SFML SoundBuffer use. The profiles are wired into the native client; accepted local intents and visible snapshot changes drive playback.\n\nNo new recordings are needed for passive idle loops, movement within the same hover target, text typing, repeated redraws, stationary selection outlines, floating health/resource numerals, or continuously recomputed auras. These stay silent or share an existing cue only when a meaningful event occurs. Background music and dialogue voice acting are separate from this sound-effect minimum.\n\n## Deliverables and verification\n\n- assets/audio/sfx/manifest.json: all 41 masters, prompts, durations, provenance and completion status.\n- assets/audio/sfx/cue-map.json: 97 per-card profiles, rule events, UI policy and complete animation inventory.\n- assets/audio/sfx/masters/: original ElevenLabs MP3 recordings; assets/audio/sfx/wav/: normalized PCM exports.\n- assets/audio/sfx/slices/: shorter timed event slices; file count here is not additional source recordings.\n- output/sfx-audit/: live snapshot, coverage CSVs, progress and technical audio QA.\n- assets/audio/sfx/preview.html: labeled audio browser; audition-reel.mp3: ordered whole-library preview.\n\nTechnical validation checks completion, decoder success, non-silent audio, finite samples, peak headroom, durations, file hashes and complete cue references. It does not substitute for a human listening pass or prove perceptual quality of every AI-generated recording. Runtime verification uses the Windows client build, offline audio/game/story tests and SFML audio-device startup checks. See SOUND_EFFECTS_INTEGRATION.md for results.\n`;
fs.writeFileSync(path.join(root,'docs/SOUND_EFFECTS_AUDIT.md'),report);
console.log(JSON.stringify({cards:profiles.length,actionAssignments:result.scope.live_actions,animations:animations.length,uiControlSites:controls.length,states:screens.length,effectCards:effects.length,masters:ids.size,unknownAbilities:profiles.filter(p=>p.ability?.uses_fallback).map(p=>p.title)}));
