#include "ai_deck.hpp"

#include "../shared/card_database.hpp"
#include "../shared/game_data.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <exception>
#include <string>

namespace
{
constexpr int StarterNonHeroKinds = game_data::DeckCardCount / game_data::MaxCardCopies;
constexpr const char* PreferredStarterHero = "Steam Baron";

std::vector<card_data::Card> loadCardsFromCardsDb()
{
    try
    {
        return card_database::loadCardsFromFile("cards.db");
    }
    catch (const std::exception& error)
    {
        fmt::println("Could not load AI starter deck from cards.db: {}", error.what());
    }
    return {};
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
        "Smoke Bomb",
        "Cannon Blast",
        "Repair Kit",
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
        makeFallbackUnit("Gearwright", 20, 5, 2, 1, 2, "omni"),
        makeFallbackUnit("Brass Medic", 30, 7, 2, 1, 2, "diag"),
        makeFallbackUnit("Boiler Imp", 10, 3, 3, 1, 2, "omni"),
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

// Card titles copied from the "Bayou Gang" deck (account database, decks.id 16).
const std::vector<std::string>& bayouGangDeckTitles()
{
    static const std::vector<std::string> titles = {
        "Automatick",
        "Automatick",
        "Choking Blossom",
        "Choking Blossom",
        "Elias Tiberion",
        "Hired Gun",
        "Hired Gun",
        "Rustbucket",
        "Scarlett Glumpkin",
        "Stingy",
        "Stingy",
        "Sweetykins",
        "Sweetykins",
        "Telematron",
        "Telematron",
        "Tinkering Tom",
        "Rustbucket",
        "Bramble Drone",
        "Bramble Drone",
        "Delving Daphodilus",
        "Gentle Bot",
        "Hop Bot",
        "Gentle Bot",
    };
    return titles;
}
}

namespace ai_deck
{
std::vector<card_data::Card> makeStarterDeck()
{
    const std::vector<card_data::Card> library = loadCardsFromCardsDb();
    if (library.empty())
    {
        return fallbackStarterDeck();
    }

    std::vector<card_data::Card> bayouGangDeck;
    for (const std::string& title : bayouGangDeckTitles())
    {
        if (const card_data::Card* card = findCardByTitle(library, title))
        {
            bayouGangDeck.push_back(*card);
        }
    }
    if (!game_data::deckRulesError(bayouGangDeck))
    {
        return bayouGangDeck;
    }
    fmt::println("Bayou Gang AI deck failed deck rules validation, falling back");

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
            card != nullptr && !game_data::isHeroCard(*card))
        {
            orderedTitles.push_back(title);
        }
    }
    for (const card_data::Card& card : library)
    {
        if (!game_data::isHeroCard(card) &&
            std::find(orderedTitles.begin(), orderedTitles.end(), card.title) == orderedTitles.end())
        {
            orderedTitles.push_back(card.title);
        }
    }

    for (std::size_t i = 0; i < orderedTitles.size() && i < static_cast<std::size_t>(StarterNonHeroKinds); ++i)
    {
        if (const card_data::Card* card = findCardByTitle(library, orderedTitles[i]))
        {
            deck.push_back(*card);
            deck.push_back(*card);
        }
    }

    if (game_data::deckRulesError(deck))
    {
        return fallbackStarterDeck();
    }
    return deck;
}
}
