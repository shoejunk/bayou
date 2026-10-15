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
    std::optional<std::size_t> focusedIndex;

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

    void setFocused(std::optional<std::size_t> index)
    {
        focusedIndex = index && *index < labels.size() ? index : std::nullopt;
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
        if (!labels.empty())
        {
            // The tabs sit in one shallow instrument rail. It keeps inactive
            // tabs from becoming floating cards and makes the active tab feel
            // mechanically docked to the panel beneath it.
            bayou::client::PlateStyle rail;
            rail.fill = bayou::client::palette::PlateDeep;
            rail.frame = bayou::client::palette::BrassDim;
            rail.cut = 10.0f;
            rail.recessed = true;
            rail.rivets = false;
            rail.brackets = false;
            rail.sheen = 0.30f;
            bayou::client::drawMaterialPlate(
                window,
                position - sf::Vector2f(6.0f, 5.0f),
                {tabSize.x * static_cast<float>(labels.size()) + 12.0f, tabSize.y + 10.0f},
                rail);
        }

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
            ? bayou::client::palette::SurfaceWarm
            : hovered ? bayou::client::palette::SurfaceTop : bayou::client::palette::Plate;
        const sf::Color outline = active
            ? bayou::client::palette::BrassBright
            : hovered ? bayou::client::palette::Brass : bayou::client::palette::BrassDim;

        bayou::client::PlateStyle style;
        style.fill = fill;
        style.frame = outline;
        style.cut = 10.0f;
        style.state = active ? bayou::client::PlateState::Selected
            : hovered      ? bayou::client::PlateState::Hover
                           : bayou::client::PlateState::Normal;
        style.focused = focusedIndex && *focusedIndex == index;
        style.focusPhase = 0.0f;
        // An inactive tab is a lid, not a button: dropping the sheen and studs is
        // what lets the active one come forward.
        style.sheen = active ? 1.0f : 0.35f;
        // A hovered tab gets the same small steel studs as the selected tab.
        // This makes the click target legible before activation, especially on
        // the wide Settings strip where inactive plates otherwise read as
        // decorative trim at a glance.
        style.rivets = active || hovered;
        style.brackets = active || hovered;
        style.recessed = !active;

        if (active)
        {
            // The violet bloom belongs under the tab face; keeping it behind the
            // plate preserves the engraved text and lets the steel rim carry the
            // emphasis instead of washing the label out.
            bayou::client::drawRadialGlow(
                window,
                {pos.x + tabSize.x * 0.5f, pos.y + tabSize.y * 0.5f},
                tabSize.x * 0.55f,
                sf::Color(132, 86, 176, hovered ? 34 : 20));
        }
        bayou::client::drawMaterialPlate(window, pos, tabSize, style);

        if (active)
        {
            // A steel connector along the bottom edge instead of a pointer
            // triangle: the tab reads as joined to the content below it, and it
            // cannot collide with whatever the panel draws at its own top edge.
            sf::RectangleShape connector({tabSize.x - 12.0f, 4.0f});
            connector.setPosition({pos.x + 6.0f, pos.y + tabSize.y - 2.0f});
            connector.setFillColor(bayou::client::palette::BrassBright);
            window.draw(connector);

            sf::RectangleShape connectorShade({tabSize.x - 12.0f, 1.0f});
            connectorShade.setPosition({pos.x + 6.0f, pos.y + tabSize.y + 2.0f});
            connectorShade.setFillColor(bayou::client::palette::BrassDeep);
            window.draw(connectorShade);

            sf::RectangleShape activeRail({tabSize.x - 28.0f, 1.0f});
            activeRail.setPosition({pos.x + 14.0f, pos.y + 6.0f});
            activeRail.setFillColor(bayou::client::withAlphaForControls(
                bayou::client::palette::BrassGlow, hovered ? 210 : 145));
            window.draw(activeRail);
        }

        // Tab labels are tracked caps in the display face when one is available:
        // navigation type should never match the body copy it switches between.
        sf::Text text(
            bayou::client::displayFontOr(font),
            labels[index],
            active ? 19u : 18u);
        text.setLetterSpacing(active ? 1.02f : 1.01f);
        text.setFillColor(active || hovered ? bayou::client::palette::InkBright : bayou::client::palette::InkMuted);
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
    bool focused = false;

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

    void setFocused(bool next)
    {
        focused = next;
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
        const sf::Color labelColor = focused
            ? bayou::client::palette::FocusBright
            : (hovered || dragging ? bayou::client::palette::InkBright : bayou::client::palette::Ink);
        bayou::client::drawText(window, font, label, 18, position, labelColor);
        bayou::client::drawText(
            window,
            font,
            std::to_string(static_cast<int>(std::lround(value * 100.0f))) + "%",
            16,
            {position.x + size.x - 54.0f, position.y + 2.0f},
            bayou::client::palette::SteelBright,
            54.0f);

        const sf::Vector2f trackPos{position.x, position.y + 36.0f};
        const sf::Vector2f trackSize{size.x, 16.0f};

        bayou::client::PlateStyle trackStyle;
        trackStyle.fill = bayou::client::palette::PlateDeep;
        trackStyle.frame = hovered || dragging ? bayou::client::palette::BrassBright : bayou::client::palette::BrassDim;
        trackStyle.cut = 4.0f;
        trackStyle.state = hovered || dragging ? bayou::client::PlateState::Hover : bayou::client::PlateState::Normal;
        trackStyle.recessed = true;
        trackStyle.rivets = false;
        trackStyle.brackets = false;
        trackStyle.sheen = 0.24f;
        trackStyle.focused = focused;
        bayou::client::drawMaterialPlate(window, trackPos, trackSize, trackStyle);

        const float fillWidth = trackSize.x * value;
        if (fillWidth > 6.0f)
        {
            // Graded steel keeps the control in the same forged material family
            // as the plates around it.
            bayou::client::drawVerticalGradient(
                window,
                {{trackPos.x + 3.0f, trackPos.y + 3.0f}, {std::max(1.0f, fillWidth - 6.0f), trackSize.y - 6.0f}},
                sf::Color(210, 228, 234, dragging ? 250 : 226),
                sf::Color(91, 115, 126, dragging ? 250 : 226));

            sf::RectangleShape sheen({std::max(0.0f, fillWidth - 8.0f), 1.0f});
            sheen.setPosition({trackPos.x + 4.0f, trackPos.y + 4.0f});
            sheen.setFillColor(sf::Color(240, 249, 252, 150));
            window.draw(sheen);
        }

        for (int i = 0; i <= 4; ++i)
        {
            const float x = trackPos.x + static_cast<float>(i) * trackSize.x / 4.0f;
            sf::RectangleShape tick({1.0f, 8.0f});
            tick.setPosition({x, trackPos.y + trackSize.y + 5.0f});
            tick.setFillColor(sf::Color(121, 143, 151, 165));
            window.draw(tick);
        }

        // Steel end stops make the slider read as a calibrated instrument,
        // not a generic HTML progress bar. They also provide a clear visual
        // affordance when the value is at either extreme.
        bayou::client::drawDiamond(
            window,
            {trackPos.x + 4.0f, trackPos.y + trackSize.y * 0.5f},
            5.0f,
            8.0f,
            bayou::client::withAlphaForControls(bayou::client::palette::Brass, 190),
            bayou::client::withAlphaForControls(bayou::client::palette::BrassShadow, 220));
        bayou::client::drawDiamond(
            window,
            {trackPos.x + trackSize.x - 4.0f, trackPos.y + trackSize.y * 0.5f},
            5.0f,
            8.0f,
            bayou::client::withAlphaForControls(bayou::client::palette::Brass, 190),
            bayou::client::withAlphaForControls(bayou::client::palette::BrassShadow, 220));

        const float knobX = trackPos.x + trackSize.x * value;
        const sf::Vector2f knobCenter{knobX, trackPos.y + trackSize.y * 0.5f};

        if (hovered || dragging || focused)
        {
            bayou::client::drawRadialGlow(
                window,
                knobCenter,
                24.0f,
                sf::Color(190, 222, 232, dragging ? 76 : (focused ? 42 : 48)));
        }

        // A struck steel stud rather than a flat disc, so the handle looks like
        // part of the same machined interface as the plates around it.
        bayou::client::drawStud(
            window,
            knobCenter,
            std::clamp(size.y * 0.18f, 9.0f, 11.0f),
            dragging ? sf::Color(229, 242, 246) : (hovered || focused ? sf::Color(198, 220, 227) : sf::Color(147, 174, 184)));
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
        if (trackRight <= trackLeft)
        {
            return;
        }
        setValue((mousePos.x - trackLeft) / (trackRight - trackLeft));
    }
};
