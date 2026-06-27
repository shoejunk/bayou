#pragma once

#include "../shared/card_data.hpp"
#include "../shared/game_data.hpp"

#include <SFML/Graphics/Color.hpp>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace bayou::client
{

using DetailRows = std::vector<std::pair<std::string, sf::Color>>;

std::string cardRarity(const card_data::Card& card);
std::string cardRarityLabel(const card_data::Card& card);
sf::Color cardRarityColor(const card_data::Card& card);
std::string joinStrings(const std::vector<std::string>& values, const std::string& separator);
std::string actionDescription(const game_data::ActionProfile& action, std::size_t index);
DetailRows deckEditorCardDetails(const card_data::Card& card);

} // namespace bayou::client
