#include "client_card_text.hpp"

#include <algorithm>

namespace bayou::client
{
namespace
{
std::string actionTypeName(const game_data::ActionProfile& action)
{
    const bool ranged = static_cast<game_data::ActionKind>(action.kind) == game_data::ActionKind::Ranged;
    const bool healing = action.heal > 0 && action.damage == 0;
    if (ranged)
    {
        return healing ? "Ranged Heal" : "Ranged Attack";
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

std::string actionRangeText(const game_data::ActionProfile& action)
{
    return action.minRange > 1
        ? std::to_string(action.minRange) + "-" + std::to_string(action.maxRange)
        : std::to_string(action.maxRange);
}

bool isHiddenCardDetailKey(const std::string& key)
{
    return key == "cost" || key == "heroCost" || key == "health" || key == "attack" || key == "Tax" || key == "tax" ||
        key == "range" || key == "move" || key == "attackingMove" || key == "power" ||
        key == "canControl" || key == "growTurns" || key == "abilityUses" || key == "gatherResources" ||
        key == "WalkAnimFrames" || key == "IdleAnimFrames" ||
        key == "AttackAnimFrames" || key == "DamagedAnimFrames" || key == "KilledAnimFrames" ||
        key == "rarity" || key == "effect" || key == "target" || key == "rebirth" ||
        key == "movement" || key == "WalkAnim" || key == "IdleAnim" ||
        key == "AttackAnim" || key == "DamagedAnim" || key == "KilledAnim" || key == "Token" ||
        key == "PieceBaseBlue" || key == "PieceBaseRed";
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
    if (rarity == "legendary")
    {
        return "Legendary";
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
    if (rarity == "legendary")
    {
        return sf::Color(248, 214, 112);
    }
    if (rarity == "rare")
    {
        return sf::Color(151, 192, 255);
    }
    return sf::Color(190, 198, 214);
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
    description.range = actionRangeText(action);
    description.damage = std::max(0, action.damage);
    description.heal = std::max(0, action.heal);
    description.stun = std::max(0, action.statusTurns);
    description.cooldown = std::max(0, action.cooldownTurns);
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

    if (unit)
    {
        details.push_back({"Health: " + std::to_string(gameCard.health), sf::Color(224, 210, 176)});
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
        if (!gameCard.rebirthTitle.empty())
        {
            details.push_back({"Rebirth: " + gameCard.rebirthTitle,
                               sf::Color(194, 150, 235)});
        }
        if (gameCard.actions.empty())
        {
            details.push_back({"Actions: none", sf::Color(225, 170, 150)});
        }
        for (std::size_t i = 0; i < gameCard.actions.size(); ++i)
        {
            details.push_back(actionDetailRow(gameCard.actions[i], i));
        }
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
        details.push_back({item.key + ": " + joinStrings(item.values, ", "), sf::Color(190, 198, 214)});
    }

    return details;
}

} // namespace bayou::client
