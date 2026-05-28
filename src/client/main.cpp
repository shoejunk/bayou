#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>

import button;
import inputbox;
import network;

namespace
{
constexpr unsigned short AccountServerPort = 55000;
constexpr unsigned short MatchmakingServerPort = 55001;

enum class GameState
{
    Menu,
    Login,
    CreateAccount,
    Authenticated,
    Matchmaking,
    Game
};

struct ServerResult
{
    bool success = false;
    std::string message;
    std::shared_ptr<sf::TcpSocket> gameSocket;
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

ServerResult joinGameServer(int matchId, int playerNumber, unsigned short gamePort)
{
    if (gamePort == 0)
    {
        return {false, "Game server did not assign a game"};
    }

    auto socket = std::make_shared<sf::TcpSocket>();
    bool connected = false;
    for (int attempt = 0; attempt < 30; ++attempt)
    {
        if (socket->connect(sf::IpAddress::LocalHost, gamePort) == sf::Socket::Status::Done)
        {
            connected = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!connected)
    {
        socket->disconnect();
        return {false, "Failed to connect to game server"};
    }

    sf::Packet joinRequest;
    joinRequest << static_cast<uint8_t>(network::MessageType::JoinGame);
    joinRequest << matchId;
    joinRequest << playerNumber;

    if (socket->send(joinRequest) != sf::Socket::Status::Done)
    {
        socket->disconnect();
        return {false, "Failed to join game"};
    }

    sf::Packet response;
    if (socket->receive(response) != sf::Socket::Status::Done)
    {
        socket->disconnect();
        return {false, "No game server response"};
    }

    uint8_t responseType = 0;
    int responseMatchId = 0;
    int responsePlayerNumber = 0;
    std::string message;
    response >> responseType >> responseMatchId >> responsePlayerNumber >> message;
    if (static_cast<network::MessageType>(responseType) != network::MessageType::GameReady ||
        responseMatchId != matchId ||
        responsePlayerNumber != playerNumber)
    {
        socket->disconnect();
        return {false, "Unexpected game server response"};
    }

    return {true, message, socket};
}

ServerResult joinMatchmaking()
{
    sf::TcpSocket socket;
    if (socket.connect(sf::IpAddress::LocalHost, MatchmakingServerPort) != sf::Socket::Status::Done)
    {
        return {false, "Failed to connect to matchmaking"};
    }

    sf::Packet packet;
    packet << static_cast<uint8_t>(network::MessageType::JoinMatchmaking);

    if (socket.send(packet) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to join matchmaking"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Disconnected from matchmaking"};
    }

    uint8_t responseType = 0;
    int matchId = 0;
    int playerNumber = 0;
    unsigned short gamePort = 0;
    response >> responseType >> matchId >> playerNumber >> gamePort;
    socket.disconnect();

    if (static_cast<network::MessageType>(responseType) != network::MessageType::MatchFound)
    {
        return {false, "Unexpected matchmaking response"};
    }

    return joinGameServer(matchId, playerNumber, gamePort);
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
    Button playButton({300.0f, 270.0f}, {200.0f, 60.0f}, "Play", font);

    sf::Text messageText(font, "", 20);
    messageText.setFillColor(sf::Color::Red);
    messageText.setPosition({400.0f, 450.0f});

    sf::Clock clock;
    GameState currentState = GameState::Menu;
    std::optional<std::future<ServerResult>> pendingRequest;
    std::optional<std::future<ServerResult>> pendingMatchmaking;
    std::shared_ptr<sf::TcpSocket> activeGameSocket;
    int focusedInput = 0;

    auto clearFocus = [&]() {
        usernameInput.setActive(false);
        passwordInput.setActive(false);
        confirmInput.setActive(false);
    };

    auto focusLoginInput = [&](int index) {
        focusedInput = (index + 2) % 2;
        usernameInput.setActive(focusedInput == 0);
        passwordInput.setActive(focusedInput == 1);
        confirmInput.setActive(false);
    };

    auto focusCreateInput = [&](int index) {
        focusedInput = (index + 3) % 3;
        usernameInput.setActive(focusedInput == 0);
        passwordInput.setActive(focusedInput == 1);
        confirmInput.setActive(focusedInput == 2);
    };

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
        if (activeGameSocket)
        {
            activeGameSocket->disconnect();
            activeGameSocket.reset();
        }
        title.setString("Main Menu");
        centerText(title, 400.0f);
        resetForm(usernameInput, passwordInput, confirmInput, messageText);
        clearFocus();
    };

    auto showAuthenticatedScreen = [&]() {
        currentState = GameState::Authenticated;
        title.setString("");
        resetForm(usernameInput, passwordInput, confirmInput, messageText);
        clearFocus();
    };

    auto showGameScreen = [&](std::shared_ptr<sf::TcpSocket> gameSocket) {
        activeGameSocket = std::move(gameSocket);
        currentState = GameState::Game;
        title.setString("Game");
        centerText(title, 400.0f);
        setMessage(messageText, "", sf::Color::Red);
        clearFocus();
    };

    auto startMatchmaking = [&]() {
        currentState = GameState::Matchmaking;
        title.setString("Matchmaking");
        centerText(title, 400.0f);
        setMessage(messageText, "Finding match...", sf::Color::Yellow);
        pendingMatchmaking = std::async(std::launch::async, joinMatchmaking);
    };

    auto submitLogin = [&]() {
        if (usernameInput.getContent().empty() || passwordInput.getContent().empty())
        {
            setMessage(messageText, "Username and password cannot be empty", sf::Color::Red);
        }
        else
        {
            startRequest(network::MessageType::Login, network::MessageType::LoginResponse);
        }
    };

    auto submitCreateAccount = [&]() {
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
    };

    while (window.isOpen())
    {
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        if (pendingRequest &&
            pendingRequest->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            ServerResult result = pendingRequest->get();
            pendingRequest.reset();
            if (result.success)
            {
                showAuthenticatedScreen();
            }
            else
            {
                setMessage(messageText, result.message, sf::Color::Red);
            }
        }

        if (pendingMatchmaking &&
            pendingMatchmaking->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            ServerResult result = pendingMatchmaking->get();
            pendingMatchmaking.reset();
            if (result.success)
            {
                showGameScreen(result.gameSocket);
            }
            else
            {
                currentState = GameState::Authenticated;
                title.setString("");
                setMessage(messageText, result.message, sf::Color::Red);
            }
        }

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>(); mousePressed && !pendingRequest && !pendingMatchmaking)
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
                        focusLoginInput(0);
                    }
                    else if (createButton.isClicked(clickPos))
                    {
                        currentState = GameState::CreateAccount;
                        title.setString("Create Account");
                        centerText(title, 400.0f);
                        resetForm(usernameInput, passwordInput, confirmInput, messageText);
                        focusCreateInput(0);
                    }
                }
                else if (currentState == GameState::Login)
                {
                    if (backButton.isClicked(clickPos))
                    {
                        returnToMenu();
                    }
                    else if (loginSubmitButton.isClicked(clickPos))
                    {
                        submitLogin();
                    }
                    else if (usernameInput.contains(clickPos))
                    {
                        focusLoginInput(0);
                    }
                    else if (passwordInput.contains(clickPos))
                    {
                        focusLoginInput(1);
                    }
                    else
                    {
                        clearFocus();
                    }
                }
                else if (currentState == GameState::CreateAccount)
                {
                    if (backButton.isClicked(clickPos))
                    {
                        returnToMenu();
                    }
                    else if (createSubmitButton.isClicked(clickPos))
                    {
                        submitCreateAccount();
                    }
                    else if (usernameInput.contains(clickPos))
                    {
                        focusCreateInput(0);
                    }
                    else if (passwordInput.contains(clickPos))
                    {
                        focusCreateInput(1);
                    }
                    else if (confirmInput.contains(clickPos))
                    {
                        focusCreateInput(2);
                    }
                    else
                    {
                        clearFocus();
                    }
                }
                else if (currentState == GameState::Authenticated)
                {
                    if (playButton.isClicked(clickPos))
                    {
                        startMatchmaking();
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

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                if (keyPressed->code == sf::Keyboard::Key::Escape)
                {
                    if (!pendingMatchmaking)
                    {
                        returnToMenu();
                    }
                }
                else if (!pendingRequest && !pendingMatchmaking && currentState == GameState::Login)
                {
                    if (keyPressed->code == sf::Keyboard::Key::Tab)
                    {
                        focusLoginInput(focusedInput + (keyPressed->shift ? -1 : 1));
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Enter)
                    {
                        submitLogin();
                    }
                }
                else if (!pendingRequest && !pendingMatchmaking && currentState == GameState::CreateAccount)
                {
                    if (keyPressed->code == sf::Keyboard::Key::Tab)
                    {
                        focusCreateInput(focusedInput + (keyPressed->shift ? -1 : 1));
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Enter)
                    {
                        submitCreateAccount();
                    }
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
            loginSubmitButton.update(mousePos);
            backButton.update(mousePos);

            float deltaTime = clock.restart().asSeconds();
            usernameInput.updateCursor(deltaTime);
            passwordInput.updateCursor(deltaTime);
        }
        else if (currentState == GameState::CreateAccount)
        {
            createSubmitButton.update(mousePos);
            backButton.update(mousePos);

            float deltaTime = clock.restart().asSeconds();
            usernameInput.updateCursor(deltaTime);
            passwordInput.updateCursor(deltaTime);
            confirmInput.updateCursor(deltaTime);
        }
        else if (currentState == GameState::Authenticated)
        {
            playButton.update(mousePos);
        }
        else if (currentState == GameState::Matchmaking)
        {
        }
        else if (currentState == GameState::Game)
        {
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
        else if (currentState == GameState::Authenticated)
        {
            playButton.draw(window);
            window.draw(messageText);
        }
        else if (currentState == GameState::Matchmaking)
        {
            window.draw(messageText);
        }
        else if (currentState == GameState::Game)
        {
        }

        window.display();
    }

    return 0;
}
