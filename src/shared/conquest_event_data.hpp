#pragma once

#include <SFML/Network.hpp>

#include "card_data.hpp"
#include "deck_data.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace conquest_data
{

inline constexpr std::uint32_t MaxConquestEvents = 64;
inline constexpr std::uint32_t MaxConquestPlayers = 12;
inline constexpr std::uint32_t MaxConquestRegions = 20;
inline constexpr std::uint32_t MaxConquestEventDecks = 120;
inline constexpr std::uint32_t MaxConquestBattles = 256;
inline constexpr std::uint32_t MaxConquestOrders = 10;
inline constexpr std::uint32_t MaxConquestBattleActions = 20000;
inline constexpr std::uint32_t MaxConquestBattleCards = 64;
inline constexpr std::uint32_t MaxConquestCatalogCards = 2048;
inline constexpr std::size_t MaxConquestTextLength = 128;
inline constexpr std::size_t ConquestBattleCapabilityHexLength = 64;

enum class EventPhase : std::uint8_t
{
    Registration,
    Planning,
    Resolving,
    Complete
};

enum class BattleKind : std::uint8_t
{
    Region,
    Crossing
};

enum class BattleStatus : std::uint8_t
{
    Queued,
    Ready,
    Complete,
    Cancelled
};

struct EventSummary
{
    std::uint64_t id = 0;
    std::string name;
    std::string mapId;
    EventPhase phase = EventPhase::Registration;
    int turn = 0;
    std::int64_t registrationEndsAt = 0;
    std::int64_t turnEndsAt = 0;
    std::uint32_t participantCount = 0;
    bool joined = false;
};

struct PlayerState
{
    std::string username;
    std::uint8_t colorIndex = 0;
    int controlledRegions = 0;
    bool ordersSubmitted = false;
    int reinforcementsAvailable = 0;
    std::int64_t nextReinforcementAt = 0;
};

struct RegionState
{
    int regionId = 0;
    std::string controller;
};

struct EventDeckState
{
    std::uint64_t id = 0;
    std::uint64_t sourceDeckId = 0;
    std::string owner;
    std::string deckName;
    int armySlot = 0;
    bool deployed = false;
    bool eliminated = false;
    int regionId = 0;
    int destinationRegionId = 0;
};

struct BattleState
{
    std::uint64_t id = 0;
    BattleKind kind = BattleKind::Region;
    BattleStatus status = BattleStatus::Queued;
    int regionId = 0;
    std::uint64_t deckOneId = 0;
    std::uint64_t deckTwoId = 0;
    std::string playerOne;
    std::string playerTwo;
    std::string deckOneName;
    std::string deckTwoName;
    std::string winner;
    bool canJoin = false;
};

struct EventState
{
    EventSummary summary;
    std::vector<PlayerState> players;
    std::vector<RegionState> regions;
    std::vector<EventDeckState> decks;
    std::vector<BattleState> battles;
};

struct StartingPlacement
{
    std::uint64_t deckId = 0;
    int regionId = 0;
};

struct MoveOrder
{
    std::uint64_t eventDeckId = 0;
    int destinationRegionId = 0;
};

// The durable action representation is intentionally limited to the tactical
// protocol's current three integer operands. It is sufficient to replay every
// action accepted by GameEngine while avoiding persistence of client-supplied
// card data.
struct BattleAction
{
    std::uint32_t sequence = 0;
    int playerNumber = 0;
    std::uint8_t actionType = 0;
    int argumentOne = 0;
    int argumentTwo = 0;
    int argumentThree = 0;
};

struct BattleData
{
    std::uint64_t battleId = 0;
    std::uint32_t seed = 0;
    std::string playerOne;
    std::string playerTwo;
    deck_data::Deck deckOne;
    deck_data::Deck deckTwo;
    // Frozen definitions make deterministic replay independent of later card
    // balance changes or catalog removal. The title-only decks remain useful
    // for display and for validating snapshot ordering.
    std::vector<card_data::Card> cardsOne;
    std::vector<card_data::Card> cardsTwo;
    std::vector<card_data::Card> catalog;
    std::vector<BattleAction> actions;
};

inline bool validText(const std::string& text)
{
    return text.size() <= MaxConquestTextLength;
}

inline void writeEventSummary(sf::Packet& packet, const EventSummary& value)
{
    packet << value.id << value.name << value.mapId
           << static_cast<std::uint8_t>(value.phase) << value.turn
           << value.registrationEndsAt << value.turnEndsAt
           << value.participantCount << value.joined;
}

inline bool readEventSummary(sf::Packet& packet, EventSummary& value)
{
    std::uint8_t phase = 0;
    packet >> value.id >> value.name >> value.mapId >> phase >> value.turn
           >> value.registrationEndsAt >> value.turnEndsAt
           >> value.participantCount >> value.joined;
    if (!packet || phase > static_cast<std::uint8_t>(EventPhase::Complete) ||
        !validText(value.name) || !validText(value.mapId))
    {
        return false;
    }
    value.phase = static_cast<EventPhase>(phase);
    return true;
}

inline void writePlayerState(sf::Packet& packet, const PlayerState& value)
{
    packet << value.username << value.colorIndex << value.controlledRegions
           << value.ordersSubmitted << value.reinforcementsAvailable
           << value.nextReinforcementAt;
}

inline bool readPlayerState(sf::Packet& packet, PlayerState& value)
{
    packet >> value.username >> value.colorIndex >> value.controlledRegions
           >> value.ordersSubmitted >> value.reinforcementsAvailable
           >> value.nextReinforcementAt;
    return packet && validText(value.username) && value.colorIndex < MaxConquestPlayers;
}

inline void writeRegionState(sf::Packet& packet, const RegionState& value)
{
    packet << value.regionId << value.controller;
}

inline bool readRegionState(sf::Packet& packet, RegionState& value)
{
    packet >> value.regionId >> value.controller;
    return packet && value.regionId >= 1 && value.regionId <= 20 && validText(value.controller);
}

inline void writeEventDeckState(sf::Packet& packet, const EventDeckState& value)
{
    packet << value.id << value.sourceDeckId << value.owner << value.deckName
           << value.armySlot << value.deployed << value.eliminated
           << value.regionId << value.destinationRegionId;
}

inline bool readEventDeckState(sf::Packet& packet, EventDeckState& value)
{
    packet >> value.id >> value.sourceDeckId >> value.owner >> value.deckName
           >> value.armySlot >> value.deployed >> value.eliminated
           >> value.regionId >> value.destinationRegionId;
    return packet && validText(value.owner) && validText(value.deckName) &&
        value.armySlot >= 0 && value.armySlot < 10 &&
        value.regionId >= 0 && value.regionId <= 20 &&
        value.destinationRegionId >= 0 && value.destinationRegionId <= 20;
}

inline void writeBattleState(sf::Packet& packet, const BattleState& value)
{
    packet << value.id << static_cast<std::uint8_t>(value.kind)
           << static_cast<std::uint8_t>(value.status) << value.regionId
           << value.deckOneId << value.deckTwoId
           << value.playerOne << value.playerTwo
           << value.deckOneName << value.deckTwoName
           << value.winner << value.canJoin;
}

inline bool readBattleState(sf::Packet& packet, BattleState& value)
{
    std::uint8_t kind = 0;
    std::uint8_t status = 0;
    packet >> value.id >> kind >> status >> value.regionId
           >> value.deckOneId >> value.deckTwoId
           >> value.playerOne >> value.playerTwo
           >> value.deckOneName >> value.deckTwoName
           >> value.winner >> value.canJoin;
    if (!packet || kind > static_cast<std::uint8_t>(BattleKind::Crossing) ||
        status > static_cast<std::uint8_t>(BattleStatus::Cancelled) ||
        !validText(value.playerOne) || !validText(value.playerTwo) ||
        !validText(value.deckOneName) || !validText(value.deckTwoName) ||
        !validText(value.winner))
    {
        return false;
    }
    value.kind = static_cast<BattleKind>(kind);
    value.status = static_cast<BattleStatus>(status);
    return true;
}

inline void writeEventState(sf::Packet& packet, const EventState& value)
{
    writeEventSummary(packet, value.summary);
    packet << static_cast<std::uint32_t>(value.players.size());
    for (const PlayerState& player : value.players)
    {
        writePlayerState(packet, player);
    }
    packet << static_cast<std::uint32_t>(value.regions.size());
    for (const RegionState& region : value.regions)
    {
        writeRegionState(packet, region);
    }
    packet << static_cast<std::uint32_t>(value.decks.size());
    for (const EventDeckState& deck : value.decks)
    {
        writeEventDeckState(packet, deck);
    }
    packet << static_cast<std::uint32_t>(value.battles.size());
    for (const BattleState& battle : value.battles)
    {
        writeBattleState(packet, battle);
    }
}

inline std::uint64_t eventStateFingerprint(const EventState& value)
{
    sf::Packet packet;
    writeEventState(packet, value);
    const auto* bytes = static_cast<const std::uint8_t*>(packet.getData());
    std::uint64_t hash = 14695981039346656037ull;
    for (std::size_t index = 0; index < packet.getDataSize(); ++index)
    {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

inline bool readEventState(sf::Packet& packet, EventState& value)
{
    if (!readEventSummary(packet, value.summary))
    {
        return false;
    }

    std::uint32_t count = 0;
    packet >> count;
    if (!packet || count > MaxConquestPlayers)
    {
        return false;
    }
    value.players.clear();
    value.players.resize(count);
    for (PlayerState& player : value.players)
    {
        if (!readPlayerState(packet, player))
        {
            return false;
        }
    }

    packet >> count;
    if (!packet || count > MaxConquestRegions)
    {
        return false;
    }
    value.regions.clear();
    value.regions.resize(count);
    for (RegionState& region : value.regions)
    {
        if (!readRegionState(packet, region))
        {
            return false;
        }
    }

    packet >> count;
    if (!packet || count > MaxConquestEventDecks)
    {
        return false;
    }
    value.decks.clear();
    value.decks.resize(count);
    for (EventDeckState& deck : value.decks)
    {
        if (!readEventDeckState(packet, deck))
        {
            return false;
        }
    }

    packet >> count;
    if (!packet || count > MaxConquestBattles)
    {
        return false;
    }
    value.battles.clear();
    value.battles.resize(count);
    for (BattleState& battle : value.battles)
    {
        if (!readBattleState(packet, battle))
        {
            return false;
        }
    }
    return true;
}

inline void writeStartingPlacement(sf::Packet& packet, const StartingPlacement& value)
{
    packet << value.deckId << value.regionId;
}

inline bool readStartingPlacement(sf::Packet& packet, StartingPlacement& value)
{
    packet >> value.deckId >> value.regionId;
    return packet && value.deckId != 0 && value.regionId >= 1 && value.regionId <= 20;
}

inline void writeMoveOrder(sf::Packet& packet, const MoveOrder& value)
{
    packet << value.eventDeckId << value.destinationRegionId;
}

inline bool readMoveOrder(sf::Packet& packet, MoveOrder& value)
{
    packet >> value.eventDeckId >> value.destinationRegionId;
    return packet && value.eventDeckId != 0 &&
        value.destinationRegionId >= 1 && value.destinationRegionId <= 20;
}

inline void writeBattleAction(sf::Packet& packet, const BattleAction& value)
{
    packet << value.sequence << value.playerNumber << value.actionType
           << value.argumentOne << value.argumentTwo << value.argumentThree;
}

inline bool readBattleAction(sf::Packet& packet, BattleAction& value)
{
    packet >> value.sequence >> value.playerNumber >> value.actionType
           >> value.argumentOne >> value.argumentTwo >> value.argumentThree;
    return packet && (value.playerNumber == 1 || value.playerNumber == 2);
}

inline void writeBattleData(sf::Packet& packet, const BattleData& value)
{
    packet << value.battleId << value.seed << value.playerOne << value.playerTwo;
    deck_data::writeDeck(packet, value.deckOne);
    deck_data::writeDeck(packet, value.deckTwo);
    packet << static_cast<std::uint32_t>(value.cardsOne.size());
    for (const card_data::Card& card : value.cardsOne)
    {
        card_data::writeCard(packet, card);
    }
    packet << static_cast<std::uint32_t>(value.cardsTwo.size());
    for (const card_data::Card& card : value.cardsTwo)
    {
        card_data::writeCard(packet, card);
    }
    packet << static_cast<std::uint32_t>(value.catalog.size());
    for (const card_data::Card& card : value.catalog)
    {
        card_data::writeCard(packet, card);
    }
    packet << static_cast<std::uint32_t>(value.actions.size());
    for (const BattleAction& action : value.actions)
    {
        writeBattleAction(packet, action);
    }
}

inline bool readBattleData(sf::Packet& packet, BattleData& value)
{
    auto readCards = [&](std::vector<card_data::Card>& cards, std::uint32_t maximum) {
        std::uint32_t count = 0;
        packet >> count;
        if (!packet || count > maximum)
        {
            return false;
        }
        cards.clear();
        cards.resize(count);
        for (card_data::Card& card : cards)
        {
            if (!card_data::readCard(packet, card))
            {
                return false;
            }
        }
        return true;
    };

    std::uint32_t actionCount = 0;
    packet >> value.battleId >> value.seed >> value.playerOne >> value.playerTwo;
    if (!packet || !validText(value.playerOne) || !validText(value.playerTwo) ||
        !deck_data::readDeck(packet, value.deckOne) ||
        !deck_data::readDeck(packet, value.deckTwo) ||
        !readCards(value.cardsOne, MaxConquestBattleCards) ||
        !readCards(value.cardsTwo, MaxConquestBattleCards) ||
        !readCards(value.catalog, MaxConquestCatalogCards))
    {
        return false;
    }
    packet >> actionCount;
    if (!packet || actionCount > MaxConquestBattleActions)
    {
        return false;
    }
    value.actions.clear();
    value.actions.resize(actionCount);
    for (BattleAction& action : value.actions)
    {
        if (!readBattleAction(packet, action))
        {
            return false;
        }
    }
    return true;
}

} // namespace conquest_data
