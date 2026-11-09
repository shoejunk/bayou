#pragma once

#include "../gameserver/game_engine.hpp"

template <typename Check>
void testSlow(Check check)
{
    using namespace game_data;
    card_data::Action frost;
    frost.name = "Frost";
    frost.kind = "ranged";
    frost.canMove = false;
    frost.canAttack = true;
    frost.maxRange = 5;
    frost.slow = 2;
    card_data::Card source;
    source.title = "Caster";
    source.actions = {frost};
    GameCard caster = toGameCard(source);
    GameCard walker = caster;
    walker.title = "Walker";
    ActionProfile walk;
    walk.maxRange = 3;
    walk.repeat = 1;
    walker.actions = {walk, caster.actions.front()};

    GameEngine engine(17, {});
    if (!engine.loadScenario({{1, caster, 0, 1, false}, {2, walker, 2, 3, false}},
            {}, {}, 0, 0, 1, "Slow", false))
    {
        check(false, "Slow scenario loads");
        return;
    }
    const int casterId = engine.boardPieces()[0].id, walkerId = engine.boardPieces()[1].id;
    check(engine.attackPiece(1, casterId, 2, 3) && engine.endTurn(1) &&
          engine.boardPieces()[1].slowTurnsRemaining == 2 && !engine.boardPieces()[1].hasActed,
          "zero-damage Slow starts on hit and survives until the target can act");
    check(!engine.movePiece(2, walkerId, 2, 5, 0) && engine.movePiece(2, walkerId, 3, 4, 0) &&
          !engine.movePiece(2, walkerId, 2, 3, 0),
          "a slowed walker can take one diagonal step but cannot repeat even back to its start");

    auto snapshot = engine.snapshotFor(2);
    snapshot.hand = snapshot.foresightChoices = snapshot.graveyard = snapshot.exiled =
        snapshot.raiseUndeadChoices = {walker};
    sf::Packet packet;
    writeSnapshot(packet, snapshot);
    Snapshot decoded;
    check(readSnapshot(packet, decoded) && packet.endOfPacket() && decoded.pieces[1].movedThisTurn &&
          decoded.pieces[1].slowTurnsRemaining == 2 && decoded.pieces[0].actions[0].slow == 2 &&
          decoded.hand[0].actions[1].slow == 2 && decoded.foresightChoices[0].actions[1].slow == 2 &&
          decoded.graveyard[0].actions[1].slow == 2 && decoded.exiled[0].actions[1].slow == 2 &&
          decoded.raiseUndeadChoices[0].actions[1].slow == 2 &&
          !resolvePieceAction(decoded.pieces, decoded.holes, decoded.pieces[1], 2, 3).legal,
          "mid-repeat snapshots preserve Slow, the spent step and action fields for every card zone");
    sf::Packet older;
    writeSnapshot(older, snapshot, true, true, true, true, false);
    check(readSnapshot(older, decoded) && older.endOfPacket() && !decoded.pieces[1].movedThisTurn &&
          decoded.pieces[1].slowTurnsRemaining == 0 && decoded.pieces[0].actions[0].slow == 0 &&
          decoded.hand[0].actions[1].slow == 0, "older snapshots default all Slow fields");

    check(engine.endTurn(2) && engine.endTurn(1) && engine.boardPieces()[1].slowTurnsRemaining == 1 &&
          !engine.boardPieces()[1].movedThisTurn && !engine.movePiece(2, walkerId, 3, 6, 0) &&
          engine.attackPiece(2, walkerId, 0, 1, 1), "Slow leaves stationary ranged attacks available");
    check(engine.endTurn(2) && engine.endTurn(1) && engine.boardPieces()[1].slowTurnsRemaining == 0 &&
          engine.movePiece(2, walkerId, 3, 6, 0), "full movement returns after the last slowed turn");

    auto board = engine.boardPieces();
    Piece& hopper = board[1];
    hopper.row = 2;
    hopper.column = 1;
    board[0].row = 2;
    board[0].column = 2;
    ActionProfile hop;
    hop.kind = static_cast<std::uint8_t>(ActionKind::Hop);
    hop.canAttack = true;
    hopper.actions = {hop};
    const std::array<std::uint8_t, BoardSquares> holes{};
    check(resolvePieceAction(board, holes, hopper, 2, 3).legal, "Hop has a valid two-square landing over a pivot");
    hopper.slowTurnsRemaining = 1;
    bool anyLegal = false;
    for (int r = 0; r < BoardSize; ++r)
        for (int c = 0; c < BoardSize; ++c)
            anyLegal |= resolvePieceAction(board, holes, hopper, r, c).legal;
    check(!anyLegal, "a slowed Hop-only unit has no moves or hopping attacks");
    hopper.actions.push_back(walk);
    hopper.movedThisTurn = false;
    check(resolvePieceAction(board, holes, hopper, 3, 1).actionIndex == 1 &&
          !resolvePieceAction(board, holes, hopper, 2, 3).legal,
          "a unit with Hop and a step can still use its own adjacent step");

    // Bodyguard absorbs damage; Intercept changes which unit takes the attack.
    Piece protector = hopper;
    protector.id = 99;
    protector.row = 3;
    protector.slowTurnsRemaining = 0;
    protector.keywords = {"Bodyguard"};
    board.push_back(protector);
    std::mt19937 rng(17);
    auto hit = [&](int damage, int slow) {
        resolvePieceAttackDamage(board, casterId, walkerId, damage, 0, 2, 2, rng, false, ActionKind::Ranged, slow);
    };
    hit(1, 3);
    hit(0, 1);
    hit(0, 0);
    check(board[1].slowTurnsRemaining == 3 && board[2].slowTurnsRemaining == 0,
          "Slow stays on the attacked unit through Bodyguard and a weaker hit cannot shorten it");
    board[1].slowTurnsRemaining = 0;
    board[2].health = 2;
    board[2].keywords = {"Intercept"};
    hit(1, 2);
    check(board[1].slowTurnsRemaining == 0 && board[2].slowTurnsRemaining == 2,
          "an interceptor takes Slow in place of the original target");
}
