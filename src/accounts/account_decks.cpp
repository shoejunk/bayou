#include "account_decks.hpp"

#include "account_catalog.hpp"

#include "../shared/card_database.hpp"
#include "../shared/game_data.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace
{
std::vector<std::string> loadDeckCards(SQLite::Database& database, std::int64_t deckId)
{
    std::vector<std::string> cardTitles;
    SQLite::Statement query(
        database,
        "SELECT card_title FROM deck_cards WHERE deck_id = ? ORDER BY card_index");
    query.bind(1, deckId);

    while (query.executeStep())
    {
        cardTitles.push_back(query.getColumn(0).getString());
    }

    return cardTitles;
}

bool collectionIsEmpty(SQLite::Database& database, const std::string& username)
{
    SQLite::Statement query(
        database,
        "SELECT 1 FROM card_collections WHERE username = ? AND copies > 0 LIMIT 1");
    query.bind(1, username);
    return !query.executeStep();
}

std::optional<std::int64_t> findDeckId(SQLite::Database& database, const std::string& username, const std::string& deckName)
{
    SQLite::Statement query(
        database,
        "SELECT id FROM decks WHERE username = ? AND name = ? LIMIT 1");
    query.bind(1, username);
    query.bind(2, deckName);
    if (!query.executeStep())
    {
        return std::nullopt;
    }

    return query.getColumn(0).getInt64();
}

void saveDeckRows(SQLite::Database& database, const std::string& username, const std::string& originalName, const deck_data::Deck& deck)
{
    const std::string lookupName = originalName.empty() ? deck.name : originalName;
    std::optional<std::int64_t> deckId = findDeckId(database, username, lookupName);
    if (deckId)
    {
        SQLite::Statement update(
            database,
            "UPDATE decks SET name = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?");
        update.bind(1, deck.name);
        update.bind(2, *deckId);
        update.exec();
    }
    else
    {
        SQLite::Statement insert(
            database,
            "INSERT INTO decks (username, name) VALUES (?, ?)");
        insert.bind(1, username);
        insert.bind(2, deck.name);
        insert.exec();
        deckId = database.getLastInsertRowid();
    }

    SQLite::Statement deleteCards(database, "DELETE FROM deck_cards WHERE deck_id = ?");
    deleteCards.bind(1, *deckId);
    deleteCards.exec();

    SQLite::Statement insertCard(
        database,
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
}

namespace account_decks
{

std::vector<deck_data::Deck> loadDecks(SQLite::Database& database, const std::string& username)
{
    std::vector<deck_data::Deck> decks;
    SQLite::Statement query(
        database,
        "SELECT id, name FROM decks WHERE username = ? ORDER BY name");
    query.bind(1, username);

    while (query.executeStep())
    {
        const std::int64_t deckId = query.getColumn(0).getInt64();
        deck_data::Deck deck;
        deck.name = query.getColumn(1).getString();
        deck.cardTitles = loadDeckCards(database, deckId);
        decks.push_back(deck);
    }

    return decks;
}

std::vector<account_data::CollectionCard> loadCollection(SQLite::Database& database, const std::string& username)
{
    std::vector<account_data::CollectionCard> collection;
    SQLite::Statement query(
        database,
        "SELECT card_title, copies FROM card_collections WHERE username = ? AND copies > 0 ORDER BY card_title");
    query.bind(1, username);

    while (query.executeStep())
    {
        collection.push_back({query.getColumn(0).getString(), query.getColumn(1).getInt()});
    }

    return collection;
}

void addCollectionCopies(SQLite::Database& database, const std::string& username, const std::string& cardTitle, int copies)
{
    if (cardTitle.empty() || copies <= 0)
    {
        return;
    }

    SQLite::Statement upsert(
        database,
        "INSERT INTO card_collections (username, card_title, copies) VALUES (?, ?, ?) "
        "ON CONFLICT(username, card_title) DO UPDATE SET copies = copies + excluded.copies");
    upsert.bind(1, username);
    upsert.bind(2, cardTitle);
    upsert.bind(3, copies);
    upsert.exec();
}

std::optional<std::string> deckCollectionError(
    SQLite::Database& database,
    const std::string& username,
    const deck_data::Deck& deck)
{
    std::unordered_map<std::string, int> available;
    for (const account_data::CollectionCard& card : loadCollection(database, username))
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

std::optional<std::string> deckRulesError(const deck_data::Deck& deck)
{
    const std::vector<card_data::Card> library = card_database::loadCardsFromFile("cards.db");
    std::unordered_map<std::string, card_data::Card> cardsByTitle;
    for (const card_data::Card& card : library)
    {
        cardsByTitle.emplace(card.title, card);
    }

    std::vector<card_data::Card> cards;
    cards.reserve(deck.cardTitles.size());
    for (const std::string& title : deck.cardTitles)
    {
        const auto found = cardsByTitle.find(title);
        if (found == cardsByTitle.end())
        {
            return "Unknown card in deck: " + title;
        }
        cards.push_back(found->second);
    }

    return game_data::deckRulesError(cards);
}

void saveDeck(SQLite::Database& database, const std::string& username, const std::string& originalName, const deck_data::Deck& deck)
{
    SQLite::Transaction transaction(database);

    saveDeckRows(database, username, originalName, deck);
    transaction.commit();
}

bool deleteDeck(SQLite::Database& database, const std::string& username, const std::string& deckName)
{
    const std::optional<std::int64_t> deckId = findDeckId(database, username, deckName);
    if (!deckId)
    {
        return false;
    }

    SQLite::Transaction transaction(database);
    SQLite::Statement statement(database, "DELETE FROM decks WHERE id = ?");
    statement.bind(1, *deckId);
    statement.exec();
    transaction.commit();
    return true;
}

std::optional<deck_data::Deck> loadStarterDeckOverride(SQLite::Database& database)
{
    deck_data::Deck deck;
    deck.name = account_catalog::StarterDeckName;
    SQLite::Statement query(
        database,
        "SELECT card_title FROM starter_deck_cards ORDER BY card_index");
    while (query.executeStep())
    {
        deck.cardTitles.push_back(query.getColumn(0).getString());
    }

    if (deck.cardTitles.empty())
    {
        return std::nullopt;
    }

    return deck;
}

void saveStarterDeckOverride(SQLite::Database& database, const deck_data::Deck& deck)
{
    SQLite::Transaction transaction(database);
    database.exec("DELETE FROM starter_deck_cards");

    SQLite::Statement insert(
        database,
        "INSERT INTO starter_deck_cards (card_index, card_title) VALUES (?, ?)");
    for (std::size_t i = 0; i < deck.cardTitles.size(); ++i)
    {
        insert.reset();
        insert.bind(1, static_cast<int>(i));
        insert.bind(2, deck.cardTitles[i]);
        insert.exec();
    }
    transaction.commit();
}

void ensureStarterInventory(SQLite::Database& database, const std::string& username)
{
    if (!collectionIsEmpty(database, username))
    {
        return;
    }

    SQLite::Transaction transaction(database);
    const std::optional<deck_data::Deck> storedStarter = loadStarterDeckOverride(database);
    const deck_data::Deck starterDeck = storedStarter ? *storedStarter : account_catalog::makeStarterDeck();
    // An admin-defined starter deck is the player's entire starting collection;
    // the built-in fallback also grants its extra collection titles.
    const std::vector<std::string> collectionTitles = storedStarter
        ? starterDeck.cardTitles
        : account_catalog::starterCollectionTitles(starterDeck);
    for (const std::string& title : collectionTitles)
    {
        addCollectionCopies(database, username, title, 1);
    }

    if (loadDecks(database, username).empty())
    {
        saveDeckRows(database, username, "", starterDeck);
    }
    transaction.commit();
}

} // namespace account_decks
