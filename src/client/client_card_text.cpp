#include "client_card_text.hpp"

#include <algorithm>

namespace bayou::client
{
namespace
{
std::string actionTypeName(const game_data::ActionProfile& action)
{
    const game_data::ActionKind kind = static_cast<game_data::ActionKind>(action.kind);
    const bool ranged = kind == game_data::ActionKind::Ranged;
    const bool healing = action.heal > 0 && action.damage == 0;
    if (ranged)
    {
        return healing ? "Ranged Heal" : "Ranged Attack";
    }
    if (kind == game_data::ActionKind::Capture)
    {
        return "Capture";
    }
    if (kind == game_data::ActionKind::Hop)
    {
        return action.canAttack ? "Hop Attack" : "Hop";
    }
    if (kind == game_data::ActionKind::Teleport)
    {
        return "Teleport";
    }
    if (kind == game_data::ActionKind::Tunnel)
    {
        return "Tunnel";
    }
    if (action.canMove)
    {
        if (healing)
        {
            return "Heal Move";
        }
        if (action.damage > 0 || action.canAttack)
        {
            return "Attack Move";
        }
        return "Move";
    }
    return healing ? "Heal" : "Attack";
}

std::string actionMoveIconPath(const game_data::ActionProfile& action)
{
    if (static_cast<game_data::ActionKind>(action.kind) == game_data::ActionKind::Hop)
    {
        return "ui/hopping-move.png";
    }
    switch (static_cast<game_data::MovePattern>(action.pattern))
    {
        case game_data::MovePattern::Ortho: return "ui/ortho-move.png";
        case game_data::MovePattern::Diag: return "ui/diagonal-move.png";
        case game_data::MovePattern::Omni: return "ui/omni-move.png";
        case game_data::MovePattern::Jump: return "ui/l-shaped-move.png";
        default: return "ui/move.png";
    }
}

std::string actionMoveTooltipTitle(const game_data::ActionProfile& action)
{
    switch (static_cast<game_data::ActionKind>(action.kind))
    {
        case game_data::ActionKind::Hop: return "Hop";
        case game_data::ActionKind::Teleport: return "Teleport";
        case game_data::ActionKind::Tunnel: return "Tunnel";
        default: return game_data::movePatternName(action.pattern);
    }
}

std::string actionMoveTooltipText(const game_data::ActionProfile& action)
{
    switch (static_cast<game_data::ActionKind>(action.kind))
    {
        case game_data::ActionKind::Hop:
            return "Jumps over an adjacent piece and lands two squares away.";
        case game_data::ActionKind::Teleport:
            return "Moves directly to any empty square.";
        case game_data::ActionKind::Tunnel:
            return "Moves directly from one hole to another.";
        default: break;
    }

    switch (static_cast<game_data::MovePattern>(action.pattern))
    {
        case game_data::MovePattern::Ortho:
            return "This action reaches in straight horizontal or vertical lines.";
        case game_data::MovePattern::Diag:
            return "This action reaches along diagonal lines.";
        case game_data::MovePattern::Omni:
            return "This action reaches in straight or diagonal lines.";
        case game_data::MovePattern::Jump:
            return "This action reaches in an L shape and can jump over pieces.";
        case game_data::MovePattern::Horizontal:
            return "This action reaches horizontally to the left or right.";
        case game_data::MovePattern::Vertical:
            return "This action reaches vertically up or down.";
        default:
            return "This action can reach any square within its listed range.";
    }
}

std::string actionRangeText(const game_data::ActionProfile& action)
{
    return action.minRange > 1
        ? std::to_string(action.minRange) + "-" + std::to_string(action.maxRange)
        : std::to_string(action.maxRange);
}

bool isHiddenCardDetailKey(const std::string& key)
{
    return key == "Deck Limit" || key == "deckLimit" || key == "cost" || key == "heroCost" || key == "health" || key == "attack" || key == "Tax" || key == "tax" ||
        key == "range" || key == "move" || key == "attackingMove" || key == "power" ||
        key == "canControl" || key == "growTurns" || key == "abilityUses" || key == "gatherResources" ||
        key == "ability" || key == "summon" || key == "summonUnit" || key == "abilityLabels" ||
        key == game_data::HealingAuraField ||
        key == "WalkAnimFrames" || key == "IdleAnimFrames" ||
        key == "AttackAnimFrames" || key == "DamagedAnimFrames" || key == "KilledAnimFrames" ||
        key == "rarity" || key == "effect" || key == "target" || key == "rebirth" ||
        key == "movement" || key == "WalkAnim" || key == "IdleAnim" ||
        key == "AttackAnim" || key == "DamagedAnim" || key == "KilledAnim" || key == "Token" ||
        key == "State1Token" ||
        key == "PieceBaseBlue" || key == "PieceBaseRed" || key == "PieceArtStyle" ||
        key == "detail" || key == "Detail";
}

bool actionStateReachable(
    const std::vector<game_data::ActionProfile>& actions,
    std::string_view ability,
    int startState,
    int targetState)
{
    if (startState == targetState)
    {
        return true;
    }
    int stateCount = std::max(startState, targetState) + 1;
    for (const game_data::ActionProfile& action : actions)
    {
        stateCount = std::max(stateCount, action.state + 1);
        stateCount = std::max(stateCount, game_data::actionNextState(action) + 1);
    }
    const std::string normalized = game_data::normalizedAbility(std::string(ability));
    if ((normalized == "transform" || normalized == "dematerialize") &&
        targetState >= 0 && targetState < stateCount)
    {
        return true;
    }
    if (startState < 0 || startState >= stateCount ||
        targetState < 0 || targetState >= stateCount)
    {
        return false;
    }

    std::vector<bool> reached(static_cast<std::size_t>(stateCount), false);
    reached[static_cast<std::size_t>(startState)] = true;
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (const game_data::ActionProfile& action : actions)
        {
            if (action.state < 0 || action.state >= stateCount ||
                !reached[static_cast<std::size_t>(action.state)])
            {
                continue;
            }
            const int nextState = game_data::actionNextState(action);
            if (nextState >= 0 && nextState < stateCount &&
                !reached[static_cast<std::size_t>(nextState)])
            {
                reached[static_cast<std::size_t>(nextState)] = true;
                changed = true;
            }
        }
    }
    return reached[static_cast<std::size_t>(targetState)];
}

std::string actionStateLabel(const game_data::GameCard& card, int state)
{
    if (card.ability == "dematerialize")
    {
        return state == 0 ? "Materialized" : "Hidden";
    }
    if (card.ability == "transform" && card.abilityLabels.size() >= 2 &&
        card.abilityLabels[0] == "Raise Gun" &&
        card.abilityLabels[1] == "Lower Gun")
    {
        return state == 0 ? "Gun lowered" : "Gun raised";
    }
    return "State " + std::to_string(state);
}

} // namespace

std::string cardRarity(const card_data::Card& card)
{
    return game_data::cardRarity(card);
}

std::string cardRarityLabel(const card_data::Card& card)
{
    const std::string rarity = cardRarity(card);
    if (rarity == "token")
    {
        return "Token";
    }
    if (rarity == "starter")
    {
        return "Starter";
    }
    if (rarity == "legendary")
    {
        return "Legendary";
    }
    if (rarity == "uncommon")
    {
        return "Uncommon";
    }
    if (rarity == "rare")
    {
        return "Rare";
    }
    return "Common";
}

sf::Color cardRarityColor(const card_data::Card& card)
{
    const std::string rarity = cardRarity(card);
    if (rarity == "token")
    {
        return sf::Color(143, 220, 205);
    }
    if (rarity == "starter")
    {
        return sf::Color(168, 208, 150);
    }
    if (rarity == "legendary")
    {
        return sf::Color(248, 214, 112);
    }
    if (rarity == "uncommon")
    {
        return sf::Color(113, 211, 145);
    }
    if (rarity == "rare")
    {
        return sf::Color(151, 192, 255);
    }
    return sf::Color(190, 198, 214);
}

std::string cardCostLabel(const card_data::Card& card)
{
    if (game_data::isHeroCard(card))
    {
        return "Hero cost: " + std::to_string(game_data::cardInt(card, "heroCost", 0));
    }
    return "Cost: " + std::to_string(game_data::cardInt(card, "cost", 0)) + " Resources";
}

std::string cardStatLabel(const card_data::Card& card)
{
    const game_data::GameCard gameCard = game_data::toGameCard(card);
    if (card.type == "Unit" || game_data::isHeroCard(card))
    {
        return "Health: " + std::to_string(gameCard.health);
    }

    return "Power: " + std::to_string(gameCard.power) +
        "  Effect: " + gameCard.effect;
}

std::string cardLibraryMeta(const card_data::Card& card)
{
    std::string result = card.type + " / " + cardRarityLabel(card);
    if (game_data::isHeroCard(card))
    {
        result += " / Hero " + std::to_string(game_data::cardInt(card, "heroCost", 0));
    }
    else
    {
        result += " / " + std::to_string(game_data::cardInt(card, "cost", 0)) + " res";
        if (card.type == "Unit")
        {
            result += " / HP " + std::to_string(game_data::cardInt(card, "health", 1));
        }
    }
    return result;
}

std::string joinStrings(const std::vector<std::string>& values, const std::string& separator)
{
    std::string result;
    for (const std::string& value : values)
    {
        if (!result.empty())
        {
            result += separator;
        }
        result += value;
    }
    return result;
}

ActionDescription actionDescription(const game_data::ActionProfile& action, std::size_t index)
{
    ActionDescription description;
    description.name = action.name.empty()
        ? "Action " + std::to_string(index + 1)
        : action.name;
    description.type = actionTypeName(action);
    description.moveIconPath = actionMoveIconPath(action);
    description.moveTooltipTitle = actionMoveTooltipTitle(action);
    description.moveTooltipText = actionMoveTooltipText(action);
    description.range = actionRangeText(action);
    description.damage = std::max(0, action.damage);
    description.heal = std::max(0, action.heal);
    description.stun = std::max(0, action.statusTurns);
    description.cooldown = std::max(0, action.cooldownTurns);
    description.control = std::max(0, action.control);
    description.repeat = std::max(0, action.repeat);
    description.push = std::max(0, action.push);
    description.passThrough = action.passThrough;
    description.clearPath =
        static_cast<game_data::ActionKind>(action.kind) == game_data::ActionKind::Ranged;
    description.pull = action.pull;
    description.targetFilter.reserve(action.targetFilter.size());
    for (const std::string& filter : action.targetFilter)
    {
        description.targetFilter.push_back(
            game_data::normalizedTrait(filter) == "construct"
                ? "Mechanical"
                 : filter);
    }
    description.infest = action.infest;
    return description;
}

DetailRow actionDetailRow(
    const game_data::ActionProfile& action,
    std::size_t index,
    sf::Color color)
{
    return {"", color, actionDescription(action, index)};
}

DetailRows deckEditorCardDetails(const card_data::Card& card)
{
    DetailRows details;
    const game_data::GameCard gameCard = game_data::toGameCard(card);
    const bool hero = game_data::isHeroCard(card);
    const bool unit = card.type == "Unit" || hero;

    if (hero)
    {
        details.push_back({"Rarity: " + cardRarityLabel(card), cardRarityColor(card)});
        details.push_back({"Hero cost: " + std::to_string(game_data::cardInt(card, "heroCost", 0)),
                           sf::Color(248, 214, 112)});
    }
    else
    {
        details.push_back({"Rarity: " + cardRarityLabel(card), cardRarityColor(card)});
        details.push_back({"Cost: " + std::to_string(game_data::cardInt(card, "cost", 0)) + " Resources",
                           sf::Color(150, 210, 235)});
    }
    details.push_back({"Deck limit: " + std::to_string(game_data::cardDeckLimit(card)),
                       sf::Color(248, 214, 112)});

    if (unit)
    {
        if (!card.traits.empty())
        {
            details.push_back({"Traits: " + joinStrings(card.traits, ", "),
                               sf::Color(248, 214, 112)});
        }
        if (!card.keywords.empty())
        {
            details.push_back({"Keywords: " + joinStrings(card.keywords, ", "),
                               sf::Color(146, 206, 226)});
        }
        details.push_back({"Health: " + std::to_string(gameCard.health), sf::Color(224, 210, 176)});
        if (!hero)
        {
            details.push_back({
                "Deployment: place on a controlled footprint that appears empty. If it contains only opposing hidden pieces, the attempt spends this card and its Resources, creates no Unit, and materializes each piece Disabled for its next owner-turn activation.",
                sf::Color(210, 216, 228)});
        }
        if (gameCard.tax > 0)
        {
            details.push_back({"Tax: " + std::to_string(gameCard.tax) + " Resources",
                               sf::Color(248, 214, 112)});
        }
        if (gameCard.gatherResources > 0)
        {
            details.push_back({"Gather: +" + std::to_string(gameCard.gatherResources) + " Resources each turn",
                               sf::Color(143, 220, 205)});
        }
        if (gameCard.healingAura > 0)
        {
            details.push_back({"Healing aura: +" + std::to_string(gameCard.healingAura) +
                                   " Health to every other adjacent piece, friend or foe, at owner's turn end",
                               sf::Color(143, 220, 205)});
        }
        if (!gameCard.rebirthTitle.empty())
        {
            details.push_back({"Rebirth: " + gameCard.rebirthTitle,
                               sf::Color(194, 150, 235)});
        }
        if (!gameCard.ability.empty())
        {
            std::string abilityText;
            if (gameCard.ability == "summon")
            {
                abilityText = gameCard.summonTitle.empty()
                    ? "Ability unavailable: Summon has no Unit defined on this live card"
                    : "Ability: Summon " + gameCard.summonTitle +
                        " into the square directly in front. If an opposing hidden piece occupies it, the ability is spent, creates no Unit, and materializes that piece Disabled for its next owner-turn activation";
            }
            else if (gameCard.ability == "dig")
            {
                abilityText = "Ability: Dig a hole on this piece's square";
            }
            else if (gameCard.ability == "command")
            {
                abilityText = "Ability: Command a ready adjacent friendly piece";
            }
            else if (gameCard.ability == "dematerialize")
            {
                abilityText = "Ability: cycle between materialized and hidden action states";
            }
            else if (gameCard.ability == "transform")
            {
                abilityText = "Ability: cycle to the next action state";
            }
            else
            {
                abilityText = "Ability: " + gameCard.ability;
            }
            details.push_back({
                std::move(abilityText),
                gameCard.ability == "summon" && gameCard.summonTitle.empty()
                    ? sf::Color(225, 170, 150)
                    : sf::Color(210, 216, 228)});
        }
        if (gameCard.actions.empty())
        {
            details.push_back({"Actions: none", sf::Color(225, 170, 150)});
        }
        const bool usesActionStates = std::any_of(
            gameCard.actions.begin(), gameCard.actions.end(),
            [](const game_data::ActionProfile& action) {
                return action.state != 0 ||
                    game_data::actionNextState(action) != action.state;
            });
        const bool hasReachableAction = std::any_of(
            gameCard.actions.begin(), gameCard.actions.end(),
            [&](const game_data::ActionProfile& action) {
                return actionStateReachable(
                    gameCard.actions, gameCard.ability, 0, action.state);
            });
        if (!gameCard.actions.empty() && !hasReachableAction)
        {
            details.push_back({
                "UNUSABLE ACTION DEFINITION: this card deploys in State 0, but no action state is reachable.",
                sf::Color(242, 126, 112)});
        }
        else if (usesActionStates)
        {
            details.push_back({
                "Deploys in " + actionStateLabel(gameCard, 0) +
                    ". Action rows below show when each profile is reachable.",
                sf::Color(210, 216, 228)});
        }
        for (std::size_t i = 0; i < gameCard.actions.size(); ++i)
        {
            const game_data::ActionProfile& action = gameCard.actions[i];
            if (usesActionStates)
            {
                const bool reachable = actionStateReachable(
                    gameCard.actions, gameCard.ability, 0, action.state);
                const int nextState = game_data::actionNextState(action);
                details.push_back({
                    std::string(reachable ? "AVAILABLE IN " :
                        "UNREACHABLE IN THIS CARD DEFINITION - requires ") +
                        actionStateLabel(gameCard, action.state) +
                        (nextState == action.state
                            ? "; remains in this state"
                            : "; after use: " + actionStateLabel(gameCard, nextState)),
                    reachable
                        ? sf::Color(225, 188, 120)
                        : sf::Color(242, 126, 112)});
            }
            details.push_back(actionDetailRow(action, i));
        }
    }
    else
    {
        const bool unsupportedSpell =
            gameCard.type == "Spell" && !game_data::isSupportedSpellEffect(gameCard);
        if (unsupportedSpell)
        {
            details.push_back({
                "UNAVAILABLE: this Spell has no defined game effect, cannot be played, and is not legal in a deck.",
                sf::Color(242, 126, 112)});
        }
        else
        {
            const std::string effect = game_data::cardStr(card, "effect", "none");
            details.push_back({"Effect: " + (effect == "steam" ? "resources" : effect),
                               sf::Color(224, 210, 176)});
            details.push_back({"Power: " + std::to_string(game_data::cardInt(card, "power", 0)),
                               sf::Color(224, 210, 176)});
            details.push_back({"Target: " + game_data::cardStr(card, "target", "none"),
                               sf::Color(143, 220, 205)});
        }
    }

    for (const card_data::KeyIntPair& item : card.integerValues)
    {
        if (!isHiddenCardDetailKey(item.key))
        {
            details.push_back({item.key + ": " + std::to_string(item.value), sf::Color(190, 198, 214)});
        }
    }
    for (const card_data::KeyStringPair& item : card.stringValues)
    {
        if (!isHiddenCardDetailKey(item.key))
        {
            details.push_back({item.key + ": " + item.value, sf::Color(190, 198, 214)});
        }
    }
    for (const card_data::KeyStringList& item : card.stringLists)
    {
        if (!isHiddenCardDetailKey(item.key))
        {
            details.push_back({item.key + ": " + joinStrings(item.values, ", "), sf::Color(190, 198, 214)});
        }
    }

    return details;
}

} // namespace bayou::client
