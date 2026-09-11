#include "../client/client_story.hpp"
#include "../client/client_story_cards.hpp"
#include "../gameserver/ai_player.hpp"
#include "../gameserver/game_engine.hpp"
#include "../shared/card_source_config.hpp"
#include "../shared/network.hpp"
#include "../shared/socket_timeout.hpp"
#include "../shared/tls_socket.hpp"
#include "../storytest/story_step_outcomes.hpp"

#include <SFML/Network.hpp>
#include <fmt/core.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

using bayou::client::StoryActionKind;
using bayou::client::StoryCampaign;
using bayou::client::StoryCardResolutionMode;
using bayou::client::StoryMission;
using bayou::client::StoryObjectiveKind;
using bayou::client::StoryPiecePlacement;

#ifndef BAYOU_SOURCE_DIR
#define BAYOU_SOURCE_DIR "."
#endif

constexpr auto RequestTimeout = std::chrono::seconds(8);

bool configureDefaultCa(std::string& error)
{
    if (const char* configured = std::getenv("BAYOU_TLS_CA_FILE");
        configured != nullptr && *configured != '\0')
    {
        return true;
    }
    const std::filesystem::path caPath =
        std::filesystem::path(BAYOU_SOURCE_DIR) / "deploy" / "ca" /
        "isrg-root-x1.pem";
    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(caPath, filesystemError))
    {
        error = "live Story gate could not find its pinned CA bundle: " +
            caPath.string();
        return false;
    }
#ifdef _WIN32
    if (_putenv_s("BAYOU_TLS_CA_FILE", caPath.string().c_str()) != 0)
#else
    if (setenv("BAYOU_TLS_CA_FILE", caPath.string().c_str(), 0) != 0)
#endif
    {
        error = "live Story gate could not configure its pinned CA bundle";
        return false;
    }
    return true;
}

struct LiveCatalog
{
    std::string endpoint;
    std::vector<card_data::Card> cards;
};

std::string campaignLabel(StoryCampaign campaign)
{
    return std::string(bayou::client::storyCampaignName(campaign));
}

std::string joined(const std::vector<std::string>& values)
{
    std::string result;
    for (const std::string& value : values)
    {
        if (!result.empty())
        {
            result += ", ";
        }
        result += value;
    }
    return result;
}

std::optional<LiveCatalog> loadSchemaElevenCatalog(
    const std::filesystem::path& configPath,
    std::string& error)
{
    const std::optional<card_source_config::Config> config =
        card_source_config::load(configPath, error);
    if (!config)
    {
        return std::nullopt;
    }
    if (!config->usesCardServer())
    {
        error = "live Story gate requires card_server in " + configPath.string() +
            "; a local cards_database is not authoritative live data";
        return std::nullopt;
    }

    const std::optional<sf::IpAddress> address =
        sf::IpAddress::resolve(config->cardServerHost);
    if (!address)
    {
        error = "could not resolve authoritative card server " +
            config->cardServerHost;
        return std::nullopt;
    }

    bayou::tls::Socket socket;
    socket.setServerName(config->cardServerHost);
    if (socket.connect(*address, config->cardServerPort) !=
        sf::Socket::Status::Done)
    {
        error = "could not connect to authoritative card server " +
            config->cardServerHost + ":" + std::to_string(config->cardServerPort);
        return std::nullopt;
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(network::MessageType::CardListRequest);
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        error = "could not send authoritative card-list request";
        return std::nullopt;
    }

    sf::Packet response;
    if (socket_timeout::receivePacket(socket, response, RequestTimeout) !=
        sf::Socket::Status::Done)
    {
        error = "timed out waiting for authoritative card-list response";
        return std::nullopt;
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    response >> responseType >> success >> message;
    if (!response ||
        static_cast<network::MessageType>(responseType) !=
            network::MessageType::CardListResponse)
    {
        error = "authoritative card server returned an unexpected response";
        return std::nullopt;
    }
    if (!success)
    {
        error = message.empty()
            ? "authoritative card server rejected the card-list request"
            : "authoritative card server rejected the card-list request: " + message;
        return std::nullopt;
    }

    // Read the header explicitly instead of using the backward-compatible
    // production decoder. This gate is meant to catch a deployment that has
    // not reached the exact schema consumed by the current game rules.
    std::uint32_t marker = 0;
    std::uint32_t version = 0;
    std::uint32_t count = 0;
    response >> marker >> version >> count;
    if (!response || marker != card_data::CardListSchemaMarker)
    {
        error = "authoritative card server returned a legacy/unversioned catalog; "
            "Story Mode requires schema " +
            std::to_string(card_data::CardListSchemaVersion);
        return std::nullopt;
    }
    if (version != card_data::CardListSchemaVersion)
    {
        error = "authoritative card server schema mismatch: expected " +
            std::to_string(card_data::CardListSchemaVersion) + ", received " +
            std::to_string(version);
        return std::nullopt;
    }
    if (version != 11)
    {
        error = "storylivecatalogtest itself is stale: it must be reviewed for schema 11";
        return std::nullopt;
    }
    if (count == 0 || count > card_data::MaxSerializedItems)
    {
        error = "authoritative schema-11 catalog declared invalid card count " +
            std::to_string(count);
        return std::nullopt;
    }

    LiveCatalog catalog;
    catalog.endpoint = config->cardServerHost + ":" +
        std::to_string(config->cardServerPort);
    catalog.cards.reserve(count);
    std::set<std::string> titles;
    for (std::uint32_t index = 0; index < count; ++index)
    {
        card_data::Card card;
        if (!card_data::readListedCard(
                response,
                card,
                false,
                true,
                true,
                true,
                true,
                true))
        {
            error = "authoritative card server returned an invalid schema-11 card at index " +
                std::to_string(index);
            return std::nullopt;
        }
        if (card.title.empty())
        {
            error = "authoritative card server returned an untitled card at index " +
                std::to_string(index);
            return std::nullopt;
        }
        if (!titles.insert(card.title).second)
        {
            error = "authoritative card server returned duplicate title: " + card.title;
            return std::nullopt;
        }
        catalog.cards.push_back(std::move(card));
    }
    if (!response.endOfPacket())
    {
        error = "authoritative schema-11 catalog contains unexpected trailing data";
        return std::nullopt;
    }
    return catalog;
}

std::optional<game_data::GameCard> resolveLiveCard(
    std::string_view title,
    std::span<const card_data::Card> liveCards)
{
    return bayou::client::resolveStoryCardDefinition(
        title,
        liveCards,
        std::span<const card_data::Card>{},
        StoryCardResolutionMode::AuthoritativeOnly);
}

std::vector<std::string> gameCardMismatchFields(
    const game_data::GameCard& packaged,
    const game_data::GameCard& live)
{
    std::vector<std::string> mismatches;
    const auto compare = [&]<typename T>(
                             std::string_view field,
                             const T& packagedValue,
                             const T& liveValue) {
        if (packagedValue != liveValue)
        {
            mismatches.emplace_back(field);
        }
    };

    compare("title", packaged.title, live.title);
    compare("type", packaged.type, live.type);
    compare("traits", packaged.traits, live.traits);
    compare("keywords", packaged.keywords, live.keywords);
    compare("imagePath", packaged.imagePath, live.imagePath);
    compare("tokenPath", packaged.tokenPath, live.tokenPath);
    compare("state1TokenPath", packaged.state1TokenPath, live.state1TokenPath);
    compare("pieceBaseBluePath", packaged.pieceBaseBluePath, live.pieceBaseBluePath);
    compare("pieceBaseRedPath", packaged.pieceBaseRedPath, live.pieceBaseRedPath);
    compare("walkAnimPath", packaged.walkAnimPath, live.walkAnimPath);
    compare("idleAnimPath", packaged.idleAnimPath, live.idleAnimPath);
    compare("attackAnimPath", packaged.attackAnimPath, live.attackAnimPath);
    compare("damagedAnimPath", packaged.damagedAnimPath, live.damagedAnimPath);
    compare("killedAnimPath", packaged.killedAnimPath, live.killedAnimPath);
    compare("fidgetAnimPath", packaged.fidgetAnimPath, live.fidgetAnimPath);
    compare("walkAnimFrames", packaged.walkAnimFrames, live.walkAnimFrames);
    compare("idleAnimFrames", packaged.idleAnimFrames, live.idleAnimFrames);
    compare("attackAnimFrames", packaged.attackAnimFrames, live.attackAnimFrames);
    compare("damagedAnimFrames", packaged.damagedAnimFrames, live.damagedAnimFrames);
    compare("killedAnimFrames", packaged.killedAnimFrames, live.killedAnimFrames);
    compare("fidgetAnimFrames", packaged.fidgetAnimFrames, live.fidgetAnimFrames);
    compare("cost", packaged.cost, live.cost);
    compare("heroCost", packaged.heroCost, live.heroCost);
    compare("health", packaged.health, live.health);
    compare("width", packaged.width, live.width);
    compare("height", packaged.height, live.height);
    compare("attack", packaged.attack, live.attack);
    compare("attackRange", packaged.attackRange, live.attackRange);
    compare("movePattern", packaged.movePattern, live.movePattern);
    compare("moveRange", packaged.moveRange, live.moveRange);
    compare("attackingMove", packaged.attackingMove, live.attackingMove);
    compare("effect", packaged.effect, live.effect);
    compare("target", packaged.target, live.target);
    compare("power", packaged.power, live.power);
    compare("tax", packaged.tax, live.tax);
    compare("gatherResources", packaged.gatherResources, live.gatherResources);
    compare("healingAura", packaged.healingAura, live.healingAura);
    compare("canControl", packaged.canControl, live.canControl);
    compare("growTurns", packaged.growTurns, live.growTurns);
    compare("ability", packaged.ability, live.ability);
    compare("summonTitle", packaged.summonTitle, live.summonTitle);
    compare("rebirthTitle", packaged.rebirthTitle, live.rebirthTitle);
    compare("abilityLabels", packaged.abilityLabels, live.abilityLabels);
    compare("abilityUses", packaged.abilityUses, live.abilityUses);
    if (packaged.actions.size() != live.actions.size())
    {
        mismatches.emplace_back("actions.size");
    }
    const std::size_t actionCount =
        std::min(packaged.actions.size(), live.actions.size());
    for (std::size_t index = 0; index < actionCount; ++index)
    {
        const game_data::ActionProfile& packagedAction = packaged.actions[index];
        const game_data::ActionProfile& liveAction = live.actions[index];
        const std::string prefix = "actions[" + std::to_string(index) + "].";
        const auto compareAction = [&]<typename T>(
                                       std::string_view field,
                                       const T& packagedValue,
                                       const T& liveValue) {
            compare(prefix + std::string(field), packagedValue, liveValue);
        };
        compareAction("name", packagedAction.name, liveAction.name);
        compareAction("kind", packagedAction.kind, liveAction.kind);
        compareAction("pattern", packagedAction.pattern, liveAction.pattern);
        compareAction("state", packagedAction.state, liveAction.state);
        compareAction(
            "nextState",
            game_data::actionNextState(packagedAction),
            game_data::actionNextState(liveAction));
        compareAction("minRange", packagedAction.minRange, liveAction.minRange);
        compareAction("maxRange", packagedAction.maxRange, liveAction.maxRange);
        compareAction("damage", packagedAction.damage, liveAction.damage);
        compareAction("heal", packagedAction.heal, liveAction.heal);
        compareAction("statusTurns", packagedAction.statusTurns, liveAction.statusTurns);
        compareAction("cooldownTurns", packagedAction.cooldownTurns, liveAction.cooldownTurns);
        compareAction("control", packagedAction.control, liveAction.control);
        compareAction("repeat", packagedAction.repeat, liveAction.repeat);
        compareAction("canMove", packagedAction.canMove, liveAction.canMove);
        compareAction("canAttack", packagedAction.canAttack, liveAction.canAttack);
        compareAction("passThrough", packagedAction.passThrough, liveAction.passThrough);
        compareAction("lineOfSight", packagedAction.lineOfSight, liveAction.lineOfSight);
        compareAction("push", packagedAction.push, liveAction.push);
        compareAction("targetFilter", packagedAction.targetFilter, liveAction.targetFilter);
        compareAction("infest", packagedAction.infest, liveAction.infest);
        compareAction("pull", packagedAction.pull, liveAction.pull);
    }
    return mismatches;
}

bool validateAuthoritativeConstructHealing(
    std::span<const card_data::Card> liveCards,
    std::size_t& checkedActions,
    std::string& error)
{
    struct ExpectedHealingAction
    {
        std::string_view cardTitle;
        std::string_view actionName;
    };
    constexpr std::array<ExpectedHealingAction, 3> Expected = {{
        {"Fizzlewick Gearwright", "Repair"},
        {"Gearjaw", "Rebuild"},
        {"Lumber Automaton", "Maintenance"},
    }};

    const std::optional<game_data::GameCard> mechanicalCard =
        resolveLiveCard("Gearjaw", liveCards);
    const std::optional<game_data::GameCard> nonMechanicalCard =
        resolveLiveCard("Bog Spearman", liveCards);
    if (!mechanicalCard || !nonMechanicalCard)
    {
        error = "construct-healing regression requires authoritative Gearjaw and Bog Spearman cards";
        return false;
    }
    if (!game_data::hasKeyword(mechanicalCard->traits, "mechanical") ||
        game_data::hasKeyword(nonMechanicalCard->traits, "mechanical") ||
        game_data::hasKeyword(nonMechanicalCard->keywords, "mechanical"))
    {
        error = "construct-healing regression target taxonomy drifted in the authoritative catalog";
        return false;
    }

    if (mechanicalCard->health <= 1 || nonMechanicalCard->health <= 1)
    {
        error = "construct-healing regression requires woundable authoritative targets";
        return false;
    }
    for (const ExpectedHealingAction& expected : Expected)
    {
        const std::optional<game_data::GameCard> healerCard =
            resolveLiveCard(expected.cardTitle, liveCards);
        if (!healerCard)
        {
            error = "authoritative construct healer is missing: " +
                std::string(expected.cardTitle);
            return false;
        }
        const auto action = std::find_if(
            healerCard->actions.begin(),
            healerCard->actions.end(),
            [&](const game_data::ActionProfile& candidate) {
                return candidate.name == expected.actionName;
            });
        if (action == healerCard->actions.end())
        {
            error = "authoritative " + std::string(expected.cardTitle) +
                " is missing displayed action " + std::string(expected.actionName);
            return false;
        }
        if (action->heal <= 0 || action->targetFilter != std::vector<std::string>{"construct"})
        {
            error = "authoritative " + std::string(expected.cardTitle) + " / " +
                std::string(expected.actionName) +
                " no longer maps to positive healing with the schema-11 construct filter";
            return false;
        }
        const int actionIndex = static_cast<int>(
            std::distance(healerCard->actions.begin(), action));

        const int mechanicalInitialHealth = mechanicalCard->health - 1;
        GameEngine healingEngine(
            0x4845414cu + static_cast<unsigned int>(checkedActions), {});
        if (!healingEngine.loadScenario(
                {{1, *healerCard, 3, 3, healerCard->type == "Hero"},
                 {1, *mechanicalCard, 3, 4, mechanicalCard->type == "Hero",
                  mechanicalInitialHealth}},
                {},
                {},
                0,
                0,
                1,
                "construct-healing authoritative regression",
                false))
        {
            error = "authoritative construct-healing regression could not load its Mechanical setup";
            return false;
        }
        const auto healerPiece = std::find_if(
            healingEngine.boardPieces().begin(),
            healingEngine.boardPieces().end(),
            [&](const game_data::Piece& piece) {
                return piece.row == 3 && piece.column == 3 &&
                    piece.name == expected.cardTitle;
            });
        const auto mechanicalTarget = std::find_if(
            healingEngine.boardPieces().begin(),
            healingEngine.boardPieces().end(),
            [&](const game_data::Piece& piece) {
                return piece.row == 3 && piece.column == 4 &&
                    piece.name == mechanicalCard->title;
            });
        if (healerPiece == healingEngine.boardPieces().end() ||
            mechanicalTarget == healingEngine.boardPieces().end())
        {
            error = "authoritative construct-healing regression lost a seeded piece";
            return false;
        }
        const int healerId = healerPiece->id;
        const int mechanicalTargetId = mechanicalTarget->id;
        const int healthBefore = mechanicalTarget->health;
        if (!healingEngine.attackPiece(
                1, healerId, mechanicalTarget->row, mechanicalTarget->column, actionIndex))
        {
            error = "authoritative " + std::string(expected.cardTitle) + " / " +
                std::string(expected.actionName) +
                " cannot legally target a wounded Mechanical card";
            return false;
        }
        const auto healedTarget = std::find_if(
            healingEngine.boardPieces().begin(),
            healingEngine.boardPieces().end(),
            [&](const game_data::Piece& piece) { return piece.id == mechanicalTargetId; });
        if (healedTarget == healingEngine.boardPieces().end() ||
            healedTarget->health != std::min(
                healedTarget->maxHealth, healthBefore + action->heal))
        {
            error = "authoritative " + std::string(expected.cardTitle) + " / " +
                std::string(expected.actionName) +
                " was accepted but did not restore the printed health";
            return false;
        }

        game_data::GameCard labelledNonMechanical = *nonMechanicalCard;
        labelledNonMechanical.keywords.push_back("Construct");
        GameEngine rejectionEngine(
            0x52454a45u + static_cast<unsigned int>(checkedActions), {});
        if (!rejectionEngine.loadScenario(
                {{1, *healerCard, 3, 3, healerCard->type == "Hero"},
                 {1, labelledNonMechanical, 3, 4,
                  labelledNonMechanical.type == "Hero",
                  labelledNonMechanical.health - 1}},
                {},
                {},
                0,
                0,
                1,
                "construct-healing rejection regression",
                false))
        {
            error = "authoritative construct-healing regression could not load its rejection setup";
            return false;
        }
        const auto rejectingHealer = std::find_if(
            rejectionEngine.boardPieces().begin(),
            rejectionEngine.boardPieces().end(),
            [&](const game_data::Piece& piece) {
                return piece.row == 3 && piece.column == 3;
            });
        const auto rejectedTarget = std::find_if(
            rejectionEngine.boardPieces().begin(),
            rejectionEngine.boardPieces().end(),
            [&](const game_data::Piece& piece) {
                return piece.row == 3 && piece.column == 4;
            });
        if (rejectingHealer == rejectionEngine.boardPieces().end() ||
            rejectedTarget == rejectionEngine.boardPieces().end())
        {
            error = "authoritative construct-healing rejection setup lost a seeded piece";
            return false;
        }
        const int rejectedHealthBefore = rejectedTarget->health;
        const int rejectedTargetId = rejectedTarget->id;
        if (rejectionEngine.attackPiece(
                1,
                rejectingHealer->id,
                rejectedTarget->row,
                rejectedTarget->column,
                actionIndex))
        {
            error = "authoritative " + std::string(expected.cardTitle) + " / " +
                std::string(expected.actionName) +
                " illegally accepts a non-Mechanical target";
            return false;
        }
        const auto stillRejected = std::find_if(
            rejectionEngine.boardPieces().begin(),
            rejectionEngine.boardPieces().end(),
            [&](const game_data::Piece& piece) { return piece.id == rejectedTargetId; });
        if (stillRejected == rejectionEngine.boardPieces().end() ||
            stillRejected->health != rejectedHealthBefore)
        {
            error = "rejected non-Mechanical construct-healing target was mutated";
            return false;
        }
        ++checkedActions;
    }
    return true;
}

bool validateAuthoritativeBodyguardTargeting(
    const std::vector<card_data::Card>& liveCards,
    std::size_t& checkedActions,
    std::string& error)
{
    const std::optional<game_data::GameCard> bodyguardCard =
        resolveLiveCard("Starbloom Knight", liveCards);
    const std::optional<game_data::GameCard> targetCard =
        resolveLiveCard("Warden", liveCards);
    const std::optional<game_data::GameCard> disableCard =
        resolveLiveCard("Duchess Dewbell", liveCards);
    const std::optional<game_data::GameCard> statusOnlyCard =
        resolveLiveCard("Widowroot", liveCards);
    const std::optional<game_data::GameCard> pullCard =
        resolveLiveCard("Bog Spearman", liveCards);
    const std::optional<game_data::GameCard> infestCard =
        resolveLiveCard("Queen Nyxara", liveCards);
    const std::optional<game_data::GameCard> pushCard =
        resolveLiveCard("Erevan the Shadow", liveCards);
    const std::optional<game_data::GameCard> controlCard =
        resolveLiveCard("Pavo Quickstep", liveCards);
    const std::optional<game_data::GameCard> infestationUnit =
        resolveLiveCard("Bristlejack", liveCards);
    if (!bodyguardCard || !targetCard || !disableCard || !statusOnlyCard || !pullCard ||
        !infestCard || !pushCard || !controlCard || !infestationUnit)
    {
        error = "Bodyguard targeting regression requires the authoritative Starbloom Knight, "
            "Warden, Duchess Dewbell, Widowroot, Bog Spearman, Queen Nyxara, Erevan, Pavo, and Bristlejack cards";
        return false;
    }
    if (!game_data::hasKeyword(bodyguardCard->keywords, "bodyguard") ||
        targetCard->type != "Unit" || infestationUnit->type != "Unit")
    {
        error = "authoritative Bodyguard targeting card taxonomy drifted";
        return false;
    }

    const auto actionIndexNamed = [](
                                      const game_data::GameCard& card,
                                      std::string_view name) {
        const auto found = std::find_if(
            card.actions.begin(),
            card.actions.end(),
            [&](const game_data::ActionProfile& action) {
                return action.name == name;
            });
        return found == card.actions.end()
            ? -1
            : static_cast<int>(std::distance(card.actions.begin(), found));
    };
    const auto pieceNamed = [](
                                const GameEngine& engine,
                                std::string_view name) -> const game_data::Piece* {
        const auto found = std::find_if(
            engine.boardPieces().begin(),
            engine.boardPieces().end(),
            [&](const game_data::Piece& piece) { return piece.name == name; });
        return found == engine.boardPieces().end() ? nullptr : &*found;
    };

    const int disableAction = actionIndexNamed(*disableCard, "Dewbell's Decree");
    const int statusOnlyAction = actionIndexNamed(*statusOnlyCard, "Widow's Bind");
    const int pullAction = actionIndexNamed(*pullCard, "Hooked Spear");
    const int infestAction = actionIndexNamed(*infestCard, "Nyxaran Infest");
    const int pushAction = actionIndexNamed(*pushCard, "Hidden Shove");
    const int controlAction = actionIndexNamed(*controlCard, "Enchanting Pipes");
    if (disableAction < 0 || statusOnlyAction < 0 || pullAction < 0 || infestAction < 0 ||
        pushAction < 0 || controlAction < 0)
    {
        error = "an authoritative action required by the Bodyguard targeting matrix is missing";
        return false;
    }
    const game_data::ActionProfile& disableProfile =
        disableCard->actions[static_cast<std::size_t>(disableAction)];
    const game_data::ActionProfile& statusOnlyProfile =
        statusOnlyCard->actions[static_cast<std::size_t>(statusOnlyAction)];
    const game_data::ActionProfile& pullProfile =
        pullCard->actions[static_cast<std::size_t>(pullAction)];
    const game_data::ActionProfile& infestProfile =
        infestCard->actions[static_cast<std::size_t>(infestAction)];
    const game_data::ActionProfile& pushProfile =
        pushCard->actions[static_cast<std::size_t>(pushAction)];
    const game_data::ActionProfile& controlProfile =
        controlCard->actions[static_cast<std::size_t>(controlAction)];
    if (disableProfile.damage <= 0 || disableProfile.statusTurns <= 1 ||
        statusOnlyProfile.damage != 0 || statusOnlyProfile.statusTurns <= 1 ||
        pullProfile.damage <= 0 || !pullProfile.pull ||
        infestProfile.damage <= 0 || infestProfile.infest != infestationUnit->title ||
        pushProfile.damage != 0 || pushProfile.push <= 0 || pushProfile.state == 0 ||
        controlProfile.damage != 0 || controlProfile.control <= 0)
    {
        error = "authoritative Bodyguard interaction profiles no longer expose the expected "
            "damage/Disable, zero-damage Disable, damage/Pull, damage/Infest, zero-damage Push, and zero-damage Control cases";
        return false;
    }

    {
        GameEngine engine(0x42474441u, liveCards);
        if (!engine.loadScenario(
                {{1, *disableCard, 3, 1, disableCard->type == "Hero"},
                 {2, *targetCard, 3, 3, false},
                 {2, *bodyguardCard, 2, 3, false}},
                {}, {}, 0, 0, 1, "authoritative Bodyguard Disable targeting", false))
        {
            error = "could not load the authoritative Bodyguard Disable setup";
            return false;
        }
        const game_data::Piece* attacker = pieceNamed(engine, disableCard->title);
        const game_data::Piece* target = pieceNamed(engine, targetCard->title);
        const game_data::Piece* bodyguard = pieceNamed(engine, bodyguardCard->title);
        if (attacker == nullptr || target == nullptr || bodyguard == nullptr)
        {
            error = "authoritative Bodyguard Disable setup lost a seeded piece";
            return false;
        }
        const int targetId = target->id;
        const int bodyguardId = bodyguard->id;
        const int targetHealth = target->health;
        const int bodyguardHealth = bodyguard->health;
        if (!engine.attackPiece(1, attacker->id, target->row, target->column, disableAction))
        {
            error = "Dewbell's Decree could not legally attack a Bodyguard-protected Warden";
            return false;
        }
        target = pieceNamed(engine, targetCard->title);
        bodyguard = pieceNamed(engine, bodyguardCard->title);
        if (target == nullptr || bodyguard == nullptr || target->id != targetId ||
            bodyguard->id != bodyguardId || target->health != targetHealth ||
            target->disabledTurns != 0 || bodyguard->health != bodyguardHealth - disableProfile.damage ||
            bodyguard->disabledTurns != game_data::disabledTurnsForDamage(
                disableProfile.damage, disableProfile.statusTurns))
        {
            error = "Bodyguard did not take Dewbell's damage and explicit Disable while the selected Warden stayed untouched";
            return false;
        }
        ++checkedActions;
    }

    {
        GameEngine engine(0x42474446u, liveCards);
        if (!engine.loadScenario(
                {{1, *statusOnlyCard, 3, 2, statusOnlyCard->type == "Hero"},
                 {2, *targetCard, 3, 3, false},
                 {2, *bodyguardCard, 2, 3, false}},
                {}, {}, 0, 0, 1, "authoritative zero-damage Bodyguard Disable targeting", false))
        {
            error = "could not load the authoritative zero-damage Bodyguard Disable setup";
            return false;
        }
        const game_data::Piece* attacker = pieceNamed(engine, statusOnlyCard->title);
        const game_data::Piece* target = pieceNamed(engine, targetCard->title);
        const game_data::Piece* bodyguard = pieceNamed(engine, bodyguardCard->title);
        if (attacker == nullptr || target == nullptr || bodyguard == nullptr)
        {
            error = "authoritative zero-damage Bodyguard Disable setup lost a seeded piece";
            return false;
        }
        const int targetHealth = target->health;
        const int bodyguardHealth = bodyguard->health;
        if (!engine.attackPiece(
                1, attacker->id, target->row, target->column, statusOnlyAction))
        {
            error = "Widow's Bind could not legally target a Warden beside Bodyguard";
            return false;
        }
        target = pieceNamed(engine, targetCard->title);
        bodyguard = pieceNamed(engine, bodyguardCard->title);
        if (target == nullptr || bodyguard == nullptr || target->health != targetHealth ||
            target->disabledTurns != statusOnlyProfile.statusTurns ||
            bodyguard->health != bodyguardHealth || bodyguard->disabledTurns != 0)
        {
            error = "zero-damage Widow's Bind did not Disable only the selected Warden beside Bodyguard";
            return false;
        }
        ++checkedActions;
    }

    {
        GameEngine engine(0x42474442u, liveCards);
        if (!engine.loadScenario(
                {{1, *pullCard, 1, 1, pullCard->type == "Hero"},
                 {2, *targetCard, 3, 3, false},
                 {2, *bodyguardCard, 3, 4, false}},
                {}, {}, 0, 0, 1, "authoritative Bodyguard Pull targeting", false))
        {
            error = "could not load the authoritative Bodyguard Pull setup";
            return false;
        }
        const game_data::Piece* attacker = pieceNamed(engine, pullCard->title);
        const game_data::Piece* target = pieceNamed(engine, targetCard->title);
        const game_data::Piece* bodyguard = pieceNamed(engine, bodyguardCard->title);
        if (attacker == nullptr || target == nullptr || bodyguard == nullptr)
        {
            error = "authoritative Bodyguard Pull setup lost a seeded piece";
            return false;
        }
        const int targetHealth = target->health;
        const int bodyguardHealth = bodyguard->health;
        if (!engine.attackPiece(1, attacker->id, target->row, target->column, pullAction))
        {
            error = "Hooked Spear could not legally attack a Bodyguard-protected Warden";
            return false;
        }
        target = pieceNamed(engine, targetCard->title);
        bodyguard = pieceNamed(engine, bodyguardCard->title);
        if (target == nullptr || bodyguard == nullptr || target->health != targetHealth ||
            target->disabledTurns != 0 || target->row != 2 || target->column != 2 ||
            bodyguard->health != bodyguardHealth - pullProfile.damage ||
            bodyguard->disabledTurns != game_data::disabledTurnsForDamage(
                pullProfile.damage, pullProfile.statusTurns) ||
            bodyguard->row != 3 || bodyguard->column != 4)
        {
            error = "Bodyguard did not take Hooked Spear damage while Pull moved only the selected Warden";
            return false;
        }
        ++checkedActions;
    }

    {
        GameEngine engine(0x42474443u, liveCards);
        if (!engine.loadScenario(
                {{1, *infestCard, 1, 1, infestCard->type == "Hero"},
                 {2, *targetCard, 3, 3, false},
                 {2, *bodyguardCard, 3, 4, false}},
                {}, {}, 0, 0, 1, "authoritative Bodyguard Infest targeting", false))
        {
            error = "could not load the authoritative Bodyguard Infest setup";
            return false;
        }
        const game_data::Piece* attacker = pieceNamed(engine, infestCard->title);
        const game_data::Piece* target = pieceNamed(engine, targetCard->title);
        const game_data::Piece* bodyguard = pieceNamed(engine, bodyguardCard->title);
        if (attacker == nullptr || target == nullptr || bodyguard == nullptr)
        {
            error = "authoritative Bodyguard Infest setup lost a seeded piece";
            return false;
        }
        const int targetHealth = target->health;
        const int bodyguardHealth = bodyguard->health;
        if (!engine.attackPiece(1, attacker->id, target->row, target->column, infestAction))
        {
            error = "Nyxaran Infest could not legally attack a Bodyguard-protected Warden";
            return false;
        }
        target = pieceNamed(engine, targetCard->title);
        bodyguard = pieceNamed(engine, bodyguardCard->title);
        if (target == nullptr || bodyguard == nullptr || target->health != targetHealth ||
            target->disabledTurns != 0 || target->infestationTitle != infestationUnit->title ||
            target->infestationOwner != 1 || bodyguard->health != bodyguardHealth - infestProfile.damage ||
            bodyguard->infestationTitle.size() != 0)
        {
            error = "Bodyguard did not take Nyxaran Infest damage while Infest marked only the selected Warden";
            return false;
        }
        ++checkedActions;
    }

    {
        GameEngine engine(0x42474444u, liveCards);
        if (!engine.loadScenario(
                {{1, *pushCard, 3, 2, pushCard->type == "Hero"},
                 {2, *targetCard, 3, 3, false},
                 {2, *bodyguardCard, 2, 3, false}},
                {}, {}, 0, 0, 1, "authoritative zero-damage Bodyguard Push targeting", false))
        {
            error = "could not load the authoritative Bodyguard Push setup";
            return false;
        }
        const game_data::Piece* attacker = pieceNamed(engine, pushCard->title);
        const game_data::Piece* target = pieceNamed(engine, targetCard->title);
        const game_data::Piece* bodyguard = pieceNamed(engine, bodyguardCard->title);
        if (attacker == nullptr || target == nullptr || bodyguard == nullptr)
        {
            error = "authoritative Bodyguard Push setup lost a seeded piece";
            return false;
        }
        const int attackerId = attacker->id;
        const int targetHealth = target->health;
        const int bodyguardHealth = bodyguard->health;
        if (!engine.useAbility(1, attackerId) || !engine.endTurn(1) ||
            !engine.endTurn(2))
        {
            error = "Erevan could not legally Dematerialize and return to a ready hidden turn";
            return false;
        }
        attacker = pieceNamed(engine, pushCard->title);
        target = pieceNamed(engine, targetCard->title);
        if (attacker == nullptr || target == nullptr || !attacker->hidden ||
            !engine.attackPiece(1, attacker->id, target->row, target->column, pushAction))
        {
            error = "Erevan's Hidden Shove could not legally target a Warden beside Bodyguard";
            return false;
        }
        target = pieceNamed(engine, targetCard->title);
        bodyguard = pieceNamed(engine, bodyguardCard->title);
        if (target == nullptr || bodyguard == nullptr || target->health != targetHealth ||
            target->disabledTurns != 0 || target->row != 3 || target->column != 4 ||
            bodyguard->health != bodyguardHealth || bodyguard->disabledTurns != 0 ||
            bodyguard->row != 2 || bodyguard->column != 3)
        {
            error = "zero-damage Hidden Shove did not Push only the selected Warden past an adjacent Bodyguard";
            return false;
        }
        ++checkedActions;
    }

    {
        GameEngine engine(0x42474445u, liveCards);
        if (!engine.loadScenario(
                {{1, *controlCard, 3, 1, controlCard->type == "Hero"},
                 {2, *targetCard, 3, 3, false},
                 {2, *bodyguardCard, 2, 3, false}},
                {}, {}, 0, 0, 1, "authoritative zero-damage Bodyguard Control targeting", false))
        {
            error = "could not load the authoritative Bodyguard Control setup";
            return false;
        }
        const game_data::Piece* attacker = pieceNamed(engine, controlCard->title);
        const game_data::Piece* target = pieceNamed(engine, targetCard->title);
        const game_data::Piece* bodyguard = pieceNamed(engine, bodyguardCard->title);
        if (attacker == nullptr || target == nullptr || bodyguard == nullptr)
        {
            error = "authoritative Bodyguard Control setup lost a seeded piece";
            return false;
        }
        const int targetHealth = target->health;
        const int bodyguardHealth = bodyguard->health;
        if (!engine.attackPiece(1, attacker->id, target->row, target->column, controlAction))
        {
            error = "Enchanting Pipes could not legally target a Warden beside Bodyguard";
            return false;
        }
        target = pieceNamed(engine, targetCard->title);
        bodyguard = pieceNamed(engine, bodyguardCard->title);
        if (target == nullptr || bodyguard == nullptr || target->health != targetHealth ||
            target->disabledTurns != 0 || target->owner != 1 || target->originalOwner != 2 ||
            target->controlTurnsRemaining != controlProfile.control ||
            bodyguard->health != bodyguardHealth || bodyguard->disabledTurns != 0 ||
            bodyguard->owner != 2)
        {
            error = "zero-damage Enchanting Pipes did not Control only the selected Warden beside Bodyguard";
            return false;
        }
        ++checkedActions;
    }

    return true;
}

bool configureObjective(
    const StoryMission& mission,
    const std::unordered_map<std::string_view, int>& roleIds,
    GameEngine& engine,
    std::string& error)
{
    const auto roleId = [&](std::string_view role) -> int {
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
        objective.kind = GameEngine::ScenarioObjectiveKind::None;
        break;
    case StoryObjectiveKind::StoryOnly:
        error = "StoryOnly mission reached tactical objective configuration";
        return false;
    }
    for (std::string_view role : mission.requiredSurvivorRoles)
    {
        const int id = roleId(role);
        if (id == 0)
        {
            error = "required survivor role is absent from live setup: " +
                std::string(role);
            return false;
        }
        objective.requiredSurvivorPieceIds.push_back(id);
    }
    if (!engine.configureScenarioObjective(std::move(objective)))
    {
        error = "engine rejected the objective against the live-card setup";
        return false;
    }
    return true;
}

struct MissionRun
{
    bool ok = false;
    bool open = false;
    bool witnessAttempted = false;
    bool witnessProven = false;
    std::size_t dependencies = 0;
    std::size_t claimChecks = 0;
    int configuredOpponentActions = 0;
    int configuredOpponentTurns = 0;
    std::string error;
};

bool exerciseConfiguredOpponent(
    GameEngine engine,
    StoryCampaign campaign,
    const StoryMission& mission,
    MissionRun& result)
{
    engine.setNarrationEnabled(false);
    constexpr int RequiredOpponentTurns = 2;
    constexpr int MaxExerciseCommands = 192;
    const int configuredDepth =
        bayou::client::storyAiSearchDepth(campaign, mission.id);

    int commands = 0;
    while (commands < MaxExerciseCommands &&
           engine.phase() == game_data::Phase::Playing &&
           result.configuredOpponentTurns < RequiredOpponentTurns)
    {
        ++commands;
        const int activePlayer = engine.currentPlayer();
        bool accepted = false;
        if (engine.hasPendingForesightChoice(activePlayer))
        {
            accepted = engine.chooseForesightCard(
                activePlayer, chooseAiForesightCard(engine, activePlayer));
        }
        else if (activePlayer == 1)
        {
            // Keep the player side deterministic and passive. This run is not
            // a win witness; it isolates whether the configured Story opponent
            // can repeatedly plan and apply legal commands in this exact setup.
            accepted = engine.endTurn(1);
        }
        else
        {
            const AiAction action = chooseAiAction(engine, 2, configuredDepth);
            accepted = applyAiAction(engine, 2, action);
            ++result.configuredOpponentActions;
            if (action.kind == AiActionKind::EndTurn)
            {
                ++result.configuredOpponentTurns;
            }
        }

        if (!accepted)
        {
            result.error = fmt::format(
                "configured-depth {} opponent produced an engine-illegal command "
                "after {} accepted opponent actions",
                configuredDepth,
                result.configuredOpponentActions);
            return false;
        }
    }

    if (result.configuredOpponentActions < 2 ||
        (result.configuredOpponentTurns < RequiredOpponentTurns &&
         engine.phase() == game_data::Phase::Playing))
    {
        result.error = fmt::format(
            "configured-depth {} opponent exercise was inconclusive after {} commands "
            "({} opponent actions, {} completed opponent turns, phase {})",
            configuredDepth,
            MaxExerciseCommands,
            result.configuredOpponentActions,
            result.configuredOpponentTurns,
            static_cast<int>(engine.phase()));
        return false;
    }
    return true;
}

MissionRun runMission(
    StoryCampaign campaign,
    const StoryMission& mission,
    std::span<const card_data::Card> liveCards,
    unsigned int seed)
{
    MissionRun result;
    result.open = mission.script.empty();

    const bayou::client::StoryCardDependencyReport dependencies =
        bayou::client::validateStoryCardDependencies(
            mission,
            liveCards,
            std::span<const card_data::Card>{},
            StoryCardResolutionMode::AuthoritativeOnly);
    result.dependencies = dependencies.requiredTitles.size();
    if (!dependencies.complete())
    {
        result.error = "missing live card dependencies: " +
            joined(dependencies.missingTitles);
        return result;
    }

    GameEngine engine(seed, {});
    for (const std::string& title : dependencies.requiredTitles)
    {
        const std::optional<game_data::GameCard> card =
            resolveLiveCard(title, liveCards);
        if (!card)
        {
            result.error = "dependency closure resolved inconsistently: " + title;
            return result;
        }
        engine.registerScenarioCard(*card);
    }

    if (mission.standardMatch)
    {
        const auto resolveDeck = [&](const std::vector<std::string_view>& titles,
                                     std::string_view owner)
            -> std::optional<std::vector<card_data::Card>> {
            std::vector<card_data::Card> deck;
            deck.reserve(titles.size());
            for (std::string_view title : titles)
            {
                const auto found = std::find_if(
                    liveCards.begin(), liveCards.end(),
                    [&](const card_data::Card& card) { return card.title == title; });
                if (found == liveCards.end())
                {
                    result.error = std::string(owner) +
                        " standard deck card vanished after dependency validation: " +
                        std::string(title);
                    return std::nullopt;
                }
                deck.push_back(*found);
            }
            return deck;
        };
        const auto playerDeck = resolveDeck(mission.playerDeck, "player");
        const auto enemyDeck = resolveDeck(mission.enemyDeck, "opponent");
        if (!playerDeck || !enemyDeck)
        {
            return result;
        }
        if (const std::optional<std::string> error =
                game_data::deckRulesError(*playerDeck))
        {
            result.error = "live player standard deck is illegal: " + *error;
            return result;
        }
        if (const std::optional<std::string> error =
                game_data::deckRulesError(*enemyDeck))
        {
            result.error = "live opponent standard deck is illegal: " + *error;
            return result;
        }

        engine.enableTimers();
        engine.submitDeck(1, *playerDeck);
        engine.submitDeck(2, *enemyDeck);
        if (!engine.bothDecksSubmitted() ||
            engine.phase() != game_data::Phase::HeroPlacement ||
            engine.playerState(1).heroesToPlace.size() != 2 ||
            engine.playerState(2).heroesToPlace.size() != 2 ||
            engine.playerState(1).drawPile.size() != 20 ||
            engine.playerState(2).drawPile.size() != 20)
        {
            result.error =
                "legal live decks did not enter ordinary two-Hero placement with 20-card draw piles";
            return result;
        }
        {
            GameEngine placementDeadlineProbe = engine;
            placementDeadlineProbe.setNarrationEnabled(false);
            placementDeadlineProbe.updateTimers(30 * 1000);
            const auto& heroes =
                placementDeadlineProbe.playerState(1).heroesToPlace;
            const auto thaeron = std::find_if(
                heroes.begin(), heroes.end(), [](const game_data::GameCard& card) {
                    return card.title == "Thaeron Baelstone";
                });
            if (thaeron == heroes.end() ||
                !placementDeadlineProbe.placeHero(
                    1,
                    static_cast<int>(std::distance(heroes.begin(), thaeron)),
                    2,
                    0))
            {
                result.error =
                    "placement-deadline probe could not place its first legal Hero";
                return result;
            }
            const game_data::Snapshot afterFirstPlacement =
                placementDeadlineProbe.snapshotFor(1);
            if (afterFirstPlacement.turnRemainingMs !=
                    GameEngine::FullTurnTimerMs - 30 * 1000 ||
                afterFirstPlacement.players[0].clockRemainingMs !=
                    GameEngine::RegularClockMs ||
                placementDeadlineProbe.phase() != game_data::Phase::HeroPlacement ||
                placementDeadlineProbe.currentPlayer() != 1 ||
                !placementDeadlineProbe.updateTimers(
                    GameEngine::FullTurnTimerMs - 30 * 1000) ||
                placementDeadlineProbe.phase() != game_data::Phase::GameOver ||
                placementDeadlineProbe.winner() != 2)
            {
                result.error =
                    "Hero placement no longer uses one non-resetting two-minute "
                    "deadline with paused 15-minute match clocks";
                return result;
            }
        }
        const auto placeNamedHero = [&](int player, std::string_view title,
                                        int row, int column) {
            const auto& heroes = engine.playerState(player).heroesToPlace;
            const auto found = std::find_if(
                heroes.begin(), heroes.end(),
                [&](const game_data::GameCard& card) { return card.title == title; });
            return found != heroes.end() && engine.placeHero(
                player, static_cast<int>(std::distance(heroes.begin(), found)),
                row, column);
        };
        if (placeNamedHero(1, "Thaeron Baelstone", 0, 0) ||
            !placeNamedHero(1, "Thaeron Baelstone", 2, 0) ||
            !placeNamedHero(1, "Ashenfang", 5, 1) ||
            !placeNamedHero(2, "Maggie Mudroot", 3, 6))
        {
            result.error = "ordinary live Hero placement rejected its legal home-zone sequence";
            return result;
        }
        const auto maggie = std::find_if(
            engine.boardPieces().begin(), engine.boardPieces().end(),
            [](const game_data::Piece& piece) {
                return piece.name == "Maggie Mudroot";
            });
        const game_data::Snapshot concealedView = engine.snapshotFor(1);
        if (maggie == engine.boardPieces().end() || !maggie->isHero ||
            maggie->width != 2 || maggie->height != 2 ||
            std::any_of(
                concealedView.pieces.begin(), concealedView.pieces.end(),
                [](const game_data::Piece& piece) { return piece.owner == 2; }) ||
            placeNamedHero(2, "Joni Pumpernickel", 3, 7) ||
            !placeNamedHero(2, "Joni Pumpernickel", 2, 7))
        {
            result.error =
                "live Maggie footprint, placement collision, or concealed-Hero rule drifted";
            return result;
        }
        const game_data::Snapshot openingView = engine.snapshotFor(1);
        if (engine.phase() != game_data::Phase::Playing ||
            engine.currentPlayer() != 1 || !engine.timersAreEnabled() ||
            openingView.turnRemainingMs != GameEngine::FullTurnTimerMs ||
            openingView.hand.size() != 4 ||
            openingView.players[0].handCount != 4 ||
            openingView.players[1].handCount != 4 ||
            openingView.players[0].drawPileCount != 16 ||
            openingView.players[1].drawPileCount != 16 ||
            openingView.players[0].resources !=
                openingView.players[0].controlledSquares ||
            openingView.players[1].resources != 0)
        {
            result.error =
                "ordinary live opening hand, first player, clock, or control income drifted";
            return result;
        }

        {
            GameEngine matchClockProbe = engine;
            matchClockProbe.setNarrationEnabled(false);
            constexpr int MaxClockEvents = 80;
            int events = 0;
            while (matchClockProbe.phase() == game_data::Phase::Playing &&
                   events++ < MaxClockEvents)
            {
                if (matchClockProbe.currentPlayer() == 1)
                {
                    const std::int64_t untilEvent =
                        matchClockProbe.timeUntilNextTimerEventMs();
                    if (untilEvent <= 0 ||
                        !matchClockProbe.updateTimers(untilEvent))
                    {
                        result.error =
                            "ordinary match clock probe failed to advance a timer event";
                        return result;
                    }
                }
                else if (!matchClockProbe.endTurn(2))
                {
                    result.error =
                        "ordinary match clock probe could not pass the opponent turn";
                    return result;
                }
            }
            const game_data::Snapshot expiredClock =
                matchClockProbe.snapshotFor(1);
            if (matchClockProbe.phase() != game_data::Phase::GameOver ||
                matchClockProbe.winner() != 2 ||
                expiredClock.players[0].clockRemainingMs != 0)
            {
                result.error =
                    "ordinary 15-minute match-clock exhaustion no longer awards "
                    "the opponent the game";
                return result;
            }
        }

        if (!exerciseConfiguredOpponent(engine, campaign, mission, result))
        {
            return result;
        }

        // The independent exercise above already proved repeated configured-
        // depth opponent planning. Keep this second run deliberately passive
        // on player 2 so it proves a bounded legal player-win path through
        // ordinary Hero elimination without conflating reachability and balance.
        engine.setNarrationEnabled(false);
        result.witnessAttempted = true;
        bool playerCommandAccepted = false;
        constexpr int MaxStandardWitnessActions = 1024;
        int standardWitnessActions = 0;
        for (; standardWitnessActions < MaxStandardWitnessActions &&
               engine.phase() == game_data::Phase::Playing;
             ++standardWitnessActions)
        {
            const int activePlayer = engine.currentPlayer();
            bool accepted = false;
            if (engine.hasPendingForesightChoice(activePlayer))
            {
                accepted = engine.chooseForesightCard(activePlayer, 0);
            }
            else if (activePlayer == 1)
            {
                const AiAction action = chooseAiAction(engine, 1, 1);
                accepted = applyAiAction(engine, 1, action);
                playerCommandAccepted = playerCommandAccepted || accepted;
            }
            else
            {
                accepted = engine.endTurn(2);
            }
            if (!accepted)
            {
                result.error =
                    "ordinary live-match witness produced an engine-illegal command";
                return result;
            }
        }
        if (!playerCommandAccepted || engine.phase() != game_data::Phase::GameOver ||
            engine.winner() != 1)
        {
            result.error = fmt::format(
                "ordinary live-match Hero-elimination win remains unproven after {} commands "
                "(phase {}, winner {}, player command {})",
                MaxStandardWitnessActions,
                static_cast<int>(engine.phase()),
                engine.winner(),
                playerCommandAccepted);
            return result;
        }
        result.witnessProven = true;
        result.ok = true;
        return result;
    }

    std::vector<GameEngine::ScenarioPiece> scenario;
    std::vector<game_data::GameCard> playerHand;
    std::vector<game_data::GameCard> enemyHand;
    std::vector<game_data::GameCard> playerDrawPile;
    std::vector<game_data::GameCard> enemyDrawPile;
    for (const StoryPiecePlacement& placement : mission.pieces)
    {
        const std::optional<game_data::GameCard> card =
            resolveLiveCard(placement.cardTitle, liveCards);
        if (!card)
        {
            result.error = "board card vanished after dependency validation: " +
                std::string(placement.cardTitle);
            return result;
        }
        scenario.push_back({
            placement.owner,
            *card,
            placement.row,
            placement.column,
            placement.isHero,
            placement.initialHealth});
    }
    const auto resolveList = [&](const std::vector<std::string_view>& titles,
                                 std::vector<game_data::GameCard>& destination,
                                 std::string_view location) {
        for (std::string_view title : titles)
        {
            const std::optional<game_data::GameCard> card =
                resolveLiveCard(title, liveCards);
            if (!card)
            {
                result.error = std::string(location) +
                    " card vanished after dependency validation: " +
                    std::string(title);
                return false;
            }
            destination.push_back(*card);
        }
        return true;
    };
    if (!resolveList(mission.playerHand, playerHand, "player hand") ||
        !resolveList(mission.enemyHand, enemyHand, "enemy hand") ||
        !resolveList(mission.playerDrawPile, playerDrawPile, "player draw pile") ||
        !resolveList(mission.enemyDrawPile, enemyDrawPile, "enemy draw pile"))
    {
        return result;
    }

    if (!engine.loadScenario(
        scenario,
        std::move(playerHand),
        std::move(enemyHand),
        mission.playerResources,
        mission.enemyResources,
        mission.firstPlayer,
        std::string(mission.objective),
        false,
        std::move(playerDrawPile),
        std::move(enemyDrawPile)))
    {
        result.error =
            "engine rejected an off-board, invalid-owner, or overlapping authored footprint";
        return result;
    }

    std::unordered_map<std::string_view, int> roleIds;
    for (const StoryPiecePlacement& placement : mission.pieces)
    {
        const auto found = std::find_if(
            engine.boardPieces().begin(),
            engine.boardPieces().end(),
            [&](const game_data::Piece& piece) {
                return piece.owner == placement.owner &&
                    piece.name == placement.cardTitle &&
                    piece.row == placement.row &&
                    piece.column == placement.column;
            });
        if (found == engine.boardPieces().end())
        {
            result.error = "engine could not place live card for role " +
                std::string(placement.role) + " (" +
                std::string(placement.cardTitle) + ")";
            return result;
        }
        if (!roleIds.emplace(placement.role, found->id).second)
        {
            result.error = "duplicate authored role: " + std::string(placement.role);
            return result;
        }
    }
    if (roleIds.size() != mission.pieces.size())
    {
        result.error = "one or more authored roles failed to map uniquely";
        return result;
    }
    if (!configureObjective(mission, roleIds, engine, result.error))
    {
        return result;
    }

    const auto& setupProgress = engine.scenarioObjectiveProgress();
    if (!setupProgress.configured || !setupProgress.valid ||
        setupProgress.complete || setupProgress.failed)
    {
        result.error = "objective is not a valid, incomplete live objective at setup";
        return result;
    }

    if (mission.id == "se14_rules_under_pressure")
    {
        // The first required open-choice bridge must require an exchange, not
        // collapse into a one-command puzzle whose configured opponent never
        // gets to demonstrate pressure. Exhaust every printed piece action and
        // ability from the live opening position so future card-data changes
        // cannot silently reintroduce an immediate win.
        for (const game_data::Piece& piece : engine.boardPieces())
        {
            if (piece.owner != 1)
            {
                continue;
            }
            for (std::size_t actionIndex = 0;
                 actionIndex < piece.actions.size();
                 ++actionIndex)
            {
                for (int row = 0; row < game_data::BoardSize; ++row)
                {
                    for (int column = 0; column < game_data::BoardSize; ++column)
                    {
                        GameEngine probe = engine;
                        probe.setNarrationEnabled(false);
                        if (probe.movePiece(
                                1,
                                piece.id,
                                row,
                                column,
                                static_cast<int>(actionIndex)) &&
                            (probe.scenarioObjectiveProgress().complete ||
                             probe.winner() == 1))
                        {
                            result.error = fmt::format(
                                "first open Seelie bridge can still finish in one "
                                "command: {} action {} to {},{}",
                                piece.name,
                                actionIndex,
                                row,
                                column);
                            return result;
                        }
                    }
                }
            }
            GameEngine abilityProbe = engine;
            abilityProbe.setNarrationEnabled(false);
            if (abilityProbe.useAbility(1, piece.id) &&
                (abilityProbe.scenarioObjectiveProgress().complete ||
                 abilityProbe.winner() == 1))
            {
                result.error =
                    "first open Seelie bridge can finish with one ability command: " +
                    piece.name;
                return result;
            }
        }
        GameEngine passProbe = engine;
        passProbe.setNarrationEnabled(false);
        if (passProbe.endTurn(1) &&
            (passProbe.scenarioObjectiveProgress().complete ||
             passProbe.winner() == 1))
        {
            result.error =
                "first open Seelie bridge can finish with a single pass";
            return result;
        }
    }

    if (result.open)
    {
        if (mission.objectiveSpec.kind == StoryObjectiveKind::Legacy ||
            mission.objectiveSpec.kind == StoryObjectiveKind::Scripted ||
            mission.objectiveSpec.kind == StoryObjectiveKind::DeployCard)
        {
            result.error = "open mission has no engine-adjudicated completion objective";
            return result;
        }
        const int playerPieces = static_cast<int>(std::count_if(
            engine.boardPieces().begin(),
            engine.boardPieces().end(),
            [](const game_data::Piece& piece) { return piece.owner == 1; }));
        const int enemyPieces = static_cast<int>(std::count_if(
            engine.boardPieces().begin(),
            engine.boardPieces().end(),
            [](const game_data::Piece& piece) { return piece.owner == 2; }));
        if (engine.phase() != game_data::Phase::Playing ||
            playerPieces == 0 || enemyPieces == 0)
        {
            result.error = "open objective does not begin as a live two-sided position";
            return result;
        }

        if (!exerciseConfiguredOpponent(engine, campaign, mission, result))
        {
            return result;
        }

        // The independent exercise above already proved repeated configured-
        // depth opponent planning. This second run keeps player 2 passive so it
        // can answer the separate question: does any bounded legal player path
        // actually reach the authored objective?
        GameEngine witness = engine;
        witness.setNarrationEnabled(false);
        result.witnessAttempted = true;
        constexpr int MaxWitnessActions = 320;
        int witnessActions = 0;
        std::array<int, 7> playerActionCounts{};
        std::array<int, 7> opponentActionCounts{};
        for (; witnessActions < MaxWitnessActions &&
               witness.phase() == game_data::Phase::Playing;
             ++witnessActions)
        {
            const int activePlayer = witness.currentPlayer();
            bool accepted = false;
            if (activePlayer == 1)
            {
                const AiAction action = chooseAiAction(witness, 1, 1);
                ++playerActionCounts[static_cast<std::size_t>(action.kind)];
                accepted = applyAiAction(witness, 1, action);
            }
            else
            {
                if (witness.hasPendingForesightChoice(2))
                {
                    accepted = witness.chooseForesightCard(2, 0);
                }
                else
                {
                    ++opponentActionCounts[static_cast<std::size_t>(AiActionKind::EndTurn)];
                    accepted = witness.endTurn(2);
                }
            }
            if (!accepted)
            {
                result.error = "open-objective witness produced an engine-illegal action";
                return result;
            }
        }
        const auto& witnessProgress = witness.scenarioObjectiveProgress();
        if (!witnessProgress.complete || witnessProgress.failed ||
            witness.winner() != 1)
        {
            const int survivingPlayerPieces = static_cast<int>(std::count_if(
                witness.boardPieces().begin(), witness.boardPieces().end(),
                [](const game_data::Piece& piece) { return piece.owner == 1; }));
            const int survivingEnemies = static_cast<int>(std::count_if(
                witness.boardPieces().begin(), witness.boardPieces().end(),
                [](const game_data::Piece& piece) { return piece.owner == 2; }));
            result.error = fmt::format(
                "open objective remains unproven after {} commands with a passive opponent "
                "(remaining {}, friendly {}, enemy {}; P1 move {}, attack {}, ability {}, pass {}; P2 move {}, attack {}, ability {}, pass {})",
                MaxWitnessActions,
                witnessProgress.remaining,
                survivingPlayerPieces,
                survivingEnemies,
                playerActionCounts[static_cast<std::size_t>(AiActionKind::MovePiece)],
                playerActionCounts[static_cast<std::size_t>(AiActionKind::AttackPiece)],
                playerActionCounts[static_cast<std::size_t>(AiActionKind::UseAbility)],
                playerActionCounts[static_cast<std::size_t>(AiActionKind::EndTurn)],
                opponentActionCounts[static_cast<std::size_t>(AiActionKind::MovePiece)],
                opponentActionCounts[static_cast<std::size_t>(AiActionKind::AttackPiece)],
                opponentActionCounts[static_cast<std::size_t>(AiActionKind::UseAbility)],
                opponentActionCounts[static_cast<std::size_t>(AiActionKind::EndTurn)]);
            result.error += "; board";
            for (const game_data::Piece& piece : witness.boardPieces())
            {
                result.error += fmt::format(
                    " [{}:{} hp{} @{},{} state{}]",
                    piece.owner,
                    piece.name,
                    piece.health,
                    piece.row,
                    piece.column,
                    piece.actionState);
            }
            // A legal setup and legal attempted actions do not prove that a
            // completion path exists. Leave this result red so an unreachable
            // or regressed open objective cannot pass the release gate.
            return result;
        }
        result.witnessProven = true;
        result.ok = true;
        return result;
    }

    const auto rolePieceId = [&](std::string_view role, std::string_view use)
        -> std::optional<int> {
        const auto found = roleIds.find(role);
        if (found == roleIds.end())
        {
            result.error = std::string(use) + " references unknown role: " +
                std::string(role);
            return std::nullopt;
        }
        return found->second;
    };
    const auto refreshRebirthRoles = [&](const std::vector<game_data::Piece>& before) {
        for (auto& [role, pieceId] : roleIds)
        {
            const bool stillPresent = std::any_of(
                engine.boardPieces().begin(),
                engine.boardPieces().end(),
                [&](const game_data::Piece& piece) { return piece.id == pieceId; });
            if (stillPresent)
            {
                continue;
            }
            const auto placement = std::find_if(
                mission.pieces.begin(),
                mission.pieces.end(),
                [&](const StoryPiecePlacement& piece) { return piece.role == role; });
            if (placement == mission.pieces.end())
            {
                continue;
            }
            const std::optional<game_data::GameCard> original =
                resolveLiveCard(placement->cardTitle, liveCards);
            if (!original || original->rebirthTitle.empty())
            {
                continue;
            }
            const auto previous = std::find_if(
                before.begin(),
                before.end(),
                [&](const game_data::Piece& piece) { return piece.id == pieceId; });
            if (previous == before.end())
            {
                continue;
            }
            const auto replacement = std::find_if(
                engine.boardPieces().begin(),
                engine.boardPieces().end(),
                [&](const game_data::Piece& piece) {
                    return piece.owner == placement->owner &&
                        piece.name == original->rebirthTitle &&
                        piece.row == previous->row &&
                        piece.column == previous->column;
                });
            if (replacement != engine.boardPieces().end())
            {
                pieceId = replacement->id;
            }
        }
    };

    bayou::storytest::ClaimCoverage claimCoverage;
    for (std::size_t index = 0; index < mission.script.size(); ++index)
    {
        const auto& step = mission.script[index];
        const bayou::storytest::EngineState beforeState =
            bayou::storytest::captureEngineState(engine);
        const bayou::storytest::RoleIds beforeRoleIds = roleIds;
        const std::vector<game_data::Piece>& before = beforeState.pieces;
        int targetRow = step.targetRow;
        int targetColumn = step.targetColumn;
        if (!step.targetRole.empty())
        {
            const std::optional<int> targetId =
                rolePieceId(step.targetRole, "target");
            if (!targetId)
            {
                return result;
            }
            const auto target = std::find_if(
                engine.boardPieces().begin(),
                engine.boardPieces().end(),
                [&](const game_data::Piece& piece) { return piece.id == *targetId; });
            if (target == engine.boardPieces().end())
            {
                result.error = "step " + std::to_string(index + 1) +
                    " targets a role no longer in play: " +
                    std::string(step.targetRole);
                return result;
            }
            targetRow = target->row;
            targetColumn = target->column;
        }

        int actorId = 0;
        if (!step.actorRole.empty())
        {
            const std::optional<int> resolved =
                rolePieceId(step.actorRole, "actor");
            if (!resolved)
            {
                return result;
            }
            actorId = *resolved;
        }

        std::optional<int> expectedActionIndex;
        std::optional<game_data::Piece> abilityActorBefore;
        if (step.kind == StoryActionKind::Move ||
            step.kind == StoryActionKind::Attack)
        {
            const auto actor = std::find_if(
                engine.boardPieces().begin(),
                engine.boardPieces().end(),
                [&](const game_data::Piece& piece) { return piece.id == actorId; });
            if (actor == engine.boardPieces().end())
            {
                result.error = "step " + std::to_string(index + 1) +
                    " has no live actor for exact profile '" +
                    std::string(step.expectedActionProfile) + "'";
                return result;
            }
            expectedActionIndex =
                bayou::client::storyExpectedActionProfileIndex(step, *actor);
            if (!expectedActionIndex)
            {
                result.error = "step " + std::to_string(index + 1) +
                    " has no unique active live profile named '" +
                    std::string(step.expectedActionProfile) + "'";
                return result;
            }

            // Mirror the client's pre-dispatch check, including whether the
            // exact live profile resolves as movement or attack from what the
            // acting side can currently see. The engine's movePiece and
            // attackPiece wrappers intentionally share one resolver, so an
            // engine-only golden path would otherwise miss a live kind drift
            // that the guided client correctly refuses.
            const std::vector<game_data::Piece> visiblePieces =
                game_data::piecesVisibleTo(engine.boardPieces(), step.owner);
            const auto visibleActor = std::find_if(
                visiblePieces.begin(), visiblePieces.end(),
                [&](const game_data::Piece& piece) { return piece.id == actorId; });
            const game_data::ActionResolution sourceResolution =
                visibleActor == visiblePieces.end()
                    ? game_data::ActionResolution{}
                    : game_data::resolvePieceAction(
                          visiblePieces,
                          engine.boardHoles(),
                          *visibleActor,
                          targetRow,
                          targetColumn,
                          false,
                          *expectedActionIndex);
            const StoryActionKind resolvedSourceKind = sourceResolution.attacks
                ? StoryActionKind::Attack
                : StoryActionKind::Move;
            if (!sourceResolution.legal ||
                !bayou::client::storySelectedPieceActionMatches(
                    step, resolvedSourceKind, *actor, *expectedActionIndex))
            {
                result.error = "step " + std::to_string(index + 1) +
                    " live profile '" + std::string(step.expectedActionProfile) +
                    "' no longer resolves as the authored Move/Attack kind";
                return result;
            }
        }
        else if (step.kind == StoryActionKind::UseAbility)
        {
            const auto actor = std::find_if(
                engine.boardPieces().begin(),
                engine.boardPieces().end(),
                [&](const game_data::Piece& piece) { return piece.id == actorId; });
            if (actor == engine.boardPieces().end() ||
                !bayou::client::storyAbilityStepMatches(step, *actor))
            {
                result.error = "step " + std::to_string(index + 1) +
                    " no longer names the actor's exact live ability identity, label, or summon result";
                return result;
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
            const auto card = std::find_if(
                hand.begin(),
                hand.end(),
                [&](const game_data::GameCard& value) {
                    return value.title == step.cardTitle;
                });
            accepted = card != hand.end() && engine.playCard(
                step.owner,
                static_cast<int>(std::distance(hand.begin(), card)),
                targetRow,
                targetColumn);
            if (accepted && !step.effectRole.empty())
            {
                const auto spawned = std::find_if(
                    engine.boardPieces().begin(),
                    engine.boardPieces().end(),
                    [&](const game_data::Piece& piece) {
                        return piece.owner == step.owner &&
                            piece.name == step.cardTitle &&
                            piece.row == targetRow &&
                            piece.column == targetColumn &&
                            piece.hasActed;
                    });
                if (spawned == engine.boardPieces().end())
                {
                    result.error = "step " + std::to_string(index + 1) +
                        " played a card but did not create an exhausted effect role " +
                        std::string(step.effectRole);
                    return result;
                }
                roleIds.insert_or_assign(step.effectRole, spawned->id);
            }
            break;
        }
        case StoryActionKind::DrawCard:
            accepted = engine.drawCard(step.owner);
            break;
        case StoryActionKind::ChooseForesight:
        {
            const auto& choices = engine.playerState(step.owner).foresightChoices;
            const auto card = std::find_if(
                choices.begin(),
                choices.end(),
                [&](const game_data::GameCard& value) {
                    return value.title == step.cardTitle;
                });
            accepted = card != choices.end() && engine.chooseForesightCard(
                step.owner,
                static_cast<int>(std::distance(choices.begin(), card)));
            break;
        }
        case StoryActionKind::DiscardCard:
        {
            const auto& hand = engine.playerState(step.owner).hand;
            const auto card = std::find_if(
                hand.begin(),
                hand.end(),
                [&](const game_data::GameCard& value) {
                    return value.title == step.cardTitle;
                });
            accepted = card != hand.end() && engine.discardCard(
                step.owner,
                static_cast<int>(std::distance(hand.begin(), card)));
            break;
        }
        case StoryActionKind::EndTurn:
            accepted = engine.endTurn(step.owner);
            break;
        case StoryActionKind::None:
            break;
        }
        if (!accepted)
        {
            result.error = "live engine rejected step " +
                std::to_string(index + 1) + " (" +
                std::string(step.heading) + "); active player P" +
                std::to_string(engine.currentPlayer()) + ", status: " +
                engine.snapshotFor(step.owner).status;
            return result;
        }
        if (abilityActorBefore &&
            !bayou::client::storyAbilityOutcomeMatches(
                step,
                *abilityActorBefore,
                engine.boardPieces(),
                engine.commandingPiece()))
        {
            result.error = "step " + std::to_string(index + 1) +
                " used its exact live ability but did not produce the authored authoritative outcome";
            return result;
        }
        const bayou::storytest::ClaimResult claimResult =
            bayou::storytest::verifyStepOutcome(
                mission,
                step,
                index,
                beforeState,
                bayou::storytest::captureEngineState(engine),
                beforeRoleIds,
                roleIds,
                claimCoverage);
        if (!claimResult.ok)
        {
            result.error = claimResult.error;
            return result;
        }
        refreshRebirthRoles(before);
    }

    const bayou::storytest::ClaimResult coverageResult =
        bayou::storytest::verifyMissionClaimCoverage(mission, claimCoverage);
    if (!coverageResult.ok)
    {
        result.error = coverageResult.error;
        return result;
    }
    result.claimChecks = claimCoverage.evaluated;

    const auto& finalProgress = engine.scenarioObjectiveProgress();
    if (finalProgress.failed || engine.winner() == 2)
    {
        result.error = "golden path finished in authoritative objective failure";
        return result;
    }
    if ((mission.objectiveSpec.kind == StoryObjectiveKind::DefeatAllEnemies ||
         mission.objectiveSpec.kind == StoryObjectiveKind::DefeatRole ||
         mission.objectiveSpec.kind == StoryObjectiveKind::ReachSquare ||
         mission.objectiveSpec.kind == StoryObjectiveKind::ControlSquares) &&
        (!finalProgress.complete || engine.winner() != 1))
    {
        result.error =
            "golden path was legal but did not complete its authoritative objective";
        return result;
    }

    result.ok = true;
    return result;
}

std::filesystem::path configFromArguments(int argc, char** argv, std::string& error)
{
    std::filesystem::path config =
        std::filesystem::path(BAYOU_SOURCE_DIR) / "client_release.cfg";
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        constexpr std::string_view Prefix = "--config=";
        if (argument.starts_with(Prefix))
        {
            config = std::filesystem::path(argument.substr(Prefix.size()));
        }
        else
        {
            error = "usage: storylivecatalogtest [--config=PATH]";
            return {};
        }
    }
    return config;
}

} // namespace

int main(int argc, char** argv)
{
    const bayou::storytest::ClaimCatalogResult claimCatalog =
        bayou::storytest::validateClaimCatalog();
    if (!claimCatalog.ok)
    {
        fmt::println(stderr, "[FAIL] {}", claimCatalog.error);
        return 2;
    }
    fmt::println(
        "Story mechanic claim catalog: {} before/after checks cover {} authored mastery claims.",
        claimCatalog.outcomeChecks,
        claimCatalog.masteryClaims);

    std::string error;
    const std::filesystem::path configPath =
        configFromArguments(argc, argv, error);
    if (!error.empty())
    {
        fmt::println(stderr, "[FAIL] {}", error);
        return 2;
    }
    if (!configureDefaultCa(error))
    {
        fmt::println(stderr, "[FAIL] {}", error);
        return 2;
    }

    std::optional<LiveCatalog> catalog;
    try
    {
        catalog = loadSchemaElevenCatalog(configPath, error);
    }
    catch (const std::exception& exception)
    {
        error = std::string("live catalog connection failed: ") + exception.what();
    }
    if (!catalog)
    {
        fmt::println(stderr, "[FAIL] {}", error);
        fmt::println(stderr, "       config: {}", configPath.string());
        return 2;
    }

    fmt::println(
        "Loaded {} authoritative schema-11 cards from {}.",
        catalog->cards.size(),
        catalog->endpoint);

    int failures = 0;
    int unprovenWitnesses = 0;
    std::size_t constructHealingActions = 0;
    if (!validateAuthoritativeConstructHealing(
            catalog->cards, constructHealingActions, error))
    {
        ++failures;
        fmt::println(stderr, "[FAIL][CONSTRUCT HEALING] {}", error);
    }
    else
    {
        fmt::println(
            "[PASS] {} authoritative Repair/Rebuild/Maintenance mappings heal Mechanical targets and reject non-Mechanical targets.",
            constructHealingActions);
    }
    std::size_t bodyguardTargetingActions = 0;
    if (!validateAuthoritativeBodyguardTargeting(
            catalog->cards, bodyguardTargetingActions, error))
    {
        ++failures;
        fmt::println(stderr, "[FAIL][BODYGUARD TARGETING] {}", error);
    }
    else
    {
        fmt::println(
            "[PASS] {} authoritative positive-damage Disable/zero-damage Disable/Pull/Infest/Push/Control actions preserve Bodyguard's damage-only retargeting boundary.",
            bodyguardTargetingActions);
    }
    int scriptedMissions = 0;
    int openMissions = 0;
    std::size_t evaluatedClaimChecks = 0;
    std::set<std::string> resolvedDependencies;
    for (StoryCampaign campaign : {
             StoryCampaign::Mirewatch,
             StoryCampaign::Blackthorn,
             StoryCampaign::Seelie})
    {
        for (const StoryMission& mission : bayou::client::storyMissions(campaign))
        {
            if (mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly)
            {
                continue;
            }
            const auto dependencyReport =
                bayou::client::validateStoryCardDependencies(
                    mission,
                    catalog->cards,
                    std::span<const card_data::Card>{},
                    StoryCardResolutionMode::AuthoritativeOnly);
            resolvedDependencies.insert(
                dependencyReport.requiredTitles.begin(),
                dependencyReport.requiredTitles.end());

            const MissionRun run = runMission(
                campaign,
                mission,
                catalog->cards,
                0x4c495645u +
                    static_cast<unsigned int>(mission.id.size()));
            if (run.open)
            {
                ++openMissions;
            }
            else
            {
                ++scriptedMissions;
            }
            if (!run.ok)
            {
                ++failures;
                if (run.witnessAttempted && !run.witnessProven)
                {
                    ++unprovenWitnesses;
                    fmt::println(
                        stderr,
                        "[FAIL][UNPROVEN] {} / {}: {}",
                        campaignLabel(campaign),
                        mission.id,
                        run.error);
                }
                else
                {
                    fmt::println(
                        stderr,
                        "[FAIL] {} / {}: {}",
                        campaignLabel(campaign),
                        mission.id,
                        run.error);
                }
            }
            else
            {
                evaluatedClaimChecks += run.claimChecks;
                fmt::println(
                    "[PASS] {} / {} ({} live dependencies; {}; {} mechanic checks; "
                    "{} configured-depth opponent actions across {} turns)",
                    campaignLabel(campaign),
                    mission.id,
                    run.dependencies,
                    run.witnessProven
                        ? "open legal-win witness"
                        : (run.open ? "open setup/objective" : "scripted golden path"),
                    run.claimChecks,
                    run.configuredOpponentActions,
                    run.configuredOpponentTurns);
            }
        }
    }

    std::size_t exactFixtureMatches = 0;
    const std::span<const std::string_view> packagedFixtureTitles =
        bayou::client::packagedStoryCardTitles();
    for (const std::string_view title : packagedFixtureTitles)
    {
        const std::optional<game_data::GameCard> packaged =
            bayou::client::packagedStoryCard(title);
        const std::optional<game_data::GameCard> live =
            resolveLiveCard(title, catalog->cards);
        if (!packaged || !live)
        {
            ++failures;
            fmt::println(
                stderr,
                "[FAIL][FIXTURE DRIFT] {}: {} definition missing",
                title,
                !packaged ? "packaged" : "live");
            continue;
        }
        const std::vector<std::string> mismatches =
            gameCardMismatchFields(*packaged, *live);
        if (!mismatches.empty())
        {
            ++failures;
            fmt::println(
                stderr,
                "[FAIL][FIXTURE DRIFT] {}: {}",
                title,
                joined(mismatches));
            continue;
        }
        ++exactFixtureMatches;
    }

    fmt::println(
        "Checked {} scripted golden paths, {} open setups/objectives, {} shared mechanic outcomes, {} authoritative construct-healing mappings, {} authoritative Bodyguard targeting actions, {} unique tactical dependencies, and {}/{} exact packaged/live fixture matches.",
        scriptedMissions,
        openMissions,
        evaluatedClaimChecks,
        constructHealingActions,
        bodyguardTargetingActions,
        resolvedDependencies.size(),
        exactFixtureMatches,
        packagedFixtureTitles.size());
    fmt::println("{} live Story catalog failure(s).", failures);
    fmt::println("{} bounded open-objective witness(es) remain unproven.", unprovenWitnesses);
    return failures == 0 ? 0 : 1;
}
