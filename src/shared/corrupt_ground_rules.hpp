#pragma once

#include "game_rules.hpp"

namespace game_data
{
// Shared Blight mechanics. The activated ability is Corrupt the Ground.
// Traps are hidden from enemies; owners see their own markers. Lifecycle is owned
// by the game loop: no expiry, one marker per owner/square, consumed on enemy contact.
inline const char* trapObjectName(TrapEffect effect)
{
    return effect == TrapEffect::Damage ? "Blight" : "Trap";
}


enum class CorruptGroundPlacement { Blocked, PlaceTrap, RevealOccupant };
struct CorruptGroundPlacementResult
{
    CorruptGroundPlacement outcome = CorruptGroundPlacement::Blocked;
    int row = 0;
    int column = 0;
    int hiddenOccupantId = 0;
    bool spendsAction() const { return outcome != CorruptGroundPlacement::Blocked; }
};

// Pure preview. Like Summon, the owner determines forward, not last movement.
// The caller still enforces turn readiness, Silence, Command and action costs.
inline CorruptGroundPlacementResult inspectCorruptGroundPlacement(
    const std::vector<Piece>& pieces, const Piece& source,
    std::optional<int> configuredStrength)
{
    const auto [row, column] = summonDestination(source);
    CorruptGroundPlacementResult result{CorruptGroundPlacement::Blocked, row, column, 0};
    if (source.health <= 0 || (source.owner != 1 && source.owner != 2) ||
        !configuredStrength || *configuredStrength < 0 || !inBounds(row, column)) return result;
    const Piece* occupant = findPieceAt(pieces, row, column);
    if (!occupant) result.outcome = CorruptGroundPlacement::PlaceTrap;
    else if (occupant->hidden && occupant->owner != source.owner)
    {
        result.outcome = CorruptGroundPlacement::RevealOccupant;
        result.hiddenOccupantId = occupant->id;
    }
    return result;
}

struct CorruptGroundActivationResult
{
    CorruptGroundPlacementResult placement;
    std::optional<Trap> trap;
};

// Returns a marker rather than inserting it into a match, allowing integration
// to supply the still-undecided stacking, visibility and lifetime policy.
inline CorruptGroundActivationResult activateCorruptGround(
    std::vector<Piece>& pieces, const Piece& source, std::optional<int> configuredStrength)
{
    CorruptGroundActivationResult result{inspectCorruptGroundPlacement(pieces, source, configuredStrength), std::nullopt};
    if (result.placement.outcome == CorruptGroundPlacement::PlaceTrap)
        result.trap = Trap{source.owner, result.placement.row, result.placement.column, *configuredStrength};
    else if (result.placement.outcome == CorruptGroundPlacement::RevealOccupant)
    {
        for (Piece& occupant : pieces)
            if (occupant.id == result.placement.hiddenOccupantId) materializeRevealedPiece(occupant);
    }
    return result;
}

// Invoke at each actual physical path contact or landing, not visual animation frames.
// Teleport paths must not synthesize intermediate contacts. Returns raw
// Strength damage; caller applies normal damage/death handling and explicitly
// chooses whether the triggered marker is consumed. Friendly landings return
// no trigger, so they must neither remove nor otherwise alter the marker.
inline std::optional<DamageAssignment> trapContact(
    const Trap& trap, const Piece& contact)
{
    if ((trap.owner != 1 && trap.owner != 2) || trap.strength < 0 || trap.effect != TrapEffect::Damage || !inBounds(trap.row, trap.column) ||
        contact.health <= 0 || (contact.owner != 1 && contact.owner != 2) || contact.owner == trap.owner ||
        trap.row < contact.row || trap.row >= contact.row + contact.height ||
        trap.column < contact.column || trap.column >= contact.column + contact.width) return std::nullopt;
    return DamageAssignment{contact.id, trap.strength};
}
// Keep unresolved forced-movement policy explicit. A caller must select
// a resolved mode, not treat unsupported movement as ordinary ground travel.
enum class TrapTravel { Ground, Jump, Flying, Unresolved };
inline std::optional<std::vector<std::pair<int,int>>> trapContactAnchors(
    int fromRow, int fromColumn, int toRow, int toColumn, TrapTravel travel)
{
    if (travel == TrapTravel::Unresolved || !inBounds(fromRow,fromColumn) || !inBounds(toRow,toColumn))
        return std::nullopt;
    std::vector<std::pair<int,int>> anchors;
    if (travel == TrapTravel::Flying) return anchors; // Immune even on landing.
    if (fromRow == toRow && fromColumn == toColumn) return anchors;
    if (travel == TrapTravel::Jump)
    {
        anchors.emplace_back(toRow,toColumn);
        return anchors;
    }
    const int dr=toRow-fromRow, dc=toColumn-fromColumn;
    // Ground actions are straight/diagonal; never guess a route for L-shapes.
    if (dr != 0 && dc != 0 && std::abs(dr) != std::abs(dc)) return std::nullopt;
    const int stepRow=(dr>0)-(dr<0), stepColumn=(dc>0)-(dc<0);
    while (fromRow != toRow || fromColumn != toColumn)
    {
        fromRow += stepRow;
        fromColumn += stepColumn;
        anchors.emplace_back(fromRow,fromColumn);
    }
    return anchors;
}
inline bool pieceIgnoresBlight(const Piece& piece)
{
    if (hasKeyword(piece.passives,"flying") || hasKeyword(piece.passives,"fly")) return true;
    return std::any_of(piece.actions.begin(),piece.actions.end(),[&](const ActionProfile& action) {
        return action.state == piece.actionState && action.canMove && action.passThrough &&
            static_cast<ActionKind>(action.kind) == ActionKind::Slide &&
            static_cast<MovePattern>(action.pattern) != MovePattern::Jump;
    });
}
inline TrapTravel blightTravel(const Piece& piece, const ActionProfile& action)
{
    if (pieceIgnoresBlight(piece)) return TrapTravel::Flying;
    const auto kind=static_cast<ActionKind>(action.kind);
    if (static_cast<MovePattern>(action.pattern)==MovePattern::Jump || kind==ActionKind::Hop ||
        kind==ActionKind::Teleport || kind==ActionKind::Tunnel) return TrapTravel::Jump;
    return TrapTravel::Ground;
}
// No piece erasure here: displacement callbacks may hold vector references.
// Owning engine resolves lethal damage through its ordinary death pipeline.
inline bool applyBlightContact(std::vector<Trap>& traps, Piece& piece)
{
    if (pieceIgnoresBlight(piece)) return false;
    bool triggered=false;
    for (auto it=traps.begin(); it!=traps.end();)
    {
        const auto hit=trapContact(*it,piece);
        if (!hit) { ++it; continue; }
        const int damage=hit->damage;
        it=traps.erase(it);
        applyActionDamage(piece,damage,0);
        triggered=true;
        if (piece.health<=0 && !reviveUndyingPiece(piece)) break;
    }
    return triggered;
}
} // namespace game_data
