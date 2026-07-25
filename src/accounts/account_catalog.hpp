#pragma once

#include "../shared/card_data.hpp"
#include "../shared/deck_data.hpp"

#include <functional>
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

// Fetches the authoritative catalog, returning an empty vector and setting
// `error` when the card source is unavailable.
using CardLibraryLoader = std::function<std::vector<card_data::Card>(std::string& error)>;

void setCardLibrary(std::vector<card_data::Card> cards);
// Installs the source refreshCardLibraryIfStale() pulls from. Without a loader
// the library only ever holds what setCardLibrary() was given (tests do that).
void setCardLibraryLoader(CardLibraryLoader loader);
// Admins edit cards while the account server runs, so validation paths re-pull
// the catalog instead of judging decks against the copy fetched at startup.
// Rate limited, and keeps the current catalog when the fetch fails. Call it
// with the database mutex held: it replaces what cardLibrary() hands out.
void refreshCardLibraryIfStale();
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
