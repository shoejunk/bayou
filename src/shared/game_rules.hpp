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
        if (row >= piece.row && row < piece.row + piece.height &&
            column >= piece.column && column < piece.column + piece.width)
        {
            return &piece;
        }
    }
    return nullptr;
}

inline bool pieceFootprintInBounds(const Piece& piece, int row, int column)
{
    return row >= 0 && column >= 0 &&
        row + piece.height <= BoardSize && column + piece.width <= BoardSize;
}

inline bool pieceFootprintFree(
    const std::vector<Piece>& pieces, const Piece& piece, int row, int column)
{
    if (!pieceFootprintInBounds(piece, row, column)) return false;
    for (int r = row; r < row + piece.height; ++r)
        for (int c = column; c < column + piece.width; ++c)
        {
            const Piece* occupant = findPieceAt(pieces, r, c);
            if (occupant != nullptr && occupant != &piece &&
                (piece.id == 0 || occupant->id != piece.id)) return false;
        }
    return true;
}

inline bool cardFootprintFree(
    const std::vector<Piece>& pieces, const GameCard& card, int row, int column)
{
    Piece footprint;
    footprint.width = card.width;
    footprint.height = card.height;
    return pieceFootprintFree(pieces, footprint, row, column);
}

inline const Piece* findOtherPieceAt(
    const std::vector<Piece>& pieces, const Piece& piece, int row, int column)
{
    const Piece* occupant = findPieceAt(pieces, row, column);
    return occupant != nullptr && occupant != &piece &&
        (piece.id == 0 || occupant->id != piece.id) ? occupant : nullptr;
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
    if (!pieceFootprintInBounds(piece, toRow, toColumn) ||
        (!allowOccupiedDestination && !pieceFootprintFree(pieces, piece, toRow, toColumn)))
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
    const int rowGap = std::max(
        0,
        std::max(target.row - (attacker.row + attacker.height - 1),
                 attacker.row - (target.row + target.height - 1)));
    const int columnGap = std::max(
        0,
        std::max(target.column - (attacker.column + attacker.width - 1),
                 attacker.column - (target.column + target.width - 1)));
    return std::max(rowGap, columnGap) <= attacker.attackRange;
}

struct ActionResolution
{
    bool legal = false;
    bool moves = false;
    bool attacks = false;
    int actionIndex = -1;
    int nextState = 0;
    int targetId = 0;
    std::vector<int> targetIds;
    int damage = 0;
    int heal = 0;
    int statusTurns = 0;
    int cooldownTurns = 0;
    int push = 0;
    int stagingRow = 0;
    int stagingColumn = 0;
};

inline void addActionTarget(ActionResolution& action, const Piece& target)
{
    if (std::find(action.targetIds.begin(), action.targetIds.end(), target.id) == action.targetIds.end())
    {
        action.targetIds.push_back(target.id);
        if (action.targetId == 0)
        {
            action.targetId = target.id;
        }
    }
}

inline bool actionCanTarget(
    const Piece& piece,
    const Piece& target,
    int damage,
    int heal,
    const std::vector<std::string>& targetFilter)
{
    const bool matchesFilter = std::all_of(
        targetFilter.begin(),
        targetFilter.end(),
        [&](const std::string& required) {
            return hasKeyword(target.traits, required) || hasKeyword(target.keywords, required);
        });
    if (!matchesFilter)
    {
        return false;
    }
    if (target.owner == piece.owner)
    {
        return heal > 0 && target.health < target.maxHealth;
    }
    return damage > 0 || (damage == 0 && heal == 0);
}

inline std::vector<const Piece*> piecesOverlappingFootprint(
    const std::vector<Piece>& pieces, const Piece& mover, int row, int column)
{
    std::vector<const Piece*> overlaps;
    for (int r = row; r < row + mover.height; ++r)
        for (int c = column; c < column + mover.width; ++c)
            if (const Piece* occupant = findOtherPieceAt(pieces, mover, r, c);
                occupant && std::find(overlaps.begin(), overlaps.end(), occupant) == overlaps.end())
                overlaps.push_back(occupant);
    return overlaps;
}

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
    const bool straight = deltaRow == 0 || deltaColumn == 0;
    const bool diagonal = absInt(deltaRow) == absInt(deltaColumn);
    if (!straight && !diagonal)
    {
        return true;
    }

    const int stepRow = (deltaRow > 0) - (deltaRow < 0);
    const int stepColumn = (deltaColumn > 0) - (deltaColumn < 0);
    int row = piece.row + stepRow;
    int column = piece.column + stepColumn;
    while (row != toRow || column != toColumn)
    {
        for (int footprintRow = row; footprintRow < row + piece.height; ++footprintRow)
            for (int footprintColumn = column; footprintColumn < column + piece.width; ++footprintColumn)
                if (findOtherPieceAt(pieces, piece, footprintRow, footprintColumn) != nullptr)
                    return false;
        row += stepRow;
        column += stepColumn;
    }
    return true;
}

inline bool rangedActionReachesTarget(
    const std::vector<Piece>& pieces,
    const Piece& attacker,
    const Piece& target,
    const ActionProfile& action)
{
    const int rowGap = std::max(
        0,
        std::max(target.row - (attacker.row + attacker.height - 1),
                 attacker.row - (target.row + target.height - 1)));
    const int columnGap = std::max(
        0,
        std::max(target.column - (attacker.column + attacker.width - 1),
                 attacker.column - (target.column + target.width - 1)));
    const int closestDistance = std::max(rowGap, columnGap);
    if (closestDistance < action.minRange || closestDistance > action.maxRange)
        return false;

    for (int attackerRow = attacker.row; attackerRow < attacker.row + attacker.height; ++attackerRow)
        for (int attackerColumn = attacker.column; attackerColumn < attacker.column + attacker.width; ++attackerColumn)
            for (int targetRow = target.row; targetRow < target.row + target.height; ++targetRow)
                for (int targetColumn = target.column; targetColumn < target.column + target.width; ++targetColumn)
                {
                    const int deltaRow = targetRow - attackerRow;
                    const int deltaColumn = targetColumn - attackerColumn;
                    if (std::max(absInt(deltaRow), absInt(deltaColumn)) != closestDistance)
                        continue;
                    if (!actionPatternMatches(
                            action.pattern, deltaRow, deltaColumn, closestDistance, closestDistance))
                        continue;

                    const bool straight = deltaRow == 0 || deltaColumn == 0;
                    const bool diagonal = absInt(deltaRow) == absInt(deltaColumn);
                    if (!straight && !diagonal)
                        return true;

                    const int stepRow = (deltaRow > 0) - (deltaRow < 0);
                    const int stepColumn = (deltaColumn > 0) - (deltaColumn < 0);
                    int row = attackerRow + stepRow;
                    int column = attackerColumn + stepColumn;
                    bool clear = true;
                    while (row != targetRow || column != targetColumn)
                    {
                        const Piece* blocker = findPieceAt(pieces, row, column);
                        if (blocker && blocker->id != attacker.id && blocker->id != target.id)
                        {
                            clear = false;
                            break;
                        }
                        row += stepRow;
                        column += stepColumn;
                    }
                    if (clear)
                        return true;
                }
    return false;
}

inline ActionResolution resolvePieceAction(
    const std::vector<Piece>& pieces,
    const std::array<std::uint8_t, BoardSquares>& holes,
    const Piece& piece,
    int toRow,
    int toColumn,
    bool attackingMovesOnly = false)
{
    ActionResolution best;
    if (!inBounds(toRow, toColumn) || piece.growTurnsRemaining > 0 || piece.disabledTurns > 0)
    {
        return best;
    }

    const Piece* destination = findOtherPieceAt(pieces, piece, toRow, toColumn);
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
        candidate.nextState = actionNextState(action);
        candidate.damage = action.damage;
        candidate.heal = action.heal;
        candidate.statusTurns = action.statusTurns;
        candidate.cooldownTurns = action.cooldownTurns;
        candidate.push = action.push;
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
                    if (action.canAttack &&
                        actionCanTarget(piece, *pivot, action.damage, action.heal, action.targetFilter))
                    {
                        candidate.attacks = true;
                        addActionTarget(candidate, *pivot);
                    }
                }
            }
        }
        else if (kind == ActionKind::Ranged)
        {
            if (action.canAttack && destination != nullptr &&
                actionCanTarget(
                    piece, *destination, action.damage, action.heal, action.targetFilter) &&
                rangedActionReachesTarget(pieces, piece, *destination, action))
            {
                candidate.legal = true;
                candidate.attacks = true;
                addActionTarget(candidate, *destination);
            }
        }
        else if (kind == ActionKind::Capture)
        {
            if (!action.canMove || !action.canAttack ||
                !actionPatternMatches(
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

            const std::vector<const Piece*> footprintTargets =
                piecesOverlappingFootprint(pieces, piece, toRow, toColumn);
            if (footprintTargets.empty() || std::any_of(
                    footprintTargets.begin(), footprintTargets.end(),
                    [&](const Piece* target) {
                        return !actionCanTarget(
                            piece, *target, action.damage, action.heal, action.targetFilter);
                    }))
            {
                continue;
            }

            candidate.legal = true;
            candidate.moves = true;
            candidate.attacks = true;
            for (const Piece* target : footprintTargets)
                addActionTarget(candidate, *target);
            if (!jumping)
            {
                const int stepRow = (deltaRow > 0) - (deltaRow < 0);
                const int stepColumn = (deltaColumn > 0) - (deltaColumn < 0);
                candidate.stagingRow = toRow - stepRow;
                candidate.stagingColumn = toColumn - stepColumn;
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

            const std::vector<const Piece*> footprintTargets = action.canMove
                ? piecesOverlappingFootprint(pieces, piece, toRow, toColumn)
                : std::vector<const Piece*>{};
            const bool invalidTargetOverlap = std::any_of(
                footprintTargets.begin(), footprintTargets.end(),
                [&](const Piece* target) {
                    return !actionCanTarget(
                        piece, *target, action.damage, action.heal, action.targetFilter);
                });
            if (!invalidTargetOverlap && action.canAttack && !footprintTargets.empty())
            {
                for (const Piece* target : footprintTargets)
                    addActionTarget(candidate, *target);
                candidate.legal = !candidate.targetIds.empty();
                candidate.attacks = candidate.legal;
                candidate.moves = action.canMove;
                if (candidate.moves && !jumping)
                {
                    const int stepRow = (deltaRow > 0) - (deltaRow < 0);
                    const int stepColumn = (deltaColumn > 0) - (deltaColumn < 0);
                    candidate.stagingRow = toRow - stepRow;
                    candidate.stagingColumn = toColumn - stepColumn;
                }
            }
            else if (destination == nullptr && action.canMove)
            {
                candidate.legal = true;
                candidate.moves = true;
            }
            else if (destination != nullptr && action.canAttack &&
                     actionCanTarget(
                         piece, *destination, action.damage, action.heal, action.targetFilter))
            {
                candidate.legal = true;
                candidate.attacks = true;
                candidate.moves = action.canMove;
                addActionTarget(candidate, *destination);
                if (candidate.moves && !jumping)
                {
                    const int stepRow = (deltaRow > 0) - (deltaRow < 0);
                    const int stepColumn = (deltaColumn > 0) - (deltaColumn < 0);
                    candidate.stagingRow = toRow - stepRow;
                    candidate.stagingColumn = toColumn - stepColumn;
                }
            }
        }

        if (candidate.legal && candidate.moves && !candidate.attacks &&
            !pieceFootprintFree(pieces, piece, toRow, toColumn))
        {
            candidate.legal = false;
        }
        if (!candidate.legal || (candidate.moves && piece.sleepTurnsRemaining > 0) ||
            (attackingMovesOnly && !(candidate.moves && candidate.attacks)))
        {
            continue;
        }

        const int candidateImpact = candidate.attacks
            ? std::max(candidate.damage, candidate.heal) + candidate.statusTurns + candidate.push
            : 0;
        const int bestImpact = best.attacks
            ? std::max(best.damage, best.heal) + best.statusTurns + best.push
            : 0;
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

// ---- dematerialized (hidden) piece handling --------------------------------

// Turns a revealed dematerialized piece spends stunned.
constexpr int HiddenRevealStunTurns = 1;

// Dematerialized pieces exert no control: the shared control map would
// otherwise betray a hidden piece's position as territory flips to its owner.
inline bool pieceExertsControl(const Piece& piece)
{
    return piece.canControl && !piece.hidden;
}

// The board as one player sees it: opposing dematerialized pieces are absent.
inline std::vector<Piece> piecesVisibleTo(const std::vector<Piece>& pieces, int playerNumber)
{
    std::vector<Piece> visible;
    visible.reserve(pieces.size());
    for (const Piece& piece : pieces)
    {
        if (!piece.hidden || piece.owner == playerNumber)
        {
            visible.push_back(piece);
        }
    }
    return visible;
}

// A piece action resolved against the acting player's view of the board, then
// adjusted for any hidden enemy piece it would actually run into:
//  - an attacking move that reaches the hidden piece strikes it instead,
//  - any other movement halts short of it without dealing damage,
//  - a hop whose landing square is occupied fails entirely (no pivot damage).
// In every collision the hidden piece is identified so the caller can
// materialize and stun it.
struct PieceActionOutcome
{
    ActionResolution action;
    int destinationRow = 0;
    int destinationColumn = 0;
    int revealedPieceId = 0;  // hidden piece struck or bumped into (0 = none)
};

inline PieceActionOutcome resolvePieceActionThroughHidden(
    const std::vector<Piece>& pieces,
    const std::array<std::uint8_t, BoardSquares>& holes,
    const Piece& piece,
    int toRow,
    int toColumn)
{
    PieceActionOutcome outcome;
    outcome.destinationRow = toRow;
    outcome.destinationColumn = toColumn;

    const std::vector<Piece> visible = piecesVisibleTo(pieces, piece.owner);
    outcome.action = resolvePieceAction(visible, holes, piece, toRow, toColumn);
    if (!outcome.action.legal || !outcome.action.moves)
    {
        return outcome;
    }

    auto hiddenEnemyAt = [&](int row, int column) -> const Piece* {
        const Piece* occupant = findPieceAt(pieces, row, column);
        return occupant != nullptr && occupant->hidden && occupant->owner != piece.owner
            ? occupant
            : nullptr;
    };

    const ActionProfile& profile =
        piece.actions[static_cast<std::size_t>(outcome.action.actionIndex)];
    const ActionKind kind = static_cast<ActionKind>(profile.kind);
    const bool jumping = static_cast<MovePattern>(profile.pattern) == MovePattern::Jump;
    const bool walksPath = (kind == ActionKind::Slide || kind == ActionKind::Capture) &&
        !jumping && !profile.passThrough;

    const Piece* hiddenBlocker = nullptr;
    int stopRow = piece.row;
    int stopColumn = piece.column;
    if (walksPath)
    {
        const int deltaRow = toRow - piece.row;
        const int deltaColumn = toColumn - piece.column;
        const int stepRow = (deltaRow > 0) - (deltaRow < 0);
        const int stepColumn = (deltaColumn > 0) - (deltaColumn < 0);
        int row = piece.row;
        int column = piece.column;
        while (row != toRow || column != toColumn)
        {
            row += stepRow;
            column += stepColumn;
            hiddenBlocker = hiddenEnemyAt(row, column);
            if (hiddenBlocker != nullptr)
            {
                break;
            }
            stopRow = row;
            stopColumn = column;
        }
    }
    else
    {
        // Jumps, hops, teleports, tunnels, and pass-through moves only collide
        // at the destination square; a failed one leaves the piece in place.
        hiddenBlocker = hiddenEnemyAt(toRow, toColumn);
    }

    if (hiddenBlocker == nullptr)
    {
        return outcome;
    }

    outcome.revealedPieceId = hiddenBlocker->id;

    // An attacking move that can reach the hidden square strikes the piece as
    // if it had been visible (damage, staging, and status apply normally).
    const ActionResolution strike = resolvePieceAction(
        pieces, holes, piece, hiddenBlocker->row, hiddenBlocker->column, true);
    if (strike.legal && strike.attacks && strike.moves &&
        strike.targetId == hiddenBlocker->id)
    {
        outcome.action = strike;
        outcome.destinationRow = hiddenBlocker->row;
        outcome.destinationColumn = hiddenBlocker->column;
        return outcome;
    }

    // Otherwise the mover bumps into it harmlessly and stops short.
    outcome.action.attacks = false;
    outcome.action.targetId = 0;
    outcome.action.targetIds.clear();
    outcome.action.damage = 0;
    outcome.action.heal = 0;
    outcome.action.push = 0;
    outcome.action.statusTurns = 0;
    outcome.action.moves = stopRow != piece.row || stopColumn != piece.column;
    outcome.destinationRow = stopRow;
    outcome.destinationColumn = stopColumn;
    return outcome;
}

// Materializes a piece that was struck or bumped into while dematerialized and
// stuns it. Damage-based status (if any) is applied separately by the attack.
inline void materializeRevealedPiece(Piece& piece)
{
    piece.hidden = false;
    piece.actionState = 0;
    applyDamageStatus(piece, 0, HiddenRevealStunTurns);
}

inline std::string pieceAbilityLabel(const Piece& piece)
{
    const std::string ability = normalizedAbility(piece.ability);
    if (ability.empty())
    {
        return "";
    }
    if (!piece.abilityLabels.empty())
    {
        const std::size_t index = static_cast<std::size_t>(
            piece.actionState % static_cast<int>(piece.abilityLabels.size()));
        return piece.abilityLabels[index];
    }
    if (ability == "dig")
    {
        return "Dig";
    }
    if (ability == "summon")
    {
        return "Summon";
    }
    if (ability == "command")
    {
        return "Command";
    }
    if (ability == "transform")
    {
        return "Transform";
    }
    if (ability == "dematerialize")
    {
        return piece.actionState == 0 ? "Dematerialize" : "Materialize";
    }
    return "Use Ability";
}

inline bool pieceHasTrailAbility(const Piece& piece)
{
    return hasKeyword(piece.keywords, "trail") && !piece.summonTitle.empty();
}

inline bool piecesAreAdjacent(const Piece& first, const Piece& second)
{
    const int rowGap = std::max(
        0,
        std::max(second.row - (first.row + first.height - 1),
                 first.row - (second.row + second.height - 1)));
    const int columnGap = std::max(
        0,
        std::max(second.column - (first.column + first.width - 1),
                 first.column - (second.column + second.width - 1)));
    return std::max(rowGap, columnGap) == 1;
}

struct PushResult
{
    int movedSquares = 0;
    int preventedSquares = 0;
};

// Pushes a surviving enemy directly away from the attack's staging square.
// Each axis is reduced to -1, 0, or 1, selecting one of the eight board
// directions. For large pieces, the target square closest to staging is used.
// Knight offsets therefore push diagonally, toward the nearest eight-way
// approximation of the L-shaped attack vector.
inline PushResult applyActionPush(
    std::vector<Piece>& pieces,
    int targetId,
    int stagingRow,
    int stagingColumn,
    int distance)
{
    PushResult result;
    const int requestedSquares = std::max(0, distance);
    if (requestedSquares == 0)
    {
        return result;
    }

    const auto targetIt = std::find_if(
        pieces.begin(),
        pieces.end(),
        [targetId](const Piece& piece) { return piece.id == targetId; });
    if (targetIt == pieces.end())
    {
        return result;
    }

    Piece& target = *targetIt;
    const int nearestTargetRow = std::clamp(
        stagingRow, target.row, target.row + target.height - 1);
    const int nearestTargetColumn = std::clamp(
        stagingColumn, target.column, target.column + target.width - 1);
    const int stepRow = (nearestTargetRow > stagingRow) - (nearestTargetRow < stagingRow);
    const int stepColumn =
        (nearestTargetColumn > stagingColumn) - (nearestTargetColumn < stagingColumn);

    if (stepRow == 0 && stepColumn == 0)
    {
        result.preventedSquares = requestedSquares;
        applyActionDamage(target, result.preventedSquares, 0);
        return result;
    }

    while (result.movedSquares < requestedSquares)
    {
        const int nextRow = target.row + stepRow;
        const int nextColumn = target.column + stepColumn;
        if (!pieceFootprintFree(pieces, target, nextRow, nextColumn))
        {
            break;
        }
        target.row = nextRow;
        target.column = nextColumn;
        ++result.movedSquares;
    }

    result.preventedSquares = requestedSquares - result.movedSquares;
    if (result.preventedSquares > 0)
    {
        applyActionDamage(target, result.preventedSquares, 0);
    }
    return result;
}

struct DamageAssignment
{
    int pieceId = 0;
    int damage = 0;
};

// Applies positive action damage, redirecting it from a non-Bodyguard
// piece to its adjacent friendly Bodyguards. Every protector receives the same
// base amount; only the indivisible remainder is assigned randomly.
template <typename RandomEngine>
inline std::vector<DamageAssignment> applyDamageWithBodyguards(
    std::vector<Piece>& pieces,
    int targetId,
    int damage,
    int statusTurns,
    RandomEngine& randomEngine)
{
    const auto targetIt = std::find_if(
        pieces.begin(),
        pieces.end(),
        [targetId](const Piece& piece) { return piece.id == targetId; });
    if (targetIt == pieces.end())
    {
        return {};
    }

    std::vector<DamageAssignment> assignments;
    if (damage > 0 && !hasKeyword(targetIt->keywords, "bodyguard"))
    {
        std::vector<int> bodyguardIds;
        for (const Piece& candidate : pieces)
        {
            if (candidate.id != targetIt->id && candidate.owner == targetIt->owner &&
                hasKeyword(candidate.keywords, "bodyguard") &&
                piecesAreAdjacent(*targetIt, candidate))
            {
                bodyguardIds.push_back(candidate.id);
            }
        }

        if (!bodyguardIds.empty())
        {
            const int baseDamage = damage / static_cast<int>(bodyguardIds.size());
            const int remainder = damage % static_cast<int>(bodyguardIds.size());
            if (remainder > 0)
            {
                std::shuffle(bodyguardIds.begin(), bodyguardIds.end(), randomEngine);
            }
            for (std::size_t index = 0; index < bodyguardIds.size(); ++index)
            {
                const int assignedDamage = baseDamage +
                    (static_cast<int>(index) < remainder ? 1 : 0);
                if (assignedDamage > 0)
                {
                    assignments.push_back({bodyguardIds[index], assignedDamage});
                }
            }
        }
    }

    if (assignments.empty())
    {
        assignments.push_back({targetId, damage});
    }

    for (const DamageAssignment& assignment : assignments)
    {
        const auto recipient = std::find_if(
            pieces.begin(),
            pieces.end(),
            [&](const Piece& piece) { return piece.id == assignment.pieceId; });
        if (recipient != pieces.end())
        {
            applyActionDamage(*recipient, assignment.damage, statusTurns);
        }
    }
    return assignments;
}

inline bool pieceCanReceiveCommand(const Piece& commander, const Piece& target)
{
    return commander.id != target.id && commander.owner == target.owner &&
        !target.hasActed && target.growTurnsRemaining <= 0 && target.disabledTurns <= 0 &&
        piecesAreAdjacent(commander, target);
}

inline std::pair<int, int> summonDestination(const Piece& piece)
{
    return {piece.row, piece.column + (piece.owner == 1 ? 1 : -1)};
}

inline bool pieceSummonDestinationFree(const std::vector<Piece>& pieces, const Piece& piece)
{
    const auto [row, column] = summonDestination(piece);
    return inBounds(row, column) && findPieceAt(pieces, row, column) == nullptr;
}

inline bool pieceAbilityAvailable(const Piece& piece)
{
    const std::string ability = normalizedAbility(piece.ability);
    if (ability.empty() || piece.growTurnsRemaining > 0 || piece.disabledTurns > 0)
    {
        return false;
    }
    if (ability == "dig")
    {
        return piece.abilityUses != 0;
    }
    if (ability == "summon")
    {
        return !piece.summonTitle.empty();
    }
    return ability == "transform" || ability == "dematerialize" || ability == "command";
}

inline bool pieceAbilityAvailable(const std::vector<Piece>& pieces, const Piece& piece)
{
    if (!pieceAbilityAvailable(piece))
    {
        return false;
    }
    const std::string ability = normalizedAbility(piece.ability);
    if (ability == "summon")
    {
        return pieceSummonDestinationFree(pieces, piece);
    }
    if (ability == "command")
    {
        return std::any_of(
            pieces.begin(),
            pieces.end(),
            [&](const Piece& target) { return pieceCanReceiveCommand(piece, target); });
    }
    return true;
}

} // namespace game_data
