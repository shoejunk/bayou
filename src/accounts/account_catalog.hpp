#pragma once

#include "../shared/card_data.hpp"
#include "../shared/deck_data.hpp"

#include <random>
#include <string>
#include <vector>

namespace account_catalog
{
struct ShopCardEntry
{
    std::string title;
    std::string rarity;
};

void setCardLibrary(std::vector<card_data::Card> cards);
const std::vector<card_data::Card>& cardLibrary();

// Every card a player may own (everything except tokens).
std::vector<ShopCardEntry> loadCollectibleCards();
// The random-shop pool: collectible cards minus starter-rarity cards, which
// are only handed out with the faction starter decks.
std::vector<ShopCardEntry> loadShopCards();
std::string chooseShopCard(const std::vector<ShopCardEntry>& cards, std::mt19937& rng);

// Built-in contents used for a faction starter deck an admin has not defined
// yet, so a fresh database still hands new players a playable deck.
deck_data::Deck makeStarterDeck(const std::string& deckName);
}
