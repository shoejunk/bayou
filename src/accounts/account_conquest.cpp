#include "account_conquest.hpp"

#include "account_catalog.hpp"
#include "account_decks.hpp"

#include "../shared/game_data.hpp"

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
std::vector<std::string> loadDeckCards(
    SQLite::Database& database,
    std::int64_t deckId)
{
    std::vector<std::string> cardTitles;
    SQLite::Statement query(
        database,
        "SELECT card_title FROM conquest_deck_cards "
        "WHERE deck_id = ? ORDER BY card_index");
    query.bind(1, deckId);
    while (query.executeStep())
    {
        cardTitles.push_back(query.getColumn(0).getString());
    }
    return cardTitles;
}

std::optional<conquest_data::ConquestDeck> loadDeck(
    SQLite::Database& database,
    const std::string& username,
    std::int64_t deckId)
{
    SQLite::Statement query(
        database,
        "SELECT name, revision FROM conquest_decks "
        "WHERE username = ? AND id = ? LIMIT 1");
    query.bind(1, username);
    query.bind(2, deckId);
    if (!query.executeStep())
    {
        return std::nullopt;
    }

    conquest_data::ConquestDeck deck;
    deck.id = deckId;
    deck.deck.name = query.getColumn(0).getString();
    const std::int64_t revision = query.getColumn(1).getInt64();
    if (revision <= 0 ||
        revision > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        throw std::runtime_error("Invalid Conquest deck revision in database");
    }
    deck.revision = static_cast<std::uint32_t>(revision);
    deck.deck.cardTitles = loadDeckCards(database, deckId);
    return deck;
}

std::optional<std::string> allocationError(
    SQLite::Database& database,
    const std::string& username,
    std::int64_t replacedDeckId,
    const deck_data::Deck& candidate)
{
    std::unordered_map<std::string, std::int64_t> owned;
    for (const account_data::CollectionCard& card :
         account_decks::loadCollection(database, username))
    {
        owned[card.title] = card.copies;
    }

    std::unordered_map<std::string, std::int64_t> allocated;
    SQLite::Statement query(
        database,
        "SELECT cards.card_title, COUNT(*) "
        "FROM conquest_deck_cards AS cards "
        "JOIN conquest_decks AS decks ON decks.id = cards.deck_id "
        "WHERE decks.username = ? AND decks.id <> ? "
        "GROUP BY cards.card_title");
    query.bind(1, username);
    query.bind(2, replacedDeckId);
    while (query.executeStep())
    {
        allocated[query.getColumn(0).getString()] = query.getColumn(1).getInt64();
    }

    for (const std::string& title : candidate.cardTitles)
    {
        ++allocated[title];
    }

    for (const auto& [title, count] : allocated)
    {
        const std::int64_t available = owned[title];
        if (count > available)
        {
            return "Conquest decks allocate " + std::to_string(count) +
                " copies of " + title + " but collection has " +
                std::to_string(available);
        }
    }
    return std::nullopt;
}

std::optional<std::string> deckIdentityError(
    SQLite::Database& database,
    const std::string& username,
    const conquest_data::ConquestDeck& deck,
    std::uint32_t& currentRevision)
{
    currentRevision = 0;
    if (deck.id < 0)
    {
        return "Invalid Conquest deck id";
    }
    if (deck.id == 0)
    {
        if (deck.revision != 0)
        {
            return "A new Conquest deck must have revision 0";
        }
        return std::nullopt;
    }
    if (deck.revision == 0)
    {
        return "A saved Conquest deck must include its revision";
    }

    SQLite::Statement query(
        database,
        "SELECT revision FROM conquest_decks WHERE username = ? AND id = ? LIMIT 1");
    query.bind(1, username);
    query.bind(2, deck.id);
    if (!query.executeStep())
    {
        return "Conquest deck not found";
    }

    const std::int64_t storedRevision = query.getColumn(0).getInt64();
    if (storedRevision <= 0 ||
        storedRevision > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        throw std::runtime_error("Invalid Conquest deck revision in database");
    }
    currentRevision = static_cast<std::uint32_t>(storedRevision);
    if (deck.revision != currentRevision)
    {
        return "Conquest deck changed since it was loaded; reload and try again";
    }
    if (currentRevision == std::numeric_limits<std::uint32_t>::max())
    {
        return "Conquest deck revision limit reached";
    }
    return std::nullopt;
}

bool conquestDeckNameExists(
    SQLite::Database& database,
    const std::string& username,
    const std::string& name,
    std::int64_t excludedDeckId)
{
    SQLite::Statement query(
        database,
        "SELECT 1 FROM conquest_decks "
        "WHERE username = ? AND name = ? AND id <> ? LIMIT 1");
    query.bind(1, username);
    query.bind(2, name);
    query.bind(3, excludedDeckId);
    return query.executeStep();
}

bool tableExists(SQLite::Database& database, const std::string& tableName)
{
    SQLite::Statement query(
        database,
        "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ? LIMIT 1");
    query.bind(1, tableName);
    return query.executeStep();
}

bool deckReservedByActiveEvent(
    SQLite::Database& database,
    const std::string& username,
    std::int64_t deckId)
{
    // account_conquest has isolated unit-test users that intentionally create
    // only the loadout schema. In the server, the event schema is present and
    // supplies the reservation check below.
    if (!tableExists(database, "conquest_events") ||
        !tableExists(database, "conquest_event_decks"))
    {
        return false;
    }

    SQLite::Statement query(
        database,
        "SELECT 1 FROM conquest_event_decks snapshot "
        "JOIN conquest_events event ON event.id = snapshot.event_id "
        "WHERE snapshot.owner = ? AND snapshot.source_deck_id = ? "
        "AND event.phase <> 3 LIMIT 1");
    query.bind(1, username);
    query.bind(2, deckId);
    return query.executeStep();
}
} // namespace

namespace account_conquest
{

std::vector<conquest_data::ConquestDeck> loadDecks(
    SQLite::Database& database,
    const std::string& username)
{
    std::vector<conquest_data::ConquestDeck> decks;
    SQLite::Statement query(
        database,
        "SELECT id, name, revision FROM conquest_decks "
        "WHERE username = ? ORDER BY name, id");
    query.bind(1, username);
    while (query.executeStep())
    {
        conquest_data::ConquestDeck deck;
        deck.id = query.getColumn(0).getInt64();
        deck.deck.name = query.getColumn(1).getString();
        const std::int64_t revision = query.getColumn(2).getInt64();
        if (revision <= 0 ||
            revision > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            throw std::runtime_error("Invalid Conquest deck revision in database");
        }
        deck.revision = static_cast<std::uint32_t>(revision);
        deck.deck.cardTitles = loadDeckCards(database, deck.id);
        decks.push_back(std::move(deck));
    }
    return decks;
}

conquest_data::ConquestArmy loadArmy(
    SQLite::Database& database,
    const std::string& username)
{
    conquest_data::ConquestArmy army;
    SQLite::Statement header(
        database,
        "SELECT revision FROM conquest_armies WHERE username = ? LIMIT 1");
    header.bind(1, username);
    if (!header.executeStep())
    {
        return army;
    }

    const std::int64_t revision = header.getColumn(0).getInt64();
    if (revision <= 0 ||
        revision > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        throw std::runtime_error("Invalid Conquest army revision in database");
    }
    army.revision = static_cast<std::uint32_t>(revision);

    SQLite::Statement members(
        database,
        "SELECT deck_id FROM conquest_army_decks "
        "WHERE username = ? ORDER BY slot_index");
    members.bind(1, username);
    while (members.executeStep())
    {
        army.deckIds.push_back(members.getColumn(0).getInt64());
    }
    return army;
}

std::optional<std::string> saveDeck(
    SQLite::Database& database,
    const std::string& username,
    conquest_data::ConquestDeck& deck)
{
    if (deck.deck.name.empty())
    {
        return "Conquest deck name cannot be empty";
    }
    if (deck.deck.name.size() > conquest_data::MaxConquestDeckNameBytes)
    {
        return "Conquest deck name is too long";
    }
    if (deck.deck.cardTitles.size() > deck_data::MaxSerializedDeckCards)
    {
        return "Conquest deck has too many cards";
    }

    SQLite::Transaction transaction(database, SQLite::TransactionBehavior::IMMEDIATE);

    std::uint32_t currentRevision = 0;
    if (const std::optional<std::string> error =
            deckIdentityError(database, username, deck, currentRevision))
    {
        return error;
    }
    if (deck.id != 0 && deckReservedByActiveEvent(database, username, deck.id))
    {
        return "A Conquest deck committed to an active event cannot be edited";
    }
    if (deck.id == 0)
    {
        SQLite::Statement count(
            database,
            "SELECT COUNT(*) FROM conquest_decks WHERE username = ?");
        count.bind(1, username);
        if (!count.executeStep() ||
            count.getColumn(0).getInt64() >=
                static_cast<std::int64_t>(conquest_data::MaxSerializedConquestDecks))
        {
            return "Conquest deck limit reached";
        }
    }
    if (conquestDeckNameExists(database, username, deck.deck.name, deck.id))
    {
        return "A Conquest deck with that name already exists";
    }
    if (const std::optional<std::string> error = account_decks::deckRulesError(deck.deck))
    {
        return error;
    }
    if (const std::optional<std::string> error =
            allocationError(database, username, deck.id, deck.deck))
    {
        return error;
    }

    std::int64_t savedDeckId = deck.id;
    std::uint32_t savedRevision = deck.revision;
    if (deck.id == 0)
    {
        SQLite::Statement insert(
            database,
            "INSERT INTO conquest_decks (username, name, revision) VALUES (?, ?, 1)");
        insert.bind(1, username);
        insert.bind(2, deck.deck.name);
        insert.exec();
        savedDeckId = database.getLastInsertRowid();
        savedRevision = 1;
    }
    else
    {
        SQLite::Statement update(
            database,
            "UPDATE conquest_decks "
            "SET name = ?, revision = revision + 1, updated_at = CURRENT_TIMESTAMP "
            "WHERE username = ? AND id = ? AND revision = ?");
        update.bind(1, deck.deck.name);
        update.bind(2, username);
        update.bind(3, deck.id);
        update.bind(4, static_cast<std::int64_t>(deck.revision));
        if (update.exec() != 1)
        {
            return "Conquest deck changed since it was loaded; reload and try again";
        }
        savedRevision = currentRevision + 1;

        SQLite::Statement removeCards(
            database,
            "DELETE FROM conquest_deck_cards WHERE deck_id = ?");
        removeCards.bind(1, savedDeckId);
        removeCards.exec();
    }

    SQLite::Statement insertCard(
        database,
        "INSERT INTO conquest_deck_cards (deck_id, card_index, card_title) "
        "VALUES (?, ?, ?)");
    for (std::size_t i = 0; i < deck.deck.cardTitles.size(); ++i)
    {
        insertCard.reset();
        insertCard.bind(1, savedDeckId);
        insertCard.bind(2, static_cast<int>(i));
        insertCard.bind(3, deck.deck.cardTitles[i]);
        insertCard.exec();
    }

    transaction.commit();
    deck.id = savedDeckId;
    deck.revision = savedRevision;
    return std::nullopt;
}

std::optional<std::string> deleteDeck(
    SQLite::Database& database,
    const std::string& username,
    std::int64_t deckId,
    std::uint32_t expectedRevision)
{
    if (deckId <= 0 || expectedRevision == 0)
    {
        return "Invalid Conquest deck deletion request";
    }

    SQLite::Transaction transaction(database, SQLite::TransactionBehavior::IMMEDIATE);
    const std::optional<conquest_data::ConquestDeck> deck =
        loadDeck(database, username, deckId);
    if (!deck)
    {
        return "Conquest deck not found";
    }
    if (deck->revision != expectedRevision)
    {
        return "Conquest deck changed since it was loaded; reload and try again";
    }
    if (deckReservedByActiveEvent(database, username, deckId))
    {
        return "A Conquest deck committed to an active event cannot be deleted";
    }

    SQLite::Statement armyUse(
        database,
        "SELECT 1 FROM conquest_army_decks "
        "WHERE username = ? AND deck_id = ? LIMIT 1");
    armyUse.bind(1, username);
    armyUse.bind(2, deckId);
    if (armyUse.executeStep())
    {
        return "Remove this Conquest deck from your army before deleting it";
    }

    SQLite::Statement remove(
        database,
        "DELETE FROM conquest_decks WHERE username = ? AND id = ? AND revision = ?");
    remove.bind(1, username);
    remove.bind(2, deckId);
    remove.bind(3, static_cast<std::int64_t>(expectedRevision));
    if (remove.exec() != 1)
    {
        return "Conquest deck changed since it was loaded; reload and try again";
    }
    transaction.commit();
    return std::nullopt;
}

std::optional<std::string> saveArmy(
    SQLite::Database& database,
    const std::string& username,
    conquest_data::ConquestArmy& army)
{
    if (army.deckIds.empty() ||
        army.deckIds.size() > conquest_data::MaxConquestArmyDecks)
    {
        return "A Conquest army must contain 1-10 decks";
    }

    std::unordered_set<std::int64_t> uniqueDeckIds;
    for (const std::int64_t deckId : army.deckIds)
    {
        if (deckId <= 0 || !uniqueDeckIds.insert(deckId).second)
        {
            return "A Conquest army must contain distinct saved Conquest decks";
        }
    }

    SQLite::Transaction transaction(database, SQLite::TransactionBehavior::IMMEDIATE);

    for (const std::int64_t deckId : army.deckIds)
    {
        const std::optional<conquest_data::ConquestDeck> deck =
            loadDeck(database, username, deckId);
        if (!deck)
        {
            return "Conquest army contains a deck that was not found";
        }
        if (const std::optional<std::string> error =
                account_decks::deckRulesError(deck->deck))
        {
            return "Conquest army contains invalid deck " + deck->deck.name + ": " + *error;
        }
    }

    std::uint32_t currentRevision = 0;
    SQLite::Statement current(
        database,
        "SELECT revision FROM conquest_armies WHERE username = ? LIMIT 1");
    current.bind(1, username);
    if (current.executeStep())
    {
        const std::int64_t storedRevision = current.getColumn(0).getInt64();
        if (storedRevision <= 0 ||
            storedRevision > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            throw std::runtime_error("Invalid Conquest army revision in database");
        }
        currentRevision = static_cast<std::uint32_t>(storedRevision);
        if (army.revision != currentRevision)
        {
            return "Conquest army changed since it was loaded; reload and try again";
        }
        if (currentRevision == std::numeric_limits<std::uint32_t>::max())
        {
            return "Conquest army revision limit reached";
        }
    }
    else if (army.revision != 0)
    {
        return "Conquest army was not found; reload and try again";
    }

    std::uint32_t savedRevision = army.revision;
    if (currentRevision == 0)
    {
        SQLite::Statement insert(
            database,
            "INSERT INTO conquest_armies (username, revision) VALUES (?, 1)");
        insert.bind(1, username);
        insert.exec();
        savedRevision = 1;
    }
    else
    {
        SQLite::Statement update(
            database,
            "UPDATE conquest_armies "
            "SET revision = revision + 1, updated_at = CURRENT_TIMESTAMP "
            "WHERE username = ? AND revision = ?");
        update.bind(1, username);
        update.bind(2, static_cast<std::int64_t>(army.revision));
        if (update.exec() != 1)
        {
            return "Conquest army changed since it was loaded; reload and try again";
        }
        savedRevision = currentRevision + 1;

        SQLite::Statement removeMembers(
            database,
            "DELETE FROM conquest_army_decks WHERE username = ?");
        removeMembers.bind(1, username);
        removeMembers.exec();
    }

    SQLite::Statement insertMember(
        database,
        "INSERT INTO conquest_army_decks (username, slot_index, deck_id) "
        "VALUES (?, ?, ?)");
    for (std::size_t i = 0; i < army.deckIds.size(); ++i)
    {
        insertMember.reset();
        insertMember.bind(1, username);
        insertMember.bind(2, static_cast<int>(i));
        insertMember.bind(3, army.deckIds[i]);
        insertMember.exec();
    }

    transaction.commit();
    army.revision = savedRevision;
    return std::nullopt;
}

void purgeTokenCards(SQLite::Database& database)
{
    std::unordered_set<std::string> tokenTitles;
    try
    {
        for (const card_data::Card& card : account_catalog::cardLibrary())
        {
            if (game_data::isTokenCard(card))
            {
                tokenTitles.insert(card.title);
            }
        }
    }
    catch (...)
    {
        return;
    }
    if (tokenTitles.empty())
    {
        return;
    }

    SQLite::Transaction transaction(database, SQLite::TransactionBehavior::IMMEDIATE);
    std::unordered_set<std::int64_t> changedDeckIds;
    SQLite::Statement findDecks(
        database,
        "SELECT DISTINCT deck_id FROM conquest_deck_cards WHERE card_title = ?");
    for (const std::string& title : tokenTitles)
    {
        findDecks.reset();
        findDecks.bind(1, title);
        while (findDecks.executeStep())
        {
            changedDeckIds.insert(findDecks.getColumn(0).getInt64());
        }
    }

    SQLite::Statement removeCards(
        database,
        "DELETE FROM conquest_deck_cards WHERE card_title = ?");
    for (const std::string& title : tokenTitles)
    {
        removeCards.reset();
        removeCards.bind(1, title);
        removeCards.exec();
    }

    SQLite::Statement updateDeck(
        database,
        "UPDATE conquest_decks "
        "SET revision = revision + 1, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = ?");
    for (const std::int64_t deckId : changedDeckIds)
    {
        updateDeck.reset();
        updateDeck.bind(1, deckId);
        updateDeck.exec();
    }
    transaction.commit();
}

} // namespace account_conquest
