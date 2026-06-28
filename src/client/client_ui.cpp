#include "client_ui.hpp"

#include <algorithm>
#include <array>

namespace bayou::client
{
void centerText(sf::Text& text, float x)
{
    sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin({bounds.position.x + bounds.size.x / 2.0f, text.getOrigin().y});
    text.setPosition({x, text.getPosition().y});
}

void centerText(sf::Text& text, sf::Vector2f center)
{
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin({
        bounds.position.x + bounds.size.x / 2.0f,
        bounds.position.y + bounds.size.y / 2.0f});
    text.setPosition(center);
}

void centerButtonText(sf::Text& text, sf::Vector2f center)
{
    const float opticalOffset = std::clamp(static_cast<float>(text.getCharacterSize()) * 0.15f, 3.0f, 8.0f);
    centerText(text, {center.x, center.y + opticalOffset});
}

void setMessage(sf::Text& text, const std::string& message, const sf::Color& color)
{
    text.setString(message);
    text.setFillColor(color);
    centerText(text, 400.0f);
}

void setMessageY(sf::Text& text, float y)
{
    text.setPosition({text.getPosition().x, y});
    centerText(text, 400.0f);
}

std::string elideToWidth(sf::Font& font, const std::string& value, unsigned int size, float maxWidth)
{
    sf::Text text(font, value, size);
    if (text.getLocalBounds().size.x <= maxWidth)
    {
        return value;
    }

    std::string display = value;
    while (!display.empty())
    {
        display.pop_back();
        text.setString(display + "...");
        if (text.getLocalBounds().size.x <= maxWidth)
        {
            return display + "...";
        }
    }

    return "...";
}

void drawText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    float maxWidth)
{
    sf::Text text(font, maxWidth > 0.0f ? elideToWidth(font, value, size, maxWidth) : value, size);
    text.setFillColor(color);
    text.setPosition(position);
    window.draw(text);
}

std::vector<std::string> wrapText(sf::Font& font, const std::string& value, unsigned int size, float maxWidth)
{
    std::vector<std::string> lines;
    sf::Text measuringText(font, "", size);
    std::string line;
    std::size_t position = 0;

    auto fits = [&](const std::string& text) {
        measuringText.setString(text);
        return measuringText.getLocalBounds().size.x <= maxWidth;
    };

    auto pushLine = [&]() {
        lines.push_back(line);
        line.clear();
    };

    while (position < value.size())
    {
        if (value[position] == '\n')
        {
            pushLine();
            ++position;
            continue;
        }

        while (position < value.size() && value[position] == ' ')
        {
            ++position;
        }
        if (position >= value.size())
        {
            break;
        }

        const std::size_t wordStart = position;
        while (position < value.size() && value[position] != ' ' && value[position] != '\n')
        {
            ++position;
        }

        const std::string word = value.substr(wordStart, position - wordStart);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (line.empty() || fits(candidate))
        {
            line = candidate;
        }
        else
        {
            pushLine();
            line = word;
        }
    }

    if (!line.empty() || lines.empty())
    {
        lines.push_back(line);
    }
    return lines;
}

float drawWrappedText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    float maxWidth,
    float lineGap)
{
    float y = position.y;
    for (const std::string& line : wrapText(font, value, size, maxWidth))
    {
        drawText(window, font, line, size, {position.x, y}, color);
        y += static_cast<float>(size) + lineGap;
    }
    return y;
}

void drawPanel(sf::RenderWindow& window, sf::Vector2f position, sf::Vector2f size)
{
    sf::RectangleShape shadow(size);
    shadow.setPosition(position + sf::Vector2f(5.0f, 6.0f));
    shadow.setFillColor(sf::Color(0, 0, 0, 110));
    window.draw(shadow);

    sf::RectangleShape panel(size);
    panel.setPosition(position);
    panel.setFillColor(sf::Color(10, 21, 23, 238));
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(sf::Color(158, 111, 56));
    window.draw(panel);

    sf::RectangleShape inner({size.x - 8.0f, size.y - 8.0f});
    inner.setPosition({position.x + 4.0f, position.y + 4.0f});
    inner.setFillColor(sf::Color::Transparent);
    inner.setOutlineThickness(1.0f);
    inner.setOutlineColor(sf::Color(50, 126, 116, 165));
    window.draw(inner);

    sf::RectangleShape topRule({size.x - 12.0f, 2.0f});
    topRule.setPosition({position.x + 6.0f, position.y + 6.0f});
    topRule.setFillColor(sf::Color(213, 157, 76, 85));
    window.draw(topRule);

    for (const sf::Vector2f offset : std::array<sf::Vector2f, 4>{
             sf::Vector2f{8.0f, 8.0f},
             sf::Vector2f{size.x - 8.0f, 8.0f},
             sf::Vector2f{8.0f, size.y - 8.0f},
             sf::Vector2f{size.x - 8.0f, size.y - 8.0f}})
    {
        sf::CircleShape rivet(2.0f);
        rivet.setOrigin({2.0f, 2.0f});
        rivet.setPosition(position + offset);
        rivet.setFillColor(sf::Color(186, 131, 61, 180));
        window.draw(rivet);
    }
}

void drawRow(
    sf::RenderWindow& window,
    sf::Font& font,
    sf::Vector2f position,
    sf::Vector2f size,
    const std::string& primary,
    const std::string& secondary,
    bool selected)
{
    sf::RectangleShape row(size);
    row.setPosition(position);
    row.setFillColor(selected ? sf::Color(42, 112, 103, 230) : sf::Color(28, 39, 42, 224));
    row.setOutlineThickness(1.0f);
    row.setOutlineColor(selected ? sf::Color(111, 226, 200) : sf::Color(102, 76, 46));
    window.draw(row);

    drawText(window, font, primary, 16, {position.x + 8.0f, position.y + 5.0f}, sf::Color(246, 238, 218), size.x - 16.0f);
    if (!secondary.empty())
    {
        drawText(window, font, secondary, 12, {position.x + 8.0f, position.y + 22.0f}, sf::Color(198, 180, 142), size.x - 16.0f);
    }
}

std::optional<std::size_t> rowIndexAt(
    sf::Vector2f mouse,
    float x,
    float y,
    float width,
    float rowHeight,
    std::size_t visibleRows,
    std::size_t offset,
    std::size_t totalRows)
{
    if (mouse.x < x || mouse.x > x + width || mouse.y < y)
    {
        return std::nullopt;
    }

    const std::size_t visibleIndex = static_cast<std::size_t>((mouse.y - y) / rowHeight);
    const std::size_t index = offset + visibleIndex;
    if (visibleIndex < visibleRows && index < totalRows)
    {
        return index;
    }

    return std::nullopt;
}

bool isInsideRect(sf::Vector2f mouse, float x, float y, float width, float height)
{
    return mouse.x >= x && mouse.x <= x + width && mouse.y >= y && mouse.y <= y + height;
}

void scrollList(std::size_t& offset, std::size_t totalRows, std::size_t visibleRows, float delta)
{
    if (delta < 0.0f)
    {
        if (offset + visibleRows < totalRows)
        {
            ++offset;
        }
    }
    else if (offset > 0)
    {
        --offset;
    }
}

void clampListOffset(std::size_t& offset, std::size_t totalRows, std::size_t visibleRows)
{
    if (totalRows <= visibleRows)
    {
        offset = 0;
        return;
    }

    offset = std::min(offset, totalRows - visibleRows);
}
}
