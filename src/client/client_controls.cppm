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
        // Inactive tabs first, so the active plate's frame and its connector bar
        // sit over its neighbours rather than being clipped by them.
        for (std::size_t i = 0; i < labels.size(); ++i)
        {
            if (i != activeIndex)
            {
                drawTab(window, i);
            }
        }
        if (activeIndex < labels.size())
        {
            drawTab(window, activeIndex);
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
            ? sf::Color(84, 51, 25, 248)
            : hovered ? sf::Color(55, 39, 27, 242) : sf::Color(24, 23, 21, 236);
        const sf::Color outline = active || hovered ? sf::Color(239, 190, 98) : sf::Color(147, 101, 54);

        bayou::client::PlateStyle style;
        style.fill = fill;
        style.frame = outline;
        style.cut = 10.0f;
        style.state = active ? bayou::client::PlateState::Selected
            : hovered      ? bayou::client::PlateState::Hover
                           : bayou::client::PlateState::Normal;
        // An inactive tab is a lid, not a button: dropping the sheen and studs is
        // what lets the active one come forward.
        style.sheen = active ? 1.0f : 0.35f;
        // A hovered tab gets the same small brass studs as the selected tab.
        // This makes the click target legible before activation, especially on
        // the wide Settings strip where inactive plates otherwise read as
        // decorative trim at a glance.
        style.rivets = active || hovered;
        bayou::client::drawMaterialPlate(window, pos, tabSize, style);

        if (active)
        {
            // A brass connector along the bottom edge instead of a pointer
            // triangle: the tab reads as joined to the content below it, and it
            // cannot collide with whatever the panel draws at its own top edge.
            sf::RectangleShape connector({tabSize.x - 12.0f, 4.0f});
            connector.setPosition({pos.x + 6.0f, pos.y + tabSize.y - 2.0f});
            connector.setFillColor(sf::Color(239, 190, 98));
            window.draw(connector);

            sf::RectangleShape connectorShade({tabSize.x - 12.0f, 1.0f});
            connectorShade.setPosition({pos.x + 6.0f, pos.y + tabSize.y + 2.0f});
            connectorShade.setFillColor(sf::Color(76, 44, 20, 200));
            window.draw(connectorShade);
        }

        // Tab labels are tracked caps in the display face when one is available:
        // navigation type should never match the body copy it switches between.
        sf::Text text(
            bayou::client::displayFontOr(font),
            labels[index],
            active ? 19u : 18u);
        text.setFillColor(active || hovered ? sf::Color(255, 244, 215) : sf::Color(203, 182, 146));
        bayou::client::centerButtonText(text, {pos.x + tabSize.x * 0.5f, pos.y + tabSize.y * 0.52f});
        bayou::client::drawCrispText(window, text);
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
        const sf::Color labelColor = hovered || dragging ? sf::Color(255, 244, 215) : sf::Color(246, 232, 200);
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

        bayou::client::drawBeveledPlate(
            window,
            trackPos,
            trackSize,
            sf::Color(8, 13, 14, 244),
            hovered || dragging ? sf::Color(239, 190, 98) : sf::Color(122, 88, 51),
            hovered || dragging,
            4.0f);

        const float fillWidth = trackSize.x * value;
        if (fillWidth > 0.0f)
        {
            // Graded brass, not the old flat teal: it was the only cool-blue fill
            // anywhere in a warm interface.
            bayou::client::drawVerticalGradient(
                window,
                {{trackPos.x + 3.0f, trackPos.y + 3.0f}, {fillWidth - 6.0f, trackSize.y - 6.0f}},
                sf::Color(246, 199, 108, dragging ? 250 : 226),
                sf::Color(168, 108, 46, dragging ? 250 : 226));

            sf::RectangleShape sheen({std::max(0.0f, fillWidth - 8.0f), 1.0f});
            sheen.setPosition({trackPos.x + 4.0f, trackPos.y + 4.0f});
            sheen.setFillColor(sf::Color(255, 236, 182, 150));
            window.draw(sheen);
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
        const sf::Vector2f knobCenter{knobX, trackPos.y + trackSize.y * 0.5f};

        if (hovered || dragging)
        {
            bayou::client::drawRadialGlow(
                window,
                knobCenter,
                24.0f,
                sf::Color(240, 190, 100, dragging ? 76 : 48));
        }

        // A struck brass stud rather than a flat disc, so the handle looks like
        // part of the same machined interface as the plates around it.
        bayou::client::drawStud(
            window,
            knobCenter,
            11.0f,
            dragging ? sf::Color(252, 214, 132) : (hovered ? sf::Color(240, 190, 100) : sf::Color(216, 158, 76)));
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
