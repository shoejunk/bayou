#include "card_database.hpp"

#include <map>
#include <string>
#include <utility>

namespace card_database
{
namespace
{
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
    action.canMove = query.getColumn(7).getInt() != 0;
    action.canAttack = query.getColumn(8).getInt() != 0;
    action.passThrough = query.getColumn(9).getInt() != 0;
    action.lineOfSight = query.getColumn(10).getInt() != 0;
    action.statusTurns = query.getColumn(11).getInt();
    action.cooldownTurns = query.getColumn(12).getInt();
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
}

std::vector<card_data::Action> loadActions(SQLite::Database& database)
{
    std::vector<card_data::Action> actions;
    SQLite::Statement query(
        database,
        "SELECT name, state, kind, pattern, min_range, max_range, damage, can_move, can_attack, "
        "pass_through, line_of_sight, status_turns, cooldown_turns FROM actions ORDER BY name");
    while (query.executeStep())
    {
        actions.push_back(actionFromQuery(query));
    }
    return actions;
}

std::vector<card_data::Action> loadCardActions(SQLite::Database& database, const std::string& title)
{
    std::vector<card_data::Action> actions;
    SQLite::Statement query(
        database,
        "SELECT a.name, a.state, a.kind, a.pattern, a.min_range, a.max_range, a.damage, "
        "a.can_move, a.can_attack, a.pass_through, a.line_of_sight, a.status_turns, a.cooldown_turns "
        "FROM card_actions ca JOIN actions a ON a.name = ca.action_name "
        "WHERE ca.title = ? ORDER BY ca.item_index");
    query.bind(1, title);
    while (query.executeStep())
    {
        actions.push_back(actionFromQuery(query));
    }
    return actions;
}

std::vector<card_data::Card> loadCards(SQLite::Database& database)
{
    std::vector<card_data::Card> cards;
    SQLite::Statement query(database, "SELECT title, type, image_path FROM cards ORDER BY title");
    while (query.executeStep())
    {
        card_data::Card card;
        card.title = query.getColumn(0).getString();
        card.type = query.getColumn(1).getString();
        card.imagePath = query.getColumn(2).getString();
        card.keywords = loadStringColumn(
            database,
            "SELECT keyword FROM card_keywords WHERE title = ? ORDER BY keyword",
            card.title);
        card.integerValues = loadIntegerValues(database, card.title);
        card.stringValues = loadStringValues(database, card.title);
        card.stringLists = loadStringLists(database, card.title);
        card.actionNames = loadActionNames(database, card.title);
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
