#pragma once

#include "deck_data.hpp"

#include <SFML/Network.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace conquest_data
{
constexpr std::uint32_t MaxSerializedConquestDecks = 1024;
constexpr std::uint32_t MaxConquestArmyDecks = 10;
constexpr std::size_t MaxConquestDeckNameBytes = 64;

struct ConquestDeck
{
    // id == 0 and revision == 0 identify a deck that has not been saved yet.
    std::int64_t id = 0;
    std::uint32_t revision = 0;
    deck_data::Deck deck;
};

struct ConquestArmy
{
    // revision == 0 with no deck ids represents an account with no saved army.
    std::uint32_t revision = 0;
    std::vector<std::int64_t> deckIds;
};

inline bool writeConquestDeck(sf::Packet& packet, const ConquestDeck& conquestDeck)
{
    if (conquestDeck.deck.name.size() > MaxConquestDeckNameBytes ||
        conquestDeck.deck.cardTitles.size() > deck_data::MaxSerializedDeckCards)
    {
        return false;
    }

    packet << conquestDeck.id << conquestDeck.revision;
    deck_data::writeDeck(packet, conquestDeck.deck);
    return true;
}

inline bool readConquestDeck(sf::Packet& packet, ConquestDeck& conquestDeck)
{
    packet >> conquestDeck.id >> conquestDeck.revision;
    if (!packet || !deck_data::readDeck(packet, conquestDeck.deck))
    {
        return false;
    }

    return conquestDeck.deck.name.size() <= MaxConquestDeckNameBytes;
}

inline bool writeConquestDeckList(
    sf::Packet& packet,
    const std::vector<ConquestDeck>& decks)
{
    if (decks.size() > MaxSerializedConquestDecks)
    {
        return false;
    }

    for (const ConquestDeck& deck : decks)
    {
        if (deck.deck.name.size() > MaxConquestDeckNameBytes ||
            deck.deck.cardTitles.size() > deck_data::MaxSerializedDeckCards)
        {
            return false;
        }
    }

    packet << static_cast<std::uint32_t>(decks.size());
    for (const ConquestDeck& deck : decks)
    {
        [[maybe_unused]] const bool wroteDeck = writeConquestDeck(packet, deck);
    }
    return true;
}

inline bool readConquestDeckList(
    sf::Packet& packet,
    std::vector<ConquestDeck>& decks)
{
    std::uint32_t count = 0;
    packet >> count;
    if (!packet || count > MaxSerializedConquestDecks)
    {
        return false;
    }

    decks.clear();
    decks.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        ConquestDeck deck;
        if (!readConquestDeck(packet, deck))
        {
            return false;
        }
        decks.push_back(std::move(deck));
    }
    return true;
}

inline bool writeConquestArmy(sf::Packet& packet, const ConquestArmy& army)
{
    if (army.deckIds.size() > MaxConquestArmyDecks)
    {
        return false;
    }

    packet << army.revision;
    packet << static_cast<std::uint32_t>(army.deckIds.size());
    for (const std::int64_t deckId : army.deckIds)
    {
        packet << deckId;
    }
    return true;
}

inline bool readConquestArmy(sf::Packet& packet, ConquestArmy& army)
{
    std::uint32_t count = 0;
    packet >> army.revision >> count;
    if (!packet || count > MaxConquestArmyDecks)
    {
        return false;
    }

    army.deckIds.clear();
    army.deckIds.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        std::int64_t deckId = 0;
        packet >> deckId;
        if (!packet)
        {
            return false;
        }
        army.deckIds.push_back(deckId);
    }

    return true;
}
} // namespace conquest_data
