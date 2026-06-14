#include <SFML/Network.hpp>
#include <SQLiteCpp/SQLiteCpp.h>
#include <fmt/core.h>

#include "../shared/deck_data.hpp"

#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

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
        database->exec("PRAGMA foreign_keys = ON");
        database->exec(
            "CREATE TABLE IF NOT EXISTS accounts ("
            "username TEXT PRIMARY KEY NOT NULL,"
            "password_hash TEXT NOT NULL,"
            "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
            ")");
        database->exec(
            "CREATE TABLE IF NOT EXISTS decks ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "username TEXT NOT NULL,"
            "name TEXT NOT NULL,"
            "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "UNIQUE(username, name),"
            "FOREIGN KEY(username) REFERENCES accounts(username) ON DELETE CASCADE"
            ")");
        database->exec(
            "CREATE TABLE IF NOT EXISTS deck_cards ("
            "deck_id INTEGER NOT NULL,"
            "card_index INTEGER NOT NULL,"
            "card_title TEXT NOT NULL,"
            "PRIMARY KEY(deck_id, card_index),"
            "FOREIGN KEY(deck_id) REFERENCES decks(id) ON DELETE CASCADE"
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
                case MessageType::DeckListRequest:
                {
                    std::string username;
                    packet >> username;
                    handleListDecks(*client, username);
                    break;
                }
                case MessageType::DeckSaveRequest:
                {
                    std::string username, originalName;
                    deck_data::Deck deck;
                    packet >> username >> originalName;
                    if (!packet || !deck_data::readDeck(packet, deck))
                    {
                        sendDeckCommandResponse(
                            *client,
                            MessageType::DeckSaveResponse,
                            false,
                            "Invalid deck save payload");
                        break;
                    }
                    handleSaveDeck(*client, username, originalName, deck);
                    break;
                }
                case MessageType::DeckDeleteRequest:
                {
                    std::string username, deckName;
                    packet >> username >> deckName;
                    if (!packet)
                    {
                        sendDeckCommandResponse(
                            *client,
                            MessageType::DeckDeleteResponse,
                            false,
                            "Invalid deck delete payload");
                        break;
                    }
                    handleDeleteDeck(*client, username, deckName);
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

    void handleListDecks(sf::TcpSocket& client, const std::string& username)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::DeckListResponse);

        if (username.empty())
        {
            response << false << std::string("Username cannot be empty");
            response << static_cast<std::uint32_t>(0);
            [[maybe_unused]] auto result = client.send(response);
            return;
        }

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            if (!accountExists(username))
            {
                response << false << std::string("Username not found");
                response << static_cast<std::uint32_t>(0);
            }
            else
            {
                const std::vector<deck_data::Deck> decks = loadDecks(username);
                response << true << std::string("Decks loaded");
                response << static_cast<std::uint32_t>(decks.size());
                for (const deck_data::Deck& deck : decks)
                {
                    deck_data::writeDeck(response, deck);
                }
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while loading decks: {}", error.what());
            response.clear();
            response << static_cast<uint8_t>(MessageType::DeckListResponse);
            response << false << std::string("Database error while loading decks");
            response << static_cast<std::uint32_t>(0);
        }

        [[maybe_unused]] auto result = client.send(response);
    }

    void handleSaveDeck(
        sf::TcpSocket& client,
        const std::string& username,
        const std::string& originalName,
        const deck_data::Deck& deck)
    {
        if (username.empty())
        {
            sendDeckCommandResponse(client, MessageType::DeckSaveResponse, false, "Username cannot be empty");
            return;
        }

        if (deck.name.empty())
        {
            sendDeckCommandResponse(client, MessageType::DeckSaveResponse, false, "Deck name cannot be empty");
            return;
        }

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            if (!accountExists(username))
            {
                sendDeckCommandResponse(client, MessageType::DeckSaveResponse, false, "Username not found");
                return;
            }

            saveDeck(username, originalName, deck);
            sendDeckCommandResponse(client, MessageType::DeckSaveResponse, true, "Deck saved");
            fmt::println("Saved deck '{}' for user {}", deck.name, username);
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while saving deck: {}", error.what());
            sendDeckCommandResponse(client, MessageType::DeckSaveResponse, false, "Database error while saving deck");
        }
    }

    void handleDeleteDeck(sf::TcpSocket& client, const std::string& username, const std::string& deckName)
    {
        if (username.empty())
        {
            sendDeckCommandResponse(client, MessageType::DeckDeleteResponse, false, "Username cannot be empty");
            return;
        }

        if (deckName.empty())
        {
            sendDeckCommandResponse(client, MessageType::DeckDeleteResponse, false, "Deck name cannot be empty");
            return;
        }

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            if (!accountExists(username))
            {
                sendDeckCommandResponse(client, MessageType::DeckDeleteResponse, false, "Username not found");
                return;
            }

            if (!deleteDeck(username, deckName))
            {
                sendDeckCommandResponse(client, MessageType::DeckDeleteResponse, false, "Deck not found");
                return;
            }

            sendDeckCommandResponse(client, MessageType::DeckDeleteResponse, true, "Deck deleted");
            fmt::println("Deleted deck '{}' for user {}", deckName, username);
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while deleting deck: {}", error.what());
            sendDeckCommandResponse(client, MessageType::DeckDeleteResponse, false, "Database error while deleting deck");
        }
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

    void sendDeckCommandResponse(
        sf::TcpSocket& client,
        MessageType responseType,
        bool success,
        const std::string& message)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(responseType);
        response << success << message;
        [[maybe_unused]] auto result = client.send(response);
    }

    std::vector<deck_data::Deck> loadDecks(const std::string& username)
    {
        std::vector<deck_data::Deck> decks;
        SQLite::Statement query(
            *database,
            "SELECT id, name FROM decks WHERE username = ? ORDER BY name");
        query.bind(1, username);

        while (query.executeStep())
        {
            const long long deckId = query.getColumn(0).getInt64();
            deck_data::Deck deck;
            deck.name = query.getColumn(1).getString();
            deck.cardTitles = loadDeckCards(deckId);
            decks.push_back(deck);
        }

        return decks;
    }

    std::vector<std::string> loadDeckCards(long long deckId)
    {
        std::vector<std::string> cardTitles;
        SQLite::Statement query(
            *database,
            "SELECT card_title FROM deck_cards WHERE deck_id = ? ORDER BY card_index");
        query.bind(1, deckId);

        while (query.executeStep())
        {
            cardTitles.push_back(query.getColumn(0).getString());
        }

        return cardTitles;
    }

    void saveDeck(const std::string& username, const std::string& originalName, const deck_data::Deck& deck)
    {
        SQLite::Transaction transaction(*database);

        const std::string lookupName = originalName.empty() ? deck.name : originalName;
        std::optional<long long> deckId = findDeckId(username, lookupName);
        if (deckId)
        {
            SQLite::Statement update(
                *database,
                "UPDATE decks SET name = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
            update.bind(1, deck.name);
            update.bind(2, *deckId);
            update.exec();
        }
        else
        {
            SQLite::Statement insert(
                *database,
                "INSERT INTO decks (username, name) VALUES (?, ?)");
            insert.bind(1, username);
            insert.bind(2, deck.name);
            insert.exec();
            deckId = database->getLastInsertRowid();
        }

        SQLite::Statement deleteCards(*database, "DELETE FROM deck_cards WHERE deck_id = ?");
        deleteCards.bind(1, *deckId);
        deleteCards.exec();

        SQLite::Statement insertCard(
            *database,
            "INSERT INTO deck_cards (deck_id, card_index, card_title) VALUES (?, ?, ?)");
        for (std::size_t i = 0; i < deck.cardTitles.size(); ++i)
        {
            insertCard.reset();
            insertCard.bind(1, *deckId);
            insertCard.bind(2, static_cast<int>(i));
            insertCard.bind(3, deck.cardTitles[i]);
            insertCard.exec();
        }

        transaction.commit();
    }

    bool deleteDeck(const std::string& username, const std::string& deckName)
    {
        const std::optional<long long> deckId = findDeckId(username, deckName);
        if (!deckId)
        {
            return false;
        }

        SQLite::Transaction transaction(*database);
        SQLite::Statement statement(*database, "DELETE FROM decks WHERE id = ?");
        statement.bind(1, *deckId);
        statement.exec();
        transaction.commit();
        return true;
    }

    std::optional<long long> findDeckId(const std::string& username, const std::string& deckName)
    {
        SQLite::Statement query(
            *database,
            "SELECT id FROM decks WHERE username = ? AND name = ? LIMIT 1");
        query.bind(1, username);
        query.bind(2, deckName);
        if (!query.executeStep())
        {
            return std::nullopt;
        }

        return query.getColumn(0).getInt64();
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
