#pragma once

#include "card_data.hpp"
#include "card_source_config.hpp"

#include <string>
#include <vector>

namespace card_server_client
{

std::vector<card_data::Card> load(const card_source_config::Config& config, std::string& error);

} // namespace card_server_client
