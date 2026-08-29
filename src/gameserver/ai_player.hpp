#pragma once

#include "game_engine.hpp"

enum class AiActionKind
{
    EndTurn,
    MovePiece,
    AttackPiece,
    UseAbility,
    PlayCard,
    DrawCard,
    DiscardCard
};

struct AiAction
{
    AiActionKind kind = AiActionKind::EndTurn;
    int pieceId = 0;
    int handIndex = 0;
    int row = 0;
    int column = 0;
};

// How many whole turns the planner looks ahead by default: its own turn and
// the reply it has to survive.
constexpr int AiDefaultSearchTurns = 2;

// Returns false when the engine rejected the action, which lets a caller stop
// instead of repeating a move that cannot be made.
bool applyAiAction(GameEngine& engine, int playerNumber, const AiAction& action);

// Picks the next action for `aiPlayer`. `searchTurns` counts whole turns, not
// single actions: 1 plans this turn only, 2 also answers the opponent's reply,
// 3 looks one turn past that.
AiAction chooseAiAction(
    const GameEngine& engine, int aiPlayer, int searchTurns = AiDefaultSearchTurns);

// Index of the Foresight choice worth keeping, or 0 when there is nothing to
// choose between.
int chooseAiForesightCard(const GameEngine& engine, int playerNumber);

void placeAiHeroes(GameEngine& engine, int aiPlayer);
