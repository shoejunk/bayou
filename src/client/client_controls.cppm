module;

#include <SFML/Graphics.hpp>

#include "client_ui.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module client_controls;

export struct TabStrip
{
    sf::Vector2f position;
    sf::Vector2f tabSize;
    sf::Font& font;
    std::vector<std::string> labels;
    std::size_t activeIndex = 0;
    std::optional<std::size_t> hoveredIndex;

    TabStrip(sf::Vector2f nextPosition, sf::Vector2f nextTabSize, std::vector<std::string> nextLabels, sf::Font& nextFont)
        : position(nextPosition)
        , tabSize(nextTabSize)
        , font(nextFont)
        , labels(std::move(nextLabels))
    {
    }

    void setActive(std::size_t index)
    {
        if (index < labels.size())
        {
            activeIndex = index;
        }
    }

    std::size_t active() const
    {
        return activeIndex;
    }

    void update(sf::Vector2f mousePos)
    {
        hoveredIndex = tabIndexAt(mousePos);
    }

    std::optional<std::size_t> clickedIndex(sf::Vector2f mousePos) const
    {
        return tabIndexAt(mousePos);
    }

    void draw(sf::RenderWindow& window) const
    {
        const float totalWidth = static_cast<float>(labels.size()) * tabSize.x;
        sf::RectangleShape baseLine({totalWidth, 3.0f});
        baseLine.setPosition({position.x, position.y + tabSize.y - 2.0f});
        baseLine.setFillColor(sf::Color(177, 124, 59, 210));

        for (std::size_t i = 0; i < labels.size(); ++i)
        {
            drawTab(window, i);
        }

        window.draw(baseLine);
        if (activeIndex < labels.size())
        {
            sf::RectangleShape activeCover({tabSize.x - 10.0f, 4.0f});
            activeCover.setPosition({tabPosition(activeIndex).x + 5.0f, position.y + tabSize.y - 3.0f});
            activeCover.setFillColor(sf::Color(50, 38, 28, 248));
            window.draw(activeCover);
        }
    }

private:
    sf::Vector2f tabPosition(std::size_t index) const
    {
        return {position.x + static_cast<float>(index) * tabSize.x, position.y};
    }

    sf::FloatRect tabBounds(std::size_t index) const
    {
        return {tabPosition(index), tabSize};
    }

    std::optional<std::size_t> tabIndexAt(sf::Vector2f mousePos) const
    {
        for (std::size_t i = 0; i < labels.size(); ++i)
        {
            if (tabBounds(i).contains(mousePos))
            {
                return i;
            }
        }
        return std::nullopt;
    }

    void drawTab(sf::RenderWindow& window, std::size_t index) const
    {
        const sf::Vector2f pos = tabPosition(index);
        const bool active = index == activeIndex;
        const bool hovered = hoveredIndex && *hoveredIndex == index;
        const sf::Color fill = active
            ? sf::Color(50, 38, 28, 248)
            : hovered ? sf::Color(73, 49, 31, 242) : sf::Color(37, 31, 27, 236);
        const sf::Color outline = active || hovered
            ? sf::Color(239, 190, 98)
            : sf::Color(147, 101, 54);

        sf::ConvexShape shadow(6);
        shadow.setPoint(0, {pos.x + 12.0f, pos.y + 7.0f});
        shadow.setPoint(1, {pos.x + tabSize.x - 12.0f, pos.y + 7.0f});
        shadow.setPoint(2, {pos.x + tabSize.x - 1.0f, pos.y + tabSize.y - 1.0f});
        shadow.setPoint(3, {pos.x + tabSize.x - 1.0f, pos.y + tabSize.y + 5.0f});
        shadow.setPoint(4, {pos.x + 1.0f, pos.y + tabSize.y + 5.0f});
        shadow.setPoint(5, {pos.x + 1.0f, pos.y + tabSize.y - 1.0f});
        shadow.setFillColor(sf::Color(0, 0, 0, active ? 118 : 72));
        window.draw(shadow);

        sf::ConvexShape tab(6);
        tab.setPoint(0, {pos.x + 13.0f, pos.y + 4.0f});
        tab.setPoint(1, {pos.x + tabSize.x - 13.0f, pos.y + 4.0f});
        tab.setPoint(2, {pos.x + tabSize.x - 4.0f, pos.y + tabSize.y - 9.0f});
        tab.setPoint(3, {pos.x + tabSize.x - 4.0f, pos.y + tabSize.y});
        tab.setPoint(4, {pos.x + 4.0f, pos.y + tabSize.y});
        tab.setPoint(5, {pos.x + 4.0f, pos.y + tabSize.y - 9.0f});
        tab.setFillColor(fill);
        tab.setOutlineThickness(2.0f);
        tab.setOutlineColor(outline);
        window.draw(tab);

        sf::RectangleShape highlight({tabSize.x - 34.0f, 1.0f});
        highlight.setPosition({pos.x + 17.0f, pos.y + 10.0f});
        highlight.setFillColor(sf::Color(255, 224, 154, active ? 110 : hovered ? 90 : 45));
        window.draw(highlight);

        sf::Text text(font, labels[index], active ? 19 : 18);
        text.setFillColor(active || hovered ? sf::Color(255, 244, 215) : sf::Color(221, 198, 157));
        bayou::client::centerButtonText(text, {pos.x + tabSize.x * 0.5f, pos.y + tabSize.y * 0.52f});
        window.draw(text);
    }
};

export struct SliderControl
{
    sf::Vector2f position;
    sf::Vector2f size;
    sf::Font& font;
    std::string label;
    float value = 1.0f;
    bool hovered = false;
    bool dragging = false;

    SliderControl(sf::Vector2f nextPosition, sf::Vector2f nextSize, const std::string& nextLabel, sf::Font& nextFont)
        : position(nextPosition)
        , size(nextSize)
        , font(nextFont)
        , label(nextLabel)
    {
    }

    void setValue(float nextValue)
    {
        value = std::clamp(nextValue, 0.0f, 1.0f);
    }

    float getValue() const
    {
        return value;
    }

    void update(sf::Vector2f mousePos)
    {
        hovered = bounds().contains(mousePos);
    }

    bool beginDrag(sf::Vector2f mousePos)
    {
        if (!bounds().contains(mousePos))
        {
            return false;
        }

        dragging = true;
        setValueFromMouse(mousePos);
        return true;
    }

    bool dragTo(sf::Vector2f mousePos)
    {
        if (!dragging)
        {
            return false;
        }

        setValueFromMouse(mousePos);
        return true;
    }

    void endDrag()
    {
        dragging = false;
    }

    void draw(sf::RenderWindow& window) const
    {
        const sf::Color labelColor = hovered || dragging ? sf::Color(255, 244, 215) : sf::Color(220, 224, 230);
        bayou::client::drawText(window, font, label, 18, position, labelColor);
        bayou::client::drawText(
            window,
            font,
            std::to_string(static_cast<int>(std::lround(value * 100.0f))) + "%",
            16,
            {position.x + size.x - 54.0f, position.y + 2.0f},
            sf::Color(248, 214, 112),
            54.0f);

        const sf::Vector2f trackPos{position.x, position.y + 36.0f};
        const sf::Vector2f trackSize{size.x, 16.0f};

        sf::RectangleShape shadow({trackSize.x, trackSize.y});
        shadow.setPosition(trackPos + sf::Vector2f(2.0f, 4.0f));
        shadow.setFillColor(sf::Color(0, 0, 0, 110));
        window.draw(shadow);

        sf::RectangleShape trough(trackSize);
        trough.setPosition(trackPos);
        trough.setFillColor(sf::Color(14, 22, 24, 238));
        trough.setOutlineThickness(2.0f);
        trough.setOutlineColor(hovered || dragging ? sf::Color(239, 190, 98) : sf::Color(122, 88, 51));
        window.draw(trough);

        const float fillWidth = trackSize.x * value;
        if (fillWidth > 0.0f)
        {
            sf::RectangleShape fill({fillWidth, trackSize.y - 6.0f});
            fill.setPosition({trackPos.x + 3.0f, trackPos.y + 3.0f});
            fill.setFillColor(sf::Color(158, 200, 216, dragging ? 235 : 205));
            window.draw(fill);
        }

        for (int i = 0; i <= 4; ++i)
        {
            const float x = trackPos.x + static_cast<float>(i) * trackSize.x / 4.0f;
            sf::RectangleShape tick({1.0f, 8.0f});
            tick.setPosition({x, trackPos.y + trackSize.y + 5.0f});
            tick.setFillColor(sf::Color(154, 112, 61, 165));
            window.draw(tick);
        }

        const float knobX = trackPos.x + trackSize.x * value;
        sf::CircleShape knob(12.0f);
        knob.setOrigin({12.0f, 12.0f});
        knob.setPosition({knobX, trackPos.y + trackSize.y * 0.5f});
        knob.setFillColor(dragging ? sf::Color(255, 222, 136) : sf::Color(224, 166, 82));
        knob.setOutlineThickness(3.0f);
        knob.setOutlineColor(sf::Color(67, 44, 28));
        window.draw(knob);

        sf::CircleShape cap(5.0f);
        cap.setOrigin({5.0f, 5.0f});
        cap.setPosition({knobX - 2.5f, trackPos.y + trackSize.y * 0.5f - 3.0f});
        cap.setFillColor(sf::Color(255, 239, 188, hovered || dragging ? 190 : 120));
        window.draw(cap);
    }

private:
    sf::FloatRect bounds() const
    {
        return {position, {size.x, size.y + 28.0f}};
    }

    void setValueFromMouse(sf::Vector2f mousePos)
    {
        const float trackLeft = position.x;
        const float trackRight = position.x + size.x;
        setValue((mousePos.x - trackLeft) / (trackRight - trackLeft));
    }
};
