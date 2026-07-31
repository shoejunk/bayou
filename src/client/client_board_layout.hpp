#pragma once

#include <SFML/Graphics.hpp>

#include "../shared/game_data.hpp"

#include <array>

namespace bayou::client
{

// In-game board layout.
constexpr float BoardOriginX = 24.0f;
// The board gives up a little height at both ends so the readout band above and
// the command bar below have room to be laid out instead of crammed against the
// screen edges.
constexpr float BoardOriginY = 66.0f;
constexpr float CellSize = 94.0f;
constexpr float BoardBottomWidth = CellSize * static_cast<float>(game_data::BoardSize);
// Keep the far edge broad enough that the back rank does not collapse into a
// thin strip at 800x600, while retaining enough taper for the board to read as
// a physical play surface instead of a flat grid.
constexpr float BoardTopWidth = 548.0f;
constexpr float BoardHeight = 388.0f;
constexpr float BoardCenterX = BoardOriginX + BoardBottomWidth * 0.5f;
constexpr float BoardPerspectiveExponent = 1.16f;
constexpr float BoardThickness = 20.0f;
// Keep the far rank readable at the smallest supported capture while limiting
// the near-rank swell that makes the board feel crowded around the HUD.
constexpr float PieceFarScale = 0.84f;
constexpr float PieceNearScale = 1.14f;
constexpr float PieceBaseWidth = 96.0f;
constexpr float PieceBaseHeight = 100.0f;
// Small shared stand adjustment: close the artwork's transparent foot gap while
// lifting the plinth enough that the piece reads as physically planted on it.
constexpr float PieceStandOffset = 3.0f;
// The plinth sits higher than the artwork anchor so its footprint stays inside
// the occupied square instead of spilling into the row below.
constexpr float PieceBaseLift = 12.0f;
constexpr float PieceBasePipOffset = 7.2f;
constexpr float PieceWalkBaseHeight = 108.0f;
constexpr float WalkAnimationLoopSeconds = 1.0f;
constexpr float AttackAnimationDurationSeconds = 0.42f;
constexpr float PieceReactionAnimationDurationSeconds = 0.55f;
constexpr float DematerializeBlinkSeconds = 3.0f;
constexpr float DematerializeBlinkPeriodSeconds = 0.5f;
constexpr float AttackLungePixels = 18.0f;
constexpr float AttackShakePixels = 4.0f;
constexpr float Pi = 3.14159265358979323846f;

// The match screen's slice of the shared brass-and-swamp palette. Prefixed to
// avoid colliding with the identically named constants inside client_ui.cpp.
inline constexpr sf::Color BoardBrass{174, 117, 54};
inline constexpr sf::Color BoardBrassBright{239, 190, 98};
inline constexpr sf::Color BoardBrassDim{96, 63, 33};
inline constexpr sf::Color BoardParchment{246, 232, 200};
inline constexpr sf::Color BoardParchmentMuted{208, 197, 174};
inline constexpr sf::Color BoardPlate{12, 17, 18};
inline constexpr sf::Color BoardArcane{123, 79, 168};

// Stone tones for the playing surface, before depth shading and control tint.
inline constexpr sf::Color BoardStoneLight{82, 94, 87};
inline constexpr sf::Color BoardStoneDark{61, 71, 67};
inline constexpr sf::Color BoardGrout{13, 18, 18};

struct BoardCellMetrics
{
    std::array<sf::Vector2f, 4> corners{};
    sf::Vector2f center{};
    float height = 0.0f;
    float depthScale = 1.0f;
    int screenRow = 0;
};

sf::Color ownerColor(int owner);
sf::Color ownerTint(int owner);
// Deep and bright variants of an owner's colour, for plate fills and edge light.
sf::Color ownerColorDeep(int owner);
sf::Color ownerColorBright(int owner);

// Multiplies a colour's channels, preserving alpha. Used for depth shading.
sf::Color shadeColor(sf::Color color, float factor);
sf::Color withAlpha(sf::Color color, int alpha);

// Fills a quad with a vertical gradient. corners[0]/[1] are the far edge and
// carry farColor; corners[2]/[3] are the near edge and carry nearColor. Flat
// ConvexShape fills are the main reason the board reads as a painted rectangle
// rather than a lit surface, so cells go through this instead.
void drawGradientQuad(
    sf::RenderTarget& target,
    const std::array<sf::Vector2f, 4>& corners,
    sf::Color farColor,
    sf::Color nearColor);

// Draws a line between two arbitrary points as a thin quad, so board grout and
// rim highlights can follow the perspective edges.
void drawEdgeLine(
    sf::RenderTarget& target, sf::Vector2f from, sf::Vector2f to, float thickness, sf::Color color);

// A squashed ellipse built from stacked rings, which fakes a blurred edge
// cheaply enough to use for every piece's contact shadow and range marker.
void drawSoftEllipse(
    sf::RenderTarget& target,
    sf::Vector2f center,
    float radiusX,
    float radiusY,
    sf::Color color,
    int layers = 5);

void drawEllipseOutline(
    sf::RenderTarget& target,
    sf::Vector2f center,
    float radiusX,
    float radiusY,
    float thickness,
    sf::Color color);

// Contact shadow plus an owner-tinted plinth under a piece. Pieces are drawn as
// bare cut-outs otherwise and read as stickers laid on the board.
void drawPieceBase(
    sf::RenderTarget& target,
    sf::Vector2f anchor,
    float scale,
    int owner,
    bool exhausted,
    float footprintWidth = 1.0f);

// The bright ring under the piece the player has picked up or selected.
void drawPieceSelectionRing(
    sf::RenderTarget& target, sf::Vector2f anchor, float scale, float pulse, sf::Color accent);
int screenRowForViewer(int row, int viewer);
int rowForScreenRow(int screenRow, int viewer);
sf::Vector2f boardEdgePoint(int screenEdge, int columnEdge);
float pieceScaleForScreenRow(int screenRow);
BoardCellMetrics boardCellMetricsForViewer(int row, int column, int viewer);
sf::Vector2f boardCellAnchor(const BoardCellMetrics& metrics);
sf::Vector2f boardFootprintAnchor(int row, int column, int width, int viewer);
bool pointInConvex(sf::Vector2f point, const std::array<sf::Vector2f, 4>& corners);
std::array<sf::Vector2f, 4> offsetQuad(std::array<sf::Vector2f, 4> corners, sf::Vector2f offset);
sf::FloatRect pieceTargetRect(
    sf::Vector2f anchor, float scale, bool walkSheet, int width = 1, int height = 1);

} // namespace bayou::client
