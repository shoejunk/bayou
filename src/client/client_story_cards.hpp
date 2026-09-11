#pragma once

#include "../shared/game_data.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace bayou::client
{

// Reviewed Story Mode snapshot of the schema-v11 cards used by the campaigns.
// Live catalogue definitions still win; this fixture makes offline play and
// capture deterministic without inventing generic replacement statistics.
std::optional<game_data::GameCard> packagedStoryCard(std::string_view title);

// Complete, stable registry of every title accepted by packagedStoryCard.
// Validation must iterate this surface directly rather than inferring it from
// currently tactical mission dependencies, because StoryOnly/capture fixtures
// also render these definitions.
std::span<const std::string_view> packagedStoryCardTitles();

} // namespace bayou::client
