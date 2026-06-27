#pragma once

#include "card_data.hpp"

#include <SQLiteCpp/SQLiteCpp.h>

#include <filesystem>
#include <vector>

namespace card_database
{
std::vector<card_data::Action> loadActions(SQLite::Database& database);
std::vector<card_data::Action> loadCardActions(SQLite::Database& database, const std::string& title);
std::vector<card_data::Card> loadCards(SQLite::Database& database);
std::vector<card_data::Card> loadCardsFromFile(const std::filesystem::path& databasePath);
}
