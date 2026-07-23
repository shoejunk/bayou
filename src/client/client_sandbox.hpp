#pragma once

#include "../shared/game_data.hpp"

namespace bayou::client
{

const game_data::Piece* pieceByIdInSnapshot(const game_data::Snapshot& snapshot, int id);
game_data::Piece* pieceByIdInSnapshotMutable(game_data::Snapshot& snapshot, int id);
const game_data::Piece* pieceAtInSnapshot(const game_data::Snapshot& snapshot, int row, int column);
void removePieceFromSnapshot(game_data::Snapshot& snapshot, int id);
bool destroyPieceInSnapshot(
    game_data::Snapshot& snapshot,
    int& nextPieceId,
    int id,
    const game_data::GameCard* rebirthCard);
int controlledCountInSnapshot(const game_data::Snapshot& snapshot, int playerNumber);
int heroesAliveInSnapshot(const game_data::Snapshot& snapshot, int playerNumber);
void refreshSandboxPlayerSnapshots(game_data::Snapshot& snapshot);
void recomputeSandboxControl(game_data::Snapshot& snapshot);
void spawnSandboxPiece(
    game_data::Snapshot& snapshot,
    int& nextPieceId,
    int owner,
    const game_data::GameCard& card,
    int row,
    int column,
    bool isHero);
game_data::GameCard makeStoryTomCard();

} // namespace bayou::client
