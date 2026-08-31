#include "client_board_layout.hpp"
#include "client_ui.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace bayou::client
{
namespace
{

// Rings are cheap and the board only ever draws a few dozen of them, so soft
// edges come from stacking rather than from a shader or a blurred texture.
constexpr std::size_t EllipsePoints = 44;

// Alpha-bound centers and socket centers were measured from the 512px team
// variants. Using visible-art origins keeps both normal and 4x4 bases centered
// on their projected footprints even though their canvases have equal sizes.
constexpr float PieceBaseArtworkCanvasWidth = 76.0f;
constexpr sf::Vector2f PieceBaseArtworkCenter{256.25f, 256.75f};
constexpr sf::Vector2f PieceBaseLargeArtworkCenter{257.0f, 255.25f};
constexpr sf::Vector2f PieceBaseSocket{256.0f, 366.5f};
constexpr sf::Vector2f PieceBaseLargeSocket{256.5f, 365.0f};
constexpr sf::Vector2f PieceBaseSocketMaskSize{40.0f, 50.0f};
constexpr sf::Vector2f PieceBaseLargeSocketMaskSize{27.0f, 31.0f};
constexpr float PieceBaseGemScale = 0.75f;
constexpr float PieceBaseLargeGemScale = 0.50f;

struct PieceBaseArtworkLayout
{
    bool usesLargeBase = false;
    sf::Vector2f artworkCenter{};
    sf::Vector2f artworkScale{};
    sf::Vector2f socketPosition{};
};

bool resolvePieceBaseArtworkLayout(
    const sf::Texture* artwork,
    sf::Vector2f baseCenter,
    float scale,
    float spreadX,
    float spreadY,
    PieceBaseArtworkLayout& result)
{
    if (!artwork)
    {
        return false;
    }
    const sf::Vector2u textureSize = artwork->getSize();
    if (textureSize.x == 0 || textureSize.y == 0)
    {
        return false;
    }

    result.usesLargeBase = spreadX >= 3.5f && spreadY >= 3.5f;
    result.artworkCenter = result.usesLargeBase
        ? PieceBaseLargeArtworkCenter
        : PieceBaseArtworkCenter;
    const sf::Vector2f socket = result.usesLargeBase
        ? PieceBaseLargeSocket
        : PieceBaseSocket;
    const float artworkScale =
        PieceBaseArtworkCanvasWidth * scale / static_cast<float>(textureSize.x);
    result.artworkScale = {artworkScale * spreadX, artworkScale * spreadY};
    result.socketPosition = {
        baseCenter.x + (socket.x - result.artworkCenter.x) * result.artworkScale.x,
        baseCenter.y + (socket.y - result.artworkCenter.y) * result.artworkScale.y};
    return true;
}

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
    if (owner == 1) return sf::Color(26, 126, 101, 146);
    if (owner == 2) return sf::Color(142, 54, 42, 146);
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
    sf::Vector2f center,
    float scale,
    int owner,
    bool exhausted,
    float footprintWidth,
    float footprintHeight,
    const sf::Texture* artwork,
    const sf::Texture* gemArtwork)
{
    const float spreadX = std::max(1.0f, footprintWidth);
    const float spreadY = std::max(1.0f, footprintHeight);
    const float radiusX = 25.0f * scale * spreadX;
    const float radiusY = 9.0f * scale * spreadY;
    const float dim = exhausted ? 0.55f : 1.0f;

    const sf::Vector2f baseCenter = center;

    // Contact shadow, offset a touch down-right to agree with the lantern-lit
    // backdrop, so the piece sits in the scene instead of floating over it.
    drawSoftEllipse(
        target,
        {baseCenter.x + 2.0f * scale, baseCenter.y + 2.0f * scale},
        radiusX * 0.92f,
        radiusY * 0.9f,
        sf::Color(0, 0, 0, 190),
        6);

    PieceBaseArtworkLayout artworkLayout;
    if (resolvePieceBaseArtworkLayout(
            artwork, baseCenter, scale, spreadX, spreadY, artworkLayout))
    {
        sf::Sprite base(*artwork);
        base.setOrigin(artworkLayout.artworkCenter);
        base.setPosition(baseCenter);
        base.setScale(artworkLayout.artworkScale);
        sf::Color artworkColor = shadeColor(sf::Color::White, dim);
        artworkColor.a = exhausted ? 210 : 255;
        base.setColor(artworkColor);
        target.draw(base);

        if (gemArtwork)
        {
            const sf::Vector2u gemSize = gemArtwork->getSize();
            if (gemSize.x > 0 && gemSize.y > 0)
            {
                // Cover the gem baked into the team artwork before drawing the
                // card-rarity gem. The mask stays inside the gold socket frame.
                const sf::Vector2f maskSize = artworkLayout.usesLargeBase
                    ? PieceBaseLargeSocketMaskSize
                    : PieceBaseSocketMaskSize;
                const float halfMaskWidth =
                    maskSize.x * artworkLayout.artworkScale.x * 0.5f;
                const float halfMaskHeight =
                    maskSize.y * artworkLayout.artworkScale.y * 0.5f;
                sf::ConvexShape socketMask(4);
                socketMask.setPoint(0, {0.0f, -halfMaskHeight});
                socketMask.setPoint(1, {halfMaskWidth, 0.0f});
                socketMask.setPoint(2, {0.0f, halfMaskHeight});
                socketMask.setPoint(3, {-halfMaskWidth, 0.0f});
                socketMask.setPosition(artworkLayout.socketPosition);
                socketMask.setFillColor(withAlpha(
                    shadeColor(sf::Color(13, 16, 24), dim),
                    exhausted ? 225 : 255));
                target.draw(socketMask);

                sf::Sprite gem(*gemArtwork);
                gem.setOrigin({
                    static_cast<float>(gemSize.x) * 0.5f,
                    static_cast<float>(gemSize.y) * 0.5f});
                gem.setPosition(artworkLayout.socketPosition);
                const float gemScale = artworkLayout.usesLargeBase
                    ? PieceBaseLargeGemScale
                    : PieceBaseGemScale;
                gem.setScale({
                    artworkLayout.artworkScale.x * gemScale,
                    artworkLayout.artworkScale.y * gemScale});
                gem.setColor(artworkColor);
                target.draw(gem);
            }
        }
        return;
    }

    sf::CircleShape plinth = makeEllipse(baseCenter, radiusX, radiusY);
    plinth.setFillColor(withAlpha(shadeColor(BoardPlate, dim), 232));
    target.draw(plinth);

    // Lit upper lip and a brass rim: two hairlines are enough to read as a
    // machined disc rather than a filled circle.
    drawEllipseOutline(
        target,
        {baseCenter.x, baseCenter.y - 0.8f * scale},
        radiusX * 0.93f,
        radiusY * 0.86f,
        1.0f,
        withAlpha(shadeColor(BoardStoneLight, dim), 150));
    drawEllipseOutline(
        target, baseCenter, radiusX, radiusY, 1.4f, withAlpha(shadeColor(BoardBrass, dim), 208));
}

void drawPieceHealthBadge(
    sf::RenderWindow& window,
    sf::Vector2f center,
    float scale,
    int health,
    int owner,
    bool dimmed,
    const sf::Font& font,
    bool crowned)
{
    const float radius = std::clamp(9.0f * scale, 8.0f, 11.0f);
    const float dim = dimmed ? 0.65f : 1.0f;
    const sf::Color bright = owner == 1
        ? sf::Color(55, 145, 255)
        : owner == 2 ? sf::Color(240, 62, 68) : sf::Color(172, 172, 172);
    const sf::Color deep = owner == 1
        ? sf::Color(5, 25, 78)
        : owner == 2 ? sf::Color(82, 7, 12) : sf::Color(28, 28, 28);
    const sf::Color highlight = owner == 1
        ? sf::Color(184, 226, 255)
        : owner == 2 ? sf::Color(255, 190, 190) : sf::Color(238, 238, 238);

    sf::CircleShape shadow(radius + 1.2f, 28);
    shadow.setOrigin({radius + 1.2f, radius + 1.2f});
    shadow.setPosition({center.x + 1.2f * scale, center.y + 1.5f * scale});
    shadow.setFillColor(sf::Color(0, 0, 0, dimmed ? 150 : 205));
    window.draw(shadow);

    sf::CircleShape badge(radius, 28);
    badge.setOrigin({radius, radius});
    badge.setPosition(center);
    badge.setFillColor(withAlpha(shadeColor(bright, dim), dimmed ? 225 : 255));
    badge.setOutlineThickness(std::max(1.1f, 1.35f * scale));
    badge.setOutlineColor(withAlpha(shadeColor(highlight, dim), dimmed ? 210 : 250));
    window.draw(badge);

    sf::CircleShape inner(radius * 0.76f, 28);
    inner.setOrigin({radius * 0.76f, radius * 0.76f});
    inner.setPosition(center);
    inner.setFillColor(sf::Color::Transparent);
    inner.setOutlineThickness(std::max(0.7f, 0.8f * scale));
    inner.setOutlineColor(withAlpha(shadeColor(deep, dim), dimmed ? 130 : 175));
    window.draw(inner);

    const unsigned int characterSize = static_cast<unsigned int>(std::lround(
        std::clamp(13.0f * scale, 12.0f, 17.0f)));
    sf::Text healthText(font, std::to_string(std::max(0, health)), characterSize);
    healthText.setStyle(sf::Text::Bold);
    healthText.setLetterSpacing(0.86f);
    healthText.setFillColor(withAlpha(
        sf::Color(255, 250, 224), dimmed ? 230 : 255));
    healthText.setOutlineColor(withAlpha(
        deep, dimmed ? 220 : 255));
    healthText.setOutlineThickness(std::max(0.85f, 0.95f * scale));

    const float maxTextWidth = radius * 1.45f;
    const float textWidth = healthText.getLocalBounds().size.x;
    if (textWidth > maxTextWidth && textWidth > 0.0f)
    {
        healthText.setScale({maxTextWidth / textWidth, 1.0f});
    }
    centerText(healthText, {center.x, center.y - 0.35f * scale});
    drawCrispText(window, healthText);

    if (crowned)
    {
        const float span = std::clamp(13.0f * scale, 11.0f, 17.0f);
        const float overlap = std::clamp(2.0f * scale, 1.5f, 2.5f);
        const float crownY = center.y - radius - span * 0.32f + overlap;
        sf::ConvexShape crown(7);
        crown.setPoint(0, {center.x - span * 0.5f, crownY + span * 0.32f});
        crown.setPoint(1, {center.x - span * 0.5f, crownY - span * 0.18f});
        crown.setPoint(2, {center.x - span * 0.25f, crownY + span * 0.04f});
        crown.setPoint(3, {center.x, crownY - span * 0.32f});
        crown.setPoint(4, {center.x + span * 0.25f, crownY + span * 0.04f});
        crown.setPoint(5, {center.x + span * 0.5f, crownY - span * 0.18f});
        crown.setPoint(6, {center.x + span * 0.5f, crownY + span * 0.32f});
        crown.setFillColor(withAlpha(BoardBrassBright, dimmed ? 155 : 245));
        crown.setOutlineThickness(std::max(0.8f, 1.0f * scale));
        crown.setOutlineColor(sf::Color(32, 20, 10, 230));
        window.draw(crown);
    }
}

void drawPieceSelectionRing(
    sf::RenderTarget& target,
    sf::Vector2f center,
    float scale,
    float pulse,
    sf::Color accent,
    float footprintWidth,
    float footprintHeight)
{
    const float radiusX = 29.0f * scale * std::max(1.0f, footprintWidth);
    const float radiusY = 10.5f * scale * std::max(1.0f, footprintHeight);
    const float swell = 1.0f + pulse * 0.09f;
    const sf::Vector2f baseCenter = center;

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

namespace
{
std::array<sf::Vector2f, 4> boardFootprintCorners(
    int row, int column, int width, int height, int viewer)
{
    const int firstRow = std::clamp(row, 0, game_data::BoardSize - 1);
    const int lastRow = std::clamp(
        row + std::max(1, height) - 1, 0, game_data::BoardSize - 1);
    const int firstColumn = std::clamp(column, 0, game_data::BoardSize - 1);
    const int lastColumn = std::clamp(
        column + std::max(1, width) - 1, 0, game_data::BoardSize - 1);

    const int firstScreenRow = screenRowForViewer(firstRow, viewer);
    const int lastScreenRow = screenRowForViewer(lastRow, viewer);
    const int topScreenEdge = std::min(firstScreenRow, lastScreenRow);
    const int bottomScreenEdge = std::max(firstScreenRow, lastScreenRow) + 1;
    const int leftColumnEdge = std::min(firstColumn, lastColumn);
    const int rightColumnEdge = std::max(firstColumn, lastColumn) + 1;

    return {
        boardEdgePoint(topScreenEdge, leftColumnEdge),
        boardEdgePoint(topScreenEdge, rightColumnEdge),
        boardEdgePoint(bottomScreenEdge, rightColumnEdge),
        boardEdgePoint(bottomScreenEdge, leftColumnEdge)};
}

sf::Vector2f normalizedDirection(sf::Vector2f value)
{
    const float length = std::sqrt(value.x * value.x + value.y * value.y);
    return length > 0.001f ? value / length : sf::Vector2f{};
}
} // namespace

sf::Vector2f boardFootprintCenter(
    int row, int column, int width, int height, int viewer)
{
    const std::array<sf::Vector2f, 4> corners =
        boardFootprintCorners(row, column, width, height, viewer);
    return {
        (corners[0].x + corners[1].x + corners[2].x + corners[3].x) * 0.25f,
        (corners[0].y + corners[1].y + corners[2].y + corners[3].y) * 0.25f};
}

sf::Vector2f boardFootprintHealthBadgeCenter(
    int row, int column, int width, int height, int viewer, int owner)
{
    const std::array<sf::Vector2f, 4> corners =
        boardFootprintCorners(row, column, width, height, viewer);
    const int firstRow = std::clamp(row, 0, game_data::BoardSize - 1);
    const int lastRow = std::clamp(
        row + std::max(1, height) - 1, 0, game_data::BoardSize - 1);
    const int nearScreenRow = std::max(
        screenRowForViewer(firstRow, viewer),
        screenRowForViewer(lastRow, viewer));
    const float radius = std::clamp(
        9.0f * pieceScaleForScreenRow(nearScreenRow), 8.0f, 11.0f);
    const float inset = radius + 2.5f;
    if (owner == 1)
    {
        const sf::Vector2f towardNearRight = normalizedDirection(corners[2] - corners[3]);
        const sf::Vector2f towardFarLeft = normalizedDirection(corners[0] - corners[3]);
        return corners[3] + (towardNearRight + towardFarLeft) * inset;
    }

    const sf::Vector2f towardNearLeft = normalizedDirection(corners[3] - corners[2]);
    const sf::Vector2f towardFarRight = normalizedDirection(corners[1] - corners[2]);
    return corners[2] + (towardNearLeft + towardFarRight) * inset;
}

sf::Vector2f boardFootprintAnchor(
    int row, int column, int width, int height, int viewer)
{
    sf::Vector2f total{0.0f, 0.0f};
    int cellCount = 0;
    for (int footprintRow = row;
         footprintRow < row + std::max(1, height);
         ++footprintRow)
    {
        for (int footprintColumn = column;
             footprintColumn < column + std::max(1, width);
             ++footprintColumn)
        {
            if (footprintRow < 0 || footprintColumn < 0 ||
                footprintRow >= game_data::BoardSize ||
                footprintColumn >= game_data::BoardSize)
            {
                continue;
            }
            total += boardCellAnchor(
                boardCellMetricsForViewer(footprintRow, footprintColumn, viewer));
            ++cellCount;
        }
    }

    if (cellCount > 0)
    {
        return total / static_cast<float>(cellCount);
    }

    // Drag previews may briefly live entirely outside the board. Clamp before
    // projecting so a negative row never reaches the perspective power curve.
    return boardCellAnchor(boardCellMetricsForViewer(
        std::clamp(row, 0, game_data::BoardSize - 1),
        std::clamp(column, 0, game_data::BoardSize - 1),
        viewer));
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
