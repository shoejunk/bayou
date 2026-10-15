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
        : text(bayou::client::displayFontOr(font), label, 24)
    {
        shape.setPosition(position);
        shape.setSize(size);
        shape.setFillColor(sf::Color(22, 29, 32, 244));
        shape.setOutlineThickness(2);
        shape.setOutlineColor(bayou::client::palette::Steel);

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
        // Caption-sized quiet actions are used by the main-menu footer. The
        // display face has a taller, finer silhouette than Roboto, so keep a
        // small floor that survives the native 800x600 layout.
        pinnedLabelSize = variant == ButtonVariant::Quiet && size <= 12 ? 14u : size;
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
            focused = false;
        }
    }

    bool isEnabled() const { return enabled; }

    void setFocused(bool next) { focused = next && enabled; }

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
        shape.setFillColor(hovered ? sf::Color(34, 45, 49, 248) : sf::Color(22, 29, 32, 244));
        shape.setOutlineColor(hovered ? bayou::client::palette::SteelBright : bayou::client::palette::Steel);
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
        // Quiet controls are still metal, but their smaller footprint needs
        // fewer ornaments to remain legible at the 800x600 design size.
        style.brackets = variant != ButtonVariant::Quiet;
        style.rivets = variant != ButtonVariant::Quiet;
        style.sheen = variant == ButtonVariant::Quiet ? 0.65f : 1.0f;

        if (variant == ButtonVariant::Primary && enabled)
        {
            // Draw the violet bloom first: it should lift the plate from the
            // background, not wash over its engraved face.
            bayou::client::drawRadialGlow(
                window,
                {position.x + size.x * 0.5f, position.y + size.y * 0.5f},
                size.x * 0.62f,
                sf::Color(132, 86, 176, hovered ? 54 : 30));
        }

        bayou::client::drawMaterialPlate(window, position, size, style);

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
            return hovered ? sf::Color(48, 39, 62, 250) : sf::Color(31, 28, 43, 248);
        case ButtonVariant::Quiet:
            return hovered ? sf::Color(31, 43, 47, 236) : sf::Color(18, 27, 30, 228);
        case ButtonVariant::Danger:
            return hovered ? sf::Color(112, 38, 30, 249) : sf::Color(58, 26, 22, 244);
        case ButtonVariant::Secondary:
            break;
        }
        return hovered ? sf::Color(38, 49, 54, 248) : sf::Color(22, 30, 34, 244);
    }

    sf::Color frameColor() const
    {
        switch (variant)
        {
        case ButtonVariant::Primary:
            return hovered ? bayou::client::palette::SteelBright : bayou::client::palette::Steel;
        case ButtonVariant::Quiet:
            return hovered ? bayou::client::palette::SteelBright : bayou::client::palette::SteelDim;
        case ButtonVariant::Danger:
            return hovered ? sf::Color(236, 140, 112) : sf::Color(170, 84, 62);
        case ButtonVariant::Secondary:
            break;
        }
        return hovered ? bayou::client::palette::SteelBright : bayou::client::palette::Steel;
    }

    sf::Color labelColor() const
    {
        if (!enabled)
        {
            return bayou::client::palette::InkFaint;
        }
        if (variant == ButtonVariant::Quiet)
        {
            return hovered ? bayou::client::palette::InkBright : bayou::client::palette::Ink;
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
