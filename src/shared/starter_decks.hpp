#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>

namespace starter_decks
{
// The four faction decks a player picks from. Exactly one is free (the pick a
// player makes when their account has no starter deck yet); the rest cost
// StarterDeckPrice coins and can each be bought only once.
inline constexpr std::array<const char*, 4> Names = {
    "The Blackthorns",
    "The Mirewatch Resistance",
    "The Seelie Court",
    "The Unseelie Court"};

inline constexpr int StarterDeckPrice = 50;

inline std::optional<std::size_t> indexOf(const std::string& deckName)
{
    for (std::size_t i = 0; i < Names.size(); ++i)
    {
        if (deckName == Names[i])
        {
            return i;
        }
    }
    return std::nullopt;
}

inline bool isStarterDeckName(const std::string& deckName)
{
    return indexOf(deckName).has_value();
}
}
