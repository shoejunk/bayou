#pragma once

#include "../shared/game_data.hpp"

#include <optional>
#include <string_view>

namespace bayou::client
{

// Reviewed Story Mode snapshot of the schema-v9 cards used by the campaigns.
// Live catalogue definitions still win; this fixture makes offline play and
// capture deterministic without inventing generic replacement statistics.
std::optional<game_data::GameCard> packagedStoryCard(std::string_view title);

} // namespace bayou::client
