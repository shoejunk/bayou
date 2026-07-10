#pragma once

#include <SFML/Network.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace deck_data
{
// Upper bound on a serialized deck's card count, checked before reserving so a
// crafted packet cannot trigger a huge allocation.
constexpr std::uint32_t MaxSerializedDeckCards = 256;

struct Deck
{
    std::string name;
    std::vector<std::string> cardTitles;
};

inline void writeDeck(sf::Packet& packet, const Deck& deck)
{
    packet << deck.name;
    packet << static_cast<std::uint32_t>(deck.cardTitles.size());
    for (const std::string& cardTitle : deck.cardTitles)
    {
        packet << cardTitle;
    }
}

inline bool readDeck(sf::Packet& packet, Deck& deck)
{
    std::uint32_t cardCount = 0;
    packet >> deck.name >> cardCount;
    if (!packet || cardCount > MaxSerializedDeckCards)
    {
        return false;
    }

    deck.cardTitles.clear();
    deck.cardTitles.reserve(cardCount);
    for (std::uint32_t i = 0; i < cardCount; ++i)
    {
        std::string cardTitle;
        packet >> cardTitle;
        if (!packet)
        {
            return false;
        }
        deck.cardTitles.push_back(cardTitle);
    }

    return true;
}
}
