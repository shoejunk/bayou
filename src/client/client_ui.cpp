#include "client_ui.hpp"

#include <algorithm>
#include <array>

namespace bayou::client
{
namespace
{
const sf::Color Brass(174, 117, 54);
const sf::Color BrassBright(239, 190, 98);
const sf::Color BrassDim(83, 54, 29);
const sf::Color Ink(246, 232, 200);

sf::ConvexShape makeCutRect(sf::Vector2f position, sf::Vector2f size, float cut)
{
    cut = std::max(0.0f, std::min(cut, std::min(size.x, size.y) * 0.45f));
    sf::ConvexShape shape(8);
    shape.setPoint(0, {position.x + cut, position.y});
    shape.setPoint(1, {position.x + size.x - cut, position.y});
    shape.setPoint(2, {position.x + size.x, position.y + cut});
    shape.setPoint(3, {position.x + size.x, position.y + size.y - cut});
    shape.setPoint(4, {position.x + size.x - cut, position.y + size.y});
    shape.setPoint(5, {position.x + cut, position.y + size.y});
    shape.setPoint(6, {position.x, position.y + size.y - cut});
    shape.setPoint(7, {position.x, position.y + cut});
    return shape;
}

void drawLine(sf::RenderWindow& window, sf::Vector2f position, sf::Vector2f size, sf::Color color)
{
    if (size.x <= 0.0f || size.y <= 0.0f)
    {
        return;
    }

    sf::RectangleShape line(size);
    line.setPosition(position);
    line.setFillColor(color);
    window.draw(line);
}

void drawRivet(sf::RenderWindow& window, sf::Vector2f center, float radius, sf::Color color)
{
    sf::CircleShape shadow(radius);
    shadow.setOrigin({radius, radius});
    shadow.setPosition(center + sf::Vector2f(1.0f, 1.0f));
    shadow.setFillColor(sf::Color(0, 0, 0, 105));
    window.draw(shadow);

    sf::CircleShape rivet(radius);
    rivet.setOrigin({radius, radius});
    rivet.setPosition(center);
    rivet.setFillColor(color);
    rivet.setOutlineThickness(std::max(1.0f, radius * 0.28f));
    rivet.setOutlineColor(sf::Color(64, 38, 20, 210));
    window.draw(rivet);

    sf::CircleShape shine(radius * 0.38f);
    shine.setOrigin({radius * 0.38f, radius * 0.38f});
    shine.setPosition(center + sf::Vector2f(-radius * 0.22f, -radius * 0.22f));
    shine.setFillColor(sf::Color(255, 231, 169, 130));
    window.draw(shine);
}

void drawCornerBrackets(sf::RenderWindow& window, sf::Vector2f position, sf::Vector2f size, float inset, sf::Color color)
{
    const float bracket = std::min(24.0f, std::min(size.x, size.y) * 0.28f);
    const float thickness = 1.5f;
    const float left = position.x + inset;
    const float top = position.y + inset;
    const float right = position.x + size.x - inset;
    const float bottom = position.y + size.y - inset;

    drawLine(window, {left, top}, {bracket, thickness}, color);
    drawLine(window, {left, top}, {thickness, bracket}, color);
    drawLine(window, {right - bracket, top}, {bracket, thickness}, color);
    drawLine(window, {right - thickness, top}, {thickness, bracket}, color);
    drawLine(window, {left, bottom - thickness}, {bracket, thickness}, color);
    drawLine(window, {left, bottom - bracket}, {thickness, bracket}, color);
    drawLine(window, {right - bracket, bottom - thickness}, {bracket, thickness}, color);
    drawLine(window, {right - thickness, bottom - bracket}, {thickness, bracket}, color);
}
}

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

void drawBeveledPlate(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Vector2f size,
    sf::Color fill,
    sf::Color outline,
    bool highlighted,
    float cut)
{
    sf::ConvexShape shadow = makeCutRect(position + sf::Vector2f(4.0f, 5.0f), size, cut);
    shadow.setFillColor(sf::Color(0, 0, 0, highlighted ? 145 : 100));
    window.draw(shadow);

    sf::ConvexShape plate = makeCutRect(position, size, cut);
    plate.setFillColor(fill);
    plate.setOutlineThickness(2.0f);
    plate.setOutlineColor(outline);
    window.draw(plate);

    const float innerCut = std::max(0.0f, cut - 3.0f);
    sf::ConvexShape inner = makeCutRect(position + sf::Vector2f(5.0f, 5.0f), size - sf::Vector2f(10.0f, 10.0f), innerCut);
    inner.setFillColor(sf::Color::Transparent);
    inner.setOutlineThickness(1.0f);
    inner.setOutlineColor(highlighted ? sf::Color(255, 205, 114, 185) : sf::Color(117, 78, 39, 170));
    window.draw(inner);

    drawLine(
        window,
        {position.x + cut + 8.0f, position.y + 7.0f},
        {std::max(0.0f, size.x - (cut + 8.0f) * 2.0f), 1.0f},
        sf::Color(255, 224, 154, highlighted ? 118 : 58));
    drawLine(
        window,
        {position.x + cut + 8.0f, position.y + size.y - 8.0f},
        {std::max(0.0f, size.x - (cut + 8.0f) * 2.0f), 1.0f},
        sf::Color(48, 30, 17, 160));
    drawCornerBrackets(
        window,
        position,
        size,
        8.0f,
        highlighted ? sf::Color(250, 190, 91, 155) : sf::Color(109, 72, 35, 145));

    if (size.x >= 54.0f && size.y >= 26.0f)
    {
        const float radius = std::clamp(size.y * 0.065f, 1.7f, 3.0f);
        drawRivet(window, {position.x + 13.0f, position.y + size.y * 0.5f}, radius, sf::Color(199, 139, 61, 210));
        drawRivet(window, {position.x + size.x - 13.0f, position.y + size.y * 0.5f}, radius, sf::Color(199, 139, 61, 210));
    }
}

void drawTitlePlaque(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    sf::Vector2f center,
    sf::Vector2f size)
{
    if (value.empty())
    {
        return;
    }

    sf::Text measuring(font, value, static_cast<unsigned int>(std::clamp(size.y * 0.55f, 24.0f, 48.0f)));
    const float measuredWidth = measuring.getLocalBounds().size.x + 132.0f;
    size.x = std::max(size.x, measuredWidth);
    const sf::Vector2f position{center.x - size.x * 0.5f, center.y - size.y * 0.5f};

    const float pipeY = center.y - 4.0f;
    sf::RectangleShape leftPipe({56.0f, 10.0f});
    leftPipe.setOrigin({56.0f, 5.0f});
    leftPipe.setPosition({position.x + 8.0f, pipeY});
    leftPipe.setFillColor(sf::Color(74, 44, 22, 230));
    leftPipe.setOutlineThickness(1.0f);
    leftPipe.setOutlineColor(BrassDim);
    window.draw(leftPipe);

    sf::RectangleShape rightPipe(leftPipe);
    rightPipe.setOrigin({0.0f, 5.0f});
    rightPipe.setPosition({position.x + size.x - 8.0f, pipeY});
    window.draw(rightPipe);

    drawRivet(window, {position.x - 50.0f, pipeY}, 10.0f, sf::Color(148, 91, 38, 220));
    drawRivet(window, {position.x + size.x + 50.0f, pipeY}, 10.0f, sf::Color(148, 91, 38, 220));

    drawBeveledPlate(
        window,
        position,
        size,
        sf::Color(23, 21, 18, 246),
        BrassBright,
        true,
        17.0f);

    sf::Text shadow(font, value, measuring.getCharacterSize());
    shadow.setFillColor(sf::Color(0, 0, 0, 190));
    centerButtonText(shadow, center + sf::Vector2f(2.0f, 3.0f));
    window.draw(shadow);

    sf::Text text(font, value, measuring.getCharacterSize());
    text.setFillColor(Ink);
    text.setOutlineThickness(1.0f);
    text.setOutlineColor(sf::Color(70, 43, 25, 210));
    centerButtonText(text, center);
    window.draw(text);
}

void drawSeparatorRule(sf::RenderWindow& window, sf::Vector2f position, float width)
{
    if (width <= 0.0f)
    {
        return;
    }

    const float centerX = position.x + width * 0.5f;
    drawLine(window, position, {width, 1.0f}, sf::Color(151, 99, 47, 155));
    drawLine(window, {position.x + 6.0f, position.y + 2.0f}, {width - 12.0f, 1.0f}, sf::Color(41, 27, 16, 125));
    drawRivet(window, {centerX, position.y + 1.0f}, 5.0f, sf::Color(170, 105, 42, 210));
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
    drawBeveledPlate(window, position, size, sf::Color(9, 15, 16, 238), Brass, false, 14.0f);

    sf::RectangleShape wash({std::max(0.0f, size.x - 18.0f), std::max(0.0f, size.y - 18.0f)});
    wash.setPosition(position + sf::Vector2f(9.0f, 9.0f));
    wash.setFillColor(sf::Color(0, 0, 0, 54));
    window.draw(wash);

    drawLine(
        window,
        {position.x + 18.0f, position.y + size.y - 15.0f},
        {std::max(0.0f, size.x - 36.0f), 1.0f},
        sf::Color(224, 159, 74, 80));
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
    drawBeveledPlate(
        window,
        position,
        size,
        selected ? sf::Color(76, 49, 25, 238) : sf::Color(19, 29, 30, 226),
        selected ? BrassBright : sf::Color(103, 72, 39),
        selected,
        5.0f);

    drawText(window, font, primary, 16, {position.x + 10.0f, position.y + 5.0f}, sf::Color(246, 238, 218), size.x - 20.0f);
    if (!secondary.empty())
    {
        drawText(window, font, secondary, 12, {position.x + 10.0f, position.y + 22.0f}, sf::Color(203, 173, 125), size.x - 20.0f);
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
