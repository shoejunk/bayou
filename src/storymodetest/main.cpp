#include "../client/client_story.hpp"
#include "../client/client_story_cards.hpp"
#include "../client/client_story_keyboard.hpp"
#include "../gameserver/game_engine.hpp"
#include "../storytest/story_step_outcomes.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace
{

using bayou::client::StoryActionKind;
using bayou::client::StoryCardResolutionMode;
using bayou::client::StoryCampaign;
using bayou::client::StoryMission;
using bayou::client::StoryObjectiveKind;
using bayou::client::StoryPanel;
using bayou::client::StoryPiecePlacement;
using bayou::client::StoryScriptAction;

int failures = 0;

#ifndef BAYOU_SOURCE_DIR
#define BAYOU_SOURCE_DIR "."
#endif

void check(bool condition, std::string_view label)
{
    if (!condition)
    {
        fmt::println("[FAIL] {}", label);
        ++failures;
    }
}

std::string storyBriefingCopy(const StoryMission& mission)
{
    std::string copy = std::string(mission.objective) + " " + std::string(mission.hint);
    for (const StoryPanel& panel : mission.briefing)
    {
        copy += ' ';
        copy += panel.text;
    }
    std::transform(copy.begin(), copy.end(), copy.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return copy;
}

void validateReorderedStoryProgress()
{
#ifdef _WIN32
    const char* previousAppData = std::getenv("APPDATA");
    const std::string savedAppData = previousAppData ? previousAppData : "";
    const auto scratch = std::filesystem::temp_directory_path() /
        ("gloomthorn-progress-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto folder = scratch / "SteamTactics" / "story_progress";
    std::filesystem::create_directories(folder);
    _putenv_s("APPDATA", scratch.string().c_str());
    const auto missions = bayou::client::storyMissions(StoryCampaign::Mirewatch);
    check(missions.size() > 6, "progress migration has enough route entries");
    if (missions.size() > 6)
    {
        {
            std::ofstream saved(folder / "clarity_progress_test_mirewatch.cfg");
            saved << "GLOOMTHORN_STORY_PROGRESS_V2\n"
                  << "completed=" << missions[0].id << '\n'
                  << "completed=removed_story_scene\n"
                  << "completed=" << missions[2].id << '\n'
                  << "continued=" << missions[4].id << '\n';
        }
        auto progress = bayou::client::loadStoryProgress(
            "clarity_progress_test", StoryCampaign::Mirewatch);
        check(progress.advancedCount == 1 && progress.completedByPlay[2] &&
                progress.advancedEntries[4] && !progress.completedByPlay[4],
            "reordered progress retains later completed and continued IDs without unlocking an unread entry");
        check(!bayou::client::recordStoryMissionProgress(
                "clarity_progress_test", StoryCampaign::Mirewatch, 3, true),
            "a later known completion does not permit skipping a new unread entry");
        check(bayou::client::recordStoryMissionProgress(
                "clarity_progress_test", StoryCampaign::Mirewatch, 1, true),
            "the next unread entry can be completed after a route revision");
        progress = bayou::client::loadStoryProgress(
            "clarity_progress_test", StoryCampaign::Mirewatch);
        check(progress.advancedCount == 3 && progress.completedByPlay[2] &&
                progress.advancedEntries[4] && !progress.completedByPlay[4],
            "saving across a route gap preserves real mastery and an optional skip farther ahead");
        check(bayou::client::recordStoryMissionProgress(
                "clarity_progress_test", StoryCampaign::Mirewatch, 3, false),
            "continuing the next entry closes the second route gap");
        progress = bayou::client::loadStoryProgress(
            "clarity_progress_test", StoryCampaign::Mirewatch);
        check(progress.advancedCount == 5 && progress.completedByPlay[2] &&
                !progress.completedByPlay[3] && !progress.completedByPlay[4],
            "reordered continuation keeps later entries without awarding unplayed mastery");
    }
    _putenv_s("APPDATA", savedAppData.c_str());
    std::error_code cleanupError;
    std::filesystem::remove_all(scratch, cleanupError);
    check(!cleanupError, "temporary progress-test files are removed");
#endif
}

void validateStoryKeyboardNavigation()
{
    using bayou::client::StoryBoardCursor;
    using bayou::client::moveStoryBoardCursor;
    using bayou::client::wrapStoryKeyboardIndex;

    StoryBoardCursor cursor{3, 3};
    cursor = moveStoryBoardCursor(cursor, 1, -1);
    check(
        cursor == StoryBoardCursor{4, 2},
        "Story keyboard cursor moves one visible board square per input");
    cursor = moveStoryBoardCursor(cursor, 99, -99);
    check(
        cursor == StoryBoardCursor{game_data::BoardSize - 1, 0},
        "Story keyboard cursor clamps at every board edge");
    check(
        wrapStoryKeyboardIndex(0, -1, 4) == 3 &&
            wrapStoryKeyboardIndex(3, 1, 4) == 0 &&
            wrapStoryKeyboardIndex(-1, 1, 4) == 0 &&
            wrapStoryKeyboardIndex(-1, -1, 4) == 3,
        "Story keyboard focus wraps forward and backward from a cold start");
    check(
        wrapStoryKeyboardIndex(2, 1, 0) == -1,
        "Story keyboard focus safely rejects an empty control list");

    using bayou::client::storyContinueWithoutMasteryAvailable;
    using bayou::client::storyRequiresSeelieSpoilerConfirmation;
    check(
        storyRequiresSeelieSpoilerConfirmation(StoryCampaign::Seelie, 29, 30) &&
            !storyRequiresSeelieSpoilerConfirmation(StoryCampaign::Seelie, 30, 30) &&
            !storyRequiresSeelieSpoilerConfirmation(StoryCampaign::Mirewatch, 0, 30),
        "Seelie alone warns until every Mirewatch entry has advanced");

    StoryMission requiredOpenMission;
    requiredOpenMission.objectiveSpec.kind = StoryObjectiveKind::DefeatAllEnemies;
    check(
        !storyContinueWithoutMasteryAvailable(requiredOpenMission, 0) &&
            storyContinueWithoutMasteryAvailable(requiredOpenMission, 1),
        "required open combat exposes no-mastery continuation only after a genuine defeat");
    requiredOpenMission.optionalRehearsal = true;
    check(
        !storyContinueWithoutMasteryAvailable(requiredOpenMission, 1),
        "optional open drills retain their existing skip path instead of defeat continuation");
    requiredOpenMission.optionalRehearsal = false;
    requiredOpenMission.script.push_back({});
    check(
        !storyContinueWithoutMasteryAvailable(requiredOpenMission, 1),
        "guided missions preserve their authored mastery path after defeat");

    bayou::client::StoryProgress progress;
    progress.advancedCount = 2;
    progress.completedByPlay = {true, false};
    check(
        !progress.continuedWithoutMastery(0) &&
            progress.continuedWithoutMastery(1) &&
            !progress.continuedWithoutMastery(2),
        "continued entries remain distinct from played completions in progress state");
}

void validateAuthoritativeCardBoundary()
{
    const std::span<const card_data::Card> noAuthoritativeCards;
    const auto normalMissing = bayou::client::resolveStoryCardDefinition(
        "Blackthorn Foreman",
        noAuthoritativeCards,
        noAuthoritativeCards,
        StoryCardResolutionMode::AuthoritativeOnly);
    const auto fixtureFallback = bayou::client::resolveStoryCardDefinition(
        "Blackthorn Foreman",
        noAuthoritativeCards,
        noAuthoritativeCards,
        StoryCardResolutionMode::AllowPackagedFixtureFallback);
    check(
        !normalMissing,
        "connected Story lookup fails closed instead of borrowing a packaged card");
    check(
        fixtureFallback && fixtureFallback->title == "Blackthorn Foreman",
        "explicit offline fixture lookup may use the packaged Story card");

    card_data::Card liveForeman;
    liveForeman.title = "Blackthorn Foreman";
    liveForeman.type = "Unit";
    liveForeman.integerValues = {{"health", 42}};
    liveForeman.stringValues = {
        {"ability", "summon"}, {"summon", "Blackthorn Lumberjack"}};
    const std::vector<card_data::Card> liveForemanOnly = {liveForeman};
    const std::span<const card_data::Card> liveForemanSpan(
        liveForemanOnly.data(), liveForemanOnly.size());
    const auto liveWins = bayou::client::resolveStoryCardDefinition(
        "Blackthorn Foreman",
        liveForemanSpan,
        noAuthoritativeCards,
        StoryCardResolutionMode::AllowPackagedFixtureFallback);
    check(
        liveWins && liveWins->health == 42,
        "authoritative Story card wins even when fixture fallback is explicitly enabled");

    StoryMission summonMission;
    summonMission.pieces = {
        {"foreman", "Blackthorn Foreman", 1, 3, 3, false, -1}};
    const auto incompleteSummonClosure =
        bayou::client::validateStoryCardDependencies(
            summonMission,
            liveForemanSpan,
            noAuthoritativeCards,
            StoryCardResolutionMode::AuthoritativeOnly);
    check(
        incompleteSummonClosure.missingTitles ==
            std::vector<std::string>{"Blackthorn Lumberjack"},
        "connected Story validation follows Summon dependencies and names the missing card");

    StoryMission completeFixtureMission;
    completeFixtureMission.pieces = {
        {"foreman", "Blackthorn Foreman", 1, 3, 3, false, -1}};
    completeFixtureMission.playerHand = {"Grove Sister"};
    completeFixtureMission.enemyHand = {"Bull Gator"};
    completeFixtureMission.playerDrawPile = {"Swamp Tracker"};
    completeFixtureMission.enemyDrawPile = {"Nettle Starbright"};
    completeFixtureMission.objectiveSpec.kind = StoryObjectiveKind::DeployCard;
    completeFixtureMission.objectiveSpec.cardTitle = "Juniper Flash";
    StoryScriptAction scriptedDeployment;
    scriptedDeployment.kind = StoryActionKind::PlayCard;
    scriptedDeployment.cardTitle = "Pavo Quickstep";
    completeFixtureMission.script = {scriptedDeployment};
    const auto fixtureClosure = bayou::client::validateStoryCardDependencies(
        completeFixtureMission,
        noAuthoritativeCards,
        noAuthoritativeCards,
        StoryCardResolutionMode::AllowPackagedFixtureFallback);
    const auto closureContains = [&](std::string_view title) {
        return std::find(
                   fixtureClosure.requiredTitles.begin(),
                   fixtureClosure.requiredTitles.end(),
                   title) != fixtureClosure.requiredTitles.end();
    };
    check(
        fixtureClosure.complete(),
        "explicit offline Story fixture resolves the complete mission dependency closure");
    check(
        closureContains("Blackthorn Foreman") &&
            closureContains("Grove Sister") &&
            closureContains("Bull Gator") &&
            closureContains("Swamp Tracker") &&
            closureContains("Nettle Starbright") &&
            closureContains("Juniper Flash") &&
            closureContains("Pavo Quickstep"),
        "Story dependency closure includes board, both hands, both draw piles, and deployments");
    check(
        closureContains("Blackthorn Lumberjack") &&
            closureContains("Sapling") &&
            closureContains("Swamp Tracker Unmounted") &&
            closureContains("Nettle Starbright Unmounted"),
        "Story dependency closure recursively includes Summon, Trail, and Rebirth definitions");
}

void validateGuidedActionProfileSelection()
{
    const std::span<const StoryMission> blackthornMissions =
        bayou::client::storyMissions(StoryCampaign::Blackthorn);
    const auto termsMission = std::find_if(
        blackthornMissions.begin(),
        blackthornMissions.end(),
        [](const StoryMission& mission) {
            return mission.id == "bt04_terms_conditions";
        });
    const auto collisionStep = termsMission == blackthornMissions.end()
        ? std::vector<StoryScriptAction>::const_iterator{}
        : std::find_if(
              termsMission->script.begin(),
              termsMission->script.end(),
              [](const StoryScriptAction& step) {
                  return step.heading == "AMBUSH THE HIDDEN SCOUT";
              });
    const bool collisionStepFound = termsMission != blackthornMissions.end() &&
        collisionStep != termsMission->script.end();
    check(
        collisionStepFound && collisionStep->kind == StoryActionKind::Move &&
            collisionStep->expectedActionProfile == "Ambush",
        "hidden-collision lesson authors a visible-board Move with the printed Ambush profile selected");
    if (!collisionStepFound)
    {
        return;
    }

    const auto ambusherCard =
        bayou::client::packagedStoryCard("Goblin Ambusher");
    const auto hiddenTargetCard =
        bayou::client::packagedStoryCard("Erevan the Shadow");
    check(
        ambusherCard && hiddenTargetCard,
        "guided-profile regression cards have packaged definitions");
    if (!(ambusherCard && hiddenTargetCard))
    {
        return;
    }

    game_data::Piece ambusher;
    ambusher.id = 1;
    ambusher.owner = 1;
    ambusher.row = 5;
    ambusher.column = 1;
    ambusher.name = ambusherCard->title;
    ambusher.actions = ambusherCard->actions;
    ambusher.actionState = 1;
    ambusher.hidden = true;

    game_data::Piece hiddenTarget;
    hiddenTarget.id = 2;
    hiddenTarget.owner = 2;
    hiddenTarget.row = 5;
    hiddenTarget.column = 2;
    hiddenTarget.name = hiddenTargetCard->title;
    hiddenTarget.actions = hiddenTargetCard->actions;
    hiddenTarget.actionState = 1;
    hiddenTarget.hidden = true;

    const std::vector<game_data::Piece> pieces = {ambusher, hiddenTarget};
    const std::array<std::uint8_t, game_data::BoardSquares> holes{};
    const std::vector<game_data::Piece> visiblePieces =
        game_data::piecesVisibleTo(pieces, ambusher.owner);
    const game_data::ActionResolution selectedSourceSneak =
        game_data::resolvePieceAction(
            visiblePieces, holes, pieces.front(), 5, 2, false, 1);
    const game_data::PieceActionOutcome selectedSneak =
        game_data::resolvePieceActionThroughHidden(
            pieces, holes, pieces.front(), 5, 2, 1);
    const game_data::PieceActionOutcome selectedAmbush =
        game_data::resolvePieceActionThroughHidden(
            pieces, holes, pieces.front(), 5, 2, 2);
    check(
        selectedSourceSneak.legal && selectedSourceSneak.moves &&
            !selectedSourceSneak.attacks && selectedSneak.action.legal &&
            !selectedSneak.action.moves && !selectedSneak.action.attacks &&
            selectedSneak.action.actionIndex == 1 &&
            selectedSneak.destinationRow == 5 && selectedSneak.destinationColumn == 1 &&
            selectedSneak.revealedPieceIds == std::vector<int>{2} &&
            selectedAmbush.action.legal && selectedAmbush.action.attacks &&
            selectedAmbush.action.actionIndex == 2,
        "hidden collision preserves the chosen profile: Sneak Around stops harmlessly while Ambush attacks");

    const StoryScriptAction& expectedAmbush = *collisionStep;
    const std::optional<int> expectedAmbushIndex =
        bayou::client::storyExpectedActionProfileIndex(
            expectedAmbush, pieces.front());
    check(
        expectedAmbushIndex == 2 &&
            bayou::client::storySelectedPieceActionMatches(
                expectedAmbush, StoryActionKind::Move, pieces.front(), 2) &&
            !bayou::client::storySelectedPieceActionMatches(
                expectedAmbush, StoryActionKind::Move, pieces.front(), 1) &&
            !bayou::client::storySelectedPieceActionMatches(
                expectedAmbush, StoryActionKind::Attack, pieces.front(), 2),
        "guided hidden collision separately requires the authored source kind and profile");
    check(
        !bayou::client::storySelectedActionProfileMatches(
            expectedAmbush, pieces.front(), -1),
        "guided Move/Attack rejects an omitted automatic profile selector");

    StoryScriptAction missingProfile;
    missingProfile.kind = StoryActionKind::Attack;
    check(
        !bayou::client::storyExpectedActionProfileIndex(
             missingProfile, pieces.front()) &&
            !bayou::client::storySelectedActionProfileMatches(
                missingProfile, pieces.front(), 1),
        "guided Move/Attack without an expected profile fails closed");

    game_data::Piece reordered = pieces.front();
    std::swap(reordered.actions[1], reordered.actions[2]);
    check(
        bayou::client::storyExpectedActionProfileIndex(
            expectedAmbush, reordered) == 1 &&
            bayou::client::storySelectedActionProfileMatches(
                expectedAmbush, reordered, 1),
        "guided profile names remain exact when authoritative action ordering changes");

    game_data::Piece ambiguous = pieces.front();
    ambiguous.actions.push_back(ambiguous.actions[2]);
    check(
        !bayou::client::storyExpectedActionProfileIndex(
             expectedAmbush, ambiguous) &&
            !bayou::client::storySelectedActionProfileMatches(
                expectedAmbush, ambiguous, 2),
        "duplicate active printed profile names fail closed");

    StoryScriptAction kindStableStep;
    kindStableStep.kind = StoryActionKind::Move;
    kindStableStep.expectedActionProfile = "Stable Route";
    game_data::Piece hopper;
    hopper.id = 11;
    hopper.owner = 1;
    hopper.row = 0;
    hopper.column = 0;
    game_data::ActionProfile stableRoute;
    stableRoute.name = "Stable Route";
    stableRoute.kind = static_cast<std::uint8_t>(game_data::ActionKind::Hop);
    stableRoute.canMove = true;
    stableRoute.canAttack = false;
    hopper.actions = {stableRoute};
    game_data::Piece pivot;
    pivot.id = 12;
    pivot.owner = 2;
    pivot.row = 0;
    pivot.column = 1;
    const std::array<std::uint8_t, game_data::BoardSquares> kindHoles{};
    const game_data::ActionResolution moveOnlyRoute =
        game_data::resolvePieceAction({hopper, pivot}, kindHoles, hopper, 0, 2, false, 0);
    hopper.actions[0].canAttack = true;
    hopper.actions[0].damage = 1;
    const game_data::ActionResolution driftedAttackRoute =
        game_data::resolvePieceAction({hopper, pivot}, kindHoles, hopper, 0, 2, false, 0);
    const StoryActionKind moveOnlyKind = moveOnlyRoute.attacks
        ? StoryActionKind::Attack
        : StoryActionKind::Move;
    const StoryActionKind driftedKind = driftedAttackRoute.attacks
        ? StoryActionKind::Attack
        : StoryActionKind::Move;
    check(
        moveOnlyRoute.legal && driftedAttackRoute.legal &&
            moveOnlyKind == StoryActionKind::Move &&
            driftedKind == StoryActionKind::Attack &&
            bayou::client::storySelectedPieceActionMatches(
                kindStableStep, moveOnlyKind, hopper, 0) &&
            !bayou::client::storySelectedPieceActionMatches(
                kindStableStep, driftedKind, hopper, 0),
        "guided live parity rejects a same-name profile whose resolved source kind drifts from Move to Attack");

    StoryScriptAction exactCommand;
    exactCommand.kind = StoryActionKind::UseAbility;
    exactCommand.expectedAbility = "command";
    exactCommand.expectedAbilityLabel = "Command";
    game_data::Piece commander;
    commander.id = 20;
    commander.owner = 1;
    commander.ability = "command";
    check(
        bayou::client::storyAbilityStepMatches(exactCommand, commander),
        "guided ability accepts its exact normalized identity and printed label");
    commander.ability = "summon";
    check(
        !bayou::client::storyAbilityStepMatches(exactCommand, commander),
        "guided ability rejects a different usable live ability");
    StoryScriptAction incompleteSummon = exactCommand;
    incompleteSummon.expectedAbility = "summon";
    incompleteSummon.expectedAbilityLabel = "Summon";
    check(
        !bayou::client::storyAbilityStepMatches(incompleteSummon, commander),
        "guided Summon without an exact result unit fails closed");

    check(
        bayou::client::storyCompletionMayBeAwarded(true, false, 1) &&
            bayou::client::storyCompletionMayBeAwarded(true, false, 0) &&
            !bayou::client::storyCompletionMayBeAwarded(false, false, 1) &&
            !bayou::client::storyCompletionMayBeAwarded(true, true, 1) &&
            !bayou::client::storyCompletionMayBeAwarded(true, false, 2),
        "completion authority rejects a closed objective stage, objective failure, and opponent victory");
}

void validateDefeatAllEnemyIdentity()
{
    StoryMission mission;
    mission.pieces = {
        {"ally", "Pavo Quickstep", 1, 0, 0, false, -1},
        {"enemy", "Bristlejack", 2, 0, 2, false, -1}};
    const std::vector<std::pair<std::string_view, int>> roles = {
        {"ally", 10}, {"enemy", 20}};
    game_data::Piece ally;
    ally.id = 10;
    ally.owner = 1;
    game_data::Piece controlledEnemy;
    controlledEnemy.id = 20;
    controlledEnemy.owner = 1;
    controlledEnemy.originalOwner = 2;
    const std::vector<game_data::Piece> controlledEnemyStillLive = {
        ally, controlledEnemy};
    const auto controlledProgress = bayou::client::storyDefeatAllEnemiesProgress(
        mission, roles, controlledEnemyStillLive);
    check(
        controlledProgress.remaining == 1 && !controlledProgress.complete,
        "temporary Control never satisfies DefeatAllEnemies while the authored enemy lives");
    const std::vector<game_data::Piece> defeatedEnemy = {ally};
    const auto defeatedProgress = bayou::client::storyDefeatAllEnemiesProgress(
        mission, roles, defeatedEnemy);
    check(
        defeatedProgress.remaining == 0 && defeatedProgress.complete,
        "DefeatAllEnemies completes only after the authored enemy identity leaves play");
    const std::vector<std::pair<std::string_view, int>> missingEnemyRole = {{"ally", 10}};
    const auto unresolvedProgress = bayou::client::storyDefeatAllEnemiesProgress(
        mission, missingEnemyRole, defeatedEnemy);
    check(
        unresolvedProgress.remaining == 1 && !unresolvedProgress.complete,
        "DefeatAllEnemies fails closed when an authored enemy role cannot be resolved");

    check(
        bayou::client::storyPlayerForcePresent(
            mission, roles, controlledEnemyStillLive),
        "an authored ally plus a Controlled enemy remains a live player force");
    check(
        !bayou::client::storyPlayerForcePresent(
            mission, roles, std::vector<game_data::Piece>{controlledEnemy}),
        "a borrowed authored enemy cannot be the player's last surviving force");
    game_data::Piece controlledAlly = ally;
    controlledAlly.owner = 2;
    game_data::Piece hostileEnemy = controlledEnemy;
    hostileEnemy.owner = 2;
    check(
        bayou::client::storyPlayerForcePresent(
            mission, roles, std::vector<game_data::Piece>{controlledAlly, hostileEnemy}),
        "an authored ally remains present while temporarily Controlled by the enemy");
    game_data::Piece deployedAlly;
    deployedAlly.id = 30;
    deployedAlly.owner = 1;
    check(
        bayou::client::storyPlayerForcePresent(
            mission, roles, std::vector<game_data::Piece>{deployedAlly, hostileEnemy}),
        "a legitimately deployed allied unit can keep the player's force alive");

    const auto pavo = bayou::client::packagedStoryCard("Pavo Quickstep");
    const auto erevan = bayou::client::packagedStoryCard("Erevan the Shadow");
    check(pavo && erevan, "hidden-enemy regression cards have packaged definitions");
    if (!(pavo && erevan))
    {
        return;
    }

    GameEngine engine(0x45524556u, {});
    engine.loadScenario(
        {{1, *pavo, 0, 0, false}, {2, *erevan, 7, 7, false}},
        {}, {}, 0, 0, 2, "Hidden enemy objective regression", false);
    const auto placedPavo = std::find_if(
        engine.boardPieces().begin(), engine.boardPieces().end(),
        [](const game_data::Piece& piece) { return piece.name == "Pavo Quickstep"; });
    const auto placedErevan = std::find_if(
        engine.boardPieces().begin(), engine.boardPieces().end(),
        [](const game_data::Piece& piece) { return piece.name == "Erevan the Shadow"; });
    check(
        placedPavo != engine.boardPieces().end() &&
            placedErevan != engine.boardPieces().end(),
        "hidden-enemy regression board loads both authored identities");
    if (placedPavo == engine.boardPieces().end() ||
        placedErevan == engine.boardPieces().end())
    {
        return;
    }
    const int pavoId = placedPavo->id;
    const int erevanId = placedErevan->id;
    const std::vector<std::pair<std::string_view, int>> hiddenRoles = {
        {"ally", pavoId}, {"enemy", erevanId}};
    check(
        engine.useAbility(2, erevanId),
        "enemy Erevan can use the real Dematerialize ability in the regression");
    const auto hiddenAuthoritative = std::find_if(
        engine.boardPieces().begin(), engine.boardPieces().end(),
        [&](const game_data::Piece& piece) { return piece.id == erevanId; });
    check(
        hiddenAuthoritative != engine.boardPieces().end() && hiddenAuthoritative->hidden,
        "Dematerialized Erevan remains alive on the authoritative board");
    const game_data::Snapshot playerView = engine.snapshotFor(1);
    check(
        std::none_of(
            playerView.pieces.begin(), playerView.pieces.end(),
            [&](const game_data::Piece& piece) { return piece.id == erevanId; }),
        "the player snapshot correctly conceals enemy Erevan");
    const auto hiddenProgress = bayou::client::storyDefeatAllEnemiesProgress(
        mission, hiddenRoles, engine.boardPieces());
    check(
        hiddenProgress.remaining == 1 && !hiddenProgress.complete,
        "a concealed but living Erevan remains counted and cannot complete DefeatAllEnemies");

    const auto foreman = bayou::client::packagedStoryCard("Blackthorn Foreman");
    const auto lumberjack = bayou::client::packagedStoryCard("Blackthorn Lumberjack");
    check(
        foreman && lumberjack,
        "enemy-reinforcement regression cards have packaged definitions");
    if (!(foreman && lumberjack))
    {
        return;
    }

    GameEngine summonEngine(0x454e454du, {});
    summonEngine.registerScenarioCard(*lumberjack);
    summonEngine.loadScenario(
        {{1, *pavo, 0, 0, false}, {2, *foreman, 3, 6, false}},
        {}, {}, 0, 0, 2, "Enemy reinforcement objective regression", false);
    const auto summonedPavo = std::find_if(
        summonEngine.boardPieces().begin(), summonEngine.boardPieces().end(),
        [](const game_data::Piece& piece) { return piece.name == "Pavo Quickstep"; });
    const auto placedForeman = std::find_if(
        summonEngine.boardPieces().begin(), summonEngine.boardPieces().end(),
        [](const game_data::Piece& piece) { return piece.name == "Blackthorn Foreman"; });
    check(
        summonedPavo != summonEngine.boardPieces().end() &&
            placedForeman != summonEngine.boardPieces().end(),
        "enemy-reinforcement regression loads both authored identities");
    if (summonedPavo == summonEngine.boardPieces().end() ||
        placedForeman == summonEngine.boardPieces().end())
    {
        return;
    }

    StoryMission summonMission;
    summonMission.pieces = {
        {"ally", "Pavo Quickstep", 1, 0, 0, false, -1},
        {"foreman", "Blackthorn Foreman", 2, 3, 6, false, -1}};
    const int summonedPavoId = summonedPavo->id;
    const int foremanId = placedForeman->id;
    const std::vector<std::pair<std::string_view, int>> summonRoles = {
        {"ally", summonedPavoId}, {"foreman", foremanId}};
    check(
        summonEngine.useAbility(2, foremanId),
        "enemy Foreman can create a real non-authored reinforcement");
    const auto reinforcedProgress = bayou::client::storyDefeatAllEnemiesProgress(
        summonMission, summonRoles, summonEngine.boardPieces());
    check(
        reinforcedProgress.remaining == 2 && !reinforcedProgress.complete,
        "DefeatAllEnemies counts a live enemy summon that has no authored role");

    std::vector<game_data::Piece> reinforcementOnly = summonEngine.boardPieces();
    std::erase_if(reinforcementOnly, [&](const game_data::Piece& piece) {
        return piece.id == foremanId;
    });
    const auto replacementProgress = bayou::client::storyDefeatAllEnemiesProgress(
        summonMission, summonRoles, reinforcementOnly);
    check(
        replacementProgress.remaining == 1 && !replacementProgress.complete,
        "DefeatAllEnemies stays open after authored enemies die while a reinforcement survives");

    auto summonedLumberjack = std::find_if(
        reinforcementOnly.begin(), reinforcementOnly.end(),
        [](const game_data::Piece& piece) { return piece.name == "Blackthorn Lumberjack"; });
    check(
        summonedLumberjack != reinforcementOnly.end(),
        "enemy reinforcement is present for the Control regression");
    if (summonedLumberjack != reinforcementOnly.end())
    {
        summonedLumberjack->originalOwner = 2;
        summonedLumberjack->owner = 1;
        summonedLumberjack->controlTurnsRemaining = 1;
        const auto controlledReinforcementProgress =
            bayou::client::storyDefeatAllEnemiesProgress(
                summonMission, summonRoles, reinforcementOnly);
        check(
            controlledReinforcementProgress.remaining == 1 &&
                !controlledReinforcementProgress.complete,
            "temporary Control cannot clear a non-authored enemy reinforcement");
    }
}

bool assetExistsWithExactCase(std::string_view relativePath)
{
    if (relativePath.empty())
    {
        return false;
    }

    std::filesystem::path current = std::filesystem::path(BAYOU_SOURCE_DIR) / "assets";
    for (const std::filesystem::path& component : std::filesystem::path(relativePath))
    {
        std::error_code error;
        bool matched = false;
        for (const auto& entry : std::filesystem::directory_iterator(current, error))
        {
            if (entry.path().filename().string() == component.string())
            {
                matched = true;
                break;
            }
        }
        if (error || !matched)
        {
            return false;
        }
        current /= component;
    }

    std::error_code error;
    return std::filesystem::is_regular_file(current, error) && !error;
}

std::optional<std::pair<std::uint32_t, std::uint32_t>> pngDimensions(
    std::string_view relativePath)
{
    const std::filesystem::path path =
        std::filesystem::path(BAYOU_SOURCE_DIR) / "assets" / relativePath;
    std::ifstream stream(path, std::ios::binary);
    std::array<unsigned char, 24> header{};
    if (!stream.read(
            reinterpret_cast<char*>(header.data()),
            static_cast<std::streamsize>(header.size())))
    {
        return std::nullopt;
    }
    constexpr std::array<unsigned char, 8> Signature = {
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    if (!std::equal(Signature.begin(), Signature.end(), header.begin()) ||
        header[12] != 'I' || header[13] != 'H' ||
        header[14] != 'D' || header[15] != 'R')
    {
        return std::nullopt;
    }
    const auto bigEndian = [&](std::size_t offset) {
        return (static_cast<std::uint32_t>(header[offset]) << 24) |
            (static_cast<std::uint32_t>(header[offset + 1]) << 16) |
            (static_cast<std::uint32_t>(header[offset + 2]) << 8) |
            static_cast<std::uint32_t>(header[offset + 3]);
    };
    return std::pair{bigEndian(16), bigEndian(20)};
}

std::optional<std::uint64_t> assetContentHash(std::string_view relativePath)
{
    const std::filesystem::path path =
        std::filesystem::path(BAYOU_SOURCE_DIR) / "assets" / relativePath;
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return std::nullopt;
    }

    std::uint64_t hash = 14695981039346656037ull;
    std::array<char, 64 * 1024> buffer{};
    while (stream)
    {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = stream.gcount();
        for (std::streamsize index = 0; index < count; ++index)
        {
            hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(index)]);
            hash *= 1099511628211ull;
        }
    }
    return stream.bad() ? std::nullopt : std::optional<std::uint64_t>{hash};
}

void validateAsset(std::string_view relativePath, std::string_view owner)
{
    check(
        assetExistsWithExactCase(relativePath),
        std::string(owner) + " references existing exact-case asset: " +
            std::string(relativePath));
}

void validateStoryAssets()
{
    std::set<std::string> cardTitles;
    std::set<std::string> scenarioArtPaths;
    std::set<std::string> riverTeethPanelArt;
    const auto rememberCard = [&](std::string_view title) {
        if (!title.empty())
        {
            cardTitles.emplace(title);
        }
    };
    const auto validatePanel = [&](const StoryPanel& panel, std::string_view missionId) {
        if (!panel.artPath.empty())
        {
            validateAsset(panel.artPath, missionId);
            if (missionId == "mw01_river_teeth")
            {
                riverTeethPanelArt.emplace(panel.artPath);
            }
        }
    };

    for (StoryCampaign campaign : {
             StoryCampaign::Mirewatch, StoryCampaign::Blackthorn, StoryCampaign::Seelie})
    {
        const std::string_view expectedArtPrefix = campaign == StoryCampaign::Mirewatch
            ? "story/mw/"
            : campaign == StoryCampaign::Blackthorn ? "story/bt/" : "story/se/";
        for (const StoryMission& mission : bayou::client::storyMissions(campaign))
        {
            const bool usesRiverTeethSequence = mission.id == "mw01_river_teeth";
            check(
                usesRiverTeethSequence || mission.id == "mw24a_victor_in_custody" ||
                    mission.id == "mw06a_card_workshop" || mission.id == "mw15a_team_workshop" ||
                    mission.id == "se03a_flight_workshop" ||
                    mission.id == "s03_published_mystery" ||
                    (campaign == StoryCampaign::Seelie && mission.id == "se18a_complete_is_a_label") ||
                    !mission.scenarioArtPath.empty(),
                std::string(mission.id) +
                    ": each scene uses its matching scenario art or a reviewed character-art fallback");
            if (!mission.scenarioArtPath.empty())
            {
                check(
                    mission.scenarioArtPath.starts_with(expectedArtPrefix),
                    std::string(mission.id) + ": scenario art belongs to its campaign");
                check(
                    scenarioArtPaths.emplace(mission.scenarioArtPath).second,
                    std::string(mission.id) + ": scenario art is not reused by another mission");
                validateAsset(mission.scenarioArtPath, mission.id);
                const auto dimensions = pngDimensions(mission.scenarioArtPath);
                check(
                    dimensions &&
                        dimensions->first == 1024 &&
                        dimensions->second == 1536,
                    std::string(mission.id) +
                        ": active commissioned scenario master is exactly 1024x1536");
            }
            for (const StoryPanel& panel : mission.briefing)
            {
                validatePanel(panel, mission.id);
            }
            for (const StoryPanel& panel : mission.aftermath)
            {
                validatePanel(panel, mission.id);
            }
            for (const StoryPiecePlacement& piece : mission.pieces)
            {
                rememberCard(piece.cardTitle);
            }
            for (std::string_view title : mission.playerHand)
            {
                rememberCard(title);
            }
            for (std::string_view title : mission.enemyHand)
            {
                rememberCard(title);
            }
            for (std::string_view title : mission.playerDrawPile)
            {
                rememberCard(title);
            }
            for (std::string_view title : mission.enemyDrawPile)
            {
                rememberCard(title);
            }
            rememberCard(mission.objectiveSpec.cardTitle);
            for (std::string_view title : mission.masteryCards)
            {
                rememberCard(title);
            }
            for (const StoryScriptAction& action : mission.script)
            {
                rememberCard(action.cardTitle);
                for (const StoryPanel& panel : action.panelsBefore)
                {
                    validatePanel(panel, mission.id);
                }
            }
        }
    }

    check(
        riverTeethPanelArt ==
            std::set<std::string>{
                "story/mw/mw01/01_gator_ambush_v2.png",
                "story/mw/mw01/02_reed_retreat_v2.png",
                "story/mw/mw01/03_telos_travel_v2.png",
                "story/mw/mw01/04_harness_aftermath_v2.png"},
        "River Teeth uses its complete four-image opening sequence");
    for (const std::string& artPath : riverTeethPanelArt)
    {
        const auto dimensions = pngDimensions(artPath);
        check(
            dimensions &&
                dimensions->first == 1024 &&
                dimensions->second == 1536,
            artPath + ": River Teeth panel master is exactly 1024x1536");
    }

    std::set<std::string> activeStoryArt = scenarioArtPaths;
    activeStoryArt.insert(riverTeethPanelArt.begin(), riverTeethPanelArt.end());
    std::unordered_map<std::uint64_t, std::string> storyArtByContent;
    for (const std::string& artPath : activeStoryArt)
    {
        const auto contentHash = assetContentHash(artPath);
        check(contentHash.has_value(), artPath + ": active story art can be hashed");
        if (!contentHash)
        {
            continue;
        }
        const auto [existing, inserted] = storyArtByContent.emplace(*contentHash, artPath);
        check(
            inserted,
            artPath + ": active story art is visually unique from " + existing->second);
    }
    fmt::println(
        "Story art catalog: {} unique active masters ({} scenario, {} River Teeth panels).",
        activeStoryArt.size(), scenarioArtPaths.size(), riverTeethPanelArt.size());

    for (const std::string& title : cardTitles)
    {
        const auto card = bayou::client::packagedStoryCard(title);
        check(card.has_value(), "packaged Story Mode card exists: " + title);
        if (!card)
        {
            continue;
        }

        validateAsset(card->imagePath, title);
        validateAsset(card->tokenPath, title);
        for (const std::string* optionalPath : {
                 &card->walkAnimPath, &card->idleAnimPath, &card->attackAnimPath,
                 &card->damagedAnimPath, &card->killedAnimPath, &card->fidgetAnimPath,
                 &card->state1TokenPath, &card->pieceBaseBluePath, &card->pieceBaseRedPath})
        {
            if (!optionalPath->empty())
            {
                validateAsset(*optionalPath, title);
            }
        }
    }
}

const StoryMission* missionById(StoryCampaign campaign, std::string_view id)
{
    const auto missions = bayou::client::storyMissions(campaign);
    const auto found = std::find_if(
        missions.begin(), missions.end(),
        [&](const StoryMission& mission) { return mission.id == id; });
    return found == missions.end() ? nullptr : &*found;
}

const game_data::Piece* pieceNamed(const GameEngine& engine, std::string_view title)
{
    const auto found = std::find_if(
        engine.boardPieces().begin(), engine.boardPieces().end(),
        [&](const game_data::Piece& piece) { return piece.name == title; });
    return found == engine.boardPieces().end() ? nullptr : &*found;
}

bool hasPieceAt(
    const GameEngine& engine,
    std::string_view title,
    int row,
    int column,
    int owner = 0)
{
    return std::any_of(
        engine.boardPieces().begin(), engine.boardPieces().end(),
        [&](const game_data::Piece& piece) {
            return piece.name == title && piece.row == row && piece.column == column &&
                (owner == 0 || piece.owner == owner);
        });
}

int pieceCount(const GameEngine& engine, std::string_view title)
{
    return static_cast<int>(std::count_if(
        engine.boardPieces().begin(), engine.boardPieces().end(),
        [&](const game_data::Piece& piece) { return piece.name == title; }));
}

void validateCampaign(
    StoryCampaign campaign,
    std::size_t expectedCount,
    std::size_t expectedTacticalCount,
    const std::vector<std::string_view>& expectedOrder)
{
    const auto missions = bayou::client::storyMissions(campaign);
    check(missions.size() == expectedCount, "campaign has its expected expanded node count");
    check(missions.size() == expectedOrder.size(), "expected-order fixture matches node count");
    check(
        static_cast<std::size_t>(std::count_if(
            missions.begin(), missions.end(), [](const StoryMission& mission) {
                return mission.objectiveSpec.kind != StoryObjectiveKind::StoryOnly;
            })) == expectedTacticalCount,
        "campaign has its expected authored tactical lesson count");

    std::set<std::string_view> ids;
    std::set<std::string_view> roles;
    for (std::size_t index = 0; index < missions.size(); ++index)
    {
        const StoryMission& mission = missions[index];
        check(!mission.id.empty(), "mission has a stable ID");
        check(ids.insert(mission.id).second, "mission ID is unique inside its campaign");
        check(!mission.title.empty(), "mission has a title");
        check(!mission.briefing.empty(), "mission has at least one story panel");
        check(
            mission.objectiveSpec.kind != StoryObjectiveKind::Legacy,
            "mission never relies on legacy numeric setup");
        if (index < expectedOrder.size())
        {
            check(mission.id == expectedOrder[index], "mission appears in canonical order");
        }

        roles.clear();
        for (const auto& placement : mission.pieces)
        {
            check(!placement.role.empty(), "placed piece has a stable role");
            check(roles.insert(placement.role).second, "placed role is unique in mission");
            check(
                bayou::client::packagedStoryCard(placement.cardTitle).has_value(),
                "placed card has a reviewed packaged definition");
        }
        for (const std::string_view card : mission.playerHand)
        {
            check(
                bayou::client::packagedStoryCard(card).has_value(),
                "player-hand card has a reviewed packaged definition");
        }
        for (const std::string_view card : mission.enemyHand)
        {
            check(
                bayou::client::packagedStoryCard(card).has_value(),
                "enemy-hand card has a reviewed packaged definition");
        }
        for (const std::string_view card : mission.playerDrawPile)
        {
            check(
                bayou::client::packagedStoryCard(card).has_value(),
                "player draw-pile card has a reviewed packaged definition");
        }
        for (const std::string_view card : mission.enemyDrawPile)
        {
            check(
                bayou::client::packagedStoryCard(card).has_value(),
                "enemy draw-pile card has a reviewed packaged definition");
        }
        for (const std::string_view card : mission.playerDeck)
        {
            check(
                bayou::client::packagedStoryCard(card).has_value(),
                "standard player-deck card has a reviewed packaged definition");
        }
        for (const std::string_view card : mission.enemyDeck)
        {
            check(
                bayou::client::packagedStoryCard(card).has_value(),
                "standard enemy-deck card has a reviewed packaged definition");
        }
        const auto packagedClosure = bayou::client::validateStoryCardDependencies(
            mission,
            {},
            {},
            StoryCardResolutionMode::AllowPackagedFixtureFallback);
        check(
            packagedClosure.complete(),
            "every direct and transitive Story card dependency has a reviewed packaged definition");
        if (mission.standardMatch)
        {
            const auto deckShapeIsOrdinary = [](const std::vector<std::string_view>& deck) {
                int heroes = 0;
                int heroCost = 0;
                int nonHeroes = 0;
                for (std::string_view title : deck)
                {
                    const auto card = bayou::client::packagedStoryCard(title);
                    if (!card)
                    {
                        return false;
                    }
                    if (card->type == "Hero")
                    {
                        ++heroes;
                        heroCost += card->heroCost;
                    }
                    else
                    {
                        ++nonHeroes;
                    }
                }
                return nonHeroes == 20 && heroes >= 1 && heroes <= 4 && heroCost <= 100;
            };
            check(
                mission.objectiveSpec.kind == StoryObjectiveKind::DefeatAllEnemies,
                "standard match uses ordinary opposing-Hero elimination framing");
            check(
                mission.pieces.empty() && mission.playerHand.empty() &&
                    mission.enemyHand.empty() && mission.playerDrawPile.empty() &&
                    mission.enemyDrawPile.empty() && mission.script.empty(),
                "standard match has no authored board, hand, draw order, or script");
            check(
                deckShapeIsOrdinary(mission.playerDeck) &&
                    deckShapeIsOrdinary(mission.enemyDeck),
                "standard match decks have 20 non-Hero cards and a legal Hero roster shape");
            check(
                mission.masteryCards.empty() && mission.masteryRules.empty() &&
                    mission.requiredSurvivorRoles.empty(),
                "standard match adds no scripted mastery award or tutorial-only survivor rule");
        }
        else if (mission.objectiveSpec.kind == StoryObjectiveKind::Scripted)
        {
            check(!mission.pieces.empty(), "tactical mission has an authored setup");
            check(!mission.script.empty(), "tactical mission has a deterministic script");
            check(
                !mission.masteryCards.empty() || !mission.masteryRules.empty(),
                "tactical mission names mastered card or rule evidence");
            check(!mission.masteryRules.empty(), "tactical mission names mastered rules");
        }
        else if (mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly)
        {
            check(mission.script.empty(), "story beat has no fake tactical script");
            check(mission.masteryCards.empty(), "story beat awards no card mastery");
            check(mission.masteryRules.empty(), "story beat awards no rule mastery");
        }
        else
        {
            check(!mission.pieces.empty(), "open tactical mission has an authored setup");
            check(mission.script.empty(), "open tactical mission releases normal player control");
            check(
                mission.masteryCards.empty() && mission.masteryRules.empty(),
                "open tactical mission awards no unverified deterministic mastery");
        }
        if (mission.objectiveSpec.kind == StoryObjectiveKind::DefeatRole ||
            mission.objectiveSpec.kind == StoryObjectiveKind::ReachSquare)
        {
            check(
                roles.contains(mission.objectiveSpec.targetRole),
                "role-based objective resolves to a placed role");
        }
        if (mission.objectiveSpec.kind == StoryObjectiveKind::DeployCard)
        {
            check(
                bayou::client::packagedStoryCard(mission.objectiveSpec.cardTitle).has_value(),
                "deploy objective names a reviewed packaged card");
        }
        if (mission.objectiveSpec.kind == StoryObjectiveKind::ControlSquares)
        {
            check(mission.objectiveSpec.amount > 0, "control objective has a positive target");
        }
        for (const std::string_view role : mission.requiredSurvivorRoles)
        {
            check(roles.contains(role), "required survivor resolves to a placed role");
        }
        std::set<std::string_view> knownRoles = roles;
        for (const auto& step : mission.script)
        {
            if (!step.actorRole.empty())
            {
                check(knownRoles.contains(step.actorRole), "script actor resolves to a known role");
            }
            if (!step.targetRole.empty())
            {
                check(knownRoles.contains(step.targetRole), "script target resolves to a known role");
            }
            if (!step.effectRole.empty())
            {
                if (step.kind == StoryActionKind::PlayCard)
                {
                    check(
                        knownRoles.insert(step.effectRole).second,
                        "deployed card creates a unique dynamic role");
                }
                else
                {
                    check(
                        knownRoles.contains(step.effectRole),
                        "script effect resolves to a known role");
                }
            }
        }

        for (const std::string_view card : mission.masteryCards)
        {
            check(
                bayou::client::packagedStoryCard(card).has_value(),
                "mastered card has a packaged definition");
            bool activeUse = std::any_of(
                mission.script.begin(), mission.script.end(), [&](const auto& step) {
                    if (step.cardTitle == card)
                    {
                        return true;
                    }
                    const auto placement = std::find_if(
                        mission.pieces.begin(), mission.pieces.end(), [&](const auto& value) {
                            return value.role == step.actorRole;
                        });
                    if (placement != mission.pieces.end() && placement->cardTitle == card)
                    {
                        return true;
                    }
                    return std::any_of(
                        mission.script.begin(), mission.script.end(), [&](const auto& producer) {
                            return producer.kind == StoryActionKind::PlayCard &&
                                producer.effectRole == step.actorRole &&
                                producer.cardTitle == card;
                        });
                });
            if (!activeUse && card == "Birdie the Wise")
            {
                const bool placed = std::any_of(
                    mission.pieces.begin(), mission.pieces.end(), [&](const auto& value) {
                        return value.cardTitle == card;
                    });
                const bool affectedDraw = std::any_of(
                    mission.script.begin(), mission.script.end(), [](const auto& step) {
                        return step.kind == StoryActionKind::DrawCard;
                    });
                activeUse = placed && affectedDraw;
            }
            if (!activeUse)
            {
                const auto definition = bayou::client::packagedStoryCard(card);
                const bool placed = std::any_of(
                    mission.pieces.begin(), mission.pieces.end(), [&](const auto& value) {
                        return value.cardTitle == card;
                    });
                const bool crossesTurnBoundary = std::any_of(
                    mission.script.begin(), mission.script.end(), [](const auto& step) {
                        return step.kind == StoryActionKind::EndTurn;
                    });
                const bool hasAutomaticRule = definition &&
                    (definition->healingAura > 0 || definition->tax > 0 ||
                     definition->gatherResources > 0);
                activeUse = placed && crossesTurnBoundary && hasAutomaticRule;
            }
            check(
                activeUse,
                std::string(mission.id) + ": mastered card acts or is played: " +
                    std::string(card));
        }
    }
}

void validateRoster(
    StoryCampaign campaign,
    const std::vector<std::string_view>& requiredCards)
{
    std::set<std::string_view> seen;
    std::set<std::string_view> mastered;
    for (const StoryMission& mission : bayou::client::storyMissions(campaign))
    {
        for (const auto& placement : mission.pieces)
        {
            if (placement.owner == 1)
            {
                seen.insert(placement.cardTitle);
            }
        }
        seen.insert(mission.playerHand.begin(), mission.playerHand.end());
        mastered.insert(mission.masteryCards.begin(), mission.masteryCards.end());
    }
    for (const std::string_view card : requiredCards)
    {
        check(seen.contains(card), std::string("starter spotlight exists: ") + std::string(card));
        check(
            mastered.contains(card),
            std::string("starter earns active mastery: ") + std::string(card));
    }
}

void validateCumulativeMasteryEvidence()
{
    std::size_t blackthornOptionalDrillsSkipped = 0;
    std::set<std::string> expectedBlackthornStarterRoster;
    if (const StoryMission* starterMatch =
            missionById(StoryCampaign::Blackthorn, "bt17b_open_mastery"))
    {
        expectedBlackthornStarterRoster.insert(
            starterMatch->playerDeck.begin(), starterMatch->playerDeck.end());
    }
    for (StoryCampaign campaign : {
             StoryCampaign::Mirewatch, StoryCampaign::Blackthorn, StoryCampaign::Seelie})
    {
        const int pathCount = campaign == StoryCampaign::Blackthorn ? 2 : 1;
        for (int pathIndex = 0; pathIndex < pathCount; ++pathIndex)
        {
        const bool skipEveryOptionalDrill = pathIndex == 1;
        std::unordered_map<std::string, std::set<std::string>> executedProfiles;
        std::unordered_map<std::string, std::set<std::string>> executedAbilities;
        std::set<std::string> observedRules;
        std::set<std::string> firstMasterySeen;
        std::set<std::string> pathEarnedMastery;

        for (const StoryMission& mission : bayou::client::storyMissions(campaign))
        {
            if (skipEveryOptionalDrill && mission.optionalRehearsal)
            {
                ++blackthornOptionalDrillsSkipped;
                continue;
            }
            std::unordered_map<std::string, std::string> roleCards;
            for (const StoryPiecePlacement& placement : mission.pieces)
            {
                roleCards.emplace(std::string(placement.role), std::string(placement.cardTitle));
            }

            for (const StoryScriptAction& step : mission.script)
            {
                if (step.kind == StoryActionKind::PlayCard && !step.effectRole.empty())
                {
                    roleCards.emplace(std::string(step.effectRole), std::string(step.cardTitle));
                }
                if (step.actorRole.empty())
                {
                    continue;
                }
                const auto actor = roleCards.find(std::string(step.actorRole));
                if (actor == roleCards.end())
                {
                    continue;
                }
                if (!step.expectedActionProfile.empty())
                {
                    executedProfiles[actor->second].insert(
                        std::string(step.expectedActionProfile));
                }
                if (!step.expectedAbility.empty())
                {
                    executedAbilities[actor->second].insert(
                        std::string(step.expectedAbility));
                }
            }
            observedRules.insert(mission.masteryRules.begin(), mission.masteryRules.end());
            pathEarnedMastery.insert(
                mission.masteryCards.begin(), mission.masteryCards.end());

            const auto hasRule = [&](std::string_view rule) {
                return observedRules.contains(std::string(rule));
            };
            const auto hasAnyRule = [&](std::initializer_list<std::string_view> rules) {
                return std::any_of(rules.begin(), rules.end(), hasRule);
            };

            for (std::string_view masteredTitle : mission.masteryCards)
            {
                if (!firstMasterySeen.emplace(masteredTitle).second)
                {
                    continue;
                }
                const auto card = bayou::client::packagedStoryCard(masteredTitle);
                check(
                    card.has_value(),
                    std::string(mission.id) + ": first mastery resolves card " +
                        std::string(masteredTitle));
                if (!card)
                {
                    continue;
                }
                const auto requireProfile = [&](const game_data::ActionProfile& action,
                                                std::string_view sourceTitle,
                                                std::string_view evidenceTitle) {
                    check(
                        executedProfiles[std::string(evidenceTitle)].contains(action.name),
                        std::string(mission.id) + ": first mastery for " +
                            std::string(masteredTitle) + " cumulatively executes " +
                            std::string(sourceTitle) + " profile " + action.name);
                    if (action.cooldownTurns > 0)
                    {
                        check(hasRule("cooldown"), "mastery cooldown has outcome evidence");
                    }
                    if (action.repeat > 0)
                    {
                        check(hasRule("repeat-lock"), "mastery Repeat has outcome evidence");
                    }
                    if (action.heal > 0)
                    {
                        check(hasRule("friendly-heal"), "mastery heal has outcome evidence");
                    }
                    if (action.statusTurns > 1)
                    {
                        check(
                            hasRule("disable-two-turns") && hasRule("status-duration"),
                            std::string(mission.id) + ": first mastery for " +
                                std::string(masteredTitle) + " proves " +
                                std::string(sourceTitle) + " profile " + action.name +
                                " through the full multi-turn status lifecycle");
                    }
                    if (action.control > 0)
                    {
                        check(
                            hasRule("zero-damage-control") &&
                                hasRule("control-restores-at-original-owner-turn-start"),
                            "mastery Control has application and restoration evidence");
                    }
                    if (action.pull)
                    {
                        check(
                            hasRule("pull-surviving-target"),
                            "mastery Pull has surviving-target outcome evidence");
                    }
                    if (action.push != 0)
                    {
                        check(hasRule("push"), "mastery Push has outcome evidence");
                    }
                    if (!action.infest.empty())
                    {
                        check(
                            hasRule("infest-spawns-allied-unit-on-death"),
                            "mastery Infest has death-replacement outcome evidence");
                    }
                    if (action.passThrough)
                    {
                        check(
                            hasAnyRule({"pass-through", "flying-pass-through"}),
                            "mastery pass-through has collision-aware movement evidence");
                    }
                    if (action.lineOfSight)
                    {
                        check(
                            hasAnyRule({"line-of-sight", "ranged-line-of-sight"}),
                            "mastery line of sight has outcome evidence");
                    }
                };

                for (const game_data::ActionProfile& action : card->actions)
                {
                    requireProfile(action, masteredTitle, masteredTitle);
                }
                if (!card->ability.empty())
                {
                    check(
                        executedAbilities[std::string(masteredTitle)].contains(card->ability),
                        std::string(mission.id) + ": first mastery for " +
                            std::string(masteredTitle) + " executes ability " + card->ability);
                }
                if (!card->rebirthTitle.empty())
                {
                    const auto dependent =
                        bayou::client::packagedStoryCard(card->rebirthTitle);
                    check(
                        dependent.has_value() && hasRule("rebirth"),
                        std::string(mission.id) + ": first mastery for " +
                            std::string(masteredTitle) +
                            " has Rebirth outcome and packaged dependent form");
                    if (dependent)
                    {
                        for (const game_data::ActionProfile& action : dependent->actions)
                        {
                            requireProfile(action, dependent->title, masteredTitle);
                        }
                    }
                }
                if (!card->summonTitle.empty() &&
                    mission.masteryIncludesDependentProfiles)
                {
                    const auto dependent =
                        bayou::client::packagedStoryCard(card->summonTitle);
                    check(
                        dependent.has_value(),
                        std::string(mission.id) + ": first mastery for " +
                            std::string(masteredTitle) +
                            " resolves its packaged Summon/Trail dependent form");
                    if (dependent)
                    {
                        for (const game_data::ActionProfile& action : dependent->actions)
                        {
                            requireProfile(action, dependent->title, dependent->title);
                        }
                        if (dependent->gatherResources > 0)
                        {
                            check(
                                hasRule("gather"),
                                "mastery dependent Gather has start-turn outcome evidence");
                        }
                    }
                }
                if (card->healingAura > 0)
                {
                    check(hasRule("healing-aura"), "mastery aura has end-turn outcome evidence");
                }
                if (card->tax > 0)
                {
                    check(hasRule("tax"), "mastery Tax has start-turn outcome evidence");
                }
                if (card->gatherResources > 0)
                {
                    check(hasRule("gather"), "mastery Gather has start-turn outcome evidence");
                }
                for (const std::string& keyword : card->keywords)
                {
                    if (keyword == "foresight")
                    {
                        check(hasRule("foresight"), "mastery Foresight has choice outcome evidence");
                    }
                    else if (keyword == "intercept")
                    {
                        check(
                            hasRule("intercept-automatic"),
                            "mastery Intercept has automatic redirect outcome evidence");
                    }
                    else if (keyword == "Reveal")
                    {
                        check(
                            hasRule("passive-reveal-at-owner-end-turn"),
                            "mastery Reveal has owner-end-turn outcome evidence");
                    }
                    else if (keyword == "trail")
                    {
                        check(hasRule("trail"), "mastery Trail has spawn outcome evidence");
                    }
                    else if (keyword == "bodyguard")
                    {
                        check(
                            hasRule("bodyguard"),
                            "mastery Bodyguard has damage-redirection outcome evidence");
                    }
                    else if (keyword == "relentless")
                    {
                        check(
                            hasRule("relentless-optional-extra-action"),
                            "mastery Relentless has optional-action outcome evidence");
                    }
                }
            }
        }
        if (skipEveryOptionalDrill)
        {
            check(
                !expectedBlackthornStarterRoster.empty() &&
                    pathEarnedMastery.empty(),
                "skipping all optional Blackthorn practices never awards unplayed card mastery");
        }
        }
    }
    check(
        blackthornOptionalDrillsSkipped > 0,
        "mastery validator exercises an explicit Blackthorn path with every optional drill skipped");
}

void validateNoFabricatedGameplay()
{
    for (const std::string_view title : {
             "Story Chain", "Story Watcher", "Story Notice", "Story Evidence",
             "Story Barricade", "Story Gate", "Story Screen", "Story Cargo",
             "Story Witness", "Grass-Woman", "Hara Dole"})
    {
        check(
            !bayou::client::packagedStoryCard(title).has_value(),
            std::string("fabricated Story fixture has no packaged card: ") + std::string(title));
    }

    for (StoryCampaign campaign : {
             StoryCampaign::Mirewatch, StoryCampaign::Blackthorn, StoryCampaign::Seelie})
    {
        for (const StoryMission& mission : bayou::client::storyMissions(campaign))
        {
            if (mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly)
            {
                continue;
            }
            const auto isFabricated = [](std::string_view title) {
                return title.starts_with("Rule Lab ") || title.starts_with("Story ") ||
                    title == "Hara Dole";
            };
            for (const StoryPiecePlacement& placement : mission.pieces)
            {
                check(
                    !isFabricated(placement.cardTitle),
                    std::string(mission.id) + ": tactical piece is authoritative: " +
                        std::string(placement.cardTitle));
            }
            for (std::string_view card : mission.playerHand)
            {
                check(!isFabricated(card), std::string(mission.id) + ": player hand is authoritative");
            }
            for (std::string_view card : mission.enemyHand)
            {
                check(!isFabricated(card), std::string(mission.id) + ": enemy hand is authoritative");
            }
            for (std::string_view card : mission.playerDrawPile)
            {
                check(!isFabricated(card), std::string(mission.id) + ": draw pile is authoritative");
            }
            for (std::string_view card : mission.masteryCards)
            {
                check(
                    !isFabricated(card),
                    std::string(mission.id) + ": mastery never credits a fabricated card");
            }
            for (const StoryScriptAction& step : mission.script)
            {
                check(
                    step.kind != StoryActionKind::None,
                    std::string(mission.id) + ": every guided step is a normal engine action");
                const std::string guidedCopy =
                    std::string(step.heading) + " " + std::string(step.instruction) + " " +
                    std::string(step.correction);
                check(
                    guidedCopy.find("rigging") == std::string::npos &&
                        guidedCopy.find("Rigging") == std::string::npos &&
                        guidedCopy.find("cargo") == std::string::npos &&
                        guidedCopy.find("Cargo") == std::string::npos,
                    std::string(mission.id) +
                        ": no guided step invents a rigging or cargo interaction");
                if (step.kind == StoryActionKind::Move || step.kind == StoryActionKind::Attack)
                {
                    check(
                        !step.expectedActionProfile.empty(),
                        std::string(mission.id) +
                            ": guided Move/Attack names one exact printed profile");
                    check(
                        (step.instruction.find("Click ") == std::string_view::npos &&
                             step.instruction.find("Drag ") == std::string_view::npos) ||
                            step.instruction.find("Keyboard:") != std::string_view::npos,
                        std::string(mission.id) +
                            ": move and attack instructions describe the game action without requiring one input device");
                }
                if (step.kind == StoryActionKind::UseAbility)
                {
                    const bool summonBound = step.expectedAbility != "summon" ||
                        (!step.expectedSummonTitle.empty() &&
                         step.expectedNextAbilityLabel.empty());
                    const bool transformBound = step.expectedAbility != "transform" ||
                        (step.expectedSummonTitle.empty() &&
                         !step.expectedNextAbilityLabel.empty());
                    check(
                        !step.expectedAbility.empty() &&
                            !step.expectedAbilityLabel.empty() &&
                            summonBound && transformBound,
                        std::string(mission.id) +
                            ": guided ability names exact identity, label, and applicable result");
                }
            }
            for (std::string_view rule : mission.masteryRules)
            {
                check(
                    rule != "scenario-interaction" && rule != "target-restraint" &&
                        rule != "protected-story-objective" && rule != "hazard-turn-clock" &&
                        rule != "separate-objective-custody" && rule != "protected-neutral" &&
                        rule != "autonomous-npc" && rule != "changing-objective" &&
                        rule != "evidence-versus-bait" && rule != "combined-arms" &&
                        rule != "artifact-custody" && rule != "durable-screen" &&
                        rule != "hero-preservation" && rule != "fragile-hero" &&
                        rule != "hidden-loses-control" && rule != "relentless-two-actions",
                    std::string(mission.id) + ": mastery counts only game rules");
            }
        }
    }
}

void validateTeachingCopy()
{
    const auto copyFor = [](const StoryMission& mission) {
        std::string text(mission.hint);
        const auto appendPanels = [&](const std::vector<StoryPanel>& panels) {
            for (const StoryPanel& panel : panels)
            {
                text += ' ';
                text += panel.text;
            }
        };
        appendPanels(mission.briefing);
        appendPanels(mission.aftermath);
        for (const StoryScriptAction& step : mission.script)
        {
            text += ' ';
            text += step.instruction;
            text += ' ';
            text += step.correction;
            appendPanels(step.panelsBefore);
        }
        std::transform(text.begin(), text.end(), text.begin(),
            [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        return text;
    };
    const auto has = [](const std::string& text,
                        std::initializer_list<std::string_view> phrases) {
        return std::all_of(phrases.begin(), phrases.end(),
            [&](std::string_view phrase) { return text.find(phrase) != std::string::npos; });
    };
    const auto boundStep = [](const StoryMission& mission, std::string_view role,
                              std::string_view profile) -> const StoryScriptAction* {
        const auto found = std::find_if(mission.script.begin(), mission.script.end(),
            [&](const StoryScriptAction& step) {
                return step.owner == 1 && step.actorRole == role &&
                    step.expectedActionProfile == profile;
            });
        return found == mission.script.end() ? nullptr : &*found;
    };
    for (StoryCampaign campaign : {
             StoryCampaign::Mirewatch, StoryCampaign::Blackthorn, StoryCampaign::Seelie})
    {
        const auto missions = storyMissions(campaign);
        const auto opening = std::find_if(missions.begin(), missions.end(),
            [](const StoryMission& mission) { return !mission.script.empty(); });
        check(
            opening != missions.end() &&
                has(copyFor(*opening), {"drag", "release", "keyboard", "enter"}),
            "each independently selectable campaign explains mouse and keyboard play");
    }

    const StoryMission* river = missionById(StoryCampaign::Mirewatch, "mw01_river_teeth");
    const StoryMission* scent = missionById(StoryCampaign::Blackthorn, "bt01_harness_hunger");
    const StoryMission* auction = missionById(StoryCampaign::Mirewatch, "mw10_beautiful_plan");
    const StoryMission* terms = missionById(StoryCampaign::Blackthorn, "bt04_terms_conditions");
    const StoryMission* capstone = missionById(StoryCampaign::Blackthorn, "bt17_natural_order");
    const StoryMission* cathedral = missionById(StoryCampaign::Seelie, "se01_cathedral_last_lights");
    const StoryMission* vault = missionById(StoryCampaign::Mirewatch, "mw11_no_plan_saves_all");
    const StoryMission* foresight = missionById(StoryCampaign::Seelie, "se04_reading_room");
    check(river && scent && auction && terms && capstone && cathedral && vault && foresight,
        "reviewed teaching missions exist");
    if (!river || !scent || !auction || !terms || !capstone || !cathedral || !vault || !foresight)
    {
        return;
    }

    const std::string openingCopy = copyFor(*river);
    const StoryScriptAction* firstMove = boundStep(*river, "reed", "Step");
    const StoryScriptAction* telos = boundStep(*river, "telos", "Travel");
    check(
        has(openingCopy, {"one ready piece", "end turn", "normally"}),
        "opening explains the guide and the ordinary one-piece action economy");
    check(
        has(openingCopy, {"health", "blue", "red", "1 damage", "0"}),
        "opening explains Health colors and damage reducing Health to zero");
    check(
        firstMove && firstMove->targetRow == 5 && firstMove->targetColumn == 2 &&
            firstMove->instruction.find("C6") != std::string_view::npos &&
            firstMove->instruction.find("release") != std::string_view::npos &&
            firstMove->instruction.find("Enter") != std::string_view::npos,
        "first move gives its exact destination and both input methods");
    check(
        telos && telos->targetRow == 4 && telos->targetColumn == 0 &&
            telos->instruction.find("A5") != std::string_view::npos &&
            telos->instruction.find("shot") != std::string_view::npos,
        "Telos uses real Travel to clear Reed's shot");

    const auto choice = std::find_if(foresight->script.begin(), foresight->script.end(),
        [](const StoryScriptAction& step) { return step.kind == StoryActionKind::ChooseForesight; });
    check(
        choice != foresight->script.end() && choice->cardTitle == "Fey Messenger" &&
            choice->instruction.find("Fey Messenger") != std::string_view::npos &&
            choice->correction.find("Fey Messenger") != std::string_view::npos,
        "Foresight names its required card in the prompt and recovery instruction");
    check(
        std::find(scent->masteryRules.begin(), scent->masteryRules.end(),
            "disable-two-turns") == scent->masteryRules.end() &&
        std::find(scent->masteryRules.begin(), scent->masteryRules.end(),
            "status-duration") == scent->masteryRules.end() &&
        std::find(scent->masteryCards.begin(), scent->masteryCards.end(),
            "Blackthorn Alchemist") == scent->masteryCards.end(),
        "opening does not award mastery of an unproven second Disable turn");
    const StoryScriptAction* blade = boundStep(*auction, "vanya", "Blade Dance");
    check(
        blade && has(copyFor(*auction), {"repeat 1", "second attack", "before choosing another piece"}),
        "Blade Dance explains its repeat limit, required second attack, and action lock");
    // The engine replay separately proves the hidden collision and its damage.
    check(
        has(copyFor(*terms), {"ambush", "c6", "1 damage", "visible"}),
        "the hidden collision prompt explains the printed attack and visible result");
    check(
        has(copyFor(*capstone), {"relentless", "normal play", "end turn", "extra action"}),
        "Relentless remains optional in normal play");
    const auto summon = std::find_if(capstone->script.begin(), capstone->script.end(),
        [](const StoryScriptAction& step) {
            return step.owner == 1 && step.actorRole == "foreman" &&
                step.expectedAbility == "summon";
        });
    check(
        summon != capstone->script.end() &&
            summon->instruction.find("B7") != std::string_view::npos &&
            summon->correction.find("B7") != std::string_view::npos,
        "Commanded Summon names the real square in front of the Foreman");
    const StoryScriptAction* decree = boundStep(*cathedral, "duchess", "Dewbell's Decree");
    check(
        decree && decree->instruction.find("Dewbell's Decree") != std::string_view::npos &&
            decree->correction.find("Dewbell's Decree") != std::string_view::npos,
        "Dewbell coaching uses the exact printed action name");
    const std::string intercept = copyFor(*vault);
    check(
        !vault->aftermath.empty() &&
            has(intercept, {"intercept", "swap", "hit"}),
        "Intercept coaching explains the demonstrated swap and incoming hit; engine tests cover exceptions");
}

void validateNarrativeCoherence()
{
    // The game adaptation may simplify or replace novel scenes. Do not pin
    // shipping totals, legal procedure, peripheral names, or exact dialogue.
    // Keep the reading limits and distinct viewpoints testable; independent
    // editorial review checks whether the stories are clear and exciting.
    const auto wordCount = [](std::string_view text) {
        int count = 0;
        bool inWord = false;
        for (const unsigned char character : text)
        {
            const bool wordCharacter = std::isalnum(character) != 0;
            if (wordCharacter && !inWord)
            {
                ++count;
            }
            inWord = wordCharacter || (character == '\'' && inWord);
        }
        return count;
    };
    const auto panelCopy = [](const StoryMission& mission) {
        std::string copy;
        for (const StoryPanel& panel : mission.briefing)
        {
            copy += panel.text;
            copy += '\n';
        }
        return copy;
    };
    for (StoryCampaign campaign : {
             StoryCampaign::Mirewatch, StoryCampaign::Blackthorn, StoryCampaign::Seelie})
    {
        for (const StoryMission& mission : storyMissions(campaign))
        {
            const std::string label = std::string(mission.id) + ": ";
            check(
                !mission.objective.empty() && !mission.hint.empty(),
                label + "has a clear goal and a next-step hint");
            const bool storyOnly =
                mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly;
            if (storyOnly)
            {
                check(
                    mission.briefing.size() >= 2 && mission.briefing.size() <= 8,
                    label + "keeps a story scene to eight short beats or fewer");
            }
            const auto checkPanels = [&](const std::vector<StoryPanel>& panels) {
                for (const StoryPanel& panel : panels)
                {
                    check(
                        !panel.speaker.empty() && !panel.text.empty(),
                        label + "names its speaker and supplies a readable line");
                    check(
                        wordCount(panel.text) <= (storyOnly ? 60 : 75),
                        label + "keeps each story or coaching panel short");
                }
            };
            checkPanels(mission.briefing);
            checkPanels(mission.aftermath);
            for (const StoryScriptAction& step : mission.script)
            {
                checkPanels(step.panelsBefore);
                check(
                    !step.heading.empty() && !step.instruction.empty() &&
                        !step.correction.empty(),
                    label + "each guided action has a prompt and a correction");
                check(
                    wordCount(step.instruction) <= 85 &&
                        wordCount(step.correction) <= 60,
                    label + "guidance fits one focused instruction");
            }
        }
    }

    for (std::string_view id : {
             "s01_hospitality", "s03_published_mystery", "s06_town_owns_itself"})
    {
        const StoryMission* mirewatch = missionById(StoryCampaign::Mirewatch, id);
        const StoryMission* blackthorn = missionById(StoryCampaign::Blackthorn, id);
        check(
            mirewatch && blackthorn &&
                panelCopy(*mirewatch) != panelCopy(*blackthorn),
            std::string(id) + ": the two sides retain distinct points of view");
    }
}

void validateSeelieCardTruth()
{
    struct ExpectedCard
    {
        std::string_view title;
        int cost;
        int heroCost;
        int health;
        std::vector<std::string_view> traits;
        std::vector<std::string_view> actions;
    };
    const std::array<ExpectedCard, 21> expected = {{
        {"Archivist Mosswake", 35, 0, 1, {"ancient", "wild"}, {"Rootwalk"}},
        {"Caltheriel", 1, 15, 1, {"fey", "honorable"}, {"Fey Flight", "Charm"}},
        {"Crystal Unicorn", 50, 0, 2, {"honorable", "wild"}, {"Prismatic Charge"}},
        {"Duchess Dewbell", 45, 0, 3, {"ancient"}, {"Dewbell's Decree"}},
        {"Fey Messenger", 25, 0, 1, {"fey"}, {"Courier Flight", "Star Dart"}},
        {"Heartwood Sister", 35, 0, 1, {"ancient", "fey"}, {"Grovewalk"}},
        {"Nettle Starbright", 30, 0, 2, {"wild"}, {"Fly", "Slingshot"}},
        {"Nettle Starbright Unmounted", 30, 0, 1, {"honorable", "wild"}, {"Punch"}},
        {"Pavo Quickstep", 30, 0, 2, {"arcane", "fey"}, {"Quickstep", "Enchanting Pipes"}},
        {"Prince Vesper", 1, 30, 1, {"arcane", "honorable"}, {"Royal Flight", "Vesper Spark"}},
        {"Quinberry Lark", 50, 0, 2, {"arcane"}, {"Swoop", "Sneak Around"}},
        {"Queen Nyxara", 1, 60, 3, {"ancient", "corrupt", "fey"}, {"Royal Advance", "Nyxaran Infest"}},
        {"Starbloom Knight", 40, 0, 4, {"fey", "honorable"}, {"Starbloom Charge"}},
        {"Sylvara", 1, 55, 2, {"ancient", "fey", "wild"}, {"Sovereign Thorns"}},
        {"Thorn Griffin", 60, 0, 4, {"wild"}, {"Griffin Flight"}},
        {"Bristlejack", 30, 0, 1, {"corrupt"}, {"Bristle Charge"}},
        {"Dusk Harvester", 45, 0, 5, {"corrupt", "fey", "undead"}, {"Scythe Advance", "Harvest"}},
        {"Eyeblight", 50, 0, 4, {"corrupt", "wild"}, {"Creeping Roots", "Blighting Gaze"}},
        {"Gloom Fairy", 30, 0, 1, {"fey", "undead"}, {"Fly", "Shoot Spark"}},
        {"Warden", 40, 0, 4, {"civilized", "honorable"}, {"RookMoveD1R3", "KingAttack1"}},
        {"Widowroot", 40, 0, 4, {"ancient"}, {"Rooted Strike", "Widow's Bind"}}
    }};

    const auto actionNamed = [](const game_data::GameCard& card, std::string_view name)
        -> const game_data::ActionProfile* {
        const auto found = std::find_if(
            card.actions.begin(), card.actions.end(),
            [&](const game_data::ActionProfile& action) { return action.name == name; });
        return found == card.actions.end() ? nullptr : &*found;
    };

    for (const ExpectedCard& truth : expected)
    {
        const auto card = bayou::client::packagedStoryCard(truth.title);
        check(card.has_value(), std::string("Seelie live mirror exists: ") + std::string(truth.title));
        if (!card)
        {
            continue;
        }
        check(
            card->cost == truth.cost && card->heroCost == truth.heroCost &&
                card->health == truth.health,
            std::string(truth.title) + ": cost and Health match the audited live card");
        check(
            card->traits.size() == truth.traits.size() &&
                std::equal(
                    truth.traits.begin(), truth.traits.end(), card->traits.begin(),
                    [](std::string_view expectedTrait, const std::string& actualTrait) {
                        return expectedTrait == actualTrait;
                    }),
            std::string(truth.title) + ": traits match the deployed live schema-11 card");
        check(
            card->actions.size() == truth.actions.size(),
            std::string(truth.title) + ": no action is added to or removed from the live card");
        for (std::string_view action : truth.actions)
        {
            check(
                actionNamed(*card, action) != nullptr,
                std::string(truth.title) + ": exact live action exists: " +
                    std::string(action));
        }
    }

    const auto mosswake = bayou::client::packagedStoryCard("Archivist Mosswake");
    const auto caltheriel = bayou::client::packagedStoryCard("Caltheriel");
    const auto heartwood = bayou::client::packagedStoryCard("Heartwood Sister");
    const auto mountedNettle = bayou::client::packagedStoryCard("Nettle Starbright");
    const auto unmountedNettle =
        bayou::client::packagedStoryCard("Nettle Starbright Unmounted");
    const auto pavo = bayou::client::packagedStoryCard("Pavo Quickstep");
    const auto lark = bayou::client::packagedStoryCard("Quinberry Lark");
    const auto nyxara = bayou::client::packagedStoryCard("Queen Nyxara");
    const auto knight = bayou::client::packagedStoryCard("Starbloom Knight");
    const auto sylvara = bayou::client::packagedStoryCard("Sylvara");
    check(
        mosswake && std::find(mosswake->keywords.begin(), mosswake->keywords.end(), "foresight") !=
            mosswake->keywords.end(),
        "Archivist Mosswake supplies the exact live Foresight keyword");
    check(
        caltheriel && heartwood && caltheriel->healingAura == 1 &&
            heartwood->healingAura == 1,
        "Caltheriel and Heartwood Sister keep their exact one-point healing auras");
    check(
        mountedNettle && unmountedNettle &&
            mountedNettle->rebirthTitle == "Nettle Starbright Unmounted" &&
            actionNamed(*unmountedNettle, "Punch") != nullptr,
        "mounted Nettle Rebirth points to the real unmounted Punch card");
    check(
        pavo && actionNamed(*pavo, "Enchanting Pipes") &&
            actionNamed(*pavo, "Enchanting Pipes")->control == 2,
        "Pavo's Enchanting Pipes keeps live Control 2");
    check(
        lark && lark->ability == "dematerialize" &&
            actionNamed(*lark, "Sneak Around") &&
            actionNamed(*lark, "Sneak Around")->state == 1,
        "Quinberry's hidden action remains gated by live Dematerialize state");
    check(
        nyxara && nyxara->type == "Hero" &&
            actionNamed(*nyxara, "Royal Advance") &&
            actionNamed(*nyxara, "Royal Advance")->canMove &&
            !actionNamed(*nyxara, "Royal Advance")->canAttack &&
            actionNamed(*nyxara, "Royal Advance")->minRange == 1 &&
            actionNamed(*nyxara, "Royal Advance")->maxRange == 2 &&
            actionNamed(*nyxara, "Nyxaran Infest") &&
            !actionNamed(*nyxara, "Nyxaran Infest")->canMove &&
            actionNamed(*nyxara, "Nyxaran Infest")->canAttack &&
            actionNamed(*nyxara, "Nyxaran Infest")->damage == 1 &&
            actionNamed(*nyxara, "Nyxaran Infest")->minRange == 1 &&
            actionNamed(*nyxara, "Nyxaran Infest")->maxRange == 2 &&
            actionNamed(*nyxara, "Nyxaran Infest")->infest == "Bristlejack",
        "Queen Nyxara keeps the exact live Royal Advance and ranged diagonal Bristlejack Infest profile");
    check(
        knight && std::find(knight->keywords.begin(), knight->keywords.end(), "bodyguard") !=
            knight->keywords.end(),
        "Starbloom Knight keeps the live Bodyguard keyword");
    check(
        sylvara && sylvara->ability.empty() && sylvara->summonTitle == "Sapling" &&
            actionNamed(*sylvara, "Sovereign Thorns") &&
            actionNamed(*sylvara, "Sovereign Thorns")->statusTurns == 2,
        "Sylvara teaches Sovereign Thorns and never fabricates an unavailable Summon ability");
}

void validateSeelieDifficultyCurve()
{
    const auto unitCount = [](const StoryMission& mission, int owner) {
        return static_cast<int>(std::count_if(
            mission.pieces.begin(), mission.pieces.end(),
            [&](const auto& placement) { return placement.owner == owner; }));
    };
    const auto distinctUnitCount = [](const StoryMission& mission, int owner) {
        std::set<std::string_view> titles;
        for (const StoryPiecePlacement& placement : mission.pieces)
        {
            if (placement.owner == owner)
            {
                titles.insert(placement.cardTitle);
            }
        }
        return static_cast<int>(titles.size());
    };

    const StoryMission* firstOpen =
        missionById(StoryCampaign::Seelie, "se14_rules_under_pressure");
    const StoryMission* bridgeLesson =
        missionById(StoryCampaign::Seelie, "se02_broken_bridge");
    const auto bodyguardBoundaryStep = bridgeLesson == nullptr
        ? std::vector<StoryScriptAction>::const_iterator{}
        : std::find_if(
              bridgeLesson->script.begin(),
              bridgeLesson->script.end(),
              [](const StoryScriptAction& step) {
                  return step.heading == "THE KNIGHT TAKES THE HIT";
              });
    const StoryMission* signalFight =
        missionById(StoryCampaign::Seelie, "se17_warm_channel");
    const StoryMission* stairFight =
        missionById(StoryCampaign::Seelie, "se19_crypt_under_order");
    const StoryMission* routeFight =
        missionById(StoryCampaign::Seelie, "se23_controlled_fall");
    const StoryMission* capstone =
        missionById(StoryCampaign::Seelie, "se27_emperor_at_tree");
    const StoryMission* vacancyGate =
        missionById(StoryCampaign::Seelie, "se28a_two_men_one_vacancy");
    const StoryMission* witnessRail =
        missionById(StoryCampaign::Seelie, "se29e_hold_the_witness_rail");

    check(
        firstOpen && firstOpen->script.empty() &&
            firstOpen->objectiveSpec.kind == StoryObjectiveKind::ControlSquares &&
            firstOpen->objectiveSpec.amount == 22 &&
            !firstOpen->optionalRehearsal &&
            storyBriefingCopy(*firstOpen).find("influence") != std::string_view::npos &&
            std::find(
                firstOpen->requiredSurvivorRoles.begin(),
                firstOpen->requiredSurvivorRoles.end(),
                "pavo") != firstOpen->requiredSurvivorRoles.end() &&
            unitCount(*firstOpen, 1) == 3 && unitCount(*firstOpen, 2) == 2,
        "SE14 is a required three-versus-two first free-play bridge that practices territory influence before the finale");
    check(
        bridgeLesson && storyBriefingCopy(*bridgeLesson).find("messenger") != std::string_view::npos &&
            std::find(bridgeLesson->requiredSurvivorRoles.begin(),
                bridgeLesson->requiredSurvivorRoles.end(), "messenger") !=
                bridgeLesson->requiredSurvivorRoles.end(),
        "SE02 tells the player that its mandatory Messenger must survive");
    check(
        bridgeLesson && bodyguardBoundaryStep != bridgeLesson->script.end() &&
            storyBriefingCopy(*bridgeLesson).find("damage") != std::string_view::npos &&
            storyBriefingCopy(*bridgeLesson).find("effects that deal no damage") != std::string_view::npos &&
            bridgeLesson->hint.find("Push") == std::string_view::npos &&
            bridgeLesson->hint.find("Pull") == std::string_view::npos &&
            bridgeLesson->hint.find("Infest") == std::string_view::npos &&
            bridgeLesson->hint.find("Control") == std::string_view::npos &&
            bodyguardBoundaryStep->instruction.find("damage") !=
                std::string_view::npos,
        "SE02 teaches the Bodyguard damage boundary in plain language without naming later mechanics early");
    check(
        signalFight && signalFight->script.empty() &&
            unitCount(*signalFight, 1) == 4 && unitCount(*signalFight, 2) == 4 &&
            distinctUnitCount(*signalFight, 1) == 4 &&
            distinctUnitCount(*signalFight, 2) == 3,
        "SE17 raises pressure with a mixed four-versus-four formation");
    check(
        stairFight && stairFight->script.empty() &&
            unitCount(*stairFight, 1) == 5 && unitCount(*stairFight, 2) == 5 &&
            distinctUnitCount(*stairFight, 1) == 4 &&
            distinctUnitCount(*stairFight, 2) == 5,
        "SE19 adds a diverse five-versus-five combined-arms battle");
    check(
        routeFight && routeFight->script.empty() &&
            routeFight->objectiveSpec.kind == StoryObjectiveKind::ReachSquare &&
            unitCount(*routeFight, 1) == 4 && unitCount(*routeFight, 2) == 4,
        "SE23 adds a non-elimination objective under equal-force pressure");
    check(
        capstone && capstone->script.empty() &&
            !capstone->standardMatch &&
            capstone->objectiveSpec.kind == StoryObjectiveKind::DefeatAllEnemies &&
            unitCount(*capstone, 1) == 8 && unitCount(*capstone, 2) == 8 &&
            capstone->objective.find("Sylvara") != std::string_view::npos &&
            std::find(capstone->requiredSurvivorRoles.begin(),
                capstone->requiredSurvivorRoles.end(), "sylvara") !=
                capstone->requiredSurvivorRoles.end(),
        "SE27 is the largest open formation and discloses Sylvara's survival condition");
    check(
        vacancyGate && vacancyGate->script.empty() &&
            vacancyGate->objectiveSpec.kind == StoryObjectiveKind::ReachSquare &&
            vacancyGate->objectiveSpec.targetRole == "rowan" &&
            vacancyGate->objectiveSpec.targetRow == 3 &&
            vacancyGate->objectiveSpec.targetColumn == 6 &&
            unitCount(*vacancyGate, 1) == 4 && unitCount(*vacancyGate, 2) == 4 &&
            vacancyGate->requiredSurvivorRoles == std::vector<std::string_view>{"rowan"},
        "SE28a is an open four-versus-four route objective with Rowan as the surviving mover");
    check(
        witnessRail && witnessRail->script.empty() &&
            witnessRail->objectiveSpec.kind == StoryObjectiveKind::ControlSquares &&
            witnessRail->objectiveSpec.amount == 28 &&
            unitCount(*witnessRail, 1) == 6 && unitCount(*witnessRail, 2) == 7 &&
            witnessRail->requiredSurvivorRoles ==
                std::vector<std::string_view>{"reed", "rowan"} &&
            witnessRail->masteryCards.empty() && witnessRail->masteryRules.empty(),
        "SE29e is an open six-versus-seven control objective that preserves Reed and Rowan");
}

void validateExpandedMirewatchBattles()
{
    const auto unitCount = [](const StoryMission& mission, int owner) {
        return static_cast<int>(std::count_if(
            mission.pieces.begin(), mission.pieces.end(),
            [&](const StoryPiecePlacement& placement) { return placement.owner == owner; }));
    };
    const auto hasRequiredSurvivor = [](const StoryMission& mission, std::string_view role) {
        return std::find(
                   mission.requiredSurvivorRoles.begin(),
                   mission.requiredSurvivorRoles.end(), role) !=
            mission.requiredSurvivorRoles.end();
    };

    const StoryMission* firstOpen =
        missionById(StoryCampaign::Mirewatch, "s02_no_one_alone");
    const StoryMission* toll =
        missionById(StoryCampaign::Mirewatch, "mw18_allies_dishonestly");
    const StoryMission* finale =
        missionById(StoryCampaign::Mirewatch, "mw24_agent_not_heir");
    check(
        firstOpen && firstOpen->optionalRehearsal && firstOpen->script.empty() &&
            firstOpen->objectiveSpec.kind == StoryObjectiveKind::DefeatAllEnemies &&
            unitCount(*firstOpen, 1) == 3 && unitCount(*firstOpen, 2) == 3 &&
            firstOpen->masteryCards.empty() && firstOpen->masteryRules.empty() &&
            hasRequiredSurvivor(*firstOpen, "reed") &&
            firstOpen->briefing.size() >= 2 && firstOpen->briefing.size() <= 11 &&
            storyBriefingCopy(*firstOpen).find("drag") != std::string::npos &&
            storyBriefingCopy(*firstOpen).find("release") != std::string::npos &&
            storyBriefingCopy(*firstOpen).find("enter") != std::string::npos &&
            storyBriefingCopy(*firstOpen).find("press i") != std::string::npos &&
            firstOpen->sourceChapter.find("practice") != std::string_view::npos &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Mirewatch, firstOpen->id) == 1,
        "Mirewatch retains a mastery-free optional 3v3 practice with clear controls after the ending");
    check(
        toll && toll->script.empty() &&
            toll->objectiveSpec.kind == StoryObjectiveKind::DefeatAllEnemies &&
            unitCount(*toll, 1) == 4 && unitCount(*toll, 2) == 4 &&
            hasRequiredSurvivor(*toll, "reed") &&
            hasRequiredSurvivor(*toll, "vanya") &&
            hasRequiredSurvivor(*toll, "birdie"),
        "MW18 is an open four-versus-four toll battle that preserves its three bounded allies");
    check(
        finale && finale->script.empty() &&
            finale->objectiveSpec.kind == StoryObjectiveKind::DefeatRole &&
            finale->objectiveSpec.targetRole == "grask" &&
            unitCount(*finale, 1) == 7 && unitCount(*finale, 2) == 7 &&
            hasRequiredSurvivor(*finale, "reed") &&
            hasRequiredSurvivor(*finale, "vanya") &&
            hasRequiredSurvivor(*finale, "joni") &&
            hasRequiredSurvivor(*finale, "victor") &&
            finale->requiredSurvivorRoles ==
                std::vector<std::string_view>{"reed", "vanya", "joni", "victor"} &&
            finale->objective.find("Victor") != std::string_view::npos &&
            std::any_of(
                finale->briefing.begin(), finale->briefing.end(),
                [](const StoryPanel& panel) {
                    return panel.text.find("Victor") !=
                        std::string_view::npos;
                }),
        "MW24 is an open seven-versus-seven Grask finale that protects Reed, Vanya, Joni, and Victor");
}

void validateBlackthornTrainingContract()
{
    const auto appendPanels = [](std::string& copy, const std::vector<StoryPanel>& panels) {
        for (const StoryPanel& panel : panels)
        {
            copy += ' ';
            copy += panel.speaker;
            copy += ' ';
            copy += panel.text;
        }
    };
    const auto marksRehearsal = [&](const StoryMission& mission) {
        std::string copy = std::string(mission.sourceChapter) + " " +
            std::string(mission.title) + " " + std::string(mission.lesson) + " " +
            std::string(mission.objective) + " " + std::string(mission.hint);
        appendPanels(copy, mission.briefing);
        appendPanels(copy, mission.aftermath);
        std::transform(copy.begin(), copy.end(), copy.begin(), [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        return copy.find("reconstruction") != std::string::npos ||
            copy.find("simulation") != std::string::npos ||
            copy.find("demonstration") != std::string::npos ||
            copy.find("drill") != std::string::npos ||
            copy.find("practice") != std::string::npos;
    };

    for (const StoryMission& mission : storyMissions(StoryCampaign::Blackthorn))
    {
        if (mission.objectiveSpec.kind != StoryObjectiveKind::StoryOnly)
        {
            check(mission.optionalRehearsal && !mission.catchUpForSkippedRehearsals,
                std::string(mission.id) + ": card practice is optional and creates no later story gate");
        }
    }

    constexpr std::array<std::string_view, 7> intendedDrills = {
        "bt01_harness_hunger",
        "bt02_customs_bell",
        "bt03_sanctuary_debt",
        "bt04_terms_conditions",
        "bt05_freight_office",
        "bt06_receipt_book",
        "bt15_monster_rules"};
    for (std::string_view id : intendedDrills)
    {
        const StoryMission* drill = missionById(StoryCampaign::Blackthorn, id);
        check(
            drill && drill->optionalRehearsal &&
                drill->objectiveSpec.kind != StoryObjectiveKind::StoryOnly &&
                marksRehearsal(*drill),
            std::string(id) + ": intended Blackthorn rules drill is tactical and optional");
    }

    const StoryMission* openMastery =
        missionById(StoryCampaign::Blackthorn, "bt17b_open_mastery");
    const StoryMission* fieldJudgment =
        missionById(StoryCampaign::Blackthorn, "bt17a_field_judgment");
    const StoryMission* guidedSynthesis =
        missionById(StoryCampaign::Blackthorn, "bt17_natural_order");
    const StoryMission* firstOpen =
        missionById(StoryCampaign::Blackthorn, "bt06_receipt_book");
    const StoryMission* monsterRules =
        missionById(StoryCampaign::Blackthorn, "bt15_monster_rules");
    const StoryMission* deployment =
        missionById(StoryCampaign::Blackthorn, "bt03_sanctuary_debt");
    const auto ownerCount = [](const StoryMission& mission, int owner) {
        return std::count_if(
            mission.pieces.begin(), mission.pieces.end(),
            [&](const StoryPiecePlacement& piece) { return piece.owner == owner; });
    };
    const auto requiresRole = [](const StoryMission& mission, std::string_view role) {
        return std::find(
                   mission.requiredSurvivorRoles.begin(),
                   mission.requiredSurvivorRoles.end(), role) !=
            mission.requiredSurvivorRoles.end();
    };
    check(
        firstOpen && firstOpen->optionalRehearsal && firstOpen->script.empty() &&
            firstOpen->objectiveSpec.kind == StoryObjectiveKind::DefeatAllEnemies &&
            ownerCount(*firstOpen, 1) == 3 && ownerCount(*firstOpen, 2) == 3 &&
            firstOpen->masteryCards.empty() && firstOpen->masteryRules.empty() &&
            requiresRole(*firstOpen, "thaeron") &&
            firstOpen->briefing.size() >= 2 && firstOpen->briefing.size() <= 11 &&
            storyBriefingCopy(*firstOpen).find("drag") != std::string::npos &&
            storyBriefingCopy(*firstOpen).find("release") != std::string::npos &&
            storyBriefingCopy(*firstOpen).find("enter") != std::string::npos &&
            storyBriefingCopy(*firstOpen).find("press i") != std::string::npos &&
            firstOpen->sourceChapter.find("non-canon open") != std::string_view::npos &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Blackthorn, firstOpen->id) == 1,
        "Blackthorn exposes a mastery-free optional 3v3 open check immediately after BT05 with clear controls");
    check(
        monsterRules &&
            monsterRules->pieces.size() == 2 &&
            ownerCount(*monsterRules, 1) == 1 &&
            ownerCount(*monsterRules, 2) == 1 &&
            std::count_if(
                monsterRules->pieces.begin(), monsterRules->pieces.end(),
                [](const StoryPiecePlacement& placement) {
                    return placement.cardTitle == "Ashenfang";
                }) == 1 &&
            std::none_of(
                monsterRules->pieces.begin(), monsterRules->pieces.end(),
                [](const StoryPiecePlacement& placement) {
                    return placement.cardTitle == "Sylvara";
                }) &&
            monsterRules->requiredSurvivorRoles.size() == 1 &&
            requiresRole(*monsterRules, "ashenfang") &&
            !requiresRole(*monsterRules, "sylvara"),
        "BT15 uses only Ashenfang and her neutral Veteran timing mark, without spectator units or a duplicated Sylvara body");
    check(
        guidedSynthesis && guidedSynthesis->optionalRehearsal &&
            !guidedSynthesis->catchUpForSkippedRehearsals &&
            !guidedSynthesis->script.empty(),
        "Blackthorn offers its full review after the story without requiring skipped earlier practice");
    if (guidedSynthesis)
    {
        const auto missions = storyMissions(StoryCampaign::Blackthorn);
        const int synthesisIndex = static_cast<int>(guidedSynthesis - missions.data());
        const int storyEnd = bayou::client::storyNarrativeEntryCount(StoryCampaign::Blackthorn);
        check(synthesisIndex >= storyEnd && fieldJudgment && openMastery &&
                fieldJudgment - missions.data() > synthesisIndex &&
                openMastery - missions.data() > fieldJudgment - missions.data(),
            "the optional review and two open practices follow the story ending in learning order");
        bayou::client::StoryProgress noPractice;
        noPractice.advancedCount = storyEnd;
        noPractice.completedByPlay.resize(missions.size(), false);
        check(!bayou::client::storyMasteryCatchUpMayBeSkipped(missions, synthesisIndex, noPractice),
            "optional review uses ordinary skip behavior, which awards no synthetic mastery");

        const int automaticPlayerPasses = static_cast<int>(std::count_if(
            guidedSynthesis->script.begin(), guidedSynthesis->script.end(),
            [&](const StoryScriptAction& step) {
                return step.owner == 1 && step.kind == StoryActionKind::EndTurn &&
                    bayou::client::storyScriptActionAutoResolves(
                        *guidedSynthesis, step);
            }));
        const int automaticOpponentPasses = static_cast<int>(std::count_if(
            guidedSynthesis->script.begin(), guidedSynthesis->script.end(),
            [&](const StoryScriptAction& step) {
                return step.owner == 2 && step.kind == StoryActionKind::EndTurn &&
                    bayou::client::storyScriptActionAutoResolves(
                        *guidedSynthesis, step);
            }));
        const bool everyPlayerInputIsPrinted = std::all_of(
            guidedSynthesis->script.begin(), guidedSynthesis->script.end(),
            [&](const StoryScriptAction& step) {
                if (!bayou::client::storyScriptActionRequiresPlayerInput(
                        *guidedSynthesis, step))
                {
                    return true;
                }
                const bool boundPieceAction =
                    (step.kind == StoryActionKind::Move ||
                     step.kind == StoryActionKind::Attack) &&
                    !step.expectedActionProfile.empty();
                const bool boundAbility =
                    step.kind == StoryActionKind::UseAbility &&
                    !step.expectedAbility.empty() &&
                    !step.expectedAbilityLabel.empty();
                return boundPieceAction || boundAbility;
            });
        const bool everyAutomaticBeatKeepsItsMessage = std::all_of(
            guidedSynthesis->script.begin(), guidedSynthesis->script.end(),
            [&](const StoryScriptAction& step) {
                return !bayou::client::storyScriptActionAutoResolves(
                           *guidedSynthesis, step) ||
                    (!step.heading.empty() && !step.instruction.empty());
            });
        const int sectionPopupCount = static_cast<int>(std::count_if(
            guidedSynthesis->script.begin(), guidedSynthesis->script.end(),
            [](const StoryScriptAction& step) {
                return !step.panelsBefore.empty();
            }));
        const bool noPrefixCanComplete = std::all_of(
            guidedSynthesis->script.begin(), guidedSynthesis->script.end(),
            [&](const StoryScriptAction& step) {
                const int nextStep = static_cast<int>(
                    &step - guidedSynthesis->script.data());
                return !bayou::client::storyScriptProgressAllowsCompletion(
                    *guidedSynthesis, nextStep);
            });
        std::string synthesisCopy = storyBriefingCopy(*guidedSynthesis);
        for (const StoryPanel& panel : guidedSynthesis->script.front().panelsBefore)
        {
            synthesisCopy += " " + std::string(panel.text);
        }
        std::transform(synthesisCopy.begin(), synthesisCopy.end(), synthesisCopy.begin(),
            [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        const bool briefingExplainsAutomation = synthesisCopy.find("26") != std::string::npos &&
            synthesisCopy.find("turns pass automatically") != std::string::npos;
        check(
            guidedSynthesis->autoResolvePlayerEndTurns &&
                automaticPlayerPasses == 24 &&
                automaticOpponentPasses == 24,
            "BT17 auto-resolves exactly 24 interstitial player passes and their 24 scripted opponent passes");
        check(
            bayou::client::storyGuidedPlayerActionCount(*guidedSynthesis) == 26 &&
                bayou::client::storyGuidedPlayerActionNumber(
                    *guidedSynthesis,
                    static_cast<int>(guidedSynthesis->script.size()) - 1) == 26 &&
                everyPlayerInputIsPrinted,
            "BT17 guided counter exposes exactly 26 bound printed action or ability inputs and excludes every automatic pass");
        check(
            noPrefixCanComplete &&
                bayou::client::storyScriptProgressAllowsCompletion(
                    *guidedSynthesis,
                    static_cast<int>(guidedSynthesis->script.size())) &&
                !guidedSynthesis->masteryCards.empty() &&
                !guidedSynthesis->masteryRules.empty(),
            "BT17 mastery remains fail-closed until every exact live script step has executed");
        check(
            briefingExplainsAutomation,
            "BT17 briefing explains its 26-input counter and authoritative automatic timing");
        check(
            everyAutomaticBeatKeepsItsMessage && sectionPopupCount == 3,
            "BT17 automation preserves every timing message and the first-action instructions and both section popups");
    }
    const int playerPassAutomationMissionCount = static_cast<int>(std::count_if(
        storyMissions(StoryCampaign::Blackthorn).begin(),
        storyMissions(StoryCampaign::Blackthorn).end(),
        [](const StoryMission& mission) {
            return mission.autoResolvePlayerEndTurns;
        }));
    check(
        playerPassAutomationMissionCount == 2 && deployment &&
            deployment->autoResolvePlayerEndTurns,
        "player End Turn automation is scoped to the dense BT03 dependent lesson and BT17 catch-up synthesis");
    check(
        fieldJudgment && fieldJudgment->script.empty() &&
            fieldJudgment->optionalRehearsal &&
            fieldJudgment->objectiveSpec.kind == StoryObjectiveKind::DefeatAllEnemies &&
            ownerCount(*fieldJudgment, 1) == 4 &&
            ownerCount(*fieldJudgment, 2) == 4 &&
            fieldJudgment->requiredSurvivorRoles.size() == 1 &&
            requiresRole(*fieldJudgment, "thaeron") &&
            fieldJudgment->hint.find("press I") != std::string_view::npos &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Blackthorn, fieldJudgment->id) == 2,
        "Blackthorn offers an optional four-versus-four depth-two battle after the story");
    check(
        openMastery && openMastery->script.empty() && openMastery->optionalRehearsal &&
            openMastery->standardMatch && openMastery->pieces.empty() &&
            openMastery->objectiveSpec.kind == StoryObjectiveKind::DefeatAllEnemies &&
            openMastery->playerDeck.size() == 22 &&
            openMastery->enemyDeck.size() == 22 &&
            openMastery->briefing.size() >= 6 && openMastery->briefing.size() <= 12 &&
            std::any_of(
                openMastery->briefing.begin(), openMastery->briefing.end(),
                [](const StoryPanel& panel) {
                    return panel.speaker == "Watch the clocks";
                }) &&
            std::any_of(
                openMastery->briefing.begin(), openMastery->briefing.end(),
                [](const StoryPanel& panel) {
                    return panel.speaker == "When time runs out";
                }) &&
            std::any_of(
                openMastery->briefing.begin(), openMastery->briefing.end(),
                [](const StoryPanel& panel) {
                    return panel.speaker == "Place your Heroes" &&
                        panel.text.find("Hold a Hero card") != std::string_view::npos &&
                        panel.text.find("empty glowing home square") !=
                            std::string_view::npos &&
                        panel.text.find("press Enter") !=
                            std::string_view::npos &&
                        panel.text.find("press Enter") != std::string_view::npos;
                }) &&
            openMastery->briefing.back().speaker == "Ready to start" &&
            openMastery->briefing.back().text.find("starts") !=
                std::string_view::npos &&
            openMastery->briefing.back().text.find("drag") !=
                std::string_view::npos &&
            openMastery->briefing.back().text.find("focus") !=
                std::string_view::npos &&
            std::count(openMastery->playerDeck.begin(), openMastery->playerDeck.end(),
                       std::string_view("Thaeron Baelstone")) == 1 &&
            std::count(openMastery->playerDeck.begin(), openMastery->playerDeck.end(),
                       std::string_view("Ashenfang")) == 1 &&
            std::count(openMastery->enemyDeck.begin(), openMastery->enemyDeck.end(),
                       std::string_view("Maggie Mudroot")) == 1 &&
            std::count(openMastery->enemyDeck.begin(), openMastery->enemyDeck.end(),
                       std::string_view("Joni Pumpernickel")) == 1 &&
            openMastery->requiredSurvivorRoles.empty() &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Blackthorn, openMastery->id) == 3,
        "Blackthorn offers a skippable legal full match after its optional open practice");
}

bool configureAuthoritativeObjective(
    const StoryMission& mission,
    const std::unordered_map<std::string_view, int>& roleIds,
    GameEngine& engine)
{
    if (roleIds.size() != mission.pieces.size())
    {
        return false;
    }
    const auto roleId = [&](std::string_view role) {
        const auto found = roleIds.find(role);
        return found == roleIds.end() ? 0 : found->second;
    };

    GameEngine::ScenarioObjective objective;
    objective.successPlayer = 1;
    objective.requiredForceOriginalOwner = 1;
    switch (mission.objectiveSpec.kind)
    {
    case StoryObjectiveKind::DefeatAllEnemies:
        objective.kind = GameEngine::ScenarioObjectiveKind::DefeatOriginalOwner;
        objective.opposingOriginalOwner = 2;
        break;
    case StoryObjectiveKind::DefeatRole:
        objective.kind = GameEngine::ScenarioObjectiveKind::DefeatPiece;
        objective.targetPieceId = roleId(mission.objectiveSpec.targetRole);
        break;
    case StoryObjectiveKind::ReachSquare:
        objective.kind = GameEngine::ScenarioObjectiveKind::ReachSquare;
        objective.targetPieceId = roleId(mission.objectiveSpec.targetRole);
        objective.targetRow = mission.objectiveSpec.targetRow;
        objective.targetColumn = mission.objectiveSpec.targetColumn;
        break;
    case StoryObjectiveKind::ControlSquares:
        objective.kind = GameEngine::ScenarioObjectiveKind::ControlSquares;
        objective.controlAmount = mission.objectiveSpec.amount;
        break;
    case StoryObjectiveKind::Scripted:
    case StoryObjectiveKind::DeployCard:
    case StoryObjectiveKind::Legacy:
    case StoryObjectiveKind::StoryOnly:
        objective.kind = GameEngine::ScenarioObjectiveKind::None;
        break;
    }
    for (std::string_view role : mission.requiredSurvivorRoles)
    {
        const int id = roleId(role);
        if (id == 0)
        {
            return false;
        }
        objective.requiredSurvivorPieceIds.push_back(id);
    }
    return engine.configureScenarioObjective(std::move(objective));
}

struct ScriptReplayStats
{
    std::size_t executedSteps = 0;
    std::size_t playerInputs = 0;
    std::size_t automaticSteps = 0;
};

bool replayScript(
    const StoryMission& mission,
    GameEngine& engine,
    ScriptReplayStats* stats = nullptr,
    std::optional<std::size_t> stopAfterSteps = std::nullopt)
{
    if (stats)
    {
        *stats = {};
    }
    std::vector<GameEngine::ScenarioPiece> scenario;
    std::vector<game_data::GameCard> playerHand;
    std::vector<game_data::GameCard> enemyHand;
    std::vector<game_data::GameCard> playerDrawPile;
    std::vector<game_data::GameCard> enemyDrawPile;
    for (const auto& placement : mission.pieces)
    {
        const auto card = bayou::client::packagedStoryCard(placement.cardTitle);
        if (!card)
        {
            return false;
        }
        scenario.push_back({
            placement.owner,
            *card,
            placement.row,
            placement.column,
            placement.isHero,
            placement.initialHealth});
    }
    for (const std::string_view title : mission.playerHand)
    {
        if (const auto card = bayou::client::packagedStoryCard(title))
        {
            playerHand.push_back(*card);
        }
    }
    for (const std::string_view title : mission.enemyHand)
    {
        if (const auto card = bayou::client::packagedStoryCard(title))
        {
            enemyHand.push_back(*card);
        }
    }
    for (const std::string_view title : mission.playerDrawPile)
    {
        if (const auto card = bayou::client::packagedStoryCard(title))
        {
            playerDrawPile.push_back(*card);
        }
    }
    for (const std::string_view title : mission.enemyDrawPile)
    {
        if (const auto card = bayou::client::packagedStoryCard(title))
        {
            enemyDrawPile.push_back(*card);
        }
    }
    for (std::string_view title : {
             "Blackthorn Lumberjack", "Sapling", "Swamp Tracker Unmounted",
             "Nettle Starbright Unmounted", "Bristlejack"})
    {
        if (const auto card = bayou::client::packagedStoryCard(title))
        {
            engine.registerScenarioCard(*card);
        }
    }
    engine.loadScenario(
        scenario,
        std::move(playerHand),
        std::move(enemyHand),
        mission.playerResources,
        mission.enemyResources,
        mission.firstPlayer,
        std::string(mission.objective),
        false,
        std::move(playerDrawPile),
        std::move(enemyDrawPile));

    std::unordered_map<std::string_view, int> roleIds;
    for (const auto& placement : mission.pieces)
    {
        const auto found = std::find_if(
            engine.boardPieces().begin(), engine.boardPieces().end(),
            [&](const game_data::Piece& value) {
                return value.owner == placement.owner &&
                    value.name == placement.cardTitle &&
                    value.row == placement.row &&
                    value.column == placement.column;
            });
        if (found == engine.boardPieces().end())
        {
            return false;
        }
        roleIds.emplace(placement.role, found->id);
    }

    if (!configureAuthoritativeObjective(mission, roleIds, engine))
    {
        return false;
    }

    const auto refreshRebirthRoles = [&](const std::vector<game_data::Piece>& beforePieces) {
        for (auto& [role, pieceId] : roleIds)
        {
            const bool stillPresent = std::any_of(
                engine.boardPieces().begin(), engine.boardPieces().end(),
                [&](const game_data::Piece& value) { return value.id == pieceId; });
            if (stillPresent)
            {
                continue;
            }

            const auto placement = std::find_if(
                mission.pieces.begin(), mission.pieces.end(),
                [&](const StoryPiecePlacement& value) { return value.role == role; });
            if (placement == mission.pieces.end())
            {
                continue;
            }
            const auto originalCard =
                bayou::client::packagedStoryCard(placement->cardTitle);
            if (!originalCard || originalCard->rebirthTitle.empty())
            {
                continue;
            }

            const auto previousPiece = std::find_if(
                beforePieces.begin(), beforePieces.end(),
                [&](const game_data::Piece& value) { return value.id == pieceId; });
            if (previousPiece == beforePieces.end())
            {
                continue;
            }
            const auto replacement = std::find_if(
                engine.boardPieces().begin(), engine.boardPieces().end(),
                [&](const game_data::Piece& value) {
                    return value.owner == placement->owner &&
                        value.name == originalCard->rebirthTitle &&
                        value.row == previousPiece->row &&
                        value.column == previousPiece->column;
                });
            if (replacement != engine.boardPieces().end())
            {
                pieceId = replacement->id;
            }
        }
    };

    bayou::storytest::ClaimCoverage claimCoverage;
    const std::size_t replayStepCount = std::min(
        stopAfterSteps.value_or(mission.script.size()), mission.script.size());
    for (std::size_t storyStepIndex = 0;
         storyStepIndex < replayStepCount;
         ++storyStepIndex)
    {
        const auto& step = mission.script[storyStepIndex];
        const bayou::storytest::EngineState beforeState =
            bayou::storytest::captureEngineState(engine);
        const bayou::storytest::RoleIds beforeRoleIds = roleIds;
        const std::vector<game_data::Piece>& beforePieces = beforeState.pieces;
        int targetRow = step.targetRow;
        int targetColumn = step.targetColumn;
        if (!step.targetRole.empty())
        {
            const int targetId = roleIds.at(step.targetRole);
            const auto target = std::find_if(
                engine.boardPieces().begin(), engine.boardPieces().end(),
                [&](const game_data::Piece& value) { return value.id == targetId; });
            if (target == engine.boardPieces().end())
            {
                return false;
            }
            targetRow = target->row;
            targetColumn = target->column;
        }
        const int actorId =
            step.actorRole.empty() ? 0 : roleIds.at(step.actorRole);
        std::optional<int> expectedActionIndex;
        std::optional<game_data::Piece> abilityActorBefore;
        if (step.kind == StoryActionKind::Move || step.kind == StoryActionKind::Attack)
        {
            const auto actor = std::find_if(
                engine.boardPieces().begin(), engine.boardPieces().end(),
                [&](const game_data::Piece& value) { return value.id == actorId; });
            if (actor == engine.boardPieces().end())
            {
                return false;
            }
            expectedActionIndex =
                bayou::client::storyExpectedActionProfileIndex(step, *actor);
            if (!expectedActionIndex)
            {
                fmt::println(
                    "[FAIL] {} has no unique active profile named '{}' for scripted step: {}",
                    mission.id,
                    step.expectedActionProfile,
                    step.heading);
                return false;
            }
        }
        else if (step.kind == StoryActionKind::UseAbility)
        {
            const auto actor = std::find_if(
                engine.boardPieces().begin(), engine.boardPieces().end(),
                [&](const game_data::Piece& value) { return value.id == actorId; });
            if (actor == engine.boardPieces().end() ||
                !bayou::client::storyAbilityStepMatches(step, *actor))
            {
                fmt::println(
                    "[FAIL] {} no longer has exact ability '{}' / '{}' for scripted step: {}",
                    mission.id,
                    step.expectedAbility,
                    step.expectedAbilityLabel,
                    step.heading);
                return false;
            }
            abilityActorBefore = *actor;
        }
        bool accepted = false;
        switch (step.kind)
        {
        case StoryActionKind::Move:
            accepted = engine.movePiece(
                step.owner,
                actorId,
                targetRow,
                targetColumn,
                *expectedActionIndex);
            break;
        case StoryActionKind::Attack:
            accepted = engine.attackPiece(
                step.owner,
                actorId,
                targetRow,
                targetColumn,
                *expectedActionIndex);
            break;
        case StoryActionKind::UseAbility:
            accepted = engine.useAbility(step.owner, actorId);
            break;
        case StoryActionKind::PlayCard:
        {
            const auto& hand = engine.playerState(step.owner).hand;
            const auto found = std::find_if(
                hand.begin(), hand.end(), [&](const game_data::GameCard& card) {
                    return card.title == step.cardTitle;
                });
            accepted = found != hand.end() && engine.playCard(
                step.owner,
                static_cast<int>(std::distance(hand.begin(), found)),
                targetRow,
                targetColumn);
            if (accepted && !step.effectRole.empty())
            {
                const auto spawned = std::find_if(
                    engine.boardPieces().begin(), engine.boardPieces().end(),
                    [&](const game_data::Piece& piece) {
                        return piece.owner == step.owner && piece.name == step.cardTitle &&
                            piece.row == targetRow && piece.column == targetColumn &&
                            piece.hasActed;
                    });
                if (spawned == engine.boardPieces().end())
                {
                    return false;
                }
                roleIds.emplace(step.effectRole, spawned->id);
            }
            break;
        }
        case StoryActionKind::DrawCard:
            accepted = engine.drawCard(step.owner);
            break;
        case StoryActionKind::ChooseForesight:
        {
            const auto& choices = engine.playerState(step.owner).foresightChoices;
            const auto found = std::find_if(
                choices.begin(), choices.end(), [&](const game_data::GameCard& card) {
                    return card.title == step.cardTitle;
                });
            accepted = found != choices.end() && engine.chooseForesightCard(
                step.owner, static_cast<int>(std::distance(choices.begin(), found)));
            break;
        }
        case StoryActionKind::DiscardCard:
        {
            const auto& hand = engine.playerState(step.owner).hand;
            const auto found = std::find_if(
                hand.begin(), hand.end(), [&](const game_data::GameCard& card) {
                    return card.title == step.cardTitle;
                });
            accepted = found != hand.end() && engine.discardCard(
                step.owner, static_cast<int>(std::distance(hand.begin(), found)));
            break;
        }
        case StoryActionKind::EndTurn:
            accepted = engine.endTurn(step.owner);
            break;
        default:
            break;
        }
        if (!accepted)
        {
            const auto actor = std::find_if(
                engine.boardPieces().begin(), engine.boardPieces().end(),
                [&](const game_data::Piece& value) { return value.id == actorId; });
            fmt::println(
                "[FAIL] {} rejected scripted step: {} (active P{}, status: {})",
                mission.id,
                step.heading,
                engine.currentPlayer(),
                engine.snapshotFor(step.owner).status);
            if (actor != engine.boardPieces().end())
            {
                fmt::println(
                    "       actor {} at ({},{}), H{}, acted={}, disabled={}, sleep={}, state={}",
                    actor->name,
                    actor->row,
                    actor->column,
                    actor->health,
                    actor->hasActed,
                    actor->disabledTurns,
                    actor->sleepTurnsRemaining,
                    actor->actionState);
            }
            return false;
        }
        if (abilityActorBefore &&
            !bayou::client::storyAbilityOutcomeMatches(
                step,
                *abilityActorBefore,
                engine.boardPieces(),
                engine.commandingPiece()))
        {
            fmt::println(
                "[FAIL] {} exact ability did not produce its authored outcome: {}",
                mission.id,
                step.heading);
            return false;
        }
        const bayou::storytest::ClaimResult claimResult =
            bayou::storytest::verifyStepOutcome(
                mission,
                step,
                storyStepIndex,
                beforeState,
                bayou::storytest::captureEngineState(engine),
                beforeRoleIds,
                roleIds,
                claimCoverage);
        if (!claimResult.ok)
        {
            fmt::println("[FAIL] {}", claimResult.error);
            return false;
        }
        if (stats)
        {
            ++stats->executedSteps;
            if (bayou::client::storyScriptActionRequiresPlayerInput(
                    mission, step))
            {
                ++stats->playerInputs;
            }
            else if (bayou::client::storyScriptActionAutoResolves(
                         mission, step))
            {
                ++stats->automaticSteps;
            }
        }
        refreshRebirthRoles(beforePieces);
    }
    if (replayStepCount < mission.script.size())
    {
        return true;
    }
    const bayou::storytest::ClaimResult coverageResult =
        bayou::storytest::verifyMissionClaimCoverage(mission, claimCoverage);
    if (!coverageResult.ok)
    {
        fmt::println("[FAIL] {}", coverageResult.error);
        return false;
    }
    return true;
}

bool validateStandardMatchPath(const StoryMission& mission)
{
    bool valid = true;
    const auto expect = [&](bool condition, std::string_view label) {
        check(condition, label);
        valid = valid && condition;
    };
    const auto resolvedDeck = [&](const std::vector<std::string_view>& titles) {
        std::vector<game_data::GameCard> deck;
        deck.reserve(titles.size());
        for (std::string_view title : titles)
        {
            const auto card = bayou::client::packagedStoryCard(title);
            if (!card)
            {
                valid = false;
                continue;
            }
            deck.push_back(*card);
        }
        return deck;
    };

    GameEngine engine(0x46554c4cu, {});
    engine.enableTimers();
    engine.submitResolvedDeck(1, resolvedDeck(mission.playerDeck));
    engine.submitResolvedDeck(2, resolvedDeck(mission.enemyDeck));
    expect(
        engine.bothDecksSubmitted() && engine.phase() == game_data::Phase::HeroPlacement,
        "standard Story capstone reaches ordinary Hero Placement through submitted decks");
    expect(
        engine.playerState(1).heroesToPlace.size() == 2 &&
            engine.playerState(2).heroesToPlace.size() == 2 &&
            engine.playerState(1).drawPile.size() == 20 &&
            engine.playerState(2).drawPile.size() == 20,
        "standard Story capstone splits two Heroes from each shuffled 20-card draw pile");
    expect(
        engine.timersAreEnabled() &&
            engine.snapshotFor(1).turnRemainingMs == GameEngine::FullTurnTimerMs &&
            engine.snapshotFor(1).players[0].clockRemainingMs == GameEngine::RegularClockMs,
        "standard Story capstone activates the ordinary placement and game clocks");

    const auto placeNamedHero = [&](int player, std::string_view title, int row, int column) {
        const auto& heroes = engine.playerState(player).heroesToPlace;
        const auto found = std::find_if(
            heroes.begin(), heroes.end(),
            [&](const game_data::GameCard& card) { return card.title == title; });
        return found != heroes.end() && engine.placeHero(
            player, static_cast<int>(std::distance(heroes.begin(), found)), row, column);
    };

    expect(
        !placeNamedHero(1, "Thaeron Baelstone", 0, 0),
        "ordinary Hero Placement rejects a square outside the two-by-four home zone");
    expect(
        placeNamedHero(1, "Thaeron Baelstone", 2, 0) &&
            placeNamedHero(1, "Ashenfang", 5, 1) &&
            engine.currentPlayer() == 2,
        "both Blackthorn Heroes place legally before placement passes to the opponent");
    expect(
        placeNamedHero(2, "Maggie Mudroot", 3, 6),
        "Maggie places only when her complete two-by-two footprint fits the opponent home zone");
    const auto maggie = std::find_if(
        engine.boardPieces().begin(), engine.boardPieces().end(),
        [](const game_data::Piece& piece) { return piece.name == "Maggie Mudroot"; });
    expect(
        maggie != engine.boardPieces().end() && maggie->isHero &&
            maggie->width == 2 && maggie->height == 2,
        "the capstone preserves Maggie's live two-by-two Hero footprint");
    const game_data::Snapshot concealedPlacementView = engine.snapshotFor(1);
    expect(
        std::none_of(
            concealedPlacementView.pieces.begin(), concealedPlacementView.pieces.end(),
            [](const game_data::Piece& piece) { return piece.owner == 2; }),
        "the opponent's placed Hero identity and position stay concealed during placement");
    expect(
        !placeNamedHero(2, "Joni Pumpernickel", 3, 7),
        "ordinary Hero Placement rejects overlap with any square of Maggie's footprint");
    expect(
        placeNamedHero(2, "Joni Pumpernickel", 2, 7) &&
            engine.phase() == game_data::Phase::Playing && engine.currentPlayer() == 1,
        "finishing both Hero rosters begins ordinary play with Player 1 active");

    const game_data::Snapshot playerView = engine.snapshotFor(1);
    expect(
        playerView.hand.size() == 4 &&
            playerView.players[0].handCount == 4 &&
            playerView.players[1].handCount == 4 &&
            playerView.players[0].drawPileCount == 16 &&
            playerView.players[1].drawPileCount == 16,
        "ordinary play deals four hidden starting cards and leaves 16 in each draw pile");
    expect(
        playerView.players[0].resources == playerView.players[0].controlledSquares &&
            playerView.players[0].resources > 0 &&
            playerView.players[1].resources == 0,
        "Player 1 receives start-turn income from controlled squares before taking the first turn");
    expect(
        static_cast<int>(std::count_if(
            playerView.pieces.begin(), playerView.pieces.end(),
            [](const game_data::Piece& piece) { return piece.owner == 2 && piece.isHero; })) == 2,
        "opposing Heroes become visible only after placement ends");

    game_data::GameCard finishingHero;
    finishingHero.title = "Victory Test Hero";
    finishingHero.type = "Hero";
    finishingHero.heroCost = 1;
    finishingHero.health = 2;
    game_data::ActionProfile finishingAttack;
    finishingAttack.name = "Victory Test";
    finishingAttack.kind = static_cast<std::uint8_t>(game_data::ActionKind::Ranged);
    finishingAttack.pattern = static_cast<std::uint8_t>(game_data::MovePattern::Ortho);
    finishingAttack.minRange = 1;
    finishingAttack.maxRange = 5;
    finishingAttack.damage = 2;
    finishingAttack.canMove = false;
    finishingAttack.canAttack = true;
    finishingHero.actions = {finishingAttack};
    game_data::GameCard targetHero = finishingHero;
    targetHero.title = "Last Enemy Hero";
    targetHero.actions.clear();
    GameEngine victoryEngine(0x56494354u, {});
    victoryEngine.submitResolvedDeck(1, {finishingHero});
    victoryEngine.submitResolvedDeck(2, {targetHero});
    const bool placedForVictory =
        victoryEngine.placeHero(1, 0, 2, 1) &&
        victoryEngine.placeHero(2, 0, 2, 6);
    const int attackerId = placedForVictory && !victoryEngine.boardPieces().empty()
        ? victoryEngine.boardPieces().front().id
        : 0;
    expect(
        placedForVictory && victoryEngine.attackPiece(1, attackerId, 2, 6) &&
            victoryEngine.phase() == game_data::Phase::GameOver &&
            victoryEngine.winner() == 1,
        "the ordinary submitted-deck path ends only when the last opposing Hero is destroyed");

    return valid;
}

void validateAdvancedOutcomes()
{
    if (const StoryMission* synthesis =
            missionById(StoryCampaign::Blackthorn, "bt17_natural_order"))
    {
        GameEngine restartedEngine(0x42545253u, {});
        ScriptReplayStats interruptedStats;
        ScriptReplayStats restartedStats;
        constexpr std::size_t InterruptedAfterSteps = 37;
        const bool interruptedReplay = replayScript(
            *synthesis,
            restartedEngine,
            &interruptedStats,
            InterruptedAfterSteps);
        const bool completionBlockedAtInterruption =
            !bayou::client::storyScriptProgressAllowsCompletion(
                *synthesis,
                static_cast<int>(InterruptedAfterSteps));
        const bool restartedReplay = replayScript(
            *synthesis, restartedEngine, &restartedStats);
        check(
            interruptedReplay && completionBlockedAtInterruption &&
                interruptedStats.executedSteps == InterruptedAfterSteps &&
                interruptedStats.playerInputs + interruptedStats.automaticSteps ==
                    interruptedStats.executedSteps,
            "BT17 interrupted auto-chain remains incomplete after only an authoritative prefix");
        check(
            restartedReplay &&
                restartedStats.executedSteps == synthesis->script.size() &&
                restartedStats.playerInputs == 26 &&
                restartedStats.automaticSteps == 48 &&
                pieceCount(restartedEngine, "Sapling") == 2 &&
                pieceCount(restartedEngine, "Blackthorn Lumberjack") == 3,
            "BT17 restart resets the interrupted chain and legally replays all 26 inputs plus 48 automatic timing steps");
    }

    const auto replayAndCheck = [](
        StoryCampaign campaign,
        std::string_view id,
        unsigned int seed,
        auto verify) {
        const StoryMission* mission = missionById(campaign, id);
        check(mission != nullptr, std::string(id) + ": advanced mission exists");
        if (mission == nullptr)
        {
            return;
        }
        GameEngine engine(seed, {});
        const bool replayed = replayScript(*mission, engine);
        check(replayed, std::string(id) + ": advanced outcome replay succeeds");
        if (replayed)
        {
            verify(engine);
        }
    };

    replayAndCheck(StoryCampaign::Blackthorn, "bt17_natural_order", 0x42543137u,
        [](const GameEngine& engine) {
            check(hasPieceAt(engine, "Victor Greyshard", 5, 5), "BT17 completes Victor's two Relentless attacks");
            const auto capturedGator = std::find_if(
                engine.boardPieces().begin(), engine.boardPieces().end(),
                [](const game_data::Piece& piece) {
                    return piece.name == "Bull Gator" &&
                        piece.row == 4 && piece.column == 2;
                });
            check(pieceCount(engine, "Blackthorn Debt Collector") == 1 &&
                    capturedGator != engine.boardPieces().end() &&
                    capturedGator->health == 2,
                "BT17 defeats both enemy Debt Collector targets, retains the roster Collector, and stages Grask before the surviving Capture target");
            check(
                pieceCount(engine, "Blackthorn Lumberjack") == 3 &&
                    hasPieceAt(engine, "Blackthorn Lumberjack", 6, 1),
                "BT17 adds the Commanded Lumberjack at B7 beside the two dependent-form profile witnesses");
            check(
                pieceCount(engine, "Sapling") == 2,
                "BT17 adds one Trail Sapling beside the dependent-form profile witness");
            const game_data::Piece* mog = pieceNamed(engine, "Mog");
            check(mog != nullptr && mog->health == 4, "BT17 Alchemist restores wounded Mog by three");
        });

    replayAndCheck(StoryCampaign::Mirewatch, "mw03_town_under_company", 0x4d573033u,
        [](const GameEngine& engine) {
            const auto woundedSpearman = std::find_if(
                engine.boardPieces().begin(), engine.boardPieces().end(),
                [](const game_data::Piece& piece) {
                    return piece.name == "Bog Spearman" && piece.row == 2 && piece.column == 2;
                });
            check(
                woundedSpearman != engine.boardPieces().end() && woundedSpearman->health == 2,
                "MW03 Joni aura visibly heals the wounded C3 Spearman to its printed maximum");
            check(
                hasPieceAt(engine, "Bog Spearman", 5, 2, 1) &&
                    !hasPieceAt(engine, "Blackthorn Lumberjack", 5, 2, 2),
                "MW03 Hooked Spear pulls the surviving guard to C6 before adjacent Spear Thrust defeats it and occupies C6");
        });

    replayAndCheck(StoryCampaign::Mirewatch, "mw11_no_plan_saves_all", 0x4d573131u,
        [](const GameEngine& engine) {
            check(
                pieceCount(engine, "Juniper Flash") == 0,
                "MW11 automatic Intercept applies the canonical lethal cost to one-Health Juniper");
            check(
                hasPieceAt(engine, "Mirewatch Informant", 4, 1, 1) &&
                    hasPieceAt(engine, "Blackthorn Debt Collector", 4, 2, 2),
                "MW11 Intercept swaps the protected runner to B5 before the attacking Debt Collector occupies C5");
            check(
                !hasPieceAt(engine, "Blackthorn Debt Collector", 2, 4, 2) &&
                    !hasPieceAt(engine, "Blackthorn Debt Collector", 6, 4, 2),
                "MW11 Birdie and Reed open both ordinary ranged escape lanes before Intercept");
        });

    replayAndCheck(StoryCampaign::Mirewatch, "mw13_making_credit", 0x4d573133u,
        [](const GameEngine& engine) {
            const game_data::Piece* tracker = pieceNamed(engine, "Swamp Tracker");
            const game_data::Piece* ambusher = pieceNamed(engine, "Goblin Ambusher");
            check(
                tracker != nullptr && tracker->row == 2 && tracker->column == 2 &&
                    tracker->owner == 1,
                "MW13 Tracker completes a real Frogback Leap to C3");
            check(
                ambusher != nullptr && ambusher->row == 2 && ambusher->column == 3 &&
                    ambusher->owner == 2 && !ambusher->hidden &&
                    ambusher->actionState == 0 && ambusher->health == ambusher->maxHealth &&
                    ambusher->disabledTurns == 0,
                "MW13 end-turn Reveal materializes the adjacent enemy without damage or Disable");
        });

    replayAndCheck(StoryCampaign::Blackthorn, "bt04_terms_conditions", 0x42543034u,
        [](const GameEngine& engine) {
            const game_data::Piece* erevan = pieceNamed(engine, "Erevan the Shadow");
            const auto collisionAmbusher = std::find_if(
                engine.boardPieces().begin(), engine.boardPieces().end(),
                [](const game_data::Piece& piece) {
                    return piece.name == "Goblin Ambusher" && piece.row == 5 &&
                        piece.column == 1;
                });
            check(
                erevan != nullptr && !erevan->hidden && erevan->disabledTurns == 0 &&
                    erevan->health == 1 && erevan->hasActed,
                "BT04 collision damages and reveals Erevan, whose next owner activation is then skipped");
            check(
                collisionAmbusher == engine.boardPieces().end() &&
                    hasPieceAt(engine, "Goblin Ambusher", 4, 0, 1) &&
                    pieceCount(engine, "Blackthorn Debt Collector") == 0,
                "BT04 materialized Ambusher later executes visible Stab and occupies defeated target A5");
        });

    replayAndCheck(StoryCampaign::Seelie, "se02_broken_bridge", 0x53453032u,
        [](const GameEngine& engine) {
            check(
                pieceCount(engine, "Bristlejack") == 0 &&
                    pieceCount(engine, "Widowroot") == 0,
                "SE02 clears both real attackers after observing damage timing");
            check(
                hasPieceAt(engine, "Crystal Unicorn", 3, 6, 1),
                "SE02 second Prismatic Charge occupies the defeated Widowroot square");
        });

    replayAndCheck(StoryCampaign::Seelie, "se03a_flight_workshop", 0x53453033u,
        [](const GameEngine& engine) {
            check(
                pieceCount(engine, "Nettle Starbright") == 0 &&
                    pieceCount(engine, "Nettle Starbright Unmounted") == 1 &&
                    hasPieceAt(engine, "Nettle Starbright Unmounted", 6, 3, 1),
                "SE03 follows Nettle's real Rebirth replacement through its Punch");
            check(
                hasPieceAt(engine, "Thorn Griffin", 5, 5, 1),
                "SE03 Griffin Flight stops before the surviving Widowroot");
            check(
                hasPieceAt(engine, "Prince Vesper", 2, 4, 1),
                "SE03 Vesper Spark attacks at range without moving the hero");
        });

    replayAndCheck(StoryCampaign::Seelie, "se04_reading_room", 0x53453034u,
        [](const GameEngine& engine) {
            check(
                hasPieceAt(engine, "Archivist Mosswake", 3, 3, 1),
                "SE04 deploys Mosswake, resolves Foresight and discard, then completes Rootwalk");
        });

    replayAndCheck(StoryCampaign::Seelie, "se07a_queens_price_scene", 0x53453741u,
        [](const GameEngine& engine) {
            const game_data::Piece* bristlejack = pieceNamed(engine, "Bristlejack");
            check(
                pieceCount(engine, "Warden") == 0 && bristlejack != nullptr &&
                    bristlejack->owner == 1 && bristlejack->row == 2 &&
                    bristlejack->column == 3 && bristlejack->health == 1 &&
                    bristlejack->maxHealth == 1 && !bristlejack->hasActed,
                "SE07A delayed Infest creates an acted Bristlejack that readies at its next owner turn");
            check(
                hasPieceAt(engine, "Queen Nyxara", 4, 3, 1) &&
                    hasPieceAt(engine, "Gloom Fairy", 2, 2, 1),
                "SE07A Nyxara completes Royal Advance after the Fairy resolves Infest");
        });

    replayAndCheck(StoryCampaign::Seelie, "se07_queens_price", 0x53453037u,
        [](const GameEngine& engine) {
            const game_data::Piece* bristlejack = pieceNamed(engine, "Bristlejack");
            const game_data::Piece* lark = pieceNamed(engine, "Quinberry Lark");
            check(
                bristlejack != nullptr && bristlejack->owner == 2 &&
                    bristlejack->health == bristlejack->maxHealth,
                "SE07 Pavo Controls without damage, then the target genuinely returns to its original side");
            check(
                lark != nullptr && lark->row == 5 && lark->column == 5 && lark->hidden,
                "SE07 hidden Quinberry crosses occupied squares and ends on empty F6");
        });

    replayAndCheck(StoryCampaign::Seelie, "se10_no_crown_holds_light", 0x53453130u,
        [](const GameEngine& engine) {
            const game_data::Piece* knight = pieceNamed(engine, "Starbloom Knight");
            check(
                knight != nullptr && knight->health == knight->maxHealth,
                "SE10 Caltheriel aura restores the wounded Knight to printed maximum Health");
            const auto charmed = std::find_if(
                engine.boardPieces().begin(), engine.boardPieces().end(),
                [](const game_data::Piece& piece) {
                    return piece.name == "Bristlejack" && piece.row == 1 && piece.column == 4;
                });
            check(
                charmed != engine.boardPieces().end() && charmed->owner == 1,
                "SE10 Caltheriel's zero-damage Charm applies real temporary Control");
        });

    replayAndCheck(StoryCampaign::Seelie, "se13_pearl_and_root", 0x53453133u,
        [](const GameEngine& engine) {
            const game_data::Piece* warden = pieceNamed(engine, "Warden");
            check(
                warden != nullptr && warden->health == 3 && warden->disabledTurns == 0,
                "SE13 Sovereign Thorns deals one damage and Disable expires after two owner turns");
    });
}

void validateWorldTreeTargetObjective()
{
    const StoryMission* mission = missionById(
        StoryCampaign::Mirewatch, "mw24_agent_not_heir");
    check(mission != nullptr, "World Tree target-objective mission exists");
    if (mission == nullptr)
    {
        return;
    }

    GameEngine engine(0x47524153u, {});
    check(
        replayScript(*mission, engine),
        "World Tree target-objective setup configures authoritatively");
    const game_data::Piece* reed = pieceNamed(engine, "Reed Baelstone");
    const game_data::Piece* grask = pieceNamed(engine, "Grask");
    const game_data::Piece* victor = pieceNamed(engine, "Victor Greyshard");
    check(
        reed && grask && victor,
        "World Tree target and protected-survivor identities are present");
    if (!(reed && grask && victor))
    {
        return;
    }
    const int reedId = reed->id;
    const int graskId = grask->id;
    const int victorId = victor->id;
    const int initialEnemies = static_cast<int>(std::count_if(
        engine.boardPieces().begin(), engine.boardPieces().end(),
        [](const game_data::Piece& piece) {
            return game_data::pieceOriginalOwner(piece) == 2;
        }));
    check(
        initialEnemies == 7 &&
            engine.scenarioObjectiveProgress().remaining == 1 &&
            engine.phase() == game_data::Phase::Playing,
        "World Tree tracks stable Grask identity rather than all seven enemies");

    bool legalApproach = true;
    for (int column = 1; column <= 3; ++column)
    {
        legalApproach = legalApproach &&
            engine.movePiece(1, reedId, 3, column) &&
            engine.endTurn(1) && engine.endTurn(2);
    }
    check(
        legalApproach,
        "Reed reaches Bow range using only his printed one-square Step");

    int legalShots = 0;
    for (int turnCycle = 0;
         turnCycle < 24 && engine.phase() == game_data::Phase::Playing;
         ++turnCycle)
    {
        if (engine.currentPlayer() == 1)
        {
            if (engine.attackPiece(1, reedId, 3, 6))
            {
                ++legalShots;
            }
            if (engine.phase() == game_data::Phase::Playing)
            {
                engine.endTurn(1);
            }
        }
        else
        {
            engine.endTurn(2);
        }
    }

    const int survivingEnemies = static_cast<int>(std::count_if(
        engine.boardPieces().begin(), engine.boardPieces().end(),
        [](const game_data::Piece& piece) {
            return game_data::pieceOriginalOwner(piece) == 2;
        }));
    check(legalShots == 5, "Reed lands Grask's five required Bow hits");
    check(
        std::none_of(
            engine.boardPieces().begin(), engine.boardPieces().end(),
            [&](const game_data::Piece& piece) { return piece.id == graskId; }),
        "the stable Grask identity leaves the authoritative board");
    check(
        survivingEnemies == 6,
        "six non-target World Tree enemies remain after Grask falls");
    check(
        std::any_of(
            engine.boardPieces().begin(), engine.boardPieces().end(),
            [&](const game_data::Piece& piece) { return piece.id == victorId; }),
        "Victor remains alive for the surrender and custody aftermath when Grask falls");
    check(
        engine.phase() == game_data::Phase::GameOver &&
            engine.winner() == 1 &&
            engine.scenarioObjectiveProgress().complete,
        "defeating stable Grask identity produces the authoritative target win");

    StoryMission victorFailureMission = *mission;
    const auto mogPlacement = std::find_if(
        victorFailureMission.pieces.begin(),
        victorFailureMission.pieces.end(),
        [](const StoryPiecePlacement& placement) {
            return placement.role == "mog";
        });
    check(
        mogPlacement != victorFailureMission.pieces.end(),
        "World Tree survivor-failure witness finds Mog's authored role");
    if (mogPlacement == victorFailureMission.pieces.end())
    {
        return;
    }
    mogPlacement->row = 1;
    mogPlacement->column = 7;

    GameEngine failureEngine(0x56494354u, {});
    check(
        replayScript(victorFailureMission, failureEngine),
        "World Tree survivor-failure witness configures authoritatively");
    const game_data::Piece* failureMog = pieceNamed(failureEngine, "Mog");
    const game_data::Piece* failureVictor =
        pieceNamed(failureEngine, "Victor Greyshard");
    check(
        failureMog && failureVictor,
        "World Tree survivor-failure witness places Mog beside Victor");
    if (!(failureMog && failureVictor))
    {
        return;
    }
    const int failureMogId = failureMog->id;
    const int failureVictorId = failureVictor->id;
    const int failureVictorRow = failureVictor->row;
    const int failureVictorColumn = failureVictor->column;
    const bool firstHit = failureEngine.attackPiece(
        1, failureMogId, failureVictorRow, failureVictorColumn);
    const bool turnCycle = firstHit && failureEngine.endTurn(1) &&
        failureEngine.endTurn(2);
    const bool secondHit = turnCycle && failureEngine.attackPiece(
        1, failureMogId, failureVictorRow, failureVictorColumn);
    const GameEngine::ScenarioObjectiveProgress failureProgress =
        failureEngine.scenarioObjectiveProgress();
    check(
        firstHit && turnCycle && secondHit,
        "Mog reaches Victor only through two real Axe Swing attacks and a real turn cycle");
    check(
        failureEngine.phase() == game_data::Phase::GameOver &&
            failureEngine.winner() == 2 && failureProgress.failed &&
            !failureProgress.complete &&
            failureProgress.failure ==
                GameEngine::ScenarioObjectiveFailure::RequiredPieceMissing &&
            failureProgress.failedPieceId == failureVictorId,
        "defeating protected Victor fails MW24 before the Grask objective can complete");
}

void validateRiverTeethLineClear()
{
    const auto reed = bayou::client::packagedStoryCard("Reed Baelstone");
    const auto telos = bayou::client::packagedStoryCard("Telos the Merchant");
    const auto gator = bayou::client::packagedStoryCard("Bull Gator");
    check(reed && telos && gator, "River Teeth line-clear cards have packaged definitions");
    if (!(reed && telos && gator))
    {
        return;
    }

    GameEngine engine(0x4c494e45u, {});
    engine.loadScenario(
        {{1, *reed, 5, 1, false},
         {1, *telos, 3, 1, false},
         {2, *gator, 2, 1, false, 1}},
        {}, {}, 0, 0, 1, "River Teeth line-clear regression", false);

    const game_data::Piece* placedReed = pieceNamed(engine, "Reed Baelstone");
    const game_data::Piece* placedTelos = pieceNamed(engine, "Telos the Merchant");
    const game_data::Piece* placedGator = pieceNamed(engine, "Bull Gator");
    check(placedReed && placedTelos && placedGator, "River Teeth line-clear setup is complete");
    if (!(placedReed && placedTelos && placedGator))
    {
        return;
    }

    const int reedId = placedReed->id;
    const int telosId = placedTelos->id;
    check(
        !engine.attackPiece(1, reedId, placedGator->row, placedGator->column),
        "Telos on B4 blocks Reed's B6-to-B3 Bow line");
    check(
        engine.movePiece(1, telosId, 4, 0),
        "Telos uses the ordinary Travel action from B4 to A5");
    check(engine.endTurn(1) && engine.endTurn(2), "turn cycle readies Reed after Telos Travels");
    check(
        engine.attackPiece(1, reedId, 2, 1),
        "Reed's ordinary Bow becomes legal after Telos clears column B");
    check(pieceCount(engine, "Bull Gator") == 0, "the Bow, not cargo, defeats the final gator");
}

void validateDependentCards()
{
    const std::span<const std::string_view> packagedTitles =
        bayou::client::packagedStoryCardTitles();
    const std::set<std::string_view> uniquePackagedTitles(
        packagedTitles.begin(), packagedTitles.end());
    check(
        packagedTitles.size() == 60 &&
            uniquePackagedTitles.size() == packagedTitles.size(),
        "the packaged Story fixture registry exposes 60 unique titles");
    for (std::string_view title : packagedTitles)
    {
        check(
            bayou::client::packagedStoryCard(title).has_value(),
            std::string("packaged Story fixture registry resolves: ") +
                std::string(title));
    }
    check(
        !bayou::client::packagedStoryCard("Unregistered Story Fixture"),
        "packaged Story fixture lookup fails closed for an unregistered title");

    const auto foreman = bayou::client::packagedStoryCard("Blackthorn Foreman");
    const auto lumberjack = bayou::client::packagedStoryCard("Blackthorn Lumberjack");
    const auto sister = bayou::client::packagedStoryCard("Grove Sister");
    const auto sapling = bayou::client::packagedStoryCard("Sapling");
    const auto tracker = bayou::client::packagedStoryCard("Swamp Tracker");
    const auto unmounted = bayou::client::packagedStoryCard("Swamp Tracker Unmounted");
    const auto gator = bayou::client::packagedStoryCard("Bull Gator");
    check(
        foreman && lumberjack && sister && sapling && tracker && unmounted && gator,
        "dependent Story Mode cards have reviewed definitions");
    if (!(foreman && lumberjack && sister && sapling && tracker && unmounted && gator))
    {
        return;
    }
    const auto leap = std::find_if(
        tracker->actions.begin(), tracker->actions.end(),
        [](const game_data::ActionProfile& action) {
            return action.name == "Frogback Leap";
        });
    check(
        std::find(tracker->keywords.begin(), tracker->keywords.end(), "Reveal") !=
                tracker->keywords.end() &&
            std::find(unmounted->keywords.begin(), unmounted->keywords.end(), "Reveal") !=
                unmounted->keywords.end() &&
            leap != tracker->actions.end() && leap->canMove && leap->canAttack &&
            leap->minRange == 2 && leap->maxRange == 2 && leap->damage == 1 &&
            leap->passThrough,
        "Swamp Tracker fixtures keep the exact live Reveal keyword and Frogback Leap profile");

    {
        GameEngine engine(0x53554d4du, {});
        engine.registerScenarioCard(*lumberjack);
        engine.loadScenario(
            {{1, *foreman, 3, 1, false}, {2, *gator, 7, 7, false}},
            {}, {}, 0, 0, 1, "Summon regression", false);
        const int foremanId = engine.boardPieces().front().id;
        check(engine.useAbility(1, foremanId), "offline Foreman can summon its dependent card");
        check(
            std::any_of(
                engine.boardPieces().begin(), engine.boardPieces().end(),
                [](const game_data::Piece& value) {
                    return value.name == "Blackthorn Lumberjack" &&
                        value.row == 3 && value.column == 2;
                }),
            "summoned Lumberjack appears in front of the Foreman");
    }

    {
        GameEngine engine(0x54524149u, {});
        engine.registerScenarioCard(*sapling);
        engine.loadScenario(
            {{1, *sister, 3, 1, false}, {2, *gator, 7, 7, false}},
            {}, {}, 0, 0, 1, "Trail regression", false);
        const int sisterId = engine.boardPieces().front().id;
        check(engine.movePiece(1, sisterId, 2, 2), "offline Grove Sister can take a Trail move");
        check(
            std::any_of(
                engine.boardPieces().begin(), engine.boardPieces().end(),
                [](const game_data::Piece& value) {
                    return value.name == "Sapling" && value.row == 3 && value.column == 1;
                }),
            "Trail leaves its dependent Sapling behind");
    }

    {
        GameEngine engine(0x52454252u, {});
        engine.registerScenarioCard(*unmounted);
        engine.loadScenario(
            {{1, *tracker, 3, 1, false, 2}, {2, *gator, 3, 2, false}},
            {}, {}, 0, 0, 2, "Rebirth regression", false);
        const int trackerId = engine.boardPieces().front().id;
        const int gatorId = engine.boardPieces().back().id;
        check(
            engine.attackPiece(2, gatorId, 3, 1),
            "ordinary Bull Gator Bite can defeat a wounded mounted Tracker");
        check(
            std::any_of(
                engine.boardPieces().begin(), engine.boardPieces().end(),
                [](const game_data::Piece& value) {
                    return value.name == "Swamp Tracker Unmounted" &&
                        value.row == 3 && value.column == 1;
                }),
            "Rebirth replaces the Tracker with its offline dependent definition");
        check(
            engine.phase() == game_data::Phase::Playing,
            "Story Mode does not end merely because a hero-free side loses a piece");
    }
}

} // namespace

int main()
{
    const bayou::storytest::ClaimCatalogResult claimCatalog =
        bayou::storytest::validateClaimCatalog();
    check(
        claimCatalog.ok,
        claimCatalog.ok
            ? "shared Story mechanic claim catalog is internally consistent"
            : claimCatalog.error);
    fmt::println(
        "Story mechanic claim catalog: {} before/after checks cover {} authored mastery claims.",
        claimCatalog.outcomeChecks,
        claimCatalog.masteryClaims);

    const std::vector<std::string_view> mirewatchOrder = {
        "mw01_river_teeth",
        "mw03_town_under_company",
        "mw04_watcher_protects",
        "s01_hospitality",
        "mw05_watched_office",
        "mw06_cost_seen",
        "mw08_lesson_night",
        "mw10_beautiful_plan",
        "mw11_no_plan_saves_all",
        "s03_published_mystery",
        "s04_wounds_that_vote",
        "mw14_public_lie",
        "mw15_title_follows_burden",
        "mw18_allies_dishonestly",
                "mw20_monster_rules",
        "mw24_agent_not_heir",
        "mw21_four_losses",
        "mw22_clean_shot",
        "mw23_choice_not_cure",
        "mw24a_victor_in_custody",
        "mw25_deed_own_hand",
        "s06_town_owns_itself",
        "s02_no_one_alone",
        "mw06a_card_workshop",
        "mw13_making_credit",
        "mw15a_team_workshop",
    };
    const std::vector<std::string_view> blackthornOrder = {
        "bt01_harness_hunger",
        "bt02_customs_bell",
        "bt03_sanctuary_debt",
        "bt04_terms_conditions",
        "s01_hospitality",
        "bt05_freight_office",
        "bt06_receipt_book",
        "bt08_north_lock",
        "bt07_debtor_prison",
        "s03_published_mystery",
        "bt10_stolen_road",
        "bt12_break_charter",
        "bt13a_poisoned_root_road",
                "bt15_monster_rules",
        "bt16_clean_shot",
        "bt17c_victor_reckoning",
        "bt18_deed_own_hand",
        "s06_town_owns_itself",
        "bt17_natural_order",
        "bt17a_field_judgment",
        "bt17b_open_mastery",
    };
    const std::vector<std::string_view> seelieOrder = {
        "se00_what_mirror_remembers",
        "se01_cathedral_last_lights",
        "se02_broken_bridge",
        "se03_road_small_lights",
        "se04a_lash_reads_the_road",
        "se05_court_behind_curtain",
        "se06_honey_without_hunger",
        "se08_nine_lives_one_choice",
        "se09_dusk_crown",
        "se19_crypt_under_order",
        "se10a_caltheriel_walks_out",
        "se12_first_boats_burn",
        "se14_rules_under_pressure",
        "se17_warm_channel",
        "se18a_complete_is_a_label",
        "se15_bell_and_order",
        "se20_harness_anyone_can_leave",
        "se23_controlled_fall",
        "se27_emperor_at_tree",
        "se28a_two_men_one_vacancy",
        "se29a_baalzapub_ascendant",
        "se29b_fizzlewick_says_no",
        "se29c_all_authority_returns",
        "se29e_hold_the_witness_rail",
        "se29d_reed_ends_the_house",
        "se29_no_complete_bearer",
        "se30_names_we_keep",
        "se03a_flight_workshop",
        "se04_reading_room",
        "se07a_queens_price_scene",
        "se07_queens_price",
        "se10_no_crown_holds_light",
        "se13_pearl_and_root",
    };

    validateAuthoritativeCardBoundary();
    validateGuidedActionProfileSelection();
    validateStoryAssets();
    validateDefeatAllEnemyIdentity();
    validateSeelieDifficultyCurve();
    validateExpandedMirewatchBattles();
    validateBlackthornTrainingContract();
    validateCampaign(StoryCampaign::Mirewatch, 26, 14, mirewatchOrder);
    validateCampaign(StoryCampaign::Blackthorn, 21, 10, blackthornOrder);
    validateCampaign(StoryCampaign::Seelie, 33, 16, seelieOrder);
    validateCumulativeMasteryEvidence();
    {
        const auto juniper = bayou::client::packagedStoryCard("Juniper Flash");
        const auto sprint = juniper
            ? std::find_if(
                  juniper->actions.begin(), juniper->actions.end(),
                  [](const auto& action) { return action.name == "Sprint"; })
            : decltype(juniper->actions.begin()){};
        check(
            juniper && sprint != juniper->actions.end() && sprint->canMove &&
                !sprint->canAttack && sprint->minRange == 1 && sprint->maxRange == 2 &&
                std::find(juniper->keywords.begin(), juniper->keywords.end(), "intercept") !=
                    juniper->keywords.end(),
            "Juniper fixture matches live Sprint and Intercept instead of omitting her protection rule");
    }
    {
        const auto spearman = bayou::client::packagedStoryCard("Bog Spearman");
        const auto hook = spearman
            ? std::find_if(
                  spearman->actions.begin(), spearman->actions.end(),
                  [](const auto& action) { return action.name == "Hooked Spear"; })
            : decltype(spearman->actions.begin()){};
        const auto thrust = spearman
            ? std::find_if(
                  spearman->actions.begin(), spearman->actions.end(),
                  [](const auto& action) { return action.name == "Spear Thrust"; })
            : decltype(spearman->actions.begin()){};
        check(
            spearman && hook != spearman->actions.end() && !hook->canMove && hook->canAttack &&
                hook->minRange == 2 && hook->maxRange == 3 && hook->damage == 1 && hook->pull &&
                thrust != spearman->actions.end() && thrust->canMove && thrust->canAttack &&
                thrust->minRange == 1 && thrust->maxRange == 1 && thrust->damage == 2 && !thrust->pull,
            "Bog Spearman fixture distinguishes ranged Hooked Spear Pull from adjacent two-damage Spear Thrust");
    }
    // The adapted route has fewer story scenes, chronological rescues and
    // optional training after the ending. Stable IDs define these boundaries.
    const auto routeIndex = [](StoryCampaign campaign, std::string_view id) {
        const auto missions = storyMissions(campaign);
        const auto found = std::find_if(missions.begin(), missions.end(),
            [&](const StoryMission& mission) { return mission.id == id; });
        return found == missions.end() ? -1 : static_cast<int>(found - missions.begin());
    };
    check(bayou::client::storyActName(StoryCampaign::Mirewatch,
              routeIndex(StoryCampaign::Mirewatch, "mw01_river_teeth")) == "ACT I - TEETH IN THE WATER" &&
          bayou::client::storyActShortGoal(StoryCampaign::Mirewatch,
              routeIndex(StoryCampaign::Mirewatch, "mw01_river_teeth")) == "Protect the people on the river.",
        "mw01_river_teeth: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Mirewatch,
              routeIndex(StoryCampaign::Mirewatch, "s01_hospitality")) == "ACT II - FRIENDS UNDER FIRE" &&
          bayou::client::storyActShortGoal(StoryCampaign::Mirewatch,
              routeIndex(StoryCampaign::Mirewatch, "s01_hospitality")) == "Get the families to shelter.",
        "s01_hospitality: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Mirewatch,
              routeIndex(StoryCampaign::Mirewatch, "mw08_lesson_night")) == "ACT III - THE TRAP" &&
          bayou::client::storyActShortGoal(StoryCampaign::Mirewatch,
              routeIndex(StoryCampaign::Mirewatch, "mw08_lesson_night")) == "Get the prisoners out.",
        "mw08_lesson_night: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Mirewatch,
              routeIndex(StoryCampaign::Mirewatch, "s04_wounds_that_vote")) == "ACT IV - THE TOWN FIGHTS BACK" &&
          bayou::client::storyActShortGoal(StoryCampaign::Mirewatch,
              routeIndex(StoryCampaign::Mirewatch, "s04_wounds_that_vote")) == "Help the town fight back.",
        "s04_wounds_that_vote: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Mirewatch,
              routeIndex(StoryCampaign::Mirewatch, "mw18_allies_dishonestly")) == "ACT V - THE ROAD TO THE TREE" &&
          bayou::client::storyActShortGoal(StoryCampaign::Mirewatch,
              routeIndex(StoryCampaign::Mirewatch, "mw18_allies_dishonestly")) == "Clear a road for the families.",
        "mw18_allies_dishonestly: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Mirewatch,
              routeIndex(StoryCampaign::Mirewatch, "mw20_monster_rules")) == "ACT VI - FREE THE TREE" &&
          bayou::client::storyActShortGoal(StoryCampaign::Mirewatch,
              routeIndex(StoryCampaign::Mirewatch, "mw20_monster_rules")) == "Free the captives. Save the tree.",
        "mw20_monster_rules: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Mirewatch,
              routeIndex(StoryCampaign::Mirewatch, "mw25_deed_own_hand")) == "ACT VII - A NEW START" &&
          bayou::client::storyActShortGoal(StoryCampaign::Mirewatch,
              routeIndex(StoryCampaign::Mirewatch, "mw25_deed_own_hand")) == "Help the town rebuild.",
        "mw25_deed_own_hand: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Blackthorn,
              routeIndex(StoryCampaign::Blackthorn, "bt01_harness_hunger")) == "ACT I - ORDERS AND DOUBTS" &&
          bayou::client::storyActShortGoal(StoryCampaign::Blackthorn,
              routeIndex(StoryCampaign::Blackthorn, "bt01_harness_hunger")) == "Mog must choose who to protect.",
        "bt01_harness_hunger: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Blackthorn,
              routeIndex(StoryCampaign::Blackthorn, "s01_hospitality")) == "ACT II - FRIENDS UNDER FIRE" &&
          bayou::client::storyActShortGoal(StoryCampaign::Blackthorn,
              routeIndex(StoryCampaign::Blackthorn, "s01_hospitality")) == "Help the hunted families.",
        "s01_hospitality: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Blackthorn,
              routeIndex(StoryCampaign::Blackthorn, "bt08_north_lock")) == "ACT III - THE TRAP" &&
          bayou::client::storyActShortGoal(StoryCampaign::Blackthorn,
              routeIndex(StoryCampaign::Blackthorn, "bt08_north_lock")) == "Get the prisoners out.",
        "bt08_north_lock: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Blackthorn,
              routeIndex(StoryCampaign::Blackthorn, "bt10_stolen_road")) == "ACT IV - THE TOWN FIGHTS BACK" &&
          bayou::client::storyActShortGoal(StoryCampaign::Blackthorn,
              routeIndex(StoryCampaign::Blackthorn, "bt10_stolen_road")) == "Stand with the town.",
        "bt10_stolen_road: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Blackthorn,
              routeIndex(StoryCampaign::Blackthorn, "bt13a_poisoned_root_road")) == "ACT V - THE ROAD TO THE TREE" &&
          bayou::client::storyActShortGoal(StoryCampaign::Blackthorn,
              routeIndex(StoryCampaign::Blackthorn, "bt13a_poisoned_root_road")) == "Clear a road for the families.",
        "bt13a_poisoned_root_road: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Blackthorn,
              routeIndex(StoryCampaign::Blackthorn, "bt15_monster_rules")) == "ACT VI - FREE THE TREE" &&
          bayou::client::storyActShortGoal(StoryCampaign::Blackthorn,
              routeIndex(StoryCampaign::Blackthorn, "bt15_monster_rules")) == "Free the captives. Save the tree.",
        "bt15_monster_rules: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Blackthorn,
              routeIndex(StoryCampaign::Blackthorn, "bt18_deed_own_hand")) == "ACT VII - A NEW START" &&
          bayou::client::storyActShortGoal(StoryCampaign::Blackthorn,
              routeIndex(StoryCampaign::Blackthorn, "bt18_deed_own_hand")) == "Help the town rebuild.",
        "bt18_deed_own_hand: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Seelie,
              routeIndex(StoryCampaign::Seelie, "se00_what_mirror_remembers")) == "ACT I - FREE CALTHERIEL" &&
          bayou::client::storyActShortGoal(StoryCampaign::Seelie,
              routeIndex(StoryCampaign::Seelie, "se00_what_mirror_remembers")) == "Help Caltheriel leave the cage.",
        "se00_what_mirror_remembers: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Seelie,
              routeIndex(StoryCampaign::Seelie, "se12_first_boats_burn")) == "ACT II - SAVE THE HARBOR" &&
          bayou::client::storyActShortGoal(StoryCampaign::Seelie,
              routeIndex(StoryCampaign::Seelie, "se12_first_boats_burn")) == "Get the families ashore.",
        "se12_first_boats_burn: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Seelie,
              routeIndex(StoryCampaign::Seelie, "se18a_complete_is_a_label")) == "ACT III - FREE THE WORKERS" &&
          bayou::client::storyActShortGoal(StoryCampaign::Seelie,
              routeIndex(StoryCampaign::Seelie, "se18a_complete_is_a_label")) == "Bring the trapped workers up.",
        "se18a_complete_is_a_label: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Seelie,
              routeIndex(StoryCampaign::Seelie, "se27_emperor_at_tree")) == "ACT IV - KEEP THE EXIT OPEN" &&
          bayou::client::storyActShortGoal(StoryCampaign::Seelie,
              routeIndex(StoryCampaign::Seelie, "se27_emperor_at_tree")) == "Get everyone out of the theater.",
        "se27_emperor_at_tree: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Seelie,
              routeIndex(StoryCampaign::Seelie, "se29d_reed_ends_the_house")) == "ACT V - STOP THE ENGINE" &&
          bayou::client::storyActShortGoal(StoryCampaign::Seelie,
              routeIndex(StoryCampaign::Seelie, "se29d_reed_ends_the_house")) == "Stop the engine.",
        "se29d_reed_ends_the_house: the revised act starts with its immediate player goal");
    check(bayou::client::storyActName(StoryCampaign::Seelie,
              routeIndex(StoryCampaign::Seelie, "se30_names_we_keep")) == "ACT VI - HOME AGAIN" &&
          bayou::client::storyActShortGoal(StoryCampaign::Seelie,
              routeIndex(StoryCampaign::Seelie, "se30_names_we_keep")) == "Bring your friends home.",
        "se30_names_we_keep: the revised act starts with its immediate player goal");
    for (const auto [campaign, ending] : {
             std::pair{StoryCampaign::Mirewatch, "s06_town_owns_itself"},
             std::pair{StoryCampaign::Blackthorn, "s06_town_owns_itself"},
             std::pair{StoryCampaign::Seelie, "se30_names_we_keep"}})
    {
        const auto missions = storyMissions(campaign);
        const int storyEnd = bayou::client::storyNarrativeEntryCount(campaign);
        check(storyEnd == routeIndex(campaign, ending) + 1 && storyEnd > 0,
            "story completion follows its ending, independently of extra practice");
        for (std::size_t index = static_cast<std::size_t>(storyEnd); index < missions.size(); ++index)
        {
            check(missions[index].optionalRehearsal && !missions[index].catchUpForSkippedRehearsals &&
                    bayou::client::storyActName(campaign, static_cast<int>(index)) == "EXTRA PRACTICE",
                "every post-story lesson is optional and labeled outside the narrative acts");
        }
    }
    check(routeIndex(StoryCampaign::Mirewatch, "mw24_agent_not_heir") <
              routeIndex(StoryCampaign::Mirewatch, "mw22_clean_shot") &&
          routeIndex(StoryCampaign::Mirewatch, "mw22_clean_shot") <
              routeIndex(StoryCampaign::Mirewatch, "mw23_choice_not_cure") &&
          routeIndex(StoryCampaign::Mirewatch, "mw23_choice_not_cure") <
              routeIndex(StoryCampaign::Mirewatch, "mw24a_victor_in_custody"),
        "the playable escape precedes Birdie's shot, freeing Sylvara and Victor's capture without a rewind");
    check(routeIndex(StoryCampaign::Seelie, "se19_crypt_under_order") >
              routeIndex(StoryCampaign::Seelie, "se09_dusk_crown") &&
          routeIndex(StoryCampaign::Seelie, "se19_crypt_under_order") <
              routeIndex(StoryCampaign::Seelie, "se10a_caltheriel_walks_out"),
        "the cage escape battle happens while Caltheriel still needs the exit");
    for (const StoryCampaign campaign :
         {StoryCampaign::Mirewatch, StoryCampaign::Blackthorn, StoryCampaign::Seelie})
    {
        for (std::size_t index = 1; index < storyMissions(campaign).size(); ++index)
        {
            const bool newAct = bayou::client::storyActName(campaign, index) !=
                bayou::client::storyActName(campaign, index - 1);
            check(
                !newAct || ((bayou::client::storyActGoal(campaign, index) !=
                    bayou::client::storyActGoal(campaign, index - 1)) &&
                (bayou::client::storyActShortGoal(campaign, index) !=
                    bayou::client::storyActShortGoal(campaign, index - 1))),
                "each new act updates both goal labels; goals may also advance within an act");
        }
    }
    check(
        !storyMissions(StoryCampaign::Mirewatch).empty() &&
            storyMissions(StoryCampaign::Mirewatch).front().id == "mw01_river_teeth",
        "Mirewatch begins immediately with the River Teeth gator attack");
    check(
        missionById(StoryCampaign::Mirewatch, "s00_wrong_hand") == nullptr &&
            missionById(StoryCampaign::Blackthorn, "s00_wrong_hand") == nullptr &&
            missionById(StoryCampaign::Seelie, "s00_wrong_hand") == nullptr,
        "the theatre prologue is absent from every playable campaign path");
    check(
        !storyMissions(StoryCampaign::Seelie).empty() &&
            storyMissions(StoryCampaign::Seelie).front().id ==
                "se00_what_mirror_remembers" &&
            storyMissions(StoryCampaign::Seelie)[1].id ==
                "se01_cathedral_last_lights",
        "Seelie begins with the Chapter 16 Cathedral-road decision, then enters the Chapter 17 attack without restoring the theatre prologue");
    check(
        bayou::client::storyAiSearchDepth(
            StoryCampaign::Seelie, "se01_cathedral_last_lights") == 1 &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Seelie, "se17_warm_channel") == 2 &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Seelie, "se19_crypt_under_order") == 3 &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Seelie, "se23_controlled_fall") == 3 &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Seelie, "se27_emperor_at_tree") == 4 &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Seelie, "se28a_two_men_one_vacancy") == 4 &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Seelie, "se29e_hold_the_witness_rail") == 4 &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Mirewatch, "mw18_allies_dishonestly") == 2 &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Mirewatch, "mw24_agent_not_heir") == 3 &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Blackthorn, "bt17a_field_judgment") == 2 &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Blackthorn, "bt17b_open_mastery") == 3 &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Mirewatch, "mw25_deed_own_hand") == 1,
        "late Seelie, Mirewatch, and Blackthorn open missions increase AI search depth along their difficulty curves");
    check(
        missionById(StoryCampaign::Mirewatch, "mw12_road_reaches") == nullptr,
        "the superseded standalone road chapter is absent from Mirewatch");
    check(
        missionById(StoryCampaign::Mirewatch, "mw25_deed_own_hand") != nullptr &&
            missionById(StoryCampaign::Blackthorn, "bt18_deed_own_hand") != nullptr,
        "both routes include the revised Chapter 29 public-offer coda");

    validateRoster(
        StoryCampaign::Mirewatch,
        {
            "Joni Pumpernickel", "Vanya Bluewater", "Birdie the Wise",
            "Donella of the Marsh", "Juniper Flash", "Scooter",
            "Erevan the Shadow", "Reed Baelstone", "Bog Spearman",
            "Marshland Veteran", "Resistance Smuggler", "Mirewatch Informant",
            "Swamp Tracker"
        });
    validateRoster(
        StoryCampaign::Blackthorn,
        {
            "Thaeron Baelstone", "Ashenfang", "Blackthorn Debt Collector",
            "Blackthorn Alchemist", "Blackthorn Foreman", "Blackthorn Lumberjack",
            "Grove Sister", "Mog", "Grask", "Goblin Ambusher",
            "Braun Stonefist", "Goblin Sharpshooter"
        });
    validateRoster(
        StoryCampaign::Seelie,
        {
            "Archivist Mosswake", "Caltheriel", "Crystal Unicorn",
            "Duchess Dewbell", "Fey Messenger", "Heartwood Sister",
            "Nettle Starbright", "Pavo Quickstep", "Prince Vesper",
            "Quinberry Lark", "Starbloom Knight", "Sylvara", "Thorn Griffin"
        });
    validateNoFabricatedGameplay();
    validateTeachingCopy();
    validateNarrativeCoherence();
    validateSeelieCardTruth();

    for (StoryCampaign campaign : {
             StoryCampaign::Mirewatch, StoryCampaign::Blackthorn, StoryCampaign::Seelie})
    {
        for (const StoryMission& mission : bayou::client::storyMissions(campaign))
        {
            if (mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly)
            {
                continue;
            }
            if (mission.standardMatch)
            {
                check(
                    validateStandardMatchPath(mission),
                    std::string("ordinary standard-match path is legal: ") +
                        std::string(mission.id));
                continue;
            }
            GameEngine engine(
                0x53544f52u + static_cast<unsigned int>(mission.id.size()), {});
            const bool loadedAndReplayed = replayScript(mission, engine);
            check(
                loadedAndReplayed,
                std::string(mission.script.empty()
                        ? "open tactical setup is legal: "
                        : "golden replay is legal: ") +
                    std::string(mission.id));
            if (loadedAndReplayed && mission.script.empty())
            {
                const int playerPieces = static_cast<int>(std::count_if(
                    engine.boardPieces().begin(), engine.boardPieces().end(),
                    [](const game_data::Piece& piece) { return piece.owner == 1; }));
                const int enemyPieces = static_cast<int>(std::count_if(
                    engine.boardPieces().begin(), engine.boardPieces().end(),
                    [](const game_data::Piece& piece) { return piece.owner == 2; }));
                check(
                    engine.phase() == game_data::Phase::Playing &&
                        playerPieces > 0 && enemyPieces > 0,
                    std::string(mission.id) +
                        ": open objective starts as a live two-sided position");
                if (mission.objectiveSpec.kind == StoryObjectiveKind::ReachSquare)
                {
                    const auto placement = std::find_if(
                        mission.pieces.begin(), mission.pieces.end(),
                        [&](const StoryPiecePlacement& value) {
                            return value.role == mission.objectiveSpec.targetRole;
                        });
                    check(
                        placement != mission.pieces.end() &&
                            (placement->row != mission.objectiveSpec.targetRow ||
                             placement->column != mission.objectiveSpec.targetColumn),
                        std::string(mission.id) +
                            ": reach-square objective is not complete at setup");
                }
                if (mission.objectiveSpec.kind == StoryObjectiveKind::ControlSquares)
                {
                    check(
                        engine.controlledSquares(1) < mission.objectiveSpec.amount,
                        std::string(mission.id) +
                            ": control-squares objective is not complete at setup");
                }
            }
        }
    }

    const StoryMission* mirewatch =
        missionById(StoryCampaign::Mirewatch, "mw01_river_teeth");
    const StoryMission* blackthorn =
        missionById(StoryCampaign::Blackthorn, "bt01_harness_hunger");
    check(mirewatch != nullptr, "River Teeth exists");
    check(blackthorn != nullptr, "Harness the Hunger exists");
    if (blackthorn)
    {
        check(
            bayou::client::storyRequiredSurvivorNames(*blackthorn) ==
                "Blackthorn Alchemist and both Blackthorn Lumberjack pieces",
            "duplicate critical survivors are grouped into a natural player-facing label");
    }
    if (const StoryMission* town =
            missionById(StoryCampaign::Mirewatch, "mw03_town_under_company"))
    {
        check(
            bayou::client::storyRequiredSurvivorNames(*town) ==
                "Joni Pumpernickel and both Bog Spearman pieces",
            "a second duplicate-survivor formation is grouped without repeated card names");
    }

    if (mirewatch)
    {
        GameEngine engine(0x4d573031u, {});
        check(replayScript(*mirewatch, engine), "River Teeth script is legal in the real engine");
        const int enemyCount = static_cast<int>(std::count_if(
            engine.boardPieces().begin(), engine.boardPieces().end(),
            [](const game_data::Piece& value) { return value.owner == 2; }));
        check(enemyCount == 0, "River Teeth defeats exactly all three gators");
        const auto reed = std::find_if(
            engine.boardPieces().begin(), engine.boardPieces().end(),
            [](const game_data::Piece& value) { return value.name == "Reed Baelstone"; });
        check(
            reed != engine.boardPieces().end() && reed->row == 5 && reed->column == 1,
            "Reed finishes on B6 after exactly two one-square moves");
        const auto reedStart = std::find_if(
            mirewatch->pieces.begin(), mirewatch->pieces.end(),
            [](const auto& value) { return value.role == "reed"; });
        const auto gatorStart = std::find_if(
            mirewatch->pieces.begin(), mirewatch->pieces.end(),
            [](const auto& value) { return value.role == "center_gator"; });
        const int reedMoveCount = static_cast<int>(std::count_if(
            mirewatch->script.begin(), mirewatch->script.end(), [](const auto& step) {
                return step.kind == StoryActionKind::Move && step.actorRole == "reed";
            }));
        check(reedMoveCount == 2, "River Teeth gives Reed exactly two movement actions");
        if (reed != engine.boardPieces().end() && reedStart != mirewatch->pieces.end() &&
            gatorStart != mirewatch->pieces.end())
        {
            const int before = std::abs(reedStart->row - gatorStart->row) +
                std::abs(reedStart->column - gatorStart->column);
            const int after = std::abs(reed->row - gatorStart->row) +
                std::abs(reed->column - gatorStart->column);
            check(after > before, "Reed's two-square route increases distance from the lunging gator");
        }
    }
    if (blackthorn)
    {
        GameEngine engine(0x42543031u, {});
        check(replayScript(*blackthorn, engine), "Harness the Hunger script is legal");
        const int gatorCount = static_cast<int>(std::count_if(
            engine.boardPieces().begin(), engine.boardPieces().end(),
            [](const game_data::Piece& value) {
                return value.owner == 2 && value.name == "Bull Gator";
            }));
        check(gatorCount == 3, "Blackthorn opening preserves all three gators");
    }

    validateAdvancedOutcomes();
    validateStoryKeyboardNavigation();
    validateReorderedStoryProgress();
    validateWorldTreeTargetObjective();
    validateRiverTeethLineClear();
    validateDependentCards();

    fmt::println("{} Story Mode test failure(s).", failures);
    return failures == 0 ? 0 : 1;
}
