#include <SFML/Network.hpp>
#include <SQLiteCpp/SQLiteCpp.h>
#include <fmt/core.h>
#include <sodium.h>

#include "account_catalog.hpp"
#include "account_decks.hpp"
#include "account_rate_limiter.hpp"
#include "account_security.hpp"
#include "account_tokens.hpp"

#include "../shared/account_data.hpp"
#include "../shared/deck_data.hpp"
#include "../shared/ranking.hpp"
#include "../shared/socket_timeout.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "../shared/network.hpp"

using namespace network;

namespace
{
constexpr int WinRewardCoins = 10;
constexpr int AiWinRewardCoins = 1;
constexpr int ShopCardCost = 5;
constexpr auto ClientRequestTimeout = std::chrono::seconds(30);

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
    account_rate_limiter::AccountRateLimiter loginRateLimiter;
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
        if (!columnExists("accounts", "is_admin"))
        {
            database->exec("ALTER TABLE accounts ADD COLUMN is_admin INTEGER NOT NULL DEFAULT 0");
        }
        if (!columnExists("accounts", "rating"))
        {
            database->exec("ALTER TABLE accounts ADD COLUMN rating INTEGER NOT NULL DEFAULT 0");
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
            "CREATE TABLE IF NOT EXISTS starter_deck_cards ("
            "card_index INTEGER PRIMARY KEY NOT NULL,"
            "card_title TEXT NOT NULL"
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
        database->exec(
            "CREATE TABLE IF NOT EXISTS access_tokens ("
            "token_hash TEXT PRIMARY KEY NOT NULL,"
            "username TEXT NOT NULL,"
            "expires_at INTEGER NOT NULL,"
            "created_at INTEGER NOT NULL,"
            "last_used_at INTEGER NOT NULL,"
            "FOREIGN KEY(username) REFERENCES accounts(username) ON DELETE CASCADE"
            ")");
        database->exec(
            "CREATE INDEX IF NOT EXISTS access_tokens_username_idx "
            "ON access_tokens(username)");
        database->exec(
            "CREATE TABLE IF NOT EXISTS ranked_matches ("
            "match_id INTEGER PRIMARY KEY NOT NULL,"
            "player_one TEXT NOT NULL,"
            "player_two TEXT NOT NULL,"
            "result_token TEXT NOT NULL,"
            "winner INTEGER NOT NULL DEFAULT 0,"
            "completed_at TEXT,"
            "FOREIGN KEY(player_one) REFERENCES accounts(username),"
            "FOREIGN KEY(player_two) REFERENCES accounts(username)"
            ")");
        database->exec(
            "CREATE TRIGGER IF NOT EXISTS ranked_matches_account_cleanup "
            "BEFORE DELETE ON accounts "
            "BEGIN "
            "DELETE FROM ranked_matches "
            "WHERE player_one = OLD.username OR player_two = OLD.username; "
            "END");
        account_decks::purgeTokenCards(*database);
    }

    void handleClient(std::unique_ptr<sf::TcpSocket> client)
    {
        sf::Packet packet;
        const std::string remoteAddress = client->getRemoteAddress()
            ? client->getRemoteAddress()->toString()
            : std::string("unknown");

        while (running)
        {
            if (socket_timeout::receivePacket(*client, packet, ClientRequestTimeout) != sf::Socket::Status::Done)
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
                    handleCreateAccount(*client, username, password, remoteAddress);
                    break;
                }
                case MessageType::Login:
                {
                    std::string username, password;
                    bool rememberMe = false;
                    packet >> username >> password >> rememberMe;
                    handleLogin(*client, username, password, rememberMe, remoteAddress);
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
                    std::string rememberToken, accessToken;
                    packet >> rememberToken >> accessToken;
                    handleRevokeRememberToken(*client, rememberToken, accessToken);
                    break;
                }
                case MessageType::DeckListRequest:
                {
                    std::string accessToken;
                    packet >> accessToken;
                    handleListDecks(*client, accessToken);
                    break;
                }
                case MessageType::DeckSaveRequest:
                {
                    std::string accessToken, originalName;
                    deck_data::Deck deck;
                    packet >> accessToken >> originalName;
                    if (!packet || !deck_data::readDeck(packet, deck))
                    {
                        sendDeckCommandResponse(
                            *client,
                            MessageType::DeckSaveResponse,
                            false,
                            "Invalid deck save payload");
                        break;
                    }
                    handleSaveDeck(*client, accessToken, originalName, deck);
                    break;
                }
                case MessageType::DeckDeleteRequest:
                {
                    std::string accessToken, deckName;
                    packet >> accessToken >> deckName;
                    if (!packet)
                    {
                        sendDeckCommandResponse(
                            *client,
                            MessageType::DeckDeleteResponse,
                            false,
                            "Invalid deck delete payload");
                        break;
                    }
                    handleDeleteDeck(*client, accessToken, deckName);
                    break;
                }
                case MessageType::AccountStateRequest:
                {
                    std::string accessToken;
                    packet >> accessToken;
                    handleAccountState(*client, accessToken);
                    break;
                }
                case MessageType::WinRewardRequest:
                {
                    std::string accessToken;
                    packet >> accessToken;
                    handleWinReward(*client, accessToken);
                    break;
                }
                case MessageType::ShopPurchaseRequest:
                {
                    std::string accessToken;
                    packet >> accessToken;
                    handleShopPurchase(*client, accessToken);
                    break;
                }
                case MessageType::AdminUserListRequest:
                {
                    std::string accessToken, search;
                    std::uint32_t page = 0;
                    std::uint32_t pageSize = 0;
                    packet >> accessToken >> search >> page >> pageSize;
                    handleAdminUserList(*client, accessToken, search, page, pageSize);
                    break;
                }
                case MessageType::AdminUserPrivilegeRequest:
                {
                    std::string accessToken, targetUsername;
                    bool makeAdmin = false;
                    packet >> accessToken >> targetUsername >> makeAdmin;
                    handleAdminUserPrivilege(*client, accessToken, targetUsername, makeAdmin);
                    break;
                }
                case MessageType::AdminUserGoldRequest:
                {
                    std::string accessToken, targetUsername;
                    int amount = 0;
                    packet >> accessToken >> targetUsername >> amount;
                    handleAdminUserGold(*client, accessToken, targetUsername, amount);
                    break;
                }
                case MessageType::AdminUserDeleteRequest:
                {
                    std::string accessToken, targetUsername;
                    packet >> accessToken >> targetUsername;
                    handleAdminUserDelete(*client, accessToken, targetUsername);
                    break;
                }
                case MessageType::AdminStarterDeckRequest:
                {
                    std::string accessToken;
                    packet >> accessToken;
                    handleAdminStarterDeck(*client, accessToken);
                    break;
                }
                case MessageType::AdminStarterDeckSaveRequest:
                {
                    std::string accessToken;
                    deck_data::Deck deck;
                    packet >> accessToken;
                    if (!packet || !deck_data::readDeck(packet, deck))
                    {
                        sendDeckCommandResponse(
                            *client,
                            MessageType::AdminStarterDeckSaveResponse,
                            false,
                            "Invalid starter deck payload");
                        break;
                    }
                    handleAdminStarterDeckSave(*client, accessToken, deck);
                    break;
                }
                case MessageType::ChangePasswordRequest:
                {
                    std::string accessToken, currentPassword, newPassword;
                    packet >> accessToken >> currentPassword >> newPassword;
                    if (!packet)
                    {
                        sendChangePasswordResponse(*client, false, "Invalid password change request");
                        break;
                    }
                    handleChangePassword(
                        *client,
                        accessToken,
                        currentPassword,
                        newPassword,
                        remoteAddress);
                    break;
                }
                case MessageType::RankedPlayerRequest:
                {
                    std::string accessToken;
                    packet >> accessToken;
                    handleRankedPlayerRequest(*client, accessToken);
                    break;
                }
                case MessageType::RegisterRankedMatch:
                {
                    int matchId = 0;
                    std::string playerOneToken, playerTwoToken;
                    packet >> matchId >> playerOneToken >> playerTwoToken;
                    handleRegisterRankedMatch(
                        *client, matchId, playerOneToken, playerTwoToken);
                    break;
                }
                case MessageType::SubmitRankedResult:
                {
                    int matchId = 0;
                    int winner = 0;
                    std::string resultToken;
                    packet >> matchId >> resultToken >> winner;
                    handleSubmitRankedResult(*client, matchId, resultToken, winner);
                    break;
                }
                case MessageType::SubmitAiResult:
                {
                    std::string accessToken;
                    bool humanWon = false;
                    packet >> accessToken >> humanWon;
                    handleSubmitAiResult(*client, accessToken, humanWon);
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

    void handleCreateAccount(
        sf::TcpSocket& client,
        const std::string& username,
        const std::string& password,
        const std::string& remoteAddress)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::CreateAccountResponse);

        if (loginRateLimiter.isBlocked("create:" + remoteAddress))
        {
            response << false << std::string("Too many account creation attempts; try again later");
            [[maybe_unused]] auto result = client.send(response);
            return;
        }
        loginRateLimiter.recordFailure("create:" + remoteAddress);

        if (!account_security::isValidUsername(username))
        {
            response << false << std::string(
                "Username must be 3-32 characters using letters, numbers, '_' or '-'");
            [[maybe_unused]] auto result = client.send(response);
            return;
        }

        if (!account_security::isValidNewPassword(password))
        {
            response << false << std::string("Password must be 15-128 characters");
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
                    "INSERT INTO accounts (username, password_hash, is_admin) VALUES (?, ?, 0)");
                insert.bind(1, username);
                insert.bind(2, account_security::hashPassword(password));
                insert.exec();

                account_decks::ensureStarterInventory(*database, username);
                const std::string accessToken = account_tokens::issueAccessToken(*database, username);
                response << true << std::string("Account created successfully")
                         << username << accessToken << std::string();
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

    void handleListDecks(sf::TcpSocket& client, const std::string& accessToken)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::DeckListResponse);

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username = account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username)
            {
                response << false << std::string("Authentication required");
                response << static_cast<std::uint32_t>(0);
            }
            else
            {
                const std::vector<deck_data::Deck> decks = account_decks::loadDecks(*database, *username);
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
        const std::string& accessToken,
        const std::string& originalName,
        const deck_data::Deck& deck)
    {
        if (deck.name.empty())
        {
            sendDeckCommandResponse(client, MessageType::DeckSaveResponse, false, "Deck name cannot be empty");
            return;
        }

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username = account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username)
            {
                sendDeckCommandResponse(client, MessageType::DeckSaveResponse, false, "Authentication required");
                return;
            }

            if (const std::optional<std::string> rulesError = account_decks::deckRulesError(deck))
            {
                sendDeckCommandResponse(client, MessageType::DeckSaveResponse, false, *rulesError);
                return;
            }

            if (const std::optional<std::string> collectionError =
                    account_decks::deckCollectionError(*database, *username, deck))
            {
                sendDeckCommandResponse(client, MessageType::DeckSaveResponse, false, *collectionError);
                return;
            }

            account_decks::saveDeck(*database, *username, originalName, deck);
            sendDeckCommandResponse(client, MessageType::DeckSaveResponse, true, "Deck saved");
            fmt::println("Saved deck '{}' for user {}", deck.name, *username);
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while saving deck: {}", error.what());
            sendDeckCommandResponse(client, MessageType::DeckSaveResponse, false, "Database error while saving deck");
        }
    }

    void handleDeleteDeck(sf::TcpSocket& client, const std::string& accessToken, const std::string& deckName)
    {
        if (deckName.empty())
        {
            sendDeckCommandResponse(client, MessageType::DeckDeleteResponse, false, "Deck name cannot be empty");
            return;
        }

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username = account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username)
            {
                sendDeckCommandResponse(client, MessageType::DeckDeleteResponse, false, "Authentication required");
                return;
            }

            if (!account_decks::deleteDeck(*database, *username, deckName))
            {
                sendDeckCommandResponse(client, MessageType::DeckDeleteResponse, false, "Deck not found");
                return;
            }

            sendDeckCommandResponse(client, MessageType::DeckDeleteResponse, true, "Deck deleted");
            fmt::println("Deleted deck '{}' for user {}", deckName, *username);
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while deleting deck: {}", error.what());
            sendDeckCommandResponse(client, MessageType::DeckDeleteResponse, false, "Database error while deleting deck");
        }
    }

    void handleAccountState(sf::TcpSocket& client, const std::string& accessToken)
    {
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username = account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username)
            {
                sendAccountStateResponse(client, false, "Authentication required", 0, 0, false, {});
                return;
            }

            account_decks::ensureStarterInventory(*database, *username);
            sendAccountStateResponse(
                client,
                true,
                "Account loaded",
                loadCoins(*username),
                loadRating(*username),
                isAdmin(*username),
                account_decks::loadCollection(*database, *username));
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while loading account state: {}", error.what());
            sendAccountStateResponse(
                client, false, "Database error while loading account state", 0, 0, false, {});
        }
    }

    void handleRankedPlayerRequest(sf::TcpSocket& client, const std::string& accessToken)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::RankedPlayerResponse);
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username = account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username)
            {
                response << false << std::string("Authentication required")
                         << std::string() << 0;
            }
            else
            {
                response << true << std::string("Player authenticated")
                         << *username << loadRating(*username);
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while authenticating ranked player: {}", error.what());
            response.clear();
            response << static_cast<uint8_t>(MessageType::RankedPlayerResponse)
                     << false << std::string("Database error")
                     << std::string() << 0;
        }
        [[maybe_unused]] auto result = client.send(response);
    }

    void handleRegisterRankedMatch(
        sf::TcpSocket& client,
        int matchId,
        const std::string& playerOneToken,
        const std::string& playerTwoToken)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::RegisterRankedMatchResponse);
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> playerOne =
                account_tokens::authenticateAccessToken(*database, playerOneToken);
            const std::optional<std::string> playerTwo =
                account_tokens::authenticateAccessToken(*database, playerTwoToken);
            if (!playerOne || !playerTwo)
            {
                response << false << std::string("Invalid ranked match players")
                         << std::string() << std::string() << std::string();
            }
            else
            {
                const std::string resultToken = account_security::generateToken();
                SQLite::Statement insert(
                    *database,
                    "INSERT INTO ranked_matches "
                    "(match_id, player_one, player_two, result_token) VALUES (?, ?, ?, ?)");
                insert.bind(1, matchId);
                insert.bind(2, *playerOne);
                insert.bind(3, *playerTwo);
                insert.bind(4, account_security::hashToken(resultToken));
                insert.exec();
                response << true << std::string("Ranked match registered")
                         << *playerOne << *playerTwo << resultToken;
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while registering ranked match: {}", error.what());
            response.clear();
            response << static_cast<uint8_t>(MessageType::RegisterRankedMatchResponse)
                     << false << std::string("Could not register ranked match")
                     << std::string() << std::string() << std::string();
        }
        [[maybe_unused]] auto result = client.send(response);
    }

    void handleSubmitRankedResult(
        sf::TcpSocket& client,
        int matchId,
        const std::string& resultToken,
        int winner)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::SubmitRankedResultResponse);
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            if (winner != 1 && winner != 2)
            {
                response << false << std::string("Invalid winner")
                         << 0 << 0 << 0 << 0 << 0 << false;
                [[maybe_unused]] auto result = client.send(response);
                return;
            }

            std::string playerOne;
            std::string playerTwo;
            {
                SQLite::Statement matchQuery(
                    *database,
                    "SELECT player_one, player_two, result_token, winner "
                    "FROM ranked_matches WHERE match_id = ?");
                matchQuery.bind(1, matchId);
                if (!matchQuery.executeStep() ||
                    matchQuery.getColumn(2).getString() != account_security::hashToken(resultToken) ||
                    matchQuery.getColumn(3).getInt() != 0)
                {
                    response << false << std::string("Ranked match is invalid or already completed")
                             << 0 << 0 << 0 << 0 << 0 << false;
                    [[maybe_unused]] auto result = client.send(response);
                    return;
                }
                playerOne = matchQuery.getColumn(0).getString();
                playerTwo = matchQuery.getColumn(1).getString();
            }

            const int oldRatingOne = loadRating(playerOne);
            const int oldRatingTwo = loadRating(playerTwo);
            const bool selfMatch = playerOne == playerTwo;
            const ranking::MatchRewards rewards = ranking::rewardsAfterMatch(
                oldRatingOne,
                oldRatingTwo,
                winner,
                selfMatch,
                WinRewardCoins);

            SQLite::Transaction transaction(*database);
            if (!selfMatch)
            {
                SQLite::Statement updateRating(
                    *database, "UPDATE accounts SET rating = ? WHERE username = ?");
                updateRating.bind(1, rewards.ratings[0]);
                updateRating.bind(2, playerOne);
                updateRating.exec();
                updateRating.reset();
                updateRating.bind(1, rewards.ratings[1]);
                updateRating.bind(2, playerTwo);
                updateRating.exec();

                SQLite::Statement awardWinner(
                    *database, "UPDATE accounts SET coins = coins + ? WHERE username = ?");
                awardWinner.bind(1, rewards.winnerCoins);
                awardWinner.bind(2, winner == 1 ? playerOne : playerTwo);
                awardWinner.exec();
            }

            SQLite::Statement complete(
                *database,
                "UPDATE ranked_matches "
                "SET winner = ?, completed_at = CURRENT_TIMESTAMP WHERE match_id = ?");
            complete.bind(1, winner);
            complete.bind(2, matchId);
            complete.exec();
            transaction.commit();

            response << true
                     << std::string(selfMatch ? "Self-match completed" : "Match rewards updated")
                     << rewards.ratings[0] << rewards.ratings[1]
                     << rewards.ratingChanges[0] << rewards.ratingChanges[1]
                     << rewards.winnerCoins << selfMatch;
            fmt::println(
                "Ranked match {} complete: {}={} ({:+}) {}={} ({:+}), winner coins {}{}",
                matchId,
                playerOne,
                rewards.ratings[0],
                rewards.ratingChanges[0],
                playerTwo,
                rewards.ratings[1],
                rewards.ratingChanges[1],
                rewards.winnerCoins,
                selfMatch ? " (self-match)" : "");
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while submitting ranked result: {}", error.what());
            response.clear();
            response << static_cast<uint8_t>(MessageType::SubmitRankedResultResponse)
                     << false << std::string("Could not update match rewards")
                     << 0 << 0 << 0 << 0 << 0 << false;
        }
        [[maybe_unused]] auto result = client.send(response);
    }

    void handleSubmitAiResult(sf::TcpSocket& client, const std::string& accessToken, bool humanWon)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::SubmitAiResultResponse);
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username = account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username)
            {
                response << false << std::string("Authentication required") << 0;
                [[maybe_unused]] auto result = client.send(response);
                return;
            }

            const int coinsAwarded = humanWon ? AiWinRewardCoins : 0;
            if (coinsAwarded > 0)
            {
                SQLite::Statement awardWinner(
                    *database, "UPDATE accounts SET coins = coins + ? WHERE username = ?");
                awardWinner.bind(1, coinsAwarded);
                awardWinner.bind(2, *username);
                awardWinner.exec();
            }

            response << true
                     << std::string(humanWon ? "Victory over the AI!" : "Defeated by the AI.")
                     << coinsAwarded;
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while submitting AI result: {}", error.what());
            response.clear();
            response << static_cast<uint8_t>(MessageType::SubmitAiResultResponse)
                     << false << std::string("Could not update match rewards") << 0;
        }
        [[maybe_unused]] auto result = client.send(response);
    }

    void handleWinReward(sf::TcpSocket& client, const std::string& accessToken)
    {
        (void)accessToken;
        sendCoinResponse(
            client,
            MessageType::WinRewardResponse,
            false,
            "Win rewards are awarded by the game server",
            0);
    }

    void handleShopPurchase(sf::TcpSocket& client, const std::string& accessToken)
    {
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username = account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username)
            {
                sendShopPurchaseResponse(client, false, "Authentication required", 0, "");
                return;
            }

            const int coins = loadCoins(*username);
            if (coins < ShopCardCost)
            {
                sendShopPurchaseResponse(client, false, "Need 5 coins to buy a card", coins, "");
                return;
            }

            const std::vector<account_catalog::ShopCardEntry> shopCards = account_catalog::loadShopCards();
            if (shopCards.empty())
            {
                sendShopPurchaseResponse(client, false, "No cards are available in the shop", coins, "");
                return;
            }

            const std::string cardTitle = account_catalog::chooseShopCard(shopCards, rng);

            SQLite::Transaction transaction(*database);
            SQLite::Statement spend(*database, "UPDATE accounts SET coins = coins - ? WHERE username = ?");
            spend.bind(1, ShopCardCost);
            spend.bind(2, *username);
            spend.exec();
            account_decks::addCollectionCopies(*database, *username, cardTitle, 1);
            transaction.commit();

            sendShopPurchaseResponse(
                client,
                true,
                "Added " + cardTitle + " to your collection",
                coins - ShopCardCost,
                cardTitle);
            fmt::println("User {} bought random card '{}'", *username, cardTitle);
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while buying shop card: {}", error.what());
            sendShopPurchaseResponse(client, false, "Database error while buying shop card", 0, "");
        }
    }

    void handleAdminUserList(
        sf::TcpSocket& client,
        const std::string& accessToken,
        const std::string& search,
        std::uint32_t page,
        std::uint32_t pageSize)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::AdminUserListResponse);

        const std::uint32_t safePageSize = std::clamp<std::uint32_t>(pageSize == 0 ? 25 : pageSize, 1, 100);
        const std::uint32_t safePage = page;
        const std::string like = "%" + search + "%";

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username = account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username || !isAdmin(*username))
            {
                response << false << std::string("Admin access required")
                         << static_cast<std::uint32_t>(0) << safePage << safePageSize
                         << static_cast<std::uint32_t>(0);
                [[maybe_unused]] auto result = client.send(response);
                return;
            }

            SQLite::Statement countQuery(*database,
                "SELECT COUNT(*) FROM accounts WHERE username LIKE ? COLLATE NOCASE");
            countQuery.bind(1, like);
            const std::uint32_t totalCount = countQuery.executeStep()
                ? static_cast<std::uint32_t>(countQuery.getColumn(0).getInt())
                : 0;

            SQLite::Statement query(*database,
                "SELECT username, is_admin, coins FROM accounts "
                "WHERE username LIKE ? COLLATE NOCASE "
                "ORDER BY username COLLATE NOCASE "
                "LIMIT ? OFFSET ?");
            query.bind(1, like);
            query.bind(2, static_cast<int>(safePageSize));
            query.bind(3, static_cast<int>(safePage * safePageSize));

            std::uint32_t rowCount = 0;
            std::vector<network::AdminUserSummary> users;
            while (query.executeStep())
            {
                users.push_back({
                    query.getColumn(0).getString(),
                    query.getColumn(1).getInt() != 0,
                    query.getColumn(2).getInt()});
                ++rowCount;
            }
            response << true << std::string("Users loaded") << totalCount << safePage << safePageSize << rowCount;
            for (const auto& user : users)
            {
                response << user.username << user.isAdmin << user.gold;
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while loading admin users: {}", error.what());
            response.clear();
            response << static_cast<uint8_t>(MessageType::AdminUserListResponse);
            response << false << std::string("Database error while loading users")
                     << static_cast<std::uint32_t>(0) << safePage << safePageSize << static_cast<std::uint32_t>(0);
        }

        [[maybe_unused]] auto result = client.send(response);
    }

    void handleChangePassword(
        sf::TcpSocket& client,
        const std::string& accessToken,
        const std::string& currentPassword,
        const std::string& newPassword,
        const std::string& remoteAddress)
    {
        if (currentPassword.empty() || currentPassword.size() > account_security::MaximumPasswordLength)
        {
            sendChangePasswordResponse(client, false, "Current password is incorrect");
            return;
        }
        if (!account_security::isValidNewPassword(newPassword))
        {
            sendChangePasswordResponse(client, false, "New password must be 15-128 characters");
            return;
        }

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username = account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username)
            {
                sendChangePasswordResponse(client, false, "Authentication required");
                return;
            }

            const std::string userRateLimitKey = "password-change-user:" + *username;
            const std::string addressRateLimitKey = "password-change-address:" + remoteAddress;
            if (loginRateLimiter.isBlocked(userRateLimitKey) || loginRateLimiter.isBlocked(addressRateLimitKey))
            {
                sendChangePasswordResponse(client, false, "Too many password attempts; try again later");
                return;
            }

            std::string storedHash;
            {
                SQLite::Statement query(
                    *database,
                    "SELECT password_hash FROM accounts WHERE username = ?");
                query.bind(1, *username);
                if (query.executeStep())
                {
                    storedHash = query.getColumn(0).getString();
                }
            }
            if (storedHash.empty() || !account_security::verifyPassword(storedHash, currentPassword))
            {
                loginRateLimiter.recordFailure(userRateLimitKey);
                loginRateLimiter.recordFailure(addressRateLimitKey, account_rate_limiter::MaximumLoginFailuresPerAddress);
                sendChangePasswordResponse(client, false, "Current password is incorrect");
                return;
            }

            if (currentPassword == newPassword)
            {
                sendChangePasswordResponse(
                    client,
                    false,
                    "New password must be different from the current password");
                return;
            }

            SQLite::Transaction transaction(*database);
            updatePasswordHash(*username, newPassword);

            SQLite::Statement deleteRememberTokens(
                *database,
                "DELETE FROM remember_tokens WHERE username = ?");
            deleteRememberTokens.bind(1, *username);
            deleteRememberTokens.exec();

            SQLite::Statement deleteOtherAccessTokens(
                *database,
                "DELETE FROM access_tokens WHERE username = ? AND token_hash <> ?");
            deleteOtherAccessTokens.bind(1, *username);
            deleteOtherAccessTokens.bind(2, account_security::hashToken(accessToken));
            deleteOtherAccessTokens.exec();
            transaction.commit();

            loginRateLimiter.clearFailures(userRateLimitKey);
            loginRateLimiter.clearFailures(addressRateLimitKey);
            fmt::println("Password changed for user: {}", *username);
            sendChangePasswordResponse(client, true, "Password changed successfully");
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while changing password: {}", error.what());
            sendChangePasswordResponse(client, false, "Database error while changing password");
        }
    }

    void handleAdminUserPrivilege(
        sf::TcpSocket& client,
        const std::string& accessToken,
        const std::string& targetUsername,
        bool makeAdmin)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::AdminUserPrivilegeResponse);

        if (targetUsername.empty())
        {
            response << false << std::string("Target username cannot be empty") << false;
            [[maybe_unused]] auto result = client.send(response);
            return;
        }

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username = account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username || !isAdmin(*username))
            {
                response << false << std::string("Admin access required") << false;
                [[maybe_unused]] auto result = client.send(response);
                return;
            }

            if (!makeAdmin && targetUsername == *username)
            {
                response << false << std::string("You cannot revoke your own admin privilege") << true;
                [[maybe_unused]] auto result = client.send(response);
                return;
            }

            if (!accountExists(targetUsername))
            {
                response << false << std::string("Username not found") << false;
            }
            else
            {
                SQLite::Statement update(*database, "UPDATE accounts SET is_admin = ? WHERE username = ?");
                update.bind(1, makeAdmin ? 1 : 0);
                update.bind(2, targetUsername);
                update.exec();
                response << true << (makeAdmin ? std::string("Admin privilege granted") : std::string("Admin privilege revoked"))
                         << makeAdmin;
                fmt::println("{} admin privilege for {}", makeAdmin ? "Granted" : "Revoked", targetUsername);
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while updating admin privilege: {}", error.what());
            response.clear();
            response << static_cast<uint8_t>(MessageType::AdminUserPrivilegeResponse);
            response << false << std::string("Database error while updating admin privilege") << false;
        }

        [[maybe_unused]] auto result = client.send(response);
    }

    void handleAdminUserGold(
        sf::TcpSocket& client,
        const std::string& accessToken,
        const std::string& targetUsername,
        int amount)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::AdminUserGoldResponse);

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username = account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username || !isAdmin(*username))
            {
                response << false << std::string("Admin access required") << 0;
            }
            else if (targetUsername.empty())
            {
                response << false << std::string("Target username cannot be empty") << 0;
            }
            else if (!accountExists(targetUsername))
            {
                response << false << std::string("Username not found") << 0;
            }
            else if (amount == 0)
            {
                response << false << std::string("Gold amount must not be zero") << loadCoins(targetUsername);
            }
            else
            {
                const int currentGold = loadCoins(targetUsername);
                const std::int64_t updatedGold =
                    static_cast<std::int64_t>(currentGold) + static_cast<std::int64_t>(amount);
                if (updatedGold < 0)
                {
                    response << false << std::string("Cannot remove more gold than the player has") << currentGold;
                }
                else if (updatedGold > std::numeric_limits<int>::max())
                {
                    response << false << std::string("Gold balance is too large") << currentGold;
                }
                else
                {
                    SQLite::Statement update(*database, "UPDATE accounts SET coins = ? WHERE username = ?");
                    update.bind(1, static_cast<int>(updatedGold));
                    update.bind(2, targetUsername);
                    update.exec();
                    response << true
                             << (amount > 0 ? std::string("Gold granted") : std::string("Gold removed"))
                             << static_cast<int>(updatedGold);
                    fmt::println(
                        "{} adjusted gold for {} by {:+}; new balance {}",
                        *username,
                        targetUsername,
                        amount,
                        updatedGold);
                }
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while updating player gold: {}", error.what());
            response.clear();
            response << static_cast<uint8_t>(MessageType::AdminUserGoldResponse);
            response << false << std::string("Database error while updating player gold") << 0;
        }

        [[maybe_unused]] auto result = client.send(response);
    }

    void handleAdminUserDelete(
        sf::TcpSocket& client,
        const std::string& accessToken,
        const std::string& targetUsername)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::AdminUserDeleteResponse);

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username = account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username || !isAdmin(*username))
            {
                response << false << std::string("Admin access required");
            }
            else if (targetUsername.empty())
            {
                response << false << std::string("Target username cannot be empty");
            }
            else if (targetUsername == *username)
            {
                response << false << std::string("You cannot delete your own account");
            }
            else if (!accountExists(targetUsername))
            {
                response << false << std::string("Username not found");
            }
            else
            {
                // Decks, card collections, and saved login tokens are removed via the
                // ON DELETE CASCADE foreign keys defined in initializeDatabase.
                SQLite::Statement remove(*database, "DELETE FROM accounts WHERE username = ?");
                remove.bind(1, targetUsername);
                remove.exec();
                response << true << std::string("User deleted");
                fmt::println("{} deleted account {}", *username, targetUsername);
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while deleting user: {}", error.what());
            response.clear();
            response << static_cast<uint8_t>(MessageType::AdminUserDeleteResponse);
            response << false << std::string("Database error while deleting user");
        }

        [[maybe_unused]] auto result = client.send(response);
    }

    void handleAdminStarterDeck(sf::TcpSocket& client, const std::string& accessToken)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::AdminStarterDeckResponse);

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username = account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username || !isAdmin(*username))
            {
                response << false << std::string("Admin access required");
                deck_data::writeDeck(response, {});
            }
            else
            {
                const deck_data::Deck starterDeck =
                    account_decks::loadStarterDeckOverride(*database)
                        .value_or(account_catalog::makeStarterDeck());
                response << true << std::string("Starter deck loaded");
                deck_data::writeDeck(response, starterDeck);
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while loading starter deck: {}", error.what());
            response.clear();
            response << static_cast<uint8_t>(MessageType::AdminStarterDeckResponse);
            response << false << std::string("Database error while loading starter deck");
            deck_data::writeDeck(response, {});
        }

        [[maybe_unused]] auto result = client.send(response);
    }

    void handleAdminStarterDeckSave(sf::TcpSocket& client, const std::string& accessToken, const deck_data::Deck& deck)
    {
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username = account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username || !isAdmin(*username))
            {
                sendDeckCommandResponse(client, MessageType::AdminStarterDeckSaveResponse, false, "Admin access required");
                return;
            }

            deck_data::Deck starterDeck = deck;
            starterDeck.name = account_catalog::StarterDeckName;
            // The starter deck is granted to every new account, so it must always
            // be a playable deck; reject rule violations instead of storing them.
            if (const std::optional<std::string> rulesError = account_decks::deckRulesError(starterDeck))
            {
                sendDeckCommandResponse(client, MessageType::AdminStarterDeckSaveResponse, false, *rulesError);
                return;
            }

            account_decks::saveStarterDeckOverride(*database, starterDeck);
            sendDeckCommandResponse(client, MessageType::AdminStarterDeckSaveResponse, true, "Starter deck saved");
            fmt::println("{} updated the starter deck ({} cards)", *username, starterDeck.cardTitles.size());
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while saving starter deck: {}", error.what());
            sendDeckCommandResponse(client, MessageType::AdminStarterDeckSaveResponse, false, "Database error while saving starter deck");
        }
    }

    void handleLogin(
        sf::TcpSocket& client,
        const std::string& username,
        const std::string& password,
        bool rememberMe,
        const std::string& remoteAddress)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::LoginResponse);
        std::string normalizedUsername = username;
        std::transform(
            normalizedUsername.begin(),
            normalizedUsername.end(),
            normalizedUsername.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        const std::string userRateLimitKey = "login-user:" + normalizedUsername;
        const std::string addressRateLimitKey = "login-address:" + remoteAddress;

        if (loginRateLimiter.isBlocked(userRateLimitKey) || loginRateLimiter.isBlocked(addressRateLimitKey))
        {
            response << false << std::string("Too many login attempts; try again later")
                     << std::string() << std::string() << std::string();
            [[maybe_unused]] auto result = client.send(response);
            return;
        }

        if (username.empty() || password.empty() || password.size() > account_security::MaximumPasswordLength)
        {
            response << false << std::string("Username and password cannot be empty")
                     << std::string() << std::string() << std::string();
            loginRateLimiter.recordFailure(userRateLimitKey, account_rate_limiter::MaximumLoginFailures);
            loginRateLimiter.recordFailure(addressRateLimitKey, account_rate_limiter::MaximumLoginFailuresPerAddress);
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
                [[maybe_unused]] const bool ignored = account_security::verifyPassword(account_security::dummyPasswordHash(), password);
                loginRateLimiter.recordFailure(userRateLimitKey, account_rate_limiter::MaximumLoginFailures);
                loginRateLimiter.recordFailure(addressRateLimitKey, account_rate_limiter::MaximumLoginFailuresPerAddress);
                response << false << std::string("Invalid username or password")
                         << std::string() << std::string() << std::string();
            }
            else
            {
                const std::string storedHash = query.getColumn(0).getString();
                if (!account_security::verifyPassword(storedHash, password))
                {
                    loginRateLimiter.recordFailure(userRateLimitKey, account_rate_limiter::MaximumLoginFailures);
                    loginRateLimiter.recordFailure(addressRateLimitKey, account_rate_limiter::MaximumLoginFailuresPerAddress);
                    response << false << std::string("Invalid username or password")
                             << std::string() << std::string() << std::string();
                }
                else
                {
                    loginRateLimiter.clearFailures(userRateLimitKey);
                    if (account_security::passwordHashNeedsUpgrade(storedHash))
                    {
                        updatePasswordHash(username, password);
                    }
                    account_decks::ensureStarterInventory(*database, username);
                    const std::string accessToken = account_tokens::issueAccessToken(*database, username);
                    const std::string rememberToken = rememberMe ? account_tokens::issueRememberToken(*database, username) : std::string();
                    response << true << std::string("Login successful") << username << accessToken << rememberToken;
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
                     << std::string() << std::string() << std::string();
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
                     << std::string() << std::string() << std::string();
            [[maybe_unused]] auto result = client.send(response);
            return;
        }

        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<account_tokens::RememberLogin> remembered =
                account_tokens::rotateRememberToken(*database, token);
            if (!remembered)
            {
                response << false << std::string("Saved login has expired")
                         << std::string() << std::string() << std::string();
            }
            else
            {
                account_decks::ensureStarterInventory(*database, remembered->username);
                const std::string accessToken =
                    account_tokens::issueAccessToken(*database, remembered->username);
                response << true << std::string("Login successful")
                         << remembered->username << accessToken << remembered->replacementToken;
                fmt::println("User restored from remembered login: {}", remembered->username);
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while restoring login: {}", error.what());
            response.clear();
            response << static_cast<uint8_t>(MessageType::RememberLoginResponse);
            response << false << std::string("Database error while restoring login")
                     << std::string() << std::string() << std::string();
        }

        [[maybe_unused]] auto result = client.send(response);
    }

    void handleRevokeRememberToken(
        sf::TcpSocket& client,
        const std::string& rememberToken,
        const std::string& accessToken)
    {
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            account_tokens::revokeRememberToken(*database, rememberToken);
            account_tokens::revokeAccessToken(*database, accessToken);
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

    bool isAdmin(const std::string& username)
    {
        SQLite::Statement query(*database, "SELECT is_admin FROM accounts WHERE username = ? LIMIT 1");
        query.bind(1, username);
        if (!query.executeStep())
        {
            return false;
        }

        return query.getColumn(0).getInt() != 0;
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
        int rating,
        bool isAdmin,
        const std::vector<account_data::CollectionCard>& collection)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::AccountStateResponse);
        response << success << message;
        account_data::writeAccountState(
            response, account_data::AccountState{coins, rating, isAdmin, collection});
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

    int loadRating(const std::string& username)
    {
        SQLite::Statement query(*database, "SELECT rating FROM accounts WHERE username = ? LIMIT 1");
        query.bind(1, username);
        if (!query.executeStep())
        {
            return 0;
        }

        return std::max(0, query.getColumn(0).getInt());
    }

    void updatePasswordHash(const std::string& username, const std::string& password)
    {
        SQLite::Statement update(
            *database,
            "UPDATE accounts SET password_hash = ? WHERE username = ?");
        update.bind(1, account_security::hashPassword(password));
        update.bind(2, username);
        update.exec();
    }

    static void sendChangePasswordResponse(
        sf::TcpSocket& client,
        bool success,
        const std::string& message)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::ChangePasswordResponse)
                 << success << message;
        [[maybe_unused]] auto result = client.send(response);
    }

};

int main()
{
    fmt::println("Starting Accounts Server...");

    AccountServer server(55000);
    server.run();

    return 0;
}
