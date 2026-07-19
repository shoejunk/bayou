#include "account_conquest_events.hpp"

#include "account_catalog.hpp"

#include "../shared/conquest_data.hpp"
#include "../shared/game_data.hpp"
#include "../shared/conquest_map.hpp"
#include "../shared/network.hpp"

#include <sodium.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
using account_conquest_events::CommandResult;
using conquest_data::BattleKind;
using conquest_data::BattleStatus;
using conquest_data::EventPhase;

constexpr std::int64_t DefaultRegistrationSeconds = 24 * 60 * 60;
constexpr std::int64_t DefaultTurnSeconds = 24 * 60 * 60;
constexpr std::int64_t DefaultReinforcementCooldownSeconds = 24 * 60 * 60;
constexpr const char* DefaultEventSeedKey = "default-dark-realms";
constexpr const char* DefaultEventName = "Dark Realms Conquest";

std::int64_t effectiveNow(std::int64_t now)
{
    if (now != 0)
    {
        return now;
    }

    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

bool tableHasColumn(
    SQLite::Database& database,
    const std::string& table,
    const std::string& column)
{
    SQLite::Statement columns(database, "PRAGMA table_info(" + table + ")");
    while (columns.executeStep())
    {
        if (columns.getColumn(1).getString() == column)
        {
            return true;
        }
    }
    return false;
}

bool validDatabaseId(std::uint64_t id)
{
    return id != 0 && id <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
}

std::int64_t databaseId(std::uint64_t id)
{
    return static_cast<std::int64_t>(id);
}

int phaseValue(EventPhase phase)
{
    return static_cast<int>(phase);
}

int battleKindValue(BattleKind kind)
{
    return static_cast<int>(kind);
}

int battleStatusValue(BattleStatus status)
{
    return static_cast<int>(status);
}

bool supportedBattleActionType(std::uint8_t actionType)
{
    switch (static_cast<network::MessageType>(actionType))
    {
        case network::MessageType::PlaceHero:
        case network::MessageType::PlayCard:
        case network::MessageType::MovePiece:
        case network::MessageType::AttackPiece:
        case network::MessageType::UseAbility:
        case network::MessageType::DiscardCard:
        case network::MessageType::EndTurn:
            return true;
        default:
            return false;
    }
}

std::string generateBattleCapability()
{
    static_assert(conquest_data::ConquestBattleCapabilityHexLength % 2 == 0);
    constexpr std::size_t ByteCount =
        conquest_data::ConquestBattleCapabilityHexLength / 2;
    std::array<unsigned char, ByteCount> random{};
    std::array<char, conquest_data::ConquestBattleCapabilityHexLength + 1> encoded{};
    randombytes_buf(random.data(), random.size());
    sodium_bin2hex(encoded.data(), encoded.size(), random.data(), random.size());
    return std::string(encoded.data(), conquest_data::ConquestBattleCapabilityHexLength);
}

std::string loadOrCreateBattleCapability(
    SQLite::Database& database,
    std::int64_t battleId)
{
    SQLite::Transaction transaction(database, SQLite::TransactionBehavior::IMMEDIATE);
    SQLite::Statement existing(
        database,
        "SELECT capability FROM conquest_battle_capabilities WHERE battle_id = ?");
    existing.bind(1, battleId);
    if (existing.executeStep())
    {
        const std::string capability = existing.getColumn(0).getString();
        if (capability.size() != conquest_data::ConquestBattleCapabilityHexLength)
        {
            throw std::runtime_error("Stored Conquest battle capability is invalid");
        }
        transaction.commit();
        return capability;
    }

    // A collision is cryptographically negligible, but the UNIQUE constraint
    // and bounded retry keep this correct even under a faulty random source.
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        const std::string capability = generateBattleCapability();
        SQLite::Statement insert(
            database,
            "INSERT OR IGNORE INTO conquest_battle_capabilities "
            "(battle_id, capability, created_at) VALUES (?, ?, ?)");
        insert.bind(1, battleId);
        insert.bind(2, capability);
        insert.bind(3, effectiveNow(0));
        if (insert.exec() == 1)
        {
            transaction.commit();
            return capability;
        }

        // A concurrent creator for the same battle wins. This is mostly
        // defensive because IMMEDIATE serializes writers on this database.
        SQLite::Statement concurrentlyCreated(
            database,
            "SELECT capability FROM conquest_battle_capabilities WHERE battle_id = ?");
        concurrentlyCreated.bind(1, battleId);
        if (concurrentlyCreated.executeStep())
        {
            const std::string stored = concurrentlyCreated.getColumn(0).getString();
            if (stored.size() == conquest_data::ConquestBattleCapabilityHexLength)
            {
                transaction.commit();
                return stored;
            }
            throw std::runtime_error("Stored Conquest battle capability is invalid");
        }
    }
    throw std::runtime_error("Could not generate a unique Conquest battle capability");
}

struct BattleCapabilityIdentity
{
    std::string playerOne;
    std::string playerTwo;
};

std::optional<BattleCapabilityIdentity> authenticateBattleCapability(
    SQLite::Database& database,
    std::uint64_t battleId,
    const std::string& capability,
    std::string& error)
{
    if (!validDatabaseId(battleId) ||
        capability.size() != conquest_data::ConquestBattleCapabilityHexLength)
    {
        error = "Invalid Conquest battle capability";
        return std::nullopt;
    }

    SQLite::Statement stored(
        database,
        "SELECT c.capability, b.player_one, b.player_two "
        "FROM conquest_battle_capabilities c "
        "JOIN conquest_battles b ON b.id = c.battle_id "
        "WHERE c.battle_id = ?");
    stored.bind(1, databaseId(battleId));
    if (!stored.executeStep())
    {
        error = "Invalid Conquest battle capability";
        return std::nullopt;
    }

    const std::string expected = stored.getColumn(0).getString();
    if (expected.size() != conquest_data::ConquestBattleCapabilityHexLength ||
        sodium_memcmp(expected.data(), capability.data(), expected.size()) != 0)
    {
        error = "Invalid Conquest battle capability";
        return std::nullopt;
    }

    error.clear();
    return BattleCapabilityIdentity{
        stored.getColumn(1).getString(),
        stored.getColumn(2).getString()};
}

struct EventRow
{
    std::int64_t id = 0;
    std::string name;
    std::string mapId;
    EventPhase phase = EventPhase::Registration;
    int turn = 0;
    std::int64_t registrationEndsAt = 0;
    std::int64_t turnEndsAt = 0;
    std::int64_t registrationSeconds = DefaultRegistrationSeconds;
    std::int64_t turnSeconds = DefaultTurnSeconds;
    std::int64_t reinforcementCooldownSeconds = DefaultReinforcementCooldownSeconds;
    std::string winner;
};

struct StoredDeck
{
    std::int64_t id = 0;
    std::int64_t sourceDeckId = 0;
    std::string owner;
    std::string name;
    int armySlot = 0;
    bool deployed = false;
    bool eliminated = false;
    int regionId = 0;
    int destinationRegionId = 0;
    bool moveResolved = false;
};

struct FrozenCard
{
    std::string title;
    std::vector<std::uint8_t> blob;
};

std::vector<std::uint8_t> serializeCard(const card_data::Card& card)
{
    sf::Packet packet;
    card_data::writeCardListHeader(packet, 1);
    card_data::writeCard(packet, card);
    const auto* first = static_cast<const std::uint8_t*>(packet.getData());
    return first == nullptr
        ? std::vector<std::uint8_t>{}
        : std::vector<std::uint8_t>(first, first + packet.getDataSize());
}

std::optional<card_data::Card> deserializeCard(const void* data, int size)
{
    if (data == nullptr || size <= 0)
    {
        return std::nullopt;
    }
    sf::Packet packet;
    packet.append(data, static_cast<std::size_t>(size));
    std::uint32_t count = 0;
    bool legacyFormat = false;
    bool actionIncludesNextState = false;
    if (!card_data::readCardListHeader(
            packet,
            count,
            legacyFormat,
            &actionIncludesNextState) || count != 1)
    {
        return std::nullopt;
    }
    card_data::Card card;
    if (!card_data::readListedCard(
            packet,
            card,
            legacyFormat,
            actionIncludesNextState))
    {
        return std::nullopt;
    }
    return card;
}

bool isDefaultEventLineage(std::string_view seedKey)
{
    if (seedKey == DefaultEventSeedKey)
    {
        return true;
    }

    const std::string prefix = std::string(DefaultEventSeedKey) + "-after-";
    if (!seedKey.starts_with(prefix) || seedKey.size() == prefix.size())
    {
        return false;
    }
    return std::all_of(
        seedKey.begin() + static_cast<std::ptrdiff_t>(prefix.size()),
        seedKey.end(),
        [](char value) {
            return value >= '0' && value <= '9';
        });
}

void populateDarkRealmsEvent(SQLite::Database& database, std::int64_t eventId)
{
    SQLite::Statement insertRegion(
        database,
        "INSERT OR IGNORE INTO conquest_regions (event_id, region_id) VALUES (?, ?)");
    for (const conquest_map::RegionDefinition& region : conquest_map::DarkRealmsRegions)
    {
        insertRegion.reset();
        insertRegion.bind(1, eventId);
        insertRegion.bind(2, region.id);
        insertRegion.exec();
    }

    SQLite::Statement catalogCount(
        database,
        "SELECT COUNT(*) FROM conquest_event_catalog_cards WHERE event_id = ?");
    catalogCount.bind(1, eventId);
    catalogCount.executeStep();
    if (catalogCount.getColumn(0).getInt() != 0)
    {
        return;
    }

    const std::vector<card_data::Card>& catalog = account_catalog::cardLibrary();
    if (catalog.empty())
    {
        throw SQLite::Exception("Cannot create a Conquest event with an empty card catalog");
    }
    if (catalog.size() > conquest_data::MaxConquestCatalogCards)
    {
        throw SQLite::Exception("Card catalog exceeds the Conquest snapshot limit");
    }

    SQLite::Statement insertCatalog(
        database,
        "INSERT INTO conquest_event_catalog_cards "
        "(event_id, card_index, card_title, card_blob) VALUES (?, ?, ?, ?)");
    for (std::size_t cardIndex = 0; cardIndex < catalog.size(); ++cardIndex)
    {
        const std::vector<std::uint8_t> blob = serializeCard(catalog[cardIndex]);
        if (catalog[cardIndex].title.empty() || blob.empty())
        {
            throw SQLite::Exception("Could not freeze the Conquest card catalog");
        }
        insertCatalog.reset();
        insertCatalog.bind(1, eventId);
        insertCatalog.bind(2, static_cast<int>(cardIndex));
        insertCatalog.bind(3, catalog[cardIndex].title);
        insertCatalog.bind(4, blob.data(), static_cast<int>(blob.size()));
        insertCatalog.exec();
    }
}

void ensureDefaultSuccessorInTransaction(
    SQLite::Database& database,
    std::int64_t completedEventId,
    std::int64_t now)
{
    std::string seedKey;
    std::string mapId;
    int phase = phaseValue(EventPhase::Registration);
    std::int64_t registrationSeconds = 0;
    std::int64_t turnSeconds = 0;
    std::int64_t reinforcementCooldownSeconds = 0;
    {
        SQLite::Statement completed(
            database,
            "SELECT seed_key, map_id, phase, registration_seconds, turn_seconds, "
            "reinforcement_cooldown_seconds FROM conquest_events WHERE id = ?");
        completed.bind(1, completedEventId);
        if (!completed.executeStep())
        {
            return;
        }
        seedKey = completed.getColumn(0).getString();
        mapId = completed.getColumn(1).getString();
        phase = completed.getColumn(2).getInt();
        registrationSeconds = completed.getColumn(3).getInt64();
        turnSeconds = completed.getColumn(4).getInt64();
        reinforcementCooldownSeconds = completed.getColumn(5).getInt64();
    }

    if (!isDefaultEventLineage(seedKey) ||
        mapId != conquest_map::DarkRealmsId ||
        phase != phaseValue(EventPhase::Complete))
    {
        return;
    }

    const std::string successorKey =
        std::string(DefaultEventSeedKey) + "-after-" + std::to_string(completedEventId);
    const std::string successorName =
        std::string(DefaultEventName) + " after Event " + std::to_string(completedEventId);
    SQLite::Statement insert(
        database,
        "INSERT INTO conquest_events "
        "(seed_key, name, map_id, phase, turn, registration_ends_at, turn_ends_at, "
        " registration_seconds, turn_seconds, reinforcement_cooldown_seconds, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, 0, ?, 0, ?, ?, ?, ?, ?) "
        "ON CONFLICT(seed_key) DO NOTHING");
    insert.bind(1, successorKey);
    insert.bind(2, successorName);
    insert.bind(3, std::string(conquest_map::DarkRealmsId));
    insert.bind(4, phaseValue(EventPhase::Registration));
    insert.bind(5, now + registrationSeconds);
    insert.bind(6, registrationSeconds);
    insert.bind(7, turnSeconds);
    insert.bind(8, reinforcementCooldownSeconds);
    insert.bind(9, now);
    insert.bind(10, now);
    insert.exec();

    SQLite::Statement successor(
        database,
        "SELECT id FROM conquest_events WHERE seed_key = ?");
    successor.bind(1, successorKey);
    if (!successor.executeStep())
    {
        throw SQLite::Exception("Could not create the next Conquest event");
    }
    populateDarkRealmsEvent(database, successor.getColumn(0).getInt64());
}

std::optional<EventRow> loadEventRow(SQLite::Database& database, std::int64_t eventId)
{
    SQLite::Statement query(
        database,
        "SELECT id, name, map_id, phase, turn, registration_ends_at, turn_ends_at, "
        "registration_seconds, turn_seconds, reinforcement_cooldown_seconds, winner "
        "FROM conquest_events WHERE id = ?");
    query.bind(1, eventId);
    if (!query.executeStep())
    {
        return std::nullopt;
    }

    const int phase = query.getColumn(3).getInt();
    if (phase < phaseValue(EventPhase::Registration) || phase > phaseValue(EventPhase::Complete))
    {
        return std::nullopt;
    }

    EventRow event;
    event.id = query.getColumn(0).getInt64();
    event.name = query.getColumn(1).getString();
    event.mapId = query.getColumn(2).getString();
    event.phase = static_cast<EventPhase>(phase);
    event.turn = query.getColumn(4).getInt();
    event.registrationEndsAt = query.getColumn(5).getInt64();
    event.turnEndsAt = query.getColumn(6).getInt64();
    event.registrationSeconds = query.getColumn(7).getInt64();
    event.turnSeconds = query.getColumn(8).getInt64();
    event.reinforcementCooldownSeconds = query.getColumn(9).getInt64();
    event.winner = query.getColumn(10).getString();
    return event;
}

std::vector<StoredDeck> loadActiveDecks(SQLite::Database& database, std::int64_t eventId)
{
    std::vector<StoredDeck> decks;
    SQLite::Statement query(
        database,
        "SELECT id, source_deck_id, owner, deck_name, army_slot, deployed, eliminated, "
        "region_id, destination_region_id, move_resolved "
        "FROM conquest_event_decks "
        "WHERE event_id = ? AND deployed = 1 AND eliminated = 0 ORDER BY id");
    query.bind(1, eventId);
    while (query.executeStep())
    {
        StoredDeck deck;
        deck.id = query.getColumn(0).getInt64();
        deck.sourceDeckId = query.getColumn(1).getInt64();
        deck.owner = query.getColumn(2).getString();
        deck.name = query.getColumn(3).getString();
        deck.armySlot = query.getColumn(4).getInt();
        deck.deployed = query.getColumn(5).getInt() != 0;
        deck.eliminated = query.getColumn(6).getInt() != 0;
        deck.regionId = query.getColumn(7).getInt();
        deck.destinationRegionId = query.getColumn(8).getInt();
        deck.moveResolved = query.getColumn(9).getInt() != 0;
        decks.push_back(std::move(deck));
    }
    return decks;
}

std::uint32_t newBattleSeed()
{
    std::uint32_t seed = 0;
    while (seed == 0)
    {
        seed = randombytes_random();
    }
    return seed;
}

bool participantExists(SQLite::Database& database, std::int64_t eventId, const std::string& username)
{
    SQLite::Statement query(
        database,
        "SELECT 1 FROM conquest_event_players WHERE event_id = ? AND username = ? LIMIT 1");
    query.bind(1, eventId);
    query.bind(2, username);
    return query.executeStep();
}

bool playerEliminated(SQLite::Database& database, std::int64_t eventId, const std::string& username)
{
    SQLite::Statement query(
        database,
        "SELECT eliminated FROM conquest_event_players WHERE event_id = ? AND username = ?");
    query.bind(1, eventId);
    query.bind(2, username);
    return query.executeStep() && query.getColumn(0).getInt() != 0;
}

std::vector<std::string> standingPlayers(SQLite::Database& database, std::int64_t eventId)
{
    std::vector<std::string> players;
    SQLite::Statement query(
        database,
        "SELECT username FROM conquest_event_players "
        "WHERE event_id = ? AND eliminated = 0 ORDER BY color_index");
    query.bind(1, eventId);
    while (query.executeStep())
    {
        players.push_back(query.getColumn(0).getString());
    }
    return players;
}

void eliminatePlayerIfDefeated(
    SQLite::Database& database,
    std::int64_t eventId,
    const std::string& username)
{
    SQLite::Statement eliminate(
        database,
        "UPDATE conquest_event_players SET eliminated = 1, orders_submitted = 1 "
        "WHERE event_id = ? AND username = ? AND NOT EXISTS ("
        "SELECT 1 FROM conquest_event_decks active "
        "WHERE active.event_id = conquest_event_players.event_id "
        "AND active.owner = conquest_event_players.username "
        "AND active.deployed = 1 AND active.eliminated = 0)");
    eliminate.bind(1, eventId);
    eliminate.bind(2, username);
    eliminate.exec();
}

void eliminateDefeatedPlayers(SQLite::Database& database, std::int64_t eventId)
{
    SQLite::Statement eliminate(
        database,
        "UPDATE conquest_event_players SET eliminated = 1, orders_submitted = 1 "
        "WHERE event_id = ? AND eliminated = 0 AND NOT EXISTS ("
        "SELECT 1 FROM conquest_event_decks active "
        "WHERE active.event_id = conquest_event_players.event_id "
        "AND active.owner = conquest_event_players.username "
        "AND active.deployed = 1 AND active.eliminated = 0)");
    eliminate.bind(1, eventId);
    eliminate.exec();
}

int participantCount(SQLite::Database& database, std::int64_t eventId)
{
    SQLite::Statement query(
        database,
        "SELECT COUNT(*) FROM conquest_event_players WHERE event_id = ?");
    query.bind(1, eventId);
    query.executeStep();
    return query.getColumn(0).getInt();
}

bool planningReady(SQLite::Database& database, const EventRow& event, std::int64_t now)
{
    if (event.turnEndsAt > 0 && now >= event.turnEndsAt)
    {
        return true;
    }

    if (standingPlayers(database, event.id).empty())
    {
        return false;
    }

    SQLite::Statement missing(
        database,
        "SELECT COUNT(*) FROM conquest_event_players p "
        "WHERE p.event_id = ? AND p.eliminated = 0 AND p.orders_submitted = 0 AND "
        "  EXISTS (SELECT 1 FROM conquest_event_decks active "
        "          WHERE active.event_id = p.event_id AND active.owner = p.username "
        "            AND active.deployed = 1 AND active.eliminated = 0)");
    missing.bind(1, event.id);
    missing.executeStep();
    return missing.getColumn(0).getInt() == 0;
}

void cancelInvalidQueuedBattles(SQLite::Database& database, std::int64_t eventId, int turn)
{
    SQLite::Statement invalid(
        database,
        "SELECT b.id FROM conquest_battles b "
        "JOIN conquest_event_decks d1 ON d1.id = b.deck_one_id "
        "JOIN conquest_event_decks d2 ON d2.id = b.deck_two_id "
        "WHERE b.event_id = ? AND b.turn = ? AND b.status IN (?, ?) "
        "AND (d1.eliminated = 1 OR d2.eliminated = 1 OR d1.deployed = 0 OR d2.deployed = 0 "
        "     OR d1.owner = d2.owner)");
    invalid.bind(1, eventId);
    invalid.bind(2, turn);
    invalid.bind(3, battleStatusValue(BattleStatus::Queued));
    invalid.bind(4, battleStatusValue(BattleStatus::Ready));

    std::vector<std::int64_t> ids;
    while (invalid.executeStep())
    {
        ids.push_back(invalid.getColumn(0).getInt64());
    }

    SQLite::Statement cancel(
        database,
        "UPDATE conquest_battles SET status = ? WHERE id = ?");
    for (const std::int64_t id : ids)
    {
        cancel.reset();
        cancel.bind(1, battleStatusValue(BattleStatus::Cancelled));
        cancel.bind(2, id);
        cancel.exec();
    }
}

void activateQueuedBattles(SQLite::Database& database, std::int64_t eventId, int turn)
{
    cancelInvalidQueuedBattles(database, eventId, turn);

    std::unordered_set<std::int64_t> busyDecks;
    SQLite::Statement ready(
        database,
        "SELECT deck_one_id, deck_two_id FROM conquest_battles "
        "WHERE event_id = ? AND turn = ? AND status = ?");
    ready.bind(1, eventId);
    ready.bind(2, turn);
    ready.bind(3, battleStatusValue(BattleStatus::Ready));
    while (ready.executeStep())
    {
        busyDecks.insert(ready.getColumn(0).getInt64());
        busyDecks.insert(ready.getColumn(1).getInt64());
    }

    SQLite::Statement queued(
        database,
        "SELECT id, deck_one_id, deck_two_id FROM conquest_battles "
        "WHERE event_id = ? AND turn = ? AND status = ? ORDER BY id");
    queued.bind(1, eventId);
    queued.bind(2, turn);
    queued.bind(3, battleStatusValue(BattleStatus::Queued));

    struct QueuedBattle
    {
        std::int64_t id = 0;
        std::int64_t first = 0;
        std::int64_t second = 0;
    };
    std::vector<QueuedBattle> candidates;
    while (queued.executeStep())
    {
        candidates.push_back({
            queued.getColumn(0).getInt64(),
            queued.getColumn(1).getInt64(),
            queued.getColumn(2).getInt64()});
    }

    SQLite::Statement activate(
        database,
        "UPDATE conquest_battles SET status = ? WHERE id = ? AND status = ?");
    for (const QueuedBattle& battle : candidates)
    {
        if (busyDecks.contains(battle.first) || busyDecks.contains(battle.second))
        {
            continue;
        }
        activate.reset();
        activate.bind(1, battleStatusValue(BattleStatus::Ready));
        activate.bind(2, battle.id);
        activate.bind(3, battleStatusValue(BattleStatus::Queued));
        activate.exec();
        busyDecks.insert(battle.first);
        busyDecks.insert(battle.second);
    }
}

void settleDecksWithoutPendingBattles(SQLite::Database& database, std::int64_t eventId, int turn)
{
    SQLite::Statement query(
        database,
        "SELECT d.id, d.owner, d.destination_region_id "
        "FROM conquest_event_decks d "
        "WHERE d.event_id = ? AND d.deployed = 1 AND d.eliminated = 0 AND d.move_resolved = 0 "
        "AND NOT EXISTS (SELECT 1 FROM conquest_battles b "
        "                WHERE b.event_id = d.event_id AND b.turn = ? "
        "                  AND b.status IN (?, ?) "
        "                  AND (b.deck_one_id = d.id OR b.deck_two_id = d.id)) "
        "ORDER BY d.id");
    query.bind(1, eventId);
    query.bind(2, turn);
    query.bind(3, battleStatusValue(BattleStatus::Queued));
    query.bind(4, battleStatusValue(BattleStatus::Ready));

    struct SettledDeck
    {
        std::int64_t id = 0;
        std::string owner;
        int destination = 0;
    };
    std::vector<SettledDeck> decks;
    while (query.executeStep())
    {
        decks.push_back({
            query.getColumn(0).getInt64(),
            query.getColumn(1).getString(),
            query.getColumn(2).getInt()});
    }

    SQLite::Statement control(
        database,
        "UPDATE conquest_regions SET controller = ? WHERE event_id = ? AND region_id = ?");
    SQLite::Statement settle(
        database,
        "UPDATE conquest_event_decks "
        "SET region_id = ?, destination_region_id = ?, move_resolved = 1 WHERE id = ?");
    for (const SettledDeck& deck : decks)
    {
        if (!conquest_map::region(deck.destination))
        {
            continue;
        }
        control.reset();
        control.bind(1, deck.owner);
        control.bind(2, eventId);
        control.bind(3, deck.destination);
        control.exec();

        settle.reset();
        settle.bind(1, deck.destination);
        settle.bind(2, deck.destination);
        settle.bind(3, deck.id);
        settle.exec();
    }
}

bool hasPendingBattles(SQLite::Database& database, std::int64_t eventId, int turn)
{
    SQLite::Statement query(
        database,
        "SELECT 1 FROM conquest_battles "
        "WHERE event_id = ? AND turn = ? AND status IN (?, ?) LIMIT 1");
    query.bind(1, eventId);
    query.bind(2, turn);
    query.bind(3, battleStatusValue(BattleStatus::Queued));
    query.bind(4, battleStatusValue(BattleStatus::Ready));
    return query.executeStep();
}

void completeEvent(
    SQLite::Database& database,
    std::int64_t eventId,
    std::int64_t now,
    const std::string& winner)
{
    SQLite::Statement complete(
        database,
        "UPDATE conquest_events SET phase = ?, turn_ends_at = 0, updated_at = ?, winner = ? "
        "WHERE id = ? AND phase <> ?");
    complete.bind(1, phaseValue(EventPhase::Complete));
    complete.bind(2, now);
    complete.bind(3, winner);
    complete.bind(4, eventId);
    complete.bind(5, phaseValue(EventPhase::Complete));
    if (complete.exec() != 1)
    {
        return;
    }
    if (!winner.empty())
    {
        SQLite::Statement award(
            database,
            "UPDATE accounts SET coins = coins + ? WHERE username = ?");
        award.bind(1, conquest_data::ConquestWinnerRewardCoins);
        award.bind(2, winner);
        if (award.exec() != 1)
        {
            throw std::runtime_error("Conquest winner account is missing");
        }
    }
    ensureDefaultSuccessorInTransaction(database, eventId, now);
}

void finalizeTurnIfReady(SQLite::Database& database, const EventRow& event, std::int64_t now)
{
    if (hasPendingBattles(database, event.id, event.turn))
    {
        return;
    }

    settleDecksWithoutPendingBattles(database, event.id, event.turn);
    eliminateDefeatedPlayers(database, event.id);
    const std::vector<std::string> standing = standingPlayers(database, event.id);
    if (standing.size() <= 1)
    {
        completeEvent(database, event.id, now, standing.empty() ? std::string{} : standing.front());
        return;
    }
    SQLite::Statement nextTurn(
        database,
        "UPDATE conquest_events SET phase = ?, turn = turn + 1, turn_ends_at = ?, updated_at = ? "
        "WHERE id = ?");
    nextTurn.bind(1, phaseValue(EventPhase::Planning));
    nextTurn.bind(2, now + event.turnSeconds);
    nextTurn.bind(3, now);
    nextTurn.bind(4, event.id);
    nextTurn.exec();

    SQLite::Statement resetPlayers(
        database,
        "UPDATE conquest_event_players SET orders_submitted = 0 WHERE event_id = ?");
    resetPlayers.bind(1, event.id);
    resetPlayers.exec();

    SQLite::Statement resetDecks(
        database,
        "UPDATE conquest_event_decks SET destination_region_id = region_id, move_resolved = 1 "
        "WHERE event_id = ? AND deployed = 1 AND eliminated = 0");
    resetDecks.bind(1, event.id);
    resetDecks.exec();
}

void createTurnConflicts(SQLite::Database& database, const EventRow& event, std::int64_t now)
{
    SQLite::Statement resolving(
        database,
        "UPDATE conquest_events SET phase = ?, updated_at = ? WHERE id = ?");
    resolving.bind(1, phaseValue(EventPhase::Resolving));
    resolving.bind(2, now);
    resolving.bind(3, event.id);
    resolving.exec();

    SQLite::Statement unresolved(
        database,
        "UPDATE conquest_event_decks SET move_resolved = 0 "
        "WHERE event_id = ? AND deployed = 1 AND eliminated = 0");
    unresolved.bind(1, event.id);
    unresolved.exec();

    const std::vector<StoredDeck> decks = loadActiveDecks(database, event.id);
    SQLite::Statement insert(
        database,
        "INSERT INTO conquest_battles "
        "(event_id, turn, kind, status, region_id, deck_one_id, deck_two_id, "
        " player_one, player_two, origin_one, origin_two, destination_one, destination_two, "
        " seed, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    for (std::size_t firstIndex = 0; firstIndex < decks.size(); ++firstIndex)
    {
        for (std::size_t secondIndex = firstIndex + 1; secondIndex < decks.size(); ++secondIndex)
        {
            const StoredDeck& first = decks[firstIndex];
            const StoredDeck& second = decks[secondIndex];
            if (first.owner == second.owner)
            {
                continue;
            }

            std::optional<BattleKind> kind;
            int battleRegion = 0;
            if (first.destinationRegionId == second.destinationRegionId)
            {
                kind = BattleKind::Region;
                battleRegion = first.destinationRegionId;
            }
            else if (first.regionId != first.destinationRegionId &&
                     second.regionId != second.destinationRegionId &&
                     first.regionId == second.destinationRegionId &&
                     second.regionId == first.destinationRegionId)
            {
                kind = BattleKind::Crossing;
                battleRegion = first.destinationRegionId;
            }

            if (!kind)
            {
                continue;
            }

            insert.reset();
            insert.bind(1, event.id);
            insert.bind(2, event.turn);
            insert.bind(3, battleKindValue(*kind));
            insert.bind(4, battleStatusValue(BattleStatus::Queued));
            insert.bind(5, battleRegion);
            insert.bind(6, first.id);
            insert.bind(7, second.id);
            insert.bind(8, first.owner);
            insert.bind(9, second.owner);
            insert.bind(10, first.regionId);
            insert.bind(11, second.regionId);
            insert.bind(12, first.destinationRegionId);
            insert.bind(13, second.destinationRegionId);
            insert.bind(14, static_cast<std::int64_t>(newBattleSeed()));
            insert.bind(15, now);
            insert.exec();
        }
    }

    activateQueuedBattles(database, event.id, event.turn);
    settleDecksWithoutPendingBattles(database, event.id, event.turn);
    finalizeTurnIfReady(database, event, now);
}

CommandResult resolveEventInTransaction(
    SQLite::Database& database,
    const EventRow& event,
    std::int64_t now)
{
    if (event.mapId != conquest_map::DarkRealmsId)
    {
        return {false, "Unsupported conquest map"};
    }

    if (event.phase == EventPhase::Complete)
    {
        return {true, "Conquest event is complete"};
    }

    if (event.phase == EventPhase::Registration)
    {
        if (now < event.registrationEndsAt)
        {
            return {true, "Registration remains open"};
        }

        if (participantCount(database, event.id) < 2)
        {
            SQLite::Statement extend(
                database,
                "UPDATE conquest_events SET registration_ends_at = ?, updated_at = ? WHERE id = ?");
            extend.bind(1, now + event.registrationSeconds);
            extend.bind(2, now);
            extend.bind(3, event.id);
            extend.exec();
            return {true, "Registration extended while the event waits for another player"};
        }

        SQLite::Statement start(
            database,
            "UPDATE conquest_events SET phase = ?, turn = 1, turn_ends_at = ?, updated_at = ? "
            "WHERE id = ?");
        start.bind(1, phaseValue(EventPhase::Planning));
        start.bind(2, now + event.turnSeconds);
        start.bind(3, now);
        start.bind(4, event.id);
        start.exec();

        SQLite::Statement reset(
            database,
            "UPDATE conquest_event_players SET orders_submitted = 0 WHERE event_id = ?");
        reset.bind(1, event.id);
        reset.exec();
        return {true, "Conquest planning has begun"};
    }

    if (event.phase == EventPhase::Planning)
    {
        if (!planningReady(database, event, now))
        {
            return {true, "Waiting for orders"};
        }
        createTurnConflicts(database, event, now);
        return {true, "Orders resolved"};
    }

    activateQueuedBattles(database, event.id, event.turn);
    settleDecksWithoutPendingBattles(database, event.id, event.turn);
    finalizeTurnIfReady(database, event, now);
    return {true, hasPendingBattles(database, event.id, event.turn)
        ? "Waiting for conquest battles"
        : "Turn finalized"};
}

std::optional<std::vector<card_data::Card>> loadSnapshotCards(
    SQLite::Database& database,
    std::int64_t eventDeckId)
{
    std::vector<card_data::Card> cards;
    SQLite::Statement query(
        database,
        "SELECT card_title, card_blob FROM conquest_event_deck_cards "
        "WHERE event_deck_id = ? ORDER BY card_index");
    query.bind(1, eventDeckId);
    while (query.executeStep())
    {
        const SQLite::Column blob = query.getColumn(1);
        std::optional<card_data::Card> card = deserializeCard(blob.getBlob(), blob.getBytes());
        if (!card || card->title != query.getColumn(0).getString())
        {
            return std::nullopt;
        }
        cards.push_back(std::move(*card));
    }
    return cards;
}

std::optional<deck_data::Deck> loadBattleDeck(
    SQLite::Database& database,
    std::int64_t eventDeckId,
    std::vector<card_data::Card>& definitions)
{
    SQLite::Statement query(
        database,
        "SELECT deck_name FROM conquest_event_decks WHERE id = ?");
    query.bind(1, eventDeckId);
    if (!query.executeStep())
    {
        return std::nullopt;
    }

    deck_data::Deck deck;
    deck.name = query.getColumn(0).getString();
    const std::optional<std::vector<card_data::Card>> cards =
        loadSnapshotCards(database, eventDeckId);
    if (!cards)
    {
        return std::nullopt;
    }
    definitions = *cards;
    deck.cardTitles.reserve(cards->size());
    for (const card_data::Card& card : *cards)
    {
        deck.cardTitles.push_back(card.title);
    }
    return deck;
}

} // namespace

namespace account_conquest_events
{

void initializeSchema(SQLite::Database& database, std::int64_t now)
{
    now = effectiveNow(now);
    database.exec("PRAGMA foreign_keys = ON");
    database.exec(
        "CREATE TABLE IF NOT EXISTS conquest_events ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "seed_key TEXT UNIQUE,"
        "name TEXT NOT NULL,"
        "map_id TEXT NOT NULL,"
        "phase INTEGER NOT NULL DEFAULT 0 CHECK(phase BETWEEN 0 AND 3),"
        "turn INTEGER NOT NULL DEFAULT 0,"
        "registration_ends_at INTEGER NOT NULL,"
        "turn_ends_at INTEGER NOT NULL DEFAULT 0,"
        "registration_seconds INTEGER NOT NULL,"
        "turn_seconds INTEGER NOT NULL,"
        "reinforcement_cooldown_seconds INTEGER NOT NULL,"
        "winner TEXT NOT NULL DEFAULT '',"
        "created_at INTEGER NOT NULL,"
        "updated_at INTEGER NOT NULL"
        ")");
    database.exec(
        "CREATE TABLE IF NOT EXISTS conquest_event_players ("
        "event_id INTEGER NOT NULL,"
        "username TEXT NOT NULL,"
        "color_index INTEGER NOT NULL CHECK(color_index BETWEEN 0 AND 11),"
        "orders_submitted INTEGER NOT NULL DEFAULT 0 CHECK(orders_submitted IN (0, 1)),"
        "reinforcements_used INTEGER NOT NULL DEFAULT 0,"
        "next_reinforcement_at INTEGER NOT NULL DEFAULT 0,"
        "eliminated INTEGER NOT NULL DEFAULT 0 CHECK(eliminated IN (0, 1)),"
        "joined_at INTEGER NOT NULL,"
        "PRIMARY KEY(event_id, username),"
        "UNIQUE(event_id, color_index),"
        "FOREIGN KEY(event_id) REFERENCES conquest_events(id) ON DELETE CASCADE,"
        "FOREIGN KEY(username) REFERENCES accounts(username) ON DELETE CASCADE"
        ")");
    if (!tableHasColumn(database, "conquest_events", "winner"))
    {
        database.exec("ALTER TABLE conquest_events ADD COLUMN winner TEXT NOT NULL DEFAULT ''");
    }
    if (!tableHasColumn(database, "conquest_event_players", "eliminated"))
    {
        database.exec(
            "ALTER TABLE conquest_event_players ADD COLUMN eliminated INTEGER NOT NULL DEFAULT 0 "
            "CHECK(eliminated IN (0, 1))");
    }
    database.exec(
        "CREATE TABLE IF NOT EXISTS conquest_regions ("
        "event_id INTEGER NOT NULL,"
        "region_id INTEGER NOT NULL CHECK(region_id BETWEEN 1 AND 20),"
        "controller TEXT NOT NULL DEFAULT '',"
        "PRIMARY KEY(event_id, region_id),"
        "FOREIGN KEY(event_id) REFERENCES conquest_events(id) ON DELETE CASCADE"
        ")");
    database.exec(
        "CREATE TABLE IF NOT EXISTS conquest_event_decks ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "event_id INTEGER NOT NULL,"
        "source_deck_id INTEGER NOT NULL,"
        "owner TEXT NOT NULL,"
        "deck_name TEXT NOT NULL,"
        "army_slot INTEGER NOT NULL CHECK(army_slot BETWEEN 0 AND 9),"
        "deployed INTEGER NOT NULL DEFAULT 0 CHECK(deployed IN (0, 1)),"
        "eliminated INTEGER NOT NULL DEFAULT 0 CHECK(eliminated IN (0, 1)),"
        "region_id INTEGER NOT NULL DEFAULT 0 CHECK(region_id BETWEEN 0 AND 20),"
        "destination_region_id INTEGER NOT NULL DEFAULT 0 CHECK(destination_region_id BETWEEN 0 AND 20),"
        "move_resolved INTEGER NOT NULL DEFAULT 1 CHECK(move_resolved IN (0, 1)),"
        "UNIQUE(event_id, owner, army_slot),"
        "UNIQUE(event_id, owner, source_deck_id),"
        "FOREIGN KEY(event_id, owner) REFERENCES conquest_event_players(event_id, username) ON DELETE CASCADE"
        ")");
    database.exec(
        "CREATE TABLE IF NOT EXISTS conquest_event_deck_cards ("
        "event_deck_id INTEGER NOT NULL,"
        "card_index INTEGER NOT NULL,"
        "card_title TEXT NOT NULL,"
        "card_blob BLOB NOT NULL,"
        "PRIMARY KEY(event_deck_id, card_index),"
        "FOREIGN KEY(event_deck_id) REFERENCES conquest_event_decks(id) ON DELETE CASCADE"
        ")");
    database.exec(
        "CREATE TABLE IF NOT EXISTS conquest_event_catalog_cards ("
        "event_id INTEGER NOT NULL,"
        "card_index INTEGER NOT NULL,"
        "card_title TEXT NOT NULL,"
        "card_blob BLOB NOT NULL,"
        "PRIMARY KEY(event_id, card_index),"
        "FOREIGN KEY(event_id) REFERENCES conquest_events(id) ON DELETE CASCADE"
        ")");
    database.exec(
        "CREATE TABLE IF NOT EXISTS conquest_battles ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "event_id INTEGER NOT NULL,"
        "turn INTEGER NOT NULL,"
        "kind INTEGER NOT NULL CHECK(kind BETWEEN 0 AND 1),"
        "status INTEGER NOT NULL CHECK(status BETWEEN 0 AND 3),"
        "region_id INTEGER NOT NULL DEFAULT 0 CHECK(region_id BETWEEN 0 AND 20),"
        "deck_one_id INTEGER NOT NULL,"
        "deck_two_id INTEGER NOT NULL,"
        "player_one TEXT NOT NULL,"
        "player_two TEXT NOT NULL,"
        "origin_one INTEGER NOT NULL,"
        "origin_two INTEGER NOT NULL,"
        "destination_one INTEGER NOT NULL,"
        "destination_two INTEGER NOT NULL,"
        "seed INTEGER NOT NULL,"
        "winner TEXT NOT NULL DEFAULT '',"
        "winner_deck_id INTEGER,"
        "created_at INTEGER NOT NULL,"
        "completed_at INTEGER,"
        "UNIQUE(event_id, turn, deck_one_id, deck_two_id),"
        "FOREIGN KEY(event_id) REFERENCES conquest_events(id) ON DELETE CASCADE,"
        "FOREIGN KEY(deck_one_id) REFERENCES conquest_event_decks(id) ON DELETE CASCADE,"
        "FOREIGN KEY(deck_two_id) REFERENCES conquest_event_decks(id) ON DELETE CASCADE"
        ")");
    database.exec(
        "CREATE TABLE IF NOT EXISTS conquest_battle_actions ("
        "battle_id INTEGER NOT NULL,"
        "sequence INTEGER NOT NULL CHECK(sequence BETWEEN 1 AND 20000),"
        "player_number INTEGER NOT NULL CHECK(player_number IN (1, 2)),"
        "action_type INTEGER NOT NULL CHECK(action_type BETWEEN 0 AND 255),"
        "argument_one INTEGER NOT NULL,"
        "argument_two INTEGER NOT NULL,"
        "argument_three INTEGER NOT NULL,"
        "created_at INTEGER NOT NULL,"
        "PRIMARY KEY(battle_id, sequence),"
        "FOREIGN KEY(battle_id) REFERENCES conquest_battles(id) ON DELETE CASCADE"
        ")");
    database.exec(
        "CREATE TABLE IF NOT EXISTS conquest_battle_capabilities ("
        "battle_id INTEGER PRIMARY KEY,"
        "capability TEXT NOT NULL UNIQUE CHECK(length(capability) = 64),"
        "created_at INTEGER NOT NULL,"
        "FOREIGN KEY(battle_id) REFERENCES conquest_battles(id) ON DELETE CASCADE"
        ")");
    database.exec(
        "CREATE INDEX IF NOT EXISTS conquest_event_decks_position_idx "
        "ON conquest_event_decks(event_id, deployed, eliminated, region_id)");
    database.exec(
        "CREATE INDEX IF NOT EXISTS conquest_battles_event_status_idx "
        "ON conquest_battles(event_id, turn, status)");
    database.exec(
        "CREATE TRIGGER IF NOT EXISTS conquest_event_players_clear_regions "
        "BEFORE DELETE ON conquest_event_players "
        "BEGIN "
        "UPDATE conquest_regions SET controller = '' "
        "WHERE event_id = OLD.event_id AND controller = OLD.username; "
        "END");

    // Older campaigns allowed a player with no deployed decks to wait for a
    // reserve. Under elimination rules, reaching zero decks on the map is
    // terminal for that player, including campaigns upgraded in place.
    database.exec(
        "UPDATE conquest_event_players SET eliminated = 1, orders_submitted = 1 "
        "WHERE event_id IN (SELECT id FROM conquest_events WHERE phase IN (1, 2)) "
        "AND NOT EXISTS (SELECT 1 FROM conquest_event_decks active "
        "WHERE active.event_id = conquest_event_players.event_id "
        "AND active.owner = conquest_event_players.username "
        "AND active.deployed = 1 AND active.eliminated = 0)");

    SQLite::Transaction transaction(database, SQLite::TransactionBehavior::IMMEDIATE);
    SQLite::Statement seed(
        database,
        "INSERT INTO conquest_events "
        "(seed_key, name, map_id, phase, turn, registration_ends_at, turn_ends_at, "
        " registration_seconds, turn_seconds, reinforcement_cooldown_seconds, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, 0, ?, 0, ?, ?, ?, ?, ?) "
        "ON CONFLICT(seed_key) DO NOTHING");
    seed.bind(1, DefaultEventSeedKey);
    seed.bind(2, DefaultEventName);
    seed.bind(3, std::string(conquest_map::DarkRealmsId));
    seed.bind(4, phaseValue(EventPhase::Registration));
    seed.bind(5, now + DefaultRegistrationSeconds);
    seed.bind(6, DefaultRegistrationSeconds);
    seed.bind(7, DefaultTurnSeconds);
    seed.bind(8, DefaultReinforcementCooldownSeconds);
    seed.bind(9, now);
    seed.bind(10, now);
    seed.exec();

    SQLite::Statement eventQuery(database, "SELECT id FROM conquest_events WHERE seed_key = ?");
    eventQuery.bind(1, DefaultEventSeedKey);
    if (eventQuery.executeStep())
    {
        populateDarkRealmsEvent(database, eventQuery.getColumn(0).getInt64());
    }

    // Backfill a successor when upgrading a database whose last campaign was
    // completed before recurring Conquest events were introduced. Only the
    // newest valid default-lineage event may create the next cycle.
    std::optional<std::int64_t> completedToBackfill;
    {
        SQLite::Statement latest(
            database,
            "SELECT id, seed_key, phase FROM conquest_events "
            "WHERE seed_key = ? OR seed_key GLOB ? ORDER BY id DESC");
        latest.bind(1, DefaultEventSeedKey);
        latest.bind(2, std::string(DefaultEventSeedKey) + "-after-[0-9]*");
        while (latest.executeStep())
        {
            if (!isDefaultEventLineage(latest.getColumn(1).getString()))
            {
                continue;
            }
            if (latest.getColumn(2).getInt() == phaseValue(EventPhase::Complete))
            {
                completedToBackfill = latest.getColumn(0).getInt64();
            }
            break;
        }
    }
    if (completedToBackfill)
    {
        ensureDefaultSuccessorInTransaction(database, *completedToBackfill, now);
    }
    transaction.commit();
}

std::vector<conquest_data::EventSummary> listEvents(
    SQLite::Database& database,
    const std::string& username,
    std::int64_t now)
{
    now = effectiveNow(now);
    std::vector<std::int64_t> eventIds;
    SQLite::Statement ids(
        database,
        "SELECT id FROM conquest_events WHERE phase <> ? ORDER BY id");
    ids.bind(1, phaseValue(EventPhase::Complete));
    while (ids.executeStep())
    {
        eventIds.push_back(ids.getColumn(0).getInt64());
    }
    for (const std::int64_t id : eventIds)
    {
        resolveEvent(database, static_cast<std::uint64_t>(id), now);
    }

    std::vector<conquest_data::EventSummary> events;
    SQLite::Statement query(
        database,
        "SELECT e.id, e.name, e.map_id, e.phase, e.turn, e.registration_ends_at, e.turn_ends_at, "
        "(SELECT COUNT(*) FROM conquest_event_players p WHERE p.event_id = e.id), "
        "EXISTS(SELECT 1 FROM conquest_event_players p WHERE p.event_id = e.id AND p.username = ?), "
        "e.winner "
        "FROM conquest_events e "
        "ORDER BY CASE WHEN e.phase = ? THEN 1 ELSE 0 END, e.id DESC LIMIT ?");
    query.bind(1, username);
    query.bind(2, phaseValue(EventPhase::Complete));
    query.bind(3, static_cast<int>(conquest_data::MaxConquestEvents));
    while (query.executeStep())
    {
        conquest_data::EventSummary event;
        event.id = static_cast<std::uint64_t>(query.getColumn(0).getInt64());
        event.name = query.getColumn(1).getString();
        event.mapId = query.getColumn(2).getString();
        event.phase = static_cast<EventPhase>(query.getColumn(3).getInt());
        event.turn = query.getColumn(4).getInt();
        event.registrationEndsAt = query.getColumn(5).getInt64();
        event.turnEndsAt = query.getColumn(6).getInt64();
        event.participantCount = static_cast<std::uint32_t>(query.getColumn(7).getInt());
        event.joined = query.getColumn(8).getInt() != 0;
        event.winner = query.getColumn(9).getString();
        events.push_back(std::move(event));
    }
    return events;
}

std::optional<conquest_data::EventState> loadEventState(
    SQLite::Database& database,
    std::uint64_t eventId,
    const std::string& username,
    std::string& error,
    std::int64_t now)
{
    error.clear();
    if (!validDatabaseId(eventId))
    {
        error = "Invalid conquest event";
        return std::nullopt;
    }
    now = effectiveNow(now);
    resolveEvent(database, eventId, now);

    const std::optional<EventRow> event = loadEventRow(database, databaseId(eventId));
    if (!event)
    {
        error = "Conquest event not found";
        return std::nullopt;
    }

    conquest_data::EventState state;
    state.summary.id = eventId;
    state.summary.name = event->name;
    state.summary.mapId = event->mapId;
    state.summary.phase = event->phase;
    state.summary.turn = event->turn;
    state.summary.registrationEndsAt = event->registrationEndsAt;
    state.summary.turnEndsAt = event->turnEndsAt;
    state.summary.participantCount = static_cast<std::uint32_t>(participantCount(database, event->id));
    state.summary.joined = participantExists(database, event->id, username);
    state.summary.winner = event->winner;

    SQLite::Statement players(
        database,
        "SELECT p.username, p.color_index, p.orders_submitted, p.reinforcements_used, "
        "p.next_reinforcement_at, p.eliminated, "
        "(SELECT COUNT(*) FROM conquest_regions r WHERE r.event_id = p.event_id AND r.controller = p.username), "
        "(SELECT COUNT(*) FROM conquest_event_decks d WHERE d.event_id = p.event_id AND d.owner = p.username "
        " AND d.deployed = 0 AND d.eliminated = 0) "
        "FROM conquest_event_players p WHERE p.event_id = ? ORDER BY p.color_index");
    players.bind(1, event->id);
    while (players.executeStep())
    {
        conquest_data::PlayerState player;
        player.username = players.getColumn(0).getString();
        player.colorIndex = static_cast<std::uint8_t>(players.getColumn(1).getInt());
        player.ordersSubmitted = players.getColumn(2).getInt() != 0;
        const int reinforcementsUsed = players.getColumn(3).getInt();
        player.nextReinforcementAt = players.getColumn(4).getInt64();
        player.eliminated = players.getColumn(5).getInt() != 0;
        player.controlledRegions = players.getColumn(6).getInt();
        const int undeployed = players.getColumn(7).getInt();
        player.reinforcementsAvailable = player.eliminated ? 0 : std::min(
            undeployed, std::max(0, player.controlledRegions / 4 - reinforcementsUsed));
        state.players.push_back(std::move(player));
    }

    SQLite::Statement regions(
        database,
        "SELECT region_id, controller FROM conquest_regions WHERE event_id = ? ORDER BY region_id");
    regions.bind(1, event->id);
    while (regions.executeStep())
    {
        state.regions.push_back({
            regions.getColumn(0).getInt(),
            regions.getColumn(1).getString()});
    }

    SQLite::Statement decks(
        database,
        "SELECT id, source_deck_id, owner, deck_name, army_slot, deployed, eliminated, "
        "region_id, destination_region_id FROM conquest_event_decks "
        "WHERE event_id = ? ORDER BY owner, army_slot");
    decks.bind(1, event->id);
    while (decks.executeStep())
    {
        conquest_data::EventDeckState deck;
        deck.id = static_cast<std::uint64_t>(decks.getColumn(0).getInt64());
        deck.sourceDeckId = static_cast<std::uint64_t>(decks.getColumn(1).getInt64());
        deck.owner = decks.getColumn(2).getString();
        deck.deckName = decks.getColumn(3).getString();
        deck.armySlot = decks.getColumn(4).getInt();
        deck.deployed = decks.getColumn(5).getInt() != 0;
        deck.eliminated = decks.getColumn(6).getInt() != 0;
        deck.regionId = decks.getColumn(7).getInt();
        deck.destinationRegionId = decks.getColumn(8).getInt();
        if (event->phase == EventPhase::Planning && deck.owner != username)
        {
            deck.destinationRegionId = deck.regionId;
        }
        state.decks.push_back(std::move(deck));
    }

    SQLite::Statement battles(
        database,
        "SELECT b.id, b.kind, b.status, b.region_id, b.deck_one_id, b.deck_two_id, "
        "b.player_one, b.player_two, d1.deck_name, d2.deck_name, b.winner "
        "FROM conquest_battles b "
        "JOIN conquest_event_decks d1 ON d1.id = b.deck_one_id "
        "JOIN conquest_event_decks d2 ON d2.id = b.deck_two_id "
        "WHERE b.event_id = ? AND ("
        " b.status = ? OR b.status IN (?, ?)) "
        "ORDER BY CASE WHEN b.status = ? THEN 0 ELSE 1 END, b.id DESC LIMIT ?");
    battles.bind(1, event->id);
    battles.bind(2, battleStatusValue(BattleStatus::Ready));
    battles.bind(3, battleStatusValue(BattleStatus::Complete));
    battles.bind(4, battleStatusValue(BattleStatus::Cancelled));
    battles.bind(5, battleStatusValue(BattleStatus::Ready));
    battles.bind(6, static_cast<int>(conquest_data::MaxConquestBattles));
    while (battles.executeStep())
    {
        conquest_data::BattleState battle;
        battle.id = static_cast<std::uint64_t>(battles.getColumn(0).getInt64());
        battle.kind = static_cast<BattleKind>(battles.getColumn(1).getInt());
        battle.status = static_cast<BattleStatus>(battles.getColumn(2).getInt());
        battle.regionId = battles.getColumn(3).getInt();
        battle.deckOneId = static_cast<std::uint64_t>(battles.getColumn(4).getInt64());
        battle.deckTwoId = static_cast<std::uint64_t>(battles.getColumn(5).getInt64());
        battle.playerOne = battles.getColumn(6).getString();
        battle.playerTwo = battles.getColumn(7).getString();
        battle.deckOneName = battles.getColumn(8).getString();
        battle.deckTwoName = battles.getColumn(9).getString();
        battle.winner = battles.getColumn(10).getString();
        battle.canJoin = battle.status == BattleStatus::Ready &&
            (battle.playerOne == username || battle.playerTwo == username) &&
            !playerEliminated(database, event->id, username);
        state.battles.push_back(std::move(battle));
    }
    std::reverse(state.battles.begin(), state.battles.end());
    return state;
}

CommandResult joinEvent(
    SQLite::Database& database,
    std::uint64_t eventId,
    const std::string& username,
    const std::vector<conquest_data::StartingPlacement>& placements,
    std::int64_t now)
{
    if (!validDatabaseId(eventId) || username.empty())
    {
        return {false, "Invalid conquest join request"};
    }
    if (placements.empty() || placements.size() > 2)
    {
        return {false, "Place one or two decks"};
    }
    if (!conquest_map::isEdgeRegion(placements.front().regionId) ||
        (placements.size() == 2 &&
         (!conquest_map::isEdgeRegion(placements[1].regionId) ||
          !conquest_map::areAdjacent(placements[0].regionId, placements[1].regionId))))
    {
        return {false, "Starting decks must use adjacent edge regions"};
    }
    if (placements.size() == 2 &&
        (placements[0].deckId == placements[1].deckId ||
         placements[0].regionId == placements[1].regionId))
    {
        return {false, "Starting decks and regions must be distinct"};
    }

    now = effectiveNow(now);
    if (const std::optional<EventRow> expired =
            loadEventRow(database, databaseId(eventId));
        expired && expired->phase == EventPhase::Registration &&
        now >= expired->registrationEndsAt)
    {
        // Commit lazy deadline progression independently of the entrant's
        // later deck/placement validation. Otherwise a rejected late join
        // could roll back an extension or the transition into Planning.
        const CommandResult resolved = resolveEvent(database, eventId, now);
        if (!resolved.success)
        {
            return resolved;
        }
    }

    SQLite::Transaction transaction(database, SQLite::TransactionBehavior::IMMEDIATE);
    const std::optional<EventRow> event = loadEventRow(database, databaseId(eventId));
    if (!event)
    {
        return {false, "Conquest event not found"};
    }
    if (event->phase != EventPhase::Registration || now >= event->registrationEndsAt)
    {
        return {false, "Registration is closed"};
    }
    if (event->mapId != conquest_map::DarkRealmsId)
    {
        return {false, "Unsupported conquest map"};
    }
    if (participantExists(database, event->id, username))
    {
        return {false, "Already joined this conquest"};
    }
    if (participantCount(database, event->id) >= static_cast<int>(conquest_data::MaxConquestPlayers))
    {
        return {false, "This conquest is full"};
    }

    SQLite::Statement occupied(
        database,
        "SELECT 1 FROM conquest_event_decks "
        "WHERE event_id = ? AND deployed = 1 AND eliminated = 0 AND region_id = ? LIMIT 1");
    for (const conquest_data::StartingPlacement& placement : placements)
    {
        occupied.reset();
        occupied.bind(1, event->id);
        occupied.bind(2, placement.regionId);
        if (occupied.executeStep())
        {
            return {false, "A starting region is already occupied"};
        }
    }

    struct ArmyDeck
    {
        int slot = 0;
        std::int64_t sourceId = 0;
        std::string name;
        std::vector<FrozenCard> cards;
    };
    std::unordered_map<std::string, FrozenCard> cardsByTitle;
    SQLite::Statement eventCatalog(
        database,
        "SELECT card_title, card_blob FROM conquest_event_catalog_cards "
        "WHERE event_id = ? ORDER BY card_index");
    eventCatalog.bind(1, event->id);
    while (eventCatalog.executeStep())
    {
        const std::string title = eventCatalog.getColumn(0).getString();
        const SQLite::Column blob = eventCatalog.getColumn(1);
        const auto* first = static_cast<const std::uint8_t*>(blob.getBlob());
        const std::optional<card_data::Card> definition =
            deserializeCard(first, blob.getBytes());
        if (first == nullptr || blob.getBytes() <= 0 ||
            !definition || definition->title != title)
        {
            return {false, "The conquest event card catalog is invalid"};
        }
        cardsByTitle.emplace(
            title,
            FrozenCard{title, std::vector<std::uint8_t>(first, first + blob.getBytes())});
    }
    if (cardsByTitle.empty())
    {
        return {false, "The conquest event card catalog is missing"};
    }
    std::vector<ArmyDeck> army;
    SQLite::Statement armyQuery(
        database,
        "SELECT a.slot_index, d.id, d.name "
        "FROM conquest_army_decks a "
        "JOIN conquest_decks d ON d.username = a.username AND d.id = a.deck_id "
        "WHERE a.username = ? ORDER BY a.slot_index");
    armyQuery.bind(1, username);
    while (armyQuery.executeStep())
    {
        ArmyDeck deck;
        deck.slot = armyQuery.getColumn(0).getInt();
        deck.sourceId = armyQuery.getColumn(1).getInt64();
        deck.name = armyQuery.getColumn(2).getString();
        SQLite::Statement cards(
            database,
            "SELECT card_title FROM conquest_deck_cards WHERE deck_id = ? ORDER BY card_index");
        cards.bind(1, deck.sourceId);
        while (cards.executeStep())
        {
            const std::string title = cards.getColumn(0).getString();
            const auto authoritative = cardsByTitle.find(title);
            if (authoritative == cardsByTitle.end())
            {
                return {false, "An army deck contains an unknown card: " + title};
            }
            deck.cards.push_back(authoritative->second);
        }
        std::vector<card_data::Card> resolvedCards;
        resolvedCards.reserve(deck.cards.size());
        for (const FrozenCard& frozen : deck.cards)
        {
            const std::optional<card_data::Card> definition =
                deserializeCard(frozen.blob.data(), static_cast<int>(frozen.blob.size()));
            if (!definition)
            {
                return {false, "An army deck uses an invalid frozen card"};
            }
            resolvedCards.push_back(*definition);
        }
        if (const std::optional<std::string> rulesError =
                game_data::deckRulesError(resolvedCards))
        {
            return {false, deck.name + ": " + *rulesError};
        }
        army.push_back(std::move(deck));
    }
    if (army.empty() || army.size() > conquest_data::MaxConquestArmyDecks)
    {
        return {false, "Save an army of one to ten Conquest decks first"};
    }

    SQLite::Statement reserved(
        database,
        "SELECT 1 FROM conquest_event_decks snapshot "
        "JOIN conquest_events active ON active.id = snapshot.event_id "
        "WHERE snapshot.owner = ? AND snapshot.source_deck_id = ? "
        "AND active.phase <> ? AND snapshot.event_id <> ? LIMIT 1");
    for (const ArmyDeck& deck : army)
    {
        reserved.reset();
        reserved.bind(1, username);
        reserved.bind(2, deck.sourceId);
        reserved.bind(3, phaseValue(EventPhase::Complete));
        reserved.bind(4, event->id);
        if (reserved.executeStep())
        {
            return {
                false,
                "Conquest deck " + deck.name +
                    " is already committed to another active conquest"};
        }
    }

    for (const conquest_data::StartingPlacement& placement : placements)
    {
        if (!validDatabaseId(placement.deckId) ||
            std::none_of(army.begin(), army.end(), [&](const ArmyDeck& deck) {
                return deck.sourceId == databaseId(placement.deckId);
            }))
        {
            return {false, "A starting deck is not in the saved army"};
        }
    }

    std::array<bool, conquest_map::PlayerColors.size()> usedColors{};
    SQLite::Statement colors(
        database,
        "SELECT color_index FROM conquest_event_players WHERE event_id = ?");
    colors.bind(1, event->id);
    while (colors.executeStep())
    {
        const int color = colors.getColumn(0).getInt();
        if (color >= 0 && color < static_cast<int>(usedColors.size()))
        {
            usedColors[static_cast<std::size_t>(color)] = true;
        }
    }
    const auto freeColor = std::find(usedColors.begin(), usedColors.end(), false);
    if (freeColor == usedColors.end())
    {
        return {false, "No player color is available"};
    }
    const int colorIndex = static_cast<int>(std::distance(usedColors.begin(), freeColor));

    SQLite::Statement chargeEntryFee(
        database,
        "UPDATE accounts SET coins = coins - ? WHERE username = ? AND coins >= ?");
    chargeEntryFee.bind(1, conquest_data::ConquestEntryFeeCoins);
    chargeEntryFee.bind(2, username);
    chargeEntryFee.bind(3, conquest_data::ConquestEntryFeeCoins);
    if (chargeEntryFee.exec() != 1)
    {
        return {
            false,
            "Need " + std::to_string(conquest_data::ConquestEntryFeeCoins) +
                " coins to join the conquest"};
    }

    SQLite::Statement insertPlayer(
        database,
        "INSERT INTO conquest_event_players "
        "(event_id, username, color_index, joined_at) VALUES (?, ?, ?, ?)");
    insertPlayer.bind(1, event->id);
    insertPlayer.bind(2, username);
    insertPlayer.bind(3, colorIndex);
    insertPlayer.bind(4, now);
    insertPlayer.exec();

    std::unordered_map<std::int64_t, std::int64_t> snapshotIds;
    SQLite::Statement insertDeck(
        database,
        "INSERT INTO conquest_event_decks "
        "(event_id, source_deck_id, owner, deck_name, army_slot) VALUES (?, ?, ?, ?, ?)");
    SQLite::Statement insertCard(
        database,
        "INSERT INTO conquest_event_deck_cards "
        "(event_deck_id, card_index, card_title, card_blob) VALUES (?, ?, ?, ?)");
    for (const ArmyDeck& deck : army)
    {
        insertDeck.reset();
        insertDeck.bind(1, event->id);
        insertDeck.bind(2, deck.sourceId);
        insertDeck.bind(3, username);
        insertDeck.bind(4, deck.name);
        insertDeck.bind(5, deck.slot);
        insertDeck.exec();
        const std::int64_t snapshotId = database.getLastInsertRowid();
        snapshotIds.emplace(deck.sourceId, snapshotId);
        for (std::size_t cardIndex = 0; cardIndex < deck.cards.size(); ++cardIndex)
        {
            insertCard.reset();
            insertCard.bind(1, snapshotId);
            insertCard.bind(2, static_cast<int>(cardIndex));
            insertCard.bind(3, deck.cards[cardIndex].title);
            insertCard.bind(
                4,
                deck.cards[cardIndex].blob.data(),
                static_cast<int>(deck.cards[cardIndex].blob.size()));
            insertCard.exec();
        }
    }

    SQLite::Statement deploy(
        database,
        "UPDATE conquest_event_decks SET deployed = 1, region_id = ?, "
        "destination_region_id = ?, move_resolved = 1 WHERE id = ?");
    SQLite::Statement control(
        database,
        "UPDATE conquest_regions SET controller = ? WHERE event_id = ? AND region_id = ?");
    for (const conquest_data::StartingPlacement& placement : placements)
    {
        const std::int64_t sourceId = databaseId(placement.deckId);
        deploy.reset();
        deploy.bind(1, placement.regionId);
        deploy.bind(2, placement.regionId);
        deploy.bind(3, snapshotIds.at(sourceId));
        deploy.exec();

        control.reset();
        control.bind(1, username);
        control.bind(2, event->id);
        control.bind(3, placement.regionId);
        control.exec();
    }

    transaction.commit();
    return {
        true,
        "Joined the conquest for " +
            std::to_string(conquest_data::ConquestEntryFeeCoins) + " coins"};
}

CommandResult submitOrders(
    SQLite::Database& database,
    std::uint64_t eventId,
    const std::string& username,
    const std::vector<conquest_data::MoveOrder>& orders,
    std::int64_t now)
{
    if (!validDatabaseId(eventId) || username.empty() || orders.size() > conquest_data::MaxConquestOrders)
    {
        return {false, "Invalid conquest orders"};
    }
    now = effectiveNow(now);
    SQLite::Transaction transaction(database, SQLite::TransactionBehavior::IMMEDIATE);
    const std::optional<EventRow> event = loadEventRow(database, databaseId(eventId));
    if (!event)
    {
        return {false, "Conquest event not found"};
    }
    if (event->phase != EventPhase::Planning)
    {
        return {false, "The conquest is not accepting move orders"};
    }
    if (event->turnEndsAt > 0 && now >= event->turnEndsAt)
    {
        const CommandResult resolved = resolveEventInTransaction(database, *event, now);
        transaction.commit();
        return {false, resolved.success ? "The order deadline has passed" : resolved.message};
    }
    if (!participantExists(database, event->id, username))
    {
        return {false, "Join the conquest before submitting orders"};
    }
    if (playerEliminated(database, event->id, username))
    {
        return {false, "Your conquest army has been defeated"};
    }

    std::unordered_map<std::int64_t, int> projected;
    SQLite::Statement decks(
        database,
        "SELECT id, region_id FROM conquest_event_decks "
        "WHERE event_id = ? AND owner = ? AND deployed = 1 AND eliminated = 0");
    decks.bind(1, event->id);
    decks.bind(2, username);
    while (decks.executeStep())
    {
        projected.emplace(decks.getColumn(0).getInt64(), decks.getColumn(1).getInt());
    }
    if (projected.empty() && !orders.empty())
    {
        return {false, "No deployed decks can receive those orders"};
    }

    std::unordered_set<std::int64_t> orderedDecks;
    for (const conquest_data::MoveOrder& order : orders)
    {
        if (!validDatabaseId(order.eventDeckId))
        {
            return {false, "Invalid deck in move orders"};
        }
        const std::int64_t deckId = databaseId(order.eventDeckId);
        const auto found = projected.find(deckId);
        if (found == projected.end())
        {
            return {false, "A move order does not belong to an active deck"};
        }
        if (!orderedDecks.insert(deckId).second)
        {
            return {false, "A deck has more than one move order"};
        }
        if (!conquest_map::region(order.destinationRegionId) ||
            (order.destinationRegionId != found->second &&
             !conquest_map::areAdjacent(found->second, order.destinationRegionId)))
        {
            return {false, "A deck may only stay or move to a touching region"};
        }
        found->second = order.destinationRegionId;
    }

    std::unordered_set<int> occupiedDestinations;
    for (const auto& [deckId, destination] : projected)
    {
        static_cast<void>(deckId);
        if (!occupiedDestinations.insert(destination).second)
        {
            return {false, "Two of your decks would end in the same region"};
        }
    }

    SQLite::Statement update(
        database,
        "UPDATE conquest_event_decks SET destination_region_id = ?, move_resolved = 0 WHERE id = ?");
    for (const auto& [deckId, destination] : projected)
    {
        update.reset();
        update.bind(1, destination);
        update.bind(2, deckId);
        update.exec();
    }

    SQLite::Statement submitted(
        database,
        "UPDATE conquest_event_players SET orders_submitted = 1 WHERE event_id = ? AND username = ?");
    submitted.bind(1, event->id);
    submitted.bind(2, username);
    submitted.exec();

    CommandResult result{true, "Orders submitted"};
    if (planningReady(database, *event, now))
    {
        result = resolveEventInTransaction(database, *event, now);
    }
    transaction.commit();
    return result;
}

CommandResult resolveEvent(
    SQLite::Database& database,
    std::uint64_t eventId,
    std::int64_t now)
{
    if (!validDatabaseId(eventId))
    {
        return {false, "Invalid conquest event"};
    }
    now = effectiveNow(now);
    SQLite::Transaction transaction(database, SQLite::TransactionBehavior::IMMEDIATE);
    const std::optional<EventRow> event = loadEventRow(database, databaseId(eventId));
    if (!event)
    {
        return {false, "Conquest event not found"};
    }
    const CommandResult result = resolveEventInTransaction(database, *event, now);
    transaction.commit();
    return result;
}

CommandResult forceStartEvent(
    SQLite::Database& database,
    std::uint64_t eventId,
    const std::string& username,
    std::int64_t now)
{
    if (!validDatabaseId(eventId) || username.empty())
    {
        return {false, "Invalid conquest start request"};
    }

    now = effectiveNow(now);
    SQLite::Transaction transaction(database, SQLite::TransactionBehavior::IMMEDIATE);
    SQLite::Statement admin(
        database,
        "SELECT is_admin FROM accounts WHERE username = ? LIMIT 1");
    admin.bind(1, username);
    if (!admin.executeStep() || admin.getColumn(0).getInt() == 0)
    {
        return {false, "Admin privilege required"};
    }

    const std::optional<EventRow> event = loadEventRow(database, databaseId(eventId));
    if (!event)
    {
        return {false, "Conquest event not found"};
    }
    if (event->phase != EventPhase::Registration)
    {
        return {false, "Only a registering conquest event can be started"};
    }
    if (participantCount(database, event->id) < 2)
    {
        return {false, "At least two players are required to start the conquest"};
    }

    SQLite::Statement closeRegistration(
        database,
        "UPDATE conquest_events SET registration_ends_at = ?, updated_at = ? WHERE id = ?");
    closeRegistration.bind(1, now);
    closeRegistration.bind(2, now);
    closeRegistration.bind(3, event->id);
    closeRegistration.exec();

    EventRow startingEvent = *event;
    startingEvent.registrationEndsAt = now;
    const CommandResult result = resolveEventInTransaction(database, startingEvent, now);
    if (result.success)
    {
        transaction.commit();
        return {true, "Conquest started by an admin"};
    }
    return result;
}

CommandResult deployReinforcement(
    SQLite::Database& database,
    std::uint64_t eventId,
    const std::string& username,
    std::uint64_t eventDeckId,
    int regionId,
    std::int64_t now)
{
    if (!validDatabaseId(eventId) || !validDatabaseId(eventDeckId) || username.empty())
    {
        return {false, "Invalid reinforcement request"};
    }
    if (!conquest_map::isEdgeRegion(regionId))
    {
        return {false, "Reinforcements must enter through an edge region"};
    }
    now = effectiveNow(now);
    SQLite::Transaction transaction(database, SQLite::TransactionBehavior::IMMEDIATE);
    const std::optional<EventRow> event = loadEventRow(database, databaseId(eventId));
    if (!event)
    {
        return {false, "Conquest event not found"};
    }
    if (event->phase != EventPhase::Planning)
    {
        return {false, "Reinforcements can only enter during planning"};
    }
    if (event->turnEndsAt > 0 && now >= event->turnEndsAt)
    {
        const CommandResult resolved = resolveEventInTransaction(database, *event, now);
        transaction.commit();
        return {false, resolved.success
            ? "The reinforcement deadline has passed"
            : resolved.message};
    }

    SQLite::Statement player(
        database,
        "SELECT orders_submitted, reinforcements_used, next_reinforcement_at "
        "FROM conquest_event_players WHERE event_id = ? AND username = ?");
    player.bind(1, event->id);
    player.bind(2, username);
    if (!player.executeStep())
    {
        return {false, "Join the conquest before reinforcing"};
    }
    if (playerEliminated(database, event->id, username))
    {
        return {false, "Your conquest army has been defeated"};
    }
    if (player.getColumn(0).getInt() != 0)
    {
        return {false, "Change reinforcements before submitting move orders"};
    }
    const int reinforcementsUsed = player.getColumn(1).getInt();
    const std::int64_t nextAt = player.getColumn(2).getInt64();
    if (now < nextAt)
    {
        return {false, "Reinforcement cooldown is still active"};
    }

    SQLite::Statement controlled(
        database,
        "SELECT COUNT(*) FROM conquest_regions WHERE event_id = ? AND controller = ?");
    controlled.bind(1, event->id);
    controlled.bind(2, username);
    controlled.executeStep();
    if (controlled.getColumn(0).getInt() / 4 <= reinforcementsUsed)
    {
        return {false, "Control four regions for each reinforcement"};
    }

    SQLite::Statement edgeControl(
        database,
        "SELECT controller FROM conquest_regions WHERE event_id = ? AND region_id = ?");
    edgeControl.bind(1, event->id);
    edgeControl.bind(2, regionId);
    if (!edgeControl.executeStep() || edgeControl.getColumn(0).getString() != username)
    {
        return {false, "You do not control that edge region"};
    }

    SQLite::Statement occupant(
        database,
        "SELECT 1 FROM conquest_event_decks "
        "WHERE event_id = ? AND deployed = 1 AND eliminated = 0 AND region_id = ? LIMIT 1");
    occupant.bind(1, event->id);
    occupant.bind(2, regionId);
    if (occupant.executeStep())
    {
        return {false, "That edge region already contains a deck"};
    }

    SQLite::Statement deck(
        database,
        "SELECT deployed, eliminated FROM conquest_event_decks "
        "WHERE id = ? AND event_id = ? AND owner = ?");
    deck.bind(1, databaseId(eventDeckId));
    deck.bind(2, event->id);
    deck.bind(3, username);
    if (!deck.executeStep() || deck.getColumn(0).getInt() != 0 || deck.getColumn(1).getInt() != 0)
    {
        return {false, "Choose an undeployed deck from this army"};
    }

    SQLite::Statement deploy(
        database,
        "UPDATE conquest_event_decks SET deployed = 1, region_id = ?, destination_region_id = ?, "
        "move_resolved = 1 WHERE id = ?");
    deploy.bind(1, regionId);
    deploy.bind(2, regionId);
    deploy.bind(3, databaseId(eventDeckId));
    deploy.exec();

    SQLite::Statement consume(
        database,
        "UPDATE conquest_event_players SET reinforcements_used = reinforcements_used + 1, "
        "next_reinforcement_at = ? WHERE event_id = ? AND username = ?");
    consume.bind(1, now + event->reinforcementCooldownSeconds);
    consume.bind(2, event->id);
    consume.bind(3, username);
    consume.exec();
    transaction.commit();
    return {true, "Reinforcement deployed"};
}

std::optional<conquest_data::BattleData> loadBattleData(
    SQLite::Database& database,
    std::uint64_t battleId,
    const std::string& username,
    int& playerNumber,
    std::string& error)
{
    playerNumber = 0;
    error.clear();
    if (!validDatabaseId(battleId) || username.empty())
    {
        error = "Invalid conquest battle";
        return std::nullopt;
    }

    SQLite::Transaction transaction(database);
    SQLite::Statement battle(
        database,
        "SELECT event_id, status, deck_one_id, deck_two_id, player_one, player_two, seed "
        "FROM conquest_battles WHERE id = ?");
    battle.bind(1, databaseId(battleId));
    if (!battle.executeStep())
    {
        error = "Conquest battle not found";
        return std::nullopt;
    }
    if (battle.getColumn(1).getInt() != battleStatusValue(BattleStatus::Ready))
    {
        error = "Conquest battle is not ready";
        return std::nullopt;
    }

    const std::int64_t eventId = battle.getColumn(0).getInt64();
    const std::int64_t deckOneId = battle.getColumn(2).getInt64();
    const std::int64_t deckTwoId = battle.getColumn(3).getInt64();
    const std::string playerOne = battle.getColumn(4).getString();
    const std::string playerTwo = battle.getColumn(5).getString();
    if (username == playerOne)
    {
        playerNumber = 1;
    }
    else if (username == playerTwo)
    {
        playerNumber = 2;
    }
    else
    {
        error = "You are not a participant in this battle";
        return std::nullopt;
    }

    std::vector<card_data::Card> cardsOne;
    std::vector<card_data::Card> cardsTwo;
    const std::optional<deck_data::Deck> deckOne =
        loadBattleDeck(database, deckOneId, cardsOne);
    const std::optional<deck_data::Deck> deckTwo =
        loadBattleDeck(database, deckTwoId, cardsTwo);
    if (!deckOne || !deckTwo)
    {
        error = "Conquest battle deck snapshot is missing";
        playerNumber = 0;
        return std::nullopt;
    }

    conquest_data::BattleData data;
    data.battleId = battleId;
    data.seed = static_cast<std::uint32_t>(battle.getColumn(6).getInt64());
    data.playerOne = playerOne;
    data.playerTwo = playerTwo;
    data.deckOne = *deckOne;
    data.deckTwo = *deckTwo;
    data.cardsOne = std::move(cardsOne);
    data.cardsTwo = std::move(cardsTwo);

    SQLite::Statement catalog(
        database,
        "SELECT card_title, card_blob FROM conquest_event_catalog_cards "
        "WHERE event_id = ? ORDER BY card_index");
    catalog.bind(1, eventId);
    while (catalog.executeStep())
    {
        if (data.catalog.size() >= conquest_data::MaxConquestCatalogCards)
        {
            error = "Conquest event catalog snapshot is too large";
            playerNumber = 0;
            return std::nullopt;
        }
        const SQLite::Column blob = catalog.getColumn(1);
        std::optional<card_data::Card> card = deserializeCard(blob.getBlob(), blob.getBytes());
        if (!card || card->title != catalog.getColumn(0).getString())
        {
            error = "Conquest event catalog snapshot is invalid";
            playerNumber = 0;
            return std::nullopt;
        }
        data.catalog.push_back(std::move(*card));
    }
    if (data.catalog.empty())
    {
        error = "Conquest event catalog snapshot is missing";
        playerNumber = 0;
        return std::nullopt;
    }

    SQLite::Statement actions(
        database,
        "SELECT sequence, player_number, action_type, argument_one, argument_two, argument_three "
        "FROM conquest_battle_actions WHERE battle_id = ? ORDER BY sequence");
    actions.bind(1, databaseId(battleId));
    while (actions.executeStep())
    {
        if (data.actions.size() >= conquest_data::MaxConquestBattleActions)
        {
            error = "Conquest battle action log is too large";
            playerNumber = 0;
            return std::nullopt;
        }
        data.actions.push_back({
            static_cast<std::uint32_t>(actions.getColumn(0).getInt64()),
            actions.getColumn(1).getInt(),
            static_cast<std::uint8_t>(actions.getColumn(2).getInt()),
            actions.getColumn(3).getInt(),
            actions.getColumn(4).getInt(),
            actions.getColumn(5).getInt()});
    }
    transaction.commit();
    return data;
}

std::optional<conquest_data::BattleData> loadBattleDataForCoordinator(
    SQLite::Database& database,
    std::uint64_t battleId,
    const std::string& username,
    int& playerNumber,
    std::string& capability,
    std::string& error)
{
    capability.clear();
    std::optional<conquest_data::BattleData> data =
        loadBattleData(database, battleId, username, playerNumber, error);
    if (!data)
    {
        return std::nullopt;
    }

    capability = loadOrCreateBattleCapability(database, databaseId(battleId));
    return data;
}

std::optional<conquest_data::BattleData> reloadBattleDataForCoordinator(
    SQLite::Database& database,
    std::uint64_t battleId,
    const std::string& capability,
    std::string& error)
{
    const std::optional<BattleCapabilityIdentity> identity =
        authenticateBattleCapability(database, battleId, capability, error);
    if (!identity)
    {
        return std::nullopt;
    }

    int ignoredPlayerNumber = 0;
    return loadBattleData(
        database,
        battleId,
        identity->playerOne,
        ignoredPlayerNumber,
        error);
}

CommandResult appendBattleAction(
    SQLite::Database& database,
    std::uint64_t battleId,
    const std::string& username,
    const conquest_data::BattleAction& action)
{
    if (!validDatabaseId(battleId) || username.empty())
    {
        return {false, "Invalid conquest battle action"};
    }
    if (!supportedBattleActionType(action.actionType))
    {
        return {false, "Unsupported tactical action type"};
    }
    SQLite::Transaction transaction(database, SQLite::TransactionBehavior::IMMEDIATE);
    SQLite::Statement battle(
        database,
        "SELECT status, player_one, player_two FROM conquest_battles WHERE id = ?");
    battle.bind(1, databaseId(battleId));
    if (!battle.executeStep())
    {
        return {false, "Conquest battle not found"};
    }
    if (battle.getColumn(0).getInt() != battleStatusValue(BattleStatus::Ready))
    {
        return {false, "Conquest battle is not accepting actions"};
    }

    const std::string playerOne = battle.getColumn(1).getString();
    const std::string playerTwo = battle.getColumn(2).getString();
    const int expectedPlayer = username == playerOne ? 1 : username == playerTwo ? 2 : 0;
    if (expectedPlayer == 0)
    {
        return {false, "You are not a participant in this battle"};
    }
    if (action.playerNumber != expectedPlayer)
    {
        return {false, "Battle action player does not match the authenticated account"};
    }
    if (action.sequence == 0 || action.sequence > conquest_data::MaxConquestBattleActions)
    {
        return {false, "Conquest battle action limit reached"};
    }

    SQLite::Statement expectedQuery(
        database,
        "SELECT COUNT(*) + 1 FROM conquest_battle_actions WHERE battle_id = ?");
    expectedQuery.bind(1, databaseId(battleId));
    expectedQuery.executeStep();
    const std::uint32_t expected = static_cast<std::uint32_t>(expectedQuery.getColumn(0).getInt64());
    if (action.sequence < expected)
    {
        SQLite::Statement existing(
            database,
            "SELECT player_number, action_type, argument_one, argument_two, argument_three "
            "FROM conquest_battle_actions WHERE battle_id = ? AND sequence = ?");
        existing.bind(1, databaseId(battleId));
        existing.bind(2, static_cast<std::int64_t>(action.sequence));
        if (existing.executeStep() &&
            existing.getColumn(0).getInt() == action.playerNumber &&
            existing.getColumn(1).getInt() == static_cast<int>(action.actionType) &&
            existing.getColumn(2).getInt() == action.argumentOne &&
            existing.getColumn(3).getInt() == action.argumentTwo &&
            existing.getColumn(4).getInt() == action.argumentThree)
        {
            transaction.commit();
            return {true, "Battle action already recorded"};
        }
        return {false, "Battle action sequence is stale"};
    }
    if (action.sequence != expected)
    {
        return {false, "Battle action sequence is out of order"};
    }

    SQLite::Statement insert(
        database,
        "INSERT INTO conquest_battle_actions "
        "(battle_id, sequence, player_number, action_type, argument_one, argument_two, argument_three, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    insert.bind(1, databaseId(battleId));
    insert.bind(2, static_cast<std::int64_t>(action.sequence));
    insert.bind(3, action.playerNumber);
    insert.bind(4, static_cast<int>(action.actionType));
    insert.bind(5, action.argumentOne);
    insert.bind(6, action.argumentTwo);
    insert.bind(7, action.argumentThree);
    insert.bind(8, effectiveNow(0));
    insert.exec();
    transaction.commit();
    return {true, "Battle action recorded"};
}

CommandResult appendBattleActionWithCapability(
    SQLite::Database& database,
    std::uint64_t battleId,
    const std::string& capability,
    const conquest_data::BattleAction& action)
{
    std::string error;
    const std::optional<BattleCapabilityIdentity> identity =
        authenticateBattleCapability(database, battleId, capability, error);
    if (!identity)
    {
        return {false, std::move(error)};
    }

    const std::string& username = action.playerNumber == 1
        ? identity->playerOne
        : action.playerNumber == 2 ? identity->playerTwo : std::string{};
    if (username.empty())
    {
        return {false, "Invalid Conquest battle action player"};
    }
    return appendBattleAction(database, battleId, username, action);
}

CommandResult applyBattleResult(
    SQLite::Database& database,
    std::uint64_t battleId,
    const std::string& username,
    int winnerPlayerNumber,
    std::int64_t now)
{
    if (!validDatabaseId(battleId) || username.empty() ||
        (winnerPlayerNumber != 1 && winnerPlayerNumber != 2))
    {
        return {false, "Invalid conquest battle result"};
    }
    now = effectiveNow(now);
    SQLite::Transaction transaction(database, SQLite::TransactionBehavior::IMMEDIATE);
    SQLite::Statement battle(
        database,
        "SELECT event_id, turn, status, deck_one_id, deck_two_id, player_one, player_two, winner "
        "FROM conquest_battles WHERE id = ?");
    battle.bind(1, databaseId(battleId));
    if (!battle.executeStep())
    {
        return {false, "Conquest battle not found"};
    }

    const std::int64_t eventId = battle.getColumn(0).getInt64();
    const int turn = battle.getColumn(1).getInt();
    const BattleStatus status = static_cast<BattleStatus>(battle.getColumn(2).getInt());
    const std::int64_t deckOneId = battle.getColumn(3).getInt64();
    const std::int64_t deckTwoId = battle.getColumn(4).getInt64();
    const std::string playerOne = battle.getColumn(5).getString();
    const std::string playerTwo = battle.getColumn(6).getString();
    const std::string storedWinner = battle.getColumn(7).getString();
    if (username != playerOne && username != playerTwo)
    {
        return {false, "You are not a participant in this battle"};
    }

    const std::string winner = winnerPlayerNumber == 1 ? playerOne : playerTwo;
    const std::int64_t winnerDeckId = winnerPlayerNumber == 1 ? deckOneId : deckTwoId;
    const std::int64_t loserDeckId = winnerPlayerNumber == 1 ? deckTwoId : deckOneId;
    if (username != winner)
    {
        return {false, "Conquest result identity does not match the reported winner"};
    }
    if (status == BattleStatus::Complete)
    {
        if (storedWinner == winner)
        {
            transaction.commit();
            return {true, "Conquest battle result already applied"};
        }
        return {false, "A different conquest battle result is already stored"};
    }
    if (status != BattleStatus::Ready)
    {
        return {false, "Conquest battle is not ready for a result"};
    }

    SQLite::Statement complete(
        database,
        "UPDATE conquest_battles SET status = ?, winner = ?, winner_deck_id = ?, completed_at = ? "
        "WHERE id = ?");
    complete.bind(1, battleStatusValue(BattleStatus::Complete));
    complete.bind(2, winner);
    complete.bind(3, winnerDeckId);
    complete.bind(4, now);
    complete.bind(5, databaseId(battleId));
    complete.exec();

    SQLite::Statement eliminate(
        database,
        "UPDATE conquest_event_decks SET eliminated = 1, deployed = 0, region_id = 0, "
        "destination_region_id = 0, move_resolved = 1 WHERE id = ?");
    eliminate.bind(1, loserDeckId);
    eliminate.exec();
    eliminatePlayerIfDefeated(database, eventId, winnerPlayerNumber == 1 ? playerTwo : playerOne);

    SQLite::Statement cancel(
        database,
        "UPDATE conquest_battles SET status = ? "
        "WHERE event_id = ? AND turn = ? AND id <> ? AND status IN (?, ?) "
        "AND (deck_one_id = ? OR deck_two_id = ?)");
    cancel.bind(1, battleStatusValue(BattleStatus::Cancelled));
    cancel.bind(2, eventId);
    cancel.bind(3, turn);
    cancel.bind(4, databaseId(battleId));
    cancel.bind(5, battleStatusValue(BattleStatus::Queued));
    cancel.bind(6, battleStatusValue(BattleStatus::Ready));
    cancel.bind(7, loserDeckId);
    cancel.bind(8, loserDeckId);
    cancel.exec();

    const std::optional<EventRow> event = loadEventRow(database, eventId);
    if (!event)
    {
        return {false, "Conquest event for battle is missing"};
    }
    activateQueuedBattles(database, eventId, turn);
    settleDecksWithoutPendingBattles(database, eventId, turn);
    finalizeTurnIfReady(database, *event, now);
    transaction.commit();
    return {true, "Conquest battle result applied"};
}

CommandResult applyBattleResultWithCapability(
    SQLite::Database& database,
    std::uint64_t battleId,
    const std::string& capability,
    int winnerPlayerNumber,
    std::int64_t now)
{
    if (winnerPlayerNumber != 1 && winnerPlayerNumber != 2)
    {
        return {false, "Invalid conquest battle result"};
    }

    std::string error;
    const std::optional<BattleCapabilityIdentity> identity =
        authenticateBattleCapability(database, battleId, capability, error);
    if (!identity)
    {
        return {false, std::move(error)};
    }
    const std::string& winner = winnerPlayerNumber == 1
        ? identity->playerOne
        : identity->playerTwo;
    return applyBattleResult(
        database,
        battleId,
        winner,
        winnerPlayerNumber,
        now);
}

} // namespace account_conquest_events
