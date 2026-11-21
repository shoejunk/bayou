#pragma once

#include <SFML/Network.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <cctype>
#include "activated_ability_data.hpp"

namespace card_data
{
// Upper bound on any serialized list count, checked before reserving so a
// crafted packet cannot trigger a huge allocation.
constexpr std::uint32_t MaxSerializedItems = 4096;
constexpr std::uint32_t CardListSchemaMarker = 0xffffffffu;
constexpr std::uint32_t CardListSchemaVersion = 15;
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

// Reserved action metadata in the versioned targetFilter vector. Legacy readers
// retain the field and treat it as an unmet trait/keyword requirement, rather
// than executing the action as ordinary healing. Never expose it as a trait.
inline constexpr char RootedActionFlag[] = "@rooted";
inline bool actionAppliesRooted(const std::vector<std::string>& filter)
{
    return std::find(filter.begin(), filter.end(), RootedActionFlag) != filter.end();
}

inline constexpr char PurifyActionFlag[] = "@purify";
inline bool actionHasPurify(const std::vector<std::string>& filter)
{
    return std::find(filter.begin(), filter.end(), PurifyActionFlag) != filter.end();
}

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
    int control = 0;
    int repeat = 0;
    std::vector<std::string> targetFilter;
    std::string infest;
    int nextState = DefaultNextState;
    bool pull = false;
    bool lifedrain = false;
    int slow = 0;
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
    std::vector<std::string> passives;
    std::vector<KeyIntPair> integerValues;
    std::vector<KeyStringPair> stringValues;
    std::vector<KeyStringList> stringLists;
    std::vector<std::string> actionNames;
    std::vector<std::string> actionDisplayNames;
    std::vector<Action> actions;
    bool abilityReferencesExplicit = false; // native catalog controls both assignments, including none
    std::string activatedAbilityName;
    std::string sacrificeAbilityName;
    std::optional<ability_data::Definition> activatedAbility;
    std::optional<ability_data::Definition> sacrificeAbility;
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

inline void writeAction(sf::Packet& packet, const Action& action, bool includesSlow = true)
{
    packet << action.name << action.state << actionNextState(action) << action.kind << action.pattern
           << action.minRange << action.maxRange << action.damage << action.heal
           << action.canMove << action.canAttack << action.passThrough << action.lineOfSight
           << action.statusTurns << action.cooldownTurns << action.push << action.control << action.repeat;
    writeStringVector(packet, action.targetFilter);
    packet << action.infest << action.pull << action.lifedrain;
    if (includesSlow) packet << action.slow;
}

inline bool readAction(
    sf::Packet& packet,
    Action& action,
    bool includesHeal = true,
    bool includesPush = true,
    bool includesTargetFilter = true,
    bool includesNextState = true,
    bool includesControl = true,
    bool includesRepeat = true,
    bool includesInfest = true,
    bool includesPull = true,
    bool includesLifedrain = true,
    bool includesSlow = true)
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
    if (includesControl)
    {
        packet >> action.control;
    }
    else
    {
        action.control = 0;
    }
    if (includesRepeat)
    {
        packet >> action.repeat;
    }
    else
    {
        action.repeat = 0;
    }
    if (action.repeat < 0)
    {
        action.repeat = 0;
    }
    if (includesTargetFilter)
    {
        if (!readStringVector(packet, action.targetFilter))
        {
            return false;
        }
    }
    else
    {
        action.targetFilter.clear();
    }
    if (includesInfest)
    {
        packet >> action.infest;
    }
    else
    {
        action.infest.clear();
    }
    if (includesPull)
    {
        packet >> action.pull;
    }
    else
    {
        action.pull = false;
    }
    action.lifedrain = false;
    if (includesLifedrain)
    {
        packet >> action.lifedrain;
    }
    action.slow = 0;
    if (includesSlow) packet >> action.slow;
    return static_cast<bool>(packet);
}

// Unversioned action lists and single-action edit requests predate Slow.
inline bool readCatalogAction(sf::Packet& packet, Action& action, bool includesSlow)
{
    return readAction(packet, action, true, true, true, true, true, true, true, true, true, includesSlow);
}

inline std::string passiveKey(std::string_view name)
{
    std::string result(name);
    for (char& ch : result) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    // Read-only compatibility with the unreleased initial Rescue spelling.
    if (result == "dash's rescue") return "rescue";
    return result;
}

inline void normalizeRescuePassives(std::vector<std::string>& passives)
{
    bool foundRescue = false;
    std::erase_if(passives, [&](const std::string& passive) {
        if (passiveKey(passive) != "rescue") return false;
        if (foundRescue) return true;
        foundRescue = true;
        return false;
    });
    for (auto& passive : passives)
        if (passiveKey(passive) == "rescue") passive = "Rescue";
}

inline bool isSupportedPassive(std::string_view name)
{
    const auto key = passiveKey(name);
    for (const auto* supported : {"bind", "bodyguard", "bristle", "chrysalis", "evacuate",
         "foresight", "intercept", "relentless", "reveal", "shelter", "swarm", "trail", "undying",
         "rot aura", "dematerialize", "fly", "flying", "rescue"})
        if (key == supported) return true;
    return false;
}

inline void splitLegacyPassives(std::vector<std::string>& keywords, std::vector<std::string>& passives)
{
    std::erase_if(keywords, [&](const std::string& value) {
        if (!isSupportedPassive(value)) return false;
        passives.push_back(value);
        return true;
    });
    normalizeRescuePassives(passives);
}

inline void splitLegacyPassives(Card& card)
{
    splitLegacyPassives(card.keywords, card.passives);
}

inline std::vector<std::string> legacyPassiveKeywords(
    const std::vector<std::string>& keywords, const std::vector<std::string>& passives)
{
    auto result = keywords;
    std::erase_if(result, [](const std::string& value) { return isSupportedPassive(value); });
    result.insert(result.end(), passives.begin(), passives.end());
    // Legacy SQLite catalog reads were ordered by exact text.
    std::stable_sort(result.begin(), result.end());
    return result;
}

inline Card legacyPassiveProjection(Card card)
{
    card.keywords = legacyPassiveKeywords(card.keywords, card.passives);
    card.passives.clear();
    return card;
}

inline void writeCardListHeader(sf::Packet& packet, std::uint32_t count, bool includesSlow = true, bool includesAbilities = false, bool includesPassives = false)
{
    packet << CardListSchemaMarker << (includesPassives ? CardListSchemaVersion : includesAbilities ? 14u : includesSlow ? 13u : 12u) << count;
}

inline bool readCardListHeader(
    sf::Packet& packet,
    std::uint32_t& count,
    bool& legacyFormat,
    bool* actionIncludesNextState = nullptr,
    bool* actionIncludesControl = nullptr,
    bool* actionIncludesRepeat = nullptr,
    bool* actionIncludesInfest = nullptr,
    bool* actionIncludesPull = nullptr,
    bool* actionIncludesLifedrain = nullptr,
    bool* actionIncludesSlow = nullptr, bool* includesAbilities = nullptr, bool* includesPassives = nullptr)
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
            *actionIncludesNextState = version >= 7;
        }
        if (actionIncludesControl != nullptr)
        {
            *actionIncludesControl = version >= 8;
        }
        if (actionIncludesRepeat != nullptr)
        {
            *actionIncludesRepeat = version >= 9;
        }
        if (actionIncludesInfest != nullptr)
        {
            *actionIncludesInfest = version >= 10;
        }
        if (actionIncludesPull != nullptr)
        {
            *actionIncludesPull = version >= 11;
        }
        if (actionIncludesLifedrain != nullptr)
        {
            *actionIncludesLifedrain = version >= 12;
        }
        if (actionIncludesSlow) *actionIncludesSlow = version >= 13;
        if (includesAbilities) *includesAbilities = version >= 14;
        if (includesPassives) *includesPassives = version >= 15;
        return static_cast<bool>(packet) && version >= 6 && version <= CardListSchemaVersion &&
            count <= MaxSerializedItems;
    }

    count = markerOrCount;
    legacyFormat = true;
    if (actionIncludesNextState != nullptr)
    {
        *actionIncludesNextState = false;
    }
    if (actionIncludesControl != nullptr)
    {
        *actionIncludesControl = false;
    }
    if (actionIncludesRepeat != nullptr)
    {
        *actionIncludesRepeat = false;
    }
    if (actionIncludesInfest != nullptr)
    {
        *actionIncludesInfest = false;
    }
    if (actionIncludesPull != nullptr)
    {
        *actionIncludesPull = false;
    }
    if (actionIncludesLifedrain != nullptr)
    {
        *actionIncludesLifedrain = false;
    }
    if (actionIncludesSlow) *actionIncludesSlow = false;
    if (includesAbilities) *includesAbilities = false;
    if (includesPassives) *includesPassives = false;
    return count <= MaxSerializedItems;
}

inline std::string abilityFieldKey(std::string key)
{
    key.erase(std::remove_if(key.begin(), key.end(), [](unsigned char c) { return !std::isalnum(c); }), key.end());
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return key;
}

inline Card withoutLegacyAbilityFields(Card card)
{
    if (!card.activatedAbility && !card.sacrificeAbility) return card;
    const bool trail=std::any_of(card.passives.begin(),card.passives.end(),[](const auto& keyword) {return abilityFieldKey(keyword)=="trail";});
    const bool summon = !trail && ((card.activatedAbility && card.activatedAbility->kind == "summon") ||
        (card.sacrificeAbility && card.sacrificeAbility->kind == "summon"));
    std::erase_if(card.integerValues, [](const auto& field) {
        const auto key=abilityFieldKey(field.key);
        return key=="abilityrange" || key=="abilitystrength" || key=="abilityduration" || key=="abilityuses" ||
            key=="healrange" || key=="shriekrange" || key=="shriekdamage" || key=="silenceturns";
    });
    std::erase_if(card.stringValues, [&](const auto& field) {
        const auto key=abilityFieldKey(field.key);
        return (key=="ability" && card.activatedAbility) || (key=="sacrifice" && card.sacrificeAbility) ||
            key=="commandrestriction" || (summon && (key=="summon" || key=="summonunit"));
    });
    std::erase_if(card.stringLists, [](const auto& field) {
        const auto key=abilityFieldKey(field.key); return key=="abilitylabels" || key=="healfilter";
    });
    return card;
}

// Schema <=13 and rollback databases keep a projection of the canonical
// definition. New clients edit the definition, never these generated fields.
inline Card legacyAbilityProjection(Card card)
{
    card=withoutLegacyAbilityFields(std::move(card));
    const auto& definition=card.activatedAbility ? card.activatedAbility : card.sacrificeAbility;
    if (!definition) return card;
    if (card.activatedAbility) card.stringValues.push_back({"ability",card.activatedAbility->kind});
    if (card.sacrificeAbility) card.stringValues.push_back({"Sacrifice",card.sacrificeAbility->kind});
    if (definition->range) card.integerValues.push_back({"ability range",*definition->range});
    if (definition->strength) card.integerValues.push_back({"ability strength",*definition->strength});
    if (definition->duration) card.integerValues.push_back({"ability duration",*definition->duration});
    card.integerValues.push_back({"abilityUses",definition->initialUses});
    if (definition->targetFilter) card.stringLists.push_back({"heal filter",*definition->targetFilter});
    if (definition->commandRestriction) card.stringValues.push_back({"CommandRestriction",*definition->commandRestriction});
    if (definition->kind=="summon" && std::none_of(card.stringValues.begin(),card.stringValues.end(),[](const auto& field) {
        const auto key=abilityFieldKey(field.key);return key=="summon" || key=="summonunit";
    })) card.stringValues.push_back({"summon",definition->summonTitle});
    int count=0; bool labels=false;
    for (const auto& state:definition->states) { count=std::max(count,state.state+1); labels=labels || !state.label.empty(); }
    if (labels) {
        std::vector<std::string> values(count);
        for (const auto& state:definition->states) values[state.state]=state.label;
        card.stringLists.push_back({"abilityLabels",std::move(values)});
    }
    return card;
}

inline void writeCard(sf::Packet& packet, const Card& input, bool includesSlow = true, bool includesAbilities = false, bool includesPassives = false)
{
    Card card=includesAbilities ? withoutLegacyAbilityFields(input) : legacyAbilityProjection(input);
    if (includesAbilities && !includesPassives)
    {
        // Schema 14 predates this ability kind and rejects its Definition.
        if (card.activatedAbility && card.activatedAbility->kind == "corrupt the ground")
        {
            card.activatedAbility.reset();
            card.activatedAbilityName.clear();
        }
        if (card.sacrificeAbility && card.sacrificeAbility->kind == "corrupt the ground")
        {
            card.sacrificeAbility.reset();
            card.sacrificeAbilityName.clear();
        }
    }
    packet << card.title;
    packet << card.type;
    packet << card.imagePath;

    writeStringVector(packet, card.traits);
    writeStringVector(packet, includesPassives ? card.keywords : legacyPassiveKeywords(card.keywords, card.passives));

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
        writeAction(packet, action, includesSlow);
    }
    if (includesAbilities)
    {
        packet << card.activatedAbilityName << card.sacrificeAbilityName;
        ability_data::writeOptional(packet, card.activatedAbility);
        ability_data::writeOptional(packet, card.sacrificeAbility);
    }
    if (includesPassives)
    {
        packet << (card.abilityReferencesExplicit || card.activatedAbility.has_value() ||
            card.sacrificeAbility.has_value() || !card.activatedAbilityName.empty() ||
            !card.sacrificeAbilityName.empty());
        writeStringVector(packet, card.passives);
    }
}

inline bool readCardRemaining(
    sf::Packet& packet,
    Card& card,
    bool actionIncludesHeal = true,
    bool actionIncludesPush = true,
    bool actionIncludesTargetFilter = true,
    bool includesActionDisplayNames = true,
    bool actionIncludesNextState = true,
    bool actionIncludesControl = true,
    bool actionIncludesRepeat = true,
    bool actionIncludesInfest = true,
    bool actionIncludesPull = true,
    bool actionIncludesLifedrain = true,
    bool actionIncludesSlow = true, bool includesAbilities = false, bool includesPassives = false)
{
    card.abilityReferencesExplicit=includesAbilities;
    card.passives.clear();
    card.activatedAbilityName.clear(); card.sacrificeAbilityName.clear();
    card.activatedAbility.reset(); card.sacrificeAbility.reset();
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
                actionIncludesNextState,
                actionIncludesControl,
                actionIncludesRepeat,
                actionIncludesInfest,
                actionIncludesPull,
                actionIncludesLifedrain,
                actionIncludesSlow))
        {
            return false;
        }
        card.actions.push_back(action);
    }

    if (includesAbilities)
    {
        if (!(packet >> card.activatedAbilityName >> card.sacrificeAbilityName) ||
            !ability_data::readOptional(packet, card.activatedAbility) ||
            !ability_data::readOptional(packet, card.sacrificeAbility)) return false;
        if ((card.activatedAbility && card.activatedAbilityName != card.activatedAbility->name) ||
            (card.sacrificeAbility && card.sacrificeAbilityName != card.sacrificeAbility->name)) return false;
    }
    if (includesPassives) {
        if (!(packet >> card.abilityReferencesExplicit) || !readStringVector(packet, card.passives)) return false;
        for (const auto& passive : card.passives) if (!isSupportedPassive(passive)) return false;
        normalizeRescuePassives(card.passives);
    } else splitLegacyPassives(card);
    return true;
}

inline bool readCard(
    sf::Packet& packet,
    Card& card,
    bool actionIncludesNextState = true,
    bool actionIncludesControl = true,
    bool actionIncludesRepeat = true,
    bool actionIncludesInfest = true,
    bool actionIncludesPull = true,
    bool actionIncludesLifedrain = true,
    bool actionIncludesSlow = true, bool includesAbilities = false, bool includesPassives = false)
{
    packet >> card.title >> card.type >> card.imagePath;
    if (!packet || !readStringVector(packet, card.traits) || !readStringVector(packet, card.keywords))
    {
        return false;
    }
    return readCardRemaining(
        packet,
        card,
        true,
        true,
        true,
        true,
        actionIncludesNextState,
        actionIncludesControl,
        actionIncludesRepeat,
        actionIncludesInfest,
        actionIncludesPull,
        actionIncludesLifedrain,
        actionIncludesSlow, includesAbilities, includesPassives);
}

// Deck submissions and editor card saves use action references; their embedded
// definitions are ignored by the server. Keep their existing schema-12 layout.
inline bool readReferencedCard(sf::Packet& packet, Card& card, bool includesAbilities = false, bool includesPassives = false)
{
    return readCard(packet, card, true, true, true, true, true, true, false, includesAbilities, includesPassives);
}

inline bool readLegacyCard(sf::Packet& packet, Card& card)
{
    packet >> card.title >> card.type >> card.imagePath;
    if (!packet || !readStringVector(packet, card.traits))
    {
        return false;
    }
    card.keywords.clear();
    return readCardRemaining(packet, card, false, false, false, false, false, false, false, false, false, false, false);
}

inline bool readListedCard(
    sf::Packet& packet,
    Card& card,
    bool legacyFormat,
    bool actionIncludesNextState = true,
    bool actionIncludesControl = true,
    bool actionIncludesRepeat = true,
    bool actionIncludesInfest = true,
    bool actionIncludesPull = true,
    bool actionIncludesLifedrain = true,
    bool actionIncludesSlow = true, bool includesAbilities = false, bool includesPassives = false)
{
    return legacyFormat
        ? readLegacyCard(packet, card)
        : readCard(
            packet,
            card,
            actionIncludesNextState,
            actionIncludesControl,
            actionIncludesRepeat,
            actionIncludesInfest,
            actionIncludesPull,
            actionIncludesLifedrain,
            actionIncludesSlow, includesAbilities, includesPassives);
}
}
