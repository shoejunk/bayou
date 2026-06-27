#include "client_sandbox.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace bayou::client
{

const game_data::Piece* pieceByIdInSnapshot(const game_data::Snapshot& snapshot, int id)
{
    for (const game_data::Piece& piece : snapshot.pieces)
    {
        if (piece.id == id)
        {
            return &piece;
        }
    }
    return nullptr;
}

game_data::Piece* pieceByIdInSnapshotMutable(game_data::Snapshot& snapshot, int id)
{
    for (game_data::Piece& piece : snapshot.pieces)
    {
        if (piece.id == id)
        {
            return &piece;
        }
    }
    return nullptr;
}

const game_data::Piece* pieceAtInSnapshot(const game_data::Snapshot& snapshot, int row, int column)
{
    return game_data::findPieceAt(snapshot.pieces, row, column);
}

void removePieceFromSnapshot(game_data::Snapshot& snapshot, int id)
{
    snapshot.pieces.erase(
        std::remove_if(snapshot.pieces.begin(), snapshot.pieces.end(), [id](const game_data::Piece& piece) {
            return piece.id == id;
        }),
        snapshot.pieces.end());
}

int controlledCountInSnapshot(const game_data::Snapshot& snapshot, int playerNumber)
{
    return static_cast<int>(std::count(
        snapshot.control.begin(),
        snapshot.control.end(),
        static_cast<std::uint8_t>(playerNumber)));
}

int heroesAliveInSnapshot(const game_data::Snapshot& snapshot, int playerNumber)
{
    return static_cast<int>(std::count_if(
        snapshot.pieces.begin(),
        snapshot.pieces.end(),
        [playerNumber](const game_data::Piece& piece) {
            return piece.owner == playerNumber && piece.isHero;
        }));
}

void refreshSandboxPlayerSnapshots(game_data::Snapshot& snapshot)
{
    snapshot.players[0].steam = 999;
    snapshot.players[0].controlledSquares = controlledCountInSnapshot(snapshot, 1);
    snapshot.players[0].handCount = static_cast<int>(snapshot.hand.size());
    snapshot.players[0].heroesToPlace = 0;
    snapshot.players[0].heroesAlive = heroesAliveInSnapshot(snapshot, 1);
    snapshot.players[0].drawPileCount = 0;

    snapshot.players[1].steam = 0;
    snapshot.players[1].controlledSquares = controlledCountInSnapshot(snapshot, 2);
    snapshot.players[1].handCount = 0;
    snapshot.players[1].heroesToPlace = 0;
    snapshot.players[1].heroesAlive = heroesAliveInSnapshot(snapshot, 2);
    snapshot.players[1].drawPileCount = 0;
}

void recomputeSandboxControl(game_data::Snapshot& snapshot)
{
    std::array<std::uint8_t, game_data::BoardSquares> next = snapshot.control;
    for (int row = 0; row < game_data::BoardSize; ++row)
    {
        for (int column = 0; column < game_data::BoardSize; ++column)
        {
            const std::size_t index = static_cast<std::size_t>(game_data::squareIndex(row, column));
            if (const game_data::Piece* occupant = pieceAtInSnapshot(snapshot, row, column))
            {
                if (occupant->canControl)
                {
                    next[index] = static_cast<std::uint8_t>(occupant->owner);
                }
                continue;
            }

            int influence1 = 0;
            int influence2 = 0;
            for (int dr = -1; dr <= 1; ++dr)
            {
                for (int dc = -1; dc <= 1; ++dc)
                {
                    if (dr == 0 && dc == 0)
                    {
                        continue;
                    }
                    const game_data::Piece* neighbor = game_data::inBounds(row + dr, column + dc)
                        ? pieceAtInSnapshot(snapshot, row + dr, column + dc)
                        : nullptr;
                    if (!neighbor || !neighbor->canControl)
                    {
                        continue;
                    }
                    if (neighbor->owner == 1)
                    {
                        ++influence1;
                    }
                    else if (neighbor->owner == 2)
                    {
                        ++influence2;
                    }
                }
            }

            if (influence1 > influence2)
            {
                next[index] = 1;
            }
            else if (influence2 > influence1)
            {
                next[index] = 2;
            }
        }
    }
    snapshot.control = next;
}

void spawnSandboxPiece(
    game_data::Snapshot& snapshot,
    int& nextPieceId,
    int owner,
    const game_data::GameCard& card,
    int row,
    int column,
    bool isHero)
{
    game_data::Piece piece;
    piece.id = nextPieceId++;
    piece.owner = owner;
    piece.row = row;
    piece.column = column;
    piece.name = card.title;
    piece.keywords = card.keywords;
    piece.imagePath = card.imagePath;
    piece.walkAnimPath = card.walkAnimPath;
    piece.blueTokenPath = card.blueTokenPath;
    piece.redTokenPath = card.redTokenPath;
    piece.blueWalkAnimPath = card.blueWalkAnimPath;
    piece.redWalkAnimPath = card.redWalkAnimPath;
    piece.walkAnimFrames = card.walkAnimFrames;
    piece.maxHealth = card.health;
    piece.health = card.health;
    piece.attack = card.attack;
    piece.attackRange = card.attackRange;
    piece.movePattern = card.movePattern;
    piece.moveRange = card.moveRange;
    piece.attackingMove = card.attackingMove;
    piece.canControl = card.canControl;
    piece.growTurnsRemaining = card.growTurns;
    piece.actions = card.actions;
    piece.ability = card.ability;
    piece.abilityLabels = card.abilityLabels;
    piece.abilityUses = card.abilityUses;
    piece.isHero = isHero;
    piece.hasActed = false;
    snapshot.pieces.push_back(std::move(piece));
}

game_data::GameCard makeStoryTomCard()
{
    game_data::GameCard card;
    card.title = "Tinkering Tom";
    card.type = "Hero";
    card.keywords = {"mechanical"};
    card.imagePath = "cards/tinkering-tom.png";
    card.blueTokenPath = "characters/blue/tinkeringTom.png";
    card.redTokenPath = "characters/red/tinkeringTom.png";
    card.blueWalkAnimPath = "animations/blue/tinkeringTom-walk.png";
    card.redWalkAnimPath = "animations/red/tinkeringTom-walk.png";
    card.walkAnimFrames = 81;
    card.health = 1;
    card.attack = 0;
    card.attackRange = 1;
    card.movePattern = static_cast<std::uint8_t>(game_data::MovePattern::Omni);
    card.moveRange = 1;
    card.canControl = false;

    game_data::ActionProfile moveAction;
    moveAction.name = "Careful Step";
    moveAction.pattern = static_cast<std::uint8_t>(game_data::MovePattern::Omni);
    moveAction.minRange = 1;
    moveAction.maxRange = 1;
    moveAction.canMove = true;
    moveAction.canAttack = false;
    card.actions.push_back(moveAction);
    return card;
}

} // namespace bayou::client
