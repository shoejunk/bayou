#include <SFML/Network.hpp>
#include <fmt/core.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

import network;

using namespace network;

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
                fmt::println("Match {} found", matchId);
                sendMatchFound(*waitingPlayer, matchId, 1);
                sendMatchFound(*client, matchId, 2);
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

    void sendMatchFound(sf::TcpSocket& client, int matchId, int playerNumber)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::MatchFound);
        response << matchId;
        response << playerNumber;
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
