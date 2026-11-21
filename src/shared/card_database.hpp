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
void initializeCardPassives(SQLite::Database& database);
void initializeActivatedAbilities(SQLite::Database& database);
std::vector<ability_data::Definition> loadActivatedAbilities(SQLite::Database& database);
std::optional<ability_data::Definition> loadActivatedAbility(SQLite::Database& database, const std::string& name);
void saveActivatedAbility(SQLite::Database& database, const ability_data::Definition& definition, const std::string& originalName = "");
void deleteActivatedAbility(SQLite::Database& database, const std::string& name);
void setCardActivatedAbilities(SQLite::Database& database, const std::string& title, const std::string& abilityName, const std::string& sacrificeName, bool authoritative = true);
void initializeBoardMarkers(SQLite::Database& database);
std::vector<marker_data::Definition> loadBoardMarkers(SQLite::Database& database);
std::optional<marker_data::Definition> loadBoardMarker(SQLite::Database& database, const std::string& name);
void saveBoardMarker(SQLite::Database& database, const marker_data::Definition& marker, const std::string& originalName = "");
void deleteBoardMarker(SQLite::Database& database, const std::string& name);
void setCardBoardMarker(SQLite::Database& database, const std::string& title, const std::string& name);
}
