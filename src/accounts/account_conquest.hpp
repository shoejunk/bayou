#pragma once

#include "../shared/conquest_data.hpp"

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace account_conquest
{

std::vector<conquest_data::ConquestDeck> loadDecks(
    SQLite::Database& database,
    const std::string& username);

conquest_data::ConquestArmy loadArmy(
    SQLite::Database& database,
    const std::string& username);

// On success, mutates deck with its stable database id and new revision.
// Validation failures are returned as user-facing errors; database failures
// are allowed to propagate to the request handler.
std::optional<std::string> saveDeck(
    SQLite::Database& database,
    const std::string& username,
    conquest_data::ConquestDeck& deck);

std::optional<std::string> deleteDeck(
    SQLite::Database& database,
    const std::string& username,
    std::int64_t deckId,
    std::uint32_t expectedRevision);

// On success, mutates army with its new revision.
std::optional<std::string> saveArmy(
    SQLite::Database& database,
    const std::string& username,
    conquest_data::ConquestArmy& army);

void purgeTokenCards(SQLite::Database& database);

} // namespace account_conquest
