#pragma once

#include <SFML/Network.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace card_data
{
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

struct Action
{
    std::string name;
    int state = 0;
    std::string kind = "slide";
    std::string pattern = "omni";
    int minRange = 1;
    int maxRange = 1;
    int damage = 0;
    bool canMove = true;
    bool canAttack = false;
    bool passThrough = false;
    bool lineOfSight = false;
    int statusTurns = 0;
    int cooldownTurns = 0;
};

struct Card
{
    std::string title;
    std::string type = "Unit";
    std::string imagePath;
    std::vector<std::string> keywords;
    std::vector<KeyIntPair> integerValues;
    std::vector<KeyStringPair> stringValues;
    std::vector<KeyStringList> stringLists;
    std::vector<std::string> actionNames;
    std::vector<Action> actions;
};

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

inline void writeAction(sf::Packet& packet, const Action& action)
{
    packet << action.name << action.state << action.kind << action.pattern
           << action.minRange << action.maxRange << action.damage
           << action.canMove << action.canAttack << action.passThrough << action.lineOfSight
           << action.statusTurns << action.cooldownTurns;
}

inline bool readAction(sf::Packet& packet, Action& action)
{
    packet >> action.name >> action.state >> action.kind >> action.pattern
           >> action.minRange >> action.maxRange >> action.damage
           >> action.canMove >> action.canAttack >> action.passThrough >> action.lineOfSight
           >> action.statusTurns >> action.cooldownTurns;
    return static_cast<bool>(packet);
}

inline void writeCard(sf::Packet& packet, const Card& card)
{
    packet << card.title;
    packet << card.type;
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

    writeStringVector(packet, card.actionNames);
    packet << static_cast<std::uint32_t>(card.actions.size());
    for (const Action& action : card.actions)
    {
        writeAction(packet, action);
    }
}

inline bool readCard(sf::Packet& packet, Card& card)
{
    packet >> card.title >> card.type >> card.imagePath;
    if (!packet)
    {
        return false;
    }

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

    if (!readStringVector(packet, card.actionNames))
    {
        return false;
    }

    std::uint32_t actionCount = 0;
    packet >> actionCount;
    card.actions.clear();
    card.actions.reserve(actionCount);
    for (std::uint32_t i = 0; i < actionCount; ++i)
    {
        Action action;
        if (!readAction(packet, action))
        {
            return false;
        }
        card.actions.push_back(action);
    }

    return true;
}
}
