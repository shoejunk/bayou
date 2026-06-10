#pragma once

#include <SFML/Network.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace card_data
{
enum class CardType : std::uint8_t
{
    Unit,
    Spell,
    Artifact,
    Reaction
};

struct KeyIntPair
{
    std::string key;
    int value = 0;
};

struct KeyStringPair
{
    std::string key;
    std::string value;
};

struct KeyStringList
{
    std::string key;
    std::vector<std::string> values;
};

struct Card
{
    std::string title;
    CardType type = CardType::Unit;
    std::string imagePath;
    std::vector<std::string> keywords;
    std::vector<KeyIntPair> integerValues;
    std::vector<KeyStringPair> stringValues;
    std::vector<KeyStringList> stringLists;
};

inline std::string toString(CardType type)
{
    switch (type)
    {
        case CardType::Unit: return "Unit";
        case CardType::Spell: return "Spell";
        case CardType::Artifact: return "Artifact";
        case CardType::Reaction: return "Reaction";
    }

    return "Unit";
}

inline std::optional<CardType> cardTypeFromIndex(int index)
{
    switch (index)
    {
        case 1: return CardType::Unit;
        case 2: return CardType::Spell;
        case 3: return CardType::Artifact;
        case 4: return CardType::Reaction;
        default: return std::nullopt;
    }
}

inline CardType cardTypeFromString(const std::string& value)
{
    if (value == "Spell") return CardType::Spell;
    if (value == "Artifact") return CardType::Artifact;
    if (value == "Reaction") return CardType::Reaction;
    return CardType::Unit;
}

inline void writeStringVector(sf::Packet& packet, const std::vector<std::string>& values)
{
    packet << static_cast<std::uint32_t>(values.size());
    for (const std::string& value : values)
    {
        packet << value;
    }
}

inline bool readStringVector(sf::Packet& packet, std::vector<std::string>& values)
{
    std::uint32_t count = 0;
    packet >> count;
    if (!packet)
    {
        return false;
    }

    values.clear();
    values.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        std::string value;
        packet >> value;
        if (!packet)
        {
            return false;
        }
        values.push_back(value);
    }

    return true;
}

inline void writeCard(sf::Packet& packet, const Card& card)
{
    packet << card.title;
    packet << toString(card.type);
    packet << card.imagePath;

    writeStringVector(packet, card.keywords);

    packet << static_cast<std::uint32_t>(card.integerValues.size());
    for (const KeyIntPair& item : card.integerValues)
    {
        packet << item.key << item.value;
    }

    packet << static_cast<std::uint32_t>(card.stringValues.size());
    for (const KeyStringPair& item : card.stringValues)
    {
        packet << item.key << item.value;
    }

    packet << static_cast<std::uint32_t>(card.stringLists.size());
    for (const KeyStringList& item : card.stringLists)
    {
        packet << item.key;
        writeStringVector(packet, item.values);
    }
}

inline bool readCard(sf::Packet& packet, Card& card)
{
    std::string typeName;
    packet >> card.title >> typeName >> card.imagePath;
    if (!packet)
    {
        return false;
    }
    card.type = cardTypeFromString(typeName);

    if (!readStringVector(packet, card.keywords))
    {
        return false;
    }

    std::uint32_t integerCount = 0;
    packet >> integerCount;
    card.integerValues.clear();
    card.integerValues.reserve(integerCount);
    for (std::uint32_t i = 0; i < integerCount; ++i)
    {
        KeyIntPair item;
        packet >> item.key >> item.value;
        if (!packet)
        {
            return false;
        }
        card.integerValues.push_back(item);
    }

    std::uint32_t stringCount = 0;
    packet >> stringCount;
    card.stringValues.clear();
    card.stringValues.reserve(stringCount);
    for (std::uint32_t i = 0; i < stringCount; ++i)
    {
        KeyStringPair item;
        packet >> item.key >> item.value;
        if (!packet)
        {
            return false;
        }
        card.stringValues.push_back(item);
    }

    std::uint32_t listCount = 0;
    packet >> listCount;
    card.stringLists.clear();
    card.stringLists.reserve(listCount);
    for (std::uint32_t i = 0; i < listCount; ++i)
    {
        KeyStringList item;
        packet >> item.key;
        if (!packet || !readStringVector(packet, item.values))
        {
            return false;
        }
        card.stringLists.push_back(item);
    }

    return true;
}
}
