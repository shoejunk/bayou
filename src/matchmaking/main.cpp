#include <SFML/Network.hpp>
#include <fmt/core.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "../shared/network.hpp"

using namespace network;

namespace
{
constexpr unsigned short GameServerPort = 55002;

unsigned short requestGameSession(int matchId)
{
    sf::TcpSocket socket;
    if (socket.connect(sf::IpAddress::LocalHost, GameServerPort) != sf::Socket::Status::Done)
    {
        fmt::println("Failed to connect to game server for match {}", matchId);
        return 0;
    }

    sf::Packet request;
    request << static_cast<uint8_t>(MessageType::CreateGameSession);
    request << matchId;

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

    uint8_t responseType = 0;
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
    MatchmakingServer(unsigned short port)
        : listener(std::make_unique<sf::TcpListener>())
    {
        if (listener->listen(port) != sf::Socket::Status::Done)
        {
            fmt::println("Failed to listen on port {}", port);
            return;
        }

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
            auto client = std::make_unique<sf::TcpSocket>();
            if (listener->accept(*client) != sf::Socket::Status::Done)
            {
                continue;
            }

            auto address = client->getRemoteAddress();
            if (address)
            {
                fmt::println("Player connected from {}:{}",
                    address->toString(),
                    client->getRemotePort());
            }

            if (!receiveJoinRequest(*client))
            {
                fmt::println("Player disconnected before joining matchmaking");
                continue;
            }

            if (waitingPlayer)
            {
                const int matchId = nextMatchId++;
                const unsigned short gamePort = requestGameSession(matchId);
                fmt::println("Match {} found on game port {}", matchId, gamePort);
                sendMatchFound(*waitingPlayer, matchId, 1, gamePort);
                sendMatchFound(*client, matchId, 2, gamePort);
                waitingPlayer->disconnect();
                client->disconnect();
                waitingPlayer.reset();
            }
            else
            {
                fmt::println("Player waiting for match");
                waitingPlayer = std::move(client);
            }
        }
    }

    void stop()
    {
        running = false;
        listener->close();
        if (waitingPlayer)
        {
            waitingPlayer->disconnect();
        }
    }

private:
    std::unique_ptr<sf::TcpListener> listener;
    std::unique_ptr<sf::TcpSocket> waitingPlayer;
    std::atomic<bool> running{false};
    bool listening = false;
    int nextMatchId = 1;

    bool receiveJoinRequest(sf::TcpSocket& client)
    {
        sf::Packet packet;
        if (client.receive(packet) != sf::Socket::Status::Done)
        {
            return false;
        }

        uint8_t msgType = 0;
        packet >> msgType;
        return static_cast<MessageType>(msgType) == MessageType::JoinMatchmaking;
    }

    void sendMatchFound(sf::TcpSocket& client, int matchId, int playerNumber, unsigned short gamePort)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::MatchFound);
        response << matchId;
        response << playerNumber;
        response << gamePort;
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
