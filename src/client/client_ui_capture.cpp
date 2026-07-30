#include "client_ui_capture.hpp"

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
    // Standing board token from assets/characters. The board renders tokens, not
    // card art, so without this every captured piece falls back to a placeholder.
    const char* token;
    int cost;
    int health;
    int attack;
    int range;
    const char* trait;
    const char* keyword;
    const char* text;
};

// A cross section of the real catalog: a hero, several units at different costs,
// a spell and a structure, so every card-bearing screen has something with the
// right shape to lay out.
constexpr SampleCard SampleCards[] = {
    {"Sylvara", "Hero", "cards/Sylvara.png", "characters/sylvara.png", 0, 22, 4, 1, "Seelie", "Regal",
     "While Sylvara holds a home square, friendly Seelie units gain +1 attack."},
    {"Nettle Starbright", "Hero", "cards/nettleStarbright.png",
     "characters/nettleStarbright_unmounted.png", 0, 20, 3, 2, "Seelie", "Swift",
     "Deploy: draw a card. Nettle may act again after a kill."},
    {"Thaeron Baelstone", "Hero", "cards/thaeronBaelstone.png", "characters/thaeronBaelstone.png",
     0, 24, 5, 1, "Unseelie", "Relentless",
     "Relentless. When Thaeron destroys a unit, he may move once more."},
    {"Duchess Dewbell", "Unit", "cards/duchessDewbell.png", "characters/duchessDewbell.png",
     4, 7, 3, 1, "Seelie", "Command",
     "Command: an adjacent friendly unit may act immediately after Dewbell."},
    {"Thorn Griffin", "Unit", "cards/thornGriffin.png", "characters/thornGriffin.png",
     5, 8, 4, 1, "Beast", "Flying",
     "Flying. Thorn Griffin ignores holes and may hop over other pieces."},
    {"Crystal Unicorn", "Unit", "cards/crystalUnicorn.png", "characters/crystallineUnicorn.png",
     6, 9, 4, 1, "Beast", "Ward",
     "Ward. The first time Crystal Unicorn would be damaged each turn, prevent it."},
    {"Gloom Fairy", "Unit", "cards/gloomFairy.png", "characters/gloomFairy.png",
     2, 3, 2, 2, "Unseelie", "Swift",
     "Swift. Gloom Fairy may move after attacking."},
    {"Blightling", "Unit", "cards/blightling.png", "characters/blightling.png",
     1, 2, 1, 1, "Unseelie", "Spore",
     "When Blightling dies, adjacent enemy units take 1 damage."},
    {"Bog Spearman", "Unit", "cards/bogSpearman.png", "characters/bogSpearman.png",
     3, 5, 3, 2, "Mirewatch", "Reach",
     "Reach. Bog Spearman strikes at range 2 without moving."},
    {"Marshland Veteran", "Unit", "cards/marshlandVeteran.png", "characters/marshlandVeteran.png",
     4, 6, 3, 1, "Mirewatch", "Steadfast",
     "Steadfast. Marshland Veteran cannot be pushed or stunned."},
    {"Goblin Sharpshooter", "Unit", "cards/goblinSharpshooter.png",
     "characters/goblinSharpshooter.png", 3, 4, 2, 3, "Blackthorn", "Line of Sight",
     "Line of sight. Deals 2 damage at range 3 along an unobstructed rank."},
    {"Blackthorn Foreman", "Unit", "cards/blackthornForeman.png", "characters/blackthornForeman.png",
     5, 8, 3, 1, "Blackthorn", "Tax",
     "Tax 1. Gain 1 extra coin at the start of each of your turns."},
    {"Heartwood Sister", "Unit", "cards/heartwoodSister.png", "characters/heartwoodSister.png",
     3, 5, 1, 1, "Seelie", "Mend",
     "Ability: heal an adjacent friendly unit for 3."},
    {"Archivist Mosswake", "Unit", "cards/archivistMosswake.png", "characters/archivistMosswake.png",
     4, 5, 2, 1, "Mirewatch", "Insight",
     "Deploy: look at the top two cards of your deck and keep one."},
    {"Erevan the Shadow", "Unit", "cards/erevanTheShadow.png", "characters/erevanTheShadow.png",
     6, 7, 5, 1, "Unseelie", "Ambush",
     "Ambush. Erevan deals double damage to units that have not yet acted."},
    {"Hidden Path", "Spell", "cards/hiddenPath.png", "", 2, 0, 0, 0, "Seelie", "Instant",
     "Move a friendly unit up to three squares. It may not attack this turn."},
    {"Heartshoot", "Spell", "cards/heartshoot.png", "", 3, 0, 0, 0, "Seelie", "Instant",
     "Deal 4 damage to a unit at range 3. Heal your hero for 2."},
    {"Heartwood", "Structure", "cards/heartwood.png", "characters/heartwoodTree.png",
     4, 12, 0, 0, "Seelie", "Grow",
     "Grow 2. After two turns Heartwood produces a Grove Sister on an adjacent square."},
};

card_data::Card makeCard(const SampleCard& source)
{
    card_data::Card card;
    card.title = source.title;
    card.type = source.type;
    card.imagePath = source.art;
    card.traits.emplace_back(source.trait);
    card.keywords.emplace_back(source.keyword);
    card.integerValues.push_back({"cost", source.cost});
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
            card.actionDisplayNames.emplace_back(strike.name);
        }
    }
    card.integerValues.push_back({"width", 1});
    card.integerValues.push_back({"height", 1});
    card.stringValues.push_back({"description", source.text});
    card.stringValues.push_back({"movePattern", "omni"});
    if (source.token[0] != '\0')
    {
        card.stringValues.push_back({"Token", source.token});
    }
    return card;
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
        "shop",
        "starter-decks",
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
        "game-victory"};
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
    const auto titlesOfType = [&](std::string_view type, std::size_t limit) {
        std::vector<std::string> titles;
        for (const card_data::Card& card : library)
        {
            if (card.type == type && titles.size() < limit)
            {
                // Two copies apiece, the way a real list reads.
                titles.push_back(card.title);
                titles.push_back(card.title);
            }
        }
        return titles;
    };

    std::vector<deck_data::Deck> decks;

    deck_data::Deck court;
    court.name = "Seelie Court Tempo";
    court.cardTitles = titlesOfType("Unit", 8);
    for (const std::string& spell : titlesOfType("Spell", 2))
    {
        court.cardTitles.push_back(spell);
    }
    decks.push_back(std::move(court));

    deck_data::Deck mire;
    mire.name = "Mirewatch Attrition";
    mire.cardTitles = titlesOfType("Unit", 6);
    decks.push_back(std::move(mire));

    deck_data::Deck thorn;
    thorn.name = "Blackthorn Toll Road";
    thorn.cardTitles = titlesOfType("Unit", 5);
    decks.push_back(std::move(thorn));

    return decks;
}

std::vector<account_data::CollectionCard> sampleCollection(
    const std::vector<card_data::Card>& library)
{
    std::vector<account_data::CollectionCard> collection;
    collection.reserve(library.size());
    int copies = 1;
    for (const card_data::Card& card : library)
    {
        collection.push_back({card.title, copies});
        copies = copies >= 3 ? 1 : copies + 1;
    }
    return collection;
}

} // namespace bayou::client::ui_capture
