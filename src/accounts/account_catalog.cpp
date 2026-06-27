#include "account_catalog.hpp"

#include "../shared/card_database.hpp"
#include "../shared/game_data.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <exception>

namespace
{
constexpr int StarterNonHeroKinds = game_data::DeckCardCount / game_data::MaxCardCopies;
constexpr const char* StarterDeckName = "Starter Deck";
constexpr const char* PreferredStarterHero = "Steam Baron";
constexpr const char* StarterDeckHeroTitles[] = {"Tinkering Tom", "Scarlett Glumpkin", "Elias Tiberion"};
constexpr const char* StarterDeckNonHeroTitles[] = {
    "Brass Pawn",
    "Rifleman",
    "Clockwork Rook",
    "Steam Bishop",
    "Automaton Knight",
    "Dredger",
    "Spark Drone",
    "Sentroid",
    "Patrol Bot",
    "Rustbucket",
};
constexpr const char* StarterCollectionExtraTitles[] = {PreferredStarterHero};

int shopRarityWeight(const std::string& rarity)
{
    if (rarity == "legendary")
    {
        return 5;
    }
    if (rarity == "rare")
    {
        return 25;
    }
    return 70;
}

std::string normalizedRarity(const std::string& rarity)
{
    if (rarity == "rare" || rarity == "legendary")
    {
        return rarity;
    }
    return "common";
}

const std::vector<std::string>& fallbackStarterNonHeroes()
{
    static const std::vector<std::string> titles = {
        "Brass Pawn",
        "Rifleman",
        "Clockwork Rook",
        "Steam Bishop",
        "Automaton Knight",
        "Dredger",
        "Spark Drone",
        "Sentroid",
        "Patrol Bot",
        "Rustbucket",
        "Overpressure",
        "Gearwright",
        "Brass Medic",
        "Boiler Imp",
        "Railgunner",
        "Swamp Skiff",
        "Arc Lantern",
        "Sprocket Swarm",
        "Chain Harpoon",
        "Mudslide",
    };
    return titles;
}

bool containsTitle(const std::vector<std::string>& titles, const std::string& title)
{
    return std::find(titles.begin(), titles.end(), title) != titles.end();
}

std::vector<card_data::Card> loadCardsFromCardsDb(const std::string& failureContext)
{
    try
    {
        return card_database::loadCardsFromFile("cards.db");
    }
    catch (const std::exception& error)
    {
        fmt::println("{}: {}", failureContext, error.what());
    }
    return {};
}

std::vector<std::string> loadCardTitlesFromCardsDb(const std::string& typeFilter)
{
    std::vector<std::string> titles;
    for (const card_data::Card& card : loadCardsFromCardsDb(
             "Could not read cards.db while building account inventory"))
    {
        if (typeFilter.empty() || card.type == typeFilter)
        {
            titles.push_back(card.title);
        }
    }
    return titles;
}

std::vector<std::string> loadNonHeroCardTitles()
{
    std::vector<std::string> titles;
    for (const card_data::Card& card : loadCardsFromCardsDb("Could not read non-hero cards from cards.db"))
    {
        if (card.type != "Hero")
        {
            titles.push_back(card.title);
        }
    }
    return titles;
}

std::vector<std::string> loadAllCardTitles()
{
    std::vector<std::string> titles = loadCardTitlesFromCardsDb("");
    if (!titles.empty())
    {
        return titles;
    }

    titles.push_back(PreferredStarterHero);
    const std::vector<std::string>& fallback = fallbackStarterNonHeroes();
    titles.insert(titles.end(), fallback.begin(), fallback.end());
    return titles;
}

std::string starterHeroTitle()
{
    std::vector<std::string> heroes = loadCardTitlesFromCardsDb("Hero");
    if (heroes.empty())
    {
        return PreferredStarterHero;
    }

    const auto preferred = std::find(heroes.begin(), heroes.end(), PreferredStarterHero);
    return preferred == heroes.end() ? heroes.front() : *preferred;
}

std::vector<std::string> starterHeroTitles()
{
    std::vector<std::string> heroes = loadCardTitlesFromCardsDb("Hero");
    std::vector<std::string> result;
    for (const char* name : StarterDeckHeroTitles)
    {
        if (containsTitle(heroes, name))
        {
            result.push_back(name);
        }
    }
    if (result.empty())
    {
        result.push_back(starterHeroTitle());
    }
    return result;
}

std::vector<std::string> starterNonHeroSlots()
{
    std::vector<std::string> available = loadNonHeroCardTitles();
    std::vector<std::string> ordered;

    for (const char* title : StarterDeckNonHeroTitles)
    {
        if (available.empty() || containsTitle(available, title))
        {
            ordered.push_back(title);
        }
    }

    const std::vector<std::string>& fallback = fallbackStarterNonHeroes();
    for (const std::string& title : fallback)
    {
        if (ordered.size() >= StarterNonHeroKinds)
        {
            break;
        }
        if ((available.empty() || containsTitle(available, title)) && !containsTitle(ordered, title))
        {
            ordered.push_back(title);
        }
    }
    for (const std::string& title : available)
    {
        if (ordered.size() >= StarterNonHeroKinds)
        {
            break;
        }
        if (!containsTitle(ordered, title))
        {
            ordered.push_back(title);
        }
    }
    if (ordered.empty())
    {
        ordered = fallback;
    }

    const std::size_t uniqueCount = ordered.size();
    for (std::size_t i = 0; ordered.size() < StarterNonHeroKinds; ++i)
    {
        ordered.push_back(ordered[i % uniqueCount]);
    }
    if (ordered.size() > StarterNonHeroKinds)
    {
        ordered.resize(StarterNonHeroKinds);
    }
    return ordered;
}
}

namespace account_catalog
{
std::vector<ShopCardEntry> loadShopCards()
{
    std::vector<ShopCardEntry> cards;
    for (const card_data::Card& card : loadCardsFromCardsDb("Could not read shop cards from cards.db"))
    {
        cards.push_back({
            card.title,
            normalizedRarity(game_data::cardStr(card, "rarity", "common"))});
    }

    if (!cards.empty())
    {
        return cards;
    }

    cards.push_back({PreferredStarterHero, "legendary"});
    const std::vector<std::string>& fallback = fallbackStarterNonHeroes();
    cards.reserve(cards.size() + fallback.size());
    for (const std::string& title : fallback)
    {
        cards.push_back({title, "common"});
    }
    return cards;
}

std::string chooseShopCard(const std::vector<ShopCardEntry>& cards, std::mt19937& rng)
{
    std::vector<ShopCardEntry> common;
    std::vector<ShopCardEntry> rare;
    std::vector<ShopCardEntry> legendary;
    for (const ShopCardEntry& card : cards)
    {
        if (card.rarity == "legendary")
        {
            legendary.push_back(card);
        }
        else if (card.rarity == "rare")
        {
            rare.push_back(card);
        }
        else
        {
            common.push_back(card);
        }
    }

    std::vector<const std::vector<ShopCardEntry>*> buckets;
    std::vector<int> weights;
    auto addBucket = [&](const std::vector<ShopCardEntry>& bucket, const std::string& rarity) {
        if (!bucket.empty())
        {
            buckets.push_back(&bucket);
            weights.push_back(shopRarityWeight(rarity));
        }
    };
    addBucket(common, "common");
    addBucket(rare, "rare");
    addBucket(legendary, "legendary");

    std::discrete_distribution<std::size_t> rarityDistribution(weights.begin(), weights.end());
    const std::vector<ShopCardEntry>& bucket = *buckets[rarityDistribution(rng)];
    std::uniform_int_distribution<std::size_t> cardDistribution(0, bucket.size() - 1);
    return bucket[cardDistribution(rng)].title;
}

deck_data::Deck makeStarterDeck()
{
    deck_data::Deck deck;
    deck.name = StarterDeckName;
    for (const std::string& hero : starterHeroTitles())
    {
        deck.cardTitles.push_back(hero);
    }
    for (const std::string& title : starterNonHeroSlots())
    {
        for (int copy = 0; copy < game_data::MaxCardCopies; ++copy)
        {
            deck.cardTitles.push_back(title);
        }
    }
    return deck;
}

std::vector<std::string> starterCollectionTitles(const deck_data::Deck& starterDeck)
{
    std::vector<std::string> collection = starterDeck.cardTitles;
    const std::vector<std::string> allCards = loadAllCardTitles();

    for (const char* title : StarterCollectionExtraTitles)
    {
        if (allCards.empty() || containsTitle(allCards, title))
        {
            collection.push_back(title);
        }
    }

    return collection;
}
}
