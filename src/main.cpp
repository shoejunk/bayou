#include <SFML/Graphics.hpp>

int main()
{
    sf::RenderWindow window(sf::VideoMode({800, 600}), "SFML Window");
    window.setFramerateLimit(60);

    sf::CircleShape circle(50.0f);
    circle.setFillColor(sf::Color::Green);
    circle.setPosition({375.0f, 275.0f});

    sf::RectangleShape rect(sf::Vector2f(100.0f, 80.0f));
    rect.setFillColor(sf::Color::Blue);
    rect.setPosition({150.0f, 100.0f});

    sf::RectangleShape movingRect(sf::Vector2f(60.0f, 60.0f));
    movingRect.setFillColor(sf::Color::Red);
    float xPos = 600.0f;
    float yPos = 400.0f;
    float xVel = 3.0f;
    float yVel = 2.5f;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        xPos += xVel;
        yPos += yVel;

        if (xPos <= 0 || xPos >= 740)
            xVel = -xVel;
        if (yPos <= 0 || yPos >= 540)
            yVel = -yVel;

        movingRect.setPosition({xPos, yPos});

        window.clear(sf::Color::Black);
        window.draw(circle);
        window.draw(rect);
        window.draw(movingRect);
        window.display();
    }

    return 0;
}
