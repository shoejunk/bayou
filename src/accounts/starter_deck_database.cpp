#include "starter_deck_database.hpp"

#include "../shared/starter_decks.hpp"

#include <string>

namespace
{
bool tableExists(SQLite::Database& database, const std::string& tableName)
{
    SQLite::Statement query(
        database,
        "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ? LIMIT 1");
    query.bind(1, tableName);
    return query.executeStep();
}

bool columnExists(
    SQLite::Database& database,
    const std::string& tableName,
    const std::string& columnName)
{
    SQLite::Statement query(database, "PRAGMA table_info(" + tableName + ")");
    while (query.executeStep())
    {
        if (query.getColumn(1).getString() == columnName)
        {
            return true;
        }
    }
    return false;
}

bool hasStarterDeckRows(SQLite::Database& database)
{
    SQLite::Statement query(database, "SELECT 1 FROM starter_deck_cards LIMIT 1");
    return query.executeStep();
}
}

namespace starter_deck_database
{

void initialize(SQLite::Database& database)
{
    database.exec(
        "CREATE TABLE IF NOT EXISTS starter_deck_cards ("
        "deck_name TEXT NOT NULL,"
        "card_index INTEGER NOT NULL,"
        "card_title TEXT NOT NULL,"
        "PRIMARY KEY(deck_name, card_index)"
        ")");
}

bool migrateFromAccounts(
    SQLite::Database& accountsDatabase,
    SQLite::Database& starterDeckDatabase)
{
    initialize(starterDeckDatabase);
    if (!tableExists(accountsDatabase, "starter_deck_cards"))
    {
        return false;
    }

    // A pre-provisioned or previously migrated starter-decks database is
    // authoritative. This also makes a retry safe if the first start copied
    // the rows but stopped before it could remove the source table.
    if (!hasStarterDeckRows(starterDeckDatabase))
    {
        SQLite::Transaction destinationTransaction(starterDeckDatabase);
        SQLite::Statement insert(
            starterDeckDatabase,
            "INSERT INTO starter_deck_cards (deck_name, card_index, card_title) "
            "VALUES (?, ?, ?)");

        if (columnExists(accountsDatabase, "starter_deck_cards", "deck_name"))
        {
            SQLite::Statement source(
                accountsDatabase,
                "SELECT deck_name, card_index, card_title FROM starter_deck_cards "
                "ORDER BY deck_name, card_index");
            while (source.executeStep())
            {
                insert.reset();
                insert.bind(1, source.getColumn(0).getString());
                insert.bind(2, source.getColumn(1).getInt());
                insert.bind(3, source.getColumn(2).getString());
                insert.exec();
            }
        }
        else
        {
            SQLite::Statement source(
                accountsDatabase,
                "SELECT card_index, card_title FROM starter_deck_cards ORDER BY card_index");
            while (source.executeStep())
            {
                insert.reset();
                insert.bind(1, std::string(starter_decks::Names.front()));
                insert.bind(2, source.getColumn(0).getInt());
                insert.bind(3, source.getColumn(1).getString());
                insert.exec();
            }
        }
        destinationTransaction.commit();
    }

    SQLite::Transaction sourceTransaction(accountsDatabase);
    accountsDatabase.exec("DROP TABLE starter_deck_cards");
    sourceTransaction.commit();
    return true;
}

} // namespace starter_deck_database
