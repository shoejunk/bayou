#pragma once

#include <SFML/Graphics.hpp>

#include <optional>
#include <string>
#include <vector>

namespace bayou::client
{
void centerText(sf::Text& text, float x);
void centerText(sf::Text& text, sf::Vector2f center);
void centerButtonText(sf::Text& text, sf::Vector2f center);
void setMessage(sf::Text& text, const std::string& message, const sf::Color& color);
void setMessageY(sf::Text& text, float y);

std::string elideToWidth(sf::Font& font, const std::string& value, unsigned int size, float maxWidth);
void drawText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    float maxWidth = 0.0f);
void drawBeveledPlate(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    sf::Color fill,
    sf::Color outline,
    bool highlighted = false,
    float cut = 8.0f);
void drawTitlePlaque(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    sf::Vector2f center,
    sf::Vector2f size);
void drawSeparatorRule(sf::RenderWindow& window, sf::Vector2f position, float width);
std::vector<std::string> wrapText(sf::Font& font, const std::string& value, unsigned int size, float maxWidth);
float drawWrappedText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    float maxWidth,
    float lineGap = 4.0f);
void drawPanel(sf::RenderWindow& window, sf::Vector2f position, sf::Vector2f size);
void drawRow(
    sf::RenderWindow& window,
    sf::Font& font,
    sf::Vector2f position,
    sf::Vector2f size,
    const std::string& primary,
    const std::string& secondary,
    bool selected);

std::optional<std::size_t> rowIndexAt(
    sf::Vector2f mouse,
    float x,
    float y,
    float width,
    float rowHeight,
    std::size_t visibleRows,
    std::size_t offset,
    std::size_t totalRows);
bool isInsideRect(sf::Vector2f mouse, float x, float y, float width, float height);
void scrollList(std::size_t& offset, std::size_t totalRows, std::size_t visibleRows, float delta);
void clampListOffset(std::size_t& offset, std::size_t totalRows, std::size_t visibleRows);
}
