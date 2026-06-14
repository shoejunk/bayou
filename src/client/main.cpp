#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include "../shared/card_data.hpp"
#include "../shared/deck_data.hpp"
#include "../shared/game_data.hpp"

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
#include <unordered_map>
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
    DeckSelect,
    Matchmaking,
    DeckEditor,
    Game
};

// In-game board layout.
constexpr float BoardOriginX = 24.0f;
constexpr float BoardOriginY = 70.0f;
constexpr float CellSize = 54.0f;
constexpr float InfoPanelX = 472.0f;
constexpr float InfoPanelY = 70.0f;
constexpr float InfoPanelWidth = 304.0f;
constexpr float InfoPanelHeight = 432.0f;
constexpr float HandY = 512.0f;
constexpr float HandCardWidth = 88.0f;
constexpr float HandCardHeight = 78.0f;
constexpr float HandGap = 6.0f;
constexpr float HandStartX = 28.0f;

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

std::string assetRelativePath(const std::string& value)
{
    const std::string trimmed = trim(value);
    if (trimmed.empty())
    {
        return "";
    }

    std::filesystem::path path(trimmed);
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
    {
        return path.lexically_normal().generic_string();
    }

    std::filesystem::path normalizedPath;
    bool checkedFirstComponent = false;
    for (const std::filesystem::path& component : path)
    {
        if (!checkedFirstComponent)
        {
            checkedFirstComponent = true;
            if (lowerKey(component.string()) == "assets")
            {
                continue;
            }
        }

        normalizedPath /= component;
    }

    return normalizedPath.lexically_normal().generic_string();
}

std::optional<std::filesystem::path> resolveAssetPath(const std::string& value)
{
    const std::string relativeValue = assetRelativePath(value);
    if (relativeValue.empty())
    {
        return std::nullopt;
    }

    const std::filesystem::path relativePath(relativeValue);
    if (relativePath.is_absolute())
    {
        return relativePath;
    }

    const std::filesystem::path cwdCandidate = (std::filesystem::path("assets") / relativePath).lexically_normal();
    if (std::filesystem::exists(cwdCandidate))
    {
        return cwdCandidate;
    }

    if (!executableDirectory.empty())
    {
        const std::filesystem::path exeCandidate = (executableDirectory / "assets" / relativePath).lexically_normal();
        if (std::filesystem::exists(exeCandidate))
        {
            return exeCandidate;
        }
    }

    return cwdCandidate;
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
    panel.setFillColor(sf::Color(15, 23, 26, 222));
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(sf::Color(142, 101, 53));
    window.draw(panel);

    sf::RectangleShape inner({size.x - 8.0f, size.y - 8.0f});
    inner.setPosition({position.x + 4.0f, position.y + 4.0f});
    inner.setFillColor(sf::Color::Transparent);
    inner.setOutlineThickness(1.0f);
    inner.setOutlineColor(sf::Color(44, 108, 101, 150));
    window.draw(inner);
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
    row.setFillColor(selected ? sf::Color(42, 112, 103, 230) : sf::Color(28, 39, 42, 224));
    row.setOutlineThickness(1.0f);
    row.setOutlineColor(selected ? sf::Color(111, 226, 200) : sf::Color(102, 76, 46));
    window.draw(row);

    drawText(window, font, primary, 16, {position.x + 8.0f, position.y + 5.0f}, sf::Color(246, 238, 218), size.x - 16.0f);
    if (!secondary.empty())
    {
        drawText(window, font, secondary, 12, {position.x + 8.0f, position.y + 22.0f}, sf::Color(198, 180, 142), size.x - 16.0f);
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

// Resolves a saved deck's card titles into full card definitions from the
// library so the game server can read their stats.
std::vector<card_data::Card> resolveDeckCards(
    const deck_data::Deck& deck,
    const std::vector<card_data::Card>& library)
{
    std::vector<card_data::Card> resolved;
    resolved.reserve(deck.cardTitles.size());
    for (const std::string& title : deck.cardTitles)
    {
        const auto found = std::find_if(library.begin(), library.end(), [&](const card_data::Card& card) {
            return card.title == title;
        });
        if (found != library.end())
        {
            resolved.push_back(*found);
        }
    }
    return resolved;
}

int countHeroes(const std::vector<card_data::Card>& cards)
{
    return static_cast<int>(std::count_if(cards.begin(), cards.end(), [](const card_data::Card& card) {
        return game_data::isHeroCard(card);
    }));
}

void sendSubmitDeck(sf::TcpSocket& socket, const std::vector<card_data::Card>& cards)
{
    sf::Packet packet;
    packet << static_cast<std::uint8_t>(network::MessageType::SubmitDeck);
    packet << static_cast<std::uint32_t>(cards.size());
    for (const card_data::Card& card : cards)
    {
        card_data::writeCard(packet, card);
    }
    [[maybe_unused]] auto result = socket.send(packet);
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

    sf::RenderWindow window(sf::VideoMode({800, 600}), "Steam Tactics");
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.openFromFile("assets/Roboto.ttf"))
    {
        return 1;
    }

    std::unordered_map<std::string, std::shared_ptr<sf::Texture>> textureCache;
    auto loadTexture = [&](const std::string& assetPath) -> sf::Texture* {
        const std::string key = assetRelativePath(assetPath);
        if (key.empty())
        {
            return nullptr;
        }

        if (const auto found = textureCache.find(key); found != textureCache.end())
        {
            return found->second.get();
        }

        const std::optional<std::filesystem::path> resolvedPath = resolveAssetPath(key);
        auto texture = std::make_shared<sf::Texture>();
        if (!resolvedPath || !texture->loadFromFile(*resolvedPath))
        {
            return nullptr;
        }

        texture->setSmooth(true);
        sf::Texture* loaded = texture.get();
        textureCache.emplace(key, std::move(texture));
        return loaded;
    };

    sf::Texture* backdropTexture = loadTexture("ui/steampunk-bayou-backdrop.png");

    auto drawCoverSprite = [&](sf::Texture& texture, sf::FloatRect target, sf::Color color = sf::Color::White) {
        sf::Sprite sprite(texture);
        const sf::Vector2u imageSize = texture.getSize();
        const float scale = std::max(target.size.x / static_cast<float>(imageSize.x),
                                     target.size.y / static_cast<float>(imageSize.y));
        sprite.setScale({scale, scale});
        sprite.setColor(color);
        sprite.setPosition({
            target.position.x + (target.size.x - static_cast<float>(imageSize.x) * scale) * 0.5f,
            target.position.y + (target.size.y - static_cast<float>(imageSize.y) * scale) * 0.5f});
        window.draw(sprite);
    };

    auto drawContainSprite = [&](sf::Texture& texture, sf::FloatRect target, sf::Color color = sf::Color::White) {
        sf::Sprite sprite(texture);
        const sf::Vector2u imageSize = texture.getSize();
        const float scale = std::min(target.size.x / static_cast<float>(imageSize.x),
                                     target.size.y / static_cast<float>(imageSize.y));
        sprite.setScale({scale, scale});
        sprite.setColor(color);
        sprite.setPosition({
            target.position.x + (target.size.x - static_cast<float>(imageSize.x) * scale) * 0.5f,
            target.position.y + (target.size.y - static_cast<float>(imageSize.y) * scale) * 0.5f});
        window.draw(sprite);
    };

    auto drawTextureRectContain = [&](sf::Texture& texture, sf::IntRect textureRect, sf::FloatRect target, sf::Color color = sf::Color::White) {
        sf::Sprite sprite(texture);
        sprite.setTextureRect(textureRect);
        const float sourceWidth = static_cast<float>(textureRect.size.x);
        const float sourceHeight = static_cast<float>(textureRect.size.y);
        const float scale = std::min(target.size.x / sourceWidth, target.size.y / sourceHeight);
        sprite.setScale({scale, scale});
        sprite.setColor(color);
        sprite.setPosition({
            target.position.x + (target.size.x - sourceWidth * scale) * 0.5f,
            target.position.y + (target.size.y - sourceHeight * scale) * 0.5f});
        window.draw(sprite);
    };

    auto drawBackdrop = [&]() {
        if (backdropTexture)
        {
            drawCoverSprite(*backdropTexture, {{0.0f, 0.0f}, {800.0f, 600.0f}});
        }
        else
        {
            window.clear(sf::Color(9, 17, 19));
        }

        sf::RectangleShape wash({800.0f, 600.0f});
        wash.setFillColor(sf::Color(5, 12, 15, 118));
        window.draw(wash);

        sf::RectangleShape topShade({800.0f, 124.0f});
        topShade.setFillColor(sf::Color(4, 9, 11, 156));
        window.draw(topShade);
    };

    sf::Text title(font, "Steam Tactics", 48);
    title.setFillColor(sf::Color(248, 224, 172));
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
    float animationTime = 0.0f;
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

    // Play / in-game state.
    std::optional<std::future<DeckEditorLoadResult>> pendingPlayLoad;
    std::vector<card_data::Card> matchDeck;     // resolved deck submitted to the game
    std::vector<card_data::Card> matchHeroes;   // hero cards in placement order
    game_data::Snapshot gameSnapshot;
    bool haveSnapshot = false;
    std::optional<int> selectedPieceId;
    std::optional<std::size_t> selectedHandIndex;
    struct PieceMoveAnimation
    {
        int fromRow = 0;
        int fromColumn = 0;
        int toRow = 0;
        int toColumn = 0;
        float startTime = 0.0f;
        float duration = 0.95f;
    };
    std::unordered_map<int, PieceMoveAnimation> pieceMoveAnimations;

    Button findMatchButton({300.0f, 458.0f}, {200.0f, 52.0f}, "Find Match", font);
    Button endTurnButton({InfoPanelX + 64.0f, 446.0f}, {176.0f, 44.0f}, "End Turn", font);
    Button leaveGameButton({684.0f, 14.0f}, {100.0f, 36.0f}, "Leave", font);

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
        title.setString("Steam Tactics");
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
        title.setString("");
        centerText(title, 400.0f);
        setMessage(messageText, "", sf::Color::Red);
        clearFocus();

        haveSnapshot = false;
        gameSnapshot = {};
        selectedPieceId.reset();
        selectedHandIndex.reset();

        // Submit our deck, then switch the socket to non-blocking polling.
        if (activeGameSocket)
        {
            sendSubmitDeck(*activeGameSocket, matchDeck);
            activeGameSocket->setBlocking(false);
        }
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

    auto cardByTitle = [&](const std::string& title) -> const card_data::Card* {
        const auto found = std::find_if(cardLibrary.begin(), cardLibrary.end(), [&](const card_data::Card& card) {
            return card.title == title;
        });
        return found == cardLibrary.end() ? nullptr : &*found;
    };

    struct DeckStats
    {
        int cardCount = 0;   // non-hero cards
        int heroCount = 0;
        int heroCost = 0;
    };

    auto computeDeckStats = [&]() {
        DeckStats stats;
        for (const std::string& title : editingDeck.cardTitles)
        {
            const card_data::Card* card = cardByTitle(title);
            if (card && game_data::isHeroCard(*card))
            {
                ++stats.heroCount;
                stats.heroCost += game_data::cardInt(*card, "heroCost", 0);
            }
            else
            {
                ++stats.cardCount;
            }
        }
        return stats;
    };

    auto deckValidationError = [&](const DeckStats& stats) -> std::string {
        if (stats.cardCount != game_data::DeckCardCount)
        {
            return "Need exactly " + std::to_string(game_data::DeckCardCount) + " non-hero cards (have " +
                   std::to_string(stats.cardCount) + ")";
        }
        if (stats.heroCount < game_data::MinHeroes || stats.heroCount > game_data::MaxHeroes)
        {
            return "Need " + std::to_string(game_data::MinHeroes) + "-" + std::to_string(game_data::MaxHeroes) +
                   " heroes (have " + std::to_string(stats.heroCount) + ")";
        }
        if (stats.heroCost > game_data::HeroCostLimit)
        {
            return "Hero cost " + std::to_string(stats.heroCost) + " exceeds limit " +
                   std::to_string(game_data::HeroCostLimit);
        }
        return "";
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

        const std::string validationError = deckValidationError(computeDeckStats());
        if (!validationError.empty())
        {
            setMessage(messageText, validationError, sf::Color::Red);
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

        const DeckStats stats = computeDeckStats();
        const bool cardsOk = stats.cardCount == game_data::DeckCardCount;
        const bool heroesOk = stats.heroCount >= game_data::MinHeroes && stats.heroCount <= game_data::MaxHeroes;
        const bool costOk = stats.heroCost <= game_data::HeroCostLimit;
        const sf::Color okColor(120, 220, 150);
        const sf::Color badColor(224, 130, 110);
        drawText(window, font,
                 "Cards " + std::to_string(stats.cardCount) + "/" + std::to_string(game_data::DeckCardCount),
                 14, {304.0f, 200.0f}, cardsOk ? okColor : badColor);
        drawText(window, font,
                 "Heroes " + std::to_string(stats.heroCount) + " (" + std::to_string(game_data::MinHeroes) +
                     "-" + std::to_string(game_data::MaxHeroes) + ")   Hero cost " +
                     std::to_string(stats.heroCost) + "/" + std::to_string(game_data::HeroCostLimit),
                 13, {304.0f, 220.0f}, (heroesOk && costOk) ? okColor : badColor);

        const std::size_t lastDeckCard = std::min(editingDeck.cardTitles.size(), deckCardListOffset + VisibleDeckCardRows);
        for (std::size_t i = deckCardListOffset; i < lastDeckCard; ++i)
        {
            const float y = DeckCardsY + static_cast<float>(i - deckCardListOffset) * DeckCardRowHeight;
            const card_data::Card* card = cardByTitle(editingDeck.cardTitles[i]);
            std::string secondary;
            if (card)
            {
                secondary = game_data::isHeroCard(*card)
                    ? "Hero  cost " + std::to_string(game_data::cardInt(*card, "heroCost", 0))
                    : card->type + "  " + std::to_string(game_data::cardInt(*card, "cost", 0)) + " steam";
            }
            drawRow(
                window,
                font,
                {DeckCardsX, y},
                {DeckCardsWidth, DeckCardRowHeight - 4.0f},
                editingDeck.cardTitles[i],
                secondary,
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
            const card_data::Card& libCard = cardLibrary[i];
            const std::string secondary = game_data::isHeroCard(libCard)
                ? "Hero  hc " + std::to_string(game_data::cardInt(libCard, "heroCost", 0))
                : libCard.type + "  " + std::to_string(game_data::cardInt(libCard, "cost", 0)) + " steam";
            drawRow(
                window,
                font,
                {LibraryX, y},
                {LibraryWidth, LibraryRowHeight - 4.0f},
                libCard.title,
                secondary,
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

    auto showDeckSelect = [&]() {
        currentState = GameState::DeckSelect;
        title.setString("Select Deck");
        centerText(title, 400.0f);
        clearFocus();
        playerDecks.clear();
        cardLibrary.clear();
        selectedDeck.reset();
        deckListOffset = 0;
        setMessageY(messageText, 524.0f);
        setMessage(messageText, "Loading decks...", sf::Color::Yellow);
        pendingPlayLoad = std::async(std::launch::async, loadDeckEditorData, loggedInUsername);
    };

    auto findMatch = [&]() {
        if (!selectedDeck || *selectedDeck >= playerDecks.size())
        {
            setMessage(messageText, "Select a deck first", sf::Color::Red);
            return;
        }

        matchDeck = resolveDeckCards(playerDecks[*selectedDeck], cardLibrary);
        matchHeroes.clear();
        for (const card_data::Card& card : matchDeck)
        {
            if (game_data::isHeroCard(card) && static_cast<int>(matchHeroes.size()) < game_data::MaxHeroes)
            {
                matchHeroes.push_back(card);
            }
        }

        if (matchHeroes.empty())
        {
            setMessage(messageText, "Deck needs at least one hero card", sf::Color::Red);
            return;
        }

        startMatchmaking();
    };

    // ---- in-game helpers ---------------------------------------------------

    auto ownerColor = [](int owner) -> sf::Color {
        if (owner == 1) return sf::Color(80, 132, 214);
        if (owner == 2) return sf::Color(214, 102, 74);
        return sf::Color(120, 124, 134);
    };

    auto ownerTint = [](int owner) -> sf::Color {
        if (owner == 1) return sf::Color(24, 64, 72, 226);
        if (owner == 2) return sf::Color(88, 48, 36, 226);
        return sf::Color(38, 48, 43, 214);
    };

    auto cellTopLeft = [&](int row, int column) -> sf::Vector2f {
        const int screenRow = gameSnapshot.yourPlayer == 1 ? (game_data::BoardSize - 1 - row) : row;
        return {BoardOriginX + static_cast<float>(column) * CellSize,
                BoardOriginY + static_cast<float>(screenRow) * CellSize};
    };

    auto cellTopLeftForViewer = [&](int row, int column, int viewer) -> sf::Vector2f {
        const int screenRow = viewer == 1 ? (game_data::BoardSize - 1 - row) : row;
        return {BoardOriginX + static_cast<float>(column) * CellSize,
                BoardOriginY + static_cast<float>(screenRow) * CellSize};
    };

    auto pieceByIdInSnapshot = [](const game_data::Snapshot& snapshot, int id) -> const game_data::Piece* {
        for (const game_data::Piece& piece : snapshot.pieces)
        {
            if (piece.id == id)
            {
                return &piece;
            }
        }
        return nullptr;
    };

    auto updatePieceMoveAnimations = [&](const game_data::Snapshot& nextSnapshot) {
        std::vector<int> staleAnimations;
        for (auto& [pieceId, animation] : pieceMoveAnimations)
        {
            if (!pieceByIdInSnapshot(nextSnapshot, pieceId))
            {
                staleAnimations.push_back(pieceId);
            }
        }
        for (int pieceId : staleAnimations)
        {
            pieceMoveAnimations.erase(pieceId);
        }

        if (!haveSnapshot)
        {
            return;
        }

        for (const game_data::Piece& nextPiece : nextSnapshot.pieces)
        {
            const game_data::Piece* currentPiece = pieceByIdInSnapshot(gameSnapshot, nextPiece.id);
            if (!currentPiece)
            {
                continue;
            }

            if (currentPiece->row != nextPiece.row || currentPiece->column != nextPiece.column)
            {
                pieceMoveAnimations[nextPiece.id] = {
                    currentPiece->row,
                    currentPiece->column,
                    nextPiece.row,
                    nextPiece.column,
                    animationTime,
                    0.95f};
            }
        }
    };

    auto squareAtPixel = [&](sf::Vector2f point) -> std::optional<std::pair<int, int>> {
        if (point.x < BoardOriginX || point.y < BoardOriginY)
        {
            return std::nullopt;
        }
        const int screenColumn = static_cast<int>((point.x - BoardOriginX) / CellSize);
        const int screenRow = static_cast<int>((point.y - BoardOriginY) / CellSize);
        if (screenColumn < 0 || screenColumn >= game_data::BoardSize ||
            screenRow < 0 || screenRow >= game_data::BoardSize)
        {
            return std::nullopt;
        }
        const int row = gameSnapshot.yourPlayer == 1 ? (game_data::BoardSize - 1 - screenRow) : screenRow;
        return std::make_pair(row, screenColumn);
    };

    auto gamePieceAt = [&](int row, int column) -> const game_data::Piece* {
        return game_data::findPieceAt(gameSnapshot.pieces, row, column);
    };

    auto gamePieceById = [&](int id) -> const game_data::Piece* {
        for (const game_data::Piece& piece : gameSnapshot.pieces)
        {
            if (piece.id == id)
            {
                return &piece;
            }
        }
        return nullptr;
    };

    auto cardArtTexture = [&](const std::string& imagePath) -> sf::Texture* {
        return loadTexture(imagePath);
    };

    auto walkAnimTexture = [&](const std::string& walkAnimPath) -> sf::Texture* {
        return loadTexture(walkAnimPath);
    };

    auto handCardAtPixel = [&](sf::Vector2f point) -> std::optional<std::size_t> {
        for (std::size_t i = 0; i < gameSnapshot.hand.size(); ++i)
        {
            const float x = HandStartX + static_cast<float>(i) * (HandCardWidth + HandGap);
            if (isInsideRect(point, x, HandY, HandCardWidth, HandCardHeight))
            {
                return i;
            }
        }
        return std::nullopt;
    };

    auto sendGamePacket = [&](sf::Packet& packet) {
        if (activeGameSocket)
        {
            [[maybe_unused]] auto result = activeGameSocket->send(packet);
        }
    };

    auto sendPlaceHero = [&](int heroIndex, int row, int column) {
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::PlaceHero) << heroIndex << row << column;
        sendGamePacket(packet);
    };

    auto sendPlayCard = [&](int handIndex, int row, int column) {
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::PlayCard) << handIndex << row << column;
        sendGamePacket(packet);
    };

    auto sendMovePiece = [&](int pieceId, int row, int column) {
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::MovePiece) << pieceId << row << column;
        sendGamePacket(packet);
    };

    auto sendAttackPiece = [&](int attackerId, int row, int column) {
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::AttackPiece) << attackerId << row << column;
        sendGamePacket(packet);
    };

    auto sendEndTurn = [&]() {
        sf::Packet packet;
        packet << static_cast<std::uint8_t>(network::MessageType::EndTurn);
        sendGamePacket(packet);
    };

    auto pollGameSocket = [&]() {
        if (!activeGameSocket)
        {
            return;
        }
        sf::Packet packet;
        while (activeGameSocket->receive(packet) == sf::Socket::Status::Done)
        {
            std::uint8_t type = 0;
            packet >> type;
            if (static_cast<network::MessageType>(type) == network::MessageType::GameStateUpdate)
            {
                game_data::Snapshot snapshot;
                if (game_data::readSnapshot(packet, snapshot))
                {
                    updatePieceMoveAnimations(snapshot);
                    gameSnapshot = snapshot;
                    haveSnapshot = true;
                }
            }
            packet.clear();
        }
    };

    auto leaveGame = [&]() {
        if (activeGameSocket)
        {
            sendDisconnect(*activeGameSocket);
            activeGameSocket.reset();
        }
        haveSnapshot = false;
        gameSnapshot = {};
        selectedPieceId.reset();
        selectedHandIndex.reset();
        pieceMoveAnimations.clear();
        showAuthenticatedScreen();
    };

    auto handleGameClick = [&](sf::Vector2f clickPos) {
        if (!haveSnapshot)
        {
            return;
        }

        const int me = gameSnapshot.yourPlayer;
        const game_data::Phase phase = static_cast<game_data::Phase>(gameSnapshot.phase);
        if (phase == game_data::Phase::GameOver)
        {
            return;
        }

        const std::optional<std::pair<int, int>> square = squareAtPixel(clickPos);

        if (phase == game_data::Phase::HeroPlacement)
        {
            if (square && gameSnapshot.players[static_cast<std::size_t>(me - 1)].heroesToPlace > 0)
            {
                sendPlaceHero(0, square->first, square->second);
            }
            return;
        }

        // Playing phase — only the active player may act.
        if (gameSnapshot.activePlayer != me)
        {
            return;
        }

        if (const std::optional<std::size_t> handIndex = handCardAtPixel(clickPos))
        {
            const game_data::GameCard& card = gameSnapshot.hand[*handIndex];
            selectedPieceId.reset();
            if (card.type != "Unit" && card.effect == "steam")
            {
                sendPlayCard(static_cast<int>(*handIndex), -1, -1);
                selectedHandIndex.reset();
            }
            else
            {
                selectedHandIndex = (selectedHandIndex && *selectedHandIndex == *handIndex)
                    ? std::nullopt
                    : std::optional<std::size_t>(*handIndex);
            }
            return;
        }

        if (!square)
        {
            selectedPieceId.reset();
            selectedHandIndex.reset();
            return;
        }

        const auto [row, column] = *square;
        const game_data::Piece* clicked = gamePieceAt(row, column);

        if (selectedHandIndex)
        {
            sendPlayCard(static_cast<int>(*selectedHandIndex), row, column);
            selectedHandIndex.reset();
            return;
        }

        if (selectedPieceId)
        {
            const game_data::Piece* selected = gamePieceById(*selectedPieceId);
            if (selected)
            {
                if (clicked && clicked->owner != me)
                {
                    sendAttackPiece(selected->id, row, column);
                    selectedPieceId.reset();
                    return;
                }
                if (clicked && clicked->owner == me)
                {
                    selectedPieceId = clicked->hasActed ? std::nullopt : std::optional<int>(clicked->id);
                    return;
                }
                sendMovePiece(selected->id, row, column);
                selectedPieceId.reset();
                return;
            }
            selectedPieceId.reset();
            return;
        }

        if (clicked && clicked->owner == me && !clicked->hasActed)
        {
            selectedPieceId = clicked->id;
        }
        else
        {
            selectedPieceId.reset();
        }
    };

    auto drawGameCardFace = [&](sf::Vector2f position, const game_data::GameCard& card, bool selected, bool affordable) {
        sf::RectangleShape rect({HandCardWidth, HandCardHeight});
        rect.setPosition(position);
        rect.setFillColor(selected ? sf::Color(35, 97, 92, 238) : sf::Color(20, 28, 30, 236));
        rect.setOutlineThickness(selected ? 2.0f : 1.0f);
        rect.setOutlineColor(selected ? sf::Color(121, 238, 207) : sf::Color(155, 111, 59));
        window.draw(rect);

        sf::RectangleShape artFrame({34.0f, 34.0f});
        artFrame.setPosition({position.x + 5.0f, position.y + 5.0f});
        artFrame.setFillColor(sf::Color(8, 14, 15));
        artFrame.setOutlineThickness(1.0f);
        artFrame.setOutlineColor(sf::Color(114, 83, 47));
        window.draw(artFrame);
        if (sf::Texture* art = cardArtTexture(card.imagePath))
        {
            drawContainSprite(*art, {{position.x + 7.0f, position.y + 7.0f}, {30.0f, 30.0f}},
                              affordable ? sf::Color::White : sf::Color(120, 112, 104));
        }

        const sf::Color titleColor = affordable ? sf::Color(248, 239, 216) : sf::Color(158, 128, 118);
        drawText(window, font, card.title, 12, {position.x + 43.0f, position.y + 7.0f}, titleColor, HandCardWidth - 66.0f);

        sf::CircleShape costBadge(11.0f);
        costBadge.setPosition({position.x + HandCardWidth - 24.0f, position.y + 3.0f});
        costBadge.setFillColor(affordable ? sf::Color(39, 126, 139) : sf::Color(91, 66, 58));
        costBadge.setOutlineThickness(1.0f);
        costBadge.setOutlineColor(sf::Color(224, 174, 83));
        window.draw(costBadge);
        drawText(window, font, std::to_string(card.cost), 13, {position.x + HandCardWidth - 21.0f, position.y + 4.0f}, sf::Color(248, 239, 216));

        std::string line2;
        std::string line3;
        if (card.type == "Unit")
        {
            line2 = "ATK " + std::to_string(card.attack) + "  HP " + std::to_string(card.health);
            line3 = "RNG " + std::to_string(card.attackRange) + "  MV " + std::to_string(card.moveRange);
        }
        else
        {
            line2 = "Spell";
            if (card.effect == "damage") line3 = "Deal " + std::to_string(card.power);
            else if (card.effect == "heal") line3 = "Heal " + std::to_string(card.power);
            else if (card.effect == "steam") line3 = "+" + std::to_string(card.power) + " steam";
        }
        drawText(window, font, line2, 12, {position.x + 6.0f, position.y + 42.0f}, sf::Color(224, 210, 176), HandCardWidth - 12.0f);
        drawText(window, font, line3, 12, {position.x + 6.0f, position.y + 58.0f}, sf::Color(143, 220, 205), HandCardWidth - 12.0f);
    };

    auto drawGame = [&]() {
        if (!haveSnapshot)
        {
            drawText(window, font, "Connecting to match...", 24, {260.0f, 280.0f}, sf::Color(200, 208, 222));
            leaveGameButton.draw(window);
            return;
        }

        const int me = gameSnapshot.yourPlayer;
        const game_data::Phase phase = static_cast<game_data::Phase>(gameSnapshot.phase);
        const game_data::Piece* selectedPiece = selectedPieceId ? gamePieceById(*selectedPieceId) : nullptr;

        // Precompute highlight masks for the current selection.
        std::array<int, game_data::BoardSquares> highlight{};  // 0 none,1 move,2 attack,3 place,4 spell
        if (phase == game_data::Phase::HeroPlacement &&
            gameSnapshot.players[static_cast<std::size_t>(me - 1)].heroesToPlace > 0)
        {
            for (const auto& [r, c] : game_data::homeSquares(me))
            {
                if (!gamePieceAt(r, c))
                {
                    highlight[static_cast<std::size_t>(game_data::squareIndex(r, c))] = 3;
                }
            }
        }
        else if (phase == game_data::Phase::Playing && gameSnapshot.activePlayer == me)
        {
            if (selectedPiece && !selectedPiece->hasActed)
            {
                for (int r = 0; r < game_data::BoardSize; ++r)
                {
                    for (int c = 0; c < game_data::BoardSize; ++c)
                    {
                        const std::size_t idx = static_cast<std::size_t>(game_data::squareIndex(r, c));
                        const game_data::Piece* occupant = gamePieceAt(r, c);
                        if (occupant && occupant->owner != me)
                        {
                            if (game_data::isLegalAttack(*selectedPiece, *occupant))
                            {
                                highlight[idx] = 2;
                            }
                        }
                        else if (game_data::isLegalMove(gameSnapshot.pieces, *selectedPiece, r, c))
                        {
                            highlight[idx] = 1;
                        }
                    }
                }
            }
            else if (selectedHandIndex && *selectedHandIndex < gameSnapshot.hand.size())
            {
                const game_data::GameCard& card = gameSnapshot.hand[*selectedHandIndex];
                for (int r = 0; r < game_data::BoardSize; ++r)
                {
                    for (int c = 0; c < game_data::BoardSize; ++c)
                    {
                        const std::size_t idx = static_cast<std::size_t>(game_data::squareIndex(r, c));
                        const game_data::Piece* occupant = gamePieceAt(r, c);
                        if (card.type == "Unit")
                        {
                            if (!occupant && gameSnapshot.control[idx] == me)
                            {
                                highlight[idx] = 3;
                            }
                        }
                        else if (card.effect == "damage" && occupant && occupant->owner != me)
                        {
                            highlight[idx] = 2;
                        }
                        else if (card.effect == "heal" && occupant && occupant->owner == me)
                        {
                            highlight[idx] = 4;
                        }
                    }
                }
            }
        }

        sf::RectangleShape boardBack({CellSize * game_data::BoardSize + 22.0f, CellSize * game_data::BoardSize + 22.0f});
        boardBack.setPosition({BoardOriginX - 11.0f, BoardOriginY - 11.0f});
        boardBack.setFillColor(sf::Color(9, 20, 21, 232));
        boardBack.setOutlineThickness(3.0f);
        boardBack.setOutlineColor(sf::Color(153, 105, 51));
        window.draw(boardBack);

        sf::RectangleShape boardWater({CellSize * game_data::BoardSize + 10.0f, CellSize * game_data::BoardSize + 10.0f});
        boardWater.setPosition({BoardOriginX - 5.0f, BoardOriginY - 5.0f});
        boardWater.setFillColor(sf::Color(13, 38, 42, 210));
        boardWater.setOutlineThickness(1.0f);
        boardWater.setOutlineColor(sf::Color(48, 125, 113));
        window.draw(boardWater);

        // Board squares.
        for (int row = 0; row < game_data::BoardSize; ++row)
        {
            for (int column = 0; column < game_data::BoardSize; ++column)
            {
                const std::size_t idx = static_cast<std::size_t>(game_data::squareIndex(row, column));
                const sf::Vector2f topLeft = cellTopLeft(row, column);
                sf::RectangleShape cell({CellSize - 2.0f, CellSize - 2.0f});
                cell.setPosition({topLeft.x + 1.0f, topLeft.y + 1.0f});
                cell.setFillColor(ownerTint(gameSnapshot.control[idx]));
                cell.setOutlineThickness(1.0f);
                cell.setOutlineColor(sf::Color(81, 63, 37));
                window.draw(cell);

                if ((row + column) % 2 == 0)
                {
                    sf::RectangleShape shade({CellSize - 4.0f, CellSize - 4.0f});
                    shade.setPosition({topLeft.x + 2.0f, topLeft.y + 2.0f});
                    shade.setFillColor(sf::Color(255, 239, 190, 16));
                    window.draw(shade);
                }

                if (highlight[idx] != 0)
                {
                    sf::RectangleShape overlay({CellSize - 2.0f, CellSize - 2.0f});
                    overlay.setPosition({topLeft.x + 1.0f, topLeft.y + 1.0f});
                    sf::Color colors[5] = {
                        sf::Color::Transparent,
                        sf::Color(90, 200, 120, 90),
                        sf::Color(220, 90, 80, 110),
                        sf::Color(90, 200, 210, 90),
                        sf::Color(110, 200, 150, 90)};
                    overlay.setFillColor(colors[highlight[idx]]);
                    window.draw(overlay);
                }
            }
        }

        // Pieces.
        for (const game_data::Piece& piece : gameSnapshot.pieces)
        {
            sf::Vector2f topLeft = cellTopLeft(piece.row, piece.column);
            bool isMoving = false;
            if (const auto animation = pieceMoveAnimations.find(piece.id); animation != pieceMoveAnimations.end())
            {
                const float progress = std::min((animationTime - animation->second.startTime) / animation->second.duration, 1.0f);
                if (progress < 1.0f)
                {
                    isMoving = true;
                    const sf::Vector2f start = cellTopLeftForViewer(
                        animation->second.fromRow, animation->second.fromColumn, gameSnapshot.yourPlayer);
                    const sf::Vector2f end = cellTopLeftForViewer(
                        animation->second.toRow, animation->second.toColumn, gameSnapshot.yourPlayer);
                    topLeft = {
                        start.x + (end.x - start.x) * progress,
                        start.y + (end.y - start.y) * progress};
                }
                else
                {
                    pieceMoveAnimations.erase(piece.id);
                }
            }

            sf::Color color = ownerColor(piece.owner);
            if (piece.hasActed && piece.owner == gameSnapshot.activePlayer)
            {
                color = sf::Color(static_cast<std::uint8_t>(color.r * 0.55f),
                                  static_cast<std::uint8_t>(color.g * 0.55f),
                                  static_cast<std::uint8_t>(color.b * 0.55f));
            }

            if (sf::Texture* walkSheet = walkAnimTexture(piece.walkAnimPath))
            {
                constexpr int WalkFrameCount = 4;
                const sf::Vector2u sheetSize = walkSheet->getSize();
                const int frameWidth = static_cast<int>(sheetSize.x / WalkFrameCount);
                const int frameHeight = static_cast<int>(sheetSize.y);
                if (frameWidth > 0 && frameHeight > 0)
                {
                    const int frame = isMoving ? static_cast<int>(animationTime * 5.0f) % WalkFrameCount : 0;
                    drawTextureRectContain(
                        *walkSheet,
                        sf::IntRect({frame * frameWidth, 0}, {frameWidth, frameHeight}),
                        {{topLeft.x + 5.0f, topLeft.y - 1.0f}, {CellSize - 10.0f, CellSize - 6.0f}},
                        piece.hasActed && piece.owner == gameSnapshot.activePlayer
                            ? sf::Color(150, 150, 150, 215)
                            : sf::Color::White);
                }
            }
            else if (sf::Texture* art = cardArtTexture(piece.imagePath))
            {
                drawContainSprite(*art, {{topLeft.x + 7.0f, topLeft.y + 7.0f}, {CellSize - 14.0f, CellSize - 14.0f}},
                                  piece.hasActed && piece.owner == gameSnapshot.activePlayer
                                      ? sf::Color(130, 130, 130)
                                      : sf::Color::White);
            }
            else
            {
                const float radius = CellSize * 0.29f;
                sf::CircleShape body(radius);
                body.setPosition({topLeft.x + CellSize / 2.0f - radius, topLeft.y + CellSize / 2.0f - radius});
                body.setFillColor(color);
                window.draw(body);
            }
            drawText(window, font, std::to_string(piece.health), 16,
                     {topLeft.x + CellSize / 2.0f - 6.0f, topLeft.y + CellSize - 22.0f}, sf::Color(248, 239, 216));
        }

        // Info panel.
        drawPanel(window, {InfoPanelX, InfoPanelY}, {InfoPanelWidth, InfoPanelHeight});
        float y = InfoPanelY + 12.0f;
        const game_data::PlayerSnapshot& mine = gameSnapshot.players[static_cast<std::size_t>(me - 1)];
        const game_data::PlayerSnapshot& foe = gameSnapshot.players[static_cast<std::size_t>(me == 1 ? 1 : 0)];

        drawText(window, font, "You are Player " + std::to_string(me), 18, {InfoPanelX + 14.0f, y}, ownerColor(me));
        y += 30.0f;

        std::string phaseLabel = phase == game_data::Phase::HeroPlacement ? "Hero Placement"
            : phase == game_data::Phase::Playing ? "Battle" : "Game Over";
        drawText(window, font, phaseLabel, 16, {InfoPanelX + 14.0f, y}, sf::Color(200, 208, 222));
        y += 26.0f;

        if (phase == game_data::Phase::Playing)
        {
            const bool myTurn = gameSnapshot.activePlayer == me;
            drawText(window, font, myTurn ? "Your turn" : "Opponent's turn", 17, {InfoPanelX + 14.0f, y},
                     myTurn ? sf::Color(120, 220, 150) : sf::Color(220, 180, 120));
            y += 30.0f;
        }
        else if (phase == game_data::Phase::HeroPlacement)
        {
            const int placed = static_cast<int>(matchHeroes.size()) - mine.heroesToPlace;
            if (mine.heroesToPlace > 0 && placed >= 0 && placed < static_cast<int>(matchHeroes.size()))
            {
                drawText(window, font, "Place: " + matchHeroes[static_cast<std::size_t>(placed)].title, 15,
                         {InfoPanelX + 14.0f, y}, sf::Color(120, 220, 205), InfoPanelWidth - 28.0f);
            }
            else
            {
                drawText(window, font, "Waiting for opponent...", 15, {InfoPanelX + 14.0f, y}, sf::Color(200, 200, 160));
            }
            y += 30.0f;
        }

        drawText(window, font, "Your steam: " + std::to_string(mine.steam), 17, {InfoPanelX + 14.0f, y}, sf::Color(150, 210, 235));
        y += 24.0f;
        drawText(window, font, "Squares " + std::to_string(mine.controlledSquares) +
                     "   Heroes " + std::to_string(mine.heroesAlive), 14, {InfoPanelX + 14.0f, y}, sf::Color(190, 198, 214));
        y += 28.0f;
        drawText(window, font, "Enemy steam: " + std::to_string(foe.steam), 15, {InfoPanelX + 14.0f, y}, sf::Color(225, 170, 150));
        y += 22.0f;
        drawText(window, font, "Squares " + std::to_string(foe.controlledSquares) +
                     "   Heroes " + std::to_string(foe.heroesAlive) +
                     "   Hand " + std::to_string(foe.handCount), 14, {InfoPanelX + 14.0f, y}, sf::Color(190, 198, 214));
        y += 30.0f;

        if (selectedPiece)
        {
            drawText(window, font, selectedPiece->name, 15, {InfoPanelX + 14.0f, y}, sf::Color::White, InfoPanelWidth - 28.0f);
            y += 20.0f;
            drawText(window, font, "ATK " + std::to_string(selectedPiece->attack) +
                         "  HP " + std::to_string(selectedPiece->health) + "/" + std::to_string(selectedPiece->maxHealth), 13,
                     {InfoPanelX + 14.0f, y}, sf::Color(190, 198, 214));
            y += 18.0f;
            drawText(window, font, "Range " + std::to_string(selectedPiece->attackRange) + "  " +
                         game_data::movePatternName(selectedPiece->movePattern) + " " + std::to_string(selectedPiece->moveRange), 13,
                     {InfoPanelX + 14.0f, y}, sf::Color(170, 180, 196), InfoPanelWidth - 28.0f);
            y += 18.0f;
        }

        // Status line near the bottom of the panel.
        drawText(window, font, gameSnapshot.status, 13, {InfoPanelX + 14.0f, InfoPanelY + InfoPanelHeight - 96.0f},
                 sf::Color(210, 216, 228), InfoPanelWidth - 28.0f);

        if (phase == game_data::Phase::Playing && gameSnapshot.activePlayer == me)
        {
            endTurnButton.draw(window);
        }
        leaveGameButton.draw(window);

        // Hand.
        for (std::size_t i = 0; i < gameSnapshot.hand.size(); ++i)
        {
            const float x = HandStartX + static_cast<float>(i) * (HandCardWidth + HandGap);
            const game_data::GameCard& card = gameSnapshot.hand[i];
            const bool affordable = card.cost <= mine.steam && gameSnapshot.activePlayer == me &&
                                    phase == game_data::Phase::Playing;
            drawGameCardFace({x, HandY}, card, selectedHandIndex && *selectedHandIndex == i, affordable);
        }

        // Game-over banner.
        if (phase == game_data::Phase::GameOver)
        {
            sf::RectangleShape banner({420.0f, 90.0f});
            banner.setPosition({40.0f, 210.0f});
            banner.setFillColor(sf::Color(20, 24, 32, 235));
            banner.setOutlineThickness(2.0f);
            banner.setOutlineColor(gameSnapshot.winner == me ? sf::Color(120, 220, 150) : sf::Color(220, 110, 90));
            window.draw(banner);
            const std::string result = gameSnapshot.winner == me ? "Victory!" : "Defeat";
            drawText(window, font, result, 34, {60.0f, 224.0f}, gameSnapshot.winner == me ? sf::Color(140, 230, 160) : sf::Color(230, 130, 110));
            drawText(window, font, "Press Leave to return.", 16, {60.0f, 268.0f}, sf::Color(200, 206, 220));
        }
    };

    auto drawDeckSelect = [&]() {
        drawPanel(window, {250.0f, 120.0f}, {300.0f, 312.0f});
        drawText(window, font, "Your Decks", 22, {266.0f, 132.0f}, sf::Color::White);

        const std::size_t lastDeck = std::min(playerDecks.size(), deckListOffset + VisibleDeckRows);
        for (std::size_t i = deckListOffset; i < lastDeck; ++i)
        {
            const float rowY = 172.0f + static_cast<float>(i - deckListOffset) * DeckRowHeight;
            drawRow(window, font, {266.0f, rowY}, {268.0f, DeckRowHeight - 4.0f},
                    playerDecks[i].name,
                    std::to_string(playerDecks[i].cardTitles.size()) + " cards",
                    selectedDeck && *selectedDeck == i);
        }
        if (playerDecks.empty() && !pendingPlayLoad)
        {
            drawText(window, font, "No decks. Build one in the", 15, {268.0f, 220.0f}, sf::Color(190, 198, 214));
            drawText(window, font, "Deck Editor first.", 15, {268.0f, 242.0f}, sf::Color(190, 198, 214));
        }

        findMatchButton.draw(window);
        backButton.draw(window);
        window.draw(messageText);
    };

    while (window.isOpen())
    {
        const float deltaTime = clock.restart().asSeconds();
        animationTime += deltaTime;
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        if (currentState == GameState::Game)
        {
            pollGameSocket();
        }

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
                currentState = GameState::DeckSelect;
                title.setString("Select Deck");
                centerText(title, 400.0f);
                setMessageY(messageText, 524.0f);
                setMessage(messageText, result.message, sf::Color::Red);
            }
        }

        if (pendingPlayLoad &&
            pendingPlayLoad->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            DeckEditorLoadResult result = pendingPlayLoad->get();
            pendingPlayLoad.reset();
            cardLibrary = std::move(result.cards);
            playerDecks = std::move(result.decks);
            sortDecks();
            deckListOffset = 0;
            selectedDeck = playerDecks.empty() ? std::nullopt : std::optional<std::size_t>(0);
            if (result.success)
            {
                setMessage(messageText,
                           playerDecks.empty() ? "No decks yet. Build one in the Deck Editor."
                                               : "Pick a deck and find a match.",
                           playerDecks.empty() ? sf::Color(220, 180, 120) : sf::Color(120, 220, 150));
            }
            else
            {
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
                        showDeckSelect();
                    }
                    else if (deckEditorButton.isClicked(clickPos))
                    {
                        loadDeckEditor();
                    }
                }
                else if (currentState == GameState::DeckSelect)
                {
                    if (backButton.isClicked(clickPos))
                    {
                        showAuthenticatedScreen();
                    }
                    else if (findMatchButton.isClicked(clickPos))
                    {
                        findMatch();
                    }
                    else if (const std::optional<std::size_t> deckIndex = rowIndexAt(
                                 clickPos, 266.0f, 172.0f, 268.0f, DeckRowHeight,
                                 VisibleDeckRows, deckListOffset, playerDecks.size()))
                    {
                        selectedDeck = *deckIndex;
                    }
                }
                else if (currentState == GameState::Game)
                {
                    if (leaveGameButton.isClicked(clickPos))
                    {
                        leaveGame();
                    }
                    else if (haveSnapshot &&
                             static_cast<game_data::Phase>(gameSnapshot.phase) == game_data::Phase::Playing &&
                             gameSnapshot.activePlayer == gameSnapshot.yourPlayer &&
                             endTurnButton.isClicked(clickPos))
                    {
                        sendEndTurn();
                        selectedPieceId.reset();
                        selectedHandIndex.reset();
                    }
                    else
                    {
                        handleGameClick(clickPos);
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
                    else if (currentState == GameState::Game)
                    {
                        leaveGame();
                    }
                    else if (currentState == GameState::DeckSelect)
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
        else if (currentState == GameState::DeckSelect)
        {
            findMatchButton.update(mousePos);
            backButton.update(mousePos);
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
            endTurnButton.update(mousePos);
            leaveGameButton.update(mousePos);
        }

        window.clear(sf::Color(9, 17, 19));
        drawBackdrop();
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
        else if (currentState == GameState::DeckSelect)
        {
            drawDeckSelect();
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
            drawGame();
        }

        window.display();
    }

    return 0;
}
