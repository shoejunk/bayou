#include "../client/client_audio_plan.hpp"
#include "../client/client_audio_events.hpp"
#include "../gameserver/game_engine.hpp"

#include <fmt/core.h>
#include <algorithm>
#include <set>
#include <string>

using namespace game_data;
namespace sound = bayou::client::audio;
int failures=0,checks=0;
void check(bool ok,const char* name){++checks;if(!ok){++failures;fmt::println("FAIL: {}",name);}}
bool has(const std::vector<sound::Request>& r,const char* id){return std::any_of(r.begin(),r.end(),[&](const auto& s){return s.id==id;});}
GameCard card(std::string name,int health=5)
{
    GameCard c;c.title=std::move(name);c.type="Unit";c.health=health;
    ActionProfile a;a.name="Knife Stab";a.canMove=true;a.canAttack=true;a.damage=1;
    a.pattern=static_cast<std::uint8_t>(MovePattern::Omni);a.maxRange=1;c.actions={a};return c;
}
int main()
{
    bayou::client::UiHoverTracker hover;
    hover.begin(1);hover.offer(10,true);check(hover.finish(),"entering an enabled target sounds");
    for(int frame=0;frame<600;++frame) {
        hover.begin(1);hover.offer(10,true);
        check(!hover.finish(),"stationary hover and redraws stay silent");
    }
    hover.begin(1);hover.offer(11,true);check(hover.finish(),"switching target sounds");
    hover.begin(1);check(!hover.finish(),"empty surface stays silent");
    hover.begin(1);hover.offer(11,true);check(hover.finish(),"reentering target sounds");
    hover.begin(2);hover.offer(11,true);check(hover.finish(),"new menu at same position sounds");
    hover.begin(2,false);hover.offer(12,true);check(!hover.finish(),"unfocused hover stays silent");
    hover.begin(2);hover.offer(12,true);hover.activated();check(!hover.finish(),"click takes precedence over hover");
    hover.begin(2);hover.offer(13,true);hover.clear();check(!hover.finish(),"modal blocks background hover");
    hover.begin(2);hover.offer(13,false);check(!hover.finish(),"disabled target stays silent");
    check(sound::definitions().size()==41,"41 source definitions");
    check(sound::profiles().size()==97,"97 live character profiles");
    std::set<std::string_view> ids;
    for(const auto& d:sound::definitions())check(ids.insert(d.id).second,"unique sound definition");
    for(const auto& p:sound::profiles()) {
        for(const auto* recipe:{&p.movement,&p.deploy,&p.damage,&p.death})for(const auto& l:*recipe)
            check(ids.contains(l.id),"profile references an existing source");
        for(const auto& a:p.actions)for(const auto& l:a.layers)check(ids.contains(l.id),"action references an existing source");
    }
    GameEngine engine(42,{});
    auto attacker=card("Thaeron Baelstone"),target=card("Blackthorn Debt Collector");
    check(engine.loadScenario({{1,attacker,3,3,false},{2,target,3,4,false}}, {},{},0,0,1,"fixture",false),"load attack fixture");
    engine.setNarrationEnabled(false);
    auto before=engine.snapshotFor(1);const int id=before.pieces[0].id;
    check(engine.attackPiece(1,id,3,4,0),"actual engine attack accepted");
    auto after=engine.snapshotFor(1);
    sound::Intent intent{sound::Intent::Kind::Action,id,3,4,0};
    auto plan=sound::planTransition(before,after,intent);
    check(has(plan,"magic_dark")&&has(plan,"blade"),"Thaeron retains cast and knife identity without narration");
    check(has(plan,"body_hit"),"victim material hit");
    check(sound::planTransition(after,after,intent).empty(),"duplicate snapshots do not replay");
    auto tick=after;tick.turnRemainingMs-=50;tick.players[0].clockRemainingMs-=50;
    check(sound::planTransition(after,tick).empty(),"clock ticks are silent");
    auto rejected=after;rejected.status="That target is not legal.";
    check(sound::planTransition(after,rejected,intent).empty(),"rejected action produces no cast or hit");
    // Test a real healing action sharing the canAttack route.
    auto healer=card("Donella of the Marsh");healer.actions[0].name="Marshlight Mend";
    healer.actions[0].heal=3;healer.actions[0].damage=0;healer.actions[0].canMove=false;
    healer.actions[0].kind=static_cast<std::uint8_t>(ActionKind::Ranged);
    GameEngine healing(12,{});
    check(healing.loadScenario({{1,healer,3,3,false},{1,target,3,4,false,1}}, {},{},0,0,1,"heal",false),"load heal fixture");
    healing.setNarrationEnabled(false);before=healing.snapshotFor(1);
    check(healing.attackPiece(1,before.pieces[0].id,3,4,0),"engine healing accepted");
    after=healing.snapshotFor(1);intent={sound::Intent::Kind::Action,before.pieces[0].id,3,4,0};
    plan=sound::planTransition(before,after,intent);
    check(has(plan,"heal")&&!has(plan,"body_hit")&&!has(plan,"blade"),"healing never plays a damage attack");
    check(std::count_if(plan.begin(),plan.end(),[](const auto& r){return r.id=="heal";})==1,"one heal cue per event");
    auto noHeal=before;noHeal.pieces[0].hasActed=true;
    check(!has(sound::planTransition(before,noHeal,intent),"heal"),"no healing bloom when actual health is unchanged");
    check(has(sound::planTransition(before,noHeal,intent),"magic_bright"),"accepted unchanged heal still has a cast");
    check(has(sound::planTransition(before,noHeal),"magic_bright"),"remote unambiguous unchanged cast sounds");
    // Status-only targets still produce an action, not just attacks that lower health.
    before.pieces[0].name="Eyeblight";before.pieces[0].actions[0].name="Blighting Gaze";
    before.pieces[0].actions[0].heal=0;before.pieces[0].actions[0].statusTurns=2;
    after=before;after.pieces[1].disabledTurns=2;after.pieces[0].hasActed=true;
    plan=sound::planTransition(before,after,intent);
    check(has(plan,"magic_dark")&&!has(plan,"body_hit"),"status-only hex has a cast without damage");
    // Opponent concealed pieces never enter snapshot-driven sound selection.
    auto secret=before;secret.pieces[0].hidden=true;secret.pieces[0].owner=2;
    auto secretAfter=secret;secretAfter.pieces[1].disabledTurns=2;secretAfter.pieces[0].hasActed=true;
    plan=sound::planTransition(secret,secretAfter);
    check(!has(plan,"magic_dark"),"hidden enemy cannot leak a character cast");
    // First visible movement after setup uses character materials.
    before.pieces[0].name="Crystal Unicorn";after=before;after.pieces[0].row=4;
    plan=sound::planTransition(before,after);
    check(has(plan,"hoof_crystal"),"unicorn movement uses crystalline hoof contacts");
    before.pieces[0].name="Gearjaw";after=before;after.pieces[0].row=4;
    plan=sound::planTransition(before,after);
    check(has(plan,"mechanism"),"construct movement uses clockwork");
    before.pieces[0].name="Future flyer";before.pieces[0].traits={"Flying"};
    after=before;after.pieces[0].row=4;
    check(has(sound::planTransition(before,after),"wing_feather"),"unknown flying character has wing fallback");
    before.pieces[0].traits.clear();before.pieces[0].actions[0].kind=static_cast<std::uint8_t>(ActionKind::Teleport);
    after=before;after.pieces[0].row=4;intent={sound::Intent::Kind::Action,before.pieces[0].id,4,3,0};
    plan=sound::planTransition(before,after,intent);
    check(has(plan,"teleport")&&!has(plan,"foot_soft"),"teleport movement replaces footsteps");
    before.pieces[0].actions[0].kind=static_cast<std::uint8_t>(ActionKind::Tunnel);
    after=before;after.pieces[0].row=4;
    check(has(sound::planTransition(before,after,intent),"swamp_squelch"),"tunnel movement uses digging material");
    before.pieces[0].name="Bull Gator";after=before;after.pieces.erase(after.pieces.begin());
    after.status="Bull Gator used Dematerialize.";
    plan=sound::planTransition(before,after);
    check(has(plan,"teleport")&&!has(plan,"death"),"vanishing is not a death");
    GameCard undefined;undefined.type="Spell";undefined.title="Hidden Path";undefined.effect="none";
    check(sound::effectRecipe(undefined).empty(),"undefined effect artwork cannot enable cast");
    undefined.effect="damage";check(!sound::effectRecipe(undefined).empty(),"defined effect can use themed recipe");
    check(sound::abilityRecipe("Silence Field")[0].id=="steam","case-normalized silence ability");
    before={};after=before;before.yourPlayer=after.yourPlayer=1;
    after.players[1].resources=20;
    check(!has(sound::planTransition(before,after),"resource_coins"),"opponent resource changes are not local rewards");
    after.players[0].resources=20;
    check(has(sound::planTransition(before,after),"resource_coins"),"local resource delta aggregated");
    before.hand={card("Reed Baelstone")};after=before;after.hand={card("Mog")};
    check(has(sound::planTransition(before,after),"card_paper"),"same-size hand swaps still rustle");
    before={};before.phase=static_cast<std::uint8_t>(Phase::HeroPlacement);before.yourPlayer=1;
    after=before;after.phase=static_cast<std::uint8_t>(Phase::Playing);
    Piece enemy;enemy.id=99;enemy.name="Bull Gator";enemy.owner=2;enemy.isHero=true;
    after.pieces={enemy};
    check(!has(sound::planTransition(before,after),"teleport"),"placement phase reveal does not fake an enemy summon");
    before={};before.pieces={enemy};before.pieces[0].owner=1;before.pieces[0].name="Thaeron Baelstone";
    before.pieces[0].actions=attacker.actions;after=before;after.players[1].resources=5;
    intent={sound::Intent::Kind::Action,99,2,2,0};
    check(!has(sound::planTransition(before,after,intent),"magic_dark"),"stale rejected intent cannot cast on unrelated resource update");
    after=before;after.pieces[0].id=100;after.pieces[0].name="Maggie Mudroot Unmounted";
    plan=sound::planTransition(before,after);
    const auto birth=std::find_if(plan.begin(),plan.end(),[](const auto& r){return r.id=="teleport";});
    check(birth!=plan.end()&&birth->delay>=.28f,"rebirth arrival follows the death impact");
    fmt::println("Audio routing: {} checks, {} failure(s)",checks,failures);
    return failures?1:0;
}
