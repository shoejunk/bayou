#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

namespace starter_deck_database
{

inline constexpr const char* Path = "starter_decks.db";

void initialize(SQLite::Database& database);

// Copies any starter-deck definitions from the legacy accounts database and
// removes the old table only after the new database has committed the data.
// Returns true when a legacy table was removed.
bool migrateFromAccounts(
    SQLite::Database& accountsDatabase,
    SQLite::Database& starterDeckDatabase);

} // namespace starter_deck_database
