module;

#include <SFML/Graphics.hpp>

export module button;

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
        shape.setFillColor(sf::Color(72, 52, 35, 230));
        shape.setOutlineThickness(2);
        shape.setOutlineColor(sf::Color(196, 143, 72));

        text.setFillColor(sf::Color(246, 232, 200));

        sf::FloatRect textBounds = text.getLocalBounds();
        text.setOrigin({textBounds.position.x + textBounds.size.x / 2.0f, textBounds.position.y + textBounds.size.y / 2.0f});
        text.setPosition({position.x + size.x / 2.0f, position.y + size.y / 2.0f});
    }

    void update(const sf::Vector2f& mousePos)
    {
        hovered = shape.getGlobalBounds().contains(mousePos);
        shape.setFillColor(hovered ? sf::Color(112, 76, 42, 245) : sf::Color(72, 52, 35, 230));
        shape.setOutlineColor(hovered ? sf::Color(235, 188, 102) : sf::Color(196, 143, 72));
    }

    bool isClicked(const sf::Vector2f& mousePos) const
    {
        return shape.getGlobalBounds().contains(mousePos);
    }

    void draw(sf::RenderWindow& window) const
    {
        window.draw(shape);
        window.draw(text);
    }
};
