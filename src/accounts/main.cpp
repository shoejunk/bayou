#include <SFML/Network.hpp>
#include <fmt/core.h>
#include <atomic>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

import network;

using namespace network;

class AccountServer
{
public:
    AccountServer(unsigned short port)
        : listener(std::make_unique<sf::TcpListener>())
    {
        if (listener->listen(port) != sf::Socket::Status::Done)
        {
            fmt::println("Failed to listen on port {}", port);
            return;
        }
        listening = true;
        fmt::println("Account server listening on port {}", port);
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
                auto address = client->getRemoteAddress();
                if (address)
                {
                    fmt::println("New client connected from {}:{}",
                        address->toString(),
                        client->getRemotePort());
                }
                std::thread(&AccountServer::handleClient, this, std::move(client)).detach();
            }
        }
    }

    void stop()
    {
        running = false;
        listener->close();
    }

private:
    std::unique_ptr<sf::TcpListener> listener;
    std::map<std::string, std::size_t> accounts;
    std::mutex accountsMutex;
    std::atomic<bool> running{false};
    bool listening = false;

    void handleClient(std::unique_ptr<sf::TcpSocket> client)
    {
        sf::Packet packet;

        while (running)
        {
            if (client->receive(packet) != sf::Socket::Status::Done)
            {
                fmt::println("Client disconnected");
                return;
            }

            uint8_t msgType;
            packet >> msgType;

            switch (static_cast<MessageType>(msgType))
            {
                case MessageType::CreateAccount:
                {
                    std::string username, password;
                    packet >> username >> password;
                    handleCreateAccount(*client, username, password);
                    break;
                }
                case MessageType::Login:
                {
                    std::string username, password;
                    packet >> username >> password;
                    handleLogin(*client, username, password);
                    break;
                }
                case MessageType::Disconnect:
                    fmt::println("Client requested disconnect");
                    return;
                default:
                    fmt::println("Unknown message type: {}", msgType);
                    break;
            }

            packet.clear();
        }
    }

    void handleCreateAccount(sf::TcpSocket& client, const std::string& username, const std::string& password)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::CreateAccountResponse);

        if (username.empty() || password.empty())
        {
            response << false << std::string("Username and password cannot be empty");
            [[maybe_unused]] auto result = client.send(response);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(accountsMutex);
            if (accounts.contains(username))
            {
                response << false << std::string("Username already exists");
            }
            else
            {
                accounts[username] = hashPassword(password);
                response << true << std::string("Account created successfully");
                fmt::println("Created account for user: {}", username);
            }
        }

        [[maybe_unused]] auto result = client.send(response);
    }

    void handleLogin(sf::TcpSocket& client, const std::string& username, const std::string& password)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::LoginResponse);

        if (username.empty() || password.empty())
        {
            response << false << std::string("Username and password cannot be empty");
            [[maybe_unused]] auto result = client.send(response);
            return;
        }

        const std::size_t passwordHash = hashPassword(password);

        {
            std::lock_guard<std::mutex> lock(accountsMutex);
            auto it = accounts.find(username);
            if (it == accounts.end())
            {
                response << false << std::string("Username not found");
            }
            else if (it->second != passwordHash)
            {
                response << false << std::string("Invalid password");
            }
            else
            {
                response << true << std::string("Login successful");
                fmt::println("User logged in: {}", username);
            }
        }

        [[maybe_unused]] auto result = client.send(response);
    }

    static std::size_t hashPassword(const std::string& password)
    {
        return std::hash<std::string>{}(password);
    }
};

int main()
{
    fmt::println("Starting Accounts Server...");

    AccountServer server(55000);
    server.run();

    return 0;
}
