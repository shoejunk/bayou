#pragma once

#include "../shared/game_data.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bayou::client
{

enum class StoryCampaign
{
    Blackthorn,
    Mirewatch,
    Seelie
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
    // Move/Attack lessons name the exact printed profile the player must
    // select. An empty, missing, or ambiguous name fails closed.
    std::string_view expectedActionProfile;
    // Ability lessons bind both the normalized engine ability and its printed
    // label. Summon and Transform additionally name the result that must be
    // produced, so a live-card drift cannot satisfy the tutorial accidentally.
    std::string_view expectedAbility;
    std::string_view expectedAbilityLabel;
    std::string_view expectedSummonTitle;
    std::string_view expectedNextAbilityLabel;
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
    // A commissioned mission-level illustration may replace the per-panel
    // character cards in both briefing and in-mission story layouts.
    std::string_view scenarioArtPath;
    std::vector<StoryPanel> aftermath;
    std::vector<StoryPiecePlacement> pieces;
    std::vector<std::string_view> playerHand;
    std::vector<std::string_view> enemyHand;
    // Draw piles are written from bottom to top. The last title is drawn first.
    std::vector<std::string_view> playerDrawPile;
    std::vector<std::string_view> enemyDrawPile;
    // A standard-match capstone submits these complete decks through the
    // ordinary engine setup path, then enters real HeroPlacement and begins
    // with the normal shuffled four-card hands. It never calls loadScenario.
    std::vector<std::string_view> playerDeck;
    std::vector<std::string_view> enemyDeck;
    bool standardMatch = false;
    int playerResources = 12;
    int enemyResources = 12;
    int firstPlayer = 1;
    StoryObjective objectiveSpec;
    std::vector<std::string_view> requiredSurvivorRoles;
    std::vector<StoryScriptAction> script;
    // A mastery stamp is awarded only when this mission completes the
    // cumulative legal evidence for the named card/rule. Story-only chapters
    // never award stamps.
    std::vector<std::string_view> masteryCards;
    std::vector<std::string_view> masteryRules;
    // Some cards create a dependent unit whose own printed profiles are part
    // of this mission's mastery promise. Keep that stronger promise explicit;
    // a passive reference alone does not imply every dependent profile was taught.
    bool masteryIncludesDependentProfiles = false;
    // Optional rehearsals expose real card rules on an explicitly non-canon
    // practice board. Players may read the story setup and skip the drill
    // without blocking the campaign.
    bool optionalRehearsal = false;
    // A required synthesis can serve as catch-up for players who continued
    // past earlier optional rehearsals. When every earlier optional rehearsal
    // was instead completed through play, the client may offer a no-repetition
    // bypass backed by that prior mastery evidence.
    bool catchUpForSkippedRehearsals = false;
    // Long synthesis missions may keep their real End Turn timing in the
    // authored script while advancing those routine player passes without an
    // extra click. Opponent actions remain automatic for every guided mission.
    bool autoResolvePlayerEndTurns = false;
};

struct StoryEnemyProgress
{
    int remaining = 0;
    bool complete = false;
};

// Packaged Story cards are reviewed offline fixtures, not a second authority.
// Callers must opt in explicitly when running UI capture or an offline test.
enum class StoryCardResolutionMode : std::uint8_t
{
    AuthoritativeOnly,
    AllowPackagedFixtureFallback
};

struct StoryCardDependencyReport
{
    // Unique titles in deterministic discovery order. Direct mission cards
    // come first, followed by recursive Summon, Rebirth, and Infest targets.
    std::vector<std::string> requiredTitles;
    std::vector<std::string> missingTitles;

    [[nodiscard]] bool complete() const noexcept
    {
        return missingTitles.empty();
    }
};

std::optional<game_data::GameCard> resolveStoryCardDefinition(
    std::string_view title,
    std::span<const card_data::Card> primaryAuthoritativeCards,
    std::span<const card_data::Card> secondaryAuthoritativeCards,
    StoryCardResolutionMode mode);
StoryCardDependencyReport validateStoryCardDependencies(
    const StoryMission& mission,
    std::span<const card_data::Card> primaryAuthoritativeCards,
    std::span<const card_data::Card> secondaryAuthoritativeCards,
    StoryCardResolutionMode mode);
std::optional<int> storyExpectedActionProfileIndex(
    const StoryScriptAction& step,
    const game_data::Piece& actor);
bool storySelectedActionProfileMatches(
    const StoryScriptAction& step,
    const game_data::Piece& actor,
    int selectedActionIndex);
bool storySelectedPieceActionMatches(
    const StoryScriptAction& step,
    StoryActionKind selectedSourceKind,
    const game_data::Piece& actor,
    int selectedActionIndex);
bool storyAbilityStepMatches(
    const StoryScriptAction& step,
    const game_data::Piece& actor);
bool storyAbilityOutcomeMatches(
    const StoryScriptAction& step,
    const game_data::Piece& actorBefore,
    std::span<const game_data::Piece> piecesAfter,
    int commandingPieceIdAfter);
bool storyCompletionMayBeAwarded(
    bool objectiveStage,
    bool objectiveFailed,
    int winner);
bool storyScriptActionAutoResolves(
    const StoryMission& mission,
    const StoryScriptAction& step) noexcept;
bool storyScriptActionRequiresPlayerInput(
    const StoryMission& mission,
    const StoryScriptAction& step) noexcept;
int storyGuidedPlayerActionCount(const StoryMission& mission) noexcept;
int storyGuidedPlayerActionNumber(
    const StoryMission& mission,
    int scriptStep) noexcept;
bool storyScriptProgressAllowsCompletion(
    const StoryMission& mission,
    int nextScriptStep) noexcept;

std::string_view storyCampaignName(StoryCampaign campaign);
std::string_view storyActName(StoryCampaign campaign, int missionIndex);
std::string_view storyActGoal(StoryCampaign campaign, int missionIndex);
std::string_view storyActShortGoal(StoryCampaign campaign, int missionIndex);
std::string storyRequiredSurvivorNames(const StoryMission& mission);
int storyAiSearchDepth(StoryCampaign campaign, std::string_view missionId);
std::span<const StoryMission> storyMissions(StoryCampaign campaign);
StoryEnemyProgress storyDefeatAllEnemiesProgress(
    const StoryMission& mission,
    std::span<const std::pair<std::string_view, int>> rolePieceIds,
    std::span<const game_data::Piece> authoritativePieces);
bool storyPlayerForcePresent(
    const StoryMission& mission,
    std::span<const std::pair<std::string_view, int>> rolePieceIds,
    std::span<const game_data::Piece> authoritativePieces);
struct StoryProgress
{
    int advancedCount = 0;
    // True when the entry carries real play evidence: either its authored play
    // path was completed, or a catch-up synthesis was bypassed because all of
    // its prerequisite rehearsals were already completed through play. A
    // defeat/ordinary skip continuation advances without setting this bit.
    std::vector<bool> completedByPlay;

    [[nodiscard]] bool continuedWithoutMastery(int missionIndex) const noexcept
    {
        return missionIndex >= 0 && missionIndex < advancedCount &&
            static_cast<std::size_t>(missionIndex) < completedByPlay.size() &&
            !completedByPlay[static_cast<std::size_t>(missionIndex)];
    }
};

bool storyMasteryCatchUpMayBeSkipped(
    std::span<const StoryMission> missions,
    int missionIndex,
    const StoryProgress& progress) noexcept;

StoryProgress loadStoryProgress(std::string_view username, StoryCampaign campaign);
bool recordStoryMissionProgress(
    std::string_view username,
    StoryCampaign campaign,
    int missionIndex,
    bool completedByPlay);
int loadStoryCompletedCount(std::string_view username, StoryCampaign campaign);
bool saveStoryCompletedCount(
    std::string_view username,
    StoryCampaign campaign,
    int completedCount);

} // namespace bayou::client
