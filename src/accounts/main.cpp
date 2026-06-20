#include <SFML/Network.hpp>
#include <SQLiteCpp/SQLiteCpp.h>
#include <fmt/core.h>
#include <sodium.h>

#include "../shared/account_data.hpp"
#include "../shared/deck_data.hpp"

#include <atomic>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../shared/network.hpp"

using namespace network;

namespace
{
constexpr int StarterNonHeroKinds = 20;
constexpr int WinRewardCoins = 10;
constexpr int ShopCardCost = 5;
constexpr const char* StarterDeckName = "Starter Deck";
constexpr const char* PreferredStarterHero = "Steam Baron";
constexpr std::int64_t RememberTokenLifetimeSeconds = 30LL * 24LL * 60LL * 60LL;

struct ShopCardEntry
{
    std::string title;
    std::string rarity;
};

int shopRarityWeight(const std::string& rarity)
{
    if (rarity == "legendary")
    {
        return 5;
    }
    if (rarity == "rare")
    {
        return 25;
    }
    return 70;
}

std::string normalizedRarity(const std::string& rarity)
{
    if (rarity == "rare" || rarity == "legendary")
    {
        return rarity;
    }
    return "common";
}

const std::vector<std::string>& fallbackStarterNonHeroes()
{
    static const std::vector<std::string> titles = {
        "Brass Pawn",
        "Rifleman",
        "Clockwork Rook",
        "Steam Bishop",
        "Automaton Knight",
        "Dredger",
        "Spark Drone",
        "Smoke Bomb",
        "Cannon Blast",
        "Repair Kit",
        "Overpressure",
        "Gearwright",
        "Brass Medic",
        "Boiler Imp",
        "Railgunner",
        "Swamp Skiff",
        "Arc Lantern",
        "Sprocket Swarm",
        "Chain Harpoon",
        "Mudslide",
    };
    return titles;
}
}

class AccountServer
{
public:
    AccountServer(unsigned short port)
        : listener(std::make_unique<sf::TcpListener>())
    {
        if (sodium_init() < 0)
        {
            fmt::println("Failed to initialize cryptography");
            return;
        }

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
    std::mt19937 rng{std::random_device{}()};
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
        if (!columnExists("accounts", "coins"))
        {
            database->exec("ALTER TABLE accounts ADD COLUMN coins INTEGER NOT NULL DEFAULT 0");
        }
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
        database->exec(
            "CREATE TABLE IF NOT EXISTS card_collections ("
            "username TEXT NOT NULL,"
            "card_title TEXT NOT NULL,"
            "copies INTEGER NOT NULL DEFAULT 0,"
            "PRIMARY KEY(username, card_title),"
            "FOREIGN KEY(username) REFERENCES accounts(username) ON DELETE CASCADE"
            ")");
        database->exec(
            "CREATE TABLE IF NOT EXISTS remember_tokens ("
            "token_hash TEXT PRIMARY KEY NOT NULL,"
            "username TEXT NOT NULL,"
            "expires_at INTEGER NOT NULL,"
            "created_at INTEGER NOT NULL,"
            "last_used_at INTEGER NOT NULL,"
            "FOREIGN KEY(username) REFERENCES accounts(username) ON DELETE CASCADE"
            ")");
        database->exec(
            "CREATE INDEX IF NOT EXISTS remember_tokens_username_idx "
            "ON remember_tokens(username)");
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
                    bool rememberMe = false;
                    packet >> username >> password >> rememberMe;
                    handleLogin(*client, username, password, rememberMe);
                    break;
                }
                case MessageType::RememberLogin:
                {
                    std::string token;
                    packet >> token;
                    handleRememberLogin(*client, token);
                    break;
                }
                case MessageType::RevokeRememberToken:
                {
                    std::string token;
                    packet >> token;
                    handleRevokeRememberToken(*client, token);
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
                case MessageType::AccountStateRequest:
                {
                    std::string username;
                    packet >> username;
                    handleAccountState(*client, username);
                    break;
                }
                case MessageType::WinRewardRequest:
                {
                    std::string username;
                    packet >> username;
                    handleWinReward(*client, username);
                    break;
                }
                case MessageType::ShopPurchaseRequest:
                {
                    std::string username;
                    packet >> username;
                    handleShopPurchase(*client, username);
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

                ensureStarterInventory(username);
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

            if (const std::optional<std::string> collectionError = deckCollectionError(username, deck))
            {
                sendDeckCommandResponse(client, MessageType::DeckSaveResponse, false, *collectionError);
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

    void handleAccountState(sf::TcpSocket& client, const std::string& username)
    {
        if (username.empty())
        {
            sendAccountStateResponse(client, false, "Username cannot be empty", 0, {});
            return;
        }

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            if (!accountExists(username))
            {
                sendAccountStateResponse(client, false, "Username not found", 0, {});
                return;
            }

            ensureStarterInventory(username);
            sendAccountStateResponse(client, true, "Account loaded", loadCoins(username), loadCollection(username));
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while loading account state: {}", error.what());
            sendAccountStateResponse(client, false, "Database error while loading account state", 0, {});
        }
    }

    void handleWinReward(sf::TcpSocket& client, const std::string& username)
    {
        if (username.empty())
        {
            sendCoinResponse(client, MessageType::WinRewardResponse, false, "Username cannot be empty", 0);
            return;
        }

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            if (!accountExists(username))
            {
                sendCoinResponse(client, MessageType::WinRewardResponse, false, "Username not found", 0);
                return;
            }

            SQLite::Statement update(*database, "UPDATE accounts SET coins = coins + ? WHERE username = ?");
            update.bind(1, WinRewardCoins);
            update.bind(2, username);
            update.exec();

            sendCoinResponse(
                client,
                MessageType::WinRewardResponse,
                true,
                "+" + std::to_string(WinRewardCoins) + " coins",
                loadCoins(username));
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while rewarding win: {}", error.what());
            sendCoinResponse(client, MessageType::WinRewardResponse, false, "Database error while rewarding win", 0);
        }
    }

    void handleShopPurchase(sf::TcpSocket& client, const std::string& username)
    {
        if (username.empty())
        {
            sendShopPurchaseResponse(client, false, "Username cannot be empty", 0, "");
            return;
        }

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            if (!accountExists(username))
            {
                sendShopPurchaseResponse(client, false, "Username not found", 0, "");
                return;
            }

            const int coins = loadCoins(username);
            if (coins < ShopCardCost)
            {
                sendShopPurchaseResponse(client, false, "Need 5 coins to buy a card", coins, "");
                return;
            }

            const std::vector<ShopCardEntry> shopCards = loadShopCards();
            if (shopCards.empty())
            {
                sendShopPurchaseResponse(client, false, "No cards are available in the shop", coins, "");
                return;
            }

            const std::string cardTitle = chooseShopCard(shopCards);

            SQLite::Transaction transaction(*database);
            SQLite::Statement spend(*database, "UPDATE accounts SET coins = coins - ? WHERE username = ?");
            spend.bind(1, ShopCardCost);
            spend.bind(2, username);
            spend.exec();
            addCollectionCopies(username, cardTitle, 1);
            transaction.commit();

            sendShopPurchaseResponse(
                client,
                true,
                "Added " + cardTitle + " to your collection",
                coins - ShopCardCost,
                cardTitle);
            fmt::println("User {} bought random card '{}'", username, cardTitle);
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while buying shop card: {}", error.what());
            sendShopPurchaseResponse(client, false, "Database error while buying shop card", 0, "");
        }
    }

    void handleLogin(
        sf::TcpSocket& client,
        const std::string& username,
        const std::string& password,
        bool rememberMe)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::LoginResponse);

        if (username.empty() || password.empty())
        {
            response << false << std::string("Username and password cannot be empty")
                     << std::string() << std::string();
            [[maybe_unused]] auto result = client.send(response);
            return;
        }

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            SQLite::Statement query(
                *database,
                "SELECT password_hash FROM accounts WHERE username = ?");
            query.bind(1, username);

            if (!query.executeStep())
            {
                response << false << std::string("Invalid username or password")
                         << std::string() << std::string();
            }
            else
            {
                const std::string storedHash = query.getColumn(0).getString();
                if (!verifyPassword(storedHash, password))
                {
                    response << false << std::string("Invalid username or password")
                             << std::string() << std::string();
                }
                else
                {
                    if (!isModernPasswordHash(storedHash))
                    {
                        updatePasswordHash(username, password);
                    }
                    ensureStarterInventory(username);
                    const std::string rememberToken = rememberMe ? issueRememberToken(username) : std::string();
                    response << true << std::string("Login successful") << username << rememberToken;
                    fmt::println("User logged in: {}", username);
                }
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while logging in: {}", error.what());
            response.clear();
            response << static_cast<uint8_t>(MessageType::LoginResponse);
            response << false << std::string("Database error while logging in")
                     << std::string() << std::string();
        }

        [[maybe_unused]] auto result = client.send(response);
    }

    void handleRememberLogin(sf::TcpSocket& client, const std::string& token)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::RememberLoginResponse);

        if (token.empty())
        {
            response << false << std::string("Saved login is invalid")
                     << std::string() << std::string();
            [[maybe_unused]] auto result = client.send(response);
            return;
        }

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            deleteExpiredRememberTokens();

            const std::string oldHash = hashRememberToken(token);
            SQLite::Statement query(
                *database,
                "SELECT username FROM remember_tokens "
                "WHERE token_hash = ? AND expires_at > ?");
            query.bind(1, oldHash);
            query.bind(2, unixTime());

            if (!query.executeStep())
            {
                response << false << std::string("Saved login has expired")
                         << std::string() << std::string();
            }
            else
            {
                const std::string username = query.getColumn(0).getString();
                const std::string replacement = generateRememberToken();
                const std::int64_t now = unixTime();
                SQLite::Statement rotate(
                    *database,
                    "UPDATE remember_tokens "
                    "SET token_hash = ?, expires_at = ?, last_used_at = ? "
                    "WHERE token_hash = ?");
                rotate.bind(1, hashRememberToken(replacement));
                rotate.bind(2, now + RememberTokenLifetimeSeconds);
                rotate.bind(3, now);
                rotate.bind(4, oldHash);
                rotate.exec();

                ensureStarterInventory(username);
                response << true << std::string("Login successful") << username << replacement;
                fmt::println("User restored from remembered login: {}", username);
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while restoring login: {}", error.what());
            response.clear();
            response << static_cast<uint8_t>(MessageType::RememberLoginResponse);
            response << false << std::string("Database error while restoring login")
                     << std::string() << std::string();
        }

        [[maybe_unused]] auto result = client.send(response);
    }

    void handleRevokeRememberToken(sf::TcpSocket& client, const std::string& token)
    {
        try
        {
            if (!token.empty())
            {
                std::lock_guard<std::mutex> lock(databaseMutex);
                SQLite::Statement revoke(
                    *database,
                    "DELETE FROM remember_tokens WHERE token_hash = ?");
                revoke.bind(1, hashRememberToken(token));
                revoke.exec();
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while revoking remembered login: {}", error.what());
        }

        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::RevokeRememberTokenResponse) << true;
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

    void sendAccountStateResponse(
        sf::TcpSocket& client,
        bool success,
        const std::string& message,
        int coins,
        const std::vector<account_data::CollectionCard>& collection)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::AccountStateResponse);
        response << success << message << coins;
        account_data::writeCollection(response, collection);
        [[maybe_unused]] auto result = client.send(response);
    }

    void sendCoinResponse(
        sf::TcpSocket& client,
        MessageType responseType,
        bool success,
        const std::string& message,
        int coins)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(responseType);
        response << success << message << coins;
        [[maybe_unused]] auto result = client.send(response);
    }

    void sendShopPurchaseResponse(
        sf::TcpSocket& client,
        bool success,
        const std::string& message,
        int coins,
        const std::string& cardTitle)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::ShopPurchaseResponse);
        response << success << message << coins << cardTitle;
        [[maybe_unused]] auto result = client.send(response);
    }

    bool columnExists(const std::string& tableName, const std::string& columnName)
    {
        SQLite::Statement query(*database, "PRAGMA table_info(" + tableName + ")");
        while (query.executeStep())
        {
            if (query.getColumn(1).getString() == columnName)
            {
                return true;
            }
        }
        return false;
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
            const std::int64_t deckId = query.getColumn(0).getInt64();
            deck_data::Deck deck;
            deck.name = query.getColumn(1).getString();
            deck.cardTitles = loadDeckCards(deckId);
            decks.push_back(deck);
        }

        return decks;
    }

    std::vector<std::string> loadDeckCards(std::int64_t deckId)
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

    int loadCoins(const std::string& username)
    {
        SQLite::Statement query(*database, "SELECT coins FROM accounts WHERE username = ? LIMIT 1");
        query.bind(1, username);
        if (!query.executeStep())
        {
            return 0;
        }

        return query.getColumn(0).getInt();
    }

    std::vector<account_data::CollectionCard> loadCollection(const std::string& username)
    {
        std::vector<account_data::CollectionCard> collection;
        SQLite::Statement query(
            *database,
            "SELECT card_title, copies FROM card_collections WHERE username = ? AND copies > 0 ORDER BY card_title");
        query.bind(1, username);

        while (query.executeStep())
        {
            collection.push_back({query.getColumn(0).getString(), query.getColumn(1).getInt()});
        }

        return collection;
    }

    bool collectionIsEmpty(const std::string& username)
    {
        SQLite::Statement query(
            *database,
            "SELECT 1 FROM card_collections WHERE username = ? AND copies > 0 LIMIT 1");
        query.bind(1, username);
        return !query.executeStep();
    }

    void addCollectionCopies(const std::string& username, const std::string& cardTitle, int copies)
    {
        if (cardTitle.empty() || copies <= 0)
        {
            return;
        }

        SQLite::Statement upsert(
            *database,
            "INSERT INTO card_collections (username, card_title, copies) VALUES (?, ?, ?) "
            "ON CONFLICT(username, card_title) DO UPDATE SET copies = copies + excluded.copies");
        upsert.bind(1, username);
        upsert.bind(2, cardTitle);
        upsert.bind(3, copies);
        upsert.exec();
    }

    std::optional<std::string> deckCollectionError(const std::string& username, const deck_data::Deck& deck)
    {
        std::unordered_map<std::string, int> available;
        for (const account_data::CollectionCard& card : loadCollection(username))
        {
            available[card.title] = card.copies;
        }

        std::unordered_map<std::string, int> used;
        for (const std::string& title : deck.cardTitles)
        {
            const int count = ++used[title];
            const int owned = available[title];
            if (count > owned)
            {
                return "Deck uses " + std::to_string(count) + " copies of " + title +
                    " but collection has " + std::to_string(owned);
            }
        }

        return std::nullopt;
    }

    void saveDeck(const std::string& username, const std::string& originalName, const deck_data::Deck& deck)
    {
        SQLite::Transaction transaction(*database);

        saveDeckRows(username, originalName, deck);
        transaction.commit();
    }

    void saveDeckRows(const std::string& username, const std::string& originalName, const deck_data::Deck& deck)
    {
        const std::string lookupName = originalName.empty() ? deck.name : originalName;
        std::optional<std::int64_t> deckId = findDeckId(username, lookupName);
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
    }

    bool deleteDeck(const std::string& username, const std::string& deckName)
    {
        const std::optional<std::int64_t> deckId = findDeckId(username, deckName);
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

    std::optional<std::int64_t> findDeckId(const std::string& username, const std::string& deckName)
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

    std::vector<std::string> loadCardTitlesFromCardsDb(const std::string& typeFilter)
    {
        std::vector<std::string> titles;

        try
        {
            SQLite::Database cardsDatabase("cards.db", SQLite::OPEN_READONLY);
            const std::string sql = typeFilter.empty()
                ? "SELECT title FROM cards ORDER BY title"
                : "SELECT title FROM cards WHERE type = ? ORDER BY title";
            SQLite::Statement query(cardsDatabase, sql);
            if (!typeFilter.empty())
            {
                query.bind(1, typeFilter);
            }

            while (query.executeStep())
            {
                titles.push_back(query.getColumn(0).getString());
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Could not read cards.db while building account inventory: {}", error.what());
        }

        return titles;
    }

    std::vector<std::string> loadNonHeroCardTitles()
    {
        std::vector<std::string> titles;

        try
        {
            SQLite::Database cardsDatabase("cards.db", SQLite::OPEN_READONLY);
            SQLite::Statement query(cardsDatabase, "SELECT title FROM cards WHERE type <> 'Hero' ORDER BY title");
            while (query.executeStep())
            {
                titles.push_back(query.getColumn(0).getString());
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Could not read non-hero cards from cards.db: {}", error.what());
        }

        return titles;
    }

    std::vector<std::string> loadAllCardTitles()
    {
        std::vector<std::string> titles = loadCardTitlesFromCardsDb("");
        if (!titles.empty())
        {
            return titles;
        }

        titles.push_back(PreferredStarterHero);
        const std::vector<std::string>& fallback = fallbackStarterNonHeroes();
        titles.insert(titles.end(), fallback.begin(), fallback.end());
        return titles;
    }

    std::vector<ShopCardEntry> loadShopCards()
    {
        std::vector<ShopCardEntry> cards;

        try
        {
            SQLite::Database cardsDatabase("cards.db", SQLite::OPEN_READONLY);
            SQLite::Statement query(
                cardsDatabase,
                "SELECT c.title, COALESCE(r.value, 'common') "
                "FROM cards c "
                "LEFT JOIN card_string_values r ON r.title = c.title AND r.key = 'rarity' "
                "ORDER BY c.title");

            while (query.executeStep())
            {
                cards.push_back({
                    query.getColumn(0).getString(),
                    normalizedRarity(query.getColumn(1).getString())});
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Could not read shop cards from cards.db: {}", error.what());
        }

        if (!cards.empty())
        {
            return cards;
        }

        cards.push_back({PreferredStarterHero, "legendary"});
        const std::vector<std::string>& fallback = fallbackStarterNonHeroes();
        cards.reserve(cards.size() + fallback.size());
        for (const std::string& title : fallback)
        {
            cards.push_back({title, "common"});
        }
        return cards;
    }

    std::string chooseShopCard(const std::vector<ShopCardEntry>& cards)
    {
        std::vector<ShopCardEntry> common;
        std::vector<ShopCardEntry> rare;
        std::vector<ShopCardEntry> legendary;
        for (const ShopCardEntry& card : cards)
        {
            if (card.rarity == "legendary")
            {
                legendary.push_back(card);
            }
            else if (card.rarity == "rare")
            {
                rare.push_back(card);
            }
            else
            {
                common.push_back(card);
            }
        }

        std::vector<const std::vector<ShopCardEntry>*> buckets;
        std::vector<int> weights;
        auto addBucket = [&](const std::vector<ShopCardEntry>& bucket, const std::string& rarity) {
            if (!bucket.empty())
            {
                buckets.push_back(&bucket);
                weights.push_back(shopRarityWeight(rarity));
            }
        };
        addBucket(common, "common");
        addBucket(rare, "rare");
        addBucket(legendary, "legendary");

        std::discrete_distribution<std::size_t> rarityDistribution(weights.begin(), weights.end());
        const std::vector<ShopCardEntry>& bucket = *buckets[rarityDistribution(rng)];
        std::uniform_int_distribution<std::size_t> cardDistribution(0, bucket.size() - 1);
        return bucket[cardDistribution(rng)].title;
    }

    std::string starterHeroTitle()
    {
        std::vector<std::string> heroes = loadCardTitlesFromCardsDb("Hero");
        if (heroes.empty())
        {
            return PreferredStarterHero;
        }

        const auto preferred = std::find(heroes.begin(), heroes.end(), PreferredStarterHero);
        return preferred == heroes.end() ? heroes.front() : *preferred;
    }

    std::vector<std::string> starterNonHeroSlots()
    {
        std::vector<std::string> available = loadNonHeroCardTitles();
        if (available.empty())
        {
            available = fallbackStarterNonHeroes();
        }

        std::vector<std::string> ordered;
        const std::vector<std::string>& fallback = fallbackStarterNonHeroes();
        for (const std::string& title : fallback)
        {
            if (std::find(available.begin(), available.end(), title) != available.end())
            {
                ordered.push_back(title);
            }
        }
        for (const std::string& title : available)
        {
            if (std::find(ordered.begin(), ordered.end(), title) == ordered.end())
            {
                ordered.push_back(title);
            }
        }
        if (ordered.empty())
        {
            ordered = fallback;
        }

        const std::size_t uniqueCount = ordered.size();
        for (std::size_t i = 0; ordered.size() < StarterNonHeroKinds; ++i)
        {
            ordered.push_back(ordered[i % uniqueCount]);
        }
        if (ordered.size() > StarterNonHeroKinds)
        {
            ordered.resize(StarterNonHeroKinds);
        }
        return ordered;
    }

    deck_data::Deck makeStarterDeck()
    {
        deck_data::Deck deck;
        deck.name = StarterDeckName;
        deck.cardTitles.push_back(starterHeroTitle());
        for (const std::string& title : starterNonHeroSlots())
        {
            deck.cardTitles.push_back(title);
            deck.cardTitles.push_back(title);
        }
        return deck;
    }

    void ensureStarterInventory(const std::string& username)
    {
        if (!collectionIsEmpty(username))
        {
            return;
        }

        SQLite::Transaction transaction(*database);
        const deck_data::Deck starterDeck = makeStarterDeck();
        if (!starterDeck.cardTitles.empty())
        {
            addCollectionCopies(username, starterDeck.cardTitles.front(), 1);
            for (std::size_t i = 1; i < starterDeck.cardTitles.size(); ++i)
            {
                addCollectionCopies(username, starterDeck.cardTitles[i], 1);
            }
        }

        if (loadDecks(username).empty())
        {
            saveDeckRows(username, "", starterDeck);
        }
        transaction.commit();
    }

    static std::int64_t unixTime()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    static std::string legacyPasswordHash(const std::string& password)
    {
        std::uint64_t hash = 14695981039346656037ull;
        for (unsigned char c : password)
        {
            hash ^= c;
            hash *= 1099511628211ull;
        }

        return fmt::format("{:016x}", hash);
    }

    static bool isModernPasswordHash(const std::string& hash)
    {
        return hash.starts_with("$argon2");
    }

    static std::string hashPassword(const std::string& password)
    {
        std::array<char, crypto_pwhash_STRBYTES> hash{};
        if (crypto_pwhash_str(
                hash.data(),
                password.data(),
                static_cast<unsigned long long>(password.size()),
                crypto_pwhash_OPSLIMIT_INTERACTIVE,
                crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
        {
            throw std::runtime_error("Unable to hash password");
        }
        return hash.data();
    }

    static bool verifyPassword(const std::string& storedHash, const std::string& password)
    {
        if (!isModernPasswordHash(storedHash))
        {
            const std::string expected = legacyPasswordHash(password);
            if (storedHash.size() != expected.size())
            {
                return false;
            }
            return sodium_memcmp(
                storedHash.data(),
                expected.data(),
                expected.size()) == 0;
        }

        return crypto_pwhash_str_verify(
            storedHash.c_str(),
            password.data(),
            static_cast<unsigned long long>(password.size())) == 0;
    }

    void updatePasswordHash(const std::string& username, const std::string& password)
    {
        SQLite::Statement update(
            *database,
            "UPDATE accounts SET password_hash = ? WHERE username = ?");
        update.bind(1, hashPassword(password));
        update.bind(2, username);
        update.exec();
    }

    static std::string generateRememberToken()
    {
        std::array<unsigned char, 32> bytes{};
        std::array<char, 65> encoded{};
        randombytes_buf(bytes.data(), bytes.size());
        sodium_bin2hex(encoded.data(), encoded.size(), bytes.data(), bytes.size());
        return encoded.data();
    }

    static std::string hashRememberToken(const std::string& token)
    {
        std::array<unsigned char, crypto_generichash_BYTES> hash{};
        std::array<char, crypto_generichash_BYTES * 2 + 1> encoded{};
        crypto_generichash(
            hash.data(),
            hash.size(),
            reinterpret_cast<const unsigned char*>(token.data()),
            static_cast<unsigned long long>(token.size()),
            nullptr,
            0);
        sodium_bin2hex(encoded.data(), encoded.size(), hash.data(), hash.size());
        return encoded.data();
    }

    void deleteExpiredRememberTokens()
    {
        SQLite::Statement cleanup(
            *database,
            "DELETE FROM remember_tokens WHERE expires_at <= ?");
        cleanup.bind(1, unixTime());
        cleanup.exec();
    }

    std::string issueRememberToken(const std::string& username)
    {
        deleteExpiredRememberTokens();
        const std::string token = generateRememberToken();
        const std::int64_t now = unixTime();
        SQLite::Statement insert(
            *database,
            "INSERT INTO remember_tokens "
            "(token_hash, username, expires_at, created_at, last_used_at) "
            "VALUES (?, ?, ?, ?, ?)");
        insert.bind(1, hashRememberToken(token));
        insert.bind(2, username);
        insert.bind(3, now + RememberTokenLifetimeSeconds);
        insert.bind(4, now);
        insert.bind(5, now);
        insert.exec();
        return token;
    }
};

int main()
{
    fmt::println("Starting Accounts Server...");

    AccountServer server(55000);
    server.run();

    return 0;
}
