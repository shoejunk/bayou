#pragma once

#include "game_engine.hpp"

enum class AiActionKind
{
    EndTurn,
    MovePiece,
    AttackPiece,
    UseAbility,
    PlayCard,
    DiscardCard,
    CollectSteam
};

struct AiAction
{
    AiActionKind kind = AiActionKind::EndTurn;
    int pieceId = 0;
    int handIndex = 0;
    int row = 0;
    int column = 0;
};

void applyAiAction(GameEngine& engine, int playerNumber, const AiAction& action);
AiAction chooseAiAction(const GameEngine& engine, int aiPlayer);
void placeAiHeroes(GameEngine& engine, int aiPlayer);
