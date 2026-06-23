#include <SFML/Network.hpp>
#include <fmt/core.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "../shared/network.hpp"
#include "../shared/ranking.hpp"

using namespace network;

namespace
{
constexpr unsigned short AccountServerPort = 55000;
constexpr unsigned short GameServerPort = 55002;
constexpr auto ListenerRetryInterval = std::chrono::milliseconds(250);
constexpr auto ListenerRetryTimeout = std::chrono::seconds(5);

struct RankedPlayer
{
    std::unique_ptr<sf::TcpSocket> socket;
    std::string accessToken;
    std::string username;
    int rating = 0;
    std::chrono::steady_clock::time_point joinedAt;
};

struct RegisteredMatch
{
    bool success = false;
    std::string playerOne;
    std::string playerTwo;
    std::string resultToken;
};

bool loadRankedPlayer(const std::string& accessToken, std::string& username, int& rating)
{
    sf::TcpSocket socket;
    if (socket.connect(sf::IpAddress::LocalHost, AccountServerPort) != sf::Socket::Status::Done)
    {
        return false;
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(MessageType::RankedPlayerRequest) << accessToken;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        return false;
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        return false;
    }

    std::uint8_t type = 0;
    bool success = false;
    std::string message;
    response >> type >> success >> message >> username >> rating;
    return response &&
        static_cast<MessageType>(type) == MessageType::RankedPlayerResponse &&
        success;
}

RegisteredMatch registerMatch(
    int matchId,
    const std::string& playerOneToken,
    const std::string& playerTwoToken)
{
    sf::TcpSocket socket;
    if (socket.connect(sf::IpAddress::LocalHost, AccountServerPort) != sf::Socket::Status::Done)
    {
        return {};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(MessageType::RegisterRankedMatch)
            << matchId << playerOneToken << playerTwoToken;
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        return {};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        return {};
    }

    std::uint8_t type = 0;
    std::string message;
    RegisteredMatch match;
    response >> type >> match.success >> message
             >> match.playerOne >> match.playerTwo >> match.resultToken;
    if (!response ||
        static_cast<MessageType>(type) != MessageType::RegisterRankedMatchResponse)
    {
        return {};
    }
    return match;
}

unsigned short requestGameSession(int matchId, const RegisteredMatch& match)
{
    sf::TcpSocket socket;
    if (socket.connect(sf::IpAddress::LocalHost, GameServerPort) != sf::Socket::Status::Done)
    {
        fmt::println("Failed to connect to game server for match {}", matchId);
        return 0;
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(MessageType::CreateGameSession)
            << matchId << match.playerOne << match.playerTwo << match.resultToken;

    if (socket.send(request) != sf::Socket::Status::Done)
    {
        fmt::println("Failed to request game session for match {}", matchId);
        return 0;
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        fmt::println("No game server response for match {}", matchId);
        return 0;
    }

    std::uint8_t responseType = 0;
    bool success = false;
    int responseMatchId = 0;
    unsigned short gamePort = 0;
    std::string message;
    response >> responseType >> success >> responseMatchId >> gamePort >> message;

    if (static_cast<MessageType>(responseType) != MessageType::GameSessionCreated ||
        !success ||
        responseMatchId != matchId)
    {
        fmt::println("Game server failed match {}: {}", matchId, message);
        return 0;
    }

    return gamePort;
}
}

class MatchmakingServer
{
public:
    explicit MatchmakingServer(unsigned short port)
        : listener(std::make_unique<sf::TcpListener>()),
          nextMatchId(std::uniform_int_distribution<int>(
              1, std::numeric_limits<int>::max())(rng))
    {
        const auto retryDeadline =
            std::chrono::steady_clock::now() + ListenerRetryTimeout;
        while (listener->listen(port) != sf::Socket::Status::Done)
        {
            if (std::chrono::steady_clock::now() >= retryDeadline)
            {
                fmt::println(
                    "Failed to listen on port {} after {} seconds",
                    port,
                    ListenerRetryTimeout.count());
                return;
            }
            fmt::println(
                "Port {} is temporarily unavailable; retrying...",
                port);
            std::this_thread::sleep_for(ListenerRetryInterval);
        }

        listener->setBlocking(false);
        listening = true;
        fmt::println("Matchmaking server listening on port {}", port);
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
            acceptWaitingPlayers();
            makeMatches();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    }

    void stop()
    {
        running = false;
        listener->close();
        for (RankedPlayer& player : waitingPlayers)
        {
            player.socket->disconnect();
        }
    }

private:
    std::unique_ptr<sf::TcpListener> listener;
    std::vector<RankedPlayer> waitingPlayers;
    std::mt19937 rng{std::random_device{}()};
    std::atomic<bool> running{false};
    bool listening = false;
    int nextMatchId = 1;

    void acceptWaitingPlayers()
    {
        while (true)
        {
            auto client = std::make_unique<sf::TcpSocket>();
            if (listener->accept(*client) != sf::Socket::Status::Done)
            {
                return;
            }

            sf::Packet packet;
            if (client->receive(packet) != sf::Socket::Status::Done)
            {
                continue;
            }

            std::uint8_t type = 0;
            std::string accessToken;
            packet >> type >> accessToken;
            if (!packet || static_cast<MessageType>(type) != MessageType::JoinMatchmaking)
            {
                continue;
            }

            std::string username;
            int rating = 0;
            if (!loadRankedPlayer(accessToken, username, rating))
            {
                fmt::println("Rejected unauthenticated matchmaking request");
                client->disconnect();
                continue;
            }

            fmt::println("Player {} waiting for match at rating {}", username, rating);
            waitingPlayers.push_back({
                std::move(client),
                std::move(accessToken),
                std::move(username),
                rating,
                std::chrono::steady_clock::now()});
        }
    }

    bool compatible(
        const RankedPlayer& one,
        const RankedPlayer& two,
        std::chrono::steady_clock::time_point now) const
    {
        const int difference = std::abs(one.rating - two.rating);
        const int rangeOne = ranking::matchmakingRange(now - one.joinedAt);
        const int rangeTwo = ranking::matchmakingRange(now - two.joinedAt);
        return difference <= std::min(rangeOne, rangeTwo);
    }

    void makeMatches()
    {
        const auto now = std::chrono::steady_clock::now();
        for (std::size_t one = 0; one < waitingPlayers.size(); ++one)
        {
            std::size_t best = waitingPlayers.size();
            int bestDifference = std::numeric_limits<int>::max();
            for (std::size_t two = one + 1; two < waitingPlayers.size(); ++two)
            {
                if (!compatible(waitingPlayers[one], waitingPlayers[two], now))
                {
                    continue;
                }

                const int difference =
                    std::abs(waitingPlayers[one].rating - waitingPlayers[two].rating);
                if (difference < bestDifference)
                {
                    best = two;
                    bestDifference = difference;
                }
            }

            if (best == waitingPlayers.size())
            {
                continue;
            }

            const int matchId = allocateMatchId();
            RegisteredMatch match = registerMatch(
                matchId,
                waitingPlayers[one].accessToken,
                waitingPlayers[best].accessToken);
            if (!match.success)
            {
                fmt::println("Could not register ranked match {}", matchId);
                continue;
            }

            const unsigned short gamePort = requestGameSession(matchId, match);
            if (gamePort == 0)
            {
                continue;
            }

            fmt::println(
                "Match {} found: {} ({}) vs {} ({}) on port {}",
                matchId,
                waitingPlayers[one].username,
                waitingPlayers[one].rating,
                waitingPlayers[best].username,
                waitingPlayers[best].rating,
                gamePort);
            sendMatchFound(*waitingPlayers[one].socket, matchId, 1, gamePort);
            sendMatchFound(*waitingPlayers[best].socket, matchId, 2, gamePort);
            waitingPlayers[one].socket->disconnect();
            waitingPlayers[best].socket->disconnect();

            waitingPlayers.erase(waitingPlayers.begin() + static_cast<std::ptrdiff_t>(best));
            waitingPlayers.erase(waitingPlayers.begin() + static_cast<std::ptrdiff_t>(one));
            --one;
        }
    }

    int allocateMatchId()
    {
        const int result = nextMatchId;
        nextMatchId = nextMatchId == std::numeric_limits<int>::max()
            ? 1
            : nextMatchId + 1;
        return result;
    }

    static void sendMatchFound(
        sf::TcpSocket& client,
        int matchId,
        int playerNumber,
        unsigned short gamePort)
    {
        sf::Packet response;
        response << static_cast<std::uint8_t>(MessageType::MatchFound)
                 << matchId << playerNumber << gamePort;
        [[maybe_unused]] auto result = client.send(response);
    }
};

int main()
{
    fmt::println("Starting Matchmaking Server...");

    MatchmakingServer server(55001);
    server.run();

    return 0;
}
