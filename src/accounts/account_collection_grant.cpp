#include "account_collection_grant.hpp"

#include "account_catalog.hpp"
#include "account_tokens.hpp"
#include "../shared/game_data.hpp"

#include <fmt/core.h>
#include <exception>
#include <unordered_set>

namespace account_collection_grant
{
Result grantAllCards(
    SQLite::Database& database,
    const std::string& accessToken,
    const std::string& targetUsername)
{
    try
    {
        const auto username = account_tokens::authenticateAccessToken(database, accessToken);
        if (!username)
        {
            return {false, "Admin access required"};
        }
        {
            SQLite::Statement admin(database, "SELECT is_admin FROM accounts WHERE username = ? LIMIT 1");
            admin.bind(1, *username);
            if (!admin.executeStep() || admin.getColumn(0).getInt() == 0)
            {
                return {false, "Admin access required"};
            }
        }
        if (targetUsername.empty())
        {
            return {false, "Target username cannot be empty"};
        }
        {
            SQLite::Statement target(database, "SELECT 1 FROM accounts WHERE username = ? LIMIT 1");
            target.bind(1, targetUsername);
            if (!target.executeStep())
            {
                return {false, "Username not found"};
            }
        }

        account_catalog::refreshCardLibraryIfStale();
        std::unordered_set<std::string> titles;
        for (const auto& card : account_catalog::cardLibrary())
        {
            // Same eligibility as loadCollectibleCards, including starter cards.
            // Use the catalog directly so an unavailable/empty catalog cannot
            // silently grant the shop's built-in fallback instead of all cards.
            if (!card.title.empty() && !game_data::isTokenCard(card))
            {
                titles.insert(card.title);
            }
        }
        if (titles.empty())
        {
            return {false, "No collectible cards available; try again later"};
        }

        SQLite::Transaction transaction(database);
        SQLite::Statement upsert(
            database,
            "INSERT INTO card_collections (username, card_title, copies) VALUES (?, ?, 4) "
            "ON CONFLICT(username, card_title) DO UPDATE SET copies = 4 WHERE copies < 4");
        for (const auto& title : titles)
        {
            upsert.reset();
            upsert.bind(1, targetUsername);
            upsert.bind(2, title);
            upsert.exec();
        }
        transaction.commit();
        fmt::println("{} topped up all collectible cards to at least four copies for {}", *username, targetUsername);
        return {true, "Topped up all collectible cards to at least four copies for " + targetUsername};
    }
    catch (const std::exception& error)
    {
        fmt::println("Error while topping up player collection: {}", error.what());
        return {false, "Unable to top up player collection; try again later"};
    }
}
}
