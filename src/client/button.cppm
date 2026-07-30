module;

#include <SFML/Graphics.hpp>

#include "client_ui.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>

export module button;

// Module linkage rather than an anonymous namespace: the exported inline
// members below reference this, and an internal-linkage entity cannot be
// referenced from an inline function that importers instantiate.
std::function<void()> buttonClickHandler;

export void setButtonClickHandler(std::function<void()> handler)
{
    buttonClickHandler = std::move(handler);
}

// What a button is for, which decides how loudly it reads. Secondary is the
// default so every existing call site keeps its current appearance.
export enum class ButtonVariant
{
    Primary,   // the one action the screen exists for
    Secondary, // ordinary actions
    Quiet,     // back, cancel, dismiss: present but never competing
    Danger,    // destructive confirmation
};

export struct Button
{
    sf::RectangleShape shape;
    sf::Text text;
    bool hovered = false;
    bool pressed = false;
    bool focused = false;

    Button(const sf::Vector2f& position, const sf::Vector2f& size, const std::string& label, sf::Font& font)
        : text(font, label, 24)
    {
        shape.setPosition(position);
        shape.setSize(size);
        shape.setFillColor(sf::Color(35, 27, 21, 244));
        shape.setOutlineThickness(2);
        shape.setOutlineColor(sf::Color(176, 123, 59));

        text.setFillColor(sf::Color(246, 232, 200));
        fitAndCenterLabel();
    }

    void setLabel(const std::string& label)
    {
        text.setString(label);
        fitAndCenterLabel();
    }

    void setPosition(const sf::Vector2f& position)
    {
        shape.setPosition(position);
        fitAndCenterLabel();
    }

    void setSize(const sf::Vector2f& size)
    {
        shape.setSize(size);
        fitAndCenterLabel();
    }

    // Pins the label size instead of auto-fitting it. Use when a screen needs a
    // deliberate type step (a footer action smaller than a primary one) rather
    // than whatever size happens to fit the plate.
    void setLabelSize(unsigned int size)
    {
        pinnedLabelSize = size;
        fitAndCenterLabel();
    }

    void setVariant(ButtonVariant nextVariant)
    {
        variant = nextVariant;
        fitAndCenterLabel();
    }

    ButtonVariant getVariant() const { return variant; }

    // A disabled button still occupies its slot and still reads as a control;
    // it just plainly cannot be used. Hiding it instead makes layouts jump.
    void setEnabled(bool next)
    {
        enabled = next;
        if (!enabled)
        {
            hovered = false;
            pressed = false;
        }
    }

    bool isEnabled() const { return enabled; }

    void setFocused(bool next) { focused = next; }

    void update(const sf::Vector2f& mousePos)
    {
        if (!enabled)
        {
            hovered = false;
            pressed = false;
            return;
        }

        hovered = shape.getGlobalBounds().contains(mousePos);
        pressed = hovered && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
        shape.setFillColor(hovered ? sf::Color(83, 50, 25, 248) : sf::Color(35, 27, 21, 244));
        shape.setOutlineColor(hovered ? sf::Color(239, 190, 98) : sf::Color(176, 123, 59));
        text.setFillColor(hovered ? sf::Color(255, 244, 215) : sf::Color(246, 232, 200));
    }

    bool isClicked(const sf::Vector2f& mousePos)
    {
        if (!enabled)
        {
            return false;
        }

        const bool clicked = shape.getGlobalBounds().contains(mousePos);
        if (clicked && buttonClickHandler)
        {
            buttonClickHandler();
        }
        return clicked;
    }

    void draw(sf::RenderWindow& window) const
    {
        draw(window, 0.0f);
    }

    // `focusPhase` drives the focus-ring pulse; pass the frame's animation time.
    void draw(sf::RenderWindow& window, float focusPhase) const
    {
        using bayou::client::PlateState;
        using bayou::client::PlateStyle;

        const sf::Vector2f position = shape.getPosition();
        const sf::Vector2f size = shape.getSize();

        PlateStyle style;
        style.cut = std::clamp(size.y * 0.20f, 5.0f, 11.0f);
        style.fill = bodyColor();
        style.frame = frameColor();
        style.state = !enabled ? PlateState::Disabled
            : pressed         ? PlateState::Pressed
            : hovered         ? PlateState::Hover
                              : PlateState::Normal;
        style.focused = focused;
        style.focusPhase = focusPhase;
        bayou::client::drawMaterialPlate(window, position, size, style);

        if (variant == ButtonVariant::Primary && enabled)
        {
            // A warm bloom behind the primary plate, brighter on hover. This is
            // what makes one button obviously the way forward.
            bayou::client::drawRadialGlow(
                window,
                {position.x + size.x * 0.5f, position.y + size.y * 0.5f},
                size.x * 0.62f,
                sf::Color(226, 164, 74, hovered ? 54 : 30));
        }

        if (size.x >= 120.0f && size.y >= 34.0f)
        {
            const float pipeAlpha = enabled ? 160.0f : 80.0f;
            sf::RectangleShape leftPipe({10.0f, size.y * 0.34f});
            leftPipe.setPosition({position.x - 5.0f, position.y + size.y * 0.33f});
            leftPipe.setFillColor(sf::Color(74, 44, 22, static_cast<std::uint8_t>(pipeAlpha)));
            leftPipe.setOutlineThickness(1.0f);
            leftPipe.setOutlineColor(sf::Color(124, 76, 36, static_cast<std::uint8_t>(pipeAlpha)));
            window.draw(leftPipe);

            sf::RectangleShape rightPipe(leftPipe);
            rightPipe.setPosition({position.x + size.x - 5.0f, position.y + size.y * 0.33f});
            window.draw(rightPipe);
        }

        sf::Text label = text;
        label.setFillColor(labelColor());
        if (pressed)
        {
            label.setPosition(label.getPosition() + sf::Vector2f(0.0f, 1.0f));
        }
        bayou::client::drawCrispText(window, label);
    }

private:
    ButtonVariant variant = ButtonVariant::Secondary;
    bool enabled = true;
    unsigned int pinnedLabelSize = 0;

    sf::Color bodyColor() const
    {
        switch (variant)
        {
        case ButtonVariant::Primary:
            return hovered ? sf::Color(106, 66, 30, 250) : sf::Color(64, 42, 22, 248);
        case ButtonVariant::Quiet:
            return hovered ? sf::Color(46, 40, 34, 236) : sf::Color(20, 24, 25, 228);
        case ButtonVariant::Danger:
            return hovered ? sf::Color(112, 38, 30, 249) : sf::Color(58, 26, 22, 244);
        case ButtonVariant::Secondary:
            break;
        }
        return hovered ? sf::Color(84, 51, 25, 248) : sf::Color(31, 27, 23, 244);
    }

    sf::Color frameColor() const
    {
        switch (variant)
        {
        case ButtonVariant::Primary:
            return hovered ? sf::Color(255, 214, 128) : sf::Color(224, 170, 84);
        case ButtonVariant::Quiet:
            return hovered ? sf::Color(178, 138, 84) : sf::Color(112, 86, 52);
        case ButtonVariant::Danger:
            return hovered ? sf::Color(236, 140, 112) : sf::Color(170, 84, 62);
        case ButtonVariant::Secondary:
            break;
        }
        return hovered ? sf::Color(239, 190, 98) : sf::Color(176, 123, 59);
    }

    sf::Color labelColor() const
    {
        if (!enabled)
        {
            return bayou::client::palette::InkFaint;
        }
        if (variant == ButtonVariant::Quiet)
        {
            return hovered ? bayou::client::palette::Ink : bayou::client::palette::InkMuted;
        }
        return hovered ? bayou::client::palette::InkBright : bayou::client::palette::Ink;
    }

    void fitAndCenterLabel()
    {
        const sf::Vector2f size = shape.getSize();
        const unsigned int minimumCharacterSize = size.y <= 32.0f ? 10 : 14;
        text.setCharacterSize(pinnedLabelSize > 0 ? pinnedLabelSize : (size.y <= 40.0f ? 18 : 24));
        while (pinnedLabelSize == 0 && text.getCharacterSize() > minimumCharacterSize)
        {
            const sf::FloatRect bounds = text.getLocalBounds();
            if (bounds.size.x <= size.x - 24.0f && bounds.size.y <= size.y - 12.0f)
            {
                break;
            }
            text.setCharacterSize(text.getCharacterSize() - 1);
        }

        const sf::Vector2f position = shape.getPosition();
        bayou::client::centerButtonText(text, {position.x + size.x / 2.0f, position.y + size.y / 2.0f});
    }
};
