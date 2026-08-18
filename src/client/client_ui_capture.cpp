#include "client_ui_capture.hpp"

#include "../shared/game_data.hpp"

#include "client_config.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>

namespace bayou::client::ui_capture
{
namespace
{

bool startsWith(std::string_view value, std::string_view prefix)
{
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::vector<std::string> splitList(std::string_view value)
{
    std::vector<std::string> items;
    std::string current;
    for (const char ch : value)
    {
        if (ch == ',' || ch == ';')
        {
            if (!current.empty())
            {
                items.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty())
    {
        items.push_back(current);
    }
    return items;
}

struct SampleCard
{
    const char* title;
    const char* type;
    const char* art;
    int cost;     // resource cost; 0 for heroes
    int heroCost; // hero cost; 0 for everything else
    int health;
    int attack;
    int range;
    // The first trait must be one of game_data::CardTraitLabels, because that is
    // what the collection's trait filters match and what real catalogue cards
    // carry. The court follows it as flavour.
    const char* trait;
    // A second canonical trait, used by heroes so a coherent faction list is not
    // flagged for trait mismatch against its own units. Empty for everything else.
    const char* trait2;
    const char* court;
    const char* rarity;
    const char* keyword;
    const char* text;
};

// A cross section of the real catalogue: two heroes and a spread of units per
// court, costs from 1 to 7, and every rarity represented, so the collection
// screens have enough shape to design against: a curve that is not flat, rarity
// gems that differ, and enough owned cards per court to build a legal deck.
constexpr SampleCard SampleCards[] = {
    // --- heroes ------------------------------------------------------------
    {"Sylvara", "Hero", "cards/Sylvara.png", 0, 34, 22, 4, 1, "Honorable", "Fey", "Seelie", "legendary", "Regal",
     "While Sylvara holds a home square, friendly Seelie units gain +1 attack."},
    {"Nettle Starbright", "Hero", "cards/nettleStarbright.png", 0, 28, 20, 3, 2, "Fey", "Arcane", "Seelie", "rare", "Swift",
     "Deploy: draw a card. Nettle may act again after a kill."},
    {"Thaeron Baelstone", "Hero", "cards/thaeronBaelstone.png", 0, 36, 24, 5, 1, "Corrupt", "Undead", "Unseelie", "legendary", "Relentless",
     "Relentless. When Thaeron destroys a unit, he may move once more."},
    {"Prince Vesper", "Hero", "cards/princeVesper.png", 0, 32, 21, 4, 2, "Arcane", "Corrupt", "Unseelie", "legendary", "Hex",
     "Ability: an enemy unit cannot act on its owner's next turn."},
    {"Reed Baelstone", "Hero", "cards/reedBaelstone.png", 0, 26, 23, 3, 1, "Wild", "Honorable", "Mirewatch", "rare", "Dig In",
     "Reed takes 1 less damage while standing on marsh."},
    {"Maggie Mudroot", "Hero", "cards/maggieMudroot.png", 0, 30, 19, 3, 2, "Ancient", "Wild", "Mirewatch", "legendary", "Rootbound",
     "Ability: heal every friendly unit within range 2 for 2."},
    {"Braun Stonefist", "Hero", "cards/braunStonefist.png", 0, 30, 26, 5, 1, "Civilized", "Mechanical", "Blackthorn", "rare", "Bruiser",
     "Braun deals 2 extra damage to structures and mechanical units."},
    {"Victor Greyshard", "Hero", "cards/victorGreyshard.png", 0, 34, 20, 4, 1, "Mechanical", "Civilized", "Blackthorn", "legendary", "Levy",
     "Levy. Gain 2 coins whenever an enemy unit is destroyed."},

    // --- the Seelie Court --------------------------------------------------
    {"Quinberry Lark", "Unit", "cards/quinberryLark.png", 1, 0, 2, 1, 2, "Fey", "", "Seelie", "starter", "Swift",
     "Swift. Quinberry Lark may move after attacking."},
    {"Fey Messenger", "Unit", "cards/feyMessenger.png", 1, 0, 2, 1, 1, "Fey", "", "Seelie", "common", "Courier",
     "Deploy: look at the top card of your deck. You may put it on the bottom."},
    {"Grove Sister", "Unit", "cards/groveSister.png", 2, 0, 4, 2, 1, "Fey", "", "Seelie", "common", "Mend",
     "Ability: heal an adjacent friendly unit for 2."},
    {"Heartwood Sister", "Unit", "cards/heartwoodSister.png", 3, 0, 5, 1, 1, "Honorable", "", "Seelie", "common", "Mend",
     "Ability: heal an adjacent friendly unit for 3."},
    {"Sylvan Enchantress", "Unit", "cards/sylvanEnchantress.png", 4, 0, 5, 2, 2, "Arcane", "", "Seelie", "rare", "Bewitch",
     "Ability: an enemy unit at range 2 cannot attack on its owner's next turn."},
    {"Duchess Dewbell", "Unit", "cards/duchessDewbell.png", 4, 0, 7, 3, 1, "Honorable", "", "Seelie", "rare", "Command",
     "Command: an adjacent friendly unit may act immediately after Dewbell."},
    {"Starbloom Knight", "Unit", "cards/starbloomKnight.png", 5, 0, 8, 4, 1, "Honorable", "", "Seelie", "common", "Charge",
     "Charge. Starbloom Knight may move two squares before attacking."},
    {"Sylvan Champion", "Unit", "cards/sylvanChampion.png", 6, 0, 9, 5, 1, "Honorable", "", "Seelie", "rare", "Rally",
     "Friendly Fey units within range 2 gain +1 attack."},
    {"Crystal Unicorn", "Unit", "cards/crystalUnicorn.png", 7, 0, 9, 4, 1, "Arcane", "", "Seelie", "legendary", "Ward",
     "Ward. The first time Crystal Unicorn would be damaged each turn, prevent it."},
    {"Hidden Path", "Spell", "cards/hiddenPath.png", 2, 0, 0, 0, 0, "Arcane", "", "Seelie", "common", "Instant",
     "Move a friendly unit up to three squares. It may not attack this turn."},
    {"Heartshoot", "Spell", "cards/heartshoot.png", 3, 0, 0, 0, 0, "Fey", "", "Seelie", "rare", "Instant",
     "Deal 4 damage to a unit at range 3. Heal your hero for 2."},
    {"Heartwood", "Structure", "cards/heartwood.png", 4, 0, 12, 0, 0, "Ancient", "", "Seelie", "rare", "Grow",
     "Grow 2. After two turns Heartwood produces a Grove Sister on an adjacent square."},

    // --- the Unseelie Court ------------------------------------------------
    {"Blightling", "Unit", "cards/blightling.png", 1, 0, 2, 1, 1, "Corrupt", "", "Unseelie", "starter", "Spore",
     "When Blightling dies, adjacent enemy units take 1 damage."},
    {"Gloom Fairy", "Unit", "cards/gloomFairy.png", 2, 0, 3, 2, 2, "Corrupt", "", "Unseelie", "common", "Swift",
     "Swift. Gloom Fairy may move after attacking."},
    {"Eyeblight", "Unit", "cards/eyeblight.png", 3, 0, 4, 2, 3, "Undead", "", "Unseelie", "common", "Watcher",
     "Enemy units cannot use Ambush while Eyeblight is on the board."},
    {"Ashenfang", "Unit", "cards/Ashenfang.png", 5, 0, 7, 5, 1, "Undead", "", "Unseelie", "rare", "Rebirth",
     "Rebirth 1. Ashenfang returns once with 1 health when destroyed."},
    {"Thorn Griffin", "Unit", "cards/thornGriffin.png", 5, 0, 8, 4, 1, "Wild", "", "Unseelie", "rare", "Flying",
     "Flying. Thorn Griffin ignores holes and may hop over other pieces."},
    {"Erevan the Shadow", "Unit", "cards/erevanTheShadow.png", 6, 0, 7, 5, 1, "Corrupt", "", "Unseelie", "legendary", "Ambush",
     "Ambush. Erevan deals double damage to units that have not yet acted."},

    // --- the Mirewatch Resistance ------------------------------------------
    {"Mirewatch Informant", "Unit", "cards/mirewatchInformant.png", 1, 0, 2, 1, 1, "Wild", "", "Mirewatch", "starter", "Scout",
     "Deploy: reveal the top card of your opponent's deck."},
    {"Swamp Tracker", "Unit", "cards/swampTracker.png", 2, 0, 4, 2, 2, "Wild", "", "Mirewatch", "common", "Tracker",
     "Swamp Tracker ignores movement penalties from marsh."},
    {"Bog Spearman", "Unit", "cards/bogSpearman.png", 3, 0, 5, 3, 2, "Wild", "", "Mirewatch", "common", "Reach",
     "Reach. Bog Spearman strikes at range 2 without moving."},
    {"Archivist Mosswake", "Unit", "cards/archivistMosswake.png", 4, 0, 5, 2, 1, "Ancient", "", "Mirewatch", "rare", "Insight",
     "Deploy: look at the top two cards of your deck and keep one."},
    {"Marshland Veteran", "Unit", "cards/marshlandVeteran.png", 4, 0, 6, 3, 1, "Honorable", "", "Mirewatch", "common", "Steadfast",
     "Steadfast. Marshland Veteran cannot be pushed or stunned."},
    {"Vanya Bluewater", "Unit", "cards/vanyaBluewater.png", 5, 0, 7, 3, 2, "Arcane", "", "Mirewatch", "rare", "Tide",
     "Ability: push an enemy unit one square away from Vanya."},
    {"Donella of the Marsh", "Unit", "cards/donellaOfTheMarsh.png", 6, 0, 8, 4, 1, "Ancient", "", "Mirewatch", "legendary", "Warden",
     "Friendly units adjacent to Donella take 1 less damage."},

    // --- the Blackthorns ---------------------------------------------------
    {"Blackthorn Lumberjack", "Unit", "cards/blackthornLumberjack.png", 2, 0, 4, 3, 1, "Civilized", "", "Blackthorn", "starter", "Fell",
     "Blackthorn Lumberjack deals 3 extra damage to structures."},
    {"Goblin Ambusher", "Unit", "cards/goblinAmbusher.png", 2, 0, 3, 3, 1, "Corrupt", "", "Blackthorn", "common", "Ambush",
     "Ambush. Deals double damage to units that have not yet acted."},
    {"Goblin Sharpshooter", "Unit", "cards/goblinSharpshooter.png", 3, 0, 4, 2, 3, "Civilized", "", "Blackthorn", "common", "Line of Sight",
     "Line of sight. Deals 2 damage at range 3 along an unobstructed rank."},
    {"Blackthorn Debt Collector", "Unit", "cards/blackthornDebtCollector.png", 3, 0, 5, 2, 1, "Civilized", "", "Blackthorn", "common", "Collect",
     "When Debt Collector destroys a unit, gain 2 coins."},
    {"Blackthorn Alchemist", "Unit", "cards/blackthornAlchemist.png", 4, 0, 5, 2, 2, "Mechanical", "", "Blackthorn", "rare", "Reagent",
     "Ability: deal 2 damage to a unit at range 2 and 1 to yourself."},
    {"Blackthorn Foreman", "Unit", "cards/blackthornForeman.png", 5, 0, 8, 3, 1, "Mechanical", "", "Blackthorn", "rare", "Tax",
     "Tax 1. Gain 1 extra coin at the start of each of your turns."},
    {"Grask", "Unit", "cards/Grask.png", 7, 0, 11, 6, 1, "Mechanical", "", "Blackthorn", "legendary", "Siege",
     "Siege. Grask cannot be blocked by structures and destroys them on contact."},
};

// The board draws standing tokens from assets/characters, not card art, so a
// captured piece falls back to a placeholder without one. Token art almost
// always shares its basename with the card art; these are the ones that do not.
std::string boardTokenFor(std::string_view art)
{
    struct Exception
    {
        std::string_view art;
        std::string_view token;
    };
    static constexpr Exception Exceptions[] = {
        {"cards/crystalUnicorn.png", "characters/crystallineUnicorn.png"},
        {"cards/nettleStarbright.png", "characters/nettleStarbright_unmounted.png"},
        {"cards/heartwood.png", "characters/heartwoodTree.png"},
        {"cards/mirewatchInformant.png", "characters/resistanceInformant.png"},
    };

    std::string candidate;
    for (const Exception& exception : Exceptions)
    {
        if (art == exception.art)
        {
            candidate = std::string(exception.token);
            break;
        }
    }
    if (candidate.empty())
    {
        const std::size_t slash = art.rfind('/');
        if (slash == std::string_view::npos)
        {
            return {};
        }
        candidate = "characters/" + std::string(art.substr(slash + 1));
    }

    // Spells have no token; only claim one that is actually on disk.
    const std::optional<std::filesystem::path> resolved = resolveAssetPath(candidate);
    std::error_code error;
    if (!resolved || !std::filesystem::exists(*resolved, error))
    {
        return {};
    }
    return candidate;
}

card_data::Card makeCard(const SampleCard& source)
{
    card_data::Card card;
    card.title = source.title;
    card.type = source.type;
    card.imagePath = source.art;
    // Canonical trait first so the collection's trait filters bite; the court
    // second so a row still reads as belonging somewhere.
    card.traits.emplace_back(source.trait);
    if (source.trait2 && *source.trait2)
    {
        card.traits.emplace_back(source.trait2);
    }
    card.traits.emplace_back(source.court);
    card.keywords.emplace_back(source.keyword);
    card.integerValues.push_back({"cost", source.cost});
    if (source.heroCost > 0)
    {
        card.integerValues.push_back({"heroCost", source.heroCost});
    }
    card.integerValues.push_back({"health", source.health});
    card.integerValues.push_back({"attack", source.attack});
    card.integerValues.push_back({"attackRange", std::max(1, source.range)});
    card.integerValues.push_back({"moveRange", 1});
    // toGameCard synthesizes a move/attack action pair from these legacy keys
    // when a card carries no explicit action list, which is how these samples
    // acquire the ranges the board highlights are drawn from.
    card.integerValues.push_back({"move", 2});
    card.integerValues.push_back({"range", std::max(1, source.range)});
    // Named explicitly, because the synthesized fallback names read as debug
    // strings ("Bog Spearman Move") in the inspect popup.
    if (std::string_view(source.type) == "Unit" || std::string_view(source.type) == "Hero")
    {
        card_data::Action advance;
        advance.name = "Advance";
        advance.pattern = "omni";
        advance.minRange = 1;
        advance.maxRange = 2;
        advance.canMove = true;
        advance.canAttack = false;
        card.actions.push_back(advance);
        card.actionNames.emplace_back(advance.name);
        card.actionDisplayNames.emplace_back("Advance");

        if (source.attack > 0)
        {
            card_data::Action strike;
            strike.name = source.range > 1 ? "Loose" : "Strike";
            strike.kind = "ranged";
            strike.pattern = "omni";
            strike.minRange = 1;
            strike.maxRange = std::max(1, source.range);
            strike.damage = source.attack;
            strike.canMove = false;
            strike.canAttack = true;
            strike.lineOfSight = true;
            card.actions.push_back(strike);
            card.actionNames.emplace_back(strike.name);
            card.actionDisplayNames.emplace_back(strike.name);
        }
    }
    card.integerValues.push_back({"width", 1});
    card.integerValues.push_back({"height", 1});
    card.stringValues.push_back({"rarity", source.rarity});
    card.stringValues.push_back({"description", source.text});
    card.stringValues.push_back({"movePattern", "omni"});
    if (const std::string token = boardTokenFor(source.art); !token.empty())
    {
        card.stringValues.push_back({"Token", token});
    }
    return card;
}

// Titles of a court's non-hero cards, cheapest first, so a fabricated deck has a
// plausible curve rather than the same four cards repeated.
std::vector<std::string> courtCards(
    const std::vector<card_data::Card>& library,
    std::string_view court,
    bool heroes)
{
    std::vector<std::string> titles;
    for (const SampleCard& source : SampleCards)
    {
        if (source.court != court)
        {
            continue;
        }
        const bool isHero = source.heroCost > 0;
        if (isHero != heroes)
        {
            continue;
        }
        const auto found = std::find_if(library.begin(), library.end(), [&](const card_data::Card& card) {
            return card.title == source.title;
        });
        if (found != library.end())
        {
            titles.emplace_back(source.title);
        }
    }
    return titles;
}

// Two copies apiece of the cheapest uniques until the deck holds exactly
// game_data::DeckCardCount non-hero cards, plus the named heroes.
deck_data::Deck buildDeck(
    const std::vector<card_data::Card>& library,
    const std::string& name,
    std::string_view court,
    std::size_t heroCount,
    int nonHeroCards)
{
    deck_data::Deck deck;
    deck.name = name;

    const std::vector<std::string> heroes = courtCards(library, court, true);
    for (std::size_t i = 0; i < heroCount && i < heroes.size(); ++i)
    {
        deck.cardTitles.push_back(heroes[i]);
    }

    const std::vector<std::string> cards = courtCards(library, court, false);
    int placed = 0;
    for (std::size_t pass = 0; pass < 2 && placed < nonHeroCards; ++pass)
    {
        for (const std::string& title : cards)
        {
            if (placed >= nonHeroCards)
            {
                break;
            }
            deck.cardTitles.push_back(title);
            ++placed;
        }
    }
    return deck;
}

} // namespace

const std::vector<std::string>& knownScreens()
{
    static const std::vector<std::string> screens = {
        "title-screen",
        "login",
        "login-error",
        "create-account",
        "create-account-invalid",
        "options",
        "options-audio",
        "options-account",
        "main-menu",
        "main-menu-hover",
        "main-menu-exit",
        "deck-select",
        "matchmaking",
        "deck-editor",
        "deck-editor-cards",
        // Collection states that are otherwise only reachable by clicking through
        // a live account: a finished deck, an empty roster, the inspect popup and
        // the unsaved-changes dialog.
        "deck-editor-full",
        "deck-editor-empty",
        "deck-editor-popup",
        "deck-editor-unsaved",
        "shop",
        "shop-reveal",
        "starter-decks",
        "starter-decks-pick",
        "admin-users",
        "admin-tools",
        "card-editor",
        "conquest",
        "game",
        // Match states. "game" only reaches the one-piece story tutorial, which
        // shows almost nothing of the board that players actually stare at.
        "game-midgame",
        "game-selected",
        "game-popup",
        "game-popup-tooltip",
        "game-resign-confirmation",
        "game-victory",
        // Admin / card-editor / Conquest review states. These screens are all
        // but empty without a service behind them, so each key seeds the state
        // that makes its layout worth looking at.
        "admin-users-selected",
        "admin-users-popup",
        "card-editor-loaded",
        "conquest-events",
        "conquest-map",
        "conquest-loadouts"};
    return screens;
}

std::optional<Request> parseCommandLine(int argc, char** argv)
{
    Request request;
    bool requested = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view argument(argv[i] ? argv[i] : "");
        if (startsWith(argument, "--ui-capture="))
        {
            request.outputDirectory = std::string(argument.substr(std::string_view("--ui-capture=").size()));
            requested = true;
        }
        else if (startsWith(argument, "--ui-capture-screens="))
        {
            request.screens =
                splitList(argument.substr(std::string_view("--ui-capture-screens=").size()));
        }
        else if (startsWith(argument, "--ui-capture-size="))
        {
            const std::string value(argument.substr(std::string_view("--ui-capture-size=").size()));
            const std::size_t separator = value.find('x');
            if (separator != std::string::npos)
            {
                const int width = std::atoi(value.substr(0, separator).c_str());
                const int height = std::atoi(value.substr(separator + 1).c_str());
                if (width > 0 && height > 0)
                {
                    request.width = static_cast<unsigned int>(width);
                    request.height = static_cast<unsigned int>(height);
                }
            }
        }
        else if (startsWith(argument, "--ui-capture-warmup="))
        {
            const int frames =
                std::atoi(std::string(argument.substr(std::string_view("--ui-capture-warmup=").size())).c_str());
            request.warmupFrames = std::clamp(frames, 1, 600);
        }
    }

    if (!requested)
    {
        return std::nullopt;
    }

    if (request.screens.empty())
    {
        request.screens = knownScreens();
    }
    return request;
}

bool saveWindow(const sf::RenderWindow& window, const std::filesystem::path& path)
{
    const sf::Vector2u size = window.getSize();
    if (size.x == 0 || size.y == 0)
    {
        return false;
    }

    sf::Texture texture;
    if (!texture.resize(size))
    {
        return false;
    }
    texture.update(window);

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    return texture.copyToImage().saveToFile(path);
}

std::vector<card_data::Card> sampleCardLibrary()
{
    std::vector<card_data::Card> library;
    library.reserve(std::size(SampleCards));
    for (const SampleCard& source : SampleCards)
    {
        library.push_back(makeCard(source));
    }
    return library;
}

std::vector<deck_data::Deck> sampleDecks(const std::vector<card_data::Card>& library)
{
    // Three decks at deliberately different stages, so one capture run shows a
    // finished deck, a half-built one and a barely-started one.
    std::vector<deck_data::Deck> decks;
    decks.push_back(buildDeck(library, "Seelie Court Tempo", "Seelie", 2, game_data::DeckCardCount));
    decks.push_back(buildDeck(library, "Mirewatch Attrition", "Mirewatch", 1, 13));
    decks.push_back(buildDeck(library, "Blackthorn Toll Road", "Blackthorn", 0, 7));
    return decks;
}

deck_data::Deck sampleLegalDeck(const std::vector<card_data::Card>& library)
{
    return buildDeck(library, "Seelie Court Tempo", "Seelie", 2, game_data::DeckCardCount);
}

std::vector<account_data::CollectionCard> sampleCollection(
    const std::vector<card_data::Card>& library)
{
    std::vector<account_data::CollectionCard> collection;
    collection.reserve(library.size());
    // Mostly playsets, so a fabricated deck is legal against the collection, with
    // a few singletons so the "at deck limit" and "only one held" states are both
    // visible in a capture.
    int step = 0;
    for (const card_data::Card& card : library)
    {
        const int copies = step % 7 == 3 ? 1 : (step % 3 == 0 ? 3 : 2);
        collection.push_back({card.title, copies});
        ++step;
    }
    return collection;
}

} // namespace bayou::client::ui_capture
