#pragma once
#include "../gameserver/game_engine.hpp"

template <typename Check>
void testBind(Check check)
{
    using namespace game_data;
    Piece mover; mover.id = 1; mover.owner = 1; mover.row = 3; mover.column = 3;
    mover.health = mover.maxHealth = 10; mover.ability = "silence";
    ActionProfile step; step.maxRange = 3; step.canMove = true; step.canAttack = true; step.damage = 1;
    ActionProfile ranged = step; ranged.kind = static_cast<std::uint8_t>(ActionKind::Ranged); ranged.canMove = false;
    mover.actions = {step, ranged};
    Piece binder = mover; binder.id = 2; binder.owner = 2; binder.column = 4; binder.passives = {"BiNd"};
    std::vector<Piece> board{mover, binder};
    std::array<std::uint8_t, BoardSquares> holes{};
    check(pieceMovementBound(board, board[0]), "Bind recognizes an adjacent enemy and case-insensitive keyword");
    check(!resolvePieceAction(board, holes, board[0], 2, 3).legal,
          "Bind blocks moving to another square still adjacent to its source");
    check(!resolvePieceAction(board, holes, board[0], 3, 1).legal,
          "Bind blocks moving away from its source");
    check(resolvePieceAction(board, holes, board[0], 3, 4, false, 1).legal && pieceAbilityAvailable(board, board[0]),
          "Bind permits stationary ranged attacks and nonmovement activated abilities");
    check(resolvePieceAction(board, holes, board[0], 3, 4, false, 0).legal,
          "Bind permits a contact strike against its source with conditional advance");
    board[1].row = 2;
    check(pieceMovementBound(board, board[0]), "Bind includes diagonal adjacency");
    board[1].column = 5;
    check(!pieceMovementBound(board, board[0]), "Bind does not extend beyond adjacent squares");
    board[1].width = 2; board[1].column = 1;
    check(pieceMovementBound(board, board[0]), "Bind uses complete large-unit footprints");
    board = {mover, binder}; board[1].owner = 1;
    check(!pieceMovementBound(board, board[0]), "friendly Bind does not restrict movement");
    board[1].owner = 2; board[1].health = 0;
    check(!pieceMovementBound(board, board[0]), "defeated Bind stops immediately");
    board = {mover, binder}; board[1].silencedTurnsRemaining = 4;
    check(pieceMovementBound(board, board[0]), "Silence does not suppress passive Bind");
    for (ActionKind kind : {ActionKind::Teleport, ActionKind::Tunnel, ActionKind::RallyStep,
                            ActionKind::AssaultStep, ActionKind::Hop})
    {
        board = {mover, binder}; board[0].actions[0].kind = static_cast<std::uint8_t>(kind);
        holes.fill(1);
        bool anyMove = false;
        for (int row = 0; row < BoardSize; ++row)
            for (int col = 0; col < BoardSize; ++col)
            {
                const auto action = resolvePieceAction(board, holes, board[0], row, col, false, 0);
                anyMove |= action.legal && action.moves;
            }
        check(!anyMove, "Bind blocks special voluntary movement paths");
    }
    board = {mover, binder};
    auto push = applyActionPush(board, 1, 3, 4, 1);
    check(push.movedSquares == 1 && board[0].column == 2 && !pieceMovementBound(board, board[0]),
          "another unit can push a bound unit free");
    board = {mover, binder};
    Piece puller = mover; puller.id = 3; puller.column = 0; board.push_back(puller);
    auto pull = applyActionPull(board, 1, 3);
    check(pull.movedSquares > 0 && !pieceMovementBound(board, board[0]),
          "external Pull can also free a bound unit");
    board = {mover, binder}; board[1].hidden = true;
    check(!resolvePieceActionThroughHidden(board, holes, board[0], 3, 1).action.legal,
          "authoritative Bind remains effective from a hidden source");

    GameCard fighter; fighter.title = "Bound fighter"; fighter.type = "Unit";
    fighter.health = 10; fighter.actions = {step, ranged}; fighter.ability = "silence";
    GameCard warden = fighter; warden.title = "Warden"; warden.passives = {"Bind"};
    for (bool lethal : {false, true})
    {
        warden.health = lethal ? 1 : 10;
        GameEngine engine(31, {});
        check(engine.loadScenario({{1, fighter, 3, 3, false}, {2, warden, 3, 4, false}},
                  {}, {}, 0, 0, 1, "Bind test", false), "Bind combat scenario loads");
        const int id = engine.boardPieces()[0].id;
        check(!engine.movePiece(1, id, 2, 3, 0), "server rejects bound relocation without spending the action");
        check(engine.attackPiece(1, id, 3, 4, 0), "server permits strike against the binder");
        const auto& actor = engine.boardPieces()[0];
        check(actor.row == 3 && actor.column == (lethal ? 4 : 3),
              "capture advances only after binder defeat, never when it survives");
    }
    warden.health = 1;
    GameEngine multiple(31, {});
    check(multiple.loadScenario({{1, fighter, 3, 3, false}, {2, warden, 3, 4, false},
                  {2, warden, 2, 3, false}}, {}, {}, 0, 0, 1, "Multiple binders", false),
          "multiple-binder scenario loads");
    check(multiple.attackPiece(1, multiple.boardPieces()[0].id, 3, 4, 0) &&
              multiple.boardPieces()[0].column == 3,
          "a surviving second binder prevents advance after killing the first");
}
