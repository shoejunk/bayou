#include "client_story.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace bayou::client
{
namespace
{

constexpr std::array<StoryMission, 8> Missions = {{
    {
        "The Pearl Above the Stage",
        "Book One, Chapter 1",
        "Selection and movement",
        "Guide Braun across both marked squares.",
        "Select Braun, then choose a glowing destination. Moving hands the next action to Mirewatch, just as it does in a match.",
        {{{"Narrator",
           "Before Mirewatch can resist the Company, you must understand the force that owns its roads, wages, and weapons.",
           "cards/braunStonefist.png"},
          {"Victor Greyshard",
           "Blackthorn doctrine begins with position. A strong piece in the wrong square is only an expensive obstruction.",
           "cards/victorGreyshard.png"},
          {"Braun Stonefist",
           "Then mark the route. I will show your clerk how a company soldier crosses the board.",
           "cards/braunStonefist.png"}}}
    },
    {
        "A Cage with a Bill of Sale",
        "Book One, Chapter 2",
        "Melee attacks and damage",
        "Destroy the harnessed bull gator.",
        "Select the Lumberjack, then select the adjacent enemy. After the strike, Mirewatch immediately gets one action to answer.",
        {{{"Narrator",
           "On the road to Mirewatch, Reed, Erevan, Telos, and Donella find that even the swamp's teeth wear Blackthorn harness.",
           "cards/bullGator.png"},
          {"Blackthorn Foreman",
           "The creature has broken its line. Put it down before the cargo route becomes a feeding ground.",
           "cards/blackthornForeman.png"},
          {"Blackthorn Lumberjack",
           "One square away. One clean swing. That is close enough for company work.",
           "cards/blackthornLumberjack.png"}}}
    },
    {
        "The Customs Bell",
        "Book One, Chapter 2",
        "Aim, fire, and lower weapon",
        "Aim the Goblin Sharpshooter, then stop the smuggler.",
        "Use Aim first. On a later Blackthorn action, fire down a clear rank; Lower Weapon returns the Sharpshooter to movement.",
        {{{"Donella of the Marsh",
           "The cage goes through the gate as cargo. No heroics. No one alone.",
           "cards/donellaOfTheMarsh.png"},
          {"Narrator",
           "The weight is wrong, the fungus is convincing, and then the customs bell begins to ring.",
           "cards/gildedCage.png"},
          {"Goblin Sharpshooter",
           "I do not need the whole truth. I only need an open rank and one target who moves too slowly.",
           "cards/goblinSharpshooter.png"}}}
    },
    {
        "The Town Beneath the Company",
        "Book One, Chapter 3",
        "Cards, deployment, and controlled ground",
        "Deploy the Debt Collector on the marked home square.",
        "Select the card in hand, then choose the glowing square your hero controls.",
        {{{"Narrator",
           "Mirewatch's pumps, cranes, ferries, and wages all lead back to the same black thorn stamped on every ledger.",
           "cards/blackthornDebtCollector.png"},
          {"Victor Greyshard",
           "A board is not owned by standing on every square. It is owned by deciding who may stand there next.",
           "cards/victorGreyshard.png"},
          {"Blackthorn Debt Collector",
           "Give me one controlled doorstep and I will turn it into an office.",
           "cards/blackthornDebtCollector.png"}}}
    },
    {
        "Terms and Conditions",
        "Book One, Chapter 4",
        "Activated powers, stealth, and summoning",
        "Hide the Ambusher, create a Lumberjack, and control fourteen squares.",
        "Select each specialist to reveal its real contextual power. Using a power normally hands the next action to Mirewatch.",
        {{{"Narrator",
           "In Mirewatch, a contract can be a wall, a lock, or a weapon. The ink matters less than who controls the room.",
           "cards/blackthornForeman.png"},
          {"Blackthorn Foreman",
           "Lumberjack left. Ambusher right. Every square between them becomes a condition nobody remembers agreeing to.",
           "cards/goblinAmbusher.png"},
          {"Reed Baelstone",
           "Blackthorn calls it order when everyone else is too boxed in to move.",
           "cards/reedBaelstone.png"}}}
    },
    {
        "Hospitality Is a Weapon",
        "Book One, Chapter 5",
        "Combined arms",
        "Defeat both Mirewatch defenders.",
        "Use the Ambusher up close and preserve the Sharpshooter's clear firing lanes.",
        {{{"Narrator",
           "A safe room, a offered drink, a polite question: Mirewatch survives by making every kindness carry two meanings.",
           "cards/mirewatchInformant.png"},
          {"Victor Greyshard",
           "Do not chase the first body you see. Close the exits, keep your firing lane, and make hospitality expensive.",
           "cards/victorGreyshard.png"},
          {"Mirewatch Informant",
           "The Company taught us to count doors. It forgot to ask who built the windows.",
           "cards/mirewatchInformant.png"}}}
    },
    {
        "The Bluewater Below",
        "Book One, Chapter 6",
        "Priority targets and protection",
        "Break through the escort and defeat Donella.",
        "Remove defenders that block your attacks, then concentrate damage on the marked target.",
        {{{"Narrator",
           "Below the streets, Bluewater routes carry people and secrets where Blackthorn maps insist there is only mud.",
           "cards/donellaOfTheMarsh.png"},
          {"Donella of the Marsh",
           "A route is a promise. Hold long enough and everyone behind you gets to keep moving.",
           "cards/donellaOfTheMarsh.png"},
          {"Braun Stonefist",
           "Then the escort falls first. After that, the promise stands alone.",
           "cards/braunStonefist.png"}}}
    },
    {
        "How to Rob an Office Everyone Is Watching",
        "Book One, Chapter 7",
        "Full tactical encounter",
        "Defeat Reed, Erevan, and every remaining defender.",
        "Use the whole Blackthorn force: screen ranged units, attack weak targets, and control approach squares.",
        {{{"Reed Baelstone",
           "A watched office is not impossible to rob. It only means the audience must be watching the wrong crime.",
           "cards/reedBaelstone.png"},
          {"Erevan the Shadow",
           "You make the plan. I will make the part where the plan stops being polite.",
           "cards/erevanTheShadow.png"},
          {"Victor Greyshard",
           "Enough exercises. Recover what was taken and teach Mirewatch what ownership means.",
           "cards/victorGreyshard.png"}}}
    }
}};

std::string storyProgressKey(std::string_view username)
{
    std::string key;
    key.reserve(username.size());
    for (const unsigned char character : username)
    {
        if (std::isalnum(character) || character == '-' || character == '_')
        {
            key.push_back(static_cast<char>(std::tolower(character)));
        }
        else
        {
            key.push_back('_');
        }
    }
    return key.empty() ? "local" : key;
}

std::filesystem::path storyProgressPath(std::string_view username)
{
#ifdef _WIN32
    if (const char* appData = std::getenv("APPDATA"); appData && *appData)
    {
        return std::filesystem::path(appData) / "SteamTactics" / "story_progress" /
            (storyProgressKey(username) + ".cfg");
    }
#else
    if (const char* home = std::getenv("HOME"); home && *home)
    {
        return std::filesystem::path(home) / ".config" / "SteamTactics" / "story_progress" /
            (storyProgressKey(username) + ".cfg");
    }
#endif
    return std::filesystem::path("story_progress") / (storyProgressKey(username) + ".cfg");
}

} // namespace

const std::array<StoryMission, 8>& storyMissions()
{
    return Missions;
}

int loadStoryCompletedCount(std::string_view username)
{
    std::ifstream stream(storyProgressPath(username));
    int completed = 0;
    if (!(stream >> completed))
    {
        return 0;
    }
    return std::clamp(completed, 0, static_cast<int>(Missions.size()));
}

bool saveStoryCompletedCount(std::string_view username, int completedCount)
{
    const std::filesystem::path path = storyProgressPath(username);
    std::error_code error;
    if (path.has_parent_path())
    {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
        {
            return false;
        }
    }

    std::ofstream stream(path, std::ios::trunc);
    stream << std::clamp(completedCount, 0, static_cast<int>(Missions.size())) << '\n';
    return static_cast<bool>(stream);
}

} // namespace bayou::client
