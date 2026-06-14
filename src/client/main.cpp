#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include "../shared/card_data.hpp"
#include "../shared/deck_data.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

import button;
import inputbox;
import network;

namespace
{
constexpr unsigned short AccountServerPort = 55000;
constexpr unsigned short MatchmakingServerPort = 55001;
constexpr unsigned short CardServerPort = 55004;
constexpr const char* DefaultServerHost = "127.0.0.1";
constexpr const char* ClientConfigFileName = "client.cfg";

constexpr float DeckPanelX = 20.0f;
constexpr float CurrentDeckPanelX = 290.0f;
constexpr float LibraryPanelX = 560.0f;
constexpr float DeckEditorPanelY = 96.0f;
constexpr float DeckEditorPanelHeight = 400.0f;
constexpr float DeckListX = 34.0f;
constexpr float DeckListY = 184.0f;
constexpr float DeckListWidth = 222.0f;
constexpr float DeckRowHeight = 34.0f;
constexpr std::size_t VisibleDeckRows = 8;

constexpr float DeckCardsX = 304.0f;
constexpr float DeckCardsY = 224.0f;
constexpr float DeckCardsWidth = 222.0f;
constexpr float DeckCardRowHeight = 29.0f;
constexpr std::size_t VisibleDeckCardRows = 9;

constexpr float LibraryX = 574.0f;
constexpr float LibraryY = 168.0f;
constexpr float LibraryWidth = 192.0f;
constexpr float LibraryRowHeight = 31.0f;
constexpr std::size_t VisibleLibraryRows = 10;

enum class GameState
{
    Menu,
    Login,
    CreateAccount,
    Authenticated,
    Matchmaking,
    DeckEditor,
    Game
};

struct ServerEndpoint
{
    std::string host = DefaultServerHost;
    unsigned short port = 0;
};

struct ClientConfig
{
    ServerEndpoint account{DefaultServerHost, AccountServerPort};
    ServerEndpoint matchmaking{DefaultServerHost, MatchmakingServerPort};
    ServerEndpoint card{DefaultServerHost, CardServerPort};
    std::string gameServerHost = DefaultServerHost;
};

struct ServerResult
{
    bool success = false;
    std::string message;
    std::shared_ptr<sf::TcpSocket> gameSocket;
};

struct CardListResult
{
    bool success = false;
    std::string message;
    std::vector<card_data::Card> cards;
};

struct DeckListResult
{
    bool success = false;
    std::string message;
    std::vector<deck_data::Deck> decks;
};

struct DeckEditorLoadResult
{
    bool success = false;
    std::string message;
    std::vector<card_data::Card> cards;
    std::vector<deck_data::Deck> decks;
};

struct DeckCommandResult
{
    bool success = false;
    std::string message;
    std::string originalName;
    deck_data::Deck deck;
};

std::filesystem::path executableDirectory;

std::string trim(const std::string& value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();

    if (first >= last)
    {
        return "";
    }

    return std::string(first, last);
}

std::string lowerKey(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::optional<unsigned short> parsePort(const std::string& value)
{
    const std::string trimmed = trim(value);
    if (trimmed.empty())
    {
        return std::nullopt;
    }

    try
    {
        std::size_t parsed = 0;
        const unsigned long port = std::stoul(trimmed, &parsed);
        if (parsed != trimmed.size() || port == 0 || port > std::numeric_limits<unsigned short>::max())
        {
            return std::nullopt;
        }

        return static_cast<unsigned short>(port);
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

void applyServerValue(ServerEndpoint& endpoint, const std::string& value)
{
    const std::string server = trim(value);
    if (server.empty())
    {
        return;
    }

    if (server.front() == '[')
    {
        const std::size_t closeBracket = server.find(']');
        if (closeBracket != std::string::npos)
        {
            const std::string host = trim(server.substr(1, closeBracket - 1));
            if (!host.empty())
            {
                endpoint.host = host;
            }

            if (closeBracket + 1 < server.size() && server[closeBracket + 1] == ':')
            {
                if (const std::optional<unsigned short> port = parsePort(server.substr(closeBracket + 2)))
                {
                    endpoint.port = *port;
                }
            }
            return;
        }
    }

    const std::size_t delimiter = server.rfind(':');
    if (delimiter != std::string::npos && server.find(':') == delimiter)
    {
        const std::string host = trim(server.substr(0, delimiter));
        if (!host.empty())
        {
            endpoint.host = host;
        }

        if (const std::optional<unsigned short> port = parsePort(server.substr(delimiter + 1)))
        {
            endpoint.port = *port;
        }
        return;
    }

    endpoint.host = server;
}

void applyConfigEntry(ClientConfig& config, const std::string& key, const std::string& value)
{
    const std::string normalizedKey = lowerKey(trim(key));
    if (normalizedKey == "account_server" || normalizedKey == "accounts_server")
    {
        applyServerValue(config.account, value);
    }
    else if (normalizedKey == "account_server_host" || normalizedKey == "accounts_server_host")
    {
        const std::string host = trim(value);
        if (!host.empty())
        {
            config.account.host = host;
        }
    }
    else if (normalizedKey == "account_server_port" || normalizedKey == "accounts_server_port")
    {
        if (const std::optional<unsigned short> port = parsePort(value))
        {
            config.account.port = *port;
        }
    }
    else if (normalizedKey == "matchmaking_server")
    {
        applyServerValue(config.matchmaking, value);
    }
    else if (normalizedKey == "matchmaking_server_host")
    {
        const std::string host = trim(value);
        if (!host.empty())
        {
            config.matchmaking.host = host;
        }
    }
    else if (normalizedKey == "matchmaking_server_port")
    {
        if (const std::optional<unsigned short> port = parsePort(value))
        {
            config.matchmaking.port = *port;
        }
    }
    else if (normalizedKey == "card_server" || normalizedKey == "cardserver")
    {
        applyServerValue(config.card, value);
    }
    else if (normalizedKey == "card_server_host" || normalizedKey == "cardserver_host")
    {
        const std::string host = trim(value);
        if (!host.empty())
        {
            config.card.host = host;
        }
    }
    else if (normalizedKey == "card_server_port" || normalizedKey == "cardserver_port")
    {
        if (const std::optional<unsigned short> port = parsePort(value))
        {
            config.card.port = *port;
        }
    }
    else if (normalizedKey == "game_server_host")
    {
        const std::string host = trim(value);
        if (!host.empty())
        {
            config.gameServerHost = host;
        }
    }
}

std::optional<ClientConfig> loadClientConfigFrom(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    if (!stream)
    {
        return std::nullopt;
    }

    ClientConfig config;
    std::string line;
    while (std::getline(stream, line))
    {
        line = trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';')
        {
            continue;
        }

        const std::size_t delimiter = line.find('=');
        if (delimiter == std::string::npos)
        {
            continue;
        }

        applyConfigEntry(config, line.substr(0, delimiter), line.substr(delimiter + 1));
    }

    return config;
}

ClientConfig loadClientConfig()
{
    if (const std::optional<ClientConfig> config = loadClientConfigFrom(ClientConfigFileName))
    {
        return *config;
    }

    if (!executableDirectory.empty())
    {
        if (const std::optional<ClientConfig> config = loadClientConfigFrom(executableDirectory / ClientConfigFileName))
        {
            return *config;
        }
    }

    return {};
}

const ClientConfig& clientConfig()
{
    static const ClientConfig config = loadClientConfig();
    return config;
}

void setExecutableDirectory(const char* executablePath)
{
    if (executablePath == nullptr)
    {
        return;
    }

    const std::filesystem::path path(executablePath);
    if (path.has_parent_path())
    {
        executableDirectory = path.parent_path();
    }
}

std::string endpointText(const ServerEndpoint& endpoint)
{
    return endpoint.host + ":" + std::to_string(endpoint.port);
}

bool connectToHostPort(sf::TcpSocket& socket, const std::string& host, unsigned short port)
{
    const std::optional<sf::IpAddress> address = sf::IpAddress::resolve(host);
    if (!address)
    {
        return false;
    }

    return socket.connect(*address, port) == sf::Socket::Status::Done;
}

bool connectToEndpoint(sf::TcpSocket& socket, const ServerEndpoint& endpoint)
{
    return connectToHostPort(socket, endpoint.host, endpoint.port);
}

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

void setMessageY(sf::Text& text, float y)
{
    text.setPosition({text.getPosition().x, y});
    centerText(text, 400.0f);
}

std::string elideToWidth(sf::Font& font, const std::string& value, unsigned int size, float maxWidth)
{
    sf::Text text(font, value, size);
    if (text.getLocalBounds().size.x <= maxWidth)
    {
        return value;
    }

    std::string display = value;
    while (!display.empty())
    {
        display.pop_back();
        text.setString(display + "...");
        if (text.getLocalBounds().size.x <= maxWidth)
        {
            return display + "...";
        }
    }

    return "...";
}

void drawText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    float maxWidth = 0.0f)
{
    sf::Text text(font, maxWidth > 0.0f ? elideToWidth(font, value, size, maxWidth) : value, size);
    text.setFillColor(color);
    text.setPosition(position);
    window.draw(text);
}

void drawPanel(sf::RenderWindow& window, sf::Vector2f position, sf::Vector2f size)
{
    sf::RectangleShape panel(size);
    panel.setPosition(position);
    panel.setFillColor(sf::Color(38, 42, 52));
    panel.setOutlineThickness(1.0f);
    panel.setOutlineColor(sf::Color(82, 90, 108));
    window.draw(panel);
}

void drawRow(
    sf::RenderWindow& window,
    sf::Font& font,
    sf::Vector2f position,
    sf::Vector2f size,
    const std::string& primary,
    const std::string& secondary,
    bool selected)
{
    sf::RectangleShape row(size);
    row.setPosition(position);
    row.setFillColor(selected ? sf::Color(54, 86, 92) : sf::Color(50, 55, 68));
    row.setOutlineThickness(1.0f);
    row.setOutlineColor(selected ? sf::Color(103, 198, 184) : sf::Color(68, 76, 92));
    window.draw(row);

    drawText(window, font, primary, 16, {position.x + 8.0f, position.y + 5.0f}, sf::Color::White, size.x - 16.0f);
    if (!secondary.empty())
    {
        drawText(window, font, secondary, 12, {position.x + 8.0f, position.y + 22.0f}, sf::Color(176, 184, 198), size.x - 16.0f);
    }
}

std::optional<std::size_t> rowIndexAt(
    sf::Vector2f mouse,
    float x,
    float y,
    float width,
    float rowHeight,
    std::size_t visibleRows,
    std::size_t offset,
    std::size_t totalRows)
{
    if (mouse.x < x || mouse.x > x + width || mouse.y < y)
    {
        return std::nullopt;
    }

    const std::size_t visibleIndex = static_cast<std::size_t>((mouse.y - y) / rowHeight);
    const std::size_t index = offset + visibleIndex;
    if (visibleIndex < visibleRows && index < totalRows)
    {
        return index;
    }

    return std::nullopt;
}

bool isInsideRect(sf::Vector2f mouse, float x, float y, float width, float height)
{
    return mouse.x >= x && mouse.x <= x + width && mouse.y >= y && mouse.y <= y + height;
}

void scrollList(std::size_t& offset, std::size_t totalRows, std::size_t visibleRows, float delta)
{
    if (delta < 0.0f)
    {
        if (offset + visibleRows < totalRows)
        {
            ++offset;
        }
    }
    else if (offset > 0)
    {
        --offset;
    }
}

void clampListOffset(std::size_t& offset, std::size_t totalRows, std::size_t visibleRows)
{
    if (totalRows <= visibleRows)
    {
        offset = 0;
        return;
    }

    offset = std::min(offset, totalRows - visibleRows);
}

void sendDisconnect(sf::TcpSocket& socket)
{
    sf::Packet disconnectPacket;
    disconnectPacket << static_cast<std::uint8_t>(network::MessageType::Disconnect);
    [[maybe_unused]] auto disconnectResult = socket.send(disconnectPacket);
    socket.disconnect();
}

ServerResult sendAccountRequest(
    network::MessageType requestType,
    network::MessageType expectedResponseType,
    const std::string& username,
    const std::string& password)
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet packet;
    packet << static_cast<uint8_t>(requestType);
    packet << username;
    packet << password;

    if (socket.send(packet) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send to account server"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No response from account server"};
    }

    uint8_t responseType = 0;
    bool success = false;
    std::string message;
    response >> responseType >> success >> message;

    if (static_cast<network::MessageType>(responseType) != expectedResponseType)
    {
        socket.disconnect();
        return {false, "Unexpected account server response"};
    }

    sendDisconnect(socket);
    return {success, message};
}

CardListResult fetchCards()
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().card))
    {
        return {false, "Failed to connect to card server " + endpointText(clientConfig().card)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::CardListRequest);
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send card list request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No response from card server"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    std::uint32_t count = 0;
    response >> responseType >> success >> message >> count;
    if (!response || static_cast<network::MessageType>(responseType) != network::MessageType::CardListResponse)
    {
        socket.disconnect();
        return {false, "Unexpected card list response"};
    }

    std::vector<card_data::Card> cards;
    cards.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        card_data::Card card;
        if (!card_data::readCard(response, card))
        {
            socket.disconnect();
            return {false, "Invalid card list payload"};
        }
        cards.push_back(card);
    }

    socket.disconnect();
    return {success, message, cards};
}

DeckListResult fetchDecks(const std::string& username)
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account)};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::DeckListRequest);
    request << username;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send deck list request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "No deck list response from account server"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    std::uint32_t count = 0;
    response >> responseType >> success >> message >> count;
    if (!response || static_cast<network::MessageType>(responseType) != network::MessageType::DeckListResponse)
    {
        socket.disconnect();
        return {false, "Unexpected deck list response"};
    }

    std::vector<deck_data::Deck> decks;
    decks.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        deck_data::Deck deck;
        if (!deck_data::readDeck(response, deck))
        {
            socket.disconnect();
            return {false, "Invalid deck list payload"};
        }
        decks.push_back(deck);
    }

    sendDisconnect(socket);
    return {success, message, decks};
}

DeckCommandResult readDeckCommandResponse(
    sf::TcpSocket& socket,
    network::MessageType expectedResponseType,
    const std::string& fallbackMessage,
    const std::string& originalName,
    const deck_data::Deck& deck)
{
    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        return {false, fallbackMessage, originalName, deck};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    response >> responseType >> success >> message;
    if (!response || static_cast<network::MessageType>(responseType) != expectedResponseType)
    {
        return {false, "Unexpected deck command response", originalName, deck};
    }

    return {success, message, originalName, deck};
}

DeckCommandResult saveDeckToAccount(
    const std::string& username,
    const std::string& originalName,
    const deck_data::Deck& deck)
{
    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account), originalName, deck};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::DeckSaveRequest);
    request << username << originalName;
    deck_data::writeDeck(request, deck);
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send deck save request", originalName, deck};
    }

    DeckCommandResult result = readDeckCommandResponse(
        socket,
        network::MessageType::DeckSaveResponse,
        "No deck save response from account server",
        originalName,
        deck);
    sendDisconnect(socket);
    return result;
}

DeckCommandResult deleteDeckFromAccount(const std::string& username, const std::string& deckName)
{
    deck_data::Deck deck;
    deck.name = deckName;

    sf::TcpSocket socket;
    if (!connectToEndpoint(socket, clientConfig().account))
    {
        return {false, "Failed to connect to account server " + endpointText(clientConfig().account), deckName, deck};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::DeckDeleteRequest);
    request << username << deckName;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        socket.disconnect();
        return {false, "Failed to send deck delete request", deckName, deck};
    }

    DeckCommandResult result = readDeckCommandResponse(
        socket,
        network::MessageType::DeckDeleteResponse,
        "No deck delete response from account server",
        deckName,
        deck);
    sendDisconnect(socket);
    return result;
}

DeckEditorLoadResult loadDeckEditorData(const std::string& username)
{
    CardListResult cardResult = fetchCards();
    if (!cardResult.success)
    {
        return {false, cardResult.message};
    }

    DeckListResult deckResult = fetchDecks(username);
    if (!deckResult.success)
    {
        return {false, deckResult.message, std::move(cardResult.cards)};
    }

    const std::string message =
        "Loaded " + std::to_string(cardResult.cards.size()) + " cards and " +
        std::to_string(deckResult.decks.size()) + " decks";
    return {true, message, std::move(cardResult.cards), std::move(deckResult.decks)};
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
        if (connectToHostPort(*socket, clientConfig().gameServerHost, gamePort))
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
    if (!connectToEndpoint(socket, clientConfig().matchmaking))
    {
        return {false, "Failed to connect to matchmaking " + endpointText(clientConfig().matchmaking)};
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

int main(int argc, char** argv)
{
    setExecutableDirectory(argc > 0 ? argv[0] : nullptr);

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
    InputBox deckNameInput({304.0f, 154.0f}, {222.0f, 40.0f}, "Deck Name", font);

    Button loginSubmitButton({300.0f, 300.0f}, {200.0f, 50.0f}, "Login", font);
    Button createSubmitButton({300.0f, 380.0f}, {200.0f, 50.0f}, "Create Account", font);
    Button backButton({20.0f, 520.0f}, {120.0f, 45.0f}, "Back", font);
    Button playButton({300.0f, 220.0f}, {200.0f, 60.0f}, "Play", font);
    Button deckEditorButton({300.0f, 300.0f}, {200.0f, 60.0f}, "Deck Editor", font);

    Button deckBackButton({664.0f, 22.0f}, {112.0f, 38.0f}, "Back", font);
    Button newDeckButton({34.0f, 132.0f}, {102.0f, 38.0f}, "New", font);
    Button refreshDeckButton({146.0f, 132.0f}, {110.0f, 38.0f}, "Refresh", font);
    Button deleteDeckButton({34.0f, 508.0f}, {110.0f, 38.0f}, "Delete", font);
    Button removeCardButton({304.0f, 508.0f}, {110.0f, 38.0f}, "Remove", font);
    Button addCardButton({574.0f, 508.0f}, {88.0f, 38.0f}, "Add", font);
    Button saveDeckButton({668.0f, 508.0f}, {108.0f, 38.0f}, "Save", font);

    sf::Text messageText(font, "", 20);
    messageText.setFillColor(sf::Color::Red);
    messageText.setPosition({400.0f, 450.0f});

    sf::Clock clock;
    GameState currentState = GameState::Menu;
    std::optional<std::future<ServerResult>> pendingRequest;
    std::optional<std::future<ServerResult>> pendingMatchmaking;
    std::optional<std::future<DeckEditorLoadResult>> pendingDeckEditorLoad;
    std::optional<std::future<DeckCommandResult>> pendingDeckSave;
    std::optional<std::future<DeckCommandResult>> pendingDeckDelete;
    std::shared_ptr<sf::TcpSocket> activeGameSocket;
    std::string loggedInUsername;
    std::vector<card_data::Card> cardLibrary;
    std::vector<deck_data::Deck> playerDecks;
    deck_data::Deck editingDeck;
    std::string activeDeckOriginalName;
    std::optional<std::size_t> selectedDeck;
    std::optional<std::size_t> selectedDeckCard;
    std::optional<std::size_t> selectedLibraryCard;
    std::optional<std::size_t> draggingLibraryCard;
    sf::Vector2f dragStartPos;
    sf::Vector2f dragCurrentPos;
    bool dragActive = false;
    std::size_t deckListOffset = 0;
    std::size_t deckCardListOffset = 0;
    std::size_t libraryOffset = 0;
    int focusedInput = 0;

    auto clearFocus = [&]() {
        usernameInput.setActive(false);
        passwordInput.setActive(false);
        confirmInput.setActive(false);
        deckNameInput.setActive(false);
    };

    auto focusLoginInput = [&](int index) {
        focusedInput = (index + 2) % 2;
        usernameInput.setActive(focusedInput == 0);
        passwordInput.setActive(focusedInput == 1);
        confirmInput.setActive(false);
        deckNameInput.setActive(false);
    };

    auto focusCreateInput = [&](int index) {
        focusedInput = (index + 3) % 3;
        usernameInput.setActive(focusedInput == 0);
        passwordInput.setActive(focusedInput == 1);
        confirmInput.setActive(focusedInput == 2);
        deckNameInput.setActive(false);
    };

    auto sortDecks = [&]() {
        std::sort(playerDecks.begin(), playerDecks.end(), [](const deck_data::Deck& left, const deck_data::Deck& right) {
            return lowerKey(left.name) < lowerKey(right.name);
        });
    };

    auto makeNewDeckName = [&]() {
        std::string name = "New Deck";
        int suffix = 2;
        auto exists = [&playerDecks](const std::string& candidate) {
            return std::any_of(playerDecks.begin(), playerDecks.end(), [&](const deck_data::Deck& deck) {
                return deck.name == candidate;
            });
        };

        while (exists(name))
        {
            name = "New Deck " + std::to_string(suffix++);
        }
        return name;
    };

    auto selectDeck = [&](std::size_t index) {
        if (index >= playerDecks.size())
        {
            return;
        }

        selectedDeck = index;
        editingDeck = playerDecks[index];
        activeDeckOriginalName = editingDeck.name;
        deckNameInput.setContent(editingDeck.name);
        selectedDeckCard.reset();
        deckCardListOffset = 0;
        clampListOffset(deckListOffset, playerDecks.size(), VisibleDeckRows);
        clearFocus();
    };

    auto selectDeckByName = [&](const std::string& deckName) {
        const auto found = std::find_if(playerDecks.begin(), playerDecks.end(), [&](const deck_data::Deck& deck) {
            return deck.name == deckName;
        });
        if (found != playerDecks.end())
        {
            selectDeck(static_cast<std::size_t>(std::distance(playerDecks.begin(), found)));
        }
    };

    auto createNewDeck = [&]() {
        selectedDeck.reset();
        selectedDeckCard.reset();
        activeDeckOriginalName.clear();
        editingDeck = {makeNewDeckName(), {}};
        deckNameInput.setContent(editingDeck.name);
        deckNameInput.setActive(true);
        usernameInput.setActive(false);
        passwordInput.setActive(false);
        confirmInput.setActive(false);
        deckCardListOffset = 0;
    };

    auto startRequest = [&](network::MessageType requestType, network::MessageType expectedResponseType) {
        setMessageY(messageText, 450.0f);
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
        loggedInUsername.clear();
        cardLibrary.clear();
        playerDecks.clear();
        editingDeck = {};
        activeDeckOriginalName.clear();
        selectedDeck.reset();
        selectedDeckCard.reset();
        selectedLibraryCard.reset();
        draggingLibraryCard.reset();
        dragActive = false;
        title.setString("Main Menu");
        centerText(title, 400.0f);
        setMessageY(messageText, 450.0f);
        resetForm(usernameInput, passwordInput, confirmInput, messageText);
        deckNameInput.clear();
        draggingLibraryCard.reset();
        dragActive = false;
        clearFocus();
    };

    auto showAuthenticatedScreen = [&]() {
        currentState = GameState::Authenticated;
        title.setString("Logged In");
        centerText(title, 400.0f);
        setMessageY(messageText, 420.0f);
        resetForm(usernameInput, passwordInput, confirmInput, messageText);
        deckNameInput.clear();
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
        setMessageY(messageText, 450.0f);
        setMessage(messageText, "Finding match...", sf::Color::Yellow);
        pendingMatchmaking = std::async(std::launch::async, joinMatchmaking);
    };

    auto loadDeckEditor = [&]() {
        currentState = GameState::DeckEditor;
        title.setString("");
        centerText(title, 400.0f);
        setMessageY(messageText, 558.0f);
        setMessage(messageText, "Loading deck editor...", sf::Color::Yellow);
        clearFocus();
        cardLibrary.clear();
        playerDecks.clear();
        editingDeck = {};
        activeDeckOriginalName.clear();
        selectedDeck.reset();
        selectedDeckCard.reset();
        selectedLibraryCard.reset();
        draggingLibraryCard.reset();
        dragActive = false;
        deckListOffset = 0;
        deckCardListOffset = 0;
        libraryOffset = 0;
        deckNameInput.clear();
        pendingDeckEditorLoad = std::async(std::launch::async, loadDeckEditorData, loggedInUsername);
    };

    auto deckEditorBusy = [&]() {
        return pendingDeckEditorLoad.has_value() || pendingDeckSave.has_value() || pendingDeckDelete.has_value();
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

    auto saveCurrentDeck = [&]() {
        if (deckEditorBusy())
        {
            return;
        }

        deck_data::Deck deck = editingDeck;
        deck.name = trim(deckNameInput.getContent());
        if (deck.name.empty())
        {
            setMessage(messageText, "Deck name cannot be empty", sf::Color::Red);
            return;
        }

        setMessage(messageText, "Saving deck...", sf::Color::Yellow);
        pendingDeckSave = std::async(std::launch::async, saveDeckToAccount, loggedInUsername, activeDeckOriginalName, deck);
    };

    auto deleteCurrentDeck = [&]() {
        if (deckEditorBusy())
        {
            return;
        }

        if (activeDeckOriginalName.empty())
        {
            setMessage(messageText, "Select a saved deck to delete", sf::Color::Red);
            return;
        }

        setMessage(messageText, "Deleting deck...", sf::Color::Yellow);
        pendingDeckDelete = std::async(std::launch::async, deleteDeckFromAccount, loggedInUsername, activeDeckOriginalName);
    };

    auto addLibraryCardToDeck = [&](std::size_t libraryIndex, const std::string& message) {
        if (libraryIndex >= cardLibrary.size())
        {
            return;
        }

        editingDeck.cardTitles.push_back(cardLibrary[libraryIndex].title);
        selectedDeckCard = editingDeck.cardTitles.size() - 1;
        clampListOffset(deckCardListOffset, editingDeck.cardTitles.size(), VisibleDeckCardRows);
        if (*selectedDeckCard >= deckCardListOffset + VisibleDeckCardRows)
        {
            deckCardListOffset = *selectedDeckCard - VisibleDeckCardRows + 1;
        }
        setMessage(messageText, message, sf::Color::Yellow);
    };

    auto addSelectedCard = [&]() {
        if (!selectedLibraryCard || *selectedLibraryCard >= cardLibrary.size())
        {
            setMessage(messageText, "Select a card from the library first", sf::Color::Red);
            return;
        }

        addLibraryCardToDeck(*selectedLibraryCard, "Card added. Save to keep changes.");
    };

    auto removeSelectedCard = [&]() {
        if (!selectedDeckCard || *selectedDeckCard >= editingDeck.cardTitles.size())
        {
            setMessage(messageText, "Select a card in the deck first", sf::Color::Red);
            return;
        }

        editingDeck.cardTitles.erase(editingDeck.cardTitles.begin() + static_cast<std::ptrdiff_t>(*selectedDeckCard));
        if (editingDeck.cardTitles.empty())
        {
            selectedDeckCard.reset();
        }
        else if (*selectedDeckCard >= editingDeck.cardTitles.size())
        {
            selectedDeckCard = editingDeck.cardTitles.size() - 1;
        }
        clampListOffset(deckCardListOffset, editingDeck.cardTitles.size(), VisibleDeckCardRows);
        setMessage(messageText, "Card removed. Save to keep changes.", sf::Color::Yellow);
    };

    auto drawDeckEditor = [&]() {
        drawText(window, font, "Deck Editor", 30, {24.0f, 18.0f}, sf::Color::White);
        drawText(window, font, "Signed in as " + loggedInUsername, 14, {270.0f, 22.0f}, sf::Color(178, 186, 202), 360.0f);
        drawText(window, font, "Card server " + endpointText(clientConfig().card), 13, {270.0f, 45.0f}, sf::Color(148, 158, 176), 360.0f);
        deckBackButton.draw(window);

        drawPanel(window, {DeckPanelX, DeckEditorPanelY}, {250.0f, DeckEditorPanelHeight});
        drawText(window, font, "Decks", 22, {34.0f, 107.0f}, sf::Color::White);
        newDeckButton.draw(window);
        refreshDeckButton.draw(window);

        const std::size_t lastDeck = std::min(playerDecks.size(), deckListOffset + VisibleDeckRows);
        for (std::size_t i = deckListOffset; i < lastDeck; ++i)
        {
            const float y = DeckListY + static_cast<float>(i - deckListOffset) * DeckRowHeight;
            drawRow(
                window,
                font,
                {DeckListX, y},
                {DeckListWidth, DeckRowHeight - 4.0f},
                playerDecks[i].name,
                std::to_string(playerDecks[i].cardTitles.size()) + " cards",
                selectedDeck && *selectedDeck == i);
        }
        if (playerDecks.empty() && !deckEditorBusy())
        {
            drawText(window, font, "No saved decks", 16, {56.0f, 296.0f}, sf::Color(178, 186, 202));
        }
        deleteDeckButton.draw(window);

        drawPanel(window, {CurrentDeckPanelX, DeckEditorPanelY}, {250.0f, DeckEditorPanelHeight});
        drawText(window, font, "Current Deck", 22, {304.0f, 107.0f}, sf::Color::White);
        deckNameInput.draw(window);
        drawText(
            window,
            font,
            std::to_string(editingDeck.cardTitles.size()) + " selected cards",
            14,
            {304.0f, 204.0f},
            sf::Color(178, 186, 202));

        const std::size_t lastDeckCard = std::min(editingDeck.cardTitles.size(), deckCardListOffset + VisibleDeckCardRows);
        for (std::size_t i = deckCardListOffset; i < lastDeckCard; ++i)
        {
            const float y = DeckCardsY + static_cast<float>(i - deckCardListOffset) * DeckCardRowHeight;
            drawRow(
                window,
                font,
                {DeckCardsX, y},
                {DeckCardsWidth, DeckCardRowHeight - 4.0f},
                editingDeck.cardTitles[i],
                "",
                selectedDeckCard && *selectedDeckCard == i);
        }
        if (editingDeck.cardTitles.empty() && !deckEditorBusy())
        {
            drawText(window, font, "No cards in this deck", 15, {328.0f, 320.0f}, sf::Color(178, 186, 202), 180.0f);
        }
        removeCardButton.draw(window);

        drawPanel(window, {LibraryPanelX, DeckEditorPanelY}, {220.0f, DeckEditorPanelHeight});
        drawText(window, font, "Card Library", 22, {574.0f, 107.0f}, sf::Color::White);
        drawText(
            window,
            font,
            std::to_string(cardLibrary.size()) + " available cards",
            14,
            {574.0f, 138.0f},
            sf::Color(178, 186, 202));

        const std::size_t lastCard = std::min(cardLibrary.size(), libraryOffset + VisibleLibraryRows);
        for (std::size_t i = libraryOffset; i < lastCard; ++i)
        {
            const float y = LibraryY + static_cast<float>(i - libraryOffset) * LibraryRowHeight;
            drawRow(
                window,
                font,
                {LibraryX, y},
                {LibraryWidth, LibraryRowHeight - 4.0f},
                cardLibrary[i].title,
                cardLibrary[i].type,
                selectedLibraryCard && *selectedLibraryCard == i);
        }
        if (cardLibrary.empty() && !deckEditorBusy())
        {
            drawText(window, font, "No cards returned", 16, {592.0f, 296.0f}, sf::Color(178, 186, 202));
        }
        addCardButton.draw(window);
        saveDeckButton.draw(window);

        const bool hoveringDropTarget = dragActive && draggingLibraryCard &&
            isInsideRect(dragCurrentPos, CurrentDeckPanelX, DeckEditorPanelY, 250.0f, DeckEditorPanelHeight);
        if (hoveringDropTarget)
        {
            sf::RectangleShape dropTarget({250.0f, DeckEditorPanelHeight});
            dropTarget.setPosition({CurrentDeckPanelX, DeckEditorPanelY});
            dropTarget.setFillColor(sf::Color(80, 140, 130, 45));
            dropTarget.setOutlineThickness(3.0f);
            dropTarget.setOutlineColor(sf::Color(103, 198, 184));
            window.draw(dropTarget);
        }

        if (dragActive && draggingLibraryCard && *draggingLibraryCard < cardLibrary.size())
        {
            const sf::Vector2f ghostPosition{dragCurrentPos.x - 96.0f, dragCurrentPos.y - 15.0f};
            sf::RectangleShape ghost({192.0f, 30.0f});
            ghost.setPosition(ghostPosition);
            ghost.setFillColor(sf::Color(60, 88, 102, 220));
            ghost.setOutlineThickness(1.0f);
            ghost.setOutlineColor(sf::Color(130, 220, 205));
            window.draw(ghost);
            drawText(
                window,
                font,
                cardLibrary[*draggingLibraryCard].title,
                15,
                {ghostPosition.x + 10.0f, ghostPosition.y + 6.0f},
                sf::Color::White,
                172.0f);
        }

        window.draw(messageText);
    };

    while (window.isOpen())
    {
        const float deltaTime = clock.restart().asSeconds();
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        if (pendingRequest &&
            pendingRequest->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            ServerResult result = pendingRequest->get();
            pendingRequest.reset();
            if (result.success)
            {
                loggedInUsername = usernameInput.getContent();
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
                title.setString("Logged In");
                centerText(title, 400.0f);
                setMessage(messageText, result.message, sf::Color::Red);
            }
        }

        if (pendingDeckEditorLoad &&
            pendingDeckEditorLoad->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            DeckEditorLoadResult result = pendingDeckEditorLoad->get();
            pendingDeckEditorLoad.reset();
            if (result.success)
            {
                cardLibrary = std::move(result.cards);
                playerDecks = std::move(result.decks);
                sortDecks();
                selectedLibraryCard = cardLibrary.empty() ? std::nullopt : std::optional<std::size_t>(0);
                if (!playerDecks.empty())
                {
                    selectDeck(0);
                }
                else
                {
                    createNewDeck();
                    deckNameInput.setActive(false);
                }
                setMessage(messageText, result.message, sf::Color(120, 220, 150));
            }
            else
            {
                cardLibrary = std::move(result.cards);
                playerDecks.clear();
                createNewDeck();
                deckNameInput.setActive(false);
                setMessage(messageText, result.message, sf::Color::Red);
            }
        }

        if (pendingDeckSave &&
            pendingDeckSave->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            DeckCommandResult result = pendingDeckSave->get();
            pendingDeckSave.reset();
            if (result.success)
            {
                const auto existing = std::find_if(playerDecks.begin(), playerDecks.end(), [&](const deck_data::Deck& deck) {
                    return (!result.originalName.empty() && deck.name == result.originalName) || deck.name == result.deck.name;
                });

                if (existing != playerDecks.end())
                {
                    *existing = result.deck;
                }
                else
                {
                    playerDecks.push_back(result.deck);
                }

                sortDecks();
                selectDeckByName(result.deck.name);
                setMessage(messageText, result.message, sf::Color(120, 220, 150));
            }
            else
            {
                setMessage(messageText, result.message, sf::Color::Red);
            }
        }

        if (pendingDeckDelete &&
            pendingDeckDelete->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            DeckCommandResult result = pendingDeckDelete->get();
            pendingDeckDelete.reset();
            if (result.success)
            {
                playerDecks.erase(
                    std::remove_if(playerDecks.begin(), playerDecks.end(), [&](const deck_data::Deck& deck) {
                        return deck.name == result.originalName;
                    }),
                    playerDecks.end());
                if (!playerDecks.empty())
                {
                    selectDeck(0);
                }
                else
                {
                    createNewDeck();
                    deckNameInput.setActive(false);
                }
                setMessage(messageText, result.message, sf::Color(120, 220, 150));
            }
            else
            {
                setMessage(messageText, result.message, sf::Color::Red);
            }
        }

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>();
                mousePressed && mousePressed->button == sf::Mouse::Button::Left && !pendingRequest && !pendingMatchmaking)
            {
                sf::Vector2f clickPos = window.mapPixelToCoords(mousePressed->position);
                if (currentState == GameState::Menu)
                {
                    if (loginButton.isClicked(clickPos))
                    {
                        currentState = GameState::Login;
                        title.setString("Login");
                        centerText(title, 400.0f);
                        setMessageY(messageText, 450.0f);
                        resetForm(usernameInput, passwordInput, confirmInput, messageText);
                        focusLoginInput(0);
                    }
                    else if (createButton.isClicked(clickPos))
                    {
                        currentState = GameState::CreateAccount;
                        title.setString("Create Account");
                        centerText(title, 400.0f);
                        setMessageY(messageText, 450.0f);
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
                    else if (deckEditorButton.isClicked(clickPos))
                    {
                        loadDeckEditor();
                    }
                }
                else if (currentState == GameState::DeckEditor)
                {
                    draggingLibraryCard.reset();
                    dragActive = false;

                    if (deckBackButton.isClicked(clickPos) && !deckEditorBusy())
                    {
                        showAuthenticatedScreen();
                    }
                    else if (!deckEditorBusy())
                    {
                        if (newDeckButton.isClicked(clickPos))
                        {
                            createNewDeck();
                            setMessage(messageText, "Editing a new deck. Save to store it.", sf::Color::Yellow);
                        }
                        else if (refreshDeckButton.isClicked(clickPos))
                        {
                            loadDeckEditor();
                        }
                        else if (deleteDeckButton.isClicked(clickPos))
                        {
                            deleteCurrentDeck();
                        }
                        else if (removeCardButton.isClicked(clickPos))
                        {
                            removeSelectedCard();
                        }
                        else if (addCardButton.isClicked(clickPos))
                        {
                            addSelectedCard();
                        }
                        else if (saveDeckButton.isClicked(clickPos))
                        {
                            saveCurrentDeck();
                        }
                        else if (deckNameInput.contains(clickPos))
                        {
                            clearFocus();
                            deckNameInput.setActive(true);
                        }
                        else if (const std::optional<std::size_t> deckIndex = rowIndexAt(
                                     clickPos,
                                     DeckListX,
                                     DeckListY,
                                     DeckListWidth,
                                     DeckRowHeight,
                                     VisibleDeckRows,
                                     deckListOffset,
                                     playerDecks.size()))
                        {
                            selectDeck(*deckIndex);
                        }
                        else if (const std::optional<std::size_t> cardIndex = rowIndexAt(
                                     clickPos,
                                     DeckCardsX,
                                     DeckCardsY,
                                     DeckCardsWidth,
                                     DeckCardRowHeight,
                                     VisibleDeckCardRows,
                                     deckCardListOffset,
                                     editingDeck.cardTitles.size()))
                        {
                            clearFocus();
                            selectedDeckCard = *cardIndex;
                        }
                        else if (const std::optional<std::size_t> libraryIndex = rowIndexAt(
                                     clickPos,
                                     LibraryX,
                                     LibraryY,
                                     LibraryWidth,
                                     LibraryRowHeight,
                                     VisibleLibraryRows,
                                     libraryOffset,
                                     cardLibrary.size()))
                        {
                            clearFocus();
                            selectedLibraryCard = *libraryIndex;
                            draggingLibraryCard = *libraryIndex;
                            dragStartPos = clickPos;
                            dragCurrentPos = clickPos;
                            dragActive = false;
                        }
                        else
                        {
                            clearFocus();
                        }
                    }
                }
            }

            if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>();
                mouseMoved && currentState == GameState::DeckEditor && draggingLibraryCard)
            {
                dragCurrentPos = window.mapPixelToCoords(mouseMoved->position);
                const sf::Vector2f delta = dragCurrentPos - dragStartPos;
                if (delta.x * delta.x + delta.y * delta.y > 16.0f)
                {
                    dragActive = true;
                }
            }

            if (const auto* mouseReleased = event->getIf<sf::Event::MouseButtonReleased>();
                mouseReleased && mouseReleased->button == sf::Mouse::Button::Left && currentState == GameState::DeckEditor)
            {
                const sf::Vector2f releasePos = window.mapPixelToCoords(mouseReleased->position);
                if (draggingLibraryCard && dragActive &&
                    isInsideRect(releasePos, CurrentDeckPanelX, DeckEditorPanelY, 250.0f, DeckEditorPanelHeight))
                {
                    addLibraryCardToDeck(*draggingLibraryCard, "Card dropped into deck. Save to keep changes.");
                }

                draggingLibraryCard.reset();
                dragActive = false;
            }

            if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>(); wheel && currentState == GameState::DeckEditor)
            {
                const sf::Vector2f wheelPos = window.mapPixelToCoords(wheel->position);
                if (isInsideRect(wheelPos, DeckListX, DeckListY, DeckListWidth, DeckRowHeight * VisibleDeckRows))
                {
                    scrollList(deckListOffset, playerDecks.size(), VisibleDeckRows, wheel->delta);
                }
                else if (isInsideRect(wheelPos, DeckCardsX, DeckCardsY, DeckCardsWidth, DeckCardRowHeight * VisibleDeckCardRows))
                {
                    scrollList(deckCardListOffset, editingDeck.cardTitles.size(), VisibleDeckCardRows, wheel->delta);
                }
                else if (isInsideRect(wheelPos, LibraryX, LibraryY, LibraryWidth, LibraryRowHeight * VisibleLibraryRows))
                {
                    scrollList(libraryOffset, cardLibrary.size(), VisibleLibraryRows, wheel->delta);
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

            if (currentState == GameState::DeckEditor && !deckEditorBusy())
            {
                deckNameInput.handleEvent(*event);
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                if (keyPressed->code == sf::Keyboard::Key::Escape)
                {
                    if (currentState == GameState::DeckEditor && !deckEditorBusy())
                    {
                        showAuthenticatedScreen();
                    }
                    else if (!pendingRequest && !pendingMatchmaking)
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
                else if (currentState == GameState::DeckEditor && !deckEditorBusy())
                {
                    if (keyPressed->code == sf::Keyboard::Key::Enter)
                    {
                        saveCurrentDeck();
                    }
                    else if (keyPressed->code == sf::Keyboard::Key::Delete)
                    {
                        removeSelectedCard();
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
            usernameInput.updateCursor(deltaTime);
            passwordInput.updateCursor(deltaTime);
        }
        else if (currentState == GameState::CreateAccount)
        {
            createSubmitButton.update(mousePos);
            backButton.update(mousePos);
            usernameInput.updateCursor(deltaTime);
            passwordInput.updateCursor(deltaTime);
            confirmInput.updateCursor(deltaTime);
        }
        else if (currentState == GameState::Authenticated)
        {
            playButton.update(mousePos);
            deckEditorButton.update(mousePos);
        }
        else if (currentState == GameState::Matchmaking)
        {
        }
        else if (currentState == GameState::DeckEditor)
        {
            deckBackButton.update(mousePos);
            newDeckButton.update(mousePos);
            refreshDeckButton.update(mousePos);
            deleteDeckButton.update(mousePos);
            removeCardButton.update(mousePos);
            addCardButton.update(mousePos);
            saveDeckButton.update(mousePos);
            deckNameInput.updateCursor(deltaTime);
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
            drawText(window, font, "Signed in as " + loggedInUsername, 18, {300.0f, 160.0f}, sf::Color(190, 198, 214), 260.0f);
            playButton.draw(window);
            deckEditorButton.draw(window);
            window.draw(messageText);
        }
        else if (currentState == GameState::Matchmaking)
        {
            window.draw(messageText);
        }
        else if (currentState == GameState::DeckEditor)
        {
            drawDeckEditor();
        }
        else if (currentState == GameState::Game)
        {
        }

        window.display();
    }

    return 0;
}
