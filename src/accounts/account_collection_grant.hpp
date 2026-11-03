#pragma once

#include <SQLiteCpp/SQLiteCpp.h>
#include <string>

namespace account_collection_grant
{
struct Result
{
    bool success = false;
    std::string message;
};

// Caller holds the accounts database mutex. Authenticates the admin and tops
// up every collectible catalog card atomically; never removes existing copies.
Result grantAllCards(
    SQLite::Database& database,
    const std::string& accessToken,
    const std::string& targetUsername);
}
