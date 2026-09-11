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
                  return step.heading == "CHOOSE AMBUSH BEFORE THE HIDDEN COLLISION";
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
                usesRiverTeethSequence || !mission.scenarioArtPath.empty(),
                std::string(mission.id) +
                    ": every mission after MW01 retains its commissioned scenario art");
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
                    pathEarnedMastery == expectedBlackthornStarterRoster &&
                    !pathEarnedMastery.contains("Victor Greyshard"),
                "skip-all Blackthorn path earns exactly the ordinary twelve-card starter roster and excludes non-starter Victor");
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
                        step.instruction.find("Click ") == std::string_view::npos &&
                            step.instruction.find("Drag ") == std::string_view::npos,
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

const StoryScriptAction* actionByHeading(
    const StoryMission& mission,
    std::string_view heading)
{
    const auto found = std::find_if(
        mission.script.begin(), mission.script.end(), [&](const StoryScriptAction& action) {
            return action.heading == heading;
        });
    return found == mission.script.end() ? nullptr : &*found;
}

void validateTeachingCopy()
{
    const StoryMission* river =
        missionById(StoryCampaign::Mirewatch, "mw01_river_teeth");
    const StoryMission* scent =
        missionById(StoryCampaign::Blackthorn, "bt01_harness_hunger");
    const StoryMission* auction =
        missionById(StoryCampaign::Mirewatch, "mw10_beautiful_plan");
    const StoryMission* terms =
        missionById(StoryCampaign::Blackthorn, "bt04_terms_conditions");
    const StoryMission* capstone =
        missionById(StoryCampaign::Blackthorn, "bt17_natural_order");
    const StoryMission* cathedral =
        missionById(StoryCampaign::Seelie, "se01_cathedral_last_lights");
    const StoryMission* vault =
        missionById(StoryCampaign::Mirewatch, "mw11_no_plan_saves_all");
    const StoryMission* readingRoom =
        missionById(StoryCampaign::Seelie, "se04_reading_room");
    check(
        river && scent && auction && terms && capstone && cathedral && vault &&
            readingRoom,
        "reviewed teaching-copy missions exist");
    if (!river || !scent || !auction || !terms || !capstone || !cathedral ||
        !vault || !readingRoom)
    {
        return;
    }

    const bool briefingDefinesProgress = std::any_of(
        river->briefing.begin(), river->briefing.end(), [](const StoryPanel& panel) {
            return panel.text.find("Guided Actions count completed game actions") != std::string_view::npos &&
                panel.text.find("not mouse motions") != std::string_view::npos &&
                panel.text.find("accepts only the glowing scripted action") != std::string_view::npos &&
                panel.text.find("one ready piece takes one normal action") != std::string_view::npos;
        });
    check(
        briefingDefinesProgress,
        "opening briefing distinguishes guided acceptance from the real one-piece action economy");
    const bool briefingDefinesInput = std::any_of(
        river->briefing.begin(), river->briefing.end(), [](const StoryPanel& panel) {
            return panel.text.find("hold the left mouse button") != std::string_view::npos &&
                panel.text.find("drag to the target") != std::string_view::npos &&
                panel.text.find("keyboard focus + Enter") != std::string_view::npos &&
                panel.text.find("focused target confirms") != std::string_view::npos;
        });
    check(
        briefingDefinesInput,
        "opening briefing states exact mouse drag-release and keyboard focus-plus-Enter semantics");
    const bool briefingDefinesHealth = std::any_of(
        river->briefing.begin(), river->briefing.end(), [](const StoryPanel& panel) {
            return panel.text.find("story context, not a status effect") != std::string_view::npos &&
                panel.text.find("Number badges show current Health") != std::string_view::npos &&
                panel.text.find("blue for your pieces, red for enemies") != std::string_view::npos &&
                panel.text.find("begin wounded at 1") != std::string_view::npos &&
                panel.text.find("removes a unit at 0") != std::string_view::npos;
        });
    check(
        briefingDefinesHealth,
        "opening briefing distinguishes story injury from Health and destruction rules");

    const StoryScriptAction* reed =
        actionByHeading(*river, "GET REED CLEAR - MOVE 1 OF 2");
    check(
        reed && reed->instruction.find("hold the left button on Reed") !=
                std::string_view::npos &&
            reed->instruction.find("then release") != std::string_view::npos &&
            reed->instruction.find("TARGET: C6") != std::string_view::npos &&
            reed->instruction.find("focus starts on Reed") != std::string_view::npos &&
            reed->instruction.find("Enter again when focus jumps to C6") !=
                std::string_view::npos &&
            reed->instruction.find("Step moves one square") != std::string_view::npos,
        "River Teeth first action gives exact mouse and keyboard activation semantics");

    const StoryScriptAction* foresight =
        actionByHeading(*readingRoom, "CHOOSE WITH FORESIGHT");
    check(
        foresight && foresight->kind == StoryActionKind::ChooseForesight &&
            foresight->cardTitle == "Fey Messenger" &&
            foresight->instruction.find("Fey Messenger is required") !=
                std::string_view::npos &&
            foresight->correction.find("Fey Messenger is required") !=
                std::string_view::npos &&
            foresight->correction.find("not Starbloom Knight") !=
                std::string_view::npos,
        "Reading Room identifies its required Foresight choice and corrects the real alternative");

    const StoryScriptAction* telos =
        actionByHeading(*river, "TELOS - TRAVEL");
    check(
        telos && telos->kind == StoryActionKind::Move &&
            telos->targetRow == 4 && telos->targetColumn == 0 &&
            telos->instruction.find("ordinary one-square move") != std::string_view::npos &&
            telos->instruction.find("clears Reed's shot along column B") != std::string_view::npos,
        "River Teeth gives Telos a causal printed Travel action");

    const StoryScriptAction* firstScent =
        actionByHeading(*scent, "PLACE SCENT TIMBER - NORTH");
    const bool scentIsPanelOnly = std::any_of(
        scent->briefing.begin(), scent->briefing.end(), [](const StoryPanel& panel) {
            return panel.text.find("already in place") != std::string_view::npos &&
                panel.text.find("not a player action") != std::string_view::npos &&
                panel.text.find("game rule") != std::string_view::npos;
        });
    check(
        firstScent == nullptr && scentIsPanelOnly,
        "Blackthorn scent work is story context, never a fake player action");

    const bool scentDefinesBothInputs = std::any_of(
        scent->briefing.begin(), scent->briefing.end(), [](const StoryPanel& panel) {
            return panel.text.find("Mouse: drag") != std::string_view::npos &&
                panel.text.find("and release") != std::string_view::npos &&
                panel.text.find("Keyboard: focus") != std::string_view::npos &&
                panel.text.find("press Enter") != std::string_view::npos;
        });
    const StoryScriptAction* firstHarnessMove =
        actionByHeading(*scent, "OPEN THE WORK LANE");
    check(
        scentDefinesBothInputs && firstHarnessMove &&
            firstHarnessMove->instruction.find("Mouse: drag and release") !=
                std::string_view::npos &&
            firstHarnessMove->instruction.find("Keyboard: focus") !=
                std::string_view::npos,
        "independently selectable Blackthorn teaches mouse and keyboard board input before its first action");
    check(
        actionByHeading(*scent, "MEASURE THE SECOND SKIPPED TURN") == nullptr &&
            actionByHeading(*scent, "SECOND ACTIVATION DISABLED") == nullptr &&
            std::find(
                scent->masteryRules.begin(), scent->masteryRules.end(),
                "disable-two-turns") == scent->masteryRules.end() &&
            std::find(
                scent->masteryRules.begin(), scent->masteryRules.end(),
                "status-duration") == scent->masteryRules.end() &&
            std::find(
                scent->masteryCards.begin(), scent->masteryCards.end(),
                "Blackthorn Alchemist") == scent->masteryCards.end(),
        "Blackthorn opening observes one Disable skip and defers full duration and Alchemist mastery to the later proof");

    const StoryScriptAction* bladeDance =
        actionByHeading(*auction, "VANYA - BLADE DANCE");
    check(
        bladeDance &&
            bladeDance->instruction.find("Blade Dance") != std::string_view::npos &&
            bladeDance->instruction.find("Repeat 1") != std::string_view::npos &&
            bladeDance->instruction.find("or Pass") != std::string_view::npos &&
            bladeDance->instruction.find("Nima's route") != std::string_view::npos &&
            bladeDance->instruction.find("before another piece can act") !=
                std::string_view::npos,
        "Blade Dance lesson explains repeat count, escape, action lock, and story purpose");

    const StoryScriptAction* collision =
        actionByHeading(*terms, "CHOOSE AMBUSH BEFORE THE HIDDEN COLLISION");
    check(
        collision && collision->targetRow == 5 && collision->targetColumn == 2 &&
            collision->instruction.find("printed Ambush") != std::string_view::npos &&
            collision->instruction.find("takes 1 damage") != std::string_view::npos &&
            collision->instruction.find("both pieces materialize") != std::string_view::npos,
        "hidden collision lesson names its real Ambush, damage, and materialization outcome");

    const StoryScriptAction* relentless =
        actionByHeading(*capstone, "VICTOR - OPTIONAL RELENTLESS ACTION");
    const StoryScriptAction* summon =
        actionByHeading(*capstone, "COMMAND THE SUMMON");
    check(
        relentless &&
            relentless->instruction.find("optional") != std::string_view::npos &&
            relentless->instruction.find("ordinary play also allows End Turn") !=
                std::string_view::npos,
        "Relentless lesson says the extra action is optional in ordinary play");
    check(
        summon && summon->correction.find("front square at B7") != std::string_view::npos,
        "player-one Summon lesson names the correct B7 front square");

    const StoryScriptAction* decree =
        actionByHeading(*cathedral, "DEWBELL - DECREE");
    check(
        decree &&
            decree->expectedActionProfile == "Dewbell's Decree" &&
            decree->instruction.find(decree->expectedActionProfile) !=
                std::string_view::npos &&
            decree->correction.find(decree->expectedActionProfile) !=
                std::string_view::npos &&
            decree->correction.find("Royal Decree") == std::string_view::npos,
        "SE01 names Dewbell's exact printed action in both instruction and correction");

    check(
        vault->aftermath.size() >= 6 &&
            vault->aftermath[0].text.find("automatic, not a button") !=
                std::string_view::npos &&
            vault->aftermath[1].text.find("footprint means every board square") !=
                std::string_view::npos &&
            vault->aftermath[2].text.find("regains Intercept") !=
                std::string_view::npos &&
            vault->aftermath[2].text.find("Disable, Sleep, or Growth") !=
                std::string_view::npos &&
            vault->aftermath[3].text.find("Spells and zero-damage attacks") !=
                std::string_view::npos,
        "MW11 splits Intercept trigger, footprint, reset, statuses, and exclusions into novice-sized panels");

}

void validateNarrativeCoherence()
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
    const auto panelCopy = [&](const StoryMission* mission) {
        std::string copy;
        if (!mission)
        {
            return copy;
        }
        appendPanels(copy, mission->briefing);
        return copy;
    };
    const auto completePanelCopy = [&](const StoryMission* mission) {
        std::string copy;
        if (!mission)
        {
            return copy;
        }
        appendPanels(copy, mission->briefing);
        appendPanels(copy, mission->aftermath);
        return copy;
    };
    const auto hasSpeaker = [](const StoryMission* mission, std::string_view speaker) {
        return mission && std::any_of(
            mission->briefing.begin(), mission->briefing.end(),
            [&](const StoryPanel& panel) { return panel.speaker == speaker; });
    };

    const StoryMission* debtors =
        missionById(StoryCampaign::Mirewatch, "s02_no_one_alone");
    const std::string debtorCopy = panelCopy(debtors);
    check(
        debtors && debtors->briefing.size() == 11 &&
            debtors->optionalRehearsal && debtors->script.empty() &&
            debtors->masteryCards.empty() && debtors->masteryRules.empty() &&
            debtorCopy.find("twenty") != std::string::npos &&
            debtorCopy.find("secured transfer") != std::string::npos &&
            debtorCopy.find("Every independent count returns twenty") != std::string::npos &&
            debtorCopy.find("Fear arrived before my conscience") != std::string::npos &&
            debtorCopy.find("No one enters alone") != std::string::npos &&
            debtorCopy.find("pearl") == std::string::npos &&
            debtorCopy.find("Elliot") == std::string::npos,
        "No One Alone stays on Hollis's debtor warning from report through verification and response");

    const StoryMission* memory =
        missionById(StoryCampaign::Mirewatch, "s05_memory_contradicts");
    const std::string memoryCopy = panelCopy(memory);
    check(
        memory && memory->briefing.size() == 7 &&
            memoryCopy.find("moonfruit") != std::string::npos &&
            memoryCopy.find("sensory memory") != std::string::npos &&
            memoryCopy.find("no scent comes") != std::string::npos &&
            memoryCopy.find("senior branch removed") != std::string::npos &&
            memoryCopy.find("memory's contradiction") != std::string::npos &&
            memoryCopy.find("Lash's cuff") == std::string::npos &&
            memoryCopy.find("Mirror") == std::string::npos,
        "The Memory That Contradicts follows one moonfruit bargain, answer, cost, and decision");

    const StoryMission* lessonNight =
        missionById(StoryCampaign::Mirewatch, "mw08_lesson_night");
    const std::string lessonNightCopy = panelCopy(lessonNight);
    check(
        lessonNight && lessonNight->briefing.size() == 5 &&
            lessonNightCopy.find("dry socks, pear broth") != std::string::npos &&
            lessonNightCopy.find("Seal the amnesty first") != std::string::npos &&
            lessonNightCopy.find("prepared flood trap") != std::string::npos &&
            lessonNightCopy.find("seven escape") != std::string::npos &&
            lessonNightCopy.find("until she is shot") != std::string::npos &&
            lessonNightCopy.find("Erevan holds route security") !=
                std::string::npos &&
            lessonNightCopy.find("Pavo") == std::string::npos &&
            lessonNightCopy.find("Nettle") == std::string::npos &&
            lessonNightCopy.find("Zippy") == std::string::npos &&
            lessonNightCopy.find("Seelie Patrol") == std::string::npos &&
            lessonNightCopy.find("detainees") == std::string::npos &&
            lessonNightCopy.find("first sight of the grove") == std::string::npos,
        "The Lesson at Night stays on the revised Chapter 10 bargain, flood trap, Bluewater loss, and route-security consequence");

    const StoryMission* reveal =
        missionById(StoryCampaign::Mirewatch, "mw13_making_credit");
    const std::string revealCopy = completePanelCopy(reveal);
    check(
        reveal && reveal->optionalRehearsal &&
            reveal->objectiveSpec.kind == StoryObjectiveKind::Scripted &&
            reveal->briefing.size() == 6 &&
            revealCopy.find("Forty-three people") != std::string::npos &&
            revealCopy.find("Mog and Braun speak separately") != std::string::npos &&
            revealCopy.find("Gearjaw") != std::string::npos &&
            revealCopy.find("blue trace") != std::string::npos &&
            revealCopy.find("does not claim a Goblin Ambusher") != std::string::npos &&
            revealCopy.find("Reveal dealt no damage") != std::string::npos,
        "Making Credit preserves all canonical witness beats and clearly fences its optional Reveal reconstruction");

    const StoryMission* harness =
        missionById(StoryCampaign::Blackthorn, "bt01_harness_hunger");
    const StoryMission* customs =
        missionById(StoryCampaign::Blackthorn, "bt02_customs_bell");
    const StoryMission* sanctuary =
        missionById(StoryCampaign::Blackthorn, "bt03_sanctuary_debt");
    const StoryMission* hiddenRoad =
        missionById(StoryCampaign::Blackthorn, "bt04_terms_conditions");
    const StoryMission* office =
        missionById(StoryCampaign::Blackthorn, "bt05_freight_office");
    const StoryMission* watcher =
        missionById(StoryCampaign::Mirewatch, "mw04_watcher_protects");
    const StoryMission* watchedOffice =
        missionById(StoryCampaign::Mirewatch, "mw05_watched_office");
    const StoryMission* costSeen =
        missionById(StoryCampaign::Mirewatch, "mw06_cost_seen");
    const StoryMission* auctionPlan =
        missionById(StoryCampaign::Mirewatch, "mw10_beautiful_plan");
    const std::string watcherCopy = panelCopy(watcher);
    const std::string hiddenRoadCopy = panelCopy(hiddenRoad);
    check(
        watcher && hiddenRoad &&
            watcherCopy.find("Lio, Juniper's wounded-mason contact") !=
                std::string::npos &&
            watcherCopy.find("Blackthorn's prepared question") != std::string::npos &&
            watcherCopy.find("his children Tench and Seli") != std::string::npos &&
            watcherCopy.find("Company observers test") != std::string::npos &&
            hiddenRoadCopy.find("Lio, Juniper's wounded-mason contact") !=
                std::string::npos &&
            hiddenRoadCopy.find("his children Tench and Seli") !=
                std::string::npos,
        "both Chapter 3 tutorials identify Lio, his arrest, and the children threatened before the watcher chooses them");

    const auto hasFreightOfficeSetup = [](const std::string& copy) {
        return copy.find("freight-office weighing clerk") != std::string::npos &&
            copy.find("black tracking dye") != std::string::npos &&
            copy.find("planted confession") != std::string::npos &&
            copy.find("four authentic freight chits") != std::string::npos &&
            copy.find("920 living pounds left") != std::string::npos &&
            copy.find("524 arrived") != std::string::npos &&
            copy.find("396 were removed in transit") != std::string::npos &&
            copy.find("orders them to warn her mother") != std::string::npos &&
            copy.find("shuts the canal hatch behind them") != std::string::npos &&
            copy.find("giving up her place in their boat") != std::string::npos;
    };
    check(
        watchedOffice && office &&
            hasFreightOfficeSetup(panelCopy(watchedOffice)) &&
            hasFreightOfficeSetup(panelCopy(office)),
        "both freight-office tutorials establish Hara, authentic evidence, the tracking mark, and her costly hatch choice");

    const std::string costSeenCopy = panelCopy(costSeen);
    check(
        costSeen &&
            costSeenCopy.find(
                "Mara Mudfen runs the bakery with Pell and their children, Ollo and Nessa") !=
                std::string::npos &&
            costSeenCopy.find("seize all four") != std::string::npos &&
            costSeenCopy.find("inherited arrears") != std::string::npos &&
            costSeenCopy.find("flight risk") != std::string::npos &&
            costSeenCopy.find("interference with pledged property") !=
                std::string::npos &&
            costSeenCopy.find("supplemental processing") != std::string::npos &&
            costSeenCopy.find("wagon with six hidden eyes") != std::string::npos &&
            costSeenCopy.find("machine records every helper") != std::string::npos,
        "The Cost of Being Seen introduces all four Mudfens, the seizure pretext, and the recorder's purpose");

    const std::string auctionPlanCopy = panelCopy(auctionPlan);
    check(
        auctionPlan && auctionPlan->briefing.size() >= 6 &&
            auctionPlan->briefing[0].speaker == "Narrator" &&
            auctionPlan->briefing[1].speaker == "Narrator" &&
            auctionPlan->briefing[2].speaker == "Narrator" &&
            auctionPlan->briefing[3].speaker == "Narrator" &&
            auctionPlan->briefing[4].speaker == "Nima" &&
            auctionPlanCopy.find("Gilded Vault's shrouded auction") !=
                std::string::npos &&
            auctionPlanCopy.find("masked bidders price eleven captive lots") !=
                std::string::npos &&
            auctionPlanCopy.find("free every breathing captive") != std::string::npos &&
            auctionPlanCopy.find("Mirror of Nine Lives") != std::string::npos &&
            auctionPlanCopy.find("artifacts never share custody") !=
                std::string::npos &&
            auctionPlanCopy.find("Joni locks the Mirror") != std::string::npos &&
            auctionPlanCopy.find("hidden Root Key is found") !=
                std::string::npos &&
            auctionPlanCopy.find("Donella carries it separately") !=
                std::string::npos &&
            auctionPlanCopy.find("Neither is a player interaction") !=
                std::string::npos &&
            auctionPlanCopy.find("she opens her own lock") != std::string::npos,
        "A Beautiful Plan establishes the auction, rescue and custody rules before Nima's self-release");

    const std::string victorOpeningCopy = completePanelCopy(harness) +
        completePanelCopy(customs) + completePanelCopy(sanctuary) +
        completePanelCopy(hiddenRoad) + completePanelCopy(office);
    check(
        hasSpeaker(harness, "Victor Greyshard") &&
            hasSpeaker(customs, "Victor Greyshard") &&
            hasSpeaker(sanctuary, "Victor Greyshard") &&
            hasSpeaker(sanctuary, "Thaeron Baelstone") &&
            hasSpeaker(hiddenRoad, "Victor Greyshard") &&
            hasSpeaker(office, "Victor Greyshard") &&
            hasSpeaker(office, "Fizzlewick Gearwright") &&
            victorOpeningCopy.find("order that bows to no name") != std::string::npos &&
            victorOpeningCopy.find("name becomes a road") != std::string::npos &&
            victorOpeningCopy.find("admitted debt") != std::string::npos &&
            victorOpeningCopy.find("nineteen years") != std::string::npos &&
            victorOpeningCopy.find("Baelstone name instead of mine") != std::string::npos,
        "Blackthorn's first five missions expose Victor's predictive-order motive and Thaeron/Fizzlewick tensions in character voice");

    {
        const std::array<std::pair<std::string_view, std::string_view>, 5>
            blackthornPerspectiveScenes = {{
                {"s01_hospitality", "unpriced route"},
                {"s02_no_one_alone", "Tomorrow Off"},
                {"s03_published_mystery", "The Trap Had an Audience"},
                {"s05_memory_contradicts", "The Exception in Thaeron's Hand"},
                {"s06_town_owns_itself", "What Remains of Blackthorn"}}};
        bool allDistinctAndFocused = true;
        std::string relationshipCopy;
        for (const auto& [id, marker] : blackthornPerspectiveScenes)
        {
            const StoryMission* blackthorn =
                missionById(StoryCampaign::Blackthorn, id);
            const StoryMission* mirewatch =
                missionById(StoryCampaign::Mirewatch, id);
            const std::string blackthornCopy = panelCopy(blackthorn);
            allDistinctAndFocused = allDistinctAndFocused &&
                blackthorn && mirewatch &&
                blackthorn->briefing.size() == 5 &&
                panelCopy(blackthorn) != panelCopy(mirewatch) &&
                (blackthorn->title.find(marker) != std::string_view::npos ||
                    blackthornCopy.find(marker) != std::string::npos) &&
                std::all_of(
                    blackthorn->briefing.begin(), blackthorn->briefing.end(),
                    [](const StoryPanel& value) {
                        return value.text.size() <= 230;
                    });
            relationshipCopy += ' ';
            relationshipCopy += blackthornCopy;
        }
        check(
            allDistinctAndFocused,
            "Blackthorn's five shared Book One events use focused Company-perspective scenes instead of Mirewatch's verbatim copy");
        check(
            relationshipCopy.find("Thaeron") != std::string::npos &&
                relationshipCopy.find("Fizzlewick") != std::string::npos &&
                relationshipCopy.find("Mog") != std::string::npos &&
                relationshipCopy.find("Grask") != std::string::npos &&
                relationshipCopy.find("Braun refused") != std::string::npos &&
                relationshipCopy.find("Victor") != std::string::npos,
            "Blackthorn connective scenes keep Victor's personal fractures with Thaeron, Fizzlewick, Mog, Grask, and Braun visible");
    }

    const StoryMission* mirewatchReckoning =
        missionById(StoryCampaign::Mirewatch, "mw24_agent_not_heir");
    const StoryMission* blackthornReckoning =
        missionById(StoryCampaign::Blackthorn, "bt17c_victor_reckoning");
    const std::string mirewatchReckoningCopy = completePanelCopy(mirewatchReckoning);
    const std::string blackthornReckoningCopy = panelCopy(blackthornReckoning);
    const auto recordsVictorReckoning = [](const std::string& copy) {
        return copy.find("three cage levels across Grask") != std::string::npos &&
            copy.find("confirms him dead") != std::string::npos &&
            copy.find("Society custody. Public record.") != std::string::npos &&
            copy.find("No sentence here, no immunity") != std::string::npos &&
            copy.find("Northglass and sixty-one loads") != std::string::npos &&
            copy.find("guarded capsule") != std::string::npos &&
            copy.find("matching glass against Victor's back molar") !=
                std::string::npos &&
            copy.find("turns it beneath her ribs. Pavo steps between them") !=
                std::string::npos &&
            copy.find("attacks Pavo") != std::string::npos &&
            copy.find("Pavo drives the blade beneath Victor's raised arm") !=
                std::string::npos &&
            copy.find("reaches once more for Nettle") != std::string::npos &&
            copy.find("surrender, breach, and death") != std::string::npos;
    };
    check(
        mirewatchReckoning && mirewatchReckoning->aftermath.size() == 5 &&
            recordsVictorReckoning(mirewatchReckoningCopy),
        "Mirewatch's World Tree finale performs Victor's surrender, testimony, breach, and death instead of compressing them into a synopsis");
    check(
        blackthornReckoning &&
            blackthornReckoning->objectiveSpec.kind == StoryObjectiveKind::StoryOnly &&
            blackthornReckoning->briefing.size() == 5 &&
            recordsVictorReckoning(blackthornReckoningCopy),
        "Blackthorn restores the same five-beat canonical Victor reckoning after both required non-canon exams");

    const StoryMission* blackthornMonsterRules =
        missionById(StoryCampaign::Blackthorn, "bt15_monster_rules");
    const std::string blackthornMonsterCopy = completePanelCopy(blackthornMonsterRules);
    check(
        blackthornMonsterRules &&
            blackthornMonsterCopy.find("Fen Oake") != std::string::npos &&
            blackthornMonsterCopy.find("knowingly tests one crossing and dies") !=
                std::string::npos &&
            blackthornMonsterCopy.find("hidden Company tack") != std::string::npos,
        "Blackthorn records Fen Oake's fixed death when the hidden Company tack changes his test crossing");

    const StoryMission* firstBaalzapub =
        missionById(StoryCampaign::Seelie, "se04a_lash_reads_the_road");
    const std::string firstBaalzapubCopy = panelCopy(firstBaalzapub);
    check(
        firstBaalzapub &&
            firstBaalzapubCopy.find("Baalzapub") != std::string::npos &&
            firstBaalzapubCopy.find("theater and workshop") != std::string::npos &&
            firstBaalzapubCopy.find("Aelon") != std::string::npos,
        "Seelie introduces Baalzapub, Fizzlewick, and Aelon before the climax depends on them");

    const StoryMission* cathedralRoad =
        missionById(StoryCampaign::Seelie, "se00_what_mirror_remembers");
    const std::string cathedralRoadCopy = panelCopy(cathedralRoad);
    check(
        cathedralRoad && cathedralRoad->briefing.size() == 6 &&
            cathedralRoad->sourceChapter ==
                "Book Two, Chapter 16 - What the Mirror Remembers" &&
            cathedralRoadCopy.find("Previously - Book One") != std::string::npos &&
            cathedralRoadCopy.find("freed Sylvara") != std::string::npos &&
            cathedralRoadCopy.find("small orange cat") != std::string::npos &&
            cathedralRoadCopy.find("clockwork refuge") != std::string::npos &&
            cathedralRoadCopy.find("clockwork keeper") != std::string::npos &&
            cathedralRoadCopy.find("copied measuring plates drain") !=
                std::string::npos &&
            cathedralRoadCopy.find("ten original plates") == std::string::npos &&
            cathedralRoadCopy.find("smoke-colored half") == std::string::npos &&
            cathedralRoadCopy.find("nine former Vespers") != std::string::npos &&
            cathedralRoadCopy.find("presumed privilege") != std::string::npos &&
            cathedralRoadCopy.find(
                "Marrowind - a Seelie archer riding the nine-tailed Vaeloren") !=
                std::string::npos &&
            cathedralRoadCopy.find("Cathedral of Last Lights remains") !=
                std::string::npos &&
            cathedralRoadCopy.find("Three names to carry forward") !=
                std::string::npos &&
            cathedralRoadCopy.find("orange cat seeking Caltheriel's freedom") !=
                std::string::npos &&
            cathedralRoadCopy.find("next board introduces its defenders as they act") !=
                std::string::npos &&
            cathedralRoadCopy.find("theater") == std::string::npos &&
            cathedralRoadCopy.find("mentor") == std::string::npos,
        "Seelie opens with a standalone Book One handoff and the revised Chapter 16 Cathedral-road cause without restoring the skipped theatre prologue");

    const StoryMission* sellaReceives =
        missionById(StoryCampaign::Seelie, "se08a_sella_receives_herself");
    const std::string sellaCopy = panelCopy(sellaReceives);
    check(
        sellaReceives && sellaReceives->briefing.size() == 6 &&
            sellaCopy.find("Lord Pallid Thread is a name-eater") !=
                std::string::npos &&
            sellaCopy.find("no receiving person exists") != std::string::npos &&
            sellaCopy.find("Pallid's mouth crosses the page") !=
                std::string::npos &&
            sellaCopy.find("Pallid's mouth crosses the page") >
                sellaCopy.find("Lord Pallid Thread is a name-eater"),
        "Sella's scene introduces Pallid, his rule, and his motive before he eats her written name");

    const StoryMission* parallelDiscoveries =
        missionById(StoryCampaign::Seelie, "se18a_complete_is_a_label");
    const StoryMission* cinderworks =
        missionById(StoryCampaign::Seelie, "se21a_cinderworks_choice");
    const StoryMission* lowerWells =
        missionById(StoryCampaign::Seelie, "se21_below_last_stair");
    const StoryMission* orchardMemory =
        missionById(StoryCampaign::Seelie, "se22c_orchard_memory");
    const StoryMission* firstPrison =
        missionById(StoryCampaign::Seelie, "se24_first_prison_design");
    const StoryMission* fishRib =
        missionById(StoryCampaign::Seelie, "se25_three_turns_fish_rib");
    const StoryMission* abandonedReceiver =
        missionById(StoryCampaign::Seelie, "se25a_abandoned_receiver");
    const std::string parallelCopy = panelCopy(parallelDiscoveries);
    const std::string cinderworksCopy = panelCopy(cinderworks);
    const std::string lowerWellsCopy = panelCopy(lowerWells);
    const std::string firstPrisonCopy = panelCopy(firstPrison);
    const std::string fishRibCopy = panelCopy(fishRib);
    const std::string receiverCopy = panelCopy(abandonedReceiver);
    check(
        parallelDiscoveries &&
            parallelCopy.find("parallel investigations") != std::string::npos,
        "Seelie's Act III recap identifies its rapid coalition discoveries as parallel investigations");
    check(
        cinderworks &&
            cinderworksCopy.find("Nettle and Vesper") != std::string::npos &&
            cinderworksCopy.find("Pavo and Caltheriel remain above") !=
                std::string::npos &&
            cinderworksCopy.find("maintenance lift has no floor") !=
                std::string::npos &&
            cinderworksCopy.find("206 living") == std::string::npos &&
            lowerWells && lowerWellsCopy.find("206 living") != std::string::npos &&
            lowerWellsCopy.find("breaks the wheel") != std::string::npos,
        "Seelie separates the Cinderworks choice and cable-cage cause from the Lower Wells evacuation and count");
    check(
        hasSpeaker(orchardMemory, "Caltheriel - THROUGH LIGHT NODE"),
        "Caltheriel's Cathedral memory offer is explicitly delivered through the surface light node");
    check(
        firstPrison &&
            firstPrisonCopy.find("five pairs of powers") != std::string::npos &&
            firstPrisonCopy.find("REMOVED THAT EXIT") != std::string::npos &&
            fishRib && fishRibCopy.find("Nima Salt") != std::string::npos &&
            fishRibCopy.find("missing, not dead") != std::string::npos &&
            abandonedReceiver && receiverCopy.find("eleven sockets") !=
                std::string::npos &&
            receiverCopy.find("Vanya's blue fishbone key") != std::string::npos &&
            receiverCopy.find("Nima Salt") == std::string::npos,
        "Seelie gives the first-prison design, Nima's open record, and the abandoned receiver separate causal chapters");

    const StoryMission* beforeNextDawn =
        missionById(StoryCampaign::Seelie, "se18c_before_next_dawn");
    const StoryMission* kindness =
        missionById(StoryCampaign::Seelie, "se18d_kindness_without_debt");
    const StoryMission* chapterFourteenPromise =
        missionById(StoryCampaign::Seelie, "se18_what_complete_means");
    const std::string beforeNextDawnCopy = panelCopy(beforeNextDawn);
    const std::string chapterFourteenPromiseCopy = panelCopy(chapterFourteenPromise);
    check(
        beforeNextDawn && kindness && chapterFourteenPromise &&
            beforeNextDawn->briefing.size() == 4 &&
            beforeNextDawnCopy.find("Jarek Sorn") != std::string::npos &&
            beforeNextDawnCopy.find("Olla Pere") != std::string::npos &&
            beforeNextDawnCopy.find("Aelon") != std::string::npos &&
            beforeNextDawnCopy.find("Silkmaw") == std::string::npos &&
            chapterFourteenPromise->briefing.size() == 6 &&
            chapterFourteenPromise->sourceChapter ==
                "Book Three, Chapter 14 - A Promise Cannot Breathe" &&
            chapterFourteenPromiseCopy.find("Ebbin") != std::string::npos &&
            chapterFourteenPromiseCopy.find("Ruvan") != std::string::npos &&
            chapterFourteenPromiseCopy.find("Silkmaw") != std::string::npos &&
            chapterFourteenPromiseCopy.find("Sorn") == std::string::npos &&
            chapterFourteenPromiseCopy.find("red chest") == std::string::npos &&
            chapterFourteenPromiseCopy.find("Before the hearing closes") ==
                std::string::npos,
        "Seelie keeps revised Book Three Chapters 12, 13, and 14 as separate chronological decisions");

    const StoryMission* bookThreeOpening =
        missionById(StoryCampaign::Seelie, "se12_first_boats_burn");
    const std::string bookThreeOpeningCopy = panelCopy(bookThreeOpening);
    check(
        bookThreeOpening &&
            bookThreeOpening->title == "BOOK THREE - The First Boats Burn" &&
            bookThreeOpening->sourceChapter.find("BOOK THREE BEGINS") !=
                std::string_view::npos &&
            hasSpeaker(bookThreeOpening, "Nettle's field record - BOOK TWO CLOSED") &&
            hasSpeaker(bookThreeOpening, "Pavo's slate - BOOK THREE BEGINS") &&
            bookThreeOpeningCopy.find("Caltheriel walked free") !=
                std::string::npos &&
            bookThreeOpeningCopy.find("Blackthorn's charter fell") !=
                std::string::npos &&
            bookThreeOpeningCopy.find("follow Nettle and me") !=
                std::string::npos &&
            bookThreeOpeningCopy.find("invasion begins") != std::string::npos,
        "Seelie marks the Book Two ending, opens Book Three explicitly, and declares Nettle and Pavo as the new field anchors");

    const StoryMission* nyxaraInfest =
        missionById(StoryCampaign::Seelie, "se07a_queens_price_scene");
    const std::string nyxaraInfestCopy = completePanelCopy(nyxaraInfest);
    check(
        nyxaraInfest && nyxaraInfest->optionalRehearsal &&
            nyxaraInfest->objectiveSpec.kind == StoryObjectiveKind::Scripted &&
            nyxaraInfest->briefing.size() == 7 &&
            nyxaraInfestCopy.find("Three living helpers") != std::string::npos &&
            nyxaraInfestCopy.find("households of two dead helpers") !=
                std::string::npos &&
            nyxaraInfestCopy.find("missing consent") != std::string::npos &&
            nyxaraInfestCopy.find("knocks Reed's cane aside") != std::string::npos &&
            nyxaraInfestCopy.find("left side of Pipistrel's compound gaze") !=
                std::string::npos &&
            nyxaraInfestCopy.find("three bright facets cloud") == std::string::npos &&
            nyxaraInfestCopy.find("guard turned the blade") == std::string::npos &&
            nyxaraInfestCopy.find("Help may be accepted. Ownership may not.") !=
                std::string::npos &&
            nyxaraInfestCopy.find("not claims about what happened") !=
                std::string::npos &&
            nyxaraInfestCopy.find("actual one-Health Bristlejack") !=
                std::string::npos,
        "The Queen's Price separates Nyxara's unasked household seal from Pipistrel moving Reed out of the blade's path and paying with left-side sight");

    const StoryMission* courtBehindCurtain =
        missionById(StoryCampaign::Seelie, "se05_court_behind_curtain");
    const std::string courtCopy = panelCopy(courtBehindCurtain);
    check(
        courtBehindCurtain &&
            courtBehindCurtain->objectiveSpec.kind == StoryObjectiveKind::StoryOnly &&
            courtBehindCurtain->briefing.size() == 6 &&
            courtBehindCurtain->briefing[4].speaker == "Erevan the Shadow" &&
            courtCopy.find("Ivara's evidence may travel under limits") !=
                std::string::npos &&
            courtCopy.find("private identity is not our fare") != std::string::npos,
        "The Court Behind the Curtain routes Erevan's refusal to trade Ivara's private identity");

    const StoryMission* cityAtNight =
        missionById(StoryCampaign::Seelie, "se22b_city_spends_at_night");
    const std::string cityAtNightCopy = panelCopy(cityAtNight);
    check(
        cityAtNight && cityAtNight->objectiveSpec.kind == StoryObjectiveKind::StoryOnly &&
            cityAtNight->briefing.size() == 6 &&
            cityAtNightCopy.find("standing permission to shoot") !=
                std::string::npos &&
            cityAtNightCopy.find("four hundredweight of onions") !=
                std::string::npos &&
            cityAtNightCopy.find("Emmet Drane") != std::string::npos &&
            cityAtNightCopy.find("WHAT IT BUYS") != std::string::npos,
        "What the City Spends at Night preserves Pavo's refusal, Lash's useful diversion, and the public accounting of both");

    const StoryMission* hungerHasNoFace =
        missionById(StoryCampaign::Seelie, "se26a_hunger_has_no_face");
    const StoryMission* burningRoot =
        missionById(StoryCampaign::Seelie, "se26b_burning_root");
    const std::string hungerCopy = panelCopy(hungerHasNoFace);
    const std::string burningRootCopy = panelCopy(burningRoot);
    check(
        hungerHasNoFace && burningRoot &&
            hungerHasNoFace->objectiveSpec.kind == StoryObjectiveKind::StoryOnly &&
            burningRoot->objectiveSpec.kind == StoryObjectiveKind::StoryOnly &&
            hungerHasNoFace->briefing.size() == 6 &&
            burningRoot->briefing.size() == 6 &&
            hungerHasNoFace->sourceChapter ==
                "Book Three, Chapter 27 - Hunger Has No Face" &&
            burningRoot->sourceChapter ==
                "Book Three, Chapter 28 - The Burning Root" &&
            hungerCopy.find("crosses fully into the cavity") != std::string::npos &&
            hungerCopy.find("lost sensation never returns") != std::string::npos &&
            hungerCopy.find("My hidden house road") != std::string::npos &&
            hungerCopy.find("I choose to break it open") != std::string::npos &&
            hungerCopy.find("main clock stops") != std::string::npos &&
            hungerCopy.find("gives no answer but remains present") !=
                std::string::npos &&
            burningRootCopy.find("Larkspur frees the strap") != std::string::npos &&
            burningRootCopy.find("Her pulse stops") != std::string::npos &&
            burningRootCopy.find("all thirty-two residents") !=
                std::string::npos &&
            burningRootCopy.find("reseats Tockman's frozen withdrawal hand") !=
                std::string::npos &&
            burningRootCopy.find("governing gear stays cracked") !=
                std::string::npos &&
            burningRootCopy.find("cannot move himself") != std::string::npos &&
            burningRootCopy.find("bell resumes exact intervals") !=
                std::string::npos,
        "Seelie keeps Chapters 27 and 28 separate while preserving Reed's rescue and permanent cost, Briar's chosen road loss, Larkspur's death, and Tockman's limited repair");

    const StoryMission* roadOfSmallLights =
        missionById(StoryCampaign::Seelie, "se03_road_small_lights");
    const StoryMission* bellAndOrder =
        missionById(StoryCampaign::Seelie, "se15_bell_and_order");
    const StoryMission* breathingPromise =
        missionById(StoryCampaign::Seelie, "se18_what_complete_means");
    const StoryMission* controlledFall =
        missionById(StoryCampaign::Seelie, "se23_controlled_fall");
    const std::string roadCopy = panelCopy(roadOfSmallLights);
    const std::string bellCopy = panelCopy(bellAndOrder);
    const std::string promiseCopy = panelCopy(breathingPromise);
    const std::string fallCopy = panelCopy(controlledFall);
    check(
        roadOfSmallLights &&
            hasSpeaker(
                roadOfSmallLights,
                "Nettle Starbright - WINGLESS ENGINEER") &&
            roadCopy.find("Zippy is the flying snail") != std::string::npos &&
            roadCopy.find("he chooses every route") != std::string::npos,
        "Nettle's crop-safe wingless label and Zippy's autonomous harness role are established at the first Seelie road lesson");
    check(
        bellAndOrder && hasSpeaker(bellAndOrder, "Liora Kest - CITY KNIGHT") &&
            bellCopy.find("sister Bryn") != std::string::npos &&
            bellCopy.find("annealing stair") != std::string::npos &&
            bellCopy.find("eight other knights") != std::string::npos,
        "Liora, Bryn, her glassworks knowledge, and her relinquished command are established before the Baalzapub payoff");
    check(
        breathingPromise && hasSpeaker(breathingPromise, "Ebbin - FEY MESSENGER") &&
            hasSpeaker(breathingPromise, "Ruvan - FEY MESSENGER") &&
            promiseCopy.find("bound the whole courier relay into a vow-web") !=
                std::string::npos &&
            promiseCopy.find("until I deliver it") != std::string::npos &&
            promiseCopy.find("until I deliver it") >
                promiseCopy.find("bound the whole courier relay into a vow-web") &&
            promiseCopy.find("Because Ebbin named no stopping point") !=
                std::string::npos &&
            promiseCopy.find("Because Ebbin named no stopping point") >
                promiseCopy.find("until I deliver it") &&
            promiseCopy.find("outer edge missing from one wing") != std::string::npos &&
            promiseCopy.find("My sister vanished on a Mirror road") != std::string::npos &&
            promiseCopy.find("I withdraw it") != std::string::npos &&
            promiseCopy.find("Anwen remains dead") != std::string::npos,
        "Silkmaw's shared vow-web is established before Ebbin's unlimited promise propagates to Anwen and Ruvan chooses a bounded refusal");
    check(
        controlledFall && fallCopy.find("Vaeloren") != std::string::npos &&
            fallCopy.find("I lower the bow and carry my partner") != std::string::npos,
        "Marrowind is introduced through his choice to save Vaeloren before later story callbacks");
    const StoryMission* emperorAtTree =
        missionById(StoryCampaign::Seelie, "se27_emperor_at_tree");
    const std::string emperorAtTreeCopy = completePanelCopy(emperorAtTree);
    check(
        emperorAtTree && emperorAtTreeCopy.find("court light for eight minutes") !=
                std::string::npos &&
            emperorAtTreeCopy.find("no crown or successor") != std::string::npos &&
            emperorAtTreeCopy.find("chronicle events, not battle mechanics") !=
                std::string::npos &&
            emperorAtTreeCopy.find("no battle timer") != std::string::npos &&
            emperorAtTreeCopy.find("no Sun Sprite unit") != std::string::npos &&
            emperorAtTreeCopy.find("no signal to wait for or click") !=
                std::string::npos,
        "Caltheriel's eight-minute chronicle beat is explicitly fenced from the battle's real mechanics");
    check(
        emperorAtTree &&
            emperorAtTreeCopy.find("debt marks") != std::string::npos &&
            emperorAtTreeCopy.find("pull them west into Baalzapub") != std::string::npos &&
            emperorAtTreeCopy.find("two choices") != std::string::npos &&
            emperorAtTreeCopy.find("says, 'Recall.'") != std::string::npos,
        "The Emperor's first Recall follows explicitly from defections feeding Lash's Baalzapub apparatus");

    const StoryMission* vacancy =
        missionById(StoryCampaign::Seelie, "se28a_two_men_one_vacancy");
    const std::string vacancyCopy = panelCopy(vacancy);
    check(
        vacancy && vacancy->briefing.size() == 6 &&
            vacancyCopy.find("The Emperor and Lash are collecting trapped pieces of people") !=
                std::string::npos &&
            vacancyCopy.find("control bodies and roads") != std::string::npos &&
            vacancyCopy.find("that act is called Recall") != std::string::npos &&
            vacancyCopy.find("4,106") != std::string::npos &&
            vacancyCopy.find("captured, detached fragments") != std::string::npos &&
            vacancyCopy.find("receiver: the empty central place") !=
                std::string::npos &&
            vacancyCopy.find("Lash calls a system COMPLETE") != std::string::npos &&
            vacancyCopy.find("4,106 freed fragments") == std::string::npos &&
            vacancyCopy.find("Both men") == std::string::npos &&
            vacancyCopy.find("Three hundred eleven") != std::string::npos &&
            vacancyCopy.find("Lash refuses") != std::string::npos,
        "Two Men and One Vacancy explains the Emperor, Lash, trapped fragments, receiver, Recall, and COMPLETE in ordinary-language causal order");

    const StoryMission* corrections =
        missionById(StoryCampaign::Seelie, "se28b_seven_corrections");
    const std::string correctionCopy = panelCopy(corrections);
    check(
        corrections && correctionCopy.find("concordance glass") != std::string::npos &&
            correctionCopy.find("ten replicas") != std::string::npos &&
            correctionCopy.find("can still raise him") != std::string::npos,
        "Seven Corrections explains why removing the concordance does not stop Lash's ascent");

    const StoryMission* ascent =
        missionById(StoryCampaign::Seelie, "se29a_baalzapub_ascendant");
    const std::string ascentCopy = panelCopy(ascent);
    check(
        ascent && ascent->briefing.size() == 6 &&
            ascentCopy.find("Erevan lifted Lash's black concordance half") !=
                std::string::npos &&
            ascentCopy.find("both remain dangerous") != std::string::npos &&
            ascentCopy.find("regulator forces the replicas") != std::string::npos &&
            ascentCopy.find("unnamed couriers still inside") != std::string::npos &&
            ascentCopy.find("Liora Kest") != std::string::npos &&
            ascentCopy.find("third mark") != std::string::npos &&
            ascentCopy.find("fourth heat mark") != std::string::npos &&
            ascent->briefing[1].text.find("Ruvan") == std::string_view::npos &&
            ascent->briefing[1].text.find("Ebbin") == std::string_view::npos &&
            ascent->briefing[5].text.find("Ruvan") != std::string_view::npos &&
            ascent->briefing[5].text.find("Ebbin") != std::string_view::npos &&
            ascentCopy.find("shaft pins his body-route") != std::string::npos,
        "Baalzapub Ascendant preserves the concordance roles, live threats, unnamed couriers before the shot, and their later reveal");

    const StoryMission* firstRelease =
        missionById(StoryCampaign::Seelie, "se29c_all_authority_returns");
    const std::string firstReleaseCopy = panelCopy(firstRelease);
    check(
        firstRelease && firstRelease->title == "The First Release" &&
            firstReleaseCopy.find("Zippy crosses, pulls") != std::string::npos &&
            firstReleaseCopy.find("Nettle accepts the no") != std::string::npos &&
            firstReleaseCopy.find("Total Recall") == std::string::npos,
        "The First Release is a concrete voluntary rescue beat and does not prematurely execute Total Recall");

    const StoryMission* withdrawals =
        missionById(StoryCampaign::Seelie, "se29c2_separate_withdrawals");
    const std::string withdrawalCopy = panelCopy(withdrawals);
    check(
        withdrawals && withdrawals->briefing.size() == 6 &&
            hasSpeaker(withdrawals, "Hushkeeper") &&
            hasSpeaker(withdrawals, "Caltheriel") &&
            hasSpeaker(withdrawals, "Queen Nyxara") &&
            hasSpeaker(withdrawals, "Pavo's slate") &&
            withdrawalCopy.find("next order") != std::string::npos &&
            withdrawalCopy.find("about to call") != std::string::npos &&
            withdrawalCopy.find("Total Recall") == std::string::npos,
        "Separate Withdrawals keeps each key renunciation in the acting character or witness voice");

    {
        const StoryMission* mirewatchEpilogue =
            missionById(StoryCampaign::Mirewatch, "s06_town_owns_itself");
        const StoryMission* blackthornEpilogue =
            missionById(StoryCampaign::Blackthorn, "s06_town_owns_itself");
        const std::string mirewatchEpilogueCopy = panelCopy(mirewatchEpilogue);
        const std::string blackthornEpilogueCopy = panelCopy(blackthornEpilogue);
        check(
            mirewatchEpilogue &&
                hasSpeaker(mirewatchEpilogue, "Narrator - SEQUEL THREAD") &&
                mirewatchEpilogueCopy.find("Mirewatch won but one route measurement survived") !=
                    std::string::npos &&
                mirewatchEpilogueCopy.find("Root Key remains inside Aelon") !=
                    std::string::npos &&
                mirewatchEpilogueCopy.find("Baalzepub theater") !=
                    std::string::npos &&
                mirewatchEpilogueCopy.find("Baalzapub theater") ==
                    std::string::npos,
            "Mirewatch's Book One epilogue names the surviving route into the Seelie story");
        check(
            blackthornEpilogue &&
                hasSpeaker(
                    blackthornEpilogue,
                    "Fizzlewick Gearwright - SEQUEL THREAD") &&
                hasSpeaker(
                    blackthornEpilogue,
                    "Erevan's murdered mentor - preserved voice") &&
                blackthornEpilogueCopy.find("one surviving route measurement") !=
                    std::string::npos &&
                blackthornEpilogueCopy.find("Root Key remains inside Aelon") !=
                    std::string::npos &&
                blackthornEpilogueCopy.find("Caltheriel's prison") !=
                    std::string::npos &&
                blackthornEpilogueCopy.find("No one alone") != std::string::npos &&
                hasSpeaker(blackthornEpilogue, "Siv Kettle - public postmortem") &&
                blackthornEpilogueCopy.find("confirmed him dead") !=
                    std::string::npos &&
                blackthornEpilogueCopy.find("taken alive") == std::string::npos &&
                blackthornEpilogueCopy.find("Grask - custody statement") ==
                    std::string::npos &&
                blackthornEpilogueCopy.find("Baalzepub theater") !=
                    std::string::npos &&
                blackthornEpilogueCopy.find("Baalzapub theater") ==
                    std::string::npos,
            "Blackthorn's distinct Book One epilogue records Grask's death and carries Fizzlewick's surviving measurement through the original Baalzepub spelling");
    }

    std::size_t totalRecallExecutions = 0;
    constexpr std::string_view totalRecall = "Total Recall. All authority returns.";
    for (const StoryMission& mission : storyMissions(StoryCampaign::Seelie))
    {
        for (const StoryPanel& storyPanel : mission.briefing)
        {
            if (storyPanel.text.find(totalRecall) != std::string_view::npos)
            {
                ++totalRecallExecutions;
            }
        }
        for (const StoryPanel& storyPanel : mission.aftermath)
        {
            if (storyPanel.text.find(totalRecall) != std::string_view::npos)
            {
                ++totalRecallExecutions;
            }
        }
    }
    check(
        totalRecallExecutions == 1,
        "the final Total Recall order executes exactly once across the active Seelie route");
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
                  return step.heading == "BODYGUARD REDIRECTS DAMAGE";
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
            firstOpen->hint.find("Territory Influence") != std::string_view::npos &&
            std::find(
                firstOpen->requiredSurvivorRoles.begin(),
                firstOpen->requiredSurvivorRoles.end(),
                "pavo") != firstOpen->requiredSurvivorRoles.end() &&
            unitCount(*firstOpen, 1) == 3 && unitCount(*firstOpen, 2) == 2,
        "SE14 is a required three-versus-two first free-play bridge that practices territory influence before the finale");
    check(
        bridgeLesson && bridgeLesson->objective.find("Fey Messenger") != std::string_view::npos &&
            std::find(bridgeLesson->requiredSurvivorRoles.begin(),
                bridgeLesson->requiredSurvivorRoles.end(), "messenger") !=
                bridgeLesson->requiredSurvivorRoles.end(),
        "SE02 tells the player that its mandatory Messenger must survive");
    check(
        bridgeLesson && bodyguardBoundaryStep != bridgeLesson->script.end() &&
            bridgeLesson->hint.find("positive-damage hit") != std::string_view::npos &&
            bridgeLesson->hint.find("effects that deal no damage") != std::string_view::npos &&
            bridgeLesson->hint.find("Push") == std::string_view::npos &&
            bridgeLesson->hint.find("Pull") == std::string_view::npos &&
            bridgeLesson->hint.find("Infest") == std::string_view::npos &&
            bridgeLesson->hint.find("Control") == std::string_view::npos &&
            bodyguardBoundaryStep->instruction.find("chosen target") !=
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
            firstOpen->briefing.size() == 11 &&
            std::any_of(
                firstOpen->briefing.begin(), firstOpen->briefing.end(),
                [](const StoryPanel& panel) {
                    return panel.speaker == "Controls" &&
                        panel.text.find("Mouse: hold a unit") != std::string_view::npos &&
                        panel.text.find("Keyboard: focus a unit") != std::string_view::npos &&
                        panel.text.find("Press I") != std::string_view::npos;
                }) &&
            firstOpen->sourceChapter.find("non-canon open") != std::string_view::npos &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Mirewatch, firstOpen->id) == 1,
        "Mirewatch exposes a mastery-free optional 3v3 open check immediately after MW06 while retaining all eight canonical S02 panels");
    check(
        toll && toll->script.empty() &&
            toll->objectiveSpec.kind == StoryObjectiveKind::DefeatAllEnemies &&
            unitCount(*toll, 1) == 4 && unitCount(*toll, 2) == 4 &&
            hasRequiredSurvivor(*toll, "reed") &&
            hasRequiredSurvivor(*toll, "pavo") &&
            hasRequiredSurvivor(*toll, "nettle"),
        "MW18 is an open four-versus-four toll battle that preserves its three bounded allies");
    check(
        finale && finale->script.empty() &&
            finale->objectiveSpec.kind == StoryObjectiveKind::DefeatRole &&
            finale->objectiveSpec.targetRole == "grask" &&
            unitCount(*finale, 1) == 7 && unitCount(*finale, 2) == 7 &&
            hasRequiredSurvivor(*finale, "reed") &&
            hasRequiredSurvivor(*finale, "pavo") &&
            hasRequiredSurvivor(*finale, "nettle") &&
            hasRequiredSurvivor(*finale, "victor") &&
            finale->requiredSurvivorRoles ==
                std::vector<std::string_view>{"reed", "pavo", "nettle", "victor"} &&
            finale->objective.find("Victor is protected") != std::string_view::npos &&
            std::any_of(
                finale->briefing.begin(), finale->briefing.end(),
                [](const StoryPanel& panel) {
                    return panel.text.find("Victor is a protected required survivor") !=
                        std::string_view::npos;
                }),
        "MW24 is an open seven-versus-seven Grask finale that protects Reed, Pavo, Nettle, and Victor");
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
            copy.find("drill") != std::string::npos;
    };

    for (const StoryMission& mission : storyMissions(StoryCampaign::Blackthorn))
    {
        if (!marksRehearsal(mission))
        {
            continue;
        }
        if (mission.id == "bt17_natural_order" ||
            mission.id == "bt17a_field_judgment")
        {
            check(
                !mission.optionalRehearsal,
                std::string(mission.id) +
                    ": the guided synthesis and open-choice bridge are required");
            continue;
        }
        check(
            mission.optionalRehearsal,
            std::string(mission.id) +
                ": Blackthorn rules rehearsals remain optional before the required open-choice bridge");
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
            firstOpen->briefing.size() == 8 &&
            std::any_of(
                firstOpen->briefing.begin(), firstOpen->briefing.end(),
                [](const StoryPanel& panel) {
                    return panel.speaker == "Controls" &&
                        panel.text.find("Mouse: hold a unit") != std::string_view::npos &&
                        panel.text.find("Keyboard: focus a unit") != std::string_view::npos &&
                        panel.text.find("Press I") != std::string_view::npos;
                }) &&
            firstOpen->sourceChapter.find("non-canon open") != std::string_view::npos &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Blackthorn, firstOpen->id) == 1,
        "Blackthorn exposes a mastery-free optional 3v3 open check immediately after BT05 while retaining all five canonical receipt panels");
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
            !requiresRole(*monsterRules, "sylvara") &&
            std::any_of(
                monsterRules->briefing.begin(), monsterRules->briefing.end(),
                [](const StoryPanel& panel) {
                    return panel.text.find("not duplicated on this board") !=
                        std::string_view::npos;
                }),
        "BT15 uses only Ashenfang and her neutral Veteran timing mark, without spectator units or a duplicated Sylvara body");
    check(
        guidedSynthesis && !guidedSynthesis->optionalRehearsal &&
            guidedSynthesis->catchUpForSkippedRehearsals &&
            !guidedSynthesis->script.empty(),
        "Blackthorn requires a guided catch-up before either open-choice exam unless every prior rehearsal carries play evidence");
    if (guidedSynthesis)
    {
        const auto missions = storyMissions(StoryCampaign::Blackthorn);
        const int synthesisIndex = static_cast<int>(
            guidedSynthesis - missions.data());
        bayou::client::StoryProgress allDrillsCompleted;
        allDrillsCompleted.advancedCount = synthesisIndex;
        allDrillsCompleted.completedByPlay.resize(missions.size(), false);
        for (int index = 0; index < synthesisIndex; ++index)
        {
            if (missions[static_cast<std::size_t>(index)].optionalRehearsal)
            {
                allDrillsCompleted.completedByPlay[static_cast<std::size_t>(index)] = true;
            }
        }
        const std::set<std::string_view> expectedDrillIds(
            intendedDrills.begin(), intendedDrills.end());
        std::set<std::string_view> observedDrillIds;
        for (int index = 0; index < synthesisIndex; ++index)
        {
            const StoryMission& prior = missions[static_cast<std::size_t>(index)];
            if (prior.optionalRehearsal)
            {
                observedDrillIds.insert(prior.id);
            }
        }
        check(
            bayou::client::storyMasteryCatchUpMayBeSkipped(
                missions, synthesisIndex, allDrillsCompleted) &&
                observedDrillIds == expectedDrillIds,
            "BT17 offers a no-repetition bypass after exactly the seven intended earlier drills are completed through play");
        std::set<std::string_view> masteryBeforeCatchUp;
        std::set<std::string_view> rulesBeforeCatchUp;
        for (int index = 0; index < synthesisIndex; ++index)
        {
            const StoryMission& prior = missions[static_cast<std::size_t>(index)];
            if (!prior.optionalRehearsal)
            {
                continue;
            }
            masteryBeforeCatchUp.insert(
                prior.masteryCards.begin(), prior.masteryCards.end());
            rulesBeforeCatchUp.insert(
                prior.masteryRules.begin(), prior.masteryRules.end());
        }
        std::set<std::string_view> expectedStarterMastery;
        if (openMastery)
        {
            expectedStarterMastery.insert(
                openMastery->playerDeck.begin(), openMastery->playerDeck.end());
        }
        check(
            !expectedStarterMastery.empty() &&
                std::includes(
                    masteryBeforeCatchUp.begin(), masteryBeforeCatchUp.end(),
                    expectedStarterMastery.begin(), expectedStarterMastery.end()),
            "completing every earlier optional drill masters every ordinary Blackthorn starter card before the catch-up bypass");
        const std::set<std::string_view> expectedSynthesisRules(
            guidedSynthesis->masteryRules.begin(), guidedSynthesis->masteryRules.end());
        check(
            std::includes(
                rulesBeforeCatchUp.begin(), rulesBeforeCatchUp.end(),
                expectedSynthesisRules.begin(), expectedSynthesisRules.end()),
            "the seven optional drills prove every rule that the bypassed BT17 synthesis would award");

        bool everySingleSkipRequiresCatchUp = true;
        for (int index = 0; index < synthesisIndex; ++index)
        {
            if (!missions[static_cast<std::size_t>(index)].optionalRehearsal)
            {
                continue;
            }
            bayou::client::StoryProgress oneSkipped = allDrillsCompleted;
            oneSkipped.completedByPlay[static_cast<std::size_t>(index)] = false;
            everySingleSkipRequiresCatchUp = everySingleSkipRequiresCatchUp &&
                !bayou::client::storyMasteryCatchUpMayBeSkipped(
                    missions, synthesisIndex, oneSkipped);
        }
        check(
            everySingleSkipRequiresCatchUp,
            "BT17 remains required when any one of the seven earlier optional rehearsals was skipped");

        bayou::client::StoryProgress insufficientAdvance = allDrillsCompleted;
        insufficientAdvance.advancedCount = 0;
        bayou::client::StoryProgress truncatedEvidence = allDrillsCompleted;
        truncatedEvidence.completedByPlay.clear();
        check(
            !bayou::client::storyMasteryCatchUpMayBeSkipped(
                missions, synthesisIndex, insufficientAdvance) &&
                !bayou::client::storyMasteryCatchUpMayBeSkipped(
                    missions, synthesisIndex, truncatedEvidence) &&
                !bayou::client::storyMasteryCatchUpMayBeSkipped(
                    missions, -1, allDrillsCompleted) &&
                !bayou::client::storyMasteryCatchUpMayBeSkipped(
                    missions, 0, allDrillsCompleted),
            "BT17 bypass eligibility fails closed for insufficient progress, truncated evidence, invalid indices, and non-catch-up missions");

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
        const bool briefingExplainsAutomation = std::any_of(
            guidedSynthesis->briefing.begin(), guidedSynthesis->briefing.end(),
            [](const StoryPanel& panel) {
                return panel.text.find("26 printed actions and abilities") !=
                        std::string_view::npos &&
                    panel.text.find("resolve automatically") !=
                        std::string_view::npos &&
                    panel.text.find("ordinary turn engine") !=
                        std::string_view::npos;
            });
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
            everyAutomaticBeatKeepsItsMessage && sectionPopupCount == 2,
            "BT17 automation preserves every timing message and both authored section popups");
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
            !fieldJudgment->optionalRehearsal &&
            fieldJudgment->objectiveSpec.kind == StoryObjectiveKind::DefeatAllEnemies &&
            ownerCount(*fieldJudgment, 1) == 4 &&
            ownerCount(*fieldJudgment, 2) == 4 &&
            fieldJudgment->requiredSurvivorRoles.size() == 1 &&
            requiresRole(*fieldJudgment, "thaeron") &&
            fieldJudgment->hint.find("focus + I") != std::string_view::npos &&
            bayou::client::storyAiSearchDepth(
                StoryCampaign::Blackthorn, fieldJudgment->id) == 2,
        "Blackthorn bridges optional drills with a clearly labeled required open four-versus-four depth-two exam");
    check(
        openMastery && openMastery->script.empty() && openMastery->optionalRehearsal &&
            openMastery->standardMatch && openMastery->pieces.empty() &&
            openMastery->objectiveSpec.kind == StoryObjectiveKind::DefeatAllEnemies &&
            openMastery->playerDeck.size() == 22 &&
            openMastery->enemyDeck.size() == 22 &&
            openMastery->briefing.size() == 10 &&
            std::any_of(
                openMastery->briefing.begin(), openMastery->briefing.end(),
                [](const StoryPanel& panel) {
                    return panel.speaker == "Clock and increments";
                }) &&
            std::any_of(
                openMastery->briefing.begin(), openMastery->briefing.end(),
                [](const StoryPanel& panel) {
                    return panel.speaker == "Timeouts and leaving";
                }) &&
            std::any_of(
                openMastery->briefing.begin(), openMastery->briefing.end(),
                [](const StoryPanel& panel) {
                    return panel.speaker == "Hero placement" &&
                        panel.text.find("hold a Hero card") != std::string_view::npos &&
                        panel.text.find("drag it to an empty highlighted home square") !=
                            std::string_view::npos &&
                        panel.text.find("focus a Hero and press Enter") !=
                            std::string_view::npos &&
                        panel.text.find("press Enter again") != std::string_view::npos;
                }) &&
            openMastery->briefing.back().speaker == "Choose before clocks start" &&
            openMastery->briefing.back().text.find("Before starting") !=
                std::string_view::npos &&
            openMastery->briefing.back().text.find("mouse placement means hold") !=
                std::string_view::npos &&
            openMastery->briefing.back().text.find("Keyboard placement means focus") !=
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
        "Blackthorn offers a skippable legal full-match depth-three exam after its required open bridge");
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

    replayAndCheck(StoryCampaign::Seelie, "se03_road_small_lights", 0x53453033u,
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
        "mw01_river_teeth", "mw02_gilded_hold",
        "mw03_town_under_company", "mw04_watcher_protects", "s01_hospitality",
        "mw05_watched_office", "mw06_cost_seen", "s02_no_one_alone",
        "mw07_twenty_debtors", "mw08_lesson_night", "mw09_invitations",
        "mw10_beautiful_plan", "mw11_no_plan_saves_all", "s03_published_mystery",
        "s04_wounds_that_vote", "mw13_making_credit",
        "mw14_public_lie", "mw15_title_follows_burden", "mw16_gossiping_trees",
        "mw17_factory_heaven", "s05_memory_contradicts", "mw18_allies_dishonestly",
        "mw19_road_keeps_one", "mw20_monster_rules", "mw21_four_losses",
        "mw22_clean_shot", "mw23_choice_not_cure", "mw24_agent_not_heir",
        "mw25_deed_own_hand", "s06_town_owns_itself"};
    const std::vector<std::string_view> blackthornOrder = {
        "bt01_harness_hunger", "bt02_customs_bell",
        "bt03_sanctuary_debt", "bt04_terms_conditions", "s01_hospitality",
        "bt05_freight_office", "bt06_receipt_book", "s02_no_one_alone",
        "bt07_debtor_prison", "bt08_north_lock", "bt09_published_mystery",
        "s03_published_mystery", "bt10_stolen_road",
        "bt11_public_lie", "bt12_break_charter", "bt13a_poisoned_root_road",
        "bt13_feyward_transit",
        "s05_memory_contradicts", "bt14_move_boundary", "bt15_monster_rules",
        "bt17_natural_order", "bt17a_field_judgment", "bt17b_open_mastery",
        "bt16_clean_shot", "bt17c_victor_reckoning",
        "bt18_deed_own_hand",
        "s06_town_owns_itself"};
    const std::vector<std::string_view> seelieOrder = {
        "se00_what_mirror_remembers", "se01_cathedral_last_lights", "se02_broken_bridge",
        "se03_road_small_lights", "se04a_lash_reads_the_road",
        "se04_reading_room", "se05_court_behind_curtain",
        "se06_honey_without_hunger",
        "se07a_queens_price_scene", "se07_queens_price",
        "se08a_sella_receives_herself", "se08_nine_lives_one_choice",
        "se09_dusk_crown", "se10a_caltheriel_walks_out",
        "se10_no_crown_holds_light", "se11a_blackthorn_unmade",
        "se11b_city_refuses", "se11_price_of_an_army",
        "se12_first_boats_burn", "se13b_cut_sylvara_chooses",
        "se13_pearl_and_root", "se14_rules_under_pressure",
        "se15_bell_and_order", "se16_army_that_can_resign",
        "se17_warm_channel", "se18a_complete_is_a_label",
        "se18c_before_next_dawn", "se18d_kindness_without_debt",
        "se18_what_complete_means",
        "se19_crypt_under_order", "se20_harness_anyone_can_leave",
        "se21a_cinderworks_choice", "se21_below_last_stair", "se22a_map_eater",
        "se22b_city_spends_at_night", "se22c_orchard_memory", "se23_controlled_fall",
        "se24_first_prison_design", "se25_three_turns_fish_rib",
        "se25a_abandoned_receiver", "se26a_hunger_has_no_face",
        "se26b_burning_root",
        "se26_vacancy_that_holds", "se27_emperor_at_tree",
        "se28a_two_men_one_vacancy", "se28b_seven_corrections",
        "se28c_a_vow_can_end", "se28d_an_unfinished_record",
        "se28_one_vacancy_seven_hands",
        "se29a_baalzapub_ascendant", "se29b_fizzlewick_says_no",
        "se29c_all_authority_returns", "se29c2_separate_withdrawals",
        "se29e_hold_the_witness_rail", "se29d_reed_ends_the_house",
        "se29_no_complete_bearer",
        "se30_names_we_keep"};

    validateAuthoritativeCardBoundary();
    validateGuidedActionProfileSelection();
    validateStoryAssets();
    validateDefeatAllEnemyIdentity();
    validateSeelieDifficultyCurve();
    validateExpandedMirewatchBattles();
    validateBlackthornTrainingContract();
    validateCampaign(StoryCampaign::Mirewatch, 30, 12, mirewatchOrder);
    validateCampaign(StoryCampaign::Blackthorn, 27, 10, blackthornOrder);
    validateCampaign(StoryCampaign::Seelie, 57, 15, seelieOrder);
    validateCumulativeMasteryEvidence();
    check(
        bayou::client::storyActName(StoryCampaign::Blackthorn, 24) ==
                "ACT VI - THE WOUNDED TREE" &&
            bayou::client::storyActName(StoryCampaign::Blackthorn, 25) ==
                "ACT VII - A TOWN'S OWN HAND" &&
            bayou::client::storyActGoal(StoryCampaign::Blackthorn, 24).find(
                "Victor's command system") != std::string_view::npos &&
            bayou::client::storyActGoal(StoryCampaign::Blackthorn, 25).find(
                "inheritance offer") != std::string_view::npos,
        "Blackthorn keeps Victor's reckoning in the wounded-tree act and begins the civic coda with Thaeron's deed");
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
    {
        const std::vector<std::pair<StoryCampaign, std::string_view>> dramaticScenes = {
            {StoryCampaign::Mirewatch, "mw02_gilded_hold"},
            {StoryCampaign::Mirewatch, "mw07_twenty_debtors"},
            {StoryCampaign::Mirewatch, "mw08_lesson_night"},
            {StoryCampaign::Mirewatch, "mw09_invitations"},
            {StoryCampaign::Mirewatch, "mw14_public_lie"},
            {StoryCampaign::Mirewatch, "mw16_gossiping_trees"},
            {StoryCampaign::Mirewatch, "mw17_factory_heaven"},
            {StoryCampaign::Mirewatch, "mw19_road_keeps_one"},
            {StoryCampaign::Mirewatch, "mw20_monster_rules"},
            {StoryCampaign::Mirewatch, "mw21_four_losses"},
            {StoryCampaign::Mirewatch, "mw22_clean_shot"},
            {StoryCampaign::Mirewatch, "mw23_choice_not_cure"},
            {StoryCampaign::Mirewatch, "mw25_deed_own_hand"},
            {StoryCampaign::Blackthorn, "bt07_debtor_prison"},
            {StoryCampaign::Blackthorn, "bt08_north_lock"},
            {StoryCampaign::Blackthorn, "bt09_published_mystery"},
            {StoryCampaign::Blackthorn, "bt10_stolen_road"},
            {StoryCampaign::Blackthorn, "bt11_public_lie"},
            {StoryCampaign::Blackthorn, "bt12_break_charter"},
            {StoryCampaign::Blackthorn, "bt13a_poisoned_root_road"},
            {StoryCampaign::Blackthorn, "bt13_feyward_transit"},
            {StoryCampaign::Blackthorn, "bt14_move_boundary"},
            {StoryCampaign::Blackthorn, "bt16_clean_shot"},
            {StoryCampaign::Blackthorn, "bt17c_victor_reckoning"},
            {StoryCampaign::Blackthorn, "bt18_deed_own_hand"}};
        for (const auto& [campaign, id] : dramaticScenes)
        {
            const StoryMission* scene = missionById(campaign, id);
            const bool characterLed = scene &&
                scene->objectiveSpec.kind == StoryObjectiveKind::StoryOnly &&
                scene->briefing.size() == 5 &&
                std::none_of(
                    scene->briefing.begin(), scene->briefing.end(),
                    [](const StoryPanel& panel) { return panel.speaker == "Chronicle"; }) &&
                std::all_of(
                    scene->briefing.begin(), scene->briefing.end(),
                    [](const StoryPanel& panel) { return panel.text.size() <= 230; });
            check(
                characterLed,
                std::string(id) +
                    ": Book One connective scene is a focused five-panel dramatic sequence");
        }
    }
    {
        const StoryMission* charterDay =
            missionById(StoryCampaign::Mirewatch, "mw15_title_follows_burden");
        const StoryMission* feywardRoad =
            missionById(StoryCampaign::Mirewatch, "mw16_gossiping_trees");
        std::string charterTransition;
        std::string roadOpening;
        if (charterDay)
        {
            for (const StoryPanel& panel : charterDay->briefing)
            {
                charterTransition += ' ';
                charterTransition += panel.text;
            }
            for (const StoryPanel& panel : charterDay->aftermath)
            {
                charterTransition += ' ';
                charterTransition += panel.text;
            }
        }
        if (feywardRoad)
        {
            for (const StoryPanel& panel : feywardRoad->briefing)
            {
                roadOpening += ' ';
                roadOpening += panel.text;
            }
        }
        check(
            charterDay && feywardRoad && charterDay->aftermath.size() == 3 &&
                charterDay->aftermath[1].speaker == "Narrator" &&
                charterDay->aftermath[1].artPath == "cards/braunStonefist.png" &&
                charterTransition.find("magistrate voids Blackthorn's charter") !=
                    std::string::npos &&
                charterTransition.find("pre-staged charges") != std::string::npos &&
                charterTransition.find("fire through the children's lane") !=
                    std::string::npos &&
                charterTransition.find("Grask kills him") != std::string::npos &&
                charterTransition.find("Mog lowers his weapon, surrenders") !=
                    std::string::npos &&
                charterTransition.find("three harvest cases") != std::string::npos &&
                charterTransition.find("abducted measurement crew") !=
                    std::string::npos &&
                charterTransition.find("municipal writ") != std::string::npos &&
                charterTransition.find("sends Reed's party after them") !=
                    std::string::npos &&
                roadOpening.find("Victor's road poisons the bayou") !=
                    std::string::npos,
            "Mirewatch Charter Day uses Braun's portrait without making him narrate his own death, then establishes the bombing, refusals, abduction, writ, and pursuit before the Feyward road");
    }
    check(
        bayou::client::storyActName(StoryCampaign::Seelie, 0) ==
                "ACT I - THE STOLEN ROAD" &&
            bayou::client::storyActName(StoryCampaign::Seelie, 14) ==
                "ACT I - THE STOLEN ROAD" &&
            bayou::client::storyActName(StoryCampaign::Seelie, 15) ==
                "ACT II - THE SHAPE OF A SIEGE" &&
            bayou::client::storyActName(StoryCampaign::Seelie, 24) ==
                "ACT II - THE SHAPE OF A SIEGE" &&
            bayou::client::storyActName(StoryCampaign::Seelie, 25) ==
                "ACT III - THE COMPLETE BEARER" &&
            bayou::client::storyActName(StoryCampaign::Seelie, 30) ==
                "ACT III - THE COMPLETE BEARER" &&
            bayou::client::storyActName(StoryCampaign::Seelie, 31) ==
                "ACT IV - WHAT THE ROADS COST" &&
            bayou::client::storyActName(StoryCampaign::Seelie, 36) ==
                "ACT IV - WHAT THE ROADS COST" &&
            bayou::client::storyActName(StoryCampaign::Seelie, 37) ==
                "ACT V - THE EMPTY-PLACE METHOD" &&
            bayou::client::storyActName(StoryCampaign::Seelie, 43) ==
                "ACT V - THE EMPTY-PLACE METHOD" &&
            bayou::client::storyActName(StoryCampaign::Seelie, 44) ==
                "ACT VI - THE MEN WHO CANNOT RELEASE" &&
            bayou::client::storyActName(StoryCampaign::Seelie, 56) ==
                "ACT VI - THE MEN WHO CANNOT RELEASE",
        "Seelie act labels cover all 57 entries at the intended boundaries");
    {
        const auto seelie = storyMissions(StoryCampaign::Seelie);
        int storySceneCount = 0;
        bool storyPanelCountsFocused = true;
        for (const StoryMission& mission : seelie)
        {
            if (mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly)
            {
                ++storySceneCount;
                storyPanelCountsFocused = storyPanelCountsFocused &&
                    mission.briefing.size() >= 3 && mission.briefing.size() <= 6;
            }
            if (mission.sourceChapter.find("Non-canon") != std::string_view::npos ||
                mission.sourceChapter.find("non-canon") != std::string_view::npos)
            {
                check(
                    mission.id == "se14_rules_under_pressure"
                        ? !mission.optionalRehearsal
                        : mission.optionalRehearsal,
                    std::string(mission.id) +
                        ": non-canon Seelie battles are skippable except the required open territory bridge");
            }
        }
        check(
            storySceneCount == 42 && storyPanelCountsFocused,
            "Seelie uses 42 focused StoryOnly scenes of three to six panels around fifteen tactical chapters");
        int actSixStoryScenes = 0;
        int actSixTacticalMissions = 0;
        std::size_t actSixStoryPanels = 0;
        for (std::size_t index = 44; index < seelie.size(); ++index)
        {
            if (seelie[index].objectiveSpec.kind == StoryObjectiveKind::StoryOnly)
            {
                ++actSixStoryScenes;
                actSixStoryPanels += seelie[index].briefing.size();
            }
            else
            {
                ++actSixTacticalMissions;
            }
        }
        check(
            seelie.size() == 57 && actSixStoryScenes == 11 &&
                actSixTacticalMissions == 2 && actSixStoryPanels == 58,
            "Seelie Act VI is thirteen escalating entries: eleven focused story scenes and two hard battles");
        const StoryMission* finalRecap =
            missionById(StoryCampaign::Seelie, "se28a_two_men_one_vacancy");
        const StoryMission* corrections =
            missionById(StoryCampaign::Seelie, "se28b_seven_corrections");
        std::string recapCopy;
        if (finalRecap)
        {
            for (const StoryPanel& panel : finalRecap->briefing)
            {
                recapCopy += ' ';
                recapCopy += panel.text;
            }
        }
        std::string correctionCopy;
        if (corrections)
        {
            for (const StoryPanel& panel : corrections->briefing)
            {
                correctionCopy += ' ';
                correctionCopy += panel.text;
            }
        }
        check(
            finalRecap && recapCopy.find("COMPLETE") != std::string::npos &&
                recapCopy.find("Recall") != std::string::npos &&
                recapCopy.find("empty place") != std::string::npos &&
                corrections && correctionCopy.find("black concordance glass") !=
                    std::string::npos &&
                correctionCopy.find("false agreement cannot last") != std::string::npos &&
                correctionCopy.find("can still raise him") != std::string::npos,
            "Act VI defines COMPLETE, the first Recall, the empty place, and why removing Lash's concordance does not stop the ascent");
        const StoryMission* vow =
            missionById(StoryCampaign::Seelie, "se28c_a_vow_can_end");
        const StoryMission* openRecord =
            missionById(StoryCampaign::Seelie, "se28d_an_unfinished_record");
        check(
            vow && openRecord && vow->briefing.size() == 4 &&
                openRecord->briefing.size() == 5 &&
                std::none_of(
                    vow->briefing.begin(), vow->briefing.end(),
                    [](const StoryPanel& panel) {
                        return panel.text.find("Pallid") != std::string_view::npos;
                    }) &&
                std::none_of(
                    openRecord->briefing.begin(), openRecord->briefing.end(),
                    [](const StoryPanel& panel) {
                        return panel.text.find("Silkmaw") != std::string_view::npos;
                    }),
            "Silkmaw's vow and Pallid's unfinished record are separate focused scenes");
        const StoryMission* ascent =
            missionById(StoryCampaign::Seelie, "se29a_baalzapub_ascendant");
        const StoryMission* workers =
            missionById(StoryCampaign::Seelie, "se29b_fizzlewick_says_no");
        check(
            ascent && workers && ascent->briefing.size() == 6 &&
                workers->briefing.size() == 6 &&
                std::any_of(
                    ascent->briefing.begin(), ascent->briefing.end(),
                    [](const StoryPanel& panel) {
                        return panel.text.find("Ruvan") != std::string_view::npos;
                    }) &&
                std::any_of(
                    ascent->briefing.begin(), ascent->briefing.end(),
                    [](const StoryPanel& panel) {
                        return panel.text.find("Ebbin") != std::string_view::npos;
                    }) &&
                std::any_of(
                    ascent->briefing.begin(), ascent->briefing.end(),
                    [](const StoryPanel& panel) { return panel.speaker == "Liora Kest"; }) &&
                std::any_of(
                    workers->briefing.begin(), workers->briefing.end(),
                    [](const StoryPanel& panel) {
                        return panel.speaker == "Nessa Bale - NIGHT STEWARD";
                    }),
            "Baalzapub's ascent and the workers' refusal are separate scenes with Ruvan, Liora, and Nessa acting in voice");
        const StoryMission* emperorAtTree =
            missionById(StoryCampaign::Seelie, "se27_emperor_at_tree");
        check(
            emperorAtTree &&
                std::any_of(
                    emperorAtTree->aftermath.begin(), emperorAtTree->aftermath.end(),
                    [](const StoryPanel& panel) {
                        return panel.text.find("Jalen") != std::string_view::npos;
                    }) &&
                std::any_of(
                    emperorAtTree->aftermath.begin(), emperorAtTree->aftermath.end(),
                    [](const StoryPanel& panel) {
                        return panel.text.find("Recall") != std::string_view::npos;
                    }),
            "the capstone aftermath carries the merged Whitewash defection and Recall consequences");
        const auto lastTactical = std::find_if(
            seelie.rbegin(), seelie.rend(), [](const StoryMission& mission) {
                return mission.objectiveSpec.kind != StoryObjectiveKind::StoryOnly;
            });
        check(
            seelie.size() == 57 && lastTactical != seelie.rend() &&
                lastTactical->id == "se29e_hold_the_witness_rail" &&
                seelie[53].id == "se29e_hold_the_witness_rail" &&
                seelie[54].id == "se29d_reed_ends_the_house" &&
                seelie[55].id == "se29_no_complete_bearer" &&
                seelie[56].id == "se30_names_we_keep",
            "the witness-rail control objective is the last legal battle before the final three causal story scenes");
        for (std::string_view id : {
                 "se04_reading_room", "se07a_queens_price_scene",
                 "se07_queens_price",
                 "se10_no_crown_holds_light", "se13_pearl_and_root"})
        {
            const StoryMission* drill = missionById(StoryCampaign::Seelie, id);
            check(
                drill && drill->optionalRehearsal &&
                    drill->objectiveSpec.kind != StoryObjectiveKind::StoryOnly,
                std::string(id) + ": non-canon rules rehearsal is explicitly optional");
        }
        const StoryMission* territoryBridge =
            missionById(StoryCampaign::Seelie, "se14_rules_under_pressure");
        check(
            territoryBridge && !territoryBridge->optionalRehearsal &&
                territoryBridge->objectiveSpec.kind == StoryObjectiveKind::ControlSquares &&
                territoryBridge->objectiveSpec.amount == 22,
            "Seelie requires a multi-command intermediate territory objective before the 28-square final capstone");
    }
    check(
        bayou::client::storyActGoal(StoryCampaign::Seelie, 0).find("Caltheriel") !=
                std::string_view::npos &&
            bayou::client::storyActGoal(StoryCampaign::Seelie, 15).find("Emberhaven") !=
                std::string_view::npos &&
            bayou::client::storyActGoal(StoryCampaign::Seelie, 25).find("COMPLETE") !=
                std::string_view::npos &&
            bayou::client::storyActGoal(StoryCampaign::Seelie, 31).find("truth") !=
                std::string_view::npos &&
            bayou::client::storyActGoal(StoryCampaign::Seelie, 37).find("empty") !=
                std::string_view::npos &&
            bayou::client::storyActGoal(StoryCampaign::Seelie, 44).find("separately") !=
                std::string_view::npos,
        "every Seelie act exposes a plain-language immediate goal");
    check(
            bayou::client::storyActShortGoal(StoryCampaign::Seelie, 0) ==
                "Ask what Caltheriel wants." &&
            bayou::client::storyActShortGoal(StoryCampaign::Seelie, 15) ==
                "Keep Emberhaven's exits open." &&
            bayou::client::storyActShortGoal(StoryCampaign::Seelie, 44) ==
                "Let every voice withdraw separately." &&
            !bayou::client::storyActShortGoal(StoryCampaign::Mirewatch, 0).empty() &&
            !bayou::client::storyActShortGoal(StoryCampaign::Blackthorn, 0).empty(),
        "every in-battle campaign header exposes a compact readable act purpose");
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
    validateWorldTreeTargetObjective();
    validateRiverTeethLineClear();
    validateDependentCards();

    fmt::println("{} Story Mode test failure(s).", failures);
    return failures == 0 ? 0 : 1;
}
