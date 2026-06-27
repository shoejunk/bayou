#pragma once

#include "../shared/card_data.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace bayou::client::card_editor_assets
{

void setAssetRoot(std::filesystem::path assetRoot);
std::string assetRelativeImagePath(const std::string& value);
std::optional<std::filesystem::path> resolveAssetImagePath(const std::string& value);
void normalizeCardImagePath(card_data::Card& card);

} // namespace bayou::client::card_editor_assets
