module;

#include <SFML/Graphics.hpp>

export module inputbox;

export struct InputBox
{
    sf::RectangleShape shape;
    sf::Text text;
    sf::Text label;
    std::string content;
    bool active = false;
    bool isPassword = false;
    float cursorTimer = 0.0f;
    bool showCursor = false;

    InputBox(const sf::Vector2f& position, const sf::Vector2f& size, const std::string& labelText, sf::Font& font, bool password = false)
        : label(font, labelText, 18), text(font, "", 20), isPassword(password)
    {
        shape.setPosition(position);
        shape.setSize(size);
        shape.setFillColor(sf::Color(50, 50, 50));
        shape.setOutlineThickness(2);
        shape.setOutlineColor(sf::Color::White);

        label.setFillColor(sf::Color::White);
        label.setPosition({position.x, position.y - 25.0f});

        text.setFillColor(sf::Color::White);
        text.setPosition({position.x + 10.0f, position.y + size.y / 2.0f - 10.0f});
    }

    void update(const sf::Vector2f& mousePos)
    {
        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
        {
            setActive(shape.getGlobalBounds().contains(mousePos));
        }
    }

    bool contains(const sf::Vector2f& point) const
    {
        return shape.getGlobalBounds().contains(point);
    }

    void setActive(bool isActive)
    {
        active = isActive;
        cursorTimer = 0.0f;
        showCursor = active;
        shape.setOutlineColor(active ? sf::Color(100, 200, 255) : sf::Color::White);
    }

    void handleEvent(const sf::Event& event)
    {
        if (!active) return;

        if (const auto* textEvent = event.getIf<sf::Event::TextEntered>())
        {
            char c = static_cast<char>(textEvent->unicode);
            if (c >= 32 && c < 127)
            {
                content += c;
            }
            else if (c == 8 && !content.empty())
            {
                content.pop_back();
            }
            updateDisplayText();
        }
    }

    void updateDisplayText()
    {
        if (isPassword)
        {
            text.setString(std::string(content.size(), '*'));
        }
        else
        {
            text.setString(content);
        }
    }

    void updateCursor(float deltaTime)
    {
        if (!active) return;
        cursorTimer += deltaTime;
        if (cursorTimer >= 0.5f)
        {
            cursorTimer = 0.0f;
            showCursor = !showCursor;
        }
    }

    void draw(sf::RenderWindow& window) const
    {
        window.draw(label);
        window.draw(shape);
        window.draw(text);
    }

    const std::string& getContent() const { return content; }
    void clear() { content.clear(); updateDisplayText(); }
};
