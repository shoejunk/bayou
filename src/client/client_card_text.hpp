#pragma once

#include "../shared/card_data.hpp"
#include "../shared/game_data.hpp"

#include <SFML/Graphics/Color.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace bayou::client
{

struct ActionDescription
{
    std::string name;
    std::string type;
    std::string moveIconPath;
    std::string range;
    int damage = 0;
    int heal = 0;
    int stun = 0;
    int cooldown = 0;
};

struct DetailRow
{
    std::string text;
    sf::Color color = sf::Color::White;
    std::optional<ActionDescription> action;
};

using DetailRows = std::vector<DetailRow>;

std::string cardRarity(const card_data::Card& card);
std::string cardRarityLabel(const card_data::Card& card);
sf::Color cardRarityColor(const card_data::Card& card);
std::string joinStrings(const std::vector<std::string>& values, const std::string& separator);
ActionDescription actionDescription(const game_data::ActionProfile& action, std::size_t index);
DetailRow actionDetailRow(
    const game_data::ActionProfile& action,
    std::size_t index,
    sf::Color color = sf::Color(143, 220, 205));
DetailRows deckEditorCardDetails(const card_data::Card& card);

} // namespace bayou::client
