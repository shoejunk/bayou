#pragma once

#include "../shared/account_data.hpp"
#include "../shared/deck_data.hpp"

#include <SQLiteCpp/SQLiteCpp.h>

#include <optional>
#include <string>
#include <vector>

namespace account_decks
{

std::vector<deck_data::Deck> loadDecks(SQLite::Database& database, const std::string& username);
std::vector<account_data::CollectionCard> loadCollection(SQLite::Database& database, const std::string& username);
void addCollectionCopies(SQLite::Database& database, const std::string& username, const std::string& cardTitle, int copies);
std::optional<std::string> deckCollectionError(
    SQLite::Database& database,
    const std::string& username,
    const deck_data::Deck& deck);
std::optional<std::string> deckRulesError(const deck_data::Deck& deck);
void saveDeck(SQLite::Database& database, const std::string& username, const std::string& originalName, const deck_data::Deck& deck);
bool deleteDeck(SQLite::Database& database, const std::string& username, const std::string& deckName);
std::optional<deck_data::Deck> loadStarterDeckOverride(SQLite::Database& database);
void saveStarterDeckOverride(SQLite::Database& database, const deck_data::Deck& deck);
void ensureStarterInventory(SQLite::Database& database, const std::string& username);
void purgeTokenCards(SQLite::Database& database);

} // namespace account_decks
