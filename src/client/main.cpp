#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

import button;
import inputbox;
import network;

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
    if (!font.openFromFile("assets/Roboto.ttf"))
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

    sf::Clock clock;

    GameState currentState = GameState::Menu;

    InputBox usernameInput({300.0f, 140.0f}, {200.0f, 40.0f}, "Username", font);
    InputBox passwordInput({300.0f, 220.0f}, {200.0f, 40.0f}, "Password", font, true);
    InputBox confirmInput({300.0f, 300.0f}, {200.0f, 40.0f}, "Confirm Password", font, true);
    Button submitButton({300.0f, 380.0f}, {200.0f, 50.0f}, "Create Account", font);

    sf::Text errorText(font, "", 20);
    errorText.setFillColor(sf::Color::Red);
    errorText.setPosition({400.0f, 450.0f});

    sf::Text statusText(font, "", 24);
    statusText.setFillColor(sf::Color::Yellow);
    statusText.setPosition({400.0f, 520.0f});

    sf::TcpSocket serverSocket;
    bool connected = false;

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
                        errorText.setString("");
                    }
                }
                else if (currentState == GameState::CreateAccount)
                {
                    usernameInput.update(mousePos);
                    passwordInput.update(mousePos);
                    confirmInput.update(mousePos);

                    if (submitButton.isClicked(mousePos))
                    {
                        if (passwordInput.getContent() != confirmInput.getContent())
                        {
                            errorText.setString("Passwords do not match!");
                            errorText.setFillColor(sf::Color::Red);
                            sf::FloatRect bounds = errorText.getLocalBounds();
                            errorText.setOrigin({bounds.position.x + bounds.size.x / 2.0f, 0});
                        }
                        else if (passwordInput.getContent().empty())
                        {
                            errorText.setString("Password cannot be empty!");
                            errorText.setFillColor(sf::Color::Red);
                            sf::FloatRect bounds = errorText.getLocalBounds();
                            errorText.setOrigin({bounds.position.x + bounds.size.x / 2.0f, 0});
                        }
                        else
                        {
                            if (!connected)
                            {
                                if (serverSocket.connect(sf::IpAddress::LocalHost, 55000) == sf::Socket::Status::Done)
                                {
                                    connected = true;
                                }
                                else
                                {
                                    errorText.setString("Failed to connect to server!");
                                    errorText.setFillColor(sf::Color::Red);
                                    sf::FloatRect bounds = errorText.getLocalBounds();
                                    errorText.setOrigin({bounds.position.x + bounds.size.x / 2.0f, 0});
                                }
                            }

                            if (connected)
                            {
                                sf::Packet packet;
                                packet << static_cast<uint8_t>(network::MessageType::CreateAccount);
                                packet << usernameInput.getContent();
                                packet << passwordInput.getContent();

                                if (serverSocket.send(packet) == sf::Socket::Status::Done)
                                {
                                    sf::Packet response;
                                    if (serverSocket.receive(response) == sf::Socket::Status::Done)
                                    {
                                        uint8_t msgType;
                                        bool success;
                                        std::string message;
                                        response >> msgType >> success >> message;

                                        if (success)
                                        {
                                            errorText.setString(message);
                                            errorText.setFillColor(sf::Color::Green);
                                        }
                                        else
                                        {
                                            errorText.setString(message);
                                            errorText.setFillColor(sf::Color::Red);
                                        }
                                        sf::FloatRect bounds = errorText.getLocalBounds();
                                        errorText.setOrigin({bounds.position.x + bounds.size.x / 2.0f, 0});
                                    }
                                    else
                                    {
                                        errorText.setString("No response from server!");
                                        errorText.setFillColor(sf::Color::Red);
                                        sf::FloatRect bounds = errorText.getLocalBounds();
                                        errorText.setOrigin({bounds.position.x + bounds.size.x / 2.0f, 0});
                                        connected = false;
                                    }
                                }
                                else
                                {
                                    errorText.setString("Failed to send to server!");
                                    errorText.setFillColor(sf::Color::Red);
                                    sf::FloatRect bounds = errorText.getLocalBounds();
                                    errorText.setOrigin({bounds.position.x + bounds.size.x / 2.0f, 0});
                                    connected = false;
                                }
                            }
                        }
                    }
                }
            }

            if (currentState == GameState::CreateAccount)
            {
                usernameInput.handleEvent(*event);
                passwordInput.handleEvent(*event);
                confirmInput.handleEvent(*event);
            }

            if (event->is<sf::Event::KeyPressed>())
            {
                if (event->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape)
                {
                    if (connected)
                    {
                        sf::Packet disconnectPacket;
                        disconnectPacket << static_cast<uint8_t>(network::MessageType::Disconnect);
                        serverSocket.send(disconnectPacket);
                        serverSocket.disconnect();
                        connected = false;
                    }
                    currentState = GameState::Menu;
                    statusText.setString("");
                    errorText.setString("");
                    errorText.setFillColor(sf::Color::Red);
                    usernameInput.clear();
                    passwordInput.clear();
                    confirmInput.clear();
                }
            }
        }

        if (currentState == GameState::Menu)
        {
            loginButton.update(mousePos);
            createButton.update(mousePos);
        }
        else if (currentState == GameState::CreateAccount)
        {
            usernameInput.update(mousePos);
            passwordInput.update(mousePos);
            confirmInput.update(mousePos);
            submitButton.update(mousePos);

            float deltaTime = clock.restart().asSeconds();
            usernameInput.updateCursor(deltaTime);
            passwordInput.updateCursor(deltaTime);
            confirmInput.updateCursor(deltaTime);
        }

        window.clear(sf::Color(30, 30, 30));

        if (currentState == GameState::Menu)
        {
            window.draw(title);
            loginButton.draw(window);
            createButton.draw(window);
        }
        else if (currentState == GameState::CreateAccount)
        {
            usernameInput.draw(window);
            passwordInput.draw(window);
            confirmInput.draw(window);
            submitButton.draw(window);
            window.draw(errorText);
        }

        window.display();
    }

    return 0;
}
