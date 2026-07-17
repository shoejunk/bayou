#pragma once

#include <SFML/Network.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace card_data
{
// Upper bound on any serialized list count, checked before reserving so a
// crafted packet cannot trigger a huge allocation.
constexpr std::uint32_t MaxSerializedItems = 4096;
constexpr std::uint32_t CardListSchemaMarker = 0xffffffffu;
constexpr std::uint32_t CardListSchemaVersion = 7;
constexpr int DefaultNextState = (-2147483647 - 1);

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
    int heal = 0;
    bool canMove = true;
    bool canAttack = false;
    bool passThrough = false;
    bool lineOfSight = false;
    int statusTurns = 0;
    int cooldownTurns = 0;
    int push = 0;
    std::vector<std::string> targetFilter;
    int nextState = DefaultNextState;
};

inline int actionNextState(const Action& action)
{
    return action.nextState == DefaultNextState ? action.state : action.nextState;
}

struct Card
{
    std::string title;
    std::string type = "Unit";
    std::string imagePath;
    std::vector<std::string> traits;
    std::vector<std::string> keywords;
    std::vector<KeyIntPair> integerValues;
    std::vector<KeyStringPair> stringValues;
    std::vector<KeyStringList> stringLists;
    std::vector<std::string> actionNames;
    std::vector<std::string> actionDisplayNames;
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
    if (!packet || count > MaxSerializedItems)
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
    packet << action.name << action.state << actionNextState(action) << action.kind << action.pattern
           << action.minRange << action.maxRange << action.damage << action.heal
           << action.canMove << action.canAttack << action.passThrough << action.lineOfSight
           << action.statusTurns << action.cooldownTurns << action.push;
    writeStringVector(packet, action.targetFilter);
}

inline bool readAction(
    sf::Packet& packet,
    Action& action,
    bool includesHeal = true,
    bool includesPush = true,
    bool includesTargetFilter = true,
    bool includesNextState = true)
{
    packet >> action.name >> action.state;
    if (includesNextState)
    {
        packet >> action.nextState;
    }
    else
    {
        action.nextState = action.state;
    }
    packet >> action.kind >> action.pattern
           >> action.minRange >> action.maxRange >> action.damage;
    if (includesHeal)
    {
        packet >> action.heal;
    }
    else if (action.damage < 0)
    {
        action.heal = -action.damage;
        action.damage = 0;
    }
    packet >> action.canMove >> action.canAttack >> action.passThrough >> action.lineOfSight
           >> action.statusTurns >> action.cooldownTurns;
    if (includesPush)
    {
        packet >> action.push;
    }
    if (!packet)
    {
        return false;
    }
    if (includesTargetFilter)
    {
        return readStringVector(packet, action.targetFilter);
    }
    action.targetFilter.clear();
    return true;
}

inline void writeCardListHeader(sf::Packet& packet, std::uint32_t count)
{
    packet << CardListSchemaMarker << CardListSchemaVersion << count;
}

inline bool readCardListHeader(
    sf::Packet& packet,
    std::uint32_t& count,
    bool& legacyFormat,
    bool* actionIncludesNextState = nullptr)
{
    std::uint32_t markerOrCount = 0;
    packet >> markerOrCount;
    if (!packet)
    {
        return false;
    }

    if (markerOrCount == CardListSchemaMarker)
    {
        std::uint32_t version = 0;
        packet >> version >> count;
        legacyFormat = false;
        if (actionIncludesNextState != nullptr)
        {
            *actionIncludesNextState = version >= CardListSchemaVersion;
        }
        return static_cast<bool>(packet) &&
            (version == 6 || version == CardListSchemaVersion) &&
            count <= MaxSerializedItems;
    }

    count = markerOrCount;
    legacyFormat = true;
    if (actionIncludesNextState != nullptr)
    {
        *actionIncludesNextState = false;
    }
    return count <= MaxSerializedItems;
}

inline void writeCard(sf::Packet& packet, const Card& card)
{
    packet << card.title;
    packet << card.type;
    packet << card.imagePath;

    writeStringVector(packet, card.traits);
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
    writeStringVector(packet, card.actionDisplayNames);
    packet << static_cast<std::uint32_t>(card.actions.size());
    for (const Action& action : card.actions)
    {
        writeAction(packet, action);
    }
}

inline bool readCardRemaining(
    sf::Packet& packet,
    Card& card,
    bool actionIncludesHeal = true,
    bool actionIncludesPush = true,
    bool actionIncludesTargetFilter = true,
    bool includesActionDisplayNames = true,
    bool actionIncludesNextState = true)
{
    std::uint32_t integerCount = 0;
    packet >> integerCount;
    if (!packet || integerCount > MaxSerializedItems)
    {
        return false;
    }
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
    if (!packet || stringCount > MaxSerializedItems)
    {
        return false;
    }
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
    if (!packet || listCount > MaxSerializedItems)
    {
        return false;
    }
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
    if (includesActionDisplayNames)
    {
        if (!readStringVector(packet, card.actionDisplayNames))
        {
            return false;
        }
    }
    else
    {
        card.actionDisplayNames = card.actionNames;
    }

    std::uint32_t actionCount = 0;
    packet >> actionCount;
    if (!packet || actionCount > MaxSerializedItems)
    {
        return false;
    }
    card.actions.clear();
    card.actions.reserve(actionCount);
    for (std::uint32_t i = 0; i < actionCount; ++i)
    {
        Action action;
        if (!readAction(
                packet,
                action,
                actionIncludesHeal,
                actionIncludesPush,
                actionIncludesTargetFilter,
                actionIncludesNextState))
        {
            return false;
        }
        card.actions.push_back(action);
    }

    return true;
}

inline bool readCard(sf::Packet& packet, Card& card, bool actionIncludesNextState = true)
{
    packet >> card.title >> card.type >> card.imagePath;
    if (!packet || !readStringVector(packet, card.traits) || !readStringVector(packet, card.keywords))
    {
        return false;
    }
    return readCardRemaining(packet, card, true, true, true, true, actionIncludesNextState);
}

inline bool readLegacyCard(sf::Packet& packet, Card& card)
{
    packet >> card.title >> card.type >> card.imagePath;
    if (!packet || !readStringVector(packet, card.traits))
    {
        return false;
    }
    card.keywords.clear();
    return readCardRemaining(packet, card, false, false, false, false, false);
}

inline bool readListedCard(
    sf::Packet& packet,
    Card& card,
    bool legacyFormat,
    bool actionIncludesNextState = true)
{
    return legacyFormat ? readLegacyCard(packet, card) : readCard(packet, card, actionIncludesNextState);
}
}
