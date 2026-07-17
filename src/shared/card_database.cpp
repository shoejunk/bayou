#include "card_database.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

namespace card_database
{
namespace
{
bool tableExists(SQLite::Database& database, const std::string& tableName)
{
    SQLite::Statement query(
        database,
        "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?");
    query.bind(1, tableName);
    query.executeStep();
    return query.getColumn(0).getInt() != 0;
}

bool columnExists(
    SQLite::Database& database,
    const std::string& tableName,
    const std::string& columnName)
{
    SQLite::Statement query(database, "PRAGMA table_info(" + tableName + ")");
    while (query.executeStep())
    {
        if (query.getColumn(1).getString() == columnName)
        {
            return true;
        }
    }
    return false;
}

card_data::Action actionFromQuery(SQLite::Statement& query)
{
    card_data::Action action;
    action.name = query.getColumn(0).getString();
    action.state = query.getColumn(1).getInt();
    action.kind = query.getColumn(2).getString();
    action.pattern = query.getColumn(3).getString();
    action.minRange = query.getColumn(4).getInt();
    action.maxRange = query.getColumn(5).getInt();
    action.damage = query.getColumn(6).getInt();
    action.heal = query.getColumn(7).getInt();
    if (action.damage < 0)
    {
        action.heal = std::max(action.heal, -action.damage);
        action.damage = 0;
    }
    action.heal = std::max(0, action.heal);
    action.canMove = query.getColumn(8).getInt() != 0;
    action.canAttack = query.getColumn(9).getInt() != 0;
    action.passThrough = query.getColumn(10).getInt() != 0;
    action.lineOfSight = query.getColumn(11).getInt() != 0;
    action.statusTurns = query.getColumn(12).getInt();
    action.cooldownTurns = query.getColumn(13).getInt();
    action.push = std::max(0, query.getColumn(14).getInt());
    return action;
}

std::vector<std::string> loadStringColumn(
    SQLite::Database& database,
    const std::string& sql,
    const std::string& title)
{
    std::vector<std::string> values;
    SQLite::Statement query(database, sql);
    query.bind(1, title);
    while (query.executeStep())
    {
        values.push_back(query.getColumn(0).getString());
    }
    return values;
}

std::vector<std::string> loadActionTargetFilter(
    SQLite::Database& database,
    const std::string& actionName)
{
    return loadStringColumn(
        database,
        "SELECT value FROM action_target_filters WHERE action_name = ? ORDER BY item_index",
        actionName);
}

std::vector<card_data::KeyIntPair> loadIntegerValues(SQLite::Database& database, const std::string& title)
{
    std::vector<card_data::KeyIntPair> values;
    SQLite::Statement query(database, "SELECT key, value FROM card_integer_values WHERE title = ? ORDER BY key");
    query.bind(1, title);
    while (query.executeStep())
    {
        values.push_back({query.getColumn(0).getString(), query.getColumn(1).getInt()});
    }
    return values;
}

std::vector<card_data::KeyStringPair> loadStringValues(SQLite::Database& database, const std::string& title)
{
    std::vector<card_data::KeyStringPair> values;
    SQLite::Statement query(database, "SELECT key, value FROM card_string_values WHERE title = ? ORDER BY key");
    query.bind(1, title);
    while (query.executeStep())
    {
        values.push_back({query.getColumn(0).getString(), query.getColumn(1).getString()});
    }
    return values;
}

std::vector<card_data::KeyStringList> loadStringLists(SQLite::Database& database, const std::string& title)
{
    std::map<std::string, std::vector<std::string>> grouped;
    SQLite::Statement query(
        database,
        "SELECT key, value FROM card_string_lists WHERE title = ? ORDER BY key, item_index");
    query.bind(1, title);
    while (query.executeStep())
    {
        grouped[query.getColumn(0).getString()].push_back(query.getColumn(1).getString());
    }

    std::vector<card_data::KeyStringList> values;
    for (auto& [key, items] : grouped)
    {
        values.push_back({key, std::move(items)});
    }
    return values;
}

std::vector<std::string> loadActionNames(SQLite::Database& database, const std::string& title)
{
    return loadStringColumn(
        database,
        "SELECT action_name FROM card_actions WHERE title = ? ORDER BY item_index",
        title);
}

std::vector<std::string> loadActionDisplayNames(SQLite::Database& database, const std::string& title)
{
    const std::string valueExpression = columnExists(database, "card_actions", "display_name")
        ? "COALESCE(NULLIF(display_name, ''), action_name)"
        : "action_name";
    return loadStringColumn(
        database,
        "SELECT " + valueExpression + " FROM card_actions WHERE title = ? ORDER BY item_index",
        title);
}
}

std::vector<card_data::Action> loadActions(SQLite::Database& database)
{
    std::vector<card_data::Action> actions;
    const bool hasHeal = columnExists(database, "actions", "heal");
    const bool hasPush = columnExists(database, "actions", "push");
    const bool hasTargetFilters = tableExists(database, "action_target_filters");
    const std::string healExpression = hasHeal
        ? "heal"
        : "CASE WHEN damage < 0 THEN -damage ELSE 0 END";
    const std::string pushExpression = hasPush ? "push" : "0";
    SQLite::Statement query(
        database,
        "SELECT name, state, kind, pattern, min_range, max_range, "
        "CASE WHEN damage < 0 THEN 0 ELSE damage END, " + healExpression +
        ", can_move, can_attack, pass_through, line_of_sight, status_turns, cooldown_turns, " +
        pushExpression + " FROM actions ORDER BY name");
    while (query.executeStep())
    {
        card_data::Action action = actionFromQuery(query);
        if (hasTargetFilters)
        {
            action.targetFilter = loadActionTargetFilter(database, action.name);
        }
        actions.push_back(std::move(action));
    }
    return actions;
}

std::vector<card_data::Action> loadCardActions(SQLite::Database& database, const std::string& title)
{
    std::vector<card_data::Action> actions;
    const bool hasHeal = columnExists(database, "actions", "heal");
    const bool hasPush = columnExists(database, "actions", "push");
    const bool hasTargetFilters = tableExists(database, "action_target_filters");
    const std::string healExpression = hasHeal
        ? "a.heal"
        : "CASE WHEN a.damage < 0 THEN -a.damage ELSE 0 END";
    const std::string pushExpression = hasPush ? "a.push" : "0";
    SQLite::Statement query(
        database,
        "SELECT a.name, a.state, a.kind, a.pattern, a.min_range, a.max_range, "
        "CASE WHEN a.damage < 0 THEN 0 ELSE a.damage END, " + healExpression +
        ", a.can_move, a.can_attack, a.pass_through, a.line_of_sight, a.status_turns, a.cooldown_turns, " +
        pushExpression +
        " FROM card_actions ca JOIN actions a ON a.name = ca.action_name "
        "WHERE ca.title = ? ORDER BY ca.item_index");
    query.bind(1, title);
    while (query.executeStep())
    {
        card_data::Action action = actionFromQuery(query);
        if (hasTargetFilters)
        {
            action.targetFilter = loadActionTargetFilter(database, action.name);
        }
        actions.push_back(std::move(action));
    }
    return actions;
}

std::vector<card_data::Card> loadCards(SQLite::Database& database)
{
    std::vector<card_data::Card> cards;
    const bool hasTraitsTable = tableExists(database, "card_traits");
    const bool hasKeywordsTable = tableExists(database, "card_keywords");
    SQLite::Statement query(database, "SELECT title, type, image_path FROM cards ORDER BY title");
    while (query.executeStep())
    {
        card_data::Card card;
        card.title = query.getColumn(0).getString();
        card.type = query.getColumn(1).getString();
        card.imagePath = query.getColumn(2).getString();
        if (hasTraitsTable)
        {
            card.traits = loadStringColumn(
                database,
                "SELECT trait FROM card_traits WHERE title = ? ORDER BY trait",
                card.title);
        }
        else if (hasKeywordsTable)
        {
            // Read-only legacy databases still expose the old gating values
            // as traits until the card server performs its migration.
            card.traits = loadStringColumn(
                database,
                "SELECT keyword FROM card_keywords WHERE title = ? ORDER BY keyword",
                card.title);
        }
        if (hasTraitsTable && hasKeywordsTable)
        {
            card.keywords = loadStringColumn(
                database,
                "SELECT keyword FROM card_keywords WHERE title = ? ORDER BY keyword",
                card.title);
        }
        card.integerValues = loadIntegerValues(database, card.title);
        card.stringValues = loadStringValues(database, card.title);
        card.stringLists = loadStringLists(database, card.title);
        card.actionNames = loadActionNames(database, card.title);
        card.actionDisplayNames = loadActionDisplayNames(database, card.title);
        card.actions = loadCardActions(database, card.title);
        cards.push_back(std::move(card));
    }
    return cards;
}

std::vector<card_data::Card> loadCardsFromFile(const std::filesystem::path& databasePath)
{
    SQLite::Database database(databasePath.string(), SQLite::OPEN_READONLY);
    return loadCards(database);
}
}
