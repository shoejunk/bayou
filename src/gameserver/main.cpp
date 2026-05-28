#include <SFML/Network.hpp>
#include <fmt/core.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

import network;

using namespace network;

namespace
{
constexpr unsigned short GameServerPort = 55002;
constexpr unsigned short FirstGamePort = 56000;

struct CardDefinition
{
    std::string id;
    std::string name;
};

struct Deck
{
    std::vector<CardDefinition> cards;
};

struct Piece
{
    std::string id;
    std::string type;
    int owner = 0;
    int row = 0;
    int column = 0;
};

struct PlayerState
{
    int playerNumber = 0;
    Deck deck;
    std::vector<Piece> startingPieces;
};

struct GameState
{
    int matchId = 0;
    std::array<std::optional<Piece>, 64> board;
    std::array<PlayerState, 2> players;
    int activePlayer = 1;
};

struct JoinedPlayer
{
    std::unique_ptr<sf::TcpSocket> socket;
    int playerNumber = 0;
};

Deck makeDefaultDeck()
{
    return {{
        {"strike", "Strike"},
        {"guard", "Guard"},
        {"advance", "Advance"},
        {"rally", "Rally"},
    }};
}

std::vector<Piece> makeStartingPieces(int playerNumber)
{
    const int backRow = playerNumber == 1 ? 7 : 0;
    const int frontRow = playerNumber == 1 ? 6 : 1;
    return {
        {fmt::format("p{}_king", playerNumber), "King", playerNumber, backRow, 4},
        {fmt::format("p{}_rook_a", playerNumber), "Rook", playerNumber, backRow, 0},
        {fmt::format("p{}_rook_h", playerNumber), "Rook", playerNumber, backRow, 7},
        {fmt::format("p{}_pawn_d", playerNumber), "Pawn", playerNumber, frontRow, 3},
        {fmt::format("p{}_pawn_e", playerNumber), "Pawn", playerNumber, frontRow, 4},
    };
}

GameState createInitialGameState(int matchId)
{
    GameState state;
    state.matchId = matchId;

    for (int playerNumber = 1; playerNumber <= 2; ++playerNumber)
    {
        PlayerState& player = state.players[static_cast<std::size_t>(playerNumber - 1)];
        player.playerNumber = playerNumber;
        player.deck = makeDefaultDeck();
        player.startingPieces = makeStartingPieces(playerNumber);

        for (const Piece& piece : player.startingPieces)
        {
            state.board[static_cast<std::size_t>(piece.row * 8 + piece.column)] = piece;
        }
    }

    return state;
}

std::string executablePath()
{
#ifdef _WIN32
    std::array<char, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return std::string(buffer.data(), length);
#else
    return "gameserver";
#endif
}

bool spawnGameProcess(int matchId, unsigned short port)
{
#ifdef _WIN32
    std::string command = fmt::format("\"{}\" --game {} {}", executablePath(), matchId, port);
    STARTUPINFOA startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    std::vector<char> commandBuffer(command.begin(), command.end());
    commandBuffer.push_back('\0');

    if (!CreateProcessA(
            nullptr,
            commandBuffer.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startupInfo,
            &processInfo))
    {
        fmt::println("Failed to start game process for match {}", matchId);
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
#else
    const std::string command = fmt::format("{} --game {} {} &", executablePath(), matchId, port);
    return std::system(command.c_str()) == 0;
#endif
}
}

class GameServerCoordinator
{
public:
    explicit GameServerCoordinator(unsigned short port)
        : listener(std::make_unique<sf::TcpListener>())
    {
        if (listener->listen(port) != sf::Socket::Status::Done)
        {
            fmt::println("Failed to listen on port {}", port);
            return;
        }

        listening = true;
        fmt::println("Game server coordinator listening on port {}", port);
    }

    void run()
    {
        if (!listening)
        {
            return;
        }

        running = true;
        while (running)
        {
            auto client = std::make_unique<sf::TcpSocket>();
            if (listener->accept(*client) == sf::Socket::Status::Done)
            {
                handleClient(*client);
            }
        }
    }

private:
    std::unique_ptr<sf::TcpListener> listener;
    std::atomic<bool> running{false};
    bool listening = false;
    unsigned short nextGamePort = FirstGamePort;

    void handleClient(sf::TcpSocket& client)
    {
        sf::Packet packet;
        if (client.receive(packet) != sf::Socket::Status::Done)
        {
            return;
        }

        uint8_t msgType = 0;
        int matchId = 0;
        packet >> msgType >> matchId;

        if (static_cast<MessageType>(msgType) != MessageType::CreateGameSession)
        {
            return;
        }

        const unsigned short port = nextGamePort++;
        const bool started = spawnGameProcess(matchId, port);

        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::GameSessionCreated);
        response << started;
        response << matchId;
        response << (started ? port : static_cast<unsigned short>(0));
        response << std::string(started ? "Game session created" : "Failed to create game session");
        [[maybe_unused]] auto result = client.send(response);

        if (started)
        {
            fmt::println("Started game process for match {} on port {}", matchId, port);
        }
    }
};

class GameProcess
{
public:
    GameProcess(int matchId, unsigned short port)
        : matchId(matchId), port(port), listener(std::make_unique<sf::TcpListener>())
    {
        if (listener->listen(port) != sf::Socket::Status::Done)
        {
            fmt::println("Game {} failed to listen on port {}", matchId, port);
            return;
        }

        listening = true;
        fmt::println("Game {} listening on port {}", matchId, port);
    }

    void run()
    {
        if (!listening)
        {
            return;
        }

        auto playerOne = acceptPlayer();
        auto playerTwo = acceptPlayer();
        if (!playerOne || !playerTwo)
        {
            fmt::println("Game {} did not receive both players", matchId);
            return;
        }

        gameState = createInitialGameState(matchId);
        fmt::println("Game {} initialized with {} board squares and two default decks",
            matchId,
            gameState.board.size());

        sendGameReady(*playerOne->socket, playerOne->playerNumber);
        sendGameReady(*playerTwo->socket, playerTwo->playerNumber);

        waitForDisconnect(*playerOne->socket, *playerTwo->socket);
        fmt::println("Game {} ended", matchId);
    }

private:
    int matchId = 0;
    unsigned short port = 0;
    std::unique_ptr<sf::TcpListener> listener;
    bool listening = false;
    GameState gameState;

    std::optional<JoinedPlayer> acceptPlayer()
    {
        auto client = std::make_unique<sf::TcpSocket>();
        if (listener->accept(*client) != sf::Socket::Status::Done)
        {
            return std::nullopt;
        }

        sf::Packet packet;
        if (client->receive(packet) != sf::Socket::Status::Done)
        {
            return std::nullopt;
        }

        uint8_t msgType = 0;
        int joinedMatchId = 0;
        int playerNumber = 0;
        packet >> msgType >> joinedMatchId >> playerNumber;

        if (static_cast<MessageType>(msgType) != MessageType::JoinGame || joinedMatchId != matchId)
        {
            return std::nullopt;
        }

        fmt::println("Player {} joined game {}", playerNumber, matchId);
        return JoinedPlayer{std::move(client), playerNumber};
    }

    void sendGameReady(sf::TcpSocket& client, int playerNumber)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::GameReady);
        response << matchId;
        response << playerNumber;
        response << std::string("Game ready");
        [[maybe_unused]] auto result = client.send(response);
    }

    void waitForDisconnect(sf::TcpSocket& playerOne, sf::TcpSocket& playerTwo)
    {
        playerOne.setBlocking(false);
        playerTwo.setBlocking(false);

        while (true)
        {
            sf::Packet packet;
            const auto oneStatus = playerOne.receive(packet);
            packet.clear();
            const auto twoStatus = playerTwo.receive(packet);

            if (oneStatus == sf::Socket::Status::Disconnected && twoStatus == sf::Socket::Status::Disconnected)
            {
                return;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
};

int main(int argc, char* argv[])
{
    if (argc == 4 && std::string(argv[1]) == "--game")
    {
        const int matchId = std::stoi(argv[2]);
        const unsigned short port = static_cast<unsigned short>(std::stoi(argv[3]));
        GameProcess game(matchId, port);
        game.run();
        return 0;
    }

    fmt::println("Starting Game Server Coordinator...");

    GameServerCoordinator coordinator(GameServerPort);
    coordinator.run();

    return 0;
}
