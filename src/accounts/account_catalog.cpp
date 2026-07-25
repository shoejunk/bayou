#include "account_catalog.hpp"

#include "../shared/game_data.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <chrono>
#include <optional>
#include <utility>

namespace
{
constexpr int StarterNonHeroKinds = game_data::DeckCardCount / game_data::MaxCardCopies;
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
// Long enough that a batch of deck validations (a Conquest lock-in checks every
// participant's deck) costs one fetch, short enough that an admin who edits a
// card can immediately save a deck that uses the new value.
constexpr auto CardLibraryRefreshInterval = std::chrono::seconds(2);

std::vector<card_data::Card> authoritativeCards;
account_catalog::CardLibraryLoader cardLibraryLoader;
std::optional<std::chrono::steady_clock::time_point> lastCardLibraryLoad;

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

std::vector<std::string> loadCardTitles(const std::string& typeFilter)
{
    std::vector<std::string> titles;
    for (const card_data::Card& card : authoritativeCards)
    {
        if (game_data::isTokenCard(card))
        {
            continue;
        }
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
    for (const card_data::Card& card : authoritativeCards)
    {
        if (card.type != "Hero" && !game_data::isTokenCard(card))
        {
            titles.push_back(card.title);
        }
    }
    return titles;
}

std::string starterHeroTitle()
{
    std::vector<std::string> heroes = loadCardTitles("Hero");
    if (heroes.empty())
    {
        return PreferredStarterHero;
    }

    const auto preferred = std::find(heroes.begin(), heroes.end(), PreferredStarterHero);
    return preferred == heroes.end() ? heroes.front() : *preferred;
}

std::vector<std::string> starterHeroTitles()
{
    std::vector<std::string> heroes = loadCardTitles("Hero");
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
void setCardLibrary(std::vector<card_data::Card> cards)
{
    authoritativeCards = std::move(cards);
    lastCardLibraryLoad = std::chrono::steady_clock::now();
}

void setCardLibraryLoader(CardLibraryLoader loader)
{
    cardLibraryLoader = std::move(loader);
}

void refreshCardLibraryIfStale()
{
    if (!cardLibraryLoader)
    {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (lastCardLibraryLoad && now - *lastCardLibraryLoad < CardLibraryRefreshInterval)
    {
        return;
    }
    // Stamp before fetching so a card source that is down is retried on the
    // interval rather than on every request.
    lastCardLibraryLoad = now;

    std::string error;
    std::vector<card_data::Card> cards = cardLibraryLoader(error);
    if (!error.empty() || cards.empty())
    {
        fmt::println(
            "Account server kept the previous card catalog: {}",
            error.empty() ? "card source returned no cards" : error);
        return;
    }

    authoritativeCards = std::move(cards);
}

const std::vector<card_data::Card>& cardLibrary()
{
    return authoritativeCards;
}

std::vector<ShopCardEntry> loadCollectibleCards()
{
    std::vector<ShopCardEntry> cards;
    for (const card_data::Card& card : authoritativeCards)
    {
        const std::string rarity = game_data::cardRarity(card);
        if (rarity == "token")
        {
            continue;
        }
        cards.push_back({
            card.title,
            rarity});
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

std::vector<ShopCardEntry> loadShopCards()
{
    std::vector<ShopCardEntry> cards = loadCollectibleCards();
    cards.erase(
        std::remove_if(
            cards.begin(),
            cards.end(),
            [](const ShopCardEntry& card) { return card.rarity == "starter"; }),
        cards.end());
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

deck_data::Deck makeStarterDeck(const std::string& deckName)
{
    deck_data::Deck deck;
    deck.name = deckName;
    for (const std::string& hero : starterHeroTitles())
    {
        const auto found = std::find_if(authoritativeCards.begin(), authoritativeCards.end(), [&](const card_data::Card& card) {
            return card.title == hero;
        });
        if (found == authoritativeCards.end() || game_data::cardDeckLimit(*found) > 0)
        {
            deck.cardTitles.push_back(hero);
        }
    }

    std::vector<std::string> nonHeroCandidates = starterNonHeroSlots();
    for (const std::string& title : loadNonHeroCardTitles())
    {
        if (!containsTitle(nonHeroCandidates, title))
        {
            nonHeroCandidates.push_back(title);
        }
    }

    int nonHeroCount = 0;
    while (nonHeroCount < game_data::DeckCardCount)
    {
        bool addedCopy = false;
        for (const std::string& title : nonHeroCandidates)
        {
            if (nonHeroCount >= game_data::DeckCardCount)
            {
                break;
            }
            const auto found = std::find_if(authoritativeCards.begin(), authoritativeCards.end(), [&](const card_data::Card& card) {
                return card.title == title;
            });
            const int copyLimit = found == authoritativeCards.end()
                ? game_data::MaxCardCopies
                : game_data::cardDeckLimit(*found);
            const int copies = static_cast<int>(std::count(
                deck.cardTitles.begin(), deck.cardTitles.end(), title));
            if (copies < copyLimit)
            {
                deck.cardTitles.push_back(title);
                ++nonHeroCount;
                addedCopy = true;
            }
        }
        if (!addedCopy)
        {
            break;
        }
    }
    return deck;
}
}
