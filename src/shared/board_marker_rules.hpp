#pragma once
#include "game_data.hpp"

namespace game_data {

inline bool placeTraceMarker(std::vector<BoardMarker>& markers, const Piece& source, int row, int column)
{
    if (!inBounds(row,column) || source.health <= 0 || !source.tracesMarker ||
        !marker_data::valid(*source.tracesMarker) || markers.size() >= MaxBoardMarkers ||
        source.owner < 1 || source.owner > 2 || !hasKeyword(source.passives,"traces"))
        return false;
    const auto key = marker_data::nameKey(source.tracesMarker->name);
    if (std::any_of(markers.begin(), markers.end(), [&](const BoardMarker& marker) {
            return marker.row == row && marker.column == column &&
                   marker_data::nameKey(marker.definition.name) == key;
        }))
        return false;
    markers.push_back({source.owner,row,column,*source.tracesMarker});
    return true;
}

inline bool applyMarkerEntry(std::vector<BoardMarker>& markers, const std::vector<Piece>& pieces,
                             Piece& entrant, int priorRow, int priorColumn)
{
    if (entrant.health <= 0) return false;
    const auto contains = [&](int anchorRow,int anchorColumn,const BoardMarker& marker) {
        return marker.row >= anchorRow && marker.row < anchorRow+entrant.height &&
               marker.column >= anchorColumn && marker.column < anchorColumn+entrant.width;
    };
    bool consumed = false;
    for (auto it = markers.begin(); it != markers.end();)
    {
        if (!contains(entrant.row,entrant.column,*it) ||
            (priorRow >= 0 && priorColumn >= 0 && contains(priorRow,priorColumn,*it)) ||
            !marker_data::valid(it->definition) ||
            (it->definition.eligibility == marker_data::Eligibility::Friendly && it->owner != entrant.owner) ||
            (it->definition.eligibility == marker_data::Eligibility::Enemy && it->owner == entrant.owner))
        {
            ++it;
            continue;
        }
        // Eligibility, not Health restored, spends the marker.
        if (it->definition.effect == marker_data::Effect::Heal)
            applyActionHealing(pieces,entrant,it->definition.strength,0);
        else
            applyActionDamage(entrant,it->definition.strength,0);
        it=markers.erase(it);
        consumed=true;
        if (entrant.health <= 0 && !reviveUndyingPiece(entrant)) break;
    }
    return consumed;
}

} // namespace game_data
