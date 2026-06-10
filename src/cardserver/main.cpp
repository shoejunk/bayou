#include <SFML/Network.hpp>
#include <SQLiteCpp/SQLiteCpp.h>
#include <fmt/core.h>

#include "../shared/card_data.hpp"

#include <atomic>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <vector>

import network;

using namespace network;

namespace
{
constexpr unsigned short CardServerPort = 55004;

void writeCards(sf::Packet& packet, const std::vector<card_data::Card>& cards)
{
    packet << static_cast<std::uint32_t>(cards.size());
    for (const card_data::Card& card : cards)
    {
        card_data::writeCard(packet, card);
    }
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
    }

    void handleClient(sf::TcpSocket& client)
    {
        sf::Packet request;
        if (client.receive(request) != sf::Socket::Status::Done)
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

    void sendUpsertResponse(sf::TcpSocket& client, bool success, const std::string& message)
    {
        sf::Packet response;
        response << static_cast<uint8_t>(MessageType::CardUpsertResponse);
        response << success;
        response << message;
        [[maybe_unused]] auto result = client.send(response);
    }

    void saveCard(const card_data::Card& card)
    {
        SQLite::Transaction transaction(*database);

        SQLite::Statement upsert(
            *database,
            "INSERT INTO cards (title, type, image_path) VALUES (?, ?, ?) "
            "ON CONFLICT(title) DO UPDATE SET type = excluded.type, image_path = excluded.image_path");
        upsert.bind(1, card.title);
        upsert.bind(2, card_data::toString(card.type));
        upsert.bind(3, card.imagePath);
        upsert.exec();

        deleteChildRows(card.title);
        insertKeywords(card);
        insertIntegerValues(card);
        insertStringValues(card);
        insertStringLists(card);

        transaction.commit();
    }

    void deleteChildRows(const std::string& title)
    {
        for (const std::string& table : {
                 "card_keywords",
                 "card_integer_values",
                 "card_string_values",
                 "card_string_lists",
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

    std::vector<card_data::Card> loadCards()
    {
        std::vector<card_data::Card> cards;
        SQLite::Statement query(*database, "SELECT title, type, image_path FROM cards ORDER BY title");
        while (query.executeStep())
        {
            card_data::Card card;
            card.title = query.getColumn(0).getString();
            card.type = card_data::cardTypeFromString(query.getColumn(1).getString());
            card.imagePath = query.getColumn(2).getString();
            card.keywords = loadKeywords(card.title);
            card.integerValues = loadIntegerValues(card.title);
            card.stringValues = loadStringValues(card.title);
            card.stringLists = loadStringLists(card.title);
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
};

int main()
{
    fmt::println("Starting Card Server...");

    CardServer server(CardServerPort);
    server.run();

    return 0;
}
