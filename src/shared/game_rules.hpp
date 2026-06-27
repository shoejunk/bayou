#pragma once

#include "game_data.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace game_data
{

// ---- geometry helpers ------------------------------------------------------

inline int squareIndex(int row, int column)
{
    return row * BoardSize + column;
}

inline bool inBounds(int row, int column)
{
    return row >= 0 && row < BoardSize && column >= 0 && column < BoardSize;
}

inline int chebyshev(int r1, int c1, int r2, int c2)
{
    const int dr = r1 > r2 ? r1 - r2 : r2 - r1;
    const int dc = c1 > c2 ? c1 - c2 : c2 - c1;
    return dr > dc ? dr : dc;
}

// The four home squares a player starts controlling.
inline std::array<std::pair<int, int>, 4> homeSquares(int playerNumber)
{
    const int column = playerNumber == 1 ? 0 : BoardSize - 1;
    constexpr int FirstMiddleRow = (BoardSize - 4) / 2;
    return {{{FirstMiddleRow, column},
             {FirstMiddleRow + 1, column},
             {FirstMiddleRow + 2, column},
             {FirstMiddleRow + 3, column}}};
}

inline int absInt(int value)
{
    return value < 0 ? -value : value;
}

inline const Piece* findPieceAt(const std::vector<Piece>& pieces, int row, int column)
{
    for (const Piece& piece : pieces)
    {
        if (piece.row == row && piece.column == column)
        {
            return &piece;
        }
    }
    return nullptr;
}

// Shared rules used by both the authoritative server and the client (for
// highlighting reachable squares). The server remains the source of truth.
inline bool hasLegalMoveGeometry(
    const std::vector<Piece>& pieces,
    const Piece& piece,
    int toRow,
    int toColumn,
    bool allowOccupiedDestination)
{
    if (!inBounds(toRow, toColumn) ||
        (!allowOccupiedDestination && findPieceAt(pieces, toRow, toColumn) != nullptr))
    {
        return false;
    }

    const int dr = toRow - piece.row;
    const int dc = toColumn - piece.column;
    if (dr == 0 && dc == 0)
    {
        return false;
    }

    const MovePattern pattern = static_cast<MovePattern>(piece.movePattern);
    if (pattern == MovePattern::None)
    {
        return false;
    }

    if (pattern == MovePattern::Jump)
    {
        const int adr = absInt(dr);
        const int adc = absInt(dc);
        return (adr == 1 && adc == 2) || (adr == 2 && adc == 1);
    }

    const bool straight = (dr == 0 || dc == 0);
    const bool diagonal = (absInt(dr) == absInt(dc));
    if (pattern == MovePattern::Ortho && !straight)
    {
        return false;
    }
    if (pattern == MovePattern::Diag && !diagonal)
    {
        return false;
    }
    if (pattern == MovePattern::Omni && !(straight || diagonal))
    {
        return false;
    }

    const int steps = absInt(dr) > absInt(dc) ? absInt(dr) : absInt(dc);
    if (steps > piece.moveRange)
    {
        return false;
    }

    const int stepR = (dr > 0) - (dr < 0);
    const int stepC = (dc > 0) - (dc < 0);
    int currentRow = piece.row + stepR;
    int currentColumn = piece.column + stepC;
    while (currentRow != toRow || currentColumn != toColumn)
    {
        if (findPieceAt(pieces, currentRow, currentColumn) != nullptr)
        {
            return false;
        }
        currentRow += stepR;
        currentColumn += stepC;
    }
    return true;
}

inline bool isLegalMove(const std::vector<Piece>& pieces, const Piece& piece, int toRow, int toColumn)
{
    return hasLegalMoveGeometry(pieces, piece, toRow, toColumn, false);
}

inline bool isLegalAttackingMove(
    const std::vector<Piece>& pieces,
    const Piece& piece,
    int toRow,
    int toColumn)
{
    const Piece* target = findPieceAt(pieces, toRow, toColumn);
    return piece.attackingMove && target != nullptr && target->owner != piece.owner &&
        hasLegalMoveGeometry(pieces, piece, toRow, toColumn, true);
}

inline std::pair<int, int> attackingMoveFallbackSquare(const Piece& piece, int targetRow, int targetColumn)
{
    if (static_cast<MovePattern>(piece.movePattern) == MovePattern::Jump)
    {
        return {piece.row, piece.column};
    }

    const int stepR = (targetRow > piece.row) - (targetRow < piece.row);
    const int stepC = (targetColumn > piece.column) - (targetColumn < piece.column);
    return {targetRow - stepR, targetColumn - stepC};
}

inline bool isLegalAttack(const Piece& attacker, const Piece& target)
{
    if (target.owner == attacker.owner || attacker.attack <= 0)
    {
        return false;
    }
    return chebyshev(attacker.row, attacker.column, target.row, target.column) <= attacker.attackRange;
}

struct ActionResolution
{
    bool legal = false;
    bool moves = false;
    bool attacks = false;
    int actionIndex = -1;
    int targetId = 0;
    int damage = 0;
    int statusTurns = 0;
    int cooldownTurns = 0;
    int stagingRow = 0;
    int stagingColumn = 0;
};

inline bool actionPatternMatches(
    std::uint8_t patternValue,
    int deltaRow,
    int deltaColumn,
    int minRange,
    int maxRange)
{
    if (deltaRow == 0 && deltaColumn == 0)
    {
        return false;
    }

    const int absoluteRow = absInt(deltaRow);
    const int absoluteColumn = absInt(deltaColumn);
    const int distance = absoluteRow > absoluteColumn ? absoluteRow : absoluteColumn;
    if (distance < minRange || distance > maxRange)
    {
        return false;
    }

    const MovePattern pattern = static_cast<MovePattern>(patternValue);
    if (pattern == MovePattern::None)
    {
        return true;
    }
    if (pattern == MovePattern::Jump)
    {
        return (absoluteRow == 1 && absoluteColumn == 2) ||
               (absoluteRow == 2 && absoluteColumn == 1);
    }

    const bool straight = deltaRow == 0 || deltaColumn == 0;
    const bool diagonal = absoluteRow == absoluteColumn;
    if (pattern == MovePattern::Ortho)
    {
        return straight;
    }
    if (pattern == MovePattern::Diag)
    {
        return diagonal;
    }
    if (pattern == MovePattern::Horizontal)
    {
        return deltaRow == 0;
    }
    if (pattern == MovePattern::Vertical)
    {
        return deltaColumn == 0;
    }
    return straight || diagonal;
}

inline bool actionPathClear(
    const std::vector<Piece>& pieces,
    const Piece& piece,
    int toRow,
    int toColumn,
    bool passThrough)
{
    if (passThrough)
    {
        return true;
    }

    const int deltaRow = toRow - piece.row;
    const int deltaColumn = toColumn - piece.column;
    const int stepRow = (deltaRow > 0) - (deltaRow < 0);
    const int stepColumn = (deltaColumn > 0) - (deltaColumn < 0);
    int row = piece.row + stepRow;
    int column = piece.column + stepColumn;
    while (row != toRow || column != toColumn)
    {
        if (findPieceAt(pieces, row, column) != nullptr)
        {
            return false;
        }
        row += stepRow;
        column += stepColumn;
    }
    return true;
}

inline ActionResolution resolvePieceAction(
    const std::vector<Piece>& pieces,
    const std::array<std::uint8_t, BoardSquares>& holes,
    const Piece& piece,
    int toRow,
    int toColumn)
{
    ActionResolution best;
    if (!inBounds(toRow, toColumn) || piece.growTurnsRemaining > 0 || piece.disabledTurns > 0)
    {
        return best;
    }

    const Piece* destination = findPieceAt(pieces, toRow, toColumn);
    const int deltaRow = toRow - piece.row;
    const int deltaColumn = toColumn - piece.column;

    for (std::size_t index = 0; index < piece.actions.size(); ++index)
    {
        const ActionProfile& action = piece.actions[index];
        if (action.state != piece.actionState)
        {
            continue;
        }

        ActionResolution candidate;
        candidate.actionIndex = static_cast<int>(index);
        candidate.damage = action.damage;
        candidate.statusTurns = action.statusTurns;
        candidate.cooldownTurns = action.cooldownTurns;
        candidate.stagingRow = piece.row;
        candidate.stagingColumn = piece.column;

        const ActionKind kind = static_cast<ActionKind>(action.kind);
        if (kind == ActionKind::Teleport)
        {
            if (action.canMove && destination == nullptr && (deltaRow != 0 || deltaColumn != 0))
            {
                candidate.legal = true;
                candidate.moves = true;
            }
        }
        else if (kind == ActionKind::Tunnel)
        {
            const std::size_t fromIndex = static_cast<std::size_t>(squareIndex(piece.row, piece.column));
            const std::size_t toIndex = static_cast<std::size_t>(squareIndex(toRow, toColumn));
            if (action.canMove && destination == nullptr && holes[fromIndex] != 0 && holes[toIndex] != 0 &&
                (deltaRow != 0 || deltaColumn != 0))
            {
                candidate.legal = true;
                candidate.moves = true;
            }
        }
        else if (kind == ActionKind::Hop)
        {
            const int absoluteRow = absInt(deltaRow);
            const int absoluteColumn = absInt(deltaColumn);
            const bool hopGeometry =
                (absoluteRow == 2 && deltaColumn == 0) ||
                (absoluteColumn == 2 && deltaRow == 0) ||
                (absoluteRow == 2 && absoluteColumn == 2);
            if (action.canMove && destination == nullptr && hopGeometry)
            {
                const Piece* pivot = findPieceAt(
                    pieces,
                    piece.row + deltaRow / 2,
                    piece.column + deltaColumn / 2);
                if (pivot != nullptr)
                {
                    candidate.legal = true;
                    candidate.moves = true;
                    if (action.canAttack && pivot->owner != piece.owner)
                    {
                        candidate.attacks = true;
                        candidate.targetId = pivot->id;
                    }
                }
            }
        }
        else if (kind == ActionKind::Ranged)
        {
            if (action.canAttack && destination != nullptr && destination->owner != piece.owner &&
                actionPatternMatches(
                    action.pattern,
                    deltaRow,
                    deltaColumn,
                    action.minRange,
                    action.maxRange) &&
                (!action.lineOfSight || actionPathClear(pieces, piece, toRow, toColumn, false)))
            {
                candidate.legal = true;
                candidate.attacks = true;
                candidate.targetId = destination->id;
            }
        }
        else
        {
            if (!actionPatternMatches(
                    action.pattern,
                    deltaRow,
                    deltaColumn,
                    action.minRange,
                    action.maxRange))
            {
                continue;
            }

            const bool jumping = static_cast<MovePattern>(action.pattern) == MovePattern::Jump;
            if (!jumping && !actionPathClear(pieces, piece, toRow, toColumn, action.passThrough))
            {
                continue;
            }

            if (destination == nullptr && action.canMove)
            {
                candidate.legal = true;
                candidate.moves = true;
            }
            else if (destination != nullptr && destination->owner != piece.owner && action.canAttack)
            {
                candidate.legal = true;
                candidate.attacks = true;
                candidate.moves = action.canMove;
                candidate.targetId = destination->id;
                if (!jumping && !action.passThrough)
                {
                    const int stepRow = (deltaRow > 0) - (deltaRow < 0);
                    const int stepColumn = (deltaColumn > 0) - (deltaColumn < 0);
                    candidate.stagingRow = toRow - stepRow;
                    candidate.stagingColumn = toColumn - stepColumn;
                }
            }
        }

        if (!candidate.legal || (candidate.moves && piece.sleepTurnsRemaining > 0))
        {
            continue;
        }

        const int candidateImpact = candidate.attacks
            ? candidate.damage + candidate.statusTurns
            : 0;
        const int bestImpact = best.attacks ? best.damage + best.statusTurns : 0;
        if (!best.legal || candidateImpact > bestImpact)
        {
            best = candidate;
        }
    }

    return best;
}

inline bool isLegalPieceMove(
    const std::vector<Piece>& pieces,
    const std::array<std::uint8_t, BoardSquares>& holes,
    const Piece& piece,
    int toRow,
    int toColumn)
{
    const ActionResolution action = resolvePieceAction(pieces, holes, piece, toRow, toColumn);
    return action.legal && action.moves;
}

inline bool isLegalPieceAttack(
    const std::vector<Piece>& pieces,
    const std::array<std::uint8_t, BoardSquares>& holes,
    const Piece& piece,
    int toRow,
    int toColumn)
{
    const ActionResolution action = resolvePieceAction(pieces, holes, piece, toRow, toColumn);
    return action.legal && action.attacks;
}

inline std::string pieceAbilityLabel(const Piece& piece)
{
    if (piece.ability.empty())
    {
        return "";
    }
    if (!piece.abilityLabels.empty())
    {
        const std::size_t index = static_cast<std::size_t>(
            piece.actionState % static_cast<int>(piece.abilityLabels.size()));
        return piece.abilityLabels[index];
    }
    if (piece.ability == "dig")
    {
        return "Dig";
    }
    return "Use Ability";
}

inline bool pieceAbilityAvailable(const Piece& piece)
{
    if (piece.ability.empty() || piece.growTurnsRemaining > 0 || piece.disabledTurns > 0)
    {
        return false;
    }
    if (piece.ability == "dig")
    {
        return piece.abilityUses != 0;
    }
    return true;
}

} // namespace game_data
