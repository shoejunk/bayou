#include "client_board_layout.hpp"

#include <cmath>

namespace bayou::client
{

sf::Color ownerColor(int owner)
{
    if (owner == 1) return sf::Color(80, 132, 214);
    if (owner == 2) return sf::Color(214, 102, 74);
    return sf::Color(120, 124, 134);
}

sf::Color ownerTint(int owner)
{
    if (owner == 1) return sf::Color(24, 64, 72, 226);
    if (owner == 2) return sf::Color(88, 48, 36, 226);
    return sf::Color(38, 48, 43, 214);
}

int screenRowForViewer(int row, int /*viewer*/)
{
    return game_data::BoardSize - 1 - row;
}

int rowForScreenRow(int screenRow, int /*viewer*/)
{
    return game_data::BoardSize - 1 - screenRow;
}

sf::Vector2f boardEdgePoint(int screenEdge, int columnEdge)
{
    const float t = static_cast<float>(screenEdge) / static_cast<float>(game_data::BoardSize);
    const float y = BoardOriginY + BoardHeight * std::pow(t, BoardPerspectiveExponent);
    const float width = BoardTopWidth + (BoardBottomWidth - BoardTopWidth) * t;
    const float left = BoardCenterX - width * 0.5f;
    return {
        left + width * static_cast<float>(columnEdge) / static_cast<float>(game_data::BoardSize),
        y};
}

float pieceScaleForScreenRow(int screenRow)
{
    const float t = static_cast<float>(screenRow) / static_cast<float>(game_data::BoardSize - 1);
    return PieceFarScale + (PieceNearScale - PieceFarScale) * t;
}

BoardCellMetrics boardCellMetricsForViewer(int row, int column, int viewer)
{
    BoardCellMetrics metrics;
    metrics.screenRow = screenRowForViewer(row, viewer);
    metrics.corners = {
        boardEdgePoint(metrics.screenRow, column),
        boardEdgePoint(metrics.screenRow, column + 1),
        boardEdgePoint(metrics.screenRow + 1, column + 1),
        boardEdgePoint(metrics.screenRow + 1, column)};
    metrics.center = {
        (metrics.corners[0].x + metrics.corners[1].x + metrics.corners[2].x + metrics.corners[3].x) * 0.25f,
        (metrics.corners[0].y + metrics.corners[1].y + metrics.corners[2].y + metrics.corners[3].y) * 0.25f};
    metrics.height = metrics.corners[3].y - metrics.corners[0].y;
    metrics.depthScale = pieceScaleForScreenRow(metrics.screenRow);
    return metrics;
}

sf::Vector2f boardCellAnchor(const BoardCellMetrics& metrics)
{
    return {metrics.center.x, metrics.center.y + metrics.height * 0.36f};
}

bool pointInConvex(sf::Vector2f point, const std::array<sf::Vector2f, 4>& corners)
{
    bool hasNegative = false;
    bool hasPositive = false;
    for (std::size_t i = 0; i < corners.size(); ++i)
    {
        const sf::Vector2f a = corners[i];
        const sf::Vector2f b = corners[(i + 1) % corners.size()];
        const float cross = (b.x - a.x) * (point.y - a.y) - (b.y - a.y) * (point.x - a.x);
        hasNegative = hasNegative || cross < -0.01f;
        hasPositive = hasPositive || cross > 0.01f;
        if (hasNegative && hasPositive)
        {
            return false;
        }
    }
    return true;
}

std::array<sf::Vector2f, 4> offsetQuad(std::array<sf::Vector2f, 4> corners, sf::Vector2f offset)
{
    for (sf::Vector2f& corner : corners)
    {
        corner += offset;
    }
    return corners;
}

sf::FloatRect pieceTargetRect(sf::Vector2f anchor, float scale, bool walkSheet)
{
    const float width = PieceBaseWidth * scale;
    const float height = (walkSheet ? PieceWalkBaseHeight : PieceBaseHeight) * scale;
    return {{anchor.x - width * 0.5f, anchor.y - height}, {width, height}};
}

} // namespace bayou::client
