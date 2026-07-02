#pragma once

#include "../shared/deck_data.hpp"

#include <random>
#include <string>
#include <vector>

namespace account_catalog
{
inline constexpr const char* StarterDeckName = "Starter Deck";

struct ShopCardEntry
{
    std::string title;
    std::string rarity;
};

std::vector<ShopCardEntry> loadShopCards();
std::string chooseShopCard(const std::vector<ShopCardEntry>& cards, std::mt19937& rng);

deck_data::Deck makeStarterDeck();
std::vector<std::string> starterCollectionTitles(const deck_data::Deck& starterDeck);
}
