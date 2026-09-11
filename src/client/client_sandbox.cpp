#include "client_sandbox.hpp"
#include "../shared/game_rules.hpp"

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
    snapshot.enchantments.erase(
        std::remove_if(
            snapshot.enchantments.begin(),
            snapshot.enchantments.end(),
            [id](const game_data::Enchantment& enchantment) {
                return enchantment.target ==
                        static_cast<std::uint8_t>(game_data::EnchantmentTarget::Piece) &&
                    enchantment.targetPieceId == id;
            }),
        snapshot.enchantments.end());
}

PieceDestructionResult destroyPieceInSnapshot(
    game_data::Snapshot& snapshot,
    int& nextPieceId,
    int id,
    const game_data::GameCard* rebirthCard,
    const game_data::GameCard* infestationCard)
{
    PieceDestructionResult result;
    const game_data::Piece* found = pieceByIdInSnapshot(snapshot, id);
    if (found == nullptr)
    {
        return result;
    }

    const game_data::Piece original = *found;
    snapshot.pieces.erase(
        std::remove_if(
            snapshot.pieces.begin(),
            snapshot.pieces.end(),
            [id](const game_data::Piece& piece) { return piece.id == id; }),
        snapshot.pieces.end());

    const bool hasValidInfestation = !original.isHero &&
        original.infestationOwner >= 1 && original.infestationOwner <= 2 &&
        infestationCard != nullptr && infestationCard->type == "Unit";
    const bool hasValidRebirth = rebirthCard != nullptr &&
        (rebirthCard->type == "Unit" || rebirthCard->type == "Hero") &&
        !hasValidInfestation;
    const game_data::GameCard* replacementCard = hasValidInfestation
        ? infestationCard
        : (hasValidRebirth ? rebirthCard : nullptr);
    const bool replacementIsInfestation = hasValidInfestation;

    int replacementPieceId = 0;
    if (replacementCard != nullptr &&
        game_data::cardFootprintFree(
            snapshot.pieces,
            *replacementCard,
            original.row,
            original.column))
    {
        spawnSandboxPiece(
            snapshot,
            nextPieceId,
            replacementIsInfestation ? original.infestationOwner : original.owner,
            *replacementCard,
            original.row,
            original.column,
            !replacementIsInfestation && replacementCard->type == "Hero");
        snapshot.pieces.back().hasActed = true;
        replacementPieceId = snapshot.pieces.back().id;
    }

    if (replacementPieceId != 0)
    {
        for (game_data::Enchantment& enchantment : snapshot.enchantments)
        {
            if (enchantment.target ==
                    static_cast<std::uint8_t>(game_data::EnchantmentTarget::Piece) &&
                enchantment.targetPieceId == id)
            {
                enchantment.targetPieceId = replacementPieceId;
                enchantment.targetRow = original.row;
                enchantment.targetColumn = original.column;
            }
        }
    }
    else
    {
        snapshot.enchantments.erase(
            std::remove_if(
                snapshot.enchantments.begin(),
                snapshot.enchantments.end(),
                [id](const game_data::Enchantment& enchantment) {
                    return enchantment.target ==
                            static_cast<std::uint8_t>(game_data::EnchantmentTarget::Piece) &&
                        enchantment.targetPieceId == id;
                }),
            snapshot.enchantments.end());
    }

    if (snapshot.commandingPieceId == id)
    {
        snapshot.commandingPieceId = 0;
    }
    if (snapshot.relentlessPieceId == id)
    {
        snapshot.relentlessPieceId = 0;
    }
    result.replacementSpawned = replacementPieceId != 0;
    result.wasInfestation = replacementIsInfestation && result.replacementSpawned;
    result.wasRebirth = !replacementIsInfestation && result.replacementSpawned;
    return result;
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
    snapshot.players[0].resources = 999;
    snapshot.players[0].controlledSquares = controlledCountInSnapshot(snapshot, 1);
    snapshot.players[0].handCount = static_cast<int>(snapshot.hand.size());
    snapshot.players[0].heroesToPlace = 0;
    snapshot.players[0].heroesAlive = heroesAliveInSnapshot(snapshot, 1);
    snapshot.players[0].drawPileCount = 0;

    snapshot.players[1].resources = 0;
    snapshot.players[1].controlledSquares = controlledCountInSnapshot(snapshot, 2);
    snapshot.players[1].handCount = 0;
    snapshot.players[1].heroesToPlace = 0;
    snapshot.players[1].heroesAlive = heroesAliveInSnapshot(snapshot, 2);
    snapshot.players[1].drawPileCount = 0;
}

void recomputeSandboxControl(game_data::Snapshot& snapshot)
{
    snapshot.control = game_data::recomputeBoardControl(snapshot.control, snapshot.pieces);
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
    game_data::populatePieceFromCard(piece, card, isHero);
    piece.hasActed = false;
    snapshot.pieces.push_back(std::move(piece));
}

game_data::GameCard makeStoryTomCard()
{
    game_data::GameCard card;
    card.title = "Tinkering Tom";
    card.type = "Hero";
    card.traits = {"corrupt"};
    card.imagePath = "cards/tinkering-tom.png";
    card.tokenPath = "characters/tinkeringTom.png";
    card.walkAnimPath = "animations/tinkeringTom-walk.png";
    card.pieceBaseBluePath = "characters/bases/piece-base-blue.png";
    card.pieceBaseRedPath = "characters/bases/piece-base-red.png";
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
