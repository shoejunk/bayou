#include "client_story.hpp"
#include "client_story_cards.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>

namespace bayou::client
{
namespace
{

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
    const std::string suffix = campaign == StoryCampaign::Blackthorn
        ? ""
        : campaign == StoryCampaign::Mirewatch ? "_mirewatch" : "_seelie";
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

std::optional<game_data::GameCard> resolveStoryCardDefinition(
    std::string_view title,
    std::span<const card_data::Card> primaryAuthoritativeCards,
    std::span<const card_data::Card> secondaryAuthoritativeCards,
    StoryCardResolutionMode mode)
{
    const auto findAuthoritative = [title](std::span<const card_data::Card> cards)
        -> const card_data::Card* {
        const auto found = std::find_if(
            cards.begin(), cards.end(), [title](const card_data::Card& card) {
                return card.title == title;
            });
        return found == cards.end() ? nullptr : &*found;
    };

    if (const card_data::Card* card = findAuthoritative(primaryAuthoritativeCards))
    {
        return game_data::toGameCard(*card);
    }
    if (const card_data::Card* card = findAuthoritative(secondaryAuthoritativeCards))
    {
        return game_data::toGameCard(*card);
    }
    if (mode == StoryCardResolutionMode::AllowPackagedFixtureFallback)
    {
        return packagedStoryCard(title);
    }
    return std::nullopt;
}

StoryCardDependencyReport validateStoryCardDependencies(
    const StoryMission& mission,
    std::span<const card_data::Card> primaryAuthoritativeCards,
    std::span<const card_data::Card> secondaryAuthoritativeCards,
    StoryCardResolutionMode mode)
{
    StoryCardDependencyReport report;
    std::unordered_set<std::string> discovered;
    const auto require = [&](std::string_view title) {
        if (title.empty())
        {
            return;
        }
        std::string ownedTitle(title);
        if (discovered.insert(ownedTitle).second)
        {
            report.requiredTitles.push_back(std::move(ownedTitle));
        }
    };

    for (const StoryPiecePlacement& piece : mission.pieces)
    {
        require(piece.cardTitle);
    }
    for (std::string_view title : mission.playerHand)
    {
        require(title);
    }
    for (std::string_view title : mission.enemyHand)
    {
        require(title);
    }
    for (std::string_view title : mission.playerDrawPile)
    {
        require(title);
    }
    for (std::string_view title : mission.enemyDrawPile)
    {
        require(title);
    }
    for (std::string_view title : mission.playerDeck)
    {
        require(title);
    }
    for (std::string_view title : mission.enemyDeck)
    {
        require(title);
    }
    require(mission.objectiveSpec.cardTitle);
    for (const StoryScriptAction& step : mission.script)
    {
        // Scripted PlayCard/deployment steps can name a definition even when
        // the authored hand changes later, so treat every explicit title as a
        // direct dependency.
        require(step.cardTitle);
    }
    for (std::string_view title : mission.masteryCards)
    {
        require(title);
    }

    for (std::size_t index = 0; index < report.requiredTitles.size(); ++index)
    {
        const std::optional<game_data::GameCard> card = resolveStoryCardDefinition(
            report.requiredTitles[index],
            primaryAuthoritativeCards,
            secondaryAuthoritativeCards,
            mode);
        if (!card)
        {
            report.missingTitles.push_back(report.requiredTitles[index]);
            continue;
        }

        // These are the only title-based definitions the engine can create
        // after scenario load: Summon (including Trail), Rebirth, and Infest.
        require(card->summonTitle);
        require(card->rebirthTitle);
        for (const game_data::ActionProfile& action : card->actions)
        {
            require(action.infest);
        }
    }
    return report;
}

std::optional<int> storyExpectedActionProfileIndex(
    const StoryScriptAction& step,
    const game_data::Piece& actor)
{
    if ((step.kind != StoryActionKind::Move && step.kind != StoryActionKind::Attack) ||
        step.expectedActionProfile.empty())
    {
        return std::nullopt;
    }

    std::optional<int> expectedIndex;
    for (std::size_t index = 0; index < actor.actions.size(); ++index)
    {
        const game_data::ActionProfile& profile = actor.actions[index];
        if (profile.state != actor.actionState ||
            profile.name != step.expectedActionProfile)
        {
            continue;
        }
        if (expectedIndex)
        {
            // A profile name is the stable Story identifier across catalog
            // reorderings. Duplicate active names would make it ambiguous.
            return std::nullopt;
        }
        expectedIndex = static_cast<int>(index);
    }
    return expectedIndex;
}

bool storySelectedActionProfileMatches(
    const StoryScriptAction& step,
    const game_data::Piece& actor,
    int selectedActionIndex)
{
    const std::optional<int> expectedIndex =
        storyExpectedActionProfileIndex(step, actor);
    return expectedIndex && selectedActionIndex == *expectedIndex;
}

bool storySelectedPieceActionMatches(
    const StoryScriptAction& step,
    StoryActionKind selectedSourceKind,
    const game_data::Piece& actor,
    int selectedActionIndex)
{
    const bool scriptedPieceAction =
        step.kind == StoryActionKind::Move || step.kind == StoryActionKind::Attack;
    const bool selectedPieceAction =
        selectedSourceKind == StoryActionKind::Move ||
        selectedSourceKind == StoryActionKind::Attack;
    return scriptedPieceAction && selectedPieceAction &&
        step.kind == selectedSourceKind &&
        storySelectedActionProfileMatches(step, actor, selectedActionIndex);
}

bool storyAbilityStepMatches(
    const StoryScriptAction& step,
    const game_data::Piece& actor)
{
    if (step.kind != StoryActionKind::UseAbility ||
        step.expectedAbility.empty() || step.expectedAbilityLabel.empty() ||
        game_data::normalizedAbility(actor.ability) != step.expectedAbility ||
        game_data::pieceAbilityLabel(actor) != step.expectedAbilityLabel)
    {
        return false;
    }
    if (step.expectedAbility == "summon")
    {
        return !step.expectedSummonTitle.empty() &&
            actor.summonTitle == step.expectedSummonTitle &&
            step.expectedNextAbilityLabel.empty();
    }
    if (step.expectedAbility == "transform")
    {
        return step.expectedSummonTitle.empty() &&
            !step.expectedNextAbilityLabel.empty();
    }
    return step.expectedSummonTitle.empty() &&
        step.expectedNextAbilityLabel.empty();
}

bool storyAbilityOutcomeMatches(
    const StoryScriptAction& step,
    const game_data::Piece& actorBefore,
    std::span<const game_data::Piece> piecesAfter,
    int commandingPieceIdAfter)
{
    if (!storyAbilityStepMatches(step, actorBefore))
    {
        return false;
    }
    const auto actorAfter = std::find_if(
        piecesAfter.begin(), piecesAfter.end(), [&](const game_data::Piece& piece) {
            return piece.id == actorBefore.id;
        });
    if (actorAfter == piecesAfter.end())
    {
        return false;
    }
    if (step.expectedAbility == "command")
    {
        return commandingPieceIdAfter == actorBefore.id && actorAfter->hasActed;
    }
    if (step.expectedAbility == "dematerialize")
    {
        return actorAfter->hidden &&
            actorAfter->actionState != actorBefore.actionState && actorAfter->hasActed;
    }
    if (step.expectedAbility == "transform")
    {
        return actorAfter->actionState != actorBefore.actionState &&
            game_data::pieceAbilityLabel(*actorAfter) ==
                step.expectedNextAbilityLabel &&
            actorAfter->hasActed;
    }
    if (step.expectedAbility == "summon")
    {
        const auto [row, column] = game_data::summonDestination(actorBefore);
        return commandingPieceIdAfter == 0 && actorAfter->hasActed &&
            std::any_of(
                piecesAfter.begin(), piecesAfter.end(),
                [&](const game_data::Piece& piece) {
                    return piece.id != actorBefore.id &&
                        piece.owner == actorBefore.owner &&
                        piece.name == step.expectedSummonTitle &&
                        piece.row == row && piece.column == column &&
                        piece.hasActed;
                });
    }
    return false;
}

bool storyCompletionMayBeAwarded(
    bool objectiveStage,
    bool objectiveFailed,
    int winner)
{
    return objectiveStage && !objectiveFailed && winner != 2;
}

bool storyScriptActionAutoResolves(
    const StoryMission& mission,
    const StoryScriptAction& step) noexcept
{
    return step.owner == 2 ||
        (mission.autoResolvePlayerEndTurns && step.owner == 1 &&
         step.kind == StoryActionKind::EndTurn);
}

bool storyScriptActionRequiresPlayerInput(
    const StoryMission& mission,
    const StoryScriptAction& step) noexcept
{
    return step.owner == 1 && !storyScriptActionAutoResolves(mission, step);
}

int storyGuidedPlayerActionCount(const StoryMission& mission) noexcept
{
    return static_cast<int>(std::count_if(
        mission.script.begin(), mission.script.end(),
        [&](const StoryScriptAction& step) {
            return storyScriptActionRequiresPlayerInput(mission, step);
        }));
}

int storyGuidedPlayerActionNumber(
    const StoryMission& mission,
    int scriptStep) noexcept
{
    if (scriptStep < 0)
    {
        return 0;
    }
    const int inclusiveEnd = std::min(
        scriptStep + 1, static_cast<int>(mission.script.size()));
    return static_cast<int>(std::count_if(
        mission.script.begin(), mission.script.begin() + inclusiveEnd,
        [&](const StoryScriptAction& step) {
            return storyScriptActionRequiresPlayerInput(mission, step);
        }));
}

bool storyScriptProgressAllowsCompletion(
    const StoryMission& mission,
    int nextScriptStep) noexcept
{
    return mission.script.empty() ||
        nextScriptStep >= static_cast<int>(mission.script.size());
}

std::string_view storyCampaignName(StoryCampaign campaign)
{
    switch (campaign)
    {
    case StoryCampaign::Blackthorn:
        return "The Blackthorns";
    case StoryCampaign::Mirewatch:
        return "The Mirewatch Resistance";
    case StoryCampaign::Seelie:
        return "The Seelie Court";
    }
    return "Story Mode";
}

std::string_view storyActName(StoryCampaign campaign, int missionIndex)
{
    if (missionIndex < 0)
    {
        return {};
    }
    if (campaign == StoryCampaign::Mirewatch || campaign == StoryCampaign::Blackthorn)
    {
        const bool mirewatch = campaign == StoryCampaign::Mirewatch;
        const int actTwo = 4;
        const int actThree = 8;
        const int actFour = mirewatch ? 14 : 12;
        const int actFive = mirewatch ? 18 : 15;
        const int actSix = mirewatch ? 22 : 19;
        const int actSeven = mirewatch ? 28 : 25;
        if (missionIndex < actTwo)
        {
            return "ACT I - HUNGER WITH A HARNESS";
        }
        if (missionIndex < actThree)
        {
            return "ACT II - THE COST OF RESCUE";
        }
        if (missionIndex < actFour)
        {
            return "ACT III - THE PUBLISHED TRAP";
        }
        if (missionIndex < actFive)
        {
            return "ACT IV - A SOCIETY IN PUBLIC";
        }
        if (missionIndex < actSix)
        {
            return "ACT V - ROADS WITH TERMS";
        }
        if (missionIndex < actSeven)
        {
            return "ACT VI - THE WOUNDED TREE";
        }
        return "ACT VII - A TOWN'S OWN HAND";
    }
    if (campaign != StoryCampaign::Seelie)
    {
        return {};
    }
    if (missionIndex < 15)
    {
        return "ACT I - THE STOLEN ROAD";
    }
    if (missionIndex < 25)
    {
        return "ACT II - THE SHAPE OF A SIEGE";
    }
    if (missionIndex < 31)
    {
        return "ACT III - THE COMPLETE BEARER";
    }
    if (missionIndex < 37)
    {
        return "ACT IV - WHAT THE ROADS COST";
    }
    if (missionIndex < 44)
    {
        return "ACT V - THE EMPTY-PLACE METHOD";
    }
    return "ACT VI - THE MEN WHO CANNOT RELEASE";
}

std::string_view storyActGoal(StoryCampaign campaign, int missionIndex)
{
    if (missionIndex < 0)
    {
        return {};
    }
    if (campaign == StoryCampaign::Mirewatch)
    {
        if (missionIndex < 4)
        {
            return "Reach Mirewatch and learn who weaponized Pedros's craft.";
        }
        if (missionIndex < 8)
        {
            return "Turn shelter and evidence into a resistance before Victor prices every rescue.";
        }
        if (missionIndex < 14)
        {
            return "Save whom you can from Lash's published trap, then name every cost.";
        }
        if (missionIndex < 19)
        {
            return "Make the new Society answer in public while Blackthorn's charter breaks.";
        }
        if (missionIndex < 22)
        {
            return "Cross Feyward by terms that keep every ally free to refuse.";
        }
        if (missionIndex < 28)
        {
            return "Free Sylvara without replacing Blackthorn with another owner.";
        }
        return "Put inheritance and recovery in public hands so the town can govern itself.";
    }
    if (campaign == StoryCampaign::Blackthorn)
    {
        if (missionIndex < 4)
        {
            return "Trace how the Company turns hunger, customs, and sanctuary into leverage.";
        }
        if (missionIndex < 8)
        {
            return "Follow how Company accounts turn rescue, evidence, and retaliation into debt.";
        }
        if (missionIndex < 12)
        {
            return "See how Thaeron and Lash build the trap that destroys Bluewater.";
        }
        if (missionIndex < 15)
        {
            return "Watch Victor defend the charter as Mirewatch makes authority public.";
        }
        if (missionIndex < 19)
        {
            return "Track the Company across Feyward as bounded alliances resist ownership.";
        }
        if (missionIndex < 25)
        {
            return "See Victor's command system break at the wounded World Tree.";
        }
        return "Record what survives when Thaeron's inheritance offer enters public custody.";
    }
    if (campaign != StoryCampaign::Seelie)
    {
        return {};
    }
    if (missionIndex < 15)
    {
        return "Learn what Caltheriel wants, then honor the release she chooses.";
    }
    if (missionIndex < 25)
    {
        return "Carry the rescue's warning to Emberhaven and keep the siege from owning its defenders.";
    }
    if (missionIndex < 31)
    {
        return "Prove COMPLETE is only a machine's label, then replace absolute promises with handoffs.";
    }
    if (missionIndex < 37)
    {
        return "Bring the truth home without pretending lost homes, bodies, or memories can be restored.";
    }
    if (missionIndex < 44)
    {
        return "Build a defense that still works when its most tempting place remains empty.";
    }
    return "Let each person withdraw or testify separately so no complete bearer replaces either tyrant.";
}

std::string_view storyActShortGoal(StoryCampaign campaign, int missionIndex)
{
    if (missionIndex < 0)
    {
        return {};
    }
    if (campaign == StoryCampaign::Mirewatch)
    {
        if (missionIndex < 4)
        {
            return "Reach Mirewatch; trace the harness.";
        }
        if (missionIndex < 8)
        {
            return "Build trust before Victor prices it.";
        }
        if (missionIndex < 14)
        {
            return "Survive Lash's trap; name every cost.";
        }
        if (missionIndex < 19)
        {
            return "Make the Society answer in public.";
        }
        if (missionIndex < 22)
        {
            return "Cross Feyward without owning allies.";
        }
        if (missionIndex < 28)
        {
            return "Free Sylvara; break Victor's command.";
        }
        return "Put recovery in the town's hands.";
    }
    if (campaign == StoryCampaign::Blackthorn)
    {
        if (missionIndex < 4)
        {
            return "Trace the Company's first leverage.";
        }
        if (missionIndex < 8)
        {
            return "Follow how rescue becomes debt.";
        }
        if (missionIndex < 12)
        {
            return "Follow the trap that burns Bluewater.";
        }
        if (missionIndex < 15)
        {
            return "Watch the charter fail in public.";
        }
        if (missionIndex < 19)
        {
            return "Track Blackthorn across Feyward.";
        }
        if (missionIndex < 25)
        {
            return "See Victor's command system break.";
        }
        return "Record what survives the Company.";
    }
    if (campaign != StoryCampaign::Seelie)
    {
        return {};
    }
    if (missionIndex < 15)
    {
        return "Ask what Caltheriel wants.";
    }
    if (missionIndex < 25)
    {
        return "Keep Emberhaven's exits open.";
    }
    if (missionIndex < 31)
    {
        return "Replace COMPLETE with chosen handoffs.";
    }
    if (missionIndex < 37)
    {
        return "Carry truth without erasing loss.";
    }
    if (missionIndex < 44)
    {
        return "Defend the empty place.";
    }
    return "Let every voice withdraw separately.";
}

std::string storyRequiredSurvivorNames(const StoryMission& mission)
{
    struct SurvivorCount
    {
        std::string_view title;
        int count = 0;
    };
    std::vector<SurvivorCount> survivors;
    std::string names;
    for (std::string_view role : mission.requiredSurvivorRoles)
    {
        const auto placement = std::find_if(
            mission.pieces.begin(), mission.pieces.end(),
            [&](const StoryPiecePlacement& piece) { return piece.role == role; });
        if (placement == mission.pieces.end())
        {
            continue;
        }
        const auto existing = std::find_if(
            survivors.begin(), survivors.end(), [&](const SurvivorCount& survivor) {
                return survivor.title == placement->cardTitle;
            });
        if (existing == survivors.end())
        {
            survivors.push_back({placement->cardTitle, 1});
        }
        else
        {
            ++existing->count;
        }
    }
    for (std::size_t index = 0; index < survivors.size(); ++index)
    {
        if (index > 0)
        {
            if (survivors.size() == 2)
            {
                names += " and ";
            }
            else if (index + 1 == survivors.size())
            {
                names += ", and ";
            }
            else
            {
                names += ", ";
            }
        }
        if (survivors[index].count == 2)
        {
            names += "both ";
        }
        else if (survivors[index].count > 2)
        {
            names += std::to_string(survivors[index].count) + " ";
        }
        names += survivors[index].title;
        if (survivors[index].count > 1)
        {
            names += " pieces";
        }
    }
    return names;
}

int storyAiSearchDepth(StoryCampaign campaign, std::string_view missionId)
{
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt17b_open_mastery")
    {
        return 3;
    }
    if (campaign == StoryCampaign::Blackthorn && missionId == "bt17a_field_judgment")
    {
        return 2;
    }
    if (campaign == StoryCampaign::Mirewatch)
    {
        if (missionId == "mw24_agent_not_heir")
        {
            return 3;
        }
        if (missionId == "mw18_allies_dishonestly")
        {
            return 2;
        }
        return 1;
    }
    if (campaign == StoryCampaign::Seelie &&
        (missionId == "se27_emperor_at_tree" ||
         missionId == "se28a_two_men_one_vacancy" ||
         missionId == "se29e_hold_the_witness_rail"))
    {
        return 4;
    }
    if (campaign == StoryCampaign::Seelie &&
        (missionId == "se19_crypt_under_order" ||
         missionId == "se23_controlled_fall"))
    {
        return 3;
    }
    if (campaign == StoryCampaign::Seelie && missionId == "se17_warm_channel")
    {
        return 2;
    }
    return 1;
}

StoryEnemyProgress storyDefeatAllEnemiesProgress(
    const StoryMission& mission,
    std::span<const std::pair<std::string_view, int>> rolePieceIds,
    std::span<const game_data::Piece> authoritativePieces)
{
    bool foundAuthoredEnemy = false;
    bool resolvedEveryRole = true;
    for (const StoryPiecePlacement& placement : mission.pieces)
    {
        if (placement.owner != 2)
        {
            continue;
        }
        foundAuthoredEnemy = true;
        const auto role = std::find_if(
            rolePieceIds.begin(), rolePieceIds.end(), [&](const auto& entry) {
                return entry.first == placement.role;
            });
        if (role == rolePieceIds.end() || role->second <= 0)
        {
            // A broken authored role must never make a mission look complete.
            resolvedEveryRole = false;
        }
    }

    // Count the authoritative force, not just the pieces authored at setup.
    // This includes summons, Trail pieces, and rebirth replacements, while
    // original ownership keeps a temporarily Controlled enemy hostile for the
    // objective's lifetime.
    int remaining = static_cast<int>(std::count_if(
        authoritativePieces.begin(), authoritativePieces.end(),
        [](const game_data::Piece& piece) {
            return game_data::pieceOriginalOwner(piece) == 2;
        }));

    // Keep a corrupt setup visibly unresolved even after the board appears
    // clear. Do not double-count the sentinel while a real enemy is present.
    if (!resolvedEveryRole && remaining == 0)
    {
        remaining = 1;
    }
    return {
        remaining,
        foundAuthoredEnemy && resolvedEveryRole && remaining == 0};
}

bool storyPlayerForcePresent(
    const StoryMission& mission,
    std::span<const std::pair<std::string_view, int>> rolePieceIds,
    std::span<const game_data::Piece> authoritativePieces)
{
    const auto roleId = [&](std::string_view role) -> int {
        const auto found = std::find_if(
            rolePieceIds.begin(), rolePieceIds.end(), [&](const auto& entry) {
                return entry.first == role;
            });
        return found == rolePieceIds.end() ? 0 : found->second;
    };

    // An authored ally still counts while temporarily Controlled by the enemy.
    for (const StoryPiecePlacement& placement : mission.pieces)
    {
        if (placement.owner != 1)
        {
            continue;
        }
        const int id = roleId(placement.role);
        if (id != 0 && std::any_of(
                authoritativePieces.begin(), authoritativePieces.end(),
                [&](const game_data::Piece& piece) { return piece.id == id; }))
        {
            return true;
        }
    }

    // A legitimately deployed or summoned ally also keeps the force alive, but
    // a borrowed authored enemy must not become the last apparent player unit.
    for (const game_data::Piece& piece : authoritativePieces)
    {
        if (piece.owner != 1)
        {
            continue;
        }
        const bool authoredEnemy = std::any_of(
            mission.pieces.begin(), mission.pieces.end(),
            [&](const StoryPiecePlacement& placement) {
                return placement.owner == 2 && roleId(placement.role) == piece.id;
            });
        if (!authoredEnemy)
        {
            return true;
        }
    }
    return false;
}

bool storyMasteryCatchUpMayBeSkipped(
    std::span<const StoryMission> missions,
    int missionIndex,
    const StoryProgress& progress) noexcept
{
    if (missionIndex < 0 || missionIndex >= static_cast<int>(missions.size()) ||
        !missions[static_cast<std::size_t>(missionIndex)]
             .catchUpForSkippedRehearsals)
    {
        return false;
    }

    bool foundPrerequisite = false;
    for (int index = 0; index < missionIndex; ++index)
    {
        if (!missions[static_cast<std::size_t>(index)].optionalRehearsal)
        {
            continue;
        }
        foundPrerequisite = true;
        if (index >= progress.advancedCount ||
            static_cast<std::size_t>(index) >= progress.completedByPlay.size() ||
            !progress.completedByPlay[static_cast<std::size_t>(index)])
        {
            return false;
        }
    }
    return foundPrerequisite;
}

namespace
{

bool writeStoryProgress(
    std::string_view username,
    StoryCampaign campaign,
    const StoryProgress& progress)
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

    const std::span<const StoryMission> missions = storyMissions(campaign);
    const int advancedCount = std::clamp(
        progress.advancedCount, 0, static_cast<int>(missions.size()));
    std::ofstream stream(path, std::ios::trunc);
    stream << "GLOOMTHORN_STORY_PROGRESS_V2\n";
    for (int index = 0; index < advancedCount; ++index)
    {
        const bool completedByPlay = static_cast<std::size_t>(index) <
                progress.completedByPlay.size() &&
            progress.completedByPlay[static_cast<std::size_t>(index)];
        stream << (completedByPlay ? "completed=" : "continued=")
               << missions[static_cast<std::size_t>(index)].id << '\n';
    }
    return static_cast<bool>(stream);
}

} // namespace

StoryProgress loadStoryProgress(std::string_view username, StoryCampaign campaign)
{
    const std::span<const StoryMission> missions = storyMissions(campaign);
    StoryProgress progress;
    progress.completedByPlay.resize(missions.size(), false);

    std::ifstream stream(storyProgressPath(username, campaign));
    if (!stream)
    {
        return progress;
    }

    std::string firstLine;
    std::getline(stream, firstLine);
    if (firstLine != "GLOOMTHORN_STORY_PROGRESS_V2")
    {
        // The retired eight-mission format stored only a position. Those old
        // stages do not map to the expanded campaign's stable mission IDs, so
        // guessing would silently skip unplayed rules (including River Teeth).
        // Preserve the file until the player completes an entry, but begin the
        // revised story at its authored opening.
        return progress;
    }

    std::unordered_set<std::string> completedIds;
    std::unordered_set<std::string> continuedIds;
    std::string line;
    while (std::getline(stream, line))
    {
        constexpr std::string_view CompletedPrefix = "completed=";
        constexpr std::string_view ContinuedPrefix = "continued=";
        if (line.rfind(CompletedPrefix, 0) == 0)
        {
            completedIds.insert(line.substr(CompletedPrefix.size()));
        }
        else if (line.rfind(ContinuedPrefix, 0) == 0)
        {
            continuedIds.insert(line.substr(ContinuedPrefix.size()));
        }
    }

    for (std::size_t index = 0; index < missions.size(); ++index)
    {
        const std::string missionId(missions[index].id);
        const bool completedByPlay = completedIds.contains(missionId);
        if (!completedByPlay && !continuedIds.contains(missionId))
        {
            break;
        }
        progress.completedByPlay[index] = completedByPlay;
        ++progress.advancedCount;
    }
    return progress;
}

bool recordStoryMissionProgress(
    std::string_view username,
    StoryCampaign campaign,
    int missionIndex,
    bool completedByPlay)
{
    const std::span<const StoryMission> missions = storyMissions(campaign);
    if (missionIndex < 0 || missionIndex >= static_cast<int>(missions.size()))
    {
        return false;
    }

    StoryProgress progress = loadStoryProgress(username, campaign);
    if (missionIndex > progress.advancedCount)
    {
        return false;
    }
    progress.advancedCount = std::max(progress.advancedCount, missionIndex + 1);
    progress.completedByPlay.resize(missions.size(), false);
    if (completedByPlay)
    {
        // Replaying a previously continued entry can earn its genuine
        // completion later; continuation can never erase that achievement.
        progress.completedByPlay[static_cast<std::size_t>(missionIndex)] = true;
    }
    return writeStoryProgress(username, campaign, progress);
}

int loadStoryCompletedCount(std::string_view username, StoryCampaign campaign)
{
    return loadStoryProgress(username, campaign).advancedCount;
}

bool saveStoryCompletedCount(
    std::string_view username,
    StoryCampaign campaign,
    int completedCount)
{
    const std::span<const StoryMission> missions = storyMissions(campaign);
    StoryProgress progress;
    progress.advancedCount = std::clamp(
        completedCount, 0, static_cast<int>(missions.size()));
    progress.completedByPlay.resize(missions.size(), false);
    std::fill_n(
        progress.completedByPlay.begin(), progress.advancedCount, true);
    return writeStoryProgress(username, campaign, progress);
}
} // namespace bayou::client
