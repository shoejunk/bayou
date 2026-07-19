#include <SFML/Network.hpp>
#include "tls_socket.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <fmt/core.h>
#include <sodium.h>

#include "account_catalog.hpp"
#include "account_conquest.hpp"
#include "account_conquest_events.hpp"
#include "account_decks.hpp"
#include "account_rate_limiter.hpp"
#include "account_security.hpp"
#include "account_tokens.hpp"

#include "../shared/account_data.hpp"
#include "../shared/card_server_client.hpp"
#include "../shared/card_source_config.hpp"
#include "../shared/conquest_data.hpp"
#include "../shared/conquest_event_data.hpp"
#include "../shared/deck_data.hpp"
#include "../shared/listener_retry.hpp"
#include "../shared/ranking.hpp"
#include "../shared/socket_timeout.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "../shared/network.hpp"

using namespace network;

namespace
{
constexpr int WinRewardCoins = 10;
constexpr int AiWinRewardCoins = 1;
constexpr int ShopCardCost = 5;
constexpr auto ClientRequestTimeout = std::chrono::seconds(30);

bool isLoopbackAddress(const std::string& address)
{
    return address == "127.0.0.1" || address == "::1" ||
        address == "0:0:0:0:0:0:0:1" || address == "::ffff:127.0.0.1";
}

}

class AccountServer
{
public:
    AccountServer(unsigned short port)
        : listener(std::make_unique<bayou::tls::Listener>())
    {
        if (sodium_init() < 0)
        {
            fmt::println("Failed to initialize cryptography");
            return;
        }

        try
        {
            database = std::make_unique<SQLite::Database>("accounts.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
            database->setBusyTimeout(5000);
            initializeDatabase();
            fmt::println("Using accounts database: accounts.db");
        }
        catch (const std::exception& error)
        {
            fmt::println("Failed to initialize accounts database: {}", error.what());
            return;
        }

        if (!listener_retry::listenWithRetry(*listener, port))
        {
            return;
        }
        listening = true;
        fmt::println("Account server listening on port {}", port);
    }

    bool isListening() const
    {
        return listening;
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
            auto client = std::make_unique<bayou::tls::Socket>();
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
        conquestChanged.notify_all();
        listener->close();
    }

private:
    std::unique_ptr<bayou::tls::Listener> listener;
    std::unique_ptr<SQLite::Database> database;
    std::mutex databaseMutex;
    std::mutex conquestChangeMutex;
    std::condition_variable conquestChanged;
    std::atomic<std::uint64_t> conquestChangeGeneration{0};
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
            "CREATE TABLE IF NOT EXISTS conquest_decks ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "username TEXT NOT NULL,"
            "name TEXT NOT NULL,"
            "revision INTEGER NOT NULL DEFAULT 1 CHECK(revision > 0),"
            "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "UNIQUE(username, name),"
            "UNIQUE(username, id),"
            "FOREIGN KEY(username) REFERENCES accounts(username) ON DELETE CASCADE"
            ")");
        database->exec(
            "CREATE TABLE IF NOT EXISTS conquest_deck_cards ("
            "deck_id INTEGER NOT NULL,"
            "card_index INTEGER NOT NULL CHECK(card_index >= 0),"
            "card_title TEXT NOT NULL,"
            "PRIMARY KEY(deck_id, card_index),"
            "FOREIGN KEY(deck_id) REFERENCES conquest_decks(id) ON DELETE CASCADE"
            ")");
        database->exec(
            "CREATE INDEX IF NOT EXISTS conquest_deck_cards_title_idx "
            "ON conquest_deck_cards(card_title)");
        database->exec(
            "CREATE TABLE IF NOT EXISTS conquest_armies ("
            "username TEXT PRIMARY KEY NOT NULL,"
            "revision INTEGER NOT NULL DEFAULT 1 CHECK(revision > 0),"
            "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "FOREIGN KEY(username) REFERENCES accounts(username) ON DELETE CASCADE"
            ")");
        database->exec(
            "CREATE TABLE IF NOT EXISTS conquest_army_decks ("
            "username TEXT NOT NULL,"
            "slot_index INTEGER NOT NULL CHECK(slot_index >= 0 AND slot_index < 10),"
            "deck_id INTEGER NOT NULL,"
            "PRIMARY KEY(username, slot_index),"
            "UNIQUE(username, deck_id),"
            "FOREIGN KEY(username) REFERENCES conquest_armies(username) ON DELETE CASCADE,"
            "FOREIGN KEY(username, deck_id) "
            "REFERENCES conquest_decks(username, id) ON DELETE CASCADE"
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
        account_conquest_events::initializeSchema(*database);
        account_decks::purgeTokenCards(*database);
        account_conquest::purgeTokenCards(*database);
    }

    void handleClient(std::unique_ptr<bayou::tls::Socket> client)
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
                case MessageType::AdminUserCardRequest:
                {
                    std::string accessToken, targetUsername, cardTitle;
                    packet >> accessToken >> targetUsername >> cardTitle;
                    handleAdminUserCard(*client, accessToken, targetUsername, cardTitle);
                    break;
                }
                case MessageType::ConquestLoadoutRequest:
                {
                    std::string accessToken;
                    packet >> accessToken;
                    if (!packet)
                    {
                        sendConquestLoadoutResponse(
                            *client, false, "Invalid Conquest loadout request", {}, {});
                        break;
                    }
                    handleConquestLoadout(*client, accessToken);
                    break;
                }
                case MessageType::ConquestDeckSaveRequest:
                {
                    std::string accessToken;
                    conquest_data::ConquestDeck deck;
                    packet >> accessToken;
                    if (!packet || !conquest_data::readConquestDeck(packet, deck))
                    {
                        sendConquestDeckResponse(
                            *client, false, "Invalid Conquest deck save payload", {});
                        break;
                    }
                    handleConquestDeckSave(*client, accessToken, std::move(deck));
                    break;
                }
                case MessageType::ConquestDeckDeleteRequest:
                {
                    std::string accessToken;
                    std::int64_t deckId = 0;
                    std::uint32_t revision = 0;
                    packet >> accessToken >> deckId >> revision;
                    if (!packet)
                    {
                        sendDeckCommandResponse(
                            *client,
                            MessageType::ConquestDeckDeleteResponse,
                            false,
                            "Invalid Conquest deck delete payload");
                        break;
                    }
                    handleConquestDeckDelete(*client, accessToken, deckId, revision);
                    break;
                }
                case MessageType::ConquestArmySaveRequest:
                {
                    std::string accessToken;
                    conquest_data::ConquestArmy army;
                    packet >> accessToken;
                    if (!packet || !conquest_data::readConquestArmy(packet, army))
                    {
                        sendConquestArmyResponse(
                            *client, false, "Invalid Conquest army save payload", {});
                        break;
                    }
                    handleConquestArmySave(*client, accessToken, std::move(army));
                    break;
                }
                case MessageType::ConquestEventListRequest:
                {
                    std::string accessToken;
                    packet >> accessToken;
                    handleConquestEventList(*client, accessToken);
                    break;
                }
                case MessageType::ConquestEventStateRequest:
                {
                    std::string accessToken;
                    std::uint64_t eventId = 0;
                    packet >> accessToken >> eventId;
                    handleConquestEventState(*client, accessToken, eventId);
                    break;
                }
                case MessageType::ConquestEventWatchRequest:
                {
                    std::string accessToken;
                    std::uint64_t eventId = 0;
                    std::uint64_t stateFingerprint = 0;
                    packet >> accessToken >> eventId >> stateFingerprint;
                    if (!packet)
                    {
                        sendConquestCommandResponse(
                            *client, MessageType::ConquestEventWatchResponse,
                            false, "Invalid Conquest event watch request");
                        break;
                    }
                    handleConquestEventWatch(
                        *client, accessToken, eventId, stateFingerprint);
                    break;
                }
                case MessageType::ConquestEventJoinRequest:
                {
                    std::string accessToken;
                    std::uint64_t eventId = 0;
                    std::uint32_t count = 0;
                    packet >> accessToken >> eventId >> count;
                    std::vector<conquest_data::StartingPlacement> placements;
                    if (!packet || count == 0 || count > 2)
                    {
                        sendConquestCommandResponse(
                            *client, MessageType::ConquestEventJoinResponse,
                            false, "Invalid Conquest placement payload");
                        break;
                    }
                    placements.resize(count);
                    bool valid = true;
                    for (conquest_data::StartingPlacement& placement : placements)
                    {
                        valid = valid && conquest_data::readStartingPlacement(packet, placement);
                    }
                    if (!valid)
                    {
                        sendConquestCommandResponse(
                            *client, MessageType::ConquestEventJoinResponse,
                            false, "Invalid Conquest placement payload");
                        break;
                    }
                    handleConquestEventJoin(
                        *client, accessToken, eventId, placements);
                    notifyConquestChanged();
                    break;
                }
                case MessageType::ConquestOrdersSubmitRequest:
                {
                    std::string accessToken;
                    std::uint64_t eventId = 0;
                    std::uint32_t count = 0;
                    packet >> accessToken >> eventId >> count;
                    std::vector<conquest_data::MoveOrder> orders;
                    if (!packet || count > conquest_data::MaxConquestOrders)
                    {
                        sendConquestCommandResponse(
                            *client, MessageType::ConquestOrdersSubmitResponse,
                            false, "Invalid Conquest orders payload");
                        break;
                    }
                    orders.resize(count);
                    bool valid = true;
                    for (conquest_data::MoveOrder& order : orders)
                    {
                        valid = valid && conquest_data::readMoveOrder(packet, order);
                    }
                    if (!valid)
                    {
                        sendConquestCommandResponse(
                            *client, MessageType::ConquestOrdersSubmitResponse,
                            false, "Invalid Conquest orders payload");
                        break;
                    }
                    handleConquestOrdersSubmit(*client, accessToken, eventId, orders);
                    notifyConquestChanged();
                    break;
                }
                case MessageType::ConquestReinforceRequest:
                {
                    std::string accessToken;
                    std::uint64_t eventId = 0;
                    std::uint64_t eventDeckId = 0;
                    int regionId = 0;
                    packet >> accessToken >> eventId >> eventDeckId >> regionId;
                    handleConquestReinforce(
                        *client, accessToken, eventId, eventDeckId, regionId);
                    notifyConquestChanged();
                    break;
                }
                case MessageType::AdminConquestEventStartRequest:
                {
                    std::string accessToken;
                    std::uint64_t eventId = 0;
                    packet >> accessToken >> eventId;
                    if (!packet)
                    {
                        sendConquestCommandResponse(
                            *client, MessageType::AdminConquestEventStartResponse,
                            false, "Invalid Conquest start request");
                        break;
                    }
                    handleAdminConquestEventStart(*client, accessToken, eventId);
                    notifyConquestChanged();
                    break;
                }
                case MessageType::ConquestBattleDataRequest:
                {
                    std::uint64_t battleId = 0;
                    std::string accessToken;
                    packet >> battleId >> accessToken;
                    if (!isLoopbackAddress(remoteAddress))
                    {
                        sendConquestBattleDataResponse(
                            *client, false, "Internal service access required", 0, {}, {});
                        break;
                    }
                    handleConquestBattleData(*client, battleId, accessToken);
                    break;
                }
                case MessageType::ConquestBattleReloadRequest:
                {
                    std::uint64_t battleId = 0;
                    std::string capability;
                    packet >> battleId >> capability;
                    if (!packet || !isLoopbackAddress(remoteAddress))
                    {
                        sendConquestBattleReloadResponse(
                            *client,
                            false,
                            isLoopbackAddress(remoteAddress)
                                ? "Invalid Conquest battle reload request"
                                : "Internal service access required",
                            {});
                        break;
                    }
                    handleConquestBattleReload(*client, battleId, capability);
                    break;
                }
                case MessageType::ConquestBattleActionRequest:
                {
                    std::uint64_t battleId = 0;
                    std::string capability;
                    conquest_data::BattleAction action;
                    packet >> battleId >> capability;
                    if (!isLoopbackAddress(remoteAddress) ||
                        !conquest_data::readBattleAction(packet, action))
                    {
                        sendConquestBattleActionResponse(
                            *client, false,
                            isLoopbackAddress(remoteAddress)
                                ? "Invalid Conquest battle action"
                                : "Internal service access required",
                            0);
                        break;
                    }
                    handleConquestBattleAction(*client, battleId, capability, action);
                    break;
                }
                case MessageType::SubmitConquestBattleResult:
                {
                    std::uint64_t battleId = 0;
                    std::string capability;
                    int winnerPlayerNumber = 0;
                    packet >> battleId >> capability >> winnerPlayerNumber;
                    if (!packet || !isLoopbackAddress(remoteAddress))
                    {
                        sendConquestCommandResponse(
                            *client, MessageType::SubmitConquestBattleResultResponse,
                            false,
                            isLoopbackAddress(remoteAddress)
                                ? "Invalid Conquest battle result request"
                                : "Internal service access required");
                        break;
                    }
                    handleSubmitConquestBattleResult(
                        *client, battleId, capability, winnerPlayerNumber);
                    notifyConquestChanged();
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
        bayou::tls::Socket& client,
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
            response << false << std::string(
                "Password must be 7-128 chars with uppercase, lowercase, number, and special char");
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

    void handleListDecks(bayou::tls::Socket& client, const std::string& accessToken)
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
        bayou::tls::Socket& client,
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

    void handleDeleteDeck(bayou::tls::Socket& client, const std::string& accessToken, const std::string& deckName)
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

    void handleConquestLoadout(
        bayou::tls::Socket& client,
        const std::string& accessToken)
    {
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username =
                account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username)
            {
                sendConquestLoadoutResponse(
                    client, false, "Authentication required", {}, {});
                return;
            }

            const std::vector<conquest_data::ConquestDeck> decks =
                account_conquest::loadDecks(*database, *username);
            const conquest_data::ConquestArmy army =
                account_conquest::loadArmy(*database, *username);
            if (decks.size() > conquest_data::MaxSerializedConquestDecks ||
                army.deckIds.size() > conquest_data::MaxConquestArmyDecks)
            {
                sendConquestLoadoutResponse(
                    client, false, "Conquest loadout is too large", {}, {});
                return;
            }

            sendConquestLoadoutResponse(
                client, true, "Conquest loadout loaded", decks, army);
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while loading Conquest loadout: {}", error.what());
            sendConquestLoadoutResponse(
                client, false, "Database error while loading Conquest loadout", {}, {});
        }
    }

    void handleConquestDeckSave(
        bayou::tls::Socket& client,
        const std::string& accessToken,
        conquest_data::ConquestDeck deck)
    {
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username =
                account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username)
            {
                sendConquestDeckResponse(
                    client, false, "Authentication required", deck);
                return;
            }

            if (const std::optional<std::string> error =
                    account_conquest::saveDeck(*database, *username, deck))
            {
                sendConquestDeckResponse(client, false, *error, deck);
                return;
            }

            sendConquestDeckResponse(client, true, "Conquest deck saved", deck);
            fmt::println(
                "Saved Conquest deck '{}' ({}) for user {}",
                deck.deck.name,
                deck.id,
                *username);
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while saving Conquest deck: {}", error.what());
            sendConquestDeckResponse(
                client, false, "Database error while saving Conquest deck", deck);
        }
    }

    void handleConquestDeckDelete(
        bayou::tls::Socket& client,
        const std::string& accessToken,
        std::int64_t deckId,
        std::uint32_t expectedRevision)
    {
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username =
                account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username)
            {
                sendDeckCommandResponse(
                    client,
                    MessageType::ConquestDeckDeleteResponse,
                    false,
                    "Authentication required");
                return;
            }

            if (const std::optional<std::string> error = account_conquest::deleteDeck(
                    *database, *username, deckId, expectedRevision))
            {
                sendDeckCommandResponse(
                    client, MessageType::ConquestDeckDeleteResponse, false, *error);
                return;
            }

            sendDeckCommandResponse(
                client,
                MessageType::ConquestDeckDeleteResponse,
                true,
                "Conquest deck deleted");
            fmt::println("Deleted Conquest deck {} for user {}", deckId, *username);
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while deleting Conquest deck: {}", error.what());
            sendDeckCommandResponse(
                client,
                MessageType::ConquestDeckDeleteResponse,
                false,
                "Database error while deleting Conquest deck");
        }
    }

    void handleConquestArmySave(
        bayou::tls::Socket& client,
        const std::string& accessToken,
        conquest_data::ConquestArmy army)
    {
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username =
                account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username)
            {
                sendConquestArmyResponse(
                    client, false, "Authentication required", army);
                return;
            }

            if (const std::optional<std::string> error =
                    account_conquest::saveArmy(*database, *username, army))
            {
                sendConquestArmyResponse(client, false, *error, army);
                return;
            }

            sendConquestArmyResponse(client, true, "Conquest army saved", army);
            fmt::println(
                "Saved Conquest army for user {} ({} decks)",
                *username,
                army.deckIds.size());
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while saving Conquest army: {}", error.what());
            sendConquestArmyResponse(
                client, false, "Database error while saving Conquest army", army);
        }
    }

    void handleConquestEventList(
        bayou::tls::Socket& client,
        const std::string& accessToken)
    {
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username =
                account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username)
            {
                sendConquestEventListResponse(
                    client, false, "Authentication required", {});
                return;
            }
            const std::vector<conquest_data::EventSummary> events =
                account_conquest_events::listEvents(*database, *username);
            sendConquestEventListResponse(
                client, true, "Conquest events loaded", events);
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while loading Conquest events: {}", error.what());
            sendConquestEventListResponse(
                client, false, "Database error while loading Conquest events", {});
        }
    }

    void handleConquestEventState(
        bayou::tls::Socket& client,
        const std::string& accessToken,
        std::uint64_t eventId)
    {
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username =
                account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username)
            {
                sendConquestEventStateResponse(
                    client, false, "Authentication required", {});
                return;
            }
            std::string error;
            const std::optional<conquest_data::EventState> state =
                account_conquest_events::loadEventState(
                    *database, eventId, *username, error);
            if (!state)
            {
                sendConquestEventStateResponse(
                    client, false,
                    error.empty() ? "Conquest event could not be loaded" : error,
                    {});
                return;
            }
            sendConquestEventStateResponse(
                client, true, "Conquest event loaded", *state);
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while loading Conquest event: {}", error.what());
            sendConquestEventStateResponse(
                client, false, "Database error while loading Conquest event", {});
        }
    }

    void handleConquestEventWatch(
        bayou::tls::Socket& client,
        const std::string& accessToken,
        std::uint64_t eventId,
        std::uint64_t knownStateFingerprint)
    {
        try
        {
            const std::uint64_t observedGeneration =
                conquestChangeGeneration.load(std::memory_order_acquire);
            std::string username;
            conquest_data::EventState state;
            {
                std::lock_guard<std::mutex> lock(databaseMutex);
                const std::optional<std::string> authenticated =
                    account_tokens::authenticateAccessToken(*database, accessToken);
                if (!authenticated)
                {
                    sendConquestCommandResponse(
                        client, MessageType::ConquestEventWatchResponse,
                        false, "Authentication required");
                    return;
                }
                username = *authenticated;
                std::string error;
                const std::optional<conquest_data::EventState> loaded =
                    account_conquest_events::loadEventState(
                        *database, eventId, username, error);
                if (!loaded)
                {
                    sendConquestCommandResponse(
                        client, MessageType::ConquestEventWatchResponse,
                        false,
                        error.empty() ? "Conquest event could not be watched" : error);
                    return;
                }
                state = *loaded;
            }

            if (conquest_data::eventStateFingerprint(state) == knownStateFingerprint)
            {
                auto wakeAt = std::chrono::steady_clock::now() + std::chrono::seconds(20);
                const std::int64_t wallNow = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                const std::int64_t eventDeadline =
                    state.summary.phase == conquest_data::EventPhase::Registration
                        ? state.summary.registrationEndsAt
                        : state.summary.turnEndsAt;
                if (eventDeadline > wallNow)
                {
                    wakeAt = std::min(
                        wakeAt,
                        std::chrono::steady_clock::now() +
                            std::chrono::seconds(eventDeadline - wallNow));
                }

                std::unique_lock<std::mutex> lock(conquestChangeMutex);
                conquestChanged.wait_until(lock, wakeAt, [&] {
                    return !running.load() ||
                        conquestChangeGeneration.load(std::memory_order_acquire) !=
                            observedGeneration;
                });
            }

            sendConquestCommandResponse(
                client, MessageType::ConquestEventWatchResponse,
                true, "Conquest event changed");
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while watching Conquest event: {}", error.what());
            sendConquestCommandResponse(
                client, MessageType::ConquestEventWatchResponse,
                false, "Database error while watching Conquest event");
        }
    }

    void notifyConquestChanged()
    {
        conquestChangeGeneration.fetch_add(1, std::memory_order_release);
        conquestChanged.notify_all();
    }

    void handleConquestEventJoin(
        bayou::tls::Socket& client,
        const std::string& accessToken,
        std::uint64_t eventId,
        const std::vector<conquest_data::StartingPlacement>& placements)
    {
        handleAuthenticatedConquestCommand(
            client,
            MessageType::ConquestEventJoinResponse,
            accessToken,
            [&](const std::string& username) {
                return account_conquest_events::joinEvent(
                    *database, eventId, username, placements);
            });
    }

    void handleConquestOrdersSubmit(
        bayou::tls::Socket& client,
        const std::string& accessToken,
        std::uint64_t eventId,
        const std::vector<conquest_data::MoveOrder>& orders)
    {
        handleAuthenticatedConquestCommand(
            client,
            MessageType::ConquestOrdersSubmitResponse,
            accessToken,
            [&](const std::string& username) {
                return account_conquest_events::submitOrders(
                    *database, eventId, username, orders);
            });
    }

    void handleConquestReinforce(
        bayou::tls::Socket& client,
        const std::string& accessToken,
        std::uint64_t eventId,
        std::uint64_t eventDeckId,
        int regionId)
    {
        handleAuthenticatedConquestCommand(
            client,
            MessageType::ConquestReinforceResponse,
            accessToken,
            [&](const std::string& username) {
                return account_conquest_events::deployReinforcement(
                    *database, eventId, username, eventDeckId, regionId);
            });
    }

    void handleAdminConquestEventStart(
        bayou::tls::Socket& client,
        const std::string& accessToken,
        std::uint64_t eventId)
    {
        handleAuthenticatedConquestCommand(
            client,
            MessageType::AdminConquestEventStartResponse,
            accessToken,
            [&](const std::string& username) {
                return account_conquest_events::forceStartEvent(
                    *database, eventId, username);
            });
    }

    void handleConquestBattleData(
        bayou::tls::Socket& client,
        std::uint64_t battleId,
        const std::string& accessToken)
    {
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username =
                account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username)
            {
                sendConquestBattleDataResponse(
                    client, false, "Authentication required", 0, {}, {});
                return;
            }
            int playerNumber = 0;
            std::string capability;
            std::string error;
            const std::optional<conquest_data::BattleData> data =
                account_conquest_events::loadBattleDataForCoordinator(
                    *database,
                    battleId,
                    *username,
                    playerNumber,
                    capability,
                    error);
            if (!data)
            {
                sendConquestBattleDataResponse(
                    client, false,
                    error.empty() ? "Conquest battle could not be loaded" : error,
                    0, {}, {});
                return;
            }
            sendConquestBattleDataResponse(
                client,
                true,
                "Conquest battle loaded",
                playerNumber,
                capability,
                *data);
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while loading Conquest battle: {}", error.what());
            sendConquestBattleDataResponse(
                client,
                false,
                "Database error while loading Conquest battle",
                0,
                {},
                {});
        }
    }

    void handleConquestBattleReload(
        bayou::tls::Socket& client,
        std::uint64_t battleId,
        const std::string& capability)
    {
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            std::string error;
            const std::optional<conquest_data::BattleData> data =
                account_conquest_events::reloadBattleDataForCoordinator(
                    *database, battleId, capability, error);
            if (!data)
            {
                sendConquestBattleReloadResponse(
                    client,
                    false,
                    error.empty() ? "Conquest battle could not be reloaded" : error,
                    {});
                return;
            }
            sendConquestBattleReloadResponse(
                client, true, "Conquest battle reloaded", *data);
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while reloading Conquest battle: {}", error.what());
            sendConquestBattleReloadResponse(
                client, false, "Database error while reloading Conquest battle", {});
        }
    }

    void handleConquestBattleAction(
        bayou::tls::Socket& client,
        std::uint64_t battleId,
        const std::string& capability,
        const conquest_data::BattleAction& action)
    {
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const account_conquest_events::CommandResult result =
                account_conquest_events::appendBattleActionWithCapability(
                    *database, battleId, capability, action);
            sendConquestBattleActionResponse(
                client, result.success, result.message,
                result.success ? action.sequence : 0);
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while storing Conquest action: {}", error.what());
            sendConquestBattleActionResponse(
                client, false, "Database error while storing Conquest action", 0);
        }
    }

    void handleSubmitConquestBattleResult(
        bayou::tls::Socket& client,
        std::uint64_t battleId,
        const std::string& capability,
        int winnerPlayerNumber)
    {
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const account_conquest_events::CommandResult result =
                account_conquest_events::applyBattleResultWithCapability(
                    *database, battleId, capability, winnerPlayerNumber);
            sendConquestCommandResponse(
                client, MessageType::SubmitConquestBattleResultResponse,
                result.success, result.message);
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while applying Conquest battle result: {}", error.what());
            sendConquestCommandResponse(
                client, MessageType::SubmitConquestBattleResultResponse,
                false, "Database error while applying Conquest battle result");
        }
    }

    template <typename Command>
    void handleAuthenticatedConquestCommand(
        bayou::tls::Socket& client,
        MessageType responseType,
        const std::string& accessToken,
        Command&& command)
    {
        try
        {
            std::lock_guard<std::mutex> lock(databaseMutex);
            const std::optional<std::string> username =
                account_tokens::authenticateAccessToken(*database, accessToken);
            if (!username)
            {
                sendConquestCommandResponse(
                    client, responseType, false, "Authentication required");
                return;
            }
            const account_conquest_events::CommandResult result = command(*username);
            sendConquestCommandResponse(
                client, responseType, result.success, result.message);
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while applying Conquest command: {}", error.what());
            sendConquestCommandResponse(
                client, responseType, false, "Database error while applying Conquest command");
        }
    }

    void handleAccountState(bayou::tls::Socket& client, const std::string& accessToken)
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

    void handleRankedPlayerRequest(bayou::tls::Socket& client, const std::string& accessToken)
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
        bayou::tls::Socket& client,
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
        bayou::tls::Socket& client,
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

    void handleSubmitAiResult(bayou::tls::Socket& client, const std::string& accessToken, bool humanWon)
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

    void handleWinReward(bayou::tls::Socket& client, const std::string& accessToken)
    {
        (void)accessToken;
        sendCoinResponse(
            client,
            MessageType::WinRewardResponse,
            false,
            "Win rewards are awarded by the game server",
            0);
    }

    void handleShopPurchase(bayou::tls::Socket& client, const std::string& accessToken)
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
        bayou::tls::Socket& client,
        const std::string& accessToken,
        const std::string& search,
        std::uint32_t page,
        std::uint32_t pageSize)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::AdminUserListResponse);

        const std::uint32_t safePageSize = std::clamp<std::uint32_t>(pageSize == 0 ? 25 : pageSize, 1, 100);
        // Clamp the page so page * pageSize can neither overflow uint32 nor the
        // int the SQL OFFSET is bound as.
        const std::uint32_t safePage = std::min(
            page,
            static_cast<std::uint32_t>(std::numeric_limits<int>::max()) / safePageSize);
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
        bayou::tls::Socket& client,
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
            sendChangePasswordResponse(
                client,
                false,
                "New password must be 7-128 chars with uppercase, lowercase, number, and special char");
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
        bayou::tls::Socket& client,
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
        bayou::tls::Socket& client,
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

    void handleAdminUserCard(
        bayou::tls::Socket& client,
        const std::string& accessToken,
        const std::string& targetUsername,
        const std::string& cardTitle)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::AdminUserCardResponse);

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
            else if (!accountExists(targetUsername))
            {
                response << false << std::string("Username not found");
            }
            else
            {
                const std::vector<account_catalog::ShopCardEntry> collectibleCards =
                    account_catalog::loadShopCards();
                const auto card = std::find_if(
                    collectibleCards.begin(),
                    collectibleCards.end(),
                    [&](const account_catalog::ShopCardEntry& candidate) {
                        return candidate.title == cardTitle;
                    });
                if (card == collectibleCards.end())
                {
                    response << false << std::string("Card not found or cannot be collected");
                }
                else
                {
                    account_decks::addCollectionCopies(*database, targetUsername, cardTitle, 1);
                    response << true << ("Added " + cardTitle + " to " + targetUsername + "'s collection");
                    fmt::println("{} added card '{}' to {}'s collection", *username, cardTitle, targetUsername);
                }
            }
        }
        catch (const std::exception& error)
        {
            fmt::println("Database error while adding card to player collection: {}", error.what());
            response.clear();
            response << static_cast<uint8_t>(MessageType::AdminUserCardResponse);
            response << false << std::string("Database error while adding card to player collection");
        }

        [[maybe_unused]] auto result = client.send(response);
    }

    void handleAdminUserDelete(
        bayou::tls::Socket& client,
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

    void handleAdminStarterDeck(bayou::tls::Socket& client, const std::string& accessToken)
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

    void handleAdminStarterDeckSave(bayou::tls::Socket& client, const std::string& accessToken, const deck_data::Deck& deck)
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
        bayou::tls::Socket& client,
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

    void handleRememberLogin(bayou::tls::Socket& client, const std::string& token)
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
        bayou::tls::Socket& client,
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
        bayou::tls::Socket& client,
        MessageType responseType,
        bool success,
        const std::string& message)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(responseType);
        response << success << message;
        [[maybe_unused]] auto result = client.send(response);
    }

    void sendConquestLoadoutResponse(
        bayou::tls::Socket& client,
        bool success,
        const std::string& message,
        const std::vector<conquest_data::ConquestDeck>& decks,
        const conquest_data::ConquestArmy& army)
    {
        const bool payloadFits =
            decks.size() <= conquest_data::MaxSerializedConquestDecks &&
            army.deckIds.size() <= conquest_data::MaxConquestArmyDecks;

        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::ConquestLoadoutResponse);
        response << (success && payloadFits)
                 << (payloadFits ? message : std::string("Conquest loadout is too large"));
        const bool wrotePayload = payloadFits &&
            conquest_data::writeConquestDeckList(response, decks) &&
            conquest_data::writeConquestArmy(response, army);
        if (!wrotePayload)
        {
            response.clear();
            response << static_cast<uint8_t>(MessageType::ConquestLoadoutResponse);
            response << false << std::string("Conquest loadout is too large");
            [[maybe_unused]] const bool wroteDecks =
                conquest_data::writeConquestDeckList(response, {});
            [[maybe_unused]] const bool wroteArmy =
                conquest_data::writeConquestArmy(response, {});
        }
        [[maybe_unused]] auto result = client.send(response);
    }

    void sendConquestDeckResponse(
        bayou::tls::Socket& client,
        bool success,
        const std::string& message,
        const conquest_data::ConquestDeck& deck)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::ConquestDeckSaveResponse);
        response << success << message;
        if (!conquest_data::writeConquestDeck(response, deck))
        {
            response.clear();
            response << static_cast<uint8_t>(MessageType::ConquestDeckSaveResponse);
            response << false << std::string("Conquest deck payload is too large");
            [[maybe_unused]] const bool wroteEmptyDeck =
                conquest_data::writeConquestDeck(response, {});
        }
        [[maybe_unused]] auto result = client.send(response);
    }

    void sendConquestArmyResponse(
        bayou::tls::Socket& client,
        bool success,
        const std::string& message,
        const conquest_data::ConquestArmy& army)
    {
        const bool payloadFits =
            army.deckIds.size() <= conquest_data::MaxConquestArmyDecks;
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::ConquestArmySaveResponse);
        response << (success && payloadFits)
                 << (payloadFits ? message : std::string("Conquest army is too large"));
        [[maybe_unused]] const bool wroteArmy = conquest_data::writeConquestArmy(
            response, payloadFits ? army : conquest_data::ConquestArmy{});
        [[maybe_unused]] auto result = client.send(response);
    }

    static void sendConquestCommandResponse(
        bayou::tls::Socket& client,
        MessageType responseType,
        bool success,
        const std::string& message)
    {
        sf::Packet response;
        response << static_cast<std::uint8_t>(responseType)
                 << success << message;
        [[maybe_unused]] auto result = client.send(response);
    }

    static void sendConquestEventListResponse(
        bayou::tls::Socket& client,
        bool success,
        const std::string& message,
        const std::vector<conquest_data::EventSummary>& events)
    {
        const bool payloadFits = events.size() <= conquest_data::MaxConquestEvents;
        sf::Packet response;
        response << static_cast<std::uint8_t>(MessageType::ConquestEventListResponse)
                 << (success && payloadFits)
                 << (payloadFits ? message : std::string("Too many Conquest events"));
        const std::uint32_t count = payloadFits
            ? static_cast<std::uint32_t>(events.size())
            : 0;
        response << count;
        if (payloadFits)
        {
            for (const conquest_data::EventSummary& event : events)
            {
                conquest_data::writeEventSummary(response, event);
            }
        }
        [[maybe_unused]] auto result = client.send(response);
    }

    static void sendConquestEventStateResponse(
        bayou::tls::Socket& client,
        bool success,
        const std::string& message,
        const conquest_data::EventState& state)
    {
        sf::Packet response;
        response << static_cast<std::uint8_t>(MessageType::ConquestEventStateResponse)
                 << success << message;
        conquest_data::writeEventState(response, state);
        [[maybe_unused]] auto result = client.send(response);
    }

    static void sendConquestBattleDataResponse(
        bayou::tls::Socket& client,
        bool success,
        const std::string& message,
        int playerNumber,
        const std::string& capability,
        const conquest_data::BattleData& data)
    {
        sf::Packet response;
        response << static_cast<std::uint8_t>(MessageType::ConquestBattleDataResponse)
                 << success << message << playerNumber << capability;
        conquest_data::writeBattleData(response, data);
        [[maybe_unused]] auto result = client.send(response);
    }

    static void sendConquestBattleReloadResponse(
        bayou::tls::Socket& client,
        bool success,
        const std::string& message,
        const conquest_data::BattleData& data)
    {
        sf::Packet response;
        response << static_cast<std::uint8_t>(MessageType::ConquestBattleReloadResponse)
                 << success << message;
        conquest_data::writeBattleData(response, data);
        [[maybe_unused]] auto result = client.send(response);
    }

    static void sendConquestBattleActionResponse(
        bayou::tls::Socket& client,
        bool success,
        const std::string& message,
        std::uint32_t acceptedSequence)
    {
        sf::Packet response;
        response << static_cast<std::uint8_t>(MessageType::ConquestBattleActionResponse)
                 << success << message << acceptedSequence;
        [[maybe_unused]] auto result = client.send(response);
    }

    void sendAccountStateResponse(
        bayou::tls::Socket& client,
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
        bayou::tls::Socket& client,
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
        bayou::tls::Socket& client,
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
        bayou::tls::Socket& client,
        bool success,
        const std::string& message)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::ChangePasswordResponse)
                 << success << message;
        [[maybe_unused]] auto result = client.send(response);
    }

};

int main(int argc, char* argv[])
{
    std::filesystem::path configPath = "gameserver.cfg";
    if (argc == 3 && std::string(argv[1]) == "--config")
    {
        configPath = argv[2];
    }
    else if (argc != 1)
    {
        fmt::println("Usage: accounts [--config <path>]");
        return 1;
    }

    std::string configError;
    const std::optional<card_source_config::Config> config =
        card_source_config::load(configPath, configError);
    if (!config)
    {
        fmt::println("Account server card-source configuration error: {}", configError);
        return 1;
    }

    std::string catalogError;
    std::vector<card_data::Card> cardLibrary = card_server_client::load(*config, catalogError);
    if (!catalogError.empty())
    {
        fmt::println("Account server could not load the authoritative card catalog: {}", catalogError);
        return 1;
    }
    if (cardLibrary.empty())
    {
        fmt::println("Account server received an empty authoritative card catalog");
        return 1;
    }
    fmt::println("Loaded {} authoritative cards", cardLibrary.size());
    account_catalog::setCardLibrary(std::move(cardLibrary));

    fmt::println("Starting Accounts Server...");

    AccountServer server(55000);
    if (!server.isListening())
    {
        return 1;
    }
    server.run();

    return 0;
}
