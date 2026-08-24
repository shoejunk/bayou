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

constexpr std::array<StoryMission, 8> BlackthornMissions = {{
    {
        "The East Gate Ledger",
        "Book One, Chapter 2",
        "Selection and movement",
        "Guide Braun across both marked squares.",
        "Select Braun, then choose a glowing destination. Each move gives Mirewatch one action, exactly as it would in a match.",
        {{{"Blackthorn Dispatch",
           "The east-gate clerk recorded Reed, Donella, Erevan, and Telos before the second customs bell finished ringing.",
           "cards/braunStonefist.png"},
          {"Victor Greyshard",
           "A description becomes a route. A route becomes an interception. Put Stonefist where the ledger says they must pass.",
           "cards/victorGreyshard.png"},
          {"Braun Stonefist",
           "Mark the crossings. I will close the distance one company step at a time.",
           "cards/braunStonefist.png"}}}
    },
    {
        "The Town Beneath the Company",
        "Book One, Chapter 3",
        "Cards, deployment, and controlled ground",
        "Deploy the Debt Collector on the marked home square.",
        "Select the card in hand, then choose the glowing square your hero controls.",
        {{{"Narrator",
           "Blackthorn's silver tree marks Mirewatch's pumps, cranes, ferries, wage windows, and every boardwalk worth claiming.",
           "cards/blackthornDebtCollector.png"},
          {"Victor Greyshard",
           "Ownership is not standing everywhere. It is deciding who may stand there next.",
           "cards/victorGreyshard.png"},
          {"Blackthorn Debt Collector",
           "Give me a controlled doorstep. I will give it a number, a balance, and an office.",
           "cards/blackthornDebtCollector.png"}}}
    },
    {
        "Terms and Conditions",
        "Book One, Chapter 4",
        "Activated powers, stealth, and summoning",
        "Hide the Ambusher, create a Lumberjack, and control fourteen squares.",
        "Select each specialist to reveal its real contextual power. Using a power normally hands the next action to Mirewatch.",
        {{{"Narrator",
           "Erevan enters a claims office in a stolen coat. Blackthorn answers the intrusion by making every question leave a name.",
           "cards/blackthornForeman.png"},
          {"Blackthorn Foreman",
           "Ambusher on the unseen route. Lumberjack at the exit. Let procedure close the space between them.",
           "cards/goblinAmbusher.png"},
          {"Reed Baelstone",
           "They call it a condition when the only other choice has already been removed.",
           "cards/reedBaelstone.png"}}}
    },
    {
        "The Freight Office",
        "Book One, Chapter 7",
        "Aim, fire, and lower weapon",
        "Aim the Goblin Sharpshooter, then stop the informant.",
        "Use Aim first. Fire on a later action down a clear rank; Lower Weapon restores movement when the lane is gone.",
        {{{"Victor Greyshard",
           "They are not robbing coin. They are taking names, routes, and the proof that turns private theft into a public pattern.",
           "cards/victorGreyshard.png"},
          {"Goblin Sharpshooter",
           "Rain changes the sightline, not the order. Raise, hold, and fire when the rank clears.",
           "cards/goblinSharpshooter.png"},
          {"Reed Baelstone",
           "A watched office can still be robbed. The audience only needs to watch the wrong crime.",
           "cards/reedBaelstone.png"}}}
    },
    {
        "The Cost of Being Seen",
        "Book One, Chapter 8",
        "Combined arms",
        "Defeat both Mirewatch defenders.",
        "Use the Ambusher up close while preserving the Sharpshooter's clear firing lanes.",
        {{{"Narrator",
           "Grask brings a seizure wagon to the Mudfen bakery while receipt books and crossbows close the canal walk.",
           "cards/Grask.png"},
          {"Grask",
           "The notice names the occupants. The wagon settles the balance. Keep the road narrow.",
           "cards/Grask.png"},
          {"Vanya Bluewater",
           "A list of names is not consent. Hold the room, move the families, and make the Company spend every step.",
           "cards/vanyaBluewater.png"}}}
    },
    {
        "Making Credit",
        "Book One, Chapter 17",
        "Priority targets and protection",
        "Break through the escort and defeat Donella.",
        "Remove defenders that block Braun's approach, then concentrate damage on Donella.",
        {{{"Narrator",
           "Seven days before Charter Day, Mirewatch turns soup, ferries, burial silver, and testimony into a mutual society.",
           "cards/braunStonefist.png"},
          {"Braun Stonefist",
           "Their ledgers move by kitchen and ferry. Break the route before the courthouse can call it a claimant.",
           "cards/braunStonefist.png"},
          {"Donella of the Marsh",
           "Braun carried the message. He did not know what the eight teeth would make of the people who answered it.",
           "cards/donellaOfTheMarsh.png"}}}
    },
    {
        "Title Follows Burden",
        "Book One, Chapter 19",
        "Hero pressure and screening",
        "Defeat Reed before the Company line collapses.",
        "Screen Victor with Braun and Grask, then remove the defenders protecting Reed.",
        {{{"Victor Greyshard",
           "A charter is only paper until someone bears its burden. Make Mirewatch prove who will pay for every promise.",
           "cards/victorGreyshard.png"},
          {"Braun Stonefist",
           "The square is crowded. Give me the route and keep Greyshard behind it.",
           "cards/braunStonefist.png"},
          {"Reed Baelstone",
           "The Society can argue over the rule I broke after it survives the men who wrote the rule for us.",
           "cards/reedBaelstone.png"}}}
    },
    {
        "Victor Greyshard's Natural Order",
        "Book One, Chapter 29",
        "Full tactical encounter",
        "Defeat Reed and every remaining defender.",
        "Use the entire Company line: protect Victor, preserve firing lanes, and focus exposed resistance units.",
        {{{"Narrator",
           "At the World Tree, Victor has one man, one sword, and one road Reed cannot see. Grask still holds the axe.",
           "cards/victorGreyshard.png"},
          {"Grask",
           "The engine is broken. The register is not. Keep them from the road long enough for the record to leave.",
           "cards/Grask.png"},
          {"Victor Greyshard",
           "Natural order is what remains when every rival claim has been made too costly to carry.",
           "cards/victorGreyshard.png"}}}
    }
}};

constexpr std::array<StoryMission, 8> MirewatchMissions = {{
    {
        "A Cage with a Bill of Sale",
        "Book One, Chapter 2",
        "Selection and movement",
        "Guide Reed across both marked squares.",
        "Select Reed, then choose a glowing destination. Blackthorn receives one action after each move.",
        {{{"Narrator",
           "A harnessed crocodile strikes Telos's freight skiff. Beneath sailcloth, the stolen Gilded Hold begins to sing.",
           "cards/gildedCage.png"},
          {"Donella of the Marsh",
           "The cage reaches Mirewatch as cargo. No heroics, and no one travels alone.",
           "cards/donellaOfTheMarsh.png"},
          {"Reed Baelstone",
           "The bells already have our description. Show me the crossing they have not closed yet.",
           "cards/reedBaelstone.png"}}}
    },
    {
        "The Town Beneath the Company",
        "Book One, Chapter 3",
        "Reach attacks and damage",
        "Destroy the Debt Collector blocking the boardwalk.",
        "Use the Bog Spearman's normal action from two squares away, then prepare for Blackthorn's reply.",
        {{{"Narrator",
           "The customs bell follows Reed into a town where Blackthorn owns the pumps, cranes, wages, and better timber.",
           "cards/reedBaelstone.png"},
          {"Mirewatch Informant",
           "The boardwalk narrows at the office. The collector thinks the bottleneck belongs to him.",
           "cards/mirewatchInformant.png"},
          {"Bog Spearman",
           "Then I do not enter his reach. I make him learn mine.",
           "cards/bogSpearman.png"}}}
    },
    {
        "Terms and Conditions",
        "Book One, Chapter 4",
        "Cards, deployment, and controlled ground",
        "Deploy the Mirewatch Informant on the marked square.",
        "Select the card in hand, then choose the glowing square Reed controls.",
        {{{"Erevan the Shadow",
           "The claims office records every question now. We need someone who can enter without asking one.",
           "cards/erevanTheShadow.png"},
          {"Reed Baelstone",
           "A controlled square is more than ground. It is a place where another pair of eyes can safely arrive.",
           "cards/reedBaelstone.png"},
          {"Mirewatch Informant",
           "Give me the doorstep. I will tell you which ledger leaves through the back.",
           "cards/mirewatchInformant.png"}}}
    },
    {
        "Hospitality Is a Weapon",
        "Book One, Chapter 5",
        "Formation and protection",
        "Defeat the Ambusher and Debt Collector.",
        "Keep allies beside Donella to benefit from her protection, then let the Veteran hold the approach.",
        {{{"Maggie Mudroot",
           "Shelter is not payment for secrets. People eat before questions, and they choose what they owe.",
           "cards/maggieMudroot.png"},
          {"Donella of the Marsh",
           "Stay close. Blackthorn wins by separating one frightened answer from every person who could challenge it.",
           "cards/donellaOfTheMarsh.png"},
          {"Marshland Veteran",
           "Let the collectors reach us. This floor has held under worse weight.",
           "cards/marshlandVeteran.png"}}}
    },
    {
        "The Bluewater Below",
        "Book One, Chapter 6",
        "Mobility and priority targets",
        "Break the escort and defeat the Sharpshooter.",
        "Use the Tracker's mobility and Vanya's reach to deny the Sharpshooter a clear rank.",
        {{{"Birdie the Wise",
           "The route begins with boiled linen because a resistance road should look like ordinary work.",
           "cards/birdieTheWise.png"},
          {"Vanya Bluewater",
           "Birdie verifies the exit. Erevan judges the route. Reed opens nothing alone.",
           "cards/vanyaBluewater.png"},
          {"Swamp Tracker",
           "The Company watches the dry boards. Good. We will use the ground it refuses to count.",
           "cards/swampTracker.png"}}}
    },
    {
        "How to Rob an Office Everyone Is Watching",
        "Book One, Chapter 7",
        "Combined arms",
        "Defeat the Foreman and Debt Collector.",
        "Use the Spearman to open lanes while the Informant and Tracker pressure separate exits.",
        {{{"Reed Baelstone",
           "The office is watched because Victor expects a theft. We are taking the receiving marks that prove a system.",
           "cards/reedBaelstone.png"},
          {"Donella of the Marsh",
           "The names leave first. The crew leaves together. No plan makes Hara Dole expendable.",
           "cards/donellaOfTheMarsh.png"},
          {"Mirewatch Informant",
           "Give the window an audience. I will give the back door a different story.",
           "cards/mirewatchInformant.png"}}}
    },
    {
        "Title Follows Burden",
        "Book One, Chapter 19",
        "Priority targets and hero safety",
        "Defeat Braun while keeping Reed alive.",
        "Hold Reed behind Donella and Vanya, then focus the Company enforcer before Grask closes the square.",
        {{{"Narrator",
           "At third bell, Reed takes the witness seat under a name he has admitted is false. Mirewatch bears the claim itself.",
           "cards/reedBaelstone.png"},
          {"Donella of the Marsh",
           "The Society seal is cooling in the wax. Hold the square long enough for the town to own its answer.",
           "cards/donellaOfTheMarsh.png"},
          {"Braun Stonefist",
           "I have the route book. You have one chance to prove the line behind you is more than paper.",
           "cards/braunStonefist.png"}}}
    },
    {
        "A Town That Owns Itself",
        "Book One, Chapters 29-30",
        "Full tactical encounter",
        "Defeat Victor, Grask, and every remaining Company unit.",
        "Protect Reed, anchor the line around Donella, and concentrate attacks before Blackthorn can isolate a target.",
        {{{"Narrator",
           "The World Tree closes around the broken engine. Victor still has Grask, a sword, and one road Reed cannot see.",
           "cards/victorGreyshard.png"},
          {"Reed Baelstone",
           "Count the living first. Then close the road, keep the register here, and make every name answerable.",
           "cards/reedBaelstone.png"},
          {"Donella of the Marsh",
           "This is a choice, not a cure. Make the choice together and leave no one inside Blackthorn iron.",
           "cards/donellaOfTheMarsh.png"}}}
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

std::filesystem::path storyProgressPath(std::string_view username, StoryCampaign campaign)
{
    const std::string suffix = campaign == StoryCampaign::Blackthorn ? "" : "_mirewatch";
#ifdef _WIN32
    if (const char* appData = std::getenv("APPDATA"); appData && *appData)
    {
        return std::filesystem::path(appData) / "SteamTactics" / "story_progress" /
            (storyProgressKey(username) + suffix + ".cfg");
    }
#else
    if (const char* home = std::getenv("HOME"); home && *home)
    {
        return std::filesystem::path(home) / ".config" / "SteamTactics" / "story_progress" /
            (storyProgressKey(username) + suffix + ".cfg");
    }
#endif
    return std::filesystem::path("story_progress") /
        (storyProgressKey(username) + suffix + ".cfg");
}

} // namespace

std::string_view storyCampaignName(StoryCampaign campaign)
{
    return campaign == StoryCampaign::Blackthorn ? "The Blackthorns" : "The Mirewatch Resistance";
}

const std::array<StoryMission, 8>& storyMissions(StoryCampaign campaign)
{
    return campaign == StoryCampaign::Blackthorn ? BlackthornMissions : MirewatchMissions;
}

int loadStoryCompletedCount(std::string_view username, StoryCampaign campaign)
{
    std::ifstream stream(storyProgressPath(username, campaign));
    int completed = 0;
    if (!(stream >> completed))
    {
        return 0;
    }
    return std::clamp(completed, 0, static_cast<int>(BlackthornMissions.size()));
}

bool saveStoryCompletedCount(
    std::string_view username,
    StoryCampaign campaign,
    int completedCount)
{
    const std::filesystem::path path = storyProgressPath(username, campaign);
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
    stream << std::clamp(completedCount, 0, static_cast<int>(BlackthornMissions.size())) << '\n';
    return static_cast<bool>(stream);
}

} // namespace bayou::client
