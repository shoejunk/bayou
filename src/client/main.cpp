#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include <chrono>
#include <cstdint>
#include <future>
#include <optional>
#include <string>

import button;
import inputbox;
import network;

namespace
{
constexpr unsigned short AccountServerPort = 55000;

enum class GameState
{
    Menu,
    Login,
    CreateAccount
};

struct ServerResult
{
    bool success = false;
    std::string message;
};

void centerText(sf::Text& text, float x)
{
    sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin({bounds.position.x + bounds.size.x / 2.0f, text.getOrigin().y});
    text.setPosition({x, text.getPosition().y});
}

void setMessage(sf::Text& text, const std::string& message, const sf::Color& color)
{
    text.setString(message);
    text.setFillColor(color);
    centerText(text, 400.0f);
}

ServerResult sendAccountRequest(
    network::MessageType requestType,
    network::MessageType expectedResponseType,
    const std::string& username,
    const std::string& password)
{
    sf::TcpSocket socket;
    if (socket.connect(sf::IpAddress::LocalHost, AccountServerPort) != sf::Socket::Status::Done)
    {
        return {false, "Failed to connect to server"};
    }

    sf::Packet packet;
    packet << static_cast<uint8_t>(requestType);
    packet << username;
    packet << password;

    if (socket.send(packet) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send to server"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No response from server"};
    }

    uint8_t responseType = 0;
    bool success = false;
    std::string message;
    response >> responseType >> success >> message;

    if (static_cast<network::MessageType>(responseType) != expectedResponseType)
    {
        socket.disconnect();
        return {false, "Unexpected response from server"};
    }

    sf::Packet disconnectPacket;
    disconnectPacket << static_cast<uint8_t>(network::MessageType::Disconnect);
    [[maybe_unused]] auto disconnectResult = socket.send(disconnectPacket);
    socket.disconnect();

    return {success, message};
}

void resetForm(InputBox& usernameInput, InputBox& passwordInput, InputBox& confirmInput, sf::Text& messageText)
{
    usernameInput.clear();
    passwordInput.clear();
    confirmInput.clear();
    setMessage(messageText, "", sf::Color::Red);
}
}

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
    title.setPosition({400.0f, 45.0f});
    centerText(title, 400.0f);

    Button loginButton({300.0f, 200.0f}, {200.0f, 60.0f}, "Login", font);
    Button createButton({300.0f, 300.0f}, {200.0f, 60.0f}, "Create Account", font);

    InputBox usernameInput({300.0f, 140.0f}, {200.0f, 40.0f}, "Username", font);
    InputBox passwordInput({300.0f, 220.0f}, {200.0f, 40.0f}, "Password", font, true);
    InputBox confirmInput({300.0f, 300.0f}, {200.0f, 40.0f}, "Confirm Password", font, true);
    Button loginSubmitButton({300.0f, 300.0f}, {200.0f, 50.0f}, "Login", font);
    Button createSubmitButton({300.0f, 380.0f}, {200.0f, 50.0f}, "Create Account", font);
    Button backButton({20.0f, 520.0f}, {120.0f, 45.0f}, "Back", font);

    sf::Text messageText(font, "", 20);
    messageText.setFillColor(sf::Color::Red);
    messageText.setPosition({400.0f, 450.0f});

    sf::Clock clock;
    GameState currentState = GameState::Menu;
    std::optional<std::future<ServerResult>> pendingRequest;

    auto startRequest = [&](network::MessageType requestType, network::MessageType expectedResponseType) {
        setMessage(messageText, requestType == network::MessageType::Login ? "Logging in..." : "Creating account...", sf::Color::Yellow);
        pendingRequest = std::async(
            std::launch::async,
            sendAccountRequest,
            requestType,
            expectedResponseType,
            usernameInput.getContent(),
            passwordInput.getContent());
    };

    auto returnToMenu = [&]() {
        currentState = GameState::Menu;
        title.setString("Main Menu");
        centerText(title, 400.0f);
        resetForm(usernameInput, passwordInput, confirmInput, messageText);
    };

    while (window.isOpen())
    {
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        if (pendingRequest &&
            pendingRequest->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            ServerResult result = pendingRequest->get();
            pendingRequest.reset();
            setMessage(messageText, result.message, result.success ? sf::Color::Green : sf::Color::Red);
        }

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>(); mousePressed && !pendingRequest)
            {
                sf::Vector2f clickPos = window.mapPixelToCoords(mousePressed->position);
                if (currentState == GameState::Menu)
                {
                    if (loginButton.isClicked(clickPos))
                    {
                        currentState = GameState::Login;
                        title.setString("Login");
                        centerText(title, 400.0f);
                        resetForm(usernameInput, passwordInput, confirmInput, messageText);
                    }
                    else if (createButton.isClicked(clickPos))
                    {
                        currentState = GameState::CreateAccount;
                        title.setString("Create Account");
                        centerText(title, 400.0f);
                        resetForm(usernameInput, passwordInput, confirmInput, messageText);
                    }
                }
                else if (currentState == GameState::Login)
                {
                    usernameInput.update(clickPos);
                    passwordInput.update(clickPos);

                    if (backButton.isClicked(clickPos))
                    {
                        returnToMenu();
                    }
                    else if (loginSubmitButton.isClicked(clickPos))
                    {
                        if (usernameInput.getContent().empty() || passwordInput.getContent().empty())
                        {
                            setMessage(messageText, "Username and password cannot be empty", sf::Color::Red);
                        }
                        else
                        {
                            startRequest(network::MessageType::Login, network::MessageType::LoginResponse);
                        }
                    }
                }
                else if (currentState == GameState::CreateAccount)
                {
                    usernameInput.update(clickPos);
                    passwordInput.update(clickPos);
                    confirmInput.update(clickPos);

                    if (backButton.isClicked(clickPos))
                    {
                        returnToMenu();
                    }
                    else if (createSubmitButton.isClicked(clickPos))
                    {
                        if (usernameInput.getContent().empty() || passwordInput.getContent().empty())
                        {
                            setMessage(messageText, "Username and password cannot be empty", sf::Color::Red);
                        }
                        else if (passwordInput.getContent() != confirmInput.getContent())
                        {
                            setMessage(messageText, "Passwords do not match", sf::Color::Red);
                        }
                        else
                        {
                            startRequest(network::MessageType::CreateAccount, network::MessageType::CreateAccountResponse);
                        }
                    }
                }
            }

            if (currentState == GameState::Login || currentState == GameState::CreateAccount)
            {
                usernameInput.handleEvent(*event);
                passwordInput.handleEvent(*event);
            }

            if (currentState == GameState::CreateAccount)
            {
                confirmInput.handleEvent(*event);
            }

            if (event->is<sf::Event::KeyPressed>())
            {
                if (event->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape)
                {
                    returnToMenu();
                }
            }
        }

        if (currentState == GameState::Menu)
        {
            loginButton.update(mousePos);
            createButton.update(mousePos);
        }
        else if (currentState == GameState::Login)
        {
            usernameInput.update(mousePos);
            passwordInput.update(mousePos);
            loginSubmitButton.update(mousePos);
            backButton.update(mousePos);

            float deltaTime = clock.restart().asSeconds();
            usernameInput.updateCursor(deltaTime);
            passwordInput.updateCursor(deltaTime);
        }
        else if (currentState == GameState::CreateAccount)
        {
            usernameInput.update(mousePos);
            passwordInput.update(mousePos);
            confirmInput.update(mousePos);
            createSubmitButton.update(mousePos);
            backButton.update(mousePos);

            float deltaTime = clock.restart().asSeconds();
            usernameInput.updateCursor(deltaTime);
            passwordInput.updateCursor(deltaTime);
            confirmInput.updateCursor(deltaTime);
        }

        window.clear(sf::Color(30, 30, 30));
        window.draw(title);

        if (currentState == GameState::Menu)
        {
            loginButton.draw(window);
            createButton.draw(window);
        }
        else if (currentState == GameState::Login)
        {
            usernameInput.draw(window);
            passwordInput.draw(window);
            loginSubmitButton.draw(window);
            backButton.draw(window);
            window.draw(messageText);
        }
        else if (currentState == GameState::CreateAccount)
        {
            usernameInput.draw(window);
            passwordInput.draw(window);
            confirmInput.draw(window);
            createSubmitButton.draw(window);
            backButton.draw(window);
            window.draw(messageText);
        }

        window.display();
    }

    return 0;
}
