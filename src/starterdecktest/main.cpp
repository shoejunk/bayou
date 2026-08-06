#include "../accounts/starter_deck_database.hpp"
#include "../shared/starter_decks.hpp"

#include <SQLiteCpp/SQLiteCpp.h>

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
class TestFailure : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

void require(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw TestFailure(message);
    }
}

bool tableExists(SQLite::Database& database, const std::string& tableName)
{
    SQLite::Statement query(
        database,
        "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ? LIMIT 1");
    query.bind(1, tableName);
    return query.executeStep();
}

std::vector<std::string> loadCards(SQLite::Database& database, const std::string& deckName)
{
    std::vector<std::string> cards;
    SQLite::Statement query(
        database,
        "SELECT card_title FROM starter_deck_cards WHERE deck_name = ? ORDER BY card_index");
    query.bind(1, deckName);
    while (query.executeStep())
    {
        cards.push_back(query.getColumn(0).getString());
    }
    return cards;
}

void testCurrentSchemaMigration()
{
    SQLite::Database accounts(":memory:", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    SQLite::Database starterDecks(":memory:", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    accounts.exec(
        "CREATE TABLE starter_deck_cards ("
        "deck_name TEXT NOT NULL,"
        "card_index INTEGER NOT NULL,"
        "card_title TEXT NOT NULL,"
        "PRIMARY KEY(deck_name, card_index)"
        ")");
    accounts.exec(
        "INSERT INTO starter_deck_cards (deck_name, card_index, card_title) VALUES "
        "('The Seelie Court', 1, 'Second'),"
        "('The Seelie Court', 0, 'First')");

    require(
        starter_deck_database::migrateFromAccounts(accounts, starterDecks),
        "current starter-deck table was not migrated");
    require(
        !tableExists(accounts, "starter_deck_cards"),
        "current starter-deck table remained in accounts database");
    require(
        loadCards(starterDecks, "The Seelie Court") ==
            std::vector<std::string>{"First", "Second"},
        "current starter-deck rows changed during migration");
    require(
        !starter_deck_database::migrateFromAccounts(accounts, starterDecks),
        "migration was not idempotent after the accounts table was removed");
}

void testUnnamedLegacyMigration()
{
    SQLite::Database accounts(":memory:", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    SQLite::Database starterDecks(":memory:", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    accounts.exec(
        "CREATE TABLE starter_deck_cards ("
        "card_index INTEGER PRIMARY KEY NOT NULL,"
        "card_title TEXT NOT NULL"
        ")");
    accounts.exec(
        "INSERT INTO starter_deck_cards (card_index, card_title) VALUES "
        "(0, 'Legacy First'), (1, 'Legacy Second')");

    require(
        starter_deck_database::migrateFromAccounts(accounts, starterDecks),
        "unnamed legacy starter deck was not migrated");
    require(
        loadCards(starterDecks, starter_decks::Names.front()) ==
            std::vector<std::string>{"Legacy First", "Legacy Second"},
        "unnamed legacy starter deck was not assigned to the first faction");
}

void testExistingDestinationWins()
{
    SQLite::Database accounts(":memory:", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    SQLite::Database starterDecks(":memory:", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    accounts.exec(
        "CREATE TABLE starter_deck_cards ("
        "deck_name TEXT NOT NULL,"
        "card_index INTEGER NOT NULL,"
        "card_title TEXT NOT NULL,"
        "PRIMARY KEY(deck_name, card_index)"
        ")");
    accounts.exec(
        "INSERT INTO starter_deck_cards (deck_name, card_index, card_title) "
        "VALUES ('The Seelie Court', 0, 'Stale Source')");
    starter_deck_database::initialize(starterDecks);
    starterDecks.exec(
        "INSERT INTO starter_deck_cards (deck_name, card_index, card_title) "
        "VALUES ('The Seelie Court', 0, 'Authoritative Destination')");

    require(
        starter_deck_database::migrateFromAccounts(accounts, starterDecks),
        "duplicate source table was not retired");
    require(
        loadCards(starterDecks, "The Seelie Court") ==
            std::vector<std::string>{"Authoritative Destination"},
        "existing starter-decks database was overwritten by legacy account data");
}
}

int main()
{
    try
    {
        testCurrentSchemaMigration();
        testUnnamedLegacyMigration();
        testExistingDestinationWins();
        std::cout << "All starter-deck database tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Starter-deck database test failed: " << error.what() << '\n';
        return 1;
    }
}
