#include "../accounts/account_catalog.hpp"
#include "../accounts/account_collection_grant.hpp"
#include "../accounts/account_tokens.hpp"

#include <SQLiteCpp/SQLiteCpp.h>
#include <sodium.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
int assertions = 0;
void require(bool condition, const std::string& message)
{
    ++assertions;
    if (!condition) throw std::runtime_error(message);
}

card_data::Card card(const std::string& title, const std::string& rarity = "common")
{
    card_data::Card result;
    result.title = title;
    result.type = "Unit";
    result.stringValues.push_back({"rarity", rarity});
    return result;
}

int copies(SQLite::Database& db, const std::string& username, const std::string& title)
{
    SQLite::Statement query(db, "SELECT copies FROM card_collections WHERE username = ? AND card_title = ?");
    query.bind(1, username);
    query.bind(2, title);
    return query.executeStep() ? query.getColumn(0).getInt() : -1;
}
}

int main()
{
    try
    {
        require(sodium_init() >= 0, "sodium initialization");
        // Entire suite uses synthetic cards, real token authentication, and an
        // in-memory database. No service, configured database, or network access.
        SQLite::Database db(":memory:", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        db.exec("PRAGMA foreign_keys = ON");
        db.exec("CREATE TABLE accounts (username TEXT PRIMARY KEY, is_admin INTEGER NOT NULL DEFAULT 0)");
        db.exec("CREATE TABLE access_tokens (token_hash TEXT PRIMARY KEY, username TEXT NOT NULL REFERENCES accounts(username), "
                "expires_at INTEGER NOT NULL, created_at INTEGER NOT NULL, last_used_at INTEGER NOT NULL)");
        db.exec("CREATE TABLE card_collections (username TEXT NOT NULL REFERENCES accounts(username), card_title TEXT NOT NULL, "
                "copies INTEGER NOT NULL, PRIMARY KEY(username, card_title))");
        db.exec("INSERT INTO accounts VALUES ('admin', 1), ('player', 0), ('other', 0)");
        const auto adminToken = account_tokens::issueAccessToken(db, "admin");
        const auto playerToken = account_tokens::issueAccessToken(db, "player");
        const auto expiredToken = account_tokens::issueAccessToken(db, "other");
        db.exec("UPDATE access_tokens SET expires_at = 0 WHERE username = 'other'");
        const auto revokedToken = account_tokens::issueAccessToken(db, "admin");
        account_tokens::revokeAccessToken(db, revokedToken);
        std::vector<card_data::Card> catalog = {
            card("Missing"), card("Zero"), card("One"), card("Three"), card("Four"), card("Seven"),
            card("Starter", "starter"), card("Legendary", "legendary"), card("Token", "token"),
            card("Existing Token", "token"), card("Missing"), card("")};
        auto grant = [&](const std::string& token, const std::string& target = "player") {
            return account_collection_grant::grantAllCards(db, token, target);
        };
        // Run before seeding the catalog so the refresh interval has not started.
        bool loaderCalled = false;
        account_catalog::setCardLibraryLoader([&](std::string&) -> std::vector<card_data::Card> {
            loaderCalled = true;
            throw std::runtime_error("synthetic catalog failure");
        });
        require(!grant(adminToken).success && loaderCalled, "catalog exception handling");
        account_catalog::setCardLibraryLoader({});
        account_catalog::setCardLibrary(catalog);
        for (const auto& token : {std::string(), std::string("invalid"), playerToken, expiredToken, revokedToken})
        {
            const auto result = grant(token);
            require(!result.success && result.message == "Admin access required", "unauthorized request must fail");
        }
        require(db.execAndGet("SELECT COUNT(*) FROM card_collections").getInt() == 0, "unauthorized requests changed collection");
        db.exec("UPDATE accounts SET is_admin = 0 WHERE username = 'admin'");
        require(!grant(adminToken).success, "demoted admin token must fail");
        db.exec("UPDATE accounts SET is_admin = 1 WHERE username = 'admin'");
        require(grant(adminToken, "").message == "Target username cannot be empty", "empty target");
        require(grant(adminToken, "missing-user").message == "Username not found", "unknown target");
        require(grant(adminToken, "player' OR 1=1 --").message == "Username not found", "target must be bound literally");
        db.exec("INSERT INTO card_collections VALUES "
                "('player','Zero',0),('player','One',1),('player','Three',3),('player','Four',4),('player','Seven',7),"
                "('player','Existing Token',2),('player','Legacy',9),('other','One',1)");
        require(grant(adminToken).success, "valid grant");
        for (const auto& title : {"Missing", "Zero", "One", "Three", "Four", "Starter", "Legendary"})
            require(copies(db, "player", title) == 4, std::string("top-up count: ") + title);
        require(copies(db, "player", "Seven") == 7, "higher count reduced");
        require(copies(db, "player", "Token") == -1, "token granted");
        require(copies(db, "player", "Existing Token") == 2, "existing token modified");
        require(copies(db, "player", "") == -1, "empty title granted");
        require(copies(db, "player", "Legacy") == 9, "out-of-catalog card changed");
        require(copies(db, "other", "One") == 1, "other account changed");
        require(grant(adminToken).success, "repeated grant");
        for (const auto& title : {"Missing", "Zero", "One", "Three", "Four", "Starter", "Legendary"})
            require(copies(db, "player", title) == 4, std::string("repeated grant changed count: ") + title);
        require(copies(db, "player", "Seven") == 7, "repeated grant reduced higher count");
        catalog.push_back(card("New Card"));
        account_catalog::setCardLibrary(catalog);
        require(grant(adminToken).success && copies(db, "player", "New Card") == 4, "new catalog card omitted");

        // Fail after at least one insert, irrespective of catalog iteration order.
        db.exec("DELETE FROM card_collections WHERE username = 'player'");
        db.exec("CREATE TRIGGER fail_second BEFORE INSERT ON card_collections "
                "WHEN NEW.username = 'player' AND (SELECT COUNT(*) FROM card_collections WHERE username = 'player') >= 1 "
                "BEGIN SELECT RAISE(ABORT, 'synthetic storage failure'); END");
        const auto failed = grant(adminToken);
        require(!failed.success && failed.message == "Unable to top up player collection; try again later", "storage failure response");
        require(db.execAndGet("SELECT COUNT(*) FROM card_collections WHERE username = 'player'").getInt() == 0, "partial grant was not rolled back");
        db.exec("DROP TRIGGER fail_second");
        require(grant(adminToken).success, "retry after rolled-back failure");

        account_catalog::setCardLibrary({});
        require(!grant(adminToken).success, "empty catalog used fallback cards");
        account_catalog::setCardLibrary({card("Token", "token")});
        require(!grant(adminToken).success, "token-only catalog used fallback cards");
        std::cout << "PASS: " << assertions << " collection-grant assertions (offline)\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
