#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace bayou::client
{

enum class StoryCampaign
{
    Blackthorn,
    Mirewatch
};

struct StoryPanel
{
    std::string_view speaker;
    std::string_view text;
    std::string_view artPath;
};

enum class StoryActionKind : std::uint8_t
{
    None,
    Move,
    Attack,
    UseAbility,
    PlayCard,
    DrawCard,
    ChooseForesight,
    DiscardCard,
    EndTurn
};

struct StoryPiecePlacement
{
    std::string_view role;
    std::string_view cardTitle;
    int owner = 1;
    int row = 0;
    int column = 0;
    bool isHero = false;
    int initialHealth = -1;
};

struct StoryScriptAction
{
    StoryActionKind kind = StoryActionKind::None;
    int owner = 1;
    std::string_view actorRole;
    std::string_view targetRole;
    std::string_view effectRole;
    std::string_view cardTitle;
    int targetRow = -1;
    int targetColumn = -1;
    std::string_view heading;
    std::string_view instruction;
    std::string_view correction;
    std::vector<StoryPanel> panelsBefore;
};

enum class StoryObjectiveKind : std::uint8_t
{
    Legacy,
    StoryOnly,
    Scripted,
    DefeatAllEnemies,
    DefeatRole,
    ReachSquare,
    DeployCard,
    ControlSquares
};

struct StoryObjective
{
    StoryObjectiveKind kind = StoryObjectiveKind::Legacy;
    std::string_view targetRole;
    std::string_view cardTitle;
    int targetRow = -1;
    int targetColumn = -1;
    int amount = 0;
};

struct StoryMission
{
    std::string_view title;
    std::string_view sourceChapter;
    std::string_view lesson;
    std::string_view objective;
    std::string_view hint;
    std::vector<StoryPanel> briefing;
    std::string_view id;
    std::vector<StoryPanel> aftermath;
    std::vector<StoryPiecePlacement> pieces;
    std::vector<std::string_view> playerHand;
    std::vector<std::string_view> enemyHand;
    // Draw piles are written from bottom to top. The last title is drawn first.
    std::vector<std::string_view> playerDrawPile;
    std::vector<std::string_view> enemyDrawPile;
    int playerResources = 12;
    int enemyResources = 12;
    int firstPlayer = 1;
    StoryObjective objectiveSpec;
    std::vector<std::string_view> requiredSurvivorRoles;
    std::vector<StoryScriptAction> script;
    // A mastery stamp is awarded only when the deterministic script legally
    // exercises the named card/rule. Story-only chapters never award stamps.
    std::vector<std::string_view> masteryCards;
    std::vector<std::string_view> masteryRules;
};

std::string_view storyCampaignName(StoryCampaign campaign);
std::span<const StoryMission> storyMissions(StoryCampaign campaign);
int loadStoryCompletedCount(std::string_view username, StoryCampaign campaign);
bool saveStoryCompletedCount(
    std::string_view username,
    StoryCampaign campaign,
    int completedCount);

} // namespace bayou::client
