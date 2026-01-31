#include <SFML/Graphics.hpp>

import button;

enum class GameState
{
    Menu,
    Login,
    CreateAccount
};

int main()
{
    sf::RenderWindow window(sf::VideoMode({800, 600}), "Main Menu");
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.openFromFile("C:/Windows/Fonts/arial.ttf"))
    {
        return 1;
    }

    sf::Text title(font, "Main Menu", 48);
    title.setFillColor(sf::Color::White);
    sf::FloatRect titleBounds = title.getLocalBounds();
    title.setOrigin({titleBounds.position.x + titleBounds.size.x / 2.0f, 0});
    title.setPosition({400.0f, 80.0f});

    Button loginButton({300.0f, 200.0f}, {200.0f, 60.0f}, "Login", font);
    Button createButton({300.0f, 300.0f}, {200.0f, 60.0f}, "Create Account", font);

    GameState currentState = GameState::Menu;

    sf::Text statusText(font, "", 24);
    statusText.setFillColor(sf::Color::Yellow);
    statusText.setPosition({400.0f, 450.0f});

    while (window.isOpen())
    {
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (event->is<sf::Event::MouseButtonPressed>())
            {
                if (currentState == GameState::Menu)
                {
                    if (loginButton.isClicked(mousePos))
                    {
                        currentState = GameState::Login;
                        statusText.setString("Login screen - Press ESC to go back");
                        sf::FloatRect bounds = statusText.getLocalBounds();
                        statusText.setOrigin({bounds.position.x + bounds.size.x / 2.0f, 0});
                    }
                    else if (createButton.isClicked(mousePos))
                    {
                        currentState = GameState::CreateAccount;
                        statusText.setString("Create Account screen - Press ESC to go back");
                        sf::FloatRect bounds = statusText.getLocalBounds();
                        statusText.setOrigin({bounds.position.x + bounds.size.x / 2.0f, 0});
                    }
                }
            }

            if (event->is<sf::Event::KeyPressed>())
            {
                if (event->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape)
                {
                    currentState = GameState::Menu;
                    statusText.setString("");
                }
            }
        }

        if (currentState == GameState::Menu)
        {
            loginButton.update(mousePos);
            createButton.update(mousePos);
        }

        window.clear(sf::Color(30, 30, 30));

        if (currentState == GameState::Menu)
        {
            window.draw(title);
            loginButton.draw(window);
            createButton.draw(window);
        }
        else
        {
            window.draw(statusText);
        }

        window.display();
    }

    return 0;
}
