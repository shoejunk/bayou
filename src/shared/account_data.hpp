#pragma once

#include <SFML/Network.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "ranking.hpp"

namespace account_data
{
// Upper bound on a serialized collection's entry count, checked before
// reserving so a crafted packet cannot trigger a huge allocation.
constexpr std::uint32_t MaxSerializedCollectionCards = 65536;

struct CollectionCard
{
    std::string title;
    int copies = 0;
};

struct AccountState
{
    int coins = 0;
    int rating = 0;
    ranking::League league = ranking::League::Wood;
    bool isAdmin = false;
    std::vector<CollectionCard> collection;
};

inline void writeCollection(sf::Packet& packet, const std::vector<CollectionCard>& collection)
{
    packet << static_cast<std::uint32_t>(collection.size());
    for (const CollectionCard& card : collection)
    {
        packet << card.title << card.copies;
    }
}

inline bool readCollection(sf::Packet& packet, std::vector<CollectionCard>& collection)
{
    std::uint32_t count = 0;
    packet >> count;
    if (!packet || count > MaxSerializedCollectionCards)
    {
        return false;
    }

    collection.clear();
    collection.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        CollectionCard card;
        packet >> card.title >> card.copies;
        if (!packet)
        {
            return false;
        }
        collection.push_back(card);
    }

    return true;
}

inline void writeAccountState(sf::Packet& packet, const AccountState& state)
{
    packet << state.coins << state.rating
           << static_cast<std::uint8_t>(state.league) << state.isAdmin;
    writeCollection(packet, state.collection);
}

inline bool readAccountState(sf::Packet& packet, AccountState& state)
{
    std::uint8_t league = 0;
    packet >> state.coins >> state.rating >> league >> state.isAdmin;
    if (!packet || !ranking::isValidLeague(league))
    {
        return false;
    }
    state.league = static_cast<ranking::League>(league);

    return readCollection(packet, state.collection);
}
}
