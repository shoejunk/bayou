#include <SFML/Network.hpp>
#include <SQLiteCpp/SQLiteCpp.h>
#include <fmt/core.h>

#include "../shared/card_data.hpp"
#include "../shared/network.hpp"
#include "../shared/socket_timeout.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace network;

namespace
{
constexpr unsigned short CardServerPort = 55004;
constexpr auto InitialRequestTimeout = std::chrono::seconds(2);

void writeCards(sf::Packet& packet, const std::vector<card_data::Card>& cards)
{
    packet << static_cast<std::uint32_t>(cards.size());
    for (const card_data::Card& card : cards)
    {
        card_data::writeCard(packet, card);
    }
}

void writeActions(sf::Packet& packet, const std::vector<card_data::Action>& actions)
{
    packet << static_cast<std::uint32_t>(actions.size());
    for (const card_data::Action& action : actions)
    {
        card_data::writeAction(packet, action);
    }
}

std::vector<std::string> split(const std::string& value, char delimiter)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= value.size())
    {
        const std::size_t end = value.find(delimiter, start);
        parts.push_back(value.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos)
        {
            break;
        }
        start = end + 1;
    }
    return parts;
}

bool hasFlag(const std::string& flags, const std::string& wanted)
{
    const std::vector<std::string> values = split(flags, ',');
    return std::find(values.begin(), values.end(), wanted) != values.end();
}

int integerOr(const std::string& value, int fallback)
{
    try
    {
        return std::stoi(value);
    }
    catch (...)
    {
        return fallback;
    }
}

std::optional<card_data::Action> decodeLegacyAction(const std::string& name, const std::string& value)
{
    const std::vector<std::string> fields = split(value, '|');
    if (fields.size() != 9)
    {
        return std::nullopt;
    }

    card_data::Action action;
    action.name = name;
    action.state = integerOr(fields[0], 0);
    action.kind = fields[1];
    action.pattern = fields[2];
    action.minRange = integerOr(fields[3], 1);
    action.maxRange = integerOr(fields[4], 1);
    action.damage = integerOr(fields[5], 0);
    action.canMove = hasFlag(fields[6], "move");
    action.canAttack = hasFlag(fields[6], "attack");
    action.passThrough = hasFlag(fields[6], "pass");
    action.lineOfSight = hasFlag(fields[6], "los");
    action.statusTurns = integerOr(fields[7], 0);
    action.cooldownTurns = integerOr(fields[8], 0);
    return action;
}
}

class CardServer
{
public:
    explicit CardServer(unsigned short port)
        : listener(std::make_unique<sf::TcpListener>())
    {
        try
        {
            database = std::make_unique<SQLite::Database>("cards.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
            initializeDatabase();
            fmt::println("Using cards database: cards.db");
        }
        catch (const std::exception& error)
        {
            fmt::println("Failed to initialize cards database: {}", error.what());
            return;
        }

        if (listener->listen(port) != sf::Socket::Status::Done)
        {
            fmt::println("Failed to listen on port {}", port);
            return;
        }

        listening = true;
        fmt::println("Card server listening on port {}", port);
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
    std::unique_ptr<SQLite::Database> database;
    std::atomic<bool> running{false};
    bool listening = false;

    void initializeDatabase()
    {
        database->exec(
            "CREATE TABLE IF NOT EXISTS cards ("
            "title TEXT PRIMARY KEY NOT NULL,"
            "type TEXT NOT NULL,"
            "image_path TEXT NOT NULL"
            ")");
        database->exec(
            "CREATE TABLE IF NOT EXISTS card_keywords ("
            "title TEXT NOT NULL,"
            "keyword TEXT NOT NULL,"
            "FOREIGN KEY(title) REFERENCES cards(title) ON DELETE CASCADE"
            ")");
        database->exec(
            "CREATE TABLE IF NOT EXISTS card_integer_values ("
            "title TEXT NOT NULL,"
            "key TEXT NOT NULL,"
            "value INTEGER NOT NULL,"
            "FOREIGN KEY(title) REFERENCES cards(title) ON DELETE CASCADE"
            ")");
        database->exec(
            "CREATE TABLE IF NOT EXISTS card_string_values ("
            "title TEXT NOT NULL,"
            "key TEXT NOT NULL,"
            "value TEXT NOT NULL,"
            "FOREIGN KEY(title) REFERENCES cards(title) ON DELETE CASCADE"
            ")");
        database->exec(
            "CREATE TABLE IF NOT EXISTS card_string_lists ("
            "title TEXT NOT NULL,"
            "key TEXT NOT NULL,"
            "item_index INTEGER NOT NULL,"
            "value TEXT NOT NULL,"
            "FOREIGN KEY(title) REFERENCES cards(title) ON DELETE CASCADE"
            ")");
        database->exec(
            "CREATE TABLE IF NOT EXISTS actions ("
            "name TEXT PRIMARY KEY NOT NULL,"
            "state INTEGER NOT NULL,"
            "kind TEXT NOT NULL,"
            "pattern TEXT NOT NULL,"
            "min_range INTEGER NOT NULL,"
            "max_range INTEGER NOT NULL,"
            "damage INTEGER NOT NULL,"
            "can_move INTEGER NOT NULL,"
            "can_attack INTEGER NOT NULL,"
            "pass_through INTEGER NOT NULL,"
            "line_of_sight INTEGER NOT NULL,"
            "status_turns INTEGER NOT NULL,"
            "cooldown_turns INTEGER NOT NULL"
            ")");
        database->exec(
            "CREATE TABLE IF NOT EXISTS card_actions ("
            "title TEXT NOT NULL,"
            "action_name TEXT NOT NULL,"
            "item_index INTEGER NOT NULL,"
            "PRIMARY KEY(title, item_index),"
            "FOREIGN KEY(title) REFERENCES cards(title) ON DELETE CASCADE,"
            "FOREIGN KEY(action_name) REFERENCES actions(name)"
            ")");
        migrateLegacyActions();
    }

    void handleClient(sf::TcpSocket& client)
    {
        sf::Packet request;
        if (socket_timeout::receivePacket(client, request, InitialRequestTimeout) != sf::Socket::Status::Done)
        {
            return;
        }

        uint8_t msgType = 0;
        request >> msgType;

        switch (static_cast<MessageType>(msgType))
        {
            case MessageType::CardListRequest:
                handleListCards(client);
                break;
            case MessageType::CardUpsertRequest:
                handleUpsertCard(client, request);
                break;
            case MessageType::CardUpdateRequest:
                handleUpdateCard(client, request);
                break;
            case MessageType::CardDeleteRequest:
                handleDeleteCard(client, request);
                break;
            case MessageType::ActionListRequest:
                handleListActions(client);
                break;
            case MessageType::ActionUpsertRequest:
                handleUpsertAction(client, request);
                break;
            case MessageType::ActionUpdateRequest:
                handleUpdateAction(client, request);
                break;
            case MessageType::ActionDeleteRequest:
                handleDeleteAction(client, request);
                break;
            case MessageType::Disconnect:
                break;
            default:
                sendUpsertResponse(client, false, "Unsupported card server request");
                break;
        }
    }

    void handleListCards(sf::TcpSocket& client)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::CardListResponse);

        try
        {
            const std::vector<card_data::Card> cards = loadCards();
            response << true << std::string("OK");
            writeCards(response, cards);
        }
        catch (const std::exception& error)
        {
            response << false << std::string(error.what());
            response << static_cast<std::uint32_t>(0);
        }

        [[maybe_unused]] auto result = client.send(response);
    }

    void handleUpsertCard(sf::TcpSocket& client, sf::Packet& request)
    {
        card_data::Card card;
        if (!card_data::readCard(request, card))
        {
            sendUpsertResponse(client, false, "Invalid card payload");
            return;
        }

        if (card.title.empty())
        {
            sendUpsertResponse(client, false, "Card title cannot be empty");
            return;
        }

        try
        {
            saveCard(card);
            sendUpsertResponse(client, true, "Card saved");
        }
        catch (const std::exception& error)
        {
            sendUpsertResponse(client, false, error.what());
        }
    }

    void handleUpdateCard(sf::TcpSocket& client, sf::Packet& request)
    {
        std::string originalTitle;
        card_data::Card card;
        request >> originalTitle;
        if (!request || !card_data::readCard(request, card))
        {
            sendCommandResponse(client, MessageType::CardUpdateResponse, false, "Invalid card update payload");
            return;
        }

        if (originalTitle.empty())
        {
            sendCommandResponse(client, MessageType::CardUpdateResponse, false, "Original card title cannot be empty");
            return;
        }

        if (card.title.empty())
        {
            sendCommandResponse(client, MessageType::CardUpdateResponse, false, "Card title cannot be empty");
            return;
        }

        try
        {
            saveCard(originalTitle, card);
            sendCommandResponse(client, MessageType::CardUpdateResponse, true, "Card saved");
        }
        catch (const std::exception& error)
        {
            sendCommandResponse(client, MessageType::CardUpdateResponse, false, error.what());
        }
    }

    void handleDeleteCard(sf::TcpSocket& client, sf::Packet& request)
    {
        std::string title;
        request >> title;
        if (!request)
        {
            sendCommandResponse(client, MessageType::CardDeleteResponse, false, "Invalid card delete payload");
            return;
        }

        if (title.empty())
        {
            sendCommandResponse(client, MessageType::CardDeleteResponse, false, "Card title cannot be empty");
            return;
        }

        try
        {
            deleteCard(title);
            sendCommandResponse(client, MessageType::CardDeleteResponse, true, "Card deleted");
        }
        catch (const std::exception& error)
        {
            sendCommandResponse(client, MessageType::CardDeleteResponse, false, error.what());
        }
    }

    void handleListActions(sf::TcpSocket& client)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::ActionListResponse);
        try
        {
            const std::vector<card_data::Action> actions = loadActions();
            response << true << std::string("OK");
            writeActions(response, actions);
        }
        catch (const std::exception& error)
        {
            response << false << std::string(error.what()) << static_cast<std::uint32_t>(0);
        }
        [[maybe_unused]] auto result = client.send(response);
    }

    void handleUpsertAction(sf::TcpSocket& client, sf::Packet& request)
    {
        card_data::Action action;
        if (!card_data::readAction(request, action) || action.name.empty())
        {
            sendCommandResponse(client, MessageType::ActionUpsertResponse, false, "Invalid action payload");
            return;
        }
        try
        {
            saveAction("", action);
            sendCommandResponse(client, MessageType::ActionUpsertResponse, true, "Action saved");
        }
        catch (const std::exception& error)
        {
            sendCommandResponse(client, MessageType::ActionUpsertResponse, false, error.what());
        }
    }

    void handleUpdateAction(sf::TcpSocket& client, sf::Packet& request)
    {
        std::string originalName;
        card_data::Action action;
        request >> originalName;
        if (!request || !card_data::readAction(request, action) || originalName.empty() || action.name.empty())
        {
            sendCommandResponse(client, MessageType::ActionUpdateResponse, false, "Invalid action update payload");
            return;
        }
        try
        {
            saveAction(originalName, action);
            sendCommandResponse(client, MessageType::ActionUpdateResponse, true, "Action saved");
        }
        catch (const std::exception& error)
        {
            sendCommandResponse(client, MessageType::ActionUpdateResponse, false, error.what());
        }
    }

    void handleDeleteAction(sf::TcpSocket& client, sf::Packet& request)
    {
        std::string name;
        request >> name;
        if (!request || name.empty())
        {
            sendCommandResponse(client, MessageType::ActionDeleteResponse, false, "Invalid action delete payload");
            return;
        }
        try
        {
            SQLite::Statement references(*database, "SELECT COUNT(*) FROM card_actions WHERE action_name = ?");
            references.bind(1, name);
            references.executeStep();
            if (references.getColumn(0).getInt() > 0)
            {
                sendCommandResponse(client, MessageType::ActionDeleteResponse, false, "Action is still referenced by a card");
                return;
            }
            SQLite::Statement statement(*database, "DELETE FROM actions WHERE name = ?");
            statement.bind(1, name);
            statement.exec();
            sendCommandResponse(client, MessageType::ActionDeleteResponse, true, "Action deleted");
        }
        catch (const std::exception& error)
        {
            sendCommandResponse(client, MessageType::ActionDeleteResponse, false, error.what());
        }
    }

    void sendUpsertResponse(sf::TcpSocket& client, bool success, const std::string& message)
    {
        sendCommandResponse(client, MessageType::CardUpsertResponse, success, message);
    }

    void sendCommandResponse(sf::TcpSocket& client, MessageType responseType, bool success, const std::string& message)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(responseType);
        response << success;
        response << message;
        [[maybe_unused]] auto result = client.send(response);
    }

    void saveCard(const card_data::Card& card)
    {
        saveCard("", card);
    }

    void saveCard(const std::string& originalTitle, const card_data::Card& card)
    {
        SQLite::Transaction transaction(*database);

        if (!originalTitle.empty() && originalTitle != card.title)
        {
            deleteCardRows(originalTitle);
        }

        SQLite::Statement upsert(
            *database,
            "INSERT INTO cards (title, type, image_path) VALUES (?, ?, ?) "
            "ON CONFLICT(title) DO UPDATE SET type = excluded.type, image_path = excluded.image_path");
        upsert.bind(1, card.title);
        upsert.bind(2, card.type);
        upsert.bind(3, card.imagePath);
        upsert.exec();

        deleteChildRows(card.title);
        insertKeywords(card);
        insertIntegerValues(card);
        insertStringValues(card);
        insertStringLists(card);
        insertActionReferences(card);

        transaction.commit();
    }

    void deleteCard(const std::string& title)
    {
        SQLite::Transaction transaction(*database);
        deleteCardRows(title);
        transaction.commit();
    }

    void deleteCardRows(const std::string& title)
    {
        deleteChildRows(title);

        SQLite::Statement statement(*database, "DELETE FROM cards WHERE title = ?");
        statement.bind(1, title);
        statement.exec();
    }

    void deleteChildRows(const std::string& title)
    {
        for (const std::string& table : {
                 "card_keywords",
                 "card_integer_values",
                 "card_string_values",
                 "card_string_lists",
                 "card_actions",
             })
        {
            SQLite::Statement statement(*database, fmt::format("DELETE FROM {} WHERE title = ?", table));
            statement.bind(1, title);
            statement.exec();
        }
    }

    void insertKeywords(const card_data::Card& card)
    {
        SQLite::Statement statement(*database, "INSERT INTO card_keywords (title, keyword) VALUES (?, ?)");
        for (const std::string& keyword : card.keywords)
        {
            statement.reset();
            statement.bind(1, card.title);
            statement.bind(2, keyword);
            statement.exec();
        }
    }

    void insertIntegerValues(const card_data::Card& card)
    {
        SQLite::Statement statement(*database, "INSERT INTO card_integer_values (title, key, value) VALUES (?, ?, ?)");
        for (const card_data::KeyIntPair& item : card.integerValues)
        {
            statement.reset();
            statement.bind(1, card.title);
            statement.bind(2, item.key);
            statement.bind(3, item.value);
            statement.exec();
        }
    }

    void insertStringValues(const card_data::Card& card)
    {
        SQLite::Statement statement(*database, "INSERT INTO card_string_values (title, key, value) VALUES (?, ?, ?)");
        for (const card_data::KeyStringPair& item : card.stringValues)
        {
            statement.reset();
            statement.bind(1, card.title);
            statement.bind(2, item.key);
            statement.bind(3, item.value);
            statement.exec();
        }
    }

    void insertStringLists(const card_data::Card& card)
    {
        SQLite::Statement statement(
            *database,
            "INSERT INTO card_string_lists (title, key, item_index, value) VALUES (?, ?, ?, ?)");
        for (const card_data::KeyStringList& item : card.stringLists)
        {
            if (item.key == "actions")
            {
                continue;
            }
            for (std::size_t i = 0; i < item.values.size(); ++i)
            {
                statement.reset();
                statement.bind(1, card.title);
                statement.bind(2, item.key);
                statement.bind(3, static_cast<int>(i));
                statement.bind(4, item.values[i]);
                statement.exec();
            }
        }
    }

    void insertActionReferences(const card_data::Card& card)
    {
        SQLite::Statement exists(*database, "SELECT 1 FROM actions WHERE name = ?");
        SQLite::Statement insert(
            *database,
            "INSERT INTO card_actions (title, action_name, item_index) VALUES (?, ?, ?)");
        for (std::size_t i = 0; i < card.actionNames.size(); ++i)
        {
            exists.reset();
            exists.bind(1, card.actionNames[i]);
            if (!exists.executeStep())
            {
                throw std::runtime_error("Unknown action reference: " + card.actionNames[i]);
            }
            insert.reset();
            insert.bind(1, card.title);
            insert.bind(2, card.actionNames[i]);
            insert.bind(3, static_cast<int>(i));
            insert.exec();
        }
    }

    std::vector<card_data::Card> loadCards()
    {
        std::vector<card_data::Card> cards;
        SQLite::Statement query(*database, "SELECT title, type, image_path FROM cards ORDER BY title");
        while (query.executeStep())
        {
            card_data::Card card;
            card.title = query.getColumn(0).getString();
            card.type = query.getColumn(1).getString();
            card.imagePath = query.getColumn(2).getString();
            card.keywords = loadKeywords(card.title);
            card.integerValues = loadIntegerValues(card.title);
            card.stringValues = loadStringValues(card.title);
            card.stringLists = loadStringLists(card.title);
            card.actionNames = loadActionNames(card.title);
            card.actions = loadCardActions(card.title);
            cards.push_back(card);
        }

        return cards;
    }

    std::vector<std::string> loadKeywords(const std::string& title)
    {
        std::vector<std::string> values;
        SQLite::Statement query(*database, "SELECT keyword FROM card_keywords WHERE title = ? ORDER BY keyword");
        query.bind(1, title);
        while (query.executeStep())
        {
            values.push_back(query.getColumn(0).getString());
        }
        return values;
    }

    std::vector<card_data::KeyIntPair> loadIntegerValues(const std::string& title)
    {
        std::vector<card_data::KeyIntPair> values;
        SQLite::Statement query(*database, "SELECT key, value FROM card_integer_values WHERE title = ? ORDER BY key");
        query.bind(1, title);
        while (query.executeStep())
        {
            values.push_back({query.getColumn(0).getString(), query.getColumn(1).getInt()});
        }
        return values;
    }

    std::vector<card_data::KeyStringPair> loadStringValues(const std::string& title)
    {
        std::vector<card_data::KeyStringPair> values;
        SQLite::Statement query(*database, "SELECT key, value FROM card_string_values WHERE title = ? ORDER BY key");
        query.bind(1, title);
        while (query.executeStep())
        {
            values.push_back({query.getColumn(0).getString(), query.getColumn(1).getString()});
        }
        return values;
    }

    std::vector<card_data::KeyStringList> loadStringLists(const std::string& title)
    {
        std::map<std::string, std::vector<std::string>> grouped;
        SQLite::Statement query(
            *database,
            "SELECT key, value FROM card_string_lists WHERE title = ? ORDER BY key, item_index");
        query.bind(1, title);
        while (query.executeStep())
        {
            grouped[query.getColumn(0).getString()].push_back(query.getColumn(1).getString());
        }

        std::vector<card_data::KeyStringList> values;
        for (auto& [key, items] : grouped)
        {
            values.push_back({key, std::move(items)});
        }
        return values;
    }

    std::vector<std::string> loadActionNames(const std::string& title)
    {
        std::vector<std::string> values;
        SQLite::Statement query(
            *database,
            "SELECT action_name FROM card_actions WHERE title = ? ORDER BY item_index");
        query.bind(1, title);
        while (query.executeStep())
        {
            values.push_back(query.getColumn(0).getString());
        }
        return values;
    }

    static card_data::Action actionFromQuery(SQLite::Statement& query)
    {
        card_data::Action action;
        action.name = query.getColumn(0).getString();
        action.state = query.getColumn(1).getInt();
        action.kind = query.getColumn(2).getString();
        action.pattern = query.getColumn(3).getString();
        action.minRange = query.getColumn(4).getInt();
        action.maxRange = query.getColumn(5).getInt();
        action.damage = query.getColumn(6).getInt();
        action.canMove = query.getColumn(7).getInt() != 0;
        action.canAttack = query.getColumn(8).getInt() != 0;
        action.passThrough = query.getColumn(9).getInt() != 0;
        action.lineOfSight = query.getColumn(10).getInt() != 0;
        action.statusTurns = query.getColumn(11).getInt();
        action.cooldownTurns = query.getColumn(12).getInt();
        return action;
    }

    std::vector<card_data::Action> loadActions()
    {
        std::vector<card_data::Action> actions;
        SQLite::Statement query(
            *database,
            "SELECT name, state, kind, pattern, min_range, max_range, damage, can_move, can_attack, "
            "pass_through, line_of_sight, status_turns, cooldown_turns FROM actions ORDER BY name");
        while (query.executeStep())
        {
            actions.push_back(actionFromQuery(query));
        }
        return actions;
    }

    std::vector<card_data::Action> loadCardActions(const std::string& title)
    {
        std::vector<card_data::Action> actions;
        SQLite::Statement query(
            *database,
            "SELECT a.name, a.state, a.kind, a.pattern, a.min_range, a.max_range, a.damage, "
            "a.can_move, a.can_attack, a.pass_through, a.line_of_sight, a.status_turns, a.cooldown_turns "
            "FROM card_actions ca JOIN actions a ON a.name = ca.action_name "
            "WHERE ca.title = ? ORDER BY ca.item_index");
        query.bind(1, title);
        while (query.executeStep())
        {
            actions.push_back(actionFromQuery(query));
        }
        return actions;
    }

    void bindAction(SQLite::Statement& statement, const card_data::Action& action, int offset = 1)
    {
        statement.bind(offset, action.name);
        statement.bind(offset + 1, action.state);
        statement.bind(offset + 2, action.kind);
        statement.bind(offset + 3, action.pattern);
        statement.bind(offset + 4, action.minRange);
        statement.bind(offset + 5, action.maxRange);
        statement.bind(offset + 6, action.damage);
        statement.bind(offset + 7, action.canMove ? 1 : 0);
        statement.bind(offset + 8, action.canAttack ? 1 : 0);
        statement.bind(offset + 9, action.passThrough ? 1 : 0);
        statement.bind(offset + 10, action.lineOfSight ? 1 : 0);
        statement.bind(offset + 11, action.statusTurns);
        statement.bind(offset + 12, action.cooldownTurns);
    }

    void saveAction(const std::string& originalName, const card_data::Action& action)
    {
        SQLite::Transaction transaction(*database);
        SQLite::Statement upsert(
            *database,
            "INSERT INTO actions (name, state, kind, pattern, min_range, max_range, damage, can_move, "
            "can_attack, pass_through, line_of_sight, status_turns, cooldown_turns) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(name) DO UPDATE SET state=excluded.state, kind=excluded.kind, pattern=excluded.pattern, "
            "min_range=excluded.min_range, max_range=excluded.max_range, damage=excluded.damage, "
            "can_move=excluded.can_move, can_attack=excluded.can_attack, pass_through=excluded.pass_through, "
            "line_of_sight=excluded.line_of_sight, status_turns=excluded.status_turns, cooldown_turns=excluded.cooldown_turns");
        bindAction(upsert, action);
        upsert.exec();

        if (!originalName.empty() && originalName != action.name)
        {
            SQLite::Statement updateReferences(
                *database,
                "UPDATE card_actions SET action_name = ? WHERE action_name = ?");
            updateReferences.bind(1, action.name);
            updateReferences.bind(2, originalName);
            updateReferences.exec();
            SQLite::Statement removeOld(*database, "DELETE FROM actions WHERE name = ?");
            removeOld.bind(1, originalName);
            removeOld.exec();
        }
        transaction.commit();
    }

    void migrateLegacyActions()
    {
        SQLite::Statement count(*database, "SELECT COUNT(*) FROM card_actions");
        count.executeStep();
        if (count.getColumn(0).getInt() != 0)
        {
            return;
        }

        SQLite::Transaction transaction(*database);
        SQLite::Statement query(
            *database,
            "SELECT title, item_index, value FROM card_string_lists WHERE key = 'actions' ORDER BY title, item_index");
        while (query.executeStep())
        {
            const std::string title = query.getColumn(0).getString();
            const int itemIndex = query.getColumn(1).getInt();
            const std::string name = fmt::format("{} / Action {}", title, itemIndex + 1);
            const std::optional<card_data::Action> action = decodeLegacyAction(name, query.getColumn(2).getString());
            if (!action)
            {
                continue;
            }
            SQLite::Statement insertAction(
                *database,
                "INSERT OR IGNORE INTO actions (name, state, kind, pattern, min_range, max_range, damage, "
                "can_move, can_attack, pass_through, line_of_sight, status_turns, cooldown_turns) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
            bindAction(insertAction, *action);
            insertAction.exec();
            SQLite::Statement insertReference(
                *database,
                "INSERT OR IGNORE INTO card_actions (title, action_name, item_index) VALUES (?, ?, ?)");
            insertReference.bind(1, title);
            insertReference.bind(2, name);
            insertReference.bind(3, itemIndex);
            insertReference.exec();
        }
        database->exec("DELETE FROM card_string_lists WHERE key = 'actions'");
        transaction.commit();
    }
};

int main()
{
    fmt::println("Starting Card Server...");

    CardServer server(CardServerPort);
    server.run();

    return 0;
}
