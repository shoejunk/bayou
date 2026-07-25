#include "ai_deck.hpp"

#include "../accounts/account_catalog.hpp"
#include "../accounts/account_decks.hpp"
#include "../shared/deck_data.hpp"
#include "../shared/game_data.hpp"
#include "../shared/starter_decks.hpp"

#include <SQLiteCpp/SQLiteCpp.h>
#include <fmt/core.h>

#include <algorithm>
#include <exception>
#include <optional>
#include <string>

namespace
{
constexpr const char* PreferredStarterHero = "Steam Baron";
constexpr const char* AccountDatabasePath = "accounts.db";

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

// The AI plays the first faction starter deck, so its opponent list stays in
// step with whatever an admin has defined for new players.
constexpr const char* AiStarterDeckName = starter_decks::Names.front();

std::optional<deck_data::Deck> loadStarterDeckOverride()
{
    try
    {
        SQLite::Database database(AccountDatabasePath, SQLite::OPEN_READONLY);
        return account_decks::loadStarterDeckOverride(database, AiStarterDeckName);
    }
    catch (const std::exception& error)
    {
        fmt::println("Could not load AI starter deck override from {}: {}", AccountDatabasePath, error.what());
    }
    return std::nullopt;
}

std::vector<std::string> starterDeckTitles()
{
    if (const std::optional<deck_data::Deck> storedStarter = loadStarterDeckOverride())
    {
        return storedStarter->cardTitles;
    }
    return account_catalog::makeStarterDeck(AiStarterDeckName).cardTitles;
}

card_data::Card makeFallbackUnit(
    const std::string& title,
    int cost,
    int health,
    int attack,
    int range,
    int move,
    const std::string& movement)
{
    card_data::Card card;
    card.title = title;
    card.type = "Unit";
    card.imagePath = "cards/clockwork-rook.png";
    card.integerValues = {{"attack", attack}, {"cost", cost}, {"health", health}, {"move", move}, {"range", range}};
    card.stringValues = {{"movement", movement}};
    return card;
}

std::vector<card_data::Card> fallbackStarterDeck()
{
    std::vector<card_data::Card> deck;
    card_data::Card hero;
    hero.title = PreferredStarterHero;
    hero.type = "Hero";
    hero.imagePath = "cards/steam-baron.png";
    hero.integerValues = {{"attack", 8}, {"health", 30}, {"heroCost", 100}, {"move", 1}, {"range", 1}};
    hero.stringValues = {{"movement", "omni"}};
    deck.push_back(hero);

    const std::vector<card_data::Card> units = {
        makeFallbackUnit("Brass Pawn", 10, 4, 2, 1, 1, "ortho"),
        makeFallbackUnit("Rifleman", 30, 6, 4, 3, 1, "ortho"),
        makeFallbackUnit("Clockwork Rook", 40, 12, 5, 1, 7, "ortho"),
        makeFallbackUnit("Steam Bishop", 40, 9, 5, 1, 7, "diag"),
        makeFallbackUnit("Automaton Knight", 30, 9, 4, 1, 1, "jump"),
        makeFallbackUnit("Dredger", 50, 16, 6, 1, 1, "omni"),
        makeFallbackUnit("Spark Drone", 20, 4, 3, 2, 2, "omni"),
        makeFallbackUnit("Sentroid", 30, 8, 3, 1, 1, "ortho"),
        makeFallbackUnit("Patrol Bot", 20, 6, 2, 1, 2, "omni"),
        makeFallbackUnit("Rustbucket", 10, 5, 2, 1, 1, "ortho"),
    };
    for (const card_data::Card& unit : units)
    {
        deck.push_back(unit);
        deck.push_back(unit);
    }
    return deck;
}

const card_data::Card* findCardByTitle(const std::vector<card_data::Card>& cards, const std::string& title)
{
    const auto found = std::find_if(cards.begin(), cards.end(), [&](const card_data::Card& card) {
        return card.title == title;
    });
    return found == cards.end() ? nullptr : &*found;
}

std::vector<card_data::Card> resolveDeckTitles(
    const std::vector<card_data::Card>& library,
    const std::vector<std::string>& titles,
    const std::string& context)
{
    std::vector<card_data::Card> deck;
    deck.reserve(titles.size());
    for (const std::string& title : titles)
    {
        const card_data::Card* card = findCardByTitle(library, title);
        if (card == nullptr)
        {
            fmt::println("AI starter deck {} references missing card '{}'", context, title);
            return {};
        }
        deck.push_back(*card);
    }

    if (const std::optional<std::string> error = game_data::deckRulesError(deck))
    {
        fmt::println("AI starter deck {} failed deck rules validation: {}", context, *error);
        return {};
    }
    return deck;
}
}

namespace ai_deck
{
std::vector<card_data::Card> makeStarterDeck(const std::vector<card_data::Card>& library)
{
    if (library.empty())
    {
        return fallbackStarterDeck();
    }

    std::vector<card_data::Card> configuredStarterDeck =
        resolveDeckTitles(library, starterDeckTitles(), "configuration");
    if (!configuredStarterDeck.empty())
    {
        return configuredStarterDeck;
    }
    fmt::println("AI starter deck configuration failed, falling back");

    const std::vector<card_data::Card> generatedStarterDeck = resolveDeckTitles(
        library,
        account_catalog::makeStarterDeck(AiStarterDeckName).cardTitles,
        "generated fallback");
    if (!generatedStarterDeck.empty())
    {
        return generatedStarterDeck;
    }

    std::vector<card_data::Card> deck;
    const card_data::Card* hero = findCardByTitle(library, PreferredStarterHero);
    if (hero == nullptr || !game_data::isHeroCard(*hero))
    {
        const auto firstHero = std::find_if(library.begin(), library.end(), game_data::isHeroCard);
        if (firstHero != library.end())
        {
            hero = &*firstHero;
        }
    }
    if (hero != nullptr)
    {
        deck.push_back(*hero);
    }

    std::vector<std::string> orderedTitles;
    for (const std::string& title : fallbackStarterNonHeroes())
    {
        if (const card_data::Card* card = findCardByTitle(library, title);
            card != nullptr && !game_data::isHeroCard(*card) && !game_data::isTokenCard(*card))
        {
            orderedTitles.push_back(title);
        }
    }
    for (const card_data::Card& card : library)
    {
        if (!game_data::isHeroCard(card) && !game_data::isTokenCard(card) &&
            std::find(orderedTitles.begin(), orderedTitles.end(), card.title) == orderedTitles.end())
        {
            orderedTitles.push_back(card.title);
        }
    }

    int nonHeroCount = 0;
    for (const std::string& title : orderedTitles)
    {
        if (nonHeroCount >= game_data::DeckCardCount)
        {
            break;
        }
        if (const card_data::Card* card = findCardByTitle(library, title))
        {
            const int copyLimit = game_data::cardDeckLimit(*card);
            for (int copy = 0; copy < copyLimit && nonHeroCount < game_data::DeckCardCount; ++copy)
            {
                deck.push_back(*card);
                ++nonHeroCount;
            }
        }
    }

    if (game_data::deckRulesError(deck))
    {
        return fallbackStarterDeck();
    }
    return deck;
}
}
