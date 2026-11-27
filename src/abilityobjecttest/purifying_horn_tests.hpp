#pragma once
#include "../gameserver/game_engine.hpp"

template<typename Check>
void testPurifyingHorn(Check check,const game_data::GameCard& unicorn)
{
    using namespace game_data;
    check(unicorn.actions.size()==2 && unicorn.actions[0].name=="Prismatic Charge" &&
        unicorn.actions[1].name=="Purifying Horn", "Unicorn retains Charge and adds Horn as second regular action");
    const auto& horn=unicorn.actions.at(1);
    check(horn.kind==static_cast<std::uint8_t>(ActionKind::Ranged) && horn.minRange==1 && horn.maxRange==2 &&
        horn.damage==0 && horn.heal==0 && !horn.canMove && horn.canAttack &&
        card_data::actionHasPurify(horn.targetFilter), "Horn is range-two pure Purify with no healing, damage or movement");
    GameCard ally; ally.title="Friendly target"; ally.type="Unit"; ally.health=6;
    GameEngine engine(712,{});
    check(engine.loadScenario({{1,unicorn,3,1,false},{1,ally,3,3,false},{2,ally,2,1,false}},
        {},{},0,0,1,"Horn",false), "Horn scenario loads");
    const int id=engine.boardPieces()[0].id;
    auto& target=const_cast<Piece&>(engine.boardPieces()[1]);
    target.health=2;target.disabledTurns=2;target.sleepTurnsRemaining=2;target.rootedTurnsRemaining=1;
    target.slowTurnsRemaining=3;target.silencedTurnsRemaining=2;target.silencedExpiryPlayer=1;
    target.weakenedPower=4;target.power=7;target.transformTurnsRemaining=3;target.transformInto="Keep form";
    target.hasActed=true;target.movedThisTurn=true;target.controlTurnsRemaining=3;target.originalOwner=2;
    target.infestationTitle="Keep infestation";target.infestationOwner=2;
    check(!engine.attackPiece(1,id,2,1,1),"pure Purify rejects an enemy instead of becoming a zero-damage attack");
    check(engine.attackPiece(1,id,3,3,1),"Horn accepts an injured friendly target at range two");
    const auto& clean=engine.boardPieces()[1];
    check(clean.disabledTurns==0 && clean.sleepTurnsRemaining==0 && clean.rootedTurnsRemaining==0 &&
        clean.slowTurnsRemaining==0 && clean.silencedTurnsRemaining==0 && clean.silencedExpiryPlayer==0 &&
        clean.weakenedPower==0, "Horn uses existing Purify to clear all listed negative statuses");
    check(clean.health==2 && clean.row==3 && clean.column==3 && clean.power==7 &&
        clean.transformTurnsRemaining==3 && clean.transformInto=="Keep form" && clean.hasActed && clean.movedThisTurn &&
        clean.controlTurnsRemaining==3 && clean.originalOwner==2 && clean.infestationTitle=="Keep infestation" &&
        clean.infestationOwner==2, "Horn preserves Health, position, buffs, transformation, readiness, control and infestation");
    check(engine.boardPieces()[0].hasActed && engine.boardPieces()[0].row==3 && engine.boardPieces()[0].column==1 &&
        engine.snapshotFor(1).players[0].pieceActionUsedThisTurn, "Horn spends the ordinary action without moving its caster");
    auto board=engine.boardPieces();board[0].hasActed=false;board[1].health=board[1].maxHealth;
    std::array<std::uint8_t,BoardSquares> holes{};
    check(resolvePieceAction(board,holes,board[0],3,3,false,1).legal,"Horn can cleanse at full Health");
    board[1].column=4;
    check(!resolvePieceAction(board,holes,board[0],3,4,false,1).legal,"Horn rejects range three");
    board[1].row=1;board[1].column=3;
    check(resolvePieceAction(board,holes,board[0],1,3,false,1).legal,"Horn uses existing diagonal range-two targeting");
    board[1].row=2;
    check(!resolvePieceAction(board,holes,board[0],2,3,false,1).legal,"Horn retains straight/diagonal geometry rather than knight targeting");
    board[1].row=3;board[1].column=3;board[0].actions[1].targetFilter.push_back("Mechanical");
    check(!resolvePieceAction(board,holes,board[0],3,3,false,1).legal,"pure Purify retains authored trait filters");
}
