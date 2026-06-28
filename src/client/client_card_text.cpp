#include "client_card_text.hpp"

namespace bayou::client
{
namespace
{
std::string actionKindName(std::uint8_t kind)
{
    switch (static_cast<game_data::ActionKind>(kind))
    {
        case game_data::ActionKind::Ranged: return "Ranged";
        case game_data::ActionKind::Hop: return "Hop";
        case game_data::ActionKind::Teleport: return "Teleport";
        case game_data::ActionKind::Tunnel: return "Tunnel";
        case game_data::ActionKind::Capture: return "Capture";
        default: return "Slide";
    }
}

std::string actionPatternName(const game_data::ActionProfile& action)
{
    if (static_cast<game_data::MovePattern>(action.pattern) == game_data::MovePattern::None)
    {
        return static_cast<game_data::ActionKind>(action.kind) == game_data::ActionKind::Ranged
            ? "Any direction"
            : "No pattern";
    }
    return game_data::movePatternName(action.pattern);
}

std::string actionRangeText(const game_data::ActionProfile& action)
{
    if (action.minRange == action.maxRange)
    {
        return "range " + std::to_string(action.maxRange);
    }
    return "range " + std::to_string(action.minRange) + "-" + std::to_string(action.maxRange);
}

bool isHiddenCardDetailKey(const std::string& key)
{
    return key == "cost" || key == "heroCost" || key == "health" || key == "attack" ||
        key == "range" || key == "move" || key == "attackingMove" || key == "power" ||
        key == "canControl" || key == "growTurns" || key == "abilityUses" ||
        key == "WalkAnimFrames" || key == "rarity" || key == "effect" || key == "target" ||
        key == "movement" || key == "WalkAnim" || key == "WalkAnimBlue" ||
        key == "WalkAnimRed" || key == "TokenBlue" || key == "TokenRed";
}

} // namespace

std::string cardRarity(const card_data::Card& card)
{
    const std::string rarity = game_data::cardStr(card, "rarity", "common");
    if (rarity == "rare" || rarity == "legendary")
    {
        return rarity;
    }
    return "common";
}

std::string cardRarityLabel(const card_data::Card& card)
{
    const std::string rarity = cardRarity(card);
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

std::string actionDescription(const game_data::ActionProfile& action, std::size_t index)
{
    std::vector<std::string> parts;
    if (action.state != 0)
    {
        parts.push_back("state " + std::to_string(action.state));
    }
    parts.push_back(actionKindName(action.kind));
    parts.push_back(actionPatternName(action));
    parts.push_back(actionRangeText(action));
    if (action.canMove)
    {
        parts.push_back("moves");
    }
    if (action.canAttack)
    {
        parts.push_back("attacks for " + std::to_string(action.damage));
    }
    if (action.statusTurns > 0)
    {
        parts.push_back("disables " + std::to_string(action.statusTurns) + " turn(s)");
    }
    if (action.cooldownTurns > 0)
    {
        parts.push_back("cooldown " + std::to_string(action.cooldownTurns));
    }
    if (action.passThrough)
    {
        parts.push_back("passes through blockers");
    }
    if (action.lineOfSight)
    {
        parts.push_back("line of sight");
    }

    const std::string label = action.name.empty()
        ? "Action " + std::to_string(index + 1)
        : action.name;
    return label + ": " + joinStrings(parts, ", ");
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
        details.push_back({"Cost: " + std::to_string(game_data::cardInt(card, "cost", 0)) + " steam",
                           sf::Color(150, 210, 235)});
    }

    if (unit)
    {
        details.push_back({"Health: " + std::to_string(gameCard.health), sf::Color(224, 210, 176)});
        if (gameCard.actions.empty())
        {
            details.push_back({"Actions: none", sf::Color(225, 170, 150)});
        }
        for (std::size_t i = 0; i < gameCard.actions.size(); ++i)
        {
            details.push_back({actionDescription(gameCard.actions[i], i), sf::Color(143, 220, 205)});
        }
        details.push_back({"Territory: occupied square + adjacent influence", sf::Color(198, 180, 142)});
    }
    else
    {
        details.push_back({"Effect: " + game_data::cardStr(card, "effect", "none"),
                           sf::Color(224, 210, 176)});
        details.push_back({"Power: " + std::to_string(game_data::cardInt(card, "power", 0)),
                           sf::Color(224, 210, 176)});
        details.push_back({"Target: " + game_data::cardStr(card, "target", "none"),
                           sf::Color(143, 220, 205)});
    }

    if (!card.keywords.empty())
    {
        details.push_back({"Keywords: " + joinStrings(card.keywords, ", "), sf::Color(210, 216, 228)});
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
