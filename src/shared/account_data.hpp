#pragma once

#include <SFML/Network.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace account_data
{
struct CollectionCard
{
    std::string title;
    int copies = 0;
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
    if (!packet)
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
}
