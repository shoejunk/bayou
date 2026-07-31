#include "client_board_layout.hpp"

#include <algorithm>
#include <cmath>

namespace bayou::client
{
namespace
{

// Rings are cheap and the board only ever draws a few dozen of them, so soft
// edges come from stacking rather than from a shader or a blurred texture.
constexpr std::size_t EllipsePoints = 44;

sf::CircleShape makeEllipse(sf::Vector2f center, float radiusX, float radiusY)
{
    const float radius = std::max(0.01f, radiusX);
    sf::CircleShape shape(radius, EllipsePoints);
    shape.setOrigin({radius, radius});
    shape.setPosition(center);
    shape.setScale({1.0f, radiusY / radius});
    return shape;
}

} // namespace

// Player one holds the verdigris side of the palette and player two the ember
// side. The previous raw blue and orange sat outside the game's colour world.
sf::Color ownerColor(int owner)
{
    if (owner == 1) return sf::Color(96, 176, 156);
    if (owner == 2) return sf::Color(202, 104, 82);
    return sf::Color(150, 140, 118);
}

sf::Color ownerColorDeep(int owner)
{
    if (owner == 1) return sf::Color(18, 54, 50);
    if (owner == 2) return sf::Color(62, 24, 20);
    return sf::Color(30, 34, 32);
}

sf::Color ownerColorBright(int owner)
{
    if (owner == 1) return sf::Color(158, 226, 202);
    if (owner == 2) return sf::Color(240, 158, 132);
    return sf::Color(214, 202, 176);
}

// A wash, not a paint-bucket fill: the stone underneath has to stay visible or
// the board reads as two flat blocks of colour.
sf::Color ownerTint(int owner)
{
    if (owner == 1) return sf::Color(30, 88, 80, 104);
    if (owner == 2) return sf::Color(104, 44, 36, 104);
    return sf::Color(0, 0, 0, 0);
}

sf::Color shadeColor(sf::Color color, float factor)
{
    const auto channel = [factor](std::uint8_t value) {
        return static_cast<std::uint8_t>(
            std::clamp(static_cast<float>(value) * factor, 0.0f, 255.0f));
    };
    return {channel(color.r), channel(color.g), channel(color.b), color.a};
}

sf::Color withAlpha(sf::Color color, int alpha)
{
    color.a = static_cast<std::uint8_t>(std::clamp(alpha, 0, 255));
    return color;
}

void drawGradientQuad(
    sf::RenderTarget& target,
    const std::array<sf::Vector2f, 4>& corners,
    sf::Color farColor,
    sf::Color nearColor)
{
    sf::VertexArray quad(sf::PrimitiveType::TriangleFan, 4);
    quad[0] = {corners[0], farColor};
    quad[1] = {corners[1], farColor};
    quad[2] = {corners[2], nearColor};
    quad[3] = {corners[3], nearColor};
    target.draw(quad);
}

void drawEdgeLine(
    sf::RenderTarget& target, sf::Vector2f from, sf::Vector2f to, float thickness, sf::Color color)
{
    const sf::Vector2f delta = to - from;
    const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (length < 0.001f)
    {
        return;
    }
    sf::RectangleShape line({length, std::max(0.4f, thickness)});
    line.setOrigin({0.0f, line.getSize().y * 0.5f});
    line.setPosition(from);
    line.setRotation(sf::radians(std::atan2(delta.y, delta.x)));
    line.setFillColor(color);
    target.draw(line);
}

void drawSoftEllipse(
    sf::RenderTarget& target,
    sf::Vector2f center,
    float radiusX,
    float radiusY,
    sf::Color color,
    int layers)
{
    const int count = std::max(1, layers);
    for (int i = 0; i < count; ++i)
    {
        // Grow outward while fading, so the stack reads as one blurred blob.
        const float t = static_cast<float>(i) / static_cast<float>(count);
        const float grow = 1.0f + t * 0.85f;
        const float falloff = (1.0f - t) * (1.0f - t);
        sf::CircleShape ring = makeEllipse(center, radiusX * grow, radiusY * grow);
        ring.setFillColor(withAlpha(color, static_cast<int>(color.a * falloff * 0.62f)));
        target.draw(ring);
    }
}

void drawEllipseOutline(
    sf::RenderTarget& target,
    sf::Vector2f center,
    float radiusX,
    float radiusY,
    float thickness,
    sf::Color color)
{
    sf::CircleShape ring = makeEllipse(center, radiusX, radiusY);
    ring.setFillColor(sf::Color::Transparent);
    // The vertical squash also squashes the outline, so pre-compensate.
    ring.setOutlineThickness(std::max(0.5f, thickness));
    ring.setOutlineColor(color);
    target.draw(ring);
}

void drawPieceBase(
    sf::RenderTarget& target,
    sf::Vector2f anchor,
    float scale,
    int owner,
    bool exhausted,
    float footprintWidth)
{
    const float spread = std::max(1.0f, footprintWidth);
    const float radiusX = 25.0f * scale * spread;
    const float radiusY = 9.0f * scale;
    const float dim = exhausted ? 0.55f : 1.0f;

    const sf::Vector2f baseCenter{anchor.x, anchor.y - PieceBaseLift * scale};

    // Contact shadow, offset a touch down-right to agree with the lantern-lit
    // backdrop, so the piece sits in the scene instead of floating over it.
    drawSoftEllipse(
        target,
        {baseCenter.x + 2.0f * scale, baseCenter.y + 2.0f * scale},
        radiusX * 0.92f,
        radiusY * 0.9f,
        sf::Color(0, 0, 0, 190),
        6);

    sf::CircleShape plinth = makeEllipse(baseCenter, radiusX, radiusY);
    plinth.setFillColor(withAlpha(shadeColor(ownerColorDeep(owner), dim), 232));
    target.draw(plinth);

    // Lit upper lip and a brass rim: two hairlines are enough to read as a
    // machined disc rather than a filled circle.
    drawEllipseOutline(
        target,
        {baseCenter.x, baseCenter.y - 0.8f * scale},
        radiusX * 0.93f,
        radiusY * 0.86f,
        1.0f,
        withAlpha(shadeColor(ownerColor(owner), dim), 150));
    drawEllipseOutline(
        target, baseCenter, radiusX, radiusY, 1.4f, withAlpha(shadeColor(BoardBrass, dim), 208));
}

void drawPieceSelectionRing(
    sf::RenderTarget& target, sf::Vector2f anchor, float scale, float pulse, sf::Color accent)
{
    const float radiusX = 29.0f * scale;
    const float radiusY = 10.5f * scale;
    const float swell = 1.0f + pulse * 0.09f;
    const sf::Vector2f baseCenter{anchor.x, anchor.y - PieceBaseLift * scale};

    drawSoftEllipse(
        target,
        baseCenter,
        radiusX * swell,
        radiusY * swell,
        withAlpha(accent, static_cast<int>(112.0f + 64.0f * pulse)),
        5);
    drawEllipseOutline(
        target, baseCenter, radiusX * swell, radiusY * swell, 2.4f, withAlpha(accent, 244));
    drawEllipseOutline(
        target,
        baseCenter,
        radiusX * swell * 1.16f,
        radiusY * swell * 1.16f,
        1.2f,
        withAlpha(accent, static_cast<int>(92.0f + 64.0f * pulse)));
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

sf::Vector2f boardFootprintAnchor(int row, int column, int width, int viewer)
{
    const sf::Vector2f left = boardCellAnchor(boardCellMetricsForViewer(row, column, viewer));
    const sf::Vector2f right = boardCellAnchor(
        boardCellMetricsForViewer(row, column + std::max(1, width) - 1, viewer));
    return {(left.x + right.x) * 0.5f, (left.y + right.y) * 0.5f};
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

sf::FloatRect pieceTargetRect(
    sf::Vector2f anchor, float scale, bool walkSheet, int footprintWidth, int footprintHeight)
{
    // Preserve the artwork's aspect ratio while making its rendered area grow
    // in proportion to the number of occupied board squares.
    const float footprintScale = std::sqrt(
        static_cast<float>(std::max(1, footprintWidth) * std::max(1, footprintHeight)));
    const float width = PieceBaseWidth * scale * footprintScale;
    const float height = (walkSheet ? PieceWalkBaseHeight : PieceBaseHeight) * scale * footprintScale;
    return {{anchor.x - width * 0.5f, anchor.y + PieceStandOffset * scale - height}, {width, height}};
}

} // namespace bayou::client
