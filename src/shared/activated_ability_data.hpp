#pragma once

#include <SFML/Network.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ability_data
{
constexpr std::uint32_t SchemaMarker = 0x41424431; // ABD1
constexpr std::uint32_t MaxStates = 256;

// Definitions are reusable catalog objects. Current state and remaining uses
// belong to the acting piece, never to this shared definition.
struct State
{
    int state = 0;
    int nextState = 0;
    std::string label;
    bool operator==(const State &) const = default;
};

struct Definition
{
    std::string name;
    std::string kind;
    std::optional<int> range;
    std::optional<int> strength;
    std::optional<int> duration;
    std::optional<std::vector<std::string>> targetFilter;
    std::optional<std::string> commandRestriction;
    std::string summonTitle;
    int initialUses = 0; // Dig: positive finite count; -1 unlimited
    std::vector<State> states;
    bool operator==(const Definition &) const = default;
};

inline bool supportedKind(const std::string &kind)
{
    return kind == "dig" || kind == "transform" || kind == "dematerialize" || kind == "summon" || kind == "command" ||
           kind == "raise undead" || kind == "swap" || kind == "power boost" || kind == "area heal" ||
           kind == "silence" || kind == "shriek" || kind == "weaken";
}

inline bool valid(const Definition &value, bool requireName = false)
{
    if ((requireName && value.name.empty()) || !supportedKind(value.kind) || (value.range && *value.range < 0) ||
        (value.strength && *value.strength < 0) || (value.duration && *value.duration < 1) || value.initialUses < -1 ||
        value.states.size() > MaxStates || (value.targetFilter && value.targetFilter->size() > 4096))
        return false;
    std::vector<bool> seen(MaxStates, false);
    for (const auto &state : value.states)
    {
        if (state.state < 0 || state.state >= static_cast<int>(MaxStates) || state.nextState < 0 ||
            state.nextState >= static_cast<int>(MaxStates) || seen[state.state])
            return false;
        seen[state.state] = true;
    }
    if (value.kind == "transform" || value.kind == "dematerialize")
    {
        if (value.states.empty() || !seen[0])
            return false;
        for (const auto &state : value.states)
            if (!seen[state.nextState])
                return false;
    }
    return true;
}

inline void write(sf::Packet &packet, const Definition &value)
{
    packet << SchemaMarker << value.name << value.kind;
    for (const auto *field : {&value.range, &value.strength, &value.duration})
    {
        packet << field->has_value();
        if (*field)
            packet << **field;
    }
    packet << value.targetFilter.has_value();
    if (value.targetFilter)
    {
        packet << static_cast<std::uint32_t>(value.targetFilter->size());
        for (const auto &filter : *value.targetFilter)
            packet << filter;
    }
    packet << value.commandRestriction.has_value();
    if (value.commandRestriction)
        packet << *value.commandRestriction;
    packet << value.summonTitle << value.initialUses << static_cast<std::uint32_t>(value.states.size());
    for (const auto &state : value.states)
        packet << state.state << state.nextState << state.label;
}

inline bool read(sf::Packet &packet, Definition &value)
{
    value = {};
    std::uint32_t marker = 0, count = 0;
    if (!(packet >> marker >> value.name >> value.kind) || marker != SchemaMarker)
        return false;
    for (auto *field : {&value.range, &value.strength, &value.duration})
    {
        bool present = false;
        if (!(packet >> present))
            return false;
        if (present)
        {
            int n = 0;
            if (!(packet >> n))
                return false;
            *field = n;
        }
    }
    bool present = false;
    if (!(packet >> present))
        return false;
    if (present)
    {
        if (!(packet >> count) || count > 4096)
            return false;
        value.targetFilter.emplace();
        for (std::uint32_t i = 0; i < count; ++i)
        {
            std::string filter;
            if (!(packet >> filter))
                return false;
            value.targetFilter->push_back(std::move(filter));
        }
    }
    if (!(packet >> present))
        return false;
    if (present)
    {
        std::string filter;
        if (!(packet >> filter))
            return false;
        value.commandRestriction = std::move(filter);
    }
    if (!(packet >> value.summonTitle >> value.initialUses >> count) || count > MaxStates)
        return false;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        State state;
        if (!(packet >> state.state >> state.nextState >> state.label))
            return false;
        value.states.push_back(std::move(state));
    }
    return valid(value);
}

inline void writeOptional(sf::Packet &packet, const std::optional<Definition> &value)
{
    packet << value.has_value();
    if (value)
        write(packet, *value);
}

inline bool readOptional(sf::Packet &packet, std::optional<Definition> &value)
{
    value.reset();
    bool present = false;
    if (!(packet >> present))
        return false;
    return !present || read(packet, value.emplace());
}
} // namespace ability_data
