#include <SFML/Network.hpp>
#include <SQLiteCpp/SQLiteCpp.h>
#include <fmt/core.h>
#include <atomic>
#include <cstdint>
#include <exception>
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
        try
        {
            database = std::make_unique<SQLite::Database>("accounts.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
            initializeDatabase();
            fmt::println("Using accounts database: accounts.db");
        }
        catch (const std::exception& error)
        {
            fmt::println("Failed to initialize accounts database: {}", error.what());
            return;
        }

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
    std::unique_ptr<SQLite::Database> database;
    std::mutex databaseMutex;
    std::atomic<bool> running{false};
    bool listening = false;

    void initializeDatabase()
    {
        database->exec(
            "CREATE TABLE IF NOT EXISTS accounts ("
            "username TEXT PRIMARY KEY NOT NULL,"
            "password_hash TEXT NOT NULL,"
            "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
            ")");
    }

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

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            if (accountExists(username))
            {
                response << false << std::string("Username already exists");
            }
            else
            {
                SQLite::Statement insert(
                    *database,
                    "INSERT INTO accounts (username, password_hash) VALUES (?, ?)");
                insert.bind(1, username);
                insert.bind(2, hashPassword(password));
                insert.exec();

                response << true << std::string("Account created successfully");
                fmt::println("Created account for user: {}", username);
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while creating account: {}", error.what());
            response.clear();
            response << static_cast<uint8_t>(MessageType::CreateAccountResponse);
            response << false << std::string("Database error while creating account");
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

        const std::string passwordHash = hashPassword(password);

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            SQLite::Statement query(
                *database,
                "SELECT password_hash FROM accounts WHERE username = ?");
            query.bind(1, username);

            if (!query.executeStep())
            {
                response << false << std::string("Username not found");
            }
            else if (query.getColumn(0).getString() != passwordHash)
            {
                response << false << std::string("Invalid password");
            }
            else
            {
                response << true << std::string("Login successful");
                fmt::println("User logged in: {}", username);
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while logging in: {}", error.what());
            response.clear();
            response << static_cast<uint8_t>(MessageType::LoginResponse);
            response << false << std::string("Database error while logging in");
        }

        [[maybe_unused]] auto result = client.send(response);
    }

    bool accountExists(const std::string& username)
    {
        SQLite::Statement query(
            *database,
            "SELECT 1 FROM accounts WHERE username = ? LIMIT 1");
        query.bind(1, username);
        return query.executeStep();
    }

    static std::string hashPassword(const std::string& password)
    {
        std::uint64_t hash = 14695981039346656037ull;
        for (unsigned char c : password)
        {
            hash ^= c;
            hash *= 1099511628211ull;
        }

        return fmt::format("{:016x}", hash);
    }
};

int main()
{
    fmt::println("Starting Accounts Server...");

    AccountServer server(55000);
    server.run();

    return 0;
}
