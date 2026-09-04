#include "../client/client_story.hpp"
#include "../client/client_story_cards.hpp"
#include "../gameserver/game_engine.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace
{

using bayou::client::StoryActionKind;
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
        }
    };

    for (StoryCampaign campaign : {StoryCampaign::Mirewatch, StoryCampaign::Blackthorn})
    {
        for (const StoryMission& mission : bayou::client::storyMissions(campaign))
        {
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
        if (mission.objectiveSpec.kind != StoryObjectiveKind::StoryOnly)
        {
            check(!mission.pieces.empty(), "tactical mission has an authored setup");
            check(!mission.script.empty(), "tactical mission has a deterministic script");
            check(!mission.masteryCards.empty(), "tactical mission names mastered cards");
            check(!mission.masteryRules.empty(), "tactical mission names mastered rules");
        }
        else
        {
            check(mission.script.empty(), "story beat has no fake tactical script");
            check(mission.masteryCards.empty(), "story beat awards no card mastery");
            check(mission.masteryRules.empty(), "story beat awards no rule mastery");
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

    for (StoryCampaign campaign : {StoryCampaign::Mirewatch, StoryCampaign::Blackthorn})
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
                if (step.kind == StoryActionKind::Move || step.kind == StoryActionKind::Attack)
                {
                    check(
                        step.instruction.find("Click ") == std::string_view::npos &&
                            step.instruction.find("Select ") == std::string_view::npos,
                        std::string(mission.id) +
                            ": move and attack instructions use the client's drag control");
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
    check(river && scent && auction && terms && capstone, "reviewed teaching-copy missions exist");
    if (!river || !scent || !auction || !terms || !capstone)
    {
        return;
    }

    const bool briefingDefinesProgress = std::any_of(
        river->briefing.begin(), river->briefing.end(), [](const StoryPanel& panel) {
            return panel.text.find("Guided Actions count completed game actions") != std::string_view::npos &&
                panel.text.find("not mouse motions") != std::string_view::npos &&
                panel.text.find("restriction applies only to this tutorial") != std::string_view::npos &&
                panel.text.find("drag the ACT piece onto its TARGET") != std::string_view::npos;
        });
    check(
        briefingDefinesProgress,
        "opening briefing distinguishes guided actions, drag controls, and tutorial-only limits");
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
        reed && reed->instruction.find("Drag ACT: Reed") != std::string_view::npos &&
            reed->instruction.find("TARGET: C6") != std::string_view::npos &&
            reed->instruction.find("normal teal move marker") != std::string_view::npos &&
            reed->instruction.find("printed \"Step\" action moves one square") != std::string_view::npos,
        "River Teeth gives an exact actor, destination, and normal movement marker");

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
            return panel.text.find("Before player control begins") != std::string_view::npos &&
                panel.text.find("not a playable rule") != std::string_view::npos;
        });
    check(
        firstScent == nullptr && scentIsPanelOnly,
        "Blackthorn scent work is story context, never a fake player action");

    const StoryScriptAction* bladeDance =
        actionByHeading(*auction, "VANYA - BLADE DANCE");
    check(
        bladeDance &&
            bladeDance->instruction.find("printed Blade Dance") != std::string_view::npos &&
            bladeDance->instruction.find("Repeat 1") != std::string_view::npos &&
            bladeDance->instruction.find("or Pass") != std::string_view::npos &&
            bladeDance->instruction.find("toward Nima") != std::string_view::npos &&
            bladeDance->instruction.find("before another piece can act") !=
                std::string_view::npos,
        "Blade Dance lesson explains repeat count, escape, action lock, and story purpose");

    const StoryScriptAction* collision =
        actionByHeading(*terms, "HIDDEN COLLISION TRIGGERS AMBUSH");
    check(
        collision && collision->targetRow == 5 && collision->targetColumn == 2 &&
            collision->instruction.find("printed Ambush") != std::string_view::npos &&
            collision->instruction.find("takes 1 damage") != std::string_view::npos &&
            collision->instruction.find("Ambusher also materializes") != std::string_view::npos,
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

}

bool replayScript(const StoryMission& mission, GameEngine& engine)
{
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
             "Blackthorn Lumberjack", "Sapling", "Swamp Tracker Unmounted"})
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

    for (const auto& step : mission.script)
    {
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
        bool accepted = false;
        switch (step.kind)
        {
        case StoryActionKind::Move:
            accepted = engine.movePiece(step.owner, actorId, targetRow, targetColumn);
            break;
        case StoryActionKind::Attack:
            accepted = engine.attackPiece(step.owner, actorId, targetRow, targetColumn);
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
                            piece.row == targetRow && piece.column == targetColumn;
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
    }
    return true;
}

void validateAdvancedOutcomes()
{
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
            check(pieceCount(engine, "Blackthorn Debt Collector") == 0 &&
                    pieceCount(engine, "Bog Spearman") == 0,
                "BT17 defeats both Relentless targets and the Capture target");
            check(
                pieceCount(engine, "Blackthorn Lumberjack") == 1 &&
                    hasPieceAt(engine, "Blackthorn Lumberjack", 6, 1),
                "BT17 Commanded Foreman summons one Lumberjack into player 1's front square B7");
            check(pieceCount(engine, "Sapling") == 1, "BT17 Grove Sister leaves one Trail Sapling");
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
                erevan != nullptr && !erevan->hidden && erevan->disabledTurns > 0 &&
                    erevan->health == 1,
                "BT04 attacking collision damages, reveals, and disables hidden Erevan");
            check(
                collisionAmbusher != engine.boardPieces().end() &&
                    !collisionAmbusher->hidden && collisionAmbusher->actionState == 0 &&
                    collisionAmbusher->hasActed,
                "BT04 collision re-resolves as Ambush, materializes the attacker, and stages it at B6");
        });
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
    const std::vector<std::string_view> mirewatchOrder = {
        "mw01_river_teeth", "mw02_gilded_hold",
        "mw03_town_under_company", "mw04_watcher_protects", "s01_hospitality",
        "mw05_watched_office", "mw06_cost_seen", "s02_no_one_alone",
        "mw07_twenty_debtors", "mw08_lesson_night", "mw09_invitations",
        "mw10_beautiful_plan", "mw11_no_plan_saves_all", "s03_published_mystery",
        "mw12_road_reaches", "s04_wounds_that_vote", "mw13_making_credit",
        "mw14_public_lie", "mw15_title_follows_burden", "mw16_gossiping_trees",
        "mw17_factory_heaven", "s05_memory_contradicts", "mw18_allies_dishonestly",
        "mw19_road_keeps_one", "mw20_monster_rules", "mw21_four_losses",
        "mw22_clean_shot", "mw23_choice_not_cure", "mw24_agent_not_heir",
        "s06_town_owns_itself"};
    const std::vector<std::string_view> blackthornOrder = {
        "bt01_harness_hunger", "bt02_customs_bell",
        "bt03_sanctuary_debt", "bt04_terms_conditions", "s01_hospitality",
        "bt05_freight_office", "bt06_receipt_book", "s02_no_one_alone",
        "bt07_debtor_prison", "bt08_north_lock", "bt09_published_mystery",
        "s03_published_mystery", "bt10_stolen_road", "s04_wounds_that_vote",
        "bt11_public_lie", "bt12_break_charter", "bt13_feyward_transit",
        "s05_memory_contradicts", "bt14_move_boundary", "bt15_monster_rules",
        "bt16_clean_shot", "bt17_natural_order", "s06_town_owns_itself"};

    validateStoryAssets();
    validateCampaign(StoryCampaign::Mirewatch, 30, 8, mirewatchOrder);
    validateCampaign(StoryCampaign::Blackthorn, 23, 7, blackthornOrder);
    check(
        !storyMissions(StoryCampaign::Mirewatch).empty() &&
            storyMissions(StoryCampaign::Mirewatch).front().id == "mw01_river_teeth",
        "Mirewatch begins immediately with the River Teeth gator attack");
    check(
        missionById(StoryCampaign::Mirewatch, "s00_wrong_hand") == nullptr &&
            missionById(StoryCampaign::Blackthorn, "s00_wrong_hand") == nullptr,
        "the theatre prologue is absent from both playable campaign paths");

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
    validateNoFabricatedGameplay();
    validateTeachingCopy();

    for (StoryCampaign campaign : {StoryCampaign::Mirewatch, StoryCampaign::Blackthorn})
    {
        for (const StoryMission& mission : bayou::client::storyMissions(campaign))
        {
            if (mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly)
            {
                continue;
            }
            GameEngine engine(
                0x53544f52u + static_cast<unsigned int>(mission.id.size()), {});
            check(
                replayScript(mission, engine),
                std::string("golden replay is legal: ") + std::string(mission.id));
        }
    }

    const StoryMission* mirewatch =
        missionById(StoryCampaign::Mirewatch, "mw01_river_teeth");
    const StoryMission* blackthorn =
        missionById(StoryCampaign::Blackthorn, "bt01_harness_hunger");
    check(mirewatch != nullptr, "River Teeth exists");
    check(blackthorn != nullptr, "Harness the Hunger exists");

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
    validateRiverTeethLineClear();
    validateDependentCards();

    fmt::println("{} Story Mode test failure(s).", failures);
    return failures == 0 ? 0 : 1;
}
