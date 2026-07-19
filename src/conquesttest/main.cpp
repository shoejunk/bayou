#include "../accounts/account_catalog.hpp"
#include "../accounts/account_conquest.hpp"
#include "../accounts/account_conquest_events.hpp"
#include "../accounts/account_decks.hpp"
#include "../gameserver/game_action.hpp"
#include "../shared/conquest_map.hpp"
#include "../shared/game_data.hpp"
#include "../shared/network.hpp"

#include <SFML/Network.hpp>
#include <SQLiteCpp/SQLiteCpp.h>
#include <sodium.h>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
using conquest_data::BattleKind;
using conquest_data::BattleStatus;
using conquest_data::ConquestArmy;
using conquest_data::ConquestDeck;
using conquest_data::EventDeckState;
using conquest_data::EventPhase;
using conquest_data::EventState;
using conquest_data::MoveOrder;
using conquest_data::StartingPlacement;

constexpr std::int64_t SeedTime = 1'000;

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

void requireContains(
    const std::optional<std::string>& error,
    const std::string& fragment,
    const std::string& context)
{
    require(error.has_value(), context + " unexpectedly succeeded");
    require(
        error->find(fragment) != std::string::npos,
        context + " returned unexpected error: " + *error);
}

void requireCommand(
    const account_conquest_events::CommandResult& result,
    const std::string& context)
{
    require(result.success, context + " failed: " + result.message);
}

std::vector<card_data::Card> syntheticCatalog()
{
    std::vector<card_data::Card> cards;

    card_data::Card hero;
    hero.title = "Conquest Hero";
    hero.type = "Hero";
    hero.imagePath = "frozen/original-hero.png";
    hero.integerValues.push_back({"heroCost", 10});
    cards.push_back(std::move(hero));

    card_data::Card braun;
    braun.title = "Braun Stonefist";
    braun.type = "Unit";
    braun.imagePath = "frozen/original-braun.png";
    braun.integerValues.push_back({"health", 7});
    cards.push_back(std::move(braun));

    for (int index = 1; index <= 40; ++index)
    {
        card_data::Card unit;
        unit.title = "Conquest Unit " + std::to_string(index);
        unit.type = "Unit";
        unit.imagePath = "frozen/unit-" + std::to_string(index) + ".png";
        unit.integerValues.push_back({"health", 2 + index % 5});
        cards.push_back(std::move(unit));
    }
    return cards;
}

void initializeBaseSchema(SQLite::Database& database)
{
    database.exec("PRAGMA foreign_keys = ON");
    database.exec(
        "CREATE TABLE accounts ("
        "username TEXT PRIMARY KEY NOT NULL,"
        "password_hash TEXT NOT NULL DEFAULT '',"
        "coins INTEGER NOT NULL DEFAULT 0,"
        "is_admin INTEGER NOT NULL DEFAULT 0,"
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ")");
    database.exec(
        "CREATE TABLE decks ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "UNIQUE(username, name),"
        "FOREIGN KEY(username) REFERENCES accounts(username) ON DELETE CASCADE"
        ")");
    database.exec(
        "CREATE TABLE deck_cards ("
        "deck_id INTEGER NOT NULL,"
        "card_index INTEGER NOT NULL,"
        "card_title TEXT NOT NULL,"
        "PRIMARY KEY(deck_id, card_index),"
        "FOREIGN KEY(deck_id) REFERENCES decks(id) ON DELETE CASCADE"
        ")");
    database.exec(
        "CREATE TABLE starter_deck_cards ("
        "card_index INTEGER PRIMARY KEY NOT NULL,"
        "card_title TEXT NOT NULL"
        ")");
    database.exec(
        "CREATE TABLE card_collections ("
        "username TEXT NOT NULL,"
        "card_title TEXT NOT NULL,"
        "copies INTEGER NOT NULL DEFAULT 0,"
        "PRIMARY KEY(username, card_title),"
        "FOREIGN KEY(username) REFERENCES accounts(username) ON DELETE CASCADE"
        ")");
    database.exec(
        "CREATE TABLE conquest_decks ("
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
    database.exec(
        "CREATE TABLE conquest_deck_cards ("
        "deck_id INTEGER NOT NULL,"
        "card_index INTEGER NOT NULL CHECK(card_index >= 0),"
        "card_title TEXT NOT NULL,"
        "PRIMARY KEY(deck_id, card_index),"
        "FOREIGN KEY(deck_id) REFERENCES conquest_decks(id) ON DELETE CASCADE"
        ")");
    database.exec(
        "CREATE TABLE conquest_armies ("
        "username TEXT PRIMARY KEY NOT NULL,"
        "revision INTEGER NOT NULL DEFAULT 1 CHECK(revision > 0),"
        "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "FOREIGN KEY(username) REFERENCES accounts(username) ON DELETE CASCADE"
        ")");
    database.exec(
        "CREATE TABLE conquest_army_decks ("
        "username TEXT NOT NULL,"
        "slot_index INTEGER NOT NULL CHECK(slot_index >= 0 AND slot_index < 10),"
        "deck_id INTEGER NOT NULL,"
        "PRIMARY KEY(username, slot_index),"
        "UNIQUE(username, deck_id),"
        "FOREIGN KEY(username) REFERENCES conquest_armies(username) ON DELETE CASCADE,"
        "FOREIGN KEY(username, deck_id) REFERENCES conquest_decks(username, id) ON DELETE CASCADE"
        ")");
}

class Fixture
{
public:
    Fixture()
        : database(":memory:", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)
    {
        catalog = syntheticCatalog();
        account_catalog::setCardLibrary(catalog);
        initializeBaseSchema(database);
        account_conquest_events::initializeSchema(database, SeedTime);
        SQLite::Statement query(
            database,
            "SELECT id FROM conquest_events WHERE seed_key = 'default-dark-realms'");
        require(query.executeStep(), "default Dark Realms event was not seeded");
        eventId = static_cast<std::uint64_t>(query.getColumn(0).getInt64());
    }

    void addAccount(
        const std::string& username,
        int copiesPerCard = 30,
        bool isAdmin = false,
        int coins = 100)
    {
        SQLite::Statement account(
            database,
            "INSERT INTO accounts (username, password_hash, coins, is_admin) VALUES (?, '', ?, ?)");
        account.bind(1, username);
        account.bind(2, coins);
        account.bind(3, isAdmin ? 1 : 0);
        account.exec();

        SQLite::Statement collection(
            database,
            "INSERT INTO card_collections (username, card_title, copies) VALUES (?, ?, ?)");
        for (const card_data::Card& card : catalog)
        {
            collection.reset();
            collection.bind(1, username);
            collection.bind(2, card.title);
            collection.bind(3, copiesPerCard);
            collection.exec();
        }
    }

    int coins(const std::string& username)
    {
        SQLite::Statement query(
            database,
            "SELECT coins FROM accounts WHERE username = ?");
        query.bind(1, username);
        require(query.executeStep(), "account coin balance was not found");
        return query.getColumn(0).getInt();
    }

    void setCoins(const std::string& username, int value)
    {
        SQLite::Statement update(
            database,
            "UPDATE accounts SET coins = ? WHERE username = ?");
        update.bind(1, value);
        update.bind(2, username);
        require(update.exec() == 1, "account coin balance could not be updated");
    }

    void setCopies(const std::string& username, const std::string& title, int copies)
    {
        SQLite::Statement update(
            database,
            "UPDATE card_collections SET copies = ? WHERE username = ? AND card_title = ?");
        update.bind(1, copies);
        update.bind(2, username);
        update.bind(3, title);
        require(update.exec() == 1, "collection row to update was not found");
    }

    ConquestDeck saveDeck(
        const std::string& username,
        const std::string& name,
        const std::vector<std::string>& cardTitles)
    {
        ConquestDeck deck;
        deck.deck.name = name;
        deck.deck.cardTitles = cardTitles;
        const std::optional<std::string> error =
            account_conquest::saveDeck(database, username, deck);
        require(!error, "saving " + name + " failed: " + error.value_or(""));
        return deck;
    }

    ConquestArmy saveArmy(const std::string& username, const std::vector<std::int64_t>& ids)
    {
        ConquestArmy army;
        army.deckIds = ids;
        const std::optional<std::string> error =
            account_conquest::saveArmy(database, username, army);
        require(!error, "saving army failed: " + error.value_or(""));
        return army;
    }

    std::vector<std::string> standardDeckTitles(int firstUnit = 1) const
    {
        std::vector<std::string> titles{"Conquest Hero"};
        for (int unit = firstUnit; unit < firstUnit + 10; ++unit)
        {
            titles.push_back("Conquest Unit " + std::to_string(unit));
            titles.push_back("Conquest Unit " + std::to_string(unit));
        }
        return titles;
    }

    std::vector<std::string> braunDeckTitles() const
    {
        std::vector<std::string> titles{"Conquest Hero", "Braun Stonefist", "Braun Stonefist"};
        for (int unit = 1; unit <= 9; ++unit)
        {
            titles.push_back("Conquest Unit " + std::to_string(unit));
            titles.push_back("Conquest Unit " + std::to_string(unit));
        }
        return titles;
    }

    EventState state(const std::string& username, std::int64_t now)
    {
        std::string error;
        std::optional<EventState> value = account_conquest_events::loadEventState(
            database,
            eventId,
            username,
            error,
            now);
        require(value.has_value(), "loading event state failed: " + error);
        return *value;
    }

    void startEvent(std::int64_t startAt = 2'000)
    {
        SQLite::Statement close(
            database,
            "UPDATE conquest_events SET registration_ends_at = ? WHERE id = ?");
        close.bind(1, startAt);
        close.bind(2, static_cast<std::int64_t>(eventId));
        close.exec();
        requireCommand(
            account_conquest_events::resolveEvent(database, eventId, startAt),
            "starting event");
        const EventState event = state("", startAt);
        require(event.summary.phase == EventPhase::Planning, "event did not enter planning");
        require(event.summary.turn == 1, "event did not begin on turn one");
    }

    SQLite::Database database;
    std::vector<card_data::Card> catalog;
    std::uint64_t eventId = 0;
};

const EventDeckState& findDeck(
    const EventState& state,
    const std::string& owner,
    int armySlot)
{
    const auto found = std::find_if(
        state.decks.begin(),
        state.decks.end(),
        [&](const EventDeckState& deck) {
            return deck.owner == owner && deck.armySlot == armySlot;
        });
    require(found != state.decks.end(), "event deck was not found");
    return *found;
}

int conquestEventCount(SQLite::Database& database)
{
    SQLite::Statement query(database, "SELECT COUNT(*) FROM conquest_events");
    require(query.executeStep(), "Conquest event count query returned no row");
    return query.getColumn(0).getInt();
}

std::int64_t eventIdForSeed(SQLite::Database& database, const std::string& seedKey)
{
    SQLite::Statement query(
        database,
        "SELECT id FROM conquest_events WHERE seed_key = ?");
    query.bind(1, seedKey);
    require(query.executeStep(), "Conquest event was not found for seed " + seedKey);
    return query.getColumn(0).getInt64();
}

int eventResourceCount(
    SQLite::Database& database,
    const std::string& table,
    std::int64_t eventId)
{
    require(
        table == "conquest_regions" || table == "conquest_event_catalog_cards",
        "unsupported Conquest resource table in test");
    SQLite::Statement query(
        database,
        "SELECT COUNT(*) FROM " + table + " WHERE event_id = ?");
    query.bind(1, eventId);
    require(query.executeStep(), "Conquest resource count query returned no row");
    return query.getColumn(0).getInt();
}

std::vector<std::uint8_t> serializedCard(const card_data::Card& card)
{
    sf::Packet packet;
    card_data::writeCardListHeader(packet, 1);
    card_data::writeCard(packet, card);
    const auto* first = static_cast<const std::uint8_t*>(packet.getData());
    require(first != nullptr && packet.getDataSize() > 0, "test card did not serialize");
    return {first, first + packet.getDataSize()};
}

std::vector<std::uint8_t> frozenCatalogBlob(
    SQLite::Database& database,
    std::int64_t eventId,
    const std::string& title)
{
    SQLite::Statement query(
        database,
        "SELECT card_blob FROM conquest_event_catalog_cards "
        "WHERE event_id = ? AND card_title = ?");
    query.bind(1, eventId);
    query.bind(2, title);
    require(query.executeStep(), "frozen catalog card was not found: " + title);
    const SQLite::Column blob = query.getColumn(0);
    const auto* first = static_cast<const std::uint8_t*>(blob.getBlob());
    const int size = blob.getBytes();
    require(first != nullptr && size > 0, "frozen catalog card has an empty blob: " + title);
    return {first, first + size};
}

void testMapMetadata()
{
    const std::vector<int> expectedEdges{1, 2, 3, 8, 13, 18, 20, 19, 14, 9, 4};
    require(
        std::vector<int>(
            conquest_map::DarkRealmsEdgeRegions.begin(),
            conquest_map::DarkRealmsEdgeRegions.end()) == expectedEdges,
        "Dark Realms edge list changed");
    require(conquest_map::DarkRealmsRegions.size() == 20, "Dark Realms must have 20 regions");
    require(!conquest_map::areAdjacent(1, 0), "padding zero must not be an adjacent region");
    require(!conquest_map::areAdjacent(1, 21), "out-of-range region must not be adjacent");

    for (const conquest_map::RegionDefinition& region : conquest_map::DarkRealmsRegions)
    {
        require(region.id >= 1 && region.id <= 20, "region id is outside the map");
        require(
            conquest_map::isEdgeRegion(region.id) ==
                (std::find(expectedEdges.begin(), expectedEdges.end(), region.id) != expectedEdges.end()),
            "compiled edge flag does not match edge list for region " + std::to_string(region.id));
        for (const int neighbour : region.neighbours)
        {
            if (neighbour == 0)
            {
                continue;
            }
            require(
                conquest_map::areAdjacent(neighbour, region.id),
                "map edge is not symmetric: " + std::to_string(region.id) + "-" +
                    std::to_string(neighbour));
        }
    }
}

void testConquestAllocationAndRegularDeckIsolation()
{
    Fixture fixture;
    fixture.addAccount("alpha", 20);
    fixture.setCopies("alpha", "Braun Stonefist", 4);

    const std::vector<std::string> braunDeck = fixture.braunDeckTitles();
    deck_data::Deck regular;
    regular.name = "Regular Braun";
    regular.cardTitles = braunDeck;
    account_decks::saveDeck(fixture.database, "alpha", "", regular);

    const ConquestDeck first = fixture.saveDeck("alpha", "Conquest Braun One", braunDeck);
    const ConquestDeck second = fixture.saveDeck("alpha", "Conquest Braun Two", braunDeck);
    require(first.id != second.id, "Conquest decks did not get stable distinct ids");

    std::vector<std::string> fifthBraun{"Conquest Hero", "Braun Stonefist", "Conquest Unit 10"};
    for (int unit = 11; unit <= 19; ++unit)
    {
        fifthBraun.push_back("Conquest Unit " + std::to_string(unit));
        fifthBraun.push_back("Conquest Unit " + std::to_string(unit));
    }
    ConquestDeck rejected;
    rejected.deck.name = "One Braun Too Many";
    rejected.deck.cardTitles = std::move(fifthBraun);
    requireContains(
        account_conquest::saveDeck(fixture.database, "alpha", rejected),
        "5 copies of Braun Stonefist",
        "fifth Conquest allocation");
    require(rejected.id == 0 && rejected.revision == 0, "failed save mutated the new deck identity");
}

void testArmyRules()
{
    Fixture fixture;
    fixture.addAccount("alpha", 40);
    fixture.addAccount("bravo", 40);

    std::vector<ConquestDeck> decks;
    for (int index = 0; index < 10; ++index)
    {
        decks.push_back(fixture.saveDeck(
            "alpha",
            "Army Deck " + std::to_string(index + 1),
            fixture.standardDeckTitles()));
    }
    const ConquestDeck foreign = fixture.saveDeck("bravo", "Foreign Deck", fixture.standardDeckTitles());

    ConquestArmy empty;
    requireContains(
        account_conquest::saveArmy(fixture.database, "alpha", empty),
        "1-10",
        "empty army");

    ConquestArmy duplicate;
    duplicate.deckIds = {decks[0].id, decks[0].id};
    requireContains(
        account_conquest::saveArmy(fixture.database, "alpha", duplicate),
        "distinct",
        "duplicate army member");

    ConquestArmy foreignArmy;
    foreignArmy.deckIds = {foreign.id};
    requireContains(
        account_conquest::saveArmy(fixture.database, "alpha", foreignArmy),
        "not found",
        "foreign army member");

    ConquestArmy one;
    one.deckIds = {decks[0].id};
    require(
        !account_conquest::saveArmy(fixture.database, "alpha", one),
        "one-deck army should be accepted");
    require(one.revision == 1, "new army revision should be one");

    ConquestArmy ten;
    ten.revision = one.revision;
    for (const ConquestDeck& deck : decks)
    {
        ten.deckIds.push_back(deck.id);
    }
    require(
        !account_conquest::saveArmy(fixture.database, "alpha", ten),
        "ten-deck army should be accepted");
    require(ten.revision == 2, "updated army revision should increment");

    ConquestArmy eleven;
    eleven.revision = ten.revision;
    eleven.deckIds.assign(11, decks[0].id);
    requireContains(
        account_conquest::saveArmy(fixture.database, "alpha", eleven),
        "1-10",
        "eleven-deck army");
}

void testRegistrationAndActiveDeckLock()
{
    Fixture fixture;
    fixture.addAccount("alpha");
    fixture.addAccount("bravo");
    fixture.addAccount("charlie");
    ConquestDeck alpha = fixture.saveDeck("alpha", "Alpha", fixture.standardDeckTitles());
    const ConquestDeck bravo = fixture.saveDeck("bravo", "Bravo", fixture.standardDeckTitles());
    const ConquestDeck charlie = fixture.saveDeck("charlie", "Charlie", fixture.standardDeckTitles());
    fixture.saveArmy("alpha", {alpha.id});
    fixture.saveArmy("bravo", {bravo.id});
    fixture.saveArmy("charlie", {charlie.id});

    constexpr std::int64_t FirstDeadline = SeedTime + 24 * 60 * 60;
    constexpr std::int64_t AlphaJoinTime = FirstDeadline + 5;
    requireCommand(
        account_conquest_events::joinEvent(
            fixture.database,
            fixture.eventId,
            "alpha",
            {{static_cast<std::uint64_t>(alpha.id), 1}},
            AlphaJoinTime),
        "first entrant lazily extending expired registration");
    EventState onePlayer = fixture.state("alpha", AlphaJoinTime);
    require(onePlayer.summary.phase == EventPhase::Registration,
        "first late entrant should leave registration open");
    require(onePlayer.summary.participantCount == 1,
        "first late entrant was not registered");
    require(onePlayer.summary.registrationEndsAt == AlphaJoinTime + 24 * 60 * 60,
        "first late entrant did not extend registration");

    alpha.deck.name = "Locked Rename";
    requireContains(
        account_conquest::saveDeck(fixture.database, "alpha", alpha),
        "active event",
        "editing active-event deck");
    requireContains(
        account_conquest::deleteDeck(fixture.database, "alpha", alpha.id, alpha.revision),
        "active event",
        "deleting active-event deck");

    const std::int64_t BravoJoinTime = onePlayer.summary.registrationEndsAt + 5;
    requireCommand(
        account_conquest_events::joinEvent(
            fixture.database,
            fixture.eventId,
            "bravo",
            {{static_cast<std::uint64_t>(bravo.id), 3}},
            BravoJoinTime),
        "second entrant lazily extending expired registration");
    EventState twoPlayers = fixture.state("alpha", BravoJoinTime);
    require(twoPlayers.summary.phase == EventPhase::Registration,
        "second late entrant should retain its newly extended registration window");
    require(twoPlayers.summary.participantCount == 2,
        "second late entrant was not registered");
    require(twoPlayers.summary.registrationEndsAt == BravoJoinTime + 24 * 60 * 60,
        "second late entrant did not extend registration");

    const std::int64_t CharlieJoinTime = twoPlayers.summary.registrationEndsAt + 5;
    const account_conquest_events::CommandResult thirdJoin =
        account_conquest_events::joinEvent(
            fixture.database,
            fixture.eventId,
            "charlie",
            {{static_cast<std::uint64_t>(charlie.id), 8}},
            CharlieJoinTime);
    require(!thirdJoin.success && thirdJoin.message.find("closed") != std::string::npos,
        "late third entrant should be rejected after starting planning");
    EventState started = fixture.state("charlie", CharlieJoinTime + 1);
    require(started.summary.phase == EventPhase::Planning,
        "rejected late third join rolled back the planning transition");
    require(started.summary.turn == 1, "lazy planning transition must start turn one");
    require(started.summary.participantCount == 2 && !started.summary.joined,
        "rejected third entrant changed the participant set");
}

void testConquestEntryFee()
{
    Fixture fixture;
    fixture.addAccount("alpha", 30, false, 10);
    const ConquestDeck alpha =
        fixture.saveDeck("alpha", "Alpha", fixture.standardDeckTitles());
    fixture.saveArmy("alpha", {alpha.id});

    const account_conquest_events::CommandResult invalidPlacement =
        account_conquest_events::joinEvent(
            fixture.database,
            fixture.eventId,
            "alpha",
            {{static_cast<std::uint64_t>(alpha.id), 5}},
            1'100);
    require(!invalidPlacement.success, "invalid Conquest placement unexpectedly joined");
    require(fixture.coins("alpha") == 10, "failed join charged the Conquest entry fee");

    fixture.setCoins("alpha", conquest_data::ConquestEntryFeeCoins - 1);
    const account_conquest_events::CommandResult insufficientCoins =
        account_conquest_events::joinEvent(
            fixture.database,
            fixture.eventId,
            "alpha",
            {{static_cast<std::uint64_t>(alpha.id), 1}},
            1'100);
    require(
        !insufficientCoins.success &&
            insufficientCoins.message.find("5 coins") != std::string::npos,
        "underfunded Conquest join was not rejected with the entry fee");
    require(
        fixture.coins("alpha") == conquest_data::ConquestEntryFeeCoins - 1,
        "underfunded Conquest join changed the coin balance");

    fixture.setCoins("alpha", conquest_data::ConquestEntryFeeCoins);
    requireCommand(
        account_conquest_events::joinEvent(
            fixture.database,
            fixture.eventId,
            "alpha",
            {{static_cast<std::uint64_t>(alpha.id), 1}},
            1'100),
        "funded Conquest join");
    require(fixture.coins("alpha") == 0, "successful Conquest join did not charge five coins");

    const account_conquest_events::CommandResult duplicate =
        account_conquest_events::joinEvent(
            fixture.database,
            fixture.eventId,
            "alpha",
            {{static_cast<std::uint64_t>(alpha.id), 1}},
            1'100);
    require(!duplicate.success, "duplicate Conquest join unexpectedly succeeded");
    require(fixture.coins("alpha") == 0, "duplicate Conquest join charged another entry fee");
}

void testAdminForcedStart()
{
    Fixture fixture;
    fixture.addAccount("alpha", 30, true);
    fixture.addAccount("bravo");
    const ConquestDeck alpha =
        fixture.saveDeck("alpha", "Alpha", fixture.standardDeckTitles());
    const ConquestDeck bravo =
        fixture.saveDeck("bravo", "Bravo", fixture.standardDeckTitles());
    fixture.saveArmy("alpha", {alpha.id});
    fixture.saveArmy("bravo", {bravo.id});

    constexpr std::int64_t AlphaJoinTime = SeedTime + 10;
    requireCommand(
        account_conquest_events::joinEvent(
            fixture.database,
            fixture.eventId,
            "alpha",
            {{static_cast<std::uint64_t>(alpha.id), 1}},
            AlphaJoinTime),
        "admin joining before forced start");

    const account_conquest_events::CommandResult tooFewPlayers =
        account_conquest_events::forceStartEvent(
            fixture.database, fixture.eventId, "alpha", AlphaJoinTime + 1);
    require(
        !tooFewPlayers.success && tooFewPlayers.message.find("two players") != std::string::npos,
        "admin force-start should require two participants");
    require(
        fixture.state("alpha", AlphaJoinTime + 1).summary.phase == EventPhase::Registration,
        "failed one-player force-start changed the event phase");

    constexpr std::int64_t BravoJoinTime = AlphaJoinTime + 2;
    requireCommand(
        account_conquest_events::joinEvent(
            fixture.database,
            fixture.eventId,
            "bravo",
            {{static_cast<std::uint64_t>(bravo.id), 3}},
            BravoJoinTime),
        "second player joining before forced start");

    const account_conquest_events::CommandResult nonAdmin =
        account_conquest_events::forceStartEvent(
            fixture.database, fixture.eventId, "bravo", BravoJoinTime + 1);
    require(
        !nonAdmin.success && nonAdmin.message.find("Admin") != std::string::npos,
        "non-admin force-start was not denied");
    require(
        fixture.state("bravo", BravoJoinTime + 1).summary.phase == EventPhase::Registration,
        "denied non-admin force-start changed the event phase");

    constexpr std::int64_t ForceStartTime = BravoJoinTime + 2;
    requireCommand(
        account_conquest_events::forceStartEvent(
            fixture.database, fixture.eventId, "alpha", ForceStartTime),
        "admin force-starting registration");
    const EventState started = fixture.state("alpha", ForceStartTime);
    require(started.summary.phase == EventPhase::Planning,
        "admin force-start did not enter planning");
    require(started.summary.turn == 1,
        "admin force-start did not begin turn one");
    require(started.summary.registrationEndsAt == ForceStartTime,
        "admin force-start did not close registration immediately");
    require(started.summary.turnEndsAt == ForceStartTime + 24 * 60 * 60,
        "admin force-start did not set the first planning deadline");
}

void testSecretVacateAndReenterOrders()
{
    Fixture fixture;
    fixture.addAccount("alpha");
    fixture.addAccount("bravo");
    const ConquestDeck alphaOne = fixture.saveDeck("alpha", "Alpha One", fixture.standardDeckTitles());
    const ConquestDeck alphaTwo = fixture.saveDeck("alpha", "Alpha Two", fixture.standardDeckTitles());
    const ConquestDeck bravo = fixture.saveDeck("bravo", "Bravo", fixture.standardDeckTitles());
    fixture.saveArmy("alpha", {alphaOne.id, alphaTwo.id});
    fixture.saveArmy("bravo", {bravo.id});
    requireCommand(
        account_conquest_events::joinEvent(
            fixture.database,
            fixture.eventId,
            "alpha",
            {{static_cast<std::uint64_t>(alphaOne.id), 1},
             {static_cast<std::uint64_t>(alphaTwo.id), 2}},
            1'100),
        "alpha joining with two decks");
    requireCommand(
        account_conquest_events::joinEvent(
            fixture.database,
            fixture.eventId,
            "bravo",
            {{static_cast<std::uint64_t>(bravo.id), 3}},
            1'100),
        "bravo joining");
    fixture.startEvent();

    const EventState before = fixture.state("alpha", 2'001);
    const EventDeckState alphaAtOne = findDeck(before, "alpha", 0);
    const EventDeckState alphaAtTwo = findDeck(before, "alpha", 1);
    require(alphaAtOne.regionId == 1 && alphaAtTwo.regionId == 2, "unexpected start positions");

    requireCommand(
        account_conquest_events::submitOrders(
            fixture.database,
            fixture.eventId,
            "alpha",
            {{alphaAtOne.id, 4}, {alphaAtTwo.id, 1}},
            2'010),
        "vacate-and-reenter order set");

    const EventState alphaView = fixture.state("alpha", 2'011);
    require(findDeck(alphaView, "alpha", 0).destinationRegionId == 4, "owner cannot see own order");
    require(findDeck(alphaView, "alpha", 1).destinationRegionId == 1, "owner cannot see reentry order");
    const EventState bravoView = fixture.state("bravo", 2'011);
    require(
        findDeck(bravoView, "alpha", 0).destinationRegionId == 1 &&
            findDeck(bravoView, "alpha", 1).destinationRegionId == 2,
        "opponent saw secret move destinations during planning");

    requireCommand(
        account_conquest_events::submitOrders(
            fixture.database,
            fixture.eventId,
            "bravo",
            {},
            2'012),
        "bravo pass orders");
    const EventState after = fixture.state("alpha", 2'013);
    require(after.summary.phase == EventPhase::Planning && after.summary.turn == 2,
        "conflict-free orders did not advance the turn");
    require(
        findDeck(after, "alpha", 0).regionId == 4 &&
            findDeck(after, "alpha", 1).regionId == 1,
        "simultaneous vacate-and-reenter positions were not applied");
}

struct BattleFixture
{
    std::unique_ptr<Fixture> fixture;
    std::uint64_t battleId = 0;
    BattleKind kind = BattleKind::Region;
};

BattleFixture makeBattleFixture(BattleKind kind)
{
    auto fixture = std::make_unique<Fixture>();
    fixture->addAccount("alpha");
    fixture->addAccount("bravo");
    const ConquestDeck alpha = fixture->saveDeck("alpha", "Alpha Battle", fixture->braunDeckTitles());
    const ConquestDeck bravo = fixture->saveDeck("bravo", "Bravo Battle", fixture->standardDeckTitles());
    fixture->saveArmy("alpha", {alpha.id});
    fixture->saveArmy("bravo", {bravo.id});
    requireCommand(
        account_conquest_events::joinEvent(
            fixture->database,
            fixture->eventId,
            "alpha",
            {{static_cast<std::uint64_t>(alpha.id), 1}},
            1'100),
        "alpha joining battle fixture");
    requireCommand(
        account_conquest_events::joinEvent(
            fixture->database,
            fixture->eventId,
            "bravo",
            {{static_cast<std::uint64_t>(bravo.id), 2}},
            1'100),
        "bravo joining battle fixture");
    fixture->startEvent();
    const EventState before = fixture->state("alpha", 2'001);
    const std::uint64_t alphaEventDeck = findDeck(before, "alpha", 0).id;
    const std::uint64_t bravoEventDeck = findDeck(before, "bravo", 0).id;

    if (kind == BattleKind::Region)
    {
        requireCommand(
            account_conquest_events::submitOrders(
                fixture->database,
                fixture->eventId,
                "alpha",
                {{alphaEventDeck, 5}},
                2'010),
            "alpha same-region order");
        requireCommand(
            account_conquest_events::submitOrders(
                fixture->database,
                fixture->eventId,
                "bravo",
                {{bravoEventDeck, 5}},
                2'011),
            "bravo same-region order");
    }
    else
    {
        requireCommand(
            account_conquest_events::submitOrders(
                fixture->database,
                fixture->eventId,
                "alpha",
                {{alphaEventDeck, 2}},
                2'010),
            "alpha crossing order");
        requireCommand(
            account_conquest_events::submitOrders(
                fixture->database,
                fixture->eventId,
                "bravo",
                {{bravoEventDeck, 1}},
                2'011),
            "bravo crossing order");
    }

    const EventState resolving = fixture->state("alpha", 2'012);
    require(resolving.summary.phase == EventPhase::Resolving, "battle did not enter resolving phase");
    require(resolving.battles.size() == 1, "expected exactly one conflict battle");
    require(resolving.battles[0].kind == kind, "battle has the wrong conflict kind");
    require(resolving.battles[0].status == BattleStatus::Ready, "conflict battle is not ready");
    require(resolving.battles[0].canJoin, "battle participant cannot join ready battle");
    return {std::move(fixture), resolving.battles[0].id, kind};
}

void testCollisionAndCrossingBattles()
{
    BattleFixture collision = makeBattleFixture(BattleKind::Region);
    require(collision.battleId != 0, "same-region battle has no id");
    BattleFixture crossing = makeBattleFixture(BattleKind::Crossing);
    require(crossing.battleId != 0, "crossing battle has no id");
}

void testFrozenBattleDataActionsAndResult()
{
    BattleFixture battleFixture = makeBattleFixture(BattleKind::Region);
    Fixture& fixture = *battleFixture.fixture;

    std::vector<card_data::Card> changed = fixture.catalog;
    for (card_data::Card& card : changed)
    {
        if (card.title == "Braun Stonefist")
        {
            card.imagePath = "live/catalog-was-changed.png";
        }
    }
    account_catalog::setCardLibrary(std::move(changed));

    int alphaNumber = 0;
    std::string capability;
    std::string error;
    const std::optional<conquest_data::BattleData> data =
        account_conquest_events::loadBattleDataForCoordinator(
            fixture.database,
            battleFixture.battleId,
            "alpha",
            alphaNumber,
            capability,
            error);
    require(data.has_value(), "loading frozen BattleData failed: " + error);
    require(alphaNumber == 1 || alphaNumber == 2, "participant number was not assigned");
    require(
        capability.size() == conquest_data::ConquestBattleCapabilityHexLength,
        "battle capability has the wrong size");
    std::string wrongCapability = capability;
    wrongCapability[0] = wrongCapability[0] == '0' ? '1' : '0';
    require(
        !account_conquest_events::reloadBattleDataForCoordinator(
             fixture.database,
             battleFixture.battleId,
             wrongCapability,
             error).has_value(),
        "an invalid battle capability reloaded BattleData");
    require(data->catalog.size() == fixture.catalog.size(), "frozen catalog is incomplete");
    require(data->cardsOne.size() == 21 && data->cardsTwo.size() == 21,
        "frozen battle deck definitions are incomplete");
    const auto frozenBraun = std::find_if(
        data->catalog.begin(),
        data->catalog.end(),
        [](const card_data::Card& card) { return card.title == "Braun Stonefist"; });
    require(frozenBraun != data->catalog.end(), "Braun is missing from frozen catalog");
    require(
        frozenBraun->imagePath == "frozen/original-braun.png",
        "BattleData used the changed live catalog instead of the event snapshot");

    conquest_data::BattleAction unsupported;
    unsupported.sequence = 1;
    unsupported.playerNumber = alphaNumber;
    unsupported.actionType =
        static_cast<std::uint8_t>(network::MessageType::CreateAccount);
    require(
        !account_conquest_events::appendBattleActionWithCapability(
             fixture.database, battleFixture.battleId, capability, unsupported).success,
        "unsupported tactical action type was persisted");

    conquest_data::BattleAction second;
    second.sequence = 2;
    second.playerNumber = alphaNumber;
    second.actionType = static_cast<std::uint8_t>(network::MessageType::MovePiece);
    second.argumentOne = 12;
    require(
        !account_conquest_events::appendBattleActionWithCapability(
             fixture.database, battleFixture.battleId, capability, second).success,
        "out-of-order durable action was accepted");

    conquest_data::BattleAction first;
    first.sequence = 1;
    first.playerNumber = alphaNumber;
    first.actionType = static_cast<std::uint8_t>(network::MessageType::PlaceHero);
    first.argumentOne = 3;
    first.argumentTwo = 4;
    first.argumentThree = 5;
    requireCommand(
        account_conquest_events::appendBattleActionWithCapability(
            fixture.database, battleFixture.battleId, capability, first),
        "first durable action");
    requireCommand(
        account_conquest_events::appendBattleActionWithCapability(
            fixture.database, battleFixture.battleId, capability, first),
        "idempotent action retry");
    conquest_data::BattleAction conflicting = first;
    conflicting.argumentThree = 99;
    require(
        !account_conquest_events::appendBattleActionWithCapability(
             fixture.database, battleFixture.battleId, capability, conflicting).success,
        "conflicting duplicate action was accepted");
    requireCommand(
        account_conquest_events::appendBattleActionWithCapability(
            fixture.database, battleFixture.battleId, capability, second),
        "second durable action");

    const std::optional<conquest_data::BattleData> reloaded =
        account_conquest_events::reloadBattleDataForCoordinator(
            fixture.database,
            battleFixture.battleId,
            capability,
            error);
    require(reloaded.has_value(), "reloading BattleData failed: " + error);
    require(reloaded->actions.size() == 2, "durable action log has wrong size");
    require(
        reloaded->actions[0].sequence == 1 && reloaded->actions[1].sequence == 2,
        "durable actions were not loaded in sequence order");

    requireCommand(
        account_conquest_events::applyBattleResultWithCapability(
            fixture.database,
            battleFixture.battleId,
            capability,
            alphaNumber,
            2'100),
        "battle result");
    requireCommand(
        account_conquest_events::applyBattleResultWithCapability(
            fixture.database,
            battleFixture.battleId,
            capability,
            alphaNumber,
            2'101),
        "idempotent battle result retry");
    const EventState advanced = fixture.state("alpha", 2'102);
    require(
        advanced.summary.phase == EventPhase::Complete && advanced.summary.winner == "alpha",
        "last standing battle winner did not win the conquest");
    const EventDeckState& alphaDeck = findDeck(advanced, "alpha", 0);
    const EventDeckState& bravoDeck = findDeck(advanced, "bravo", 0);
    require(alphaDeck.deployed && alphaDeck.regionId == 5, "winning deck did not complete its move");
    require(bravoDeck.eliminated && !bravoDeck.deployed, "losing deck was not eliminated");
    const auto bravoPlayer = std::find_if(
        advanced.players.begin(),
        advanced.players.end(),
        [](const conquest_data::PlayerState& player) { return player.username == "bravo"; });
    require(bravoPlayer != advanced.players.end() && bravoPlayer->eliminated,
        "player with no surviving map decks was not eliminated from the conquest");
}

void testReinforcementRules()
{
    Fixture fixture;
    fixture.addAccount("alpha");
    fixture.addAccount("bravo");
    const ConquestDeck alphaOne = fixture.saveDeck("alpha", "Alpha One", fixture.standardDeckTitles());
    const ConquestDeck alphaTwo = fixture.saveDeck("alpha", "Alpha Two", fixture.standardDeckTitles());
    const ConquestDeck alphaThree = fixture.saveDeck("alpha", "Alpha Three", fixture.standardDeckTitles());
    const ConquestDeck bravo = fixture.saveDeck("bravo", "Bravo", fixture.standardDeckTitles());
    fixture.saveArmy("alpha", {alphaOne.id, alphaTwo.id, alphaThree.id});
    fixture.saveArmy("bravo", {bravo.id});
    requireCommand(
        account_conquest_events::joinEvent(
            fixture.database,
            fixture.eventId,
            "alpha",
            {{static_cast<std::uint64_t>(alphaOne.id), 1}},
            1'100),
        "alpha joining reinforcement fixture");
    requireCommand(
        account_conquest_events::joinEvent(
            fixture.database,
            fixture.eventId,
            "bravo",
            {{static_cast<std::uint64_t>(bravo.id), 18}},
            1'100),
        "bravo joining reinforcement fixture");
    fixture.startEvent(2'000);

    fixture.database.exec(
        "UPDATE conquest_regions SET controller = ''");
    SQLite::Statement control(
        fixture.database,
        "UPDATE conquest_regions SET controller = 'alpha' WHERE event_id = ? AND region_id = ?");
    for (const int region : {1, 4, 5, 6, 9, 10, 14, 15})
    {
        control.reset();
        control.bind(1, static_cast<std::int64_t>(fixture.eventId));
        control.bind(2, region);
        control.exec();
    }
    SQLite::Statement moveInitial(
        fixture.database,
        "UPDATE conquest_event_decks SET region_id = 5, destination_region_id = 5 "
        "WHERE event_id = ? AND owner = 'alpha' AND deployed = 1");
    moveInitial.bind(1, static_cast<std::int64_t>(fixture.eventId));
    moveInitial.exec();
    SQLite::Statement tune(
        fixture.database,
        "UPDATE conquest_events SET turn_ends_at = 5000, reinforcement_cooldown_seconds = 100 "
        "WHERE id = ?");
    tune.bind(1, static_cast<std::int64_t>(fixture.eventId));
    tune.exec();

    EventState before = fixture.state("alpha", 2'010);
    require(findDeck(before, "alpha", 0).deployed, "initial army deck should be deployed");
    const std::uint64_t reserveOne = findDeck(before, "alpha", 1).id;
    const std::uint64_t reserveTwo = findDeck(before, "alpha", 2).id;
    const auto alphaPlayer = std::find_if(
        before.players.begin(),
        before.players.end(),
        [](const conquest_data::PlayerState& player) { return player.username == "alpha"; });
    require(alphaPlayer != before.players.end(), "alpha player state missing");
    require(alphaPlayer->controlledRegions == 8, "reinforcement control setup failed");
    require(alphaPlayer->reinforcementsAvailable == 2, "two reinforcements should be earned");

    const account_conquest_events::CommandResult atDeadline =
        account_conquest_events::deployReinforcement(
            fixture.database, fixture.eventId, "alpha", reserveOne, 1, 5'000);
    require(!atDeadline.success && atDeadline.message.find("deadline") != std::string::npos,
        "reinforcement at the turn deadline was not rejected");
    const EventState afterDeadline = fixture.state("alpha", 5'001);
    require(afterDeadline.summary.phase == EventPhase::Planning && afterDeadline.summary.turn == 2,
        "deadline reinforcement attempt did not resolve and advance the turn");
    require(!findDeck(afterDeadline, "alpha", 1).deployed,
        "deadline reinforcement attempt deployed its deck");

    require(
        !account_conquest_events::deployReinforcement(
             fixture.database, fixture.eventId, "alpha", reserveOne, 5, 5'010).success,
        "reinforcement entered through a non-edge region");
    require(
        !account_conquest_events::deployReinforcement(
             fixture.database, fixture.eventId, "alpha", reserveOne, 2, 5'010).success,
        "reinforcement entered through an uncontrolled edge");
    requireCommand(
        account_conquest_events::deployReinforcement(
            fixture.database, fixture.eventId, "alpha", reserveOne, 1, 5'010),
        "first reinforcement");
    const account_conquest_events::CommandResult cooldown =
        account_conquest_events::deployReinforcement(
            fixture.database, fixture.eventId, "alpha", reserveTwo, 4, 5'050);
    require(!cooldown.success && cooldown.message.find("cooldown") != std::string::npos,
        "second reinforcement ignored cooldown");
    requireCommand(
        account_conquest_events::deployReinforcement(
            fixture.database, fixture.eventId, "alpha", reserveTwo, 4, 5'110),
        "reinforcement after cooldown");

    const EventState after = fixture.state("alpha", 5'111);
    require(findDeck(after, "alpha", 1).regionId == 1, "first reinforcement entered wrong region");
    require(findDeck(after, "alpha", 2).regionId == 4, "second reinforcement entered wrong region");
    const auto updatedAlpha = std::find_if(
        after.players.begin(),
        after.players.end(),
        [](const conquest_data::PlayerState& player) { return player.username == "alpha"; });
    require(updatedAlpha != after.players.end(), "updated alpha player state missing");
    require(updatedAlpha->reinforcementsAvailable == 0, "reinforcement entitlements were not consumed");
}

void testZeroActiveEventTerminalStates()
{
    {
        Fixture fixture;
        fixture.addAccount("alpha");
        fixture.addAccount("bravo");
        const ConquestDeck alphaActive =
            fixture.saveDeck("alpha", "Alpha Active", fixture.standardDeckTitles());
        const ConquestDeck alphaReserve =
            fixture.saveDeck("alpha", "Alpha Reserve", fixture.standardDeckTitles());
        const ConquestDeck bravo =
            fixture.saveDeck("bravo", "Bravo Active", fixture.standardDeckTitles());
        fixture.saveArmy("alpha", {alphaActive.id, alphaReserve.id});
        fixture.saveArmy("bravo", {bravo.id});
        requireCommand(
            account_conquest_events::joinEvent(
                fixture.database,
                fixture.eventId,
                "alpha",
                {{static_cast<std::uint64_t>(alphaActive.id), 1}},
                1'100),
            "alpha joining terminal fixture");
        requireCommand(
            account_conquest_events::joinEvent(
                fixture.database,
                fixture.eventId,
                "bravo",
                {{static_cast<std::uint64_t>(bravo.id), 18}},
                1'100),
            "bravo joining terminal fixture");
        fixture.startEvent();

        // Both deployed armies are gone. Alpha still has a reserve, but one
        // controlled region earns no reinforcement, so the event is terminal.
        fixture.database.exec(
            "UPDATE conquest_event_decks SET deployed = 0, eliminated = 1, "
            "region_id = 0, destination_region_id = 0 "
            "WHERE deployed = 1");
        requireCommand(
            account_conquest_events::resolveEvent(
                fixture.database, fixture.eventId, 2'010),
            "resolving terminal zero-active event");
        const EventState complete = fixture.state("alpha", 2'011);
        require(complete.summary.phase == EventPhase::Complete,
            "zero-active event without an earned reinforcement did not complete");
        require(!findDeck(complete, "alpha", 1).deployed,
            "terminal event unexpectedly deployed its reserve");
    }

    {
        Fixture fixture;
        fixture.addAccount("alpha");
        fixture.addAccount("bravo");
        const ConquestDeck alphaActive =
            fixture.saveDeck("alpha", "Alpha Active", fixture.standardDeckTitles());
        const ConquestDeck alphaReserve =
            fixture.saveDeck("alpha", "Alpha Reserve", fixture.standardDeckTitles());
        const ConquestDeck bravo =
            fixture.saveDeck("bravo", "Bravo Active", fixture.standardDeckTitles());
        fixture.saveArmy("alpha", {alphaActive.id, alphaReserve.id});
        fixture.saveArmy("bravo", {bravo.id});
        requireCommand(
            account_conquest_events::joinEvent(
                fixture.database,
                fixture.eventId,
                "alpha",
                {{static_cast<std::uint64_t>(alphaActive.id), 1}},
                1'100),
            "alpha joining viable zero-active fixture");
        requireCommand(
            account_conquest_events::joinEvent(
                fixture.database,
                fixture.eventId,
                "bravo",
                {{static_cast<std::uint64_t>(bravo.id), 18}},
                1'100),
            "bravo joining viable zero-active fixture");
        fixture.startEvent();
        const EventState planning = fixture.state("alpha", 2'001);
        const std::int64_t deadline = planning.summary.turnEndsAt;

        fixture.database.exec(
            "UPDATE conquest_event_decks SET deployed = 0, eliminated = 1, "
            "region_id = 0, destination_region_id = 0 "
            "WHERE deployed = 1");
        fixture.database.exec("UPDATE conquest_regions SET controller = ''");
        SQLite::Statement control(
            fixture.database,
            "UPDATE conquest_regions SET controller = 'alpha' WHERE event_id = ? AND region_id = ?");
        for (const int region : {1, 4, 5, 9})
        {
            control.reset();
            control.bind(1, static_cast<std::int64_t>(fixture.eventId));
            control.bind(2, region);
            control.exec();
        }
        SQLite::Statement cooldown(
            fixture.database,
            "UPDATE conquest_event_players SET next_reinforcement_at = ? "
            "WHERE event_id = ? AND username = 'alpha'");
        cooldown.bind(1, deadline + 1'000);
        cooldown.bind(2, static_cast<std::int64_t>(fixture.eventId));
        cooldown.exec();

        requireCommand(
            account_conquest_events::resolveEvent(
                fixture.database, fixture.eventId, deadline),
            "resolving zero-active event with reserves");
        const EventState keptAlive = fixture.state("alpha", deadline + 1);
        require(
            keptAlive.summary.phase == EventPhase::Complete,
            "reserves kept players alive after all of their map decks were defeated");
        const auto alphaPlayer = std::find_if(
            keptAlive.players.begin(),
            keptAlive.players.end(),
            [](const conquest_data::PlayerState& player) { return player.username == "alpha"; });
        require(alphaPlayer != keptAlive.players.end(), "alpha state missing in viable event");
        require(alphaPlayer->eliminated && alphaPlayer->reinforcementsAvailable == 0,
            "defeated player retained Conquest eligibility or reinforcements");
        const std::uint64_t reserveId = findDeck(keptAlive, "alpha", 1).id;
        const account_conquest_events::CommandResult redeploy =
            account_conquest_events::deployReinforcement(
                fixture.database,
                fixture.eventId,
                "alpha",
                reserveId,
                1,
                deadline + 1);
        require(!redeploy.success,
            "defeated player redeployed a reserve after losing the conquest");
    }
}

void testPlayerEliminationAndLastStandingVictory()
{
    Fixture fixture;
    fixture.addAccount("alpha");
    fixture.addAccount("bravo");
    fixture.addAccount("charlie");
    const ConquestDeck alpha =
        fixture.saveDeck("alpha", "Alpha Active", fixture.standardDeckTitles());
    const ConquestDeck bravo =
        fixture.saveDeck("bravo", "Bravo Active", fixture.standardDeckTitles());
    const ConquestDeck bravoReserve =
        fixture.saveDeck("bravo", "Bravo Reserve", fixture.standardDeckTitles());
    const ConquestDeck charlie =
        fixture.saveDeck("charlie", "Charlie Active", fixture.standardDeckTitles());
    fixture.saveArmy("alpha", {alpha.id});
    fixture.saveArmy("bravo", {bravo.id, bravoReserve.id});
    fixture.saveArmy("charlie", {charlie.id});
    requireCommand(
        account_conquest_events::joinEvent(
            fixture.database, fixture.eventId, "alpha",
            {{static_cast<std::uint64_t>(alpha.id), 1}}, 1'100),
        "alpha joining elimination fixture");
    requireCommand(
        account_conquest_events::joinEvent(
            fixture.database, fixture.eventId, "bravo",
            {{static_cast<std::uint64_t>(bravo.id), 2}}, 1'100),
        "bravo joining elimination fixture");
    requireCommand(
        account_conquest_events::joinEvent(
            fixture.database, fixture.eventId, "charlie",
            {{static_cast<std::uint64_t>(charlie.id), 18}}, 1'100),
        "charlie joining elimination fixture");
    fixture.startEvent();

    const EventState firstTurn = fixture.state("bravo", 2'001);
    const std::uint64_t bravoReserveId = findDeck(firstTurn, "bravo", 1).id;
    SQLite::Statement defeatBravo(
        fixture.database,
        "UPDATE conquest_event_decks SET deployed = 0, eliminated = 1, region_id = 0, "
        "destination_region_id = 0 WHERE event_id = ? AND owner = 'bravo' AND deployed = 1");
    defeatBravo.bind(1, static_cast<std::int64_t>(fixture.eventId));
    require(defeatBravo.exec() == 1, "could not defeat bravo's last map deck");
    requireCommand(
        account_conquest_events::resolveEvent(
            fixture.database, fixture.eventId, firstTurn.summary.turnEndsAt),
        "resolving player elimination");

    const EventState afterBravo = fixture.state("bravo", firstTurn.summary.turnEndsAt + 1);
    require(
        afterBravo.summary.phase == EventPhase::Planning && afterBravo.summary.winner.empty(),
        "conquest ended before only one player remained");
    const auto bravoState = std::find_if(
        afterBravo.players.begin(), afterBravo.players.end(),
        [](const conquest_data::PlayerState& player) { return player.username == "bravo"; });
    require(
        bravoState != afterBravo.players.end() && bravoState->eliminated &&
            bravoState->reinforcementsAvailable == 0,
        "player with no map decks remained eligible for conquest play");
    const account_conquest_events::CommandResult defeatedOrders =
        account_conquest_events::submitOrders(
            fixture.database, fixture.eventId, "bravo", {}, firstTurn.summary.turnEndsAt + 2);
    require(
        !defeatedOrders.success && defeatedOrders.message.find("defeated") != std::string::npos,
        "eliminated player submitted Conquest orders");
    const account_conquest_events::CommandResult defeatedReinforcement =
        account_conquest_events::deployReinforcement(
            fixture.database,
            fixture.eventId,
            "bravo",
            bravoReserveId,
            2,
            firstTurn.summary.turnEndsAt + 2);
    require(
        !defeatedReinforcement.success &&
            defeatedReinforcement.message.find("defeated") != std::string::npos,
        "eliminated player deployed a reserve");

    SQLite::Statement defeatCharlie(
        fixture.database,
        "UPDATE conquest_event_decks SET deployed = 0, eliminated = 1, region_id = 0, "
        "destination_region_id = 0 WHERE event_id = ? AND owner = 'charlie' AND deployed = 1");
    defeatCharlie.bind(1, static_cast<std::int64_t>(fixture.eventId));
    require(defeatCharlie.exec() == 1, "could not defeat charlie's last map deck");
    requireCommand(
        account_conquest_events::resolveEvent(
            fixture.database, fixture.eventId, afterBravo.summary.turnEndsAt),
        "resolving last-standing victory");
    const EventState complete = fixture.state("alpha", afterBravo.summary.turnEndsAt + 1);
    require(
        complete.summary.phase == EventPhase::Complete && complete.summary.winner == "alpha",
        "last standing player did not win the conquest");
    require(
        fixture.coins("alpha") ==
            100 - conquest_data::ConquestEntryFeeCoins +
                conquest_data::ConquestWinnerRewardCoins,
        "Conquest winner did not receive exactly 100 coins");
    requireCommand(
        account_conquest_events::resolveEvent(
            fixture.database, fixture.eventId, afterBravo.summary.turnEndsAt + 2),
        "re-resolving completed Conquest");
    require(
        fixture.coins("alpha") ==
            100 - conquest_data::ConquestEntryFeeCoins +
                conquest_data::ConquestWinnerRewardCoins,
        "completed Conquest paid its winner more than once");
}

void testRecurringEventLifecycleAndHistory()
{
    constexpr std::int64_t ExtensionAt = 1'100;
    constexpr std::int64_t FirstCompletionAt = 1'200;
    constexpr std::int64_t BackfillAt = 2'000;
    constexpr std::int64_t SecondCompletionAt = 2'100;

    {
        Fixture fixture;

        SQLite::Statement expireRegistration(
            fixture.database,
            "UPDATE conquest_events SET registration_ends_at = ? WHERE id = ?");
        expireRegistration.bind(1, ExtensionAt);
        expireRegistration.bind(2, static_cast<std::int64_t>(fixture.eventId));
        require(expireRegistration.exec() == 1, "could not expire recurring fixture registration");
        requireCommand(
            account_conquest_events::resolveEvent(
                fixture.database, fixture.eventId, ExtensionAt),
            "extending an empty recurring registration");

        SQLite::Statement extended(
            fixture.database,
            "SELECT phase, registration_ends_at FROM conquest_events WHERE id = ?");
        extended.bind(1, static_cast<std::int64_t>(fixture.eventId));
        require(extended.executeStep(), "extended recurring event was not found");
        require(
            extended.getColumn(0).getInt() == static_cast<int>(EventPhase::Registration) &&
                extended.getColumn(1).getInt64() == ExtensionAt + 24 * 60 * 60,
            "empty registration did not extend deterministically");
        require(
            conquestEventCount(fixture.database) == 1,
            "registration extension spawned a successor event");

        std::vector<card_data::Card> currentCatalog = fixture.catalog;
        const auto currentBraun = std::find_if(
            currentCatalog.begin(),
            currentCatalog.end(),
            [](const card_data::Card& card) { return card.title == "Braun Stonefist"; });
        require(currentBraun != currentCatalog.end(), "Braun is missing from lifecycle catalog");
        currentBraun->imagePath = "current/successor-braun.png";
        const std::vector<std::uint8_t> expectedBraunBlob = serializedCard(*currentBraun);
        account_catalog::setCardLibrary(currentCatalog);

        SQLite::Statement makeTerminal(
            fixture.database,
            "UPDATE conquest_events SET phase = ?, turn = 1, turn_ends_at = 0 WHERE id = ?");
        makeTerminal.bind(1, static_cast<int>(EventPhase::Resolving));
        makeTerminal.bind(2, static_cast<std::int64_t>(fixture.eventId));
        require(makeTerminal.exec() == 1, "could not make root Conquest event terminal");
        requireCommand(
            account_conquest_events::resolveEvent(
                fixture.database, fixture.eventId, FirstCompletionAt),
            "completing root recurring event");

        const std::string successorSeed =
            "default-dark-realms-after-" + std::to_string(fixture.eventId);
        const std::int64_t successorId = eventIdForSeed(fixture.database, successorSeed);
        require(
            conquestEventCount(fixture.database) == 2,
            "root completion did not create exactly one successor");

        SQLite::Statement successor(
            fixture.database,
            "SELECT phase, turn, registration_ends_at FROM conquest_events WHERE id = ?");
        successor.bind(1, successorId);
        require(successor.executeStep(), "root successor event was not found");
        require(
            successor.getColumn(0).getInt() == static_cast<int>(EventPhase::Registration) &&
                successor.getColumn(1).getInt() == 0 &&
                successor.getColumn(2).getInt64() == FirstCompletionAt + 24 * 60 * 60,
            "root successor did not start a fresh deterministic registration");
        require(
            eventResourceCount(fixture.database, "conquest_regions", successorId) ==
                static_cast<int>(conquest_map::DarkRealmsRegions.size()),
            "root successor did not receive every Dark Realms region");
        require(
            eventResourceCount(
                fixture.database, "conquest_event_catalog_cards", successorId) ==
                static_cast<int>(currentCatalog.size()),
            "root successor did not receive a complete frozen catalog");
        require(
            frozenCatalogBlob(fixture.database, successorId, "Braun Stonefist") ==
                expectedBraunBlob,
            "successor froze the root catalog instead of the current in-memory catalog");
    }

    {
        Fixture fixture;

        // Simulate upgrading a legacy database that completed its only seeded
        // event before recurring successors existed.
        SQLite::Statement completeLegacyRoot(
            fixture.database,
            "UPDATE conquest_events SET phase = ?, turn_ends_at = 0 WHERE id = ?");
        completeLegacyRoot.bind(1, static_cast<int>(EventPhase::Complete));
        completeLegacyRoot.bind(2, static_cast<std::int64_t>(fixture.eventId));
        require(completeLegacyRoot.exec() == 1, "could not prepare legacy completed root");
        require(conquestEventCount(fixture.database) == 1, "legacy root fixture already has a successor");

        account_conquest_events::initializeSchema(fixture.database, BackfillAt);
        account_conquest_events::initializeSchema(fixture.database, BackfillAt + 1);
        const std::string firstSuccessorSeed =
            "default-dark-realms-after-" + std::to_string(fixture.eventId);
        const std::int64_t firstSuccessorId =
            eventIdForSeed(fixture.database, firstSuccessorSeed);
        require(
            conquestEventCount(fixture.database) == 2,
            "repeated schema initialization duplicated the backfilled successor");
        require(
            eventResourceCount(fixture.database, "conquest_regions", firstSuccessorId) ==
                static_cast<int>(conquest_map::DarkRealmsRegions.size()),
            "backfilled successor is missing Dark Realms regions");
        require(
            eventResourceCount(
                fixture.database, "conquest_event_catalog_cards", firstSuccessorId) ==
                static_cast<int>(fixture.catalog.size()),
            "backfilled successor is missing its frozen catalog");

        SQLite::Statement completeFirstSuccessor(
            fixture.database,
            "UPDATE conquest_events SET phase = ?, turn = 1, turn_ends_at = 0 WHERE id = ?");
        completeFirstSuccessor.bind(1, static_cast<int>(EventPhase::Resolving));
        completeFirstSuccessor.bind(2, firstSuccessorId);
        require(completeFirstSuccessor.exec() == 1, "could not make first successor terminal");
        requireCommand(
            account_conquest_events::resolveEvent(
                fixture.database,
                static_cast<std::uint64_t>(firstSuccessorId),
                SecondCompletionAt),
            "completing first recurring successor");

        const std::string liveSuccessorSeed =
            "default-dark-realms-after-" + std::to_string(firstSuccessorId);
        const std::int64_t liveSuccessorId =
            eventIdForSeed(fixture.database, liveSuccessorSeed);
        account_conquest_events::initializeSchema(fixture.database, SecondCompletionAt + 1);
        account_conquest_events::initializeSchema(fixture.database, SecondCompletionAt + 2);
        require(
            conquestEventCount(fixture.database) == 3,
            "successor completion or repeated initialization created the wrong number of cycles");

        SQLite::Statement live(
            fixture.database,
            "SELECT phase FROM conquest_events WHERE id = ?");
        live.bind(1, liveSuccessorId);
        require(
            live.executeStep() &&
                live.getColumn(0).getInt() == static_cast<int>(EventPhase::Registration),
            "next recurring cycle is not accepting registrations");

        SQLite::Statement insertHistory(
            fixture.database,
            "INSERT INTO conquest_events "
            "(seed_key, name, map_id, phase, turn, registration_ends_at, turn_ends_at, "
            " registration_seconds, turn_seconds, reinforcement_cooldown_seconds, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, 0, 0, 0, 86400, 86400, 86400, ?, ?)");
        for (int index = 0; index < 70; ++index)
        {
            insertHistory.reset();
            insertHistory.bind(1, "completed-history-" + std::to_string(index));
            insertHistory.bind(2, "Completed History " + std::to_string(index));
            insertHistory.bind(3, std::string(conquest_map::DarkRealmsId));
            insertHistory.bind(4, static_cast<int>(EventPhase::Complete));
            insertHistory.bind(5, 3'000 + index);
            insertHistory.bind(6, 3'000 + index);
            insertHistory.exec();
        }
        require(
            conquestEventCount(fixture.database) >
                static_cast<int>(conquest_data::MaxConquestEvents),
            "history fixture did not exceed the serialized event cap");

        const std::vector<conquest_data::EventSummary> listed =
            account_conquest_events::listEvents(
                fixture.database,
                "observer",
                SecondCompletionAt + 3);
        require(
            listed.size() == conquest_data::MaxConquestEvents,
            "event history list was not capped at the wire limit");
        const auto listedLive = std::find_if(
            listed.begin(),
            listed.end(),
            [liveSuccessorId](const conquest_data::EventSummary& event) {
                return event.id == static_cast<std::uint64_t>(liveSuccessorId);
            });
        require(listedLive != listed.end(), "capped event history omitted the live cycle");
        require(
            listedLive->phase == EventPhase::Registration,
            "listing historical events advanced or completed the live cycle");
    }
}

void testConquestActionLimitAdjudication()
{
    const std::vector<card_data::Card> catalog = syntheticCatalog();

    GameEngine belowCap(11, catalog);
    require(
        !game_action::adjudicateConquestActionLimit(
            belowCap,
            conquest_data::MaxConquestBattleActions - 1,
            20),
        "below-cap Conquest battle was adjudicated");
    require(
        belowCap.phase() == game_data::Phase::HeroPlacement && belowCap.winner() == 0,
        "below-cap adjudication mutated the tactical game");

    const auto hero = std::find_if(
        catalog.begin(),
        catalog.end(),
        [](const card_data::Card& card) { return card.type == "Hero"; });
    require(hero != catalog.end(), "action-limit fixture has no hero");
    GameEngine strongerBoard(12, catalog);
    strongerBoard.submitDeck(1, {*hero});
    strongerBoard.submitDeck(2, {});
    const auto playerOneHome = game_data::homeSquares(1)[0];
    require(
        strongerBoard.placeHero(1, 0, playerOneHome.first, playerOneHome.second),
        "could not prepare stronger action-limit board");
    const game_data::Snapshot strongerBefore = strongerBoard.snapshotFor(1);
    require(
        strongerBefore.players[0].heroesAlive > strongerBefore.players[1].heroesAlive,
        "action-limit score fixture is not stronger for player one");
    require(
        game_action::adjudicateConquestActionLimit(
            strongerBoard,
            conquest_data::MaxConquestBattleActions,
            21),
        "at-cap stronger board was not adjudicated");
    require(
        strongerBoard.phase() == game_data::Phase::GameOver && strongerBoard.winner() == 1,
        "stronger board score did not win action-limit adjudication");

    const auto tiedWinner = [&](unsigned int engineSeed, std::uint32_t battleSeed) {
        GameEngine tied(engineSeed, catalog);
        require(
            game_action::adjudicateConquestActionLimit(
                tied,
                conquest_data::MaxConquestBattleActions,
                battleSeed),
            "at-cap tied board was not adjudicated");
        require(
            tied.phase() == game_data::Phase::GameOver,
            "at-cap tie did not end the tactical game");
        return tied.winner();
    };

    const int evenWinnerOne = tiedWinner(31, 100);
    const int evenWinnerTwo = tiedWinner(32, 100);
    const int oddWinnerOne = tiedWinner(33, 101);
    const int oddWinnerTwo = tiedWinner(34, 101);
    require(
        evenWinnerOne == 2 && evenWinnerTwo == evenWinnerOne,
        "even battle seed did not reproducibly award an exact tie to player two");
    require(
        oddWinnerOne == 1 && oddWinnerTwo == oddWinnerOne,
        "odd battle seed did not reproducibly award an exact tie to player one");
}

} // namespace

int main()
{
    if (sodium_init() < 0)
    {
        std::cerr << "libsodium initialization failed\n";
        return 1;
    }

    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"Dark Realms metadata", testMapMetadata},
        {"Conquest allocation", testConquestAllocationAndRegularDeckIsolation},
        {"Army rules", testArmyRules},
        {"Registration and event locks", testRegistrationAndActiveDeckLock},
        {"Conquest entry fee", testConquestEntryFee},
        {"Admin forced start", testAdminForcedStart},
        {"Secret vacate/re-enter orders", testSecretVacateAndReenterOrders},
        {"Region and crossing battles", testCollisionAndCrossingBattles},
        {"Frozen battle replay", testFrozenBattleDataActionsAndResult},
        {"Reinforcement rules", testReinforcementRules},
        {"Zero-active event viability", testZeroActiveEventTerminalStates},
        {"Player elimination and last standing", testPlayerEliminationAndLastStandingVictory},
        {"Recurring event lifecycle", testRecurringEventLifecycleAndHistory},
        {"Conquest action-limit adjudication", testConquestActionLimitAdjudication},
    };

    int failed = 0;
    for (const auto& [name, test] : tests)
    {
        try
        {
            test();
            std::cout << "[PASS] " << name << '\n';
        }
        catch (const std::exception& exception)
        {
            ++failed;
            std::cerr << "[FAIL] " << name << ": " << exception.what() << '\n';
        }
    }

    if (failed != 0)
    {
        std::cerr << failed << " Conquest test group(s) failed\n";
        return 1;
    }
    std::cout << "All Conquest test groups passed\n";
    return 0;
}
