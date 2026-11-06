#include "client_audio_plan.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <tuple>

namespace bayou::client::audio
{
#include "client_audio_data.inl"

const SoundProfile* profile(std::string_view title)
{
    const auto found = std::find_if(profiles().begin(), profiles().end(),
        [title](const auto& p) { return p.title == title; });
    return found == profiles().end() ? nullptr : &*found;
}

std::vector<Layer> abilityRecipe(std::string_view value)
{
    const std::string name = game_data::normalizedAbility(std::string(value));
    if (name == "dig") return {{"swamp_squelch"}, {"wood_hit", .25f}};
    if (name == "transform") return {{"mechanism"}, {"teleport", .25f}};
    if (name == "dematerialize" || name == "swap") return {{"teleport"}};
    if (name == "summon") return {{"teleport"}, {"power", .25f}};
    if (name == "shriek") return {{"shriek"}};
    if (name == "silence field") return {{"steam"}, {"magic_dark", .25f}};
    if (name == "raise undead") return {{"death", .5f}, {"heal", .4f}, {"teleport", .3f}};
    return {{"power"}};
}

std::vector<Layer> effectRecipe(const game_data::GameCard& card)
{
    // Undefined effects remain rejected. Artwork/title never enables a rule.
    if (card.type == "Spell" && !game_data::isSupportedSpellEffect(card)) return {};
    if (card.effect == "heal") return {{"heal"}};
    if (game_data::isResourcesEffect(card))
        return card.effect == "steam" ? std::vector<Layer>{{"steam"}, {"resource_coins", .3f}}
                                      : std::vector<Layer>{{"resource_coins"}};
    if (card.effect == "resourceDrain") return {{"magic_dark", .4f}, {"resource_coins", .4f}};
    // Once defined, these effect-card artworks retain their reviewed identity.
    if (card.title == "Ambush") return {{"blade"}, {"beast_bite", .25f}};
    if (card.title == "Rapid Assembly") return {{"mechanism"}, {"teleport", .3f}};
    if (card.title == "Supply Shortage") return {{"resource_coins", .45f}, {"ui_reject", .25f}};
    if (card.title == "Survival Instinct") return {{"power"}, {"beast_bite", .2f}};
    if (card.title == "Hidden Path") return {{"teleport"}, {"roots", .25f}};
    if (card.title == "Spell Surge") return {{"power"}, {"magic_bright", .3f}};
    if (card.title == "Ancient Awakening") return {{"roots"}, {"foot_heavy", .3f}};
    if (card.title == "Cursed Ground") return {{"magic_dark"}, {"roots", .3f}};
    if (card.title == "Raise the Fallen") return {{"death", .4f}, {"teleport", .5f}, {"heal", .4f}};
    if (card.title == "Last Stand") return {{"power"}, {"metal_hit", .35f}};
    return {{card.effect == "damage" ? "magic_bright" : "power"}};
}

namespace
{
using game_data::Piece;
const Piece* find(const game_data::Snapshot& s, int id)
{
    const auto it = std::find_if(s.pieces.begin(), s.pieces.end(), [id](const auto& p) { return p.id == id; });
    return it == s.pieces.end() ? nullptr : &*it;
}
bool changed(const Piece& a, const Piece& b)
{
    return std::tie(a.row,a.column,a.health,a.disabledTurns,a.actionState,a.hasActed,a.owner,
        a.controlTurnsRemaining,a.power,a.abilityUses,a.hidden,a.infestationTitle,a.sacrificePending,
        a.silenceTurnsRemaining,a.abilitySilenced,a.healingBlocked,a.repeatActionUses,a.growTurnsRemaining)
        != std::tie(b.row,b.column,b.health,b.disabledTurns,b.actionState,b.hasActed,b.owner,
        b.controlTurnsRemaining,b.power,b.abilityUses,b.hidden,b.infestationTitle,b.sacrificePending,
        b.silenceTurnsRemaining,b.abilitySilenced,b.healingBlocked,b.repeatActionUses,b.growTurnsRemaining);
}
bool moved(const Piece& a, const Piece& b) { return a.row != b.row || a.column != b.column; }
bool outcomeChanged(const Piece& a, const Piece* b)
{
    return !b || a.health != b->health || a.disabledTurns != b->disabledTurns ||
        a.owner != b->owner || a.controlTurnsRemaining != b->controlTurnsRemaining ||
        a.infestationTitle != b->infestationTitle || moved(a,*b);
}
bool namedEvent(const game_data::Snapshot& a, const game_data::Snapshot& b, const Piece& p)
{
    if (a.status == b.status) return false;
    return b.status.starts_with(p.name + " hit ") || b.status.starts_with(p.name + " healed ") ||
        b.status.starts_with(p.name + " used ") || b.status.starts_with(p.name + " moved") ||
        b.status.find("commanded " + p.name + " to ") != std::string::npos;
}
}

bool gameplayChanged(const game_data::Snapshot& a, const game_data::Snapshot& b)
{
    if (a.phase != b.phase || a.activePlayer != b.activePlayer || a.winner != b.winner ||
        a.commandingPieceId != b.commandingPieceId || a.relentlessPieceId != b.relentlessPieceId ||
        a.swapChoicePending != b.swapChoicePending || a.pieces.size() != b.pieces.size() ||
        a.hand.size() != b.hand.size() || a.foresightChoices.size() != b.foresightChoices.size() ||
        a.graveyard.size() != b.graveyard.size() || a.exiled.size() != b.exiled.size() ||
        a.raiseUndeadChoices.size() != b.raiseUndeadChoices.size() || a.enchantments.size() != b.enchantments.size()) return true;
    for (std::size_t i=0;i<2;++i)
        if (a.players[i].resources != b.players[i].resources || a.players[i].handCount != b.players[i].handCount ||
            a.players[i].discardsThisTurn != b.players[i].discardsThisTurn) return true;
    for (std::size_t i=0;i<a.hand.size();++i) if(a.hand[i].title != b.hand[i].title) return true;
    for (const auto& p : a.pieces) { const auto* q=find(b,p.id); if(!q || changed(p,*q)) return true; }
    return false;
}

std::vector<Request> planTransition(const game_data::Snapshot& before,
    const game_data::Snapshot& after, const Intent& intent)
{
    std::vector<Request> result;
    // Clock ticks, status-only errors and replayed snapshots are not game actions.
    if (!gameplayChanged(before,after)) return result;
    std::set<std::pair<std::string,int>> scheduled;
    auto add = [&](const std::vector<Layer>& recipe, const Piece* actor, bool slice=true,
                   float delay=0.0f, Bus bus=Bus::Combat, bool reverse=false) {
        const auto* mapped=actor ? profile(actor->name) : nullptr;
        for (std::size_t i=0;i<recipe.size();++i) {
            const auto& l=recipe[i];
            const int identity=actor?actor->id:0;
            if (!scheduled.emplace(std::string(l.id),identity).second) continue;
            float identityPitch=mapped?mapped->identitySemitones:0.0f;
            if(actor && actor->name=="The Choir") {
                if(actor->attackAnimPath.find("_small")!=std::string::npos)identityPitch+=2.0f;
                else if(actor->attackAnimPath.find("_med")!=std::string::npos)identityPitch+=.75f;
                else identityPitch-=1.0f;
            }
            result.push_back({std::string(l.id),l.gain,identityPitch,
                delay+l.delay,slice,reverse,i>0?Bus::Accent:bus,identity});
        }
    };
    const bool phaseReveal = before.phase == static_cast<std::uint8_t>(game_data::Phase::HeroPlacement) &&
        after.phase != before.phase;
    bool anyHealed=false;
    int reactions=0;
    for (const auto& p : before.pieces) {
        if(p.hidden && p.owner!=before.yourPlayer) continue;
        const auto* next=find(after,p.id);
        const auto* mapped=profile(p.name);
        if (!next) {
            const bool newlyDead=after.graveyard.size()>before.graveyard.size() ||
                after.exiled.size()>before.exiled.size() || after.winner!=before.winner;
            const bool vanished=after.status.find(p.name + " used Dematerialize") != std::string::npos ||
                (!newlyDead && game_data::normalizedAbility(p.ability)=="dematerialize") ||
                (intent.actorId==p.id && intent.kind==Intent::Kind::Ability &&
                 game_data::normalizedAbility(p.ability)=="dematerialize");
            if (vanished) add({{"teleport"}},&p);
            else if (reactions++ < 4) add(mapped?mapped->death:std::vector<Layer>{{"body_hit",.4f},{"death",.5f}},&p,true,.18f);
            continue;
        }
        if (moved(p,*next)) {
            auto recipe=mapped?mapped->movement:std::vector<Layer>{};
            const auto resolved=game_data::resolvePieceAction(before.pieces,before.holes,p,next->row,next->column);
            const auto movementIndex=intent.actorId==p.id && intent.kind==Intent::Kind::Action &&
                intent.actionIndex>=0 ? intent.actionIndex : resolved.actionIndex;
            const auto kind=movementIndex>=0 && movementIndex<static_cast<int>(p.actions.size())
                ? static_cast<game_data::ActionKind>(p.actions[static_cast<std::size_t>(movementIndex)].kind)
                : game_data::ActionKind::Slide;
            if(kind==game_data::ActionKind::Teleport) recipe={{"teleport",.55f}};
            else if(kind==game_data::ActionKind::Tunnel) recipe={{"swamp_squelch",.55f},{"roots",.25f}};
            if(recipe.empty()) {
                const bool flying=std::any_of(p.traits.begin(),p.traits.end(),[](const auto& trait){
                    const auto name=game_data::normalizedAbility(trait);
                    return name=="flying" || name=="flight" || name=="winged";
                });
                recipe={{flying?"wing_feather":"foot_soft",.48f}};
            }
            add(recipe,&p,true,.02f);
            // A second contact, with an alternate sample, follows the first.
            if(kind!=game_data::ActionKind::Teleport)for (const auto& l:recipe) result.push_back({std::string(l.id),l.gain*.8f,
                mapped?mapped->identitySemitones:0.0f,.49f+l.delay,true,false,Bus::Accent,p.id});
        }
        if(next->health < p.health && reactions++ < 4)
            add(mapped?mapped->damage:std::vector<Layer>{{"body_hit",.6f}},&p,true,.18f);
        if(next->health > p.health) anyHealed=true;
        if(next->owner != p.owner) add({{"charm_pipes",.45f}},&p,true,.12f);
        if(next->power > p.power) add({{"power",.3f}},nullptr,true,.12f,Bus::Accent);
        if(next->silenceTurnsRemaining > p.silenceTurnsRemaining) add(abilityRecipe("silence field"),&p);
        if(!p.healingBlocked && next->healingBlocked) add({{"steam",.2f}},nullptr,true,0,Bus::Accent);
        if(next->infestationTitle != p.infestationTitle && !next->infestationTitle.empty())
            add({{"wing_insect",.25f},{"roots",.2f}},&p);
    }
    for (const auto& p:after.pieces) if(!find(before,p.id) &&
        !(p.hidden && p.owner!=after.yourPlayer) && !(phaseReveal && p.owner!=after.yourPlayer)) {
        const auto* mapped=profile(p.name);
        const bool replacement=std::any_of(before.pieces.begin(),before.pieces.end(),[&](const auto& previous){
            return previous.row==p.row && previous.column==p.column && !find(after,previous.id);});
        add(mapped?mapped->deploy:std::vector<Layer>{{"card_paper",.25f},{"teleport",.5f}},&p,
            true,replacement?.28f:0.0f);
    }

    bool cast=false;
    for (const auto& p:before.pieces) {
        if(cast || (p.hidden && p.owner!=before.yourPlayer)) continue;
        const auto* next=find(after,p.id);
        const bool named=namedEvent(before,after,p);
        const auto* requestedTarget=game_data::inBounds(intent.row,intent.column)
            ? game_data::findPieceAt(before.pieces,intent.row,intent.column) : nullptr;
        const bool actorAccepted=!next || moved(p,*next) || (!p.hasActed && next->hasActed) ||
            p.actionState!=next->actionState || p.repeatActionUses!=next->repeatActionUses ||
            p.abilityUses!=next->abilityUses || after.commandingPieceId==p.id ||
            (requestedTarget && outcomeChanged(*requestedTarget,find(after,requestedTarget->id)));
        const bool intended=intent.actorId==p.id && actorAccepted &&
            (intent.kind==Intent::Kind::Action || intent.kind==Intent::Kind::Ability);
        if(!intended && !named && !(next && p.owner==before.activePlayer &&
            (moved(p,*next) || (!p.hasActed && next->hasActed) || p.actionState!=next->actionState ||
             p.repeatActionUses!=next->repeatActionUses))) continue;
        const auto ability=game_data::normalizedAbility(intent.sacrifice?p.sacrifice:p.ability);
        const bool usedAbility = intended && intent.kind==Intent::Kind::Ability;
        const bool legacyAbility=named && (after.status.find(" used ")!=std::string::npos ||
            after.status.find(" to use ")!=std::string::npos);
        const bool visibleAbilityChange=after.commandingPieceId==p.id && before.commandingPieceId!=p.id;
        if(usedAbility || legacyAbility || visibleAbilityChange || (next && p.actionState!=next->actionState && !ability.empty() &&
            intent.kind!=Intent::Kind::Action)) {
            add(abilityRecipe(ability),&p);
            if(intent.sacrifice) add({{"death",.4f}},&p);
            cast=true; continue;
        }
        int actionIndex=-1;
        if(intended && intent.actionIndex>=0 && intent.actionIndex<static_cast<int>(p.actions.size()))
            actionIndex=intent.actionIndex;
        if(actionIndex<0) {
            // Resolve actual structured actions against changed visible targets;
            // this also handles heals/status-only actions and redirected hits.
            for(const auto& target:before.pieces) {
                if(target.id==p.id || !outcomeChanged(target,find(after,target.id))) continue;
                const auto action=game_data::resolvePieceAction(before.pieces,before.holes,p,target.row,target.column);
                if(action.legal && action.attacks) { actionIndex=action.actionIndex; break; }
            }
        }
        if(actionIndex<0 && intended && game_data::inBounds(intent.row,intent.column)) {
            const auto action=game_data::resolvePieceAction(before.pieces,before.holes,p,intent.row,intent.column);
            if(action.legal)actionIndex=action.actionIndex;
        }
        // A visible actor can complete a cast against an unchanged target.
        // Infer its action only when the consumed turn has one possible attack.
        if(actionIndex<0 && next && !p.hasActed && next->hasActed && !moved(p,*next)) {
            int candidate=-1;
            for(std::size_t i=0;i<p.actions.size();++i)
                if(p.actions[i].canAttack) {
                    if(candidate>=0) {candidate=-1;break;}
                    candidate=static_cast<int>(i);
                }
            actionIndex=candidate;
        }
        if(actionIndex<0 || actionIndex>=static_cast<int>(p.actions.size())) continue;
        const auto& action=p.actions[static_cast<std::size_t>(actionIndex)];
        if(!action.canAttack) continue;
        if(action.heal>0 && !anyHealed) {
            add({{"magic_bright",.3f}},&p,true,.06f);
            cast=true;continue;
        }
        const auto* mapped=profile(p.name);
        const MappedAction* recipe=nullptr;
        if(mapped)for(const auto& a:mapped->actions)
            if(a.name==action.name || a.internalName==action.name) {recipe=&a;break;}
        auto generic=action.heal>0?std::vector<Layer>{{"heal"}}:
            action.control>0?std::vector<Layer>{{"charm_pipes"}}:
            action.statusTurns>0?std::vector<Layer>{{"roots"},{"magic_dark",.25f}}:
            action.kind==static_cast<std::uint8_t>(game_data::ActionKind::Ranged)?
                std::vector<Layer>{{"magic_bright"}}:std::vector<Layer>{{"blade"}};
        auto layers=recipe?recipe->layers:generic;
        // Lifedrain restoration is conditional on the actual resulting Health.
        if(!anyHealed) std::erase_if(layers,[](const auto& l){return l.id=="heal";});
        add(layers,&p,true,.06f);
        cast=true;
    }
    if(anyHealed && std::none_of(result.begin(),result.end(),[](const auto& r){return r.id=="heal";}))
        add({{"heal",.38f}},nullptr,true,.18f,Bus::Accent);
    if(intent.kind==Intent::Kind::PlayCard && intent.handIndex>=0 &&
        intent.handIndex<static_cast<int>(before.hand.size())) {
        const auto& card=before.hand[static_cast<std::size_t>(intent.handIndex)];
        const auto copies=[&](const auto& hand){return std::count_if(hand.begin(),hand.end(),
            [&](const auto& item){return item.title==card.title;});};
        const bool consumed=copies(after.hand)<copies(before.hand);
        if(consumed && (card.type=="Spell"||card.type=="Enchantment")) add(effectRecipe(card),nullptr);
    }
    // Opponent effect cards are publicly represented by enchantments or by their
    // visible result; no attempt is made to infer an opponent's secret hand.
    for(const auto& e:after.enchantments)if(std::none_of(before.enchantments.begin(),before.enchantments.end(),
        [&](const auto& previous){return previous.id==e.id;})) {
        game_data::GameCard c;c.type="Enchantment";c.title=e.title;c.effect=e.effect;add(effectRecipe(c),nullptr);
    }
    const bool handChanged=before.hand.size()!=after.hand.size() ||
        !std::equal(before.hand.begin(),before.hand.end(),after.hand.begin(),after.hand.end(),
            [](const auto& a,const auto& b){return a.title==b.title;});
    if(handChanged || before.foresightChoices.size()!=after.foresightChoices.size() ||
        intent.kind==Intent::Kind::Discard) add({{"card_paper",.4f}},nullptr,false,0,Bus::UI);
    if(after.foresightChoices.size()>before.foresightChoices.size()) add({{"crystal",.22f}},nullptr,true,0,Bus::UI);
    if(after.yourPlayer>=1 && after.yourPlayer<=2) {
        const auto i=static_cast<std::size_t>(after.yourPlayer-1);
        if(before.players[i].resources!=after.players[i].resources)
            add({{"resource_coins",.30f}},nullptr,false,.1f,Bus::UI,
                after.players[i].resources<before.players[i].resources);
    }
    if(after.activePlayer!=before.activePlayer && after.activePlayer==after.yourPlayer &&
        after.phase==static_cast<std::uint8_t>(game_data::Phase::Playing))
        add({{"ui_confirm",.35f}},nullptr,false,0,Bus::UI);
    if(after.relentlessPieceId && after.relentlessPieceId!=before.relentlessPieceId)
        add({{"power",.35f}},nullptr,true,.25f,Bus::Accent);
    return result;
}
}
