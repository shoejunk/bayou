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
// Admin-defined contents of one of the four faction starter decks, or nullopt
// when that deck has never been saved.
std::optional<deck_data::Deck> loadStarterDeckOverride(SQLite::Database& database, const std::string& deckName);
// Contents an account would receive today: the admin-defined deck when one
// exists, otherwise the built-in fallback.
deck_data::Deck effectiveStarterDeck(SQLite::Database& database, const std::string& deckName);
void saveStarterDeckOverride(SQLite::Database& database, const deck_data::Deck& deck);
std::vector<std::string> loadOwnedStarterDecks(SQLite::Database& database, const std::string& username);
bool ownsStarterDeck(SQLite::Database& database, const std::string& username, const std::string& deckName);
// Records ownership, adds every card of the deck to the collection, and saves
// the deck itself when the player has no deck by that name yet. Opens no
// transaction of its own so callers can bundle it with a coin deduction.
void grantStarterDeck(SQLite::Database& database, const std::string& username, const std::string& deckName);
void purgeTokenCards(SQLite::Database& database);

} // namespace account_decks
