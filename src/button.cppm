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
        shape.setFillColor(sf::Color(70, 70, 70));
        shape.setOutlineThickness(2);
        shape.setOutlineColor(sf::Color::White);

        text.setFillColor(sf::Color::White);

        sf::FloatRect textBounds = text.getLocalBounds();
        text.setOrigin({textBounds.position.x + textBounds.size.x / 2.0f, textBounds.position.y + textBounds.size.y / 2.0f});
        text.setPosition({position.x + size.x / 2.0f, position.y + size.y / 2.0f});
    }

    void update(const sf::Vector2f& mousePos)
    {
        hovered = shape.getGlobalBounds().contains(mousePos);
        shape.setFillColor(hovered ? sf::Color(100, 100, 100) : sf::Color(70, 70, 70));
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
