#pragma once
#include "../gameserver/game_engine.hpp"

template <typename Check> void testRooted(Check check)
{
    using namespace game_data;
    GameCard sylvara; sylvara.title="Sylvara"; sylvara.type="Hero"; sylvara.health=5;
    ActionProfile vines; vines.kind=static_cast<std::uint8_t>(ActionKind::Ranged); vines.canAttack=true;
    vines.canMove=false; vines.maxRange=3; vines.targetFilter={card_data::RootedActionFlag};
    sylvara.actions={vines};
    GameCard enemy; enemy.title="Enemy"; enemy.type="Unit"; enemy.health=5; enemy.ability="dig"; enemy.abilityUses=-1;
    ActionProfile shot=vines; shot.targetFilter.clear(); shot.damage=1; enemy.actions={shot};
    GameEngine e(411,{});
    check(e.loadScenario({{1,sylvara,3,1,true},{2,enemy,3,4,false},{1,enemy,4,2,false}},
        {},{},0,0,1,"Rooted",false),"Rooted fixture loads");
    const int caster=e.boardPieces()[0].id, id=e.boardPieces()[1].id;
    check(!e.attackPiece(1,caster,4,2,0),"Living Vines rejects an ally");
    check(e.attackPiece(1,caster,3,4,0),"Living Vines roots at range three");
    const auto& rooted=e.boardPieces()[1];
    check(rooted.rootedTurnsRemaining==1&&rooted.health==5&&rooted.sleepTurnsRemaining==0&&rooted.disabledTurns==0,
        "Living Vines does not damage, Disable, or Sleep");
    const auto original=e.snapshotFor(2);
    auto sleeping = original;
    sleeping.pieces[1].sleepTurnsRemaining=1;
    sf::Packet oldSleep;
    writeSnapshot(oldSleep,sleeping);
    Snapshot projectedSleep;
    check(readSnapshot(oldSleep,projectedSleep) && projectedSleep.pieces[1].ability.empty() &&
        !projectedSleep.pieces[1].actions[0].canAttack,"legacy projection blocks both actions and abilities while Sleeping");
    sf::Packet modern, legacy;
    writeSnapshot(modern,original,true,true,true,true,true,true,true,true,true,true,true,true,true);
    writeSnapshot(legacy,original,true,true,true,true,true,true,true,true,true,true,true,true,false);
    Snapshot a,b;
    check(readSnapshot(modern,a)&&a.pieces[1].rootedTurnsRemaining==1,"Rooted negotiable field round trips");
    check(readSnapshot(legacy,b)&&b.pieces[1].rootedTurnsRemaining==0&&!b.pieces[1].actions[0].canAttack&&
        !b.pieces[1].hasActed&&pieceAbilityAvailable(b.pieces[1]),"legacy projection still permits activated ability only");
    check(e.endTurn(1)&&e.boardPieces()[1].rootedTurnsRemaining==1,"Rooted lasts until affected turn ends");
    check(!e.attackPiece(2,id,3,1,0)&&e.useAbility(2,id),"Rooted rejects stationary attack but permits Dig");
    check(e.endTurn(2)&&e.boardPieces()[1].rootedTurnsRemaining==0,"Rooted expires at end of affected turn");
    GameCard commander=enemy; commander.ability="command";
    GameEngine commanded(412,{});
    check(commanded.loadScenario({{1,sylvara,3,1,true},{2,enemy,3,4,false},{2,commander,4,4,false}},
        {},{},0,0,1,"Commanded Rooted",false),"Commanded Rooted fixture loads");
    const int digger=commanded.boardPieces()[1].id, leader=commanded.boardPieces()[2].id;
    check(commanded.attackPiece(1,commanded.boardPieces()[0].id,3,4,0)&&commanded.endTurn(1)&&
        commanded.useAbility(2,leader)&&!commanded.attackPiece(2,digger,3,1,0)&&commanded.useAbility(2,digger),
        "Command does not bypass Rooted for regular attacks but permits Dig");
    Piece probe=original.pieces[1]; probe.hasActed=false; probe.rootedTurnsRemaining=0;
    const auto square=std::array<std::uint8_t,BoardSquares>{};
    auto board=original.pieces;
    probe.sleepTurnsRemaining=1;
    check(!resolvePieceAction(board,square,probe,3,1).legal&&!pieceAbilityAvailable(probe),"Sleep blocks stationary attack and activated ability");
    probe.sleepTurnsRemaining=0; probe.silencedTurnsRemaining=1;
    check(resolvePieceAction(board,square,probe,3,1).legal&&!pieceAbilityAvailable(board,probe),"Silence only blocks activated ability");
    probe.rootedTurnsRemaining=1; probe.hasActed=true; applyPurify(probe);
    check(probe.rootedTurnsRemaining==0&&probe.hasActed,"Purify removes Rooted and preserves spent action");
}
