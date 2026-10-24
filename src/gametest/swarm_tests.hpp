#pragma once

#include "../gameserver/game_engine.hpp"

template <typename Check>
void testSwarm(Check check)
{
    using namespace game_data;
    Piece source;
    source.id = 1;
    source.owner = 1;
    source.row = 3;
    source.column = 3;
    source.name = "Swarm Rat";
    source.keywords = {"SwArM", "swarm"};
    source.power = 2;
    Piece neighbor = source;
    neighbor.id = 2;
    neighbor.column = 4;
    neighbor.keywords.clear();
    std::vector<Piece> board{source, neighbor};
    check(effectivePiecePower(board, board[0]) == 3 && board[0].power == 2,
          "Swarm adds once per same-name neighbor without changing permanent Power");
    check(effectivePiecePower(board, board[1]) == 2,
          "same-name neighbors do not need Swarm and do not inherit it");
    neighbor.id = 3;
    neighbor.owner = 2;
    neighbor.row = 2;
    neighbor.column = 2;
    neighbor.hidden = true;
    board.push_back(neighbor);
    check(effectivePiecePower(board, board[0]) == 4,
          "Swarm stacks across friendly, enemy, diagonal, and hidden neighbors");
    board[1].column = 7;
    check(effectivePiecePower(board, board[0]) == 3, "Swarm ends when a neighbor moves away");
    board[1].column = 4;
    board[1].health = 0;
    check(effectivePiecePower(board, board[0]) == 3, "Swarm ignores dead neighbors before removal");
    board[2].name = "swarm rat";
    check(effectivePiecePower(board, board[0]) == 2, "Swarm requires the exact same unit name");
    board = {source};
    check(effectivePiecePower(board, board[0]) == 2, "Swarm never counts the unit itself");
    neighbor.name = source.name;
    neighbor.row = 2;
    neighbor.column = 4;
    neighbor.width = 2;
    neighbor.height = 3;
    board.push_back(neighbor);
    check(effectivePiecePower(board, board[0]) == 3, "Swarm counts a large neighbor once");
    board[0].width = 2;
    board[0].height = 2;
    board[1].column = 5;
    check(effectivePiecePower(board, board[0]) == 3, "Swarm uses the recipient's full footprint");
    applyPowerBoost(board, board[1]); // Enemy Power Boost must not alter the recipient.
    check(board[0].power == 2, "Swarm does not change Power Boost ownership rules");
    board[1].owner = 1;
    applyPowerBoost(board, board[1]);
    check(board[0].power == 3 && effectivePiecePower(board, board[0]) == 4,
          "permanent Power Boost stacks with temporary Swarm");
    board[0].power = std::numeric_limits<int>::max();
    check(effectivePiecePower(board, board[0]) == std::numeric_limits<int>::max(),
          "Swarm total Power saturates without overflow");
    check(damageWithPower(0, effectivePiecePower(board, board[0])) == 0,
          "Swarm does not turn a non-damaging action into damage");

    // Counterstrike still triggers on lethal hits and uses the current board.
    source.power = 0;
    source.counterstrike = 2;
    source.health = 1;
    neighbor = source;
    neighbor.id = 2;
    neighbor.column = 4;
    neighbor.keywords.clear();
    Piece attacker;
    attacker.id = 3;
    attacker.owner = 2;
    attacker.row = 3;
    attacker.column = 2;
    attacker.health = 10;
    board = {source, neighbor, attacker};
    std::mt19937 random(42);
    resolvePieceAttackDamage(board, 3, 1, 1, 0, 3, 2, random);
    check(board[0].health == 0 && board[2].health == 7,
          "Swarm augments lethal-hit Counterstrike");

    GameCard rat;
    rat.title = "Swarm Rat";
    rat.type = "Unit";
    rat.health = 20;
    rat.power = 2;
    rat.keywords = {"swarm"};
    ActionProfile action;
    action.kind = static_cast<std::uint8_t>(ActionKind::Ranged);
    action.canAttack = true;
    action.canMove = false;
    action.damage = 3;
    action.maxRange = 7;
    rat.actions = {action};
    GameCard companion = rat;
    companion.keywords.clear();
    GameCard victim = companion;
    victim.title = "Swarm Target";
    for (bool push : {false, true})
    {
        rat.actions[0].push = push ? 2 : 0;
        GameEngine engine(82, {});
        check(engine.loadScenario({{1, rat, 3, 0, false}, {1, companion, 2, 0, false},
                                  {2, victim, 3, 7, false}},
                                 {}, {}, 0, 0, 1, "Swarm", false),
              "Swarm authoritative scenario loads");
        const Snapshot snapshot = engine.snapshotFor(1);
        sf::Packet packet;
        writePiece(packet, snapshot.pieces.front());
        Piece received;
        check(readPiece(packet, received) && received.power == 3 &&
                  hasKeyword(received.keywords, "swarm") && engine.boardPieces().front().power == 2,
              "Swarm sends total Power to clients without mutating authoritative permanent Power");
        check(engine.attackPiece(1, engine.boardPieces().front().id, 3, 7),
              "Swarm authoritative attack succeeds");
        const Piece* target = findPieceAt(engine.boardPieces(), 3, 7);
        check(target && target->health == (push ? 9 : 14),
              push ? "Swarm augments push collision damage" : "Swarm augments attack damage");
    }
    companion.ability = "dematerialize";
    companion.actions[0].nextState = 1;
    GameEngine concealed(83, {});
    check(concealed.loadScenario({{1, rat, 3, 0, false}, {2, companion, 2, 0, false}},
                                {}, {}, 0, 0, 2, "Hidden Swarm", false),
          "hidden Swarm neighbor scenario loads");
    check(concealed.useAbility(2, concealed.boardPieces().back().id),
          "Swarm neighbor can dematerialize");
    const Snapshot visible = concealed.snapshotFor(1);
    check(visible.pieces.size() == 1 && visible.pieces.front().power == 3,
          "displayed Swarm Power includes neighbors concealed from the viewer");
}
