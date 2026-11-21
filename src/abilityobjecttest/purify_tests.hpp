#pragma once
#include "../gameserver/game_engine.hpp"

template<typename Check> void testPurifyingRadiance(Check check)
{
    using namespace game_data;
    GameCard caster; caster.title = "Caltheriel fixture"; caster.type = "Hero"; caster.health = 5;
    ActionProfile radiance; radiance.kind = static_cast<std::uint8_t>(ActionKind::Ranged);
    radiance.minRange=1; radiance.maxRange=3; radiance.heal=3; radiance.canMove=false; radiance.canAttack=true;
    radiance.targetFilter={card_data::PurifyActionFlag}; caster.actions={radiance};
    GameCard ally;ally.title="Ally";ally.type="Unit";ally.health=6;
    GameCard rot=ally;rot.title="Rot";rot.passives={"rot aura"};
    GameEngine engine(517, {});
    check(engine.loadScenario({{1,caster,3,1,true},{1,ally,3,4,false},{2,rot,4,4,false},{2,ally,2,2,false}},
        {},{},0,0,1,"Purifying Radiance",false),"Purifying Radiance scenario loads");
    const int casterId=engine.boardPieces()[0].id;
    auto& target=const_cast<Piece&>(engine.boardPieces()[1]);
    target.disabledTurns=2;target.sleepTurnsRemaining=1;target.slowTurnsRemaining=3;
    target.silencedTurnsRemaining=2;target.silencedExpiryPlayer=1;target.weakenedPower=7;
    target.power=4;target.hasActed=true;target.movedThisTurn=true;
    target.originalOwner=2;target.controlTurnsRemaining=4;target.infestationOwner=2;target.infestationTitle="Keep";
    check(!engine.attackPiece(1,casterId,2,2,0),"Purify rejects an enemy");
    check(engine.attackPiece(1,casterId,3,4,0),"radiance selects full-health ally at range three even in Rot Aura");
    const auto& purified=engine.boardPieces()[1];
    check(purified.disabledTurns==0&&purified.sleepTurnsRemaining==0&&purified.slowTurnsRemaining==0&&
        purified.silencedTurnsRemaining==0&&purified.silencedExpiryPlayer==0&&purified.weakenedPower==0,
        "radiance clears only listed statuses and Silence expiry player");
    check(purified.health==6&&purified.power==4&&purified.hasActed&&purified.movedThisTurn&&
        purified.originalOwner==2&&purified.controlTurnsRemaining==4&&purified.infestationTitle=="Keep"&&
        purified.infestationOwner==2&&pieceHealingBlocked(engine.boardPieces(),purified),
        "Purify preserves buffs, history/readiness, control, infestation, and Rot Aura");
    GameEngine hurt(518,{});
    hurt.loadScenario({{1,caster,3,1,true},{1,ally,3,4,false}}, {},{},0,0,1,"Radiance heal",false);
    auto& injured=const_cast<Piece&>(hurt.boardPieces()[1]);injured.health=1;injured.silencedTurnsRemaining=1;
    check(hurt.attackPiece(1,hurt.boardPieces()[0].id,3,4,0)&&hurt.boardPieces()[1].health==4&&
        hurt.boardPieces()[1].silencedTurnsRemaining==0,"hurt ally heals three and is cleansed");
    GameEngine blocked(519,{});
    blocked.loadScenario({{1,caster,3,1,true},{1,ally,3,4,false},{2,rot,4,4,false}}, {},{},0,0,1,"Aura block",false);
    auto& blockedAlly=const_cast<Piece&>(blocked.boardPieces()[1]);blockedAlly.health=1;blockedAlly.weakenedPower=3;
    check(blocked.attackPiece(1,blocked.boardPieces()[0].id,3,4,0)&&blocked.boardPieces()[1].health==1&&
        blocked.boardPieces()[1].weakenedPower==0,"Rot blocks only healing; Purify still clears Weaken");
    auto board=hurt.boardPieces();board[0].hasActed=false;
    board[1].column=5;
    std::array<std::uint8_t, BoardSquares> holes{};
    check(!resolvePieceAction(board,holes,board[0],3,5).legal,"Purify rejects target beyond range");
    board[1].column=4;board[1].health=board[1].maxHealth;
    board[0].actions[0].targetFilter.clear();
    check(!resolvePieceAction(board,holes,board[0],3,4).legal,"ordinary healing still rejects a full-health target");
    board[0].actions[0].targetFilter={card_data::PurifyActionFlag,"Mechanical"};
    check(!resolvePieceAction(board,holes,board[0],3,4).legal,"Purify retains genuine target filters");
    board[1].traits={"Mechanical"};
    check(resolvePieceAction(board,holes,board[0],3,4).purify,"matching actual target filter still purifies full-health ally");
    check(!pieceMatchesActionTargetFilter(board[1],card_data::PurifyActionFlag),
        "legacy target filter predicate would reject reserved metadata on actual ally");
}
