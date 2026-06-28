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
        shape.setFillColor(sf::Color(58, 43, 31, 244));
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
        shape.setFillColor(hovered ? sf::Color(91, 62, 35, 248) : sf::Color(58, 43, 31, 244));
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

        sf::RectangleShape shadow(size);
        shadow.setPosition(position + sf::Vector2f(3.0f, 4.0f));
        shadow.setFillColor(sf::Color(0, 0, 0, 110));
        window.draw(shadow);
        window.draw(shape);

        sf::RectangleShape inner({size.x - 8.0f, size.y - 8.0f});
        inner.setPosition(position + sf::Vector2f(4.0f, 4.0f));
        inner.setFillColor(sf::Color::Transparent);
        inner.setOutlineThickness(1.0f);
        inner.setOutlineColor(hovered ? sf::Color(224, 167, 80, 180) : sf::Color(104, 75, 43, 190));
        window.draw(inner);

        sf::RectangleShape highlight({size.x - 10.0f, 1.0f});
        highlight.setPosition(position + sf::Vector2f(5.0f, 5.0f));
        highlight.setFillColor(sf::Color(255, 224, 154, hovered ? 105 : 55));
        window.draw(highlight);

        const float rivetRadius = size.y >= 42.0f ? 2.0f : 1.5f;
        for (float x : {position.x + 8.0f, position.x + size.x - 8.0f})
        {
            sf::CircleShape rivet(rivetRadius);
            rivet.setOrigin({rivetRadius, rivetRadius});
            rivet.setPosition({x, position.y + size.y * 0.5f});
            rivet.setFillColor(sf::Color(205, 151, 72, 190));
            window.draw(rivet);
        }

        window.draw(text);
    }

private:
    void fitAndCenterLabel()
    {
        const sf::Vector2f size = shape.getSize();
        text.setCharacterSize(size.y <= 40.0f ? 18 : 24);
        while (text.getCharacterSize() > 14)
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
