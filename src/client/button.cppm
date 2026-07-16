module;

#include <SFML/Graphics.hpp>

#include "client_ui.hpp"

#include <functional>

export module button;

namespace
{
std::function<void()> buttonClickHandler;
}

export void setButtonClickHandler(std::function<void()> handler)
{
    buttonClickHandler = std::move(handler);
}

export struct Button
{
    sf::RectangleShape shape;
    sf::Text text;
    bool hovered = false;

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

    void update(const sf::Vector2f& mousePos)
    {
        hovered = shape.getGlobalBounds().contains(mousePos);
        shape.setFillColor(hovered ? sf::Color(83, 50, 25, 248) : sf::Color(35, 27, 21, 244));
        shape.setOutlineColor(hovered ? sf::Color(239, 190, 98) : sf::Color(176, 123, 59));
        text.setFillColor(hovered ? sf::Color(255, 244, 215) : sf::Color(246, 232, 200));
    }

    bool isClicked(const sf::Vector2f& mousePos)
    {
        const bool clicked = shape.getGlobalBounds().contains(mousePos);
        if (clicked && buttonClickHandler)
        {
            buttonClickHandler();
        }
        return clicked;
    }

    void draw(sf::RenderWindow& window) const
    {
        const sf::Vector2f position = shape.getPosition();
        const sf::Vector2f size = shape.getSize();
        bayou::client::drawBeveledPlate(
            window,
            position,
            size,
            hovered ? sf::Color(84, 51, 25, 248) : sf::Color(31, 27, 23, 244),
            hovered ? sf::Color(239, 190, 98) : sf::Color(176, 123, 59),
            hovered,
            std::clamp(size.y * 0.20f, 5.0f, 11.0f));

        if (size.x >= 120.0f && size.y >= 34.0f)
        {
            sf::RectangleShape leftPipe({10.0f, size.y * 0.34f});
            leftPipe.setPosition({position.x - 5.0f, position.y + size.y * 0.33f});
            leftPipe.setFillColor(sf::Color(74, 44, 22, 160));
            leftPipe.setOutlineThickness(1.0f);
            leftPipe.setOutlineColor(sf::Color(124, 76, 36, 160));
            window.draw(leftPipe);

            sf::RectangleShape rightPipe(leftPipe);
            rightPipe.setPosition({position.x + size.x - 5.0f, position.y + size.y * 0.33f});
            window.draw(rightPipe);
        }

        window.draw(text);
    }

private:
    void fitAndCenterLabel()
    {
        const sf::Vector2f size = shape.getSize();
        const unsigned int minimumCharacterSize = size.y <= 32.0f ? 10 : 14;
        text.setCharacterSize(size.y <= 40.0f ? 18 : 24);
        while (text.getCharacterSize() > minimumCharacterSize)
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
