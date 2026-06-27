#pragma once

#include <SFML/Graphics.hpp>

#include "../shared/game_data.hpp"

#include <array>

namespace bayou::client
{

// In-game board layout.
constexpr float BoardOriginX = 24.0f;
constexpr float BoardOriginY = 70.0f;
constexpr float CellSize = 94.0f;
constexpr float BoardBottomWidth = CellSize * static_cast<float>(game_data::BoardSize);
constexpr float BoardTopWidth = 544.0f;
constexpr float BoardHeight = 418.0f;
constexpr float BoardCenterX = BoardOriginX + BoardBottomWidth * 0.5f;
constexpr float BoardPerspectiveExponent = 1.18f;
constexpr float BoardThickness = 14.0f;
constexpr float PieceFarScale = 0.72f;
constexpr float PieceNearScale = 1.22f;
constexpr float PieceBaseWidth = 96.0f;
constexpr float PieceBaseHeight = 100.0f;
constexpr float PieceWalkBaseHeight = 108.0f;
constexpr float WalkAnimationLoopSeconds = 1.0f;
constexpr float AttackAnimationDurationSeconds = 0.42f;
constexpr float AttackLungePixels = 18.0f;
constexpr float AttackShakePixels = 4.0f;
constexpr float Pi = 3.14159265358979323846f;

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
int screenRowForViewer(int row, int viewer);
int rowForScreenRow(int screenRow, int viewer);
sf::Vector2f boardEdgePoint(int screenEdge, int columnEdge);
float pieceScaleForScreenRow(int screenRow);
BoardCellMetrics boardCellMetricsForViewer(int row, int column, int viewer);
sf::Vector2f boardCellAnchor(const BoardCellMetrics& metrics);
bool pointInConvex(sf::Vector2f point, const std::array<sf::Vector2f, 4>& corners);
std::array<sf::Vector2f, 4> offsetQuad(std::array<sf::Vector2f, 4> corners, sf::Vector2f offset);
sf::FloatRect pieceTargetRect(sf::Vector2f anchor, float scale, bool walkSheet);

} // namespace bayou::client
