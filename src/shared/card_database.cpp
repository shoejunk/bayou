#include "card_database.hpp"
#include "game_data.hpp"
#include <stdexcept>

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
    action.control = std::max(0, query.getColumn(15).getInt());
    action.repeat = std::max(0, query.getColumn(16).getInt());
    action.nextState = query.getColumn(17).getInt();
    action.infest = query.getColumn(18).getString();
    action.pull = query.getColumn(19).getInt() != 0;
    action.lifedrain = query.getColumn(20).getInt() != 0;
    action.slow = std::max(0, query.getColumn(21).getInt());
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
    const bool hasControl = columnExists(database, "actions", "control");
    const bool hasRepeat = columnExists(database, "actions", "repeat");
    const bool hasNextState = columnExists(database, "actions", "next_state");
    const bool hasInfest = columnExists(database, "actions", "infest");
    const bool hasPull = columnExists(database, "actions", "pull");
    const bool hasLifedrain = columnExists(database, "actions", "lifedrain");
    const bool hasSlow = columnExists(database, "actions", "slow");
    const bool hasTargetFilters = tableExists(database, "action_target_filters");
    const std::string healExpression = hasHeal
        ? "heal"
        : "CASE WHEN damage < 0 THEN -damage ELSE 0 END";
    const std::string pushExpression = hasPush ? "push" : "0";
    const std::string controlExpression = hasControl ? "control" : "0";
    const std::string repeatExpression = hasRepeat ? "repeat" : "0";
    const std::string nextStateExpression = hasNextState ? "next_state" : "state";
    const std::string infestExpression = hasInfest ? "infest" : "''";
    const std::string pullExpression = hasPull ? "pull" : "0";
    const std::string lifedrainExpression = hasLifedrain ? "lifedrain" : "0";
    const std::string slowExpression = hasSlow ? "slow" : "0";
    SQLite::Statement query(
        database,
        "SELECT name, state, kind, pattern, min_range, max_range, "
        "CASE WHEN damage < 0 THEN 0 ELSE damage END, " + healExpression +
        ", can_move, can_attack, pass_through, line_of_sight, status_turns, cooldown_turns, " +
         pushExpression + ", " + controlExpression + ", " + repeatExpression + ", " + nextStateExpression + ", " + infestExpression + ", " + pullExpression + ", " + lifedrainExpression + ", " + slowExpression +
        " FROM actions ORDER BY name");
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
    const bool hasControl = columnExists(database, "actions", "control");
    const bool hasRepeat = columnExists(database, "actions", "repeat");
    const bool hasNextState = columnExists(database, "actions", "next_state");
    const bool hasInfest = columnExists(database, "actions", "infest");
    const bool hasPull = columnExists(database, "actions", "pull");
    const bool hasLifedrain = columnExists(database, "actions", "lifedrain");
    const bool hasSlow = columnExists(database, "actions", "slow");
    const bool hasTargetFilters = tableExists(database, "action_target_filters");
    const std::string healExpression = hasHeal
        ? "a.heal"
        : "CASE WHEN a.damage < 0 THEN -a.damage ELSE 0 END";
    const std::string pushExpression = hasPush ? "a.push" : "0";
    const std::string controlExpression = hasControl ? "a.control" : "0";
    const std::string repeatExpression = hasRepeat ? "a.repeat" : "0";
    const std::string nextStateExpression = hasNextState ? "a.next_state" : "a.state";
    const std::string infestExpression = hasInfest ? "a.infest" : "''";
    const std::string pullExpression = hasPull ? "a.pull" : "0";
    const std::string lifedrainExpression = hasLifedrain ? "a.lifedrain" : "0";
    const std::string slowExpression = hasSlow ? "a.slow" : "0";
    SQLite::Statement query(
        database,
        "SELECT a.name, a.state, a.kind, a.pattern, a.min_range, a.max_range, "
        "CASE WHEN a.damage < 0 THEN 0 ELSE a.damage END, " + healExpression +
        ", a.can_move, a.can_attack, a.pass_through, a.line_of_sight, a.status_turns, a.cooldown_turns, " +
         pushExpression + ", " + controlExpression + ", " + repeatExpression + ", " + nextStateExpression + ", " + infestExpression + ", " + pullExpression + ", " + lifedrainExpression + ", " + slowExpression +
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

void initializeCardPassives(SQLite::Database& database)
{
    SQLite::Transaction transaction(database);
    if (tableExists(database, "card_passives")) return;
    database.exec("CREATE TABLE card_passives (title TEXT NOT NULL, passive TEXT NOT NULL,"
                  "FOREIGN KEY(title) REFERENCES cards(title) ON DELETE CASCADE)");
    // Before the Traits table, card_keywords held eligibility traits and must
    // never be classified as behavior by the fallback migration.
    if (tableExists(database, "card_traits") && tableExists(database, "card_keywords"))
    {
        struct Row { long long id; std::string title, value; };
        std::vector<Row> rows;
        {
            SQLite::Statement query(database, "SELECT rowid,title,keyword FROM card_keywords");
            while (query.executeStep()) {
                std::string value=query.getColumn(2).getString();
                if (card_data::passiveKey(value)!="traces" && card_data::isSupportedPassive(value))
                    rows.push_back({query.getColumn(0).getInt64(),query.getColumn(1).getString(),std::move(value)});
            }
        }
        SQLite::Statement insert(database, "INSERT INTO card_passives (title,passive) VALUES (?,?)");
        SQLite::Statement remove(database, "DELETE FROM card_keywords WHERE rowid=?");
        for (const auto& row : rows) {
            insert.reset(); insert.bind(1,row.title); insert.bind(2,row.value); insert.exec();
            remove.reset(); remove.bind(1,static_cast<std::int64_t>(row.id)); remove.exec();
        }
    }
    transaction.commit();
}

std::vector<card_data::Card> loadCards(SQLite::Database& database)
{
    std::vector<card_data::Card> cards;
    const bool hasTraitsTable = tableExists(database, "card_traits");
    const bool hasKeywordsTable = tableExists(database, "card_keywords");
    const bool hasPassivesTable = tableExists(database, "card_passives");
    const bool hasMarkerTable = tableExists(database, "card_board_markers");
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
        if (hasPassivesTable)
            card.passives = loadStringColumn(database,
                "SELECT passive FROM card_passives WHERE title = ? ORDER BY passive", card.title);
        else if (hasTraitsTable && hasKeywordsTable)
            card_data::splitLegacyPassives(card);
        card_data::normalizeRescuePassives(card.passives);
        if (hasMarkerTable) {
            SQLite::Statement marker(database,"SELECT marker_name FROM card_board_markers WHERE title=?");
            marker.bind(1,card.title);
            if (marker.executeStep()) {
                card.tracesMarkerName=marker.getColumn(0).getString();
                card.tracesMarker=loadBoardMarker(database,card.tracesMarkerName);
                if (!card.tracesMarker) throw std::runtime_error("Invalid Board Marker reference for " + card.title);
            }
        }
        card.integerValues = loadIntegerValues(database, card.title);
        card.stringValues = loadStringValues(database, card.title);
        card.stringLists = loadStringLists(database, card.title);
        card.actionNames = loadActionNames(database, card.title);
        card.actionDisplayNames = loadActionDisplayNames(database, card.title);
        card.actions = loadCardActions(database, card.title);
        if (tableExists(database,"card_ability_object_migrations")) {
            SQLite::Statement known(database,"SELECT 1 FROM card_ability_object_migrations WHERE title=?");
            known.bind(1,card.title);card.abilityReferencesExplicit=known.executeStep();
        }
        if (tableExists(database, "card_activated_abilities"))
        {
            SQLite::Statement links(database, "SELECT slot, ability_name FROM card_activated_abilities WHERE title = ?");
            links.bind(1,card.title);
            while (links.executeStep())
            {
                const bool sacrifice=links.getColumn(0).getInt()!=0;
                const auto name=links.getColumn(1).getString();
                const auto definition=loadActivatedAbility(database,name);
                if (!definition) throw std::runtime_error("Missing activated ability: "+name);
                if (sacrifice) { card.sacrificeAbilityName=name; card.sacrificeAbility=definition; }
                else { card.activatedAbilityName=name; card.activatedAbility=definition; }
            }
        }
        cards.push_back(std::move(card));
    }
    return cards;
}

std::vector<card_data::Card> loadCardsFromFile(const std::filesystem::path& databasePath)
{
    SQLite::Database database(databasePath.string(), SQLite::OPEN_READONLY);
    return loadCards(database);
}

std::optional<ability_data::Definition> loadActivatedAbility(SQLite::Database& database, const std::string& name)
{
    if (name.empty() || !tableExists(database,"activated_abilities")) return std::nullopt;
    SQLite::Statement query(database,"SELECT definition FROM activated_abilities WHERE name = ?");
    query.bind(1,name);
    if (!query.executeStep()) return std::nullopt;
    sf::Packet packet;
    packet.append(query.getColumn(0).getBlob(),query.getColumn(0).getBytes());
    ability_data::Definition definition;
    if (!ability_data::read(packet,definition) || !packet.endOfPacket() || definition.name!=name)
        throw std::runtime_error("Invalid stored activated ability: "+name);
    return definition;
}

std::vector<ability_data::Definition> loadActivatedAbilities(SQLite::Database& database)
{
    std::vector<ability_data::Definition> result;
    if (!tableExists(database,"activated_abilities")) return result;
    SQLite::Statement names(database,"SELECT name FROM activated_abilities ORDER BY name");
    while (names.executeStep()) result.push_back(*loadActivatedAbility(database,names.getColumn(0).getString()));
    return result;
}

namespace
{
void storeAbility(SQLite::Database& database, const ability_data::Definition& definition)
{
    if (!ability_data::valid(definition,true)) throw std::runtime_error("Invalid activated ability definition");
    sf::Packet packet; ability_data::write(packet,definition);
    SQLite::Statement insert(database,"INSERT INTO activated_abilities(name,definition) VALUES(?,?) "
        "ON CONFLICT(name) DO UPDATE SET definition=excluded.definition");
    insert.bind(1,definition.name);
    insert.bind(2,packet.getData(),static_cast<int>(packet.getDataSize()));
    insert.exec();
}

void storeAbilityLinks(SQLite::Database& database, const std::string& title,
    const std::string& abilityName, const std::string& sacrificeName)
{
    for (const auto* name:{&abilityName,&sacrificeName})
        if (!name->empty() && !loadActivatedAbility(database,*name)) throw std::runtime_error("Unknown activated ability: "+*name);
    SQLite::Statement remove(database,"DELETE FROM card_activated_abilities WHERE title=?");
    remove.bind(1,title); remove.exec();
    SQLite::Statement insert(database,"INSERT INTO card_activated_abilities(title,slot,ability_name) VALUES(?,?,?)");
    for (int slot=0; slot<2; ++slot)
    {
        const auto& name=slot==0 ? abilityName : sacrificeName;
        if (name.empty()) continue;
        insert.reset(); insert.bind(1,title);insert.bind(2,slot);insert.bind(3,name);insert.exec();
    }
}

// Only called after an explicit definition edit. Initial migration retains
// every original field for rollback; edited definitions refresh that projection.
void refreshAbilityProjections(SQLite::Database& database, const std::string& name)
{
    for (const auto& original:loadCards(database))
    {
        if (original.activatedAbilityName!=name && original.sacrificeAbilityName!=name) continue;
        const auto card=card_data::legacyAbilityProjection(original);
        for (const auto* table:{"card_integer_values","card_string_values","card_string_lists"})
        {
            SQLite::Statement remove(database,std::string("DELETE FROM ")+table+" WHERE title=?");
            remove.bind(1,card.title);remove.exec();
        }
        SQLite::Statement ints(database,"INSERT INTO card_integer_values(title,key,value) VALUES(?,?,?)");
        for (const auto& field:card.integerValues)
        { ints.reset();ints.bind(1,card.title);ints.bind(2,field.key);ints.bind(3,field.value);ints.exec(); }
        SQLite::Statement strings(database,"INSERT INTO card_string_values(title,key,value) VALUES(?,?,?)");
        for (const auto& field:card.stringValues)
        { strings.reset();strings.bind(1,card.title);strings.bind(2,field.key);strings.bind(3,field.value);strings.exec(); }
        SQLite::Statement lists(database,"INSERT INTO card_string_lists(title,key,item_index,value) VALUES(?,?,?,?)");
        for (const auto& field:card.stringLists)
            for (std::size_t i=0;i<field.values.size();++i)
            { lists.reset();lists.bind(1,card.title);lists.bind(2,field.key);lists.bind(3,static_cast<int>(i));lists.bind(4,field.values[i]);lists.exec(); }
    }
}
}

void initializeActivatedAbilities(SQLite::Database& database)
{
    SQLite::Transaction transaction(database);
    database.exec("CREATE TABLE IF NOT EXISTS activated_abilities(name TEXT PRIMARY KEY NOT NULL,definition BLOB NOT NULL)");
    database.exec("CREATE TABLE IF NOT EXISTS card_activated_abilities(title TEXT NOT NULL,slot INTEGER NOT NULL CHECK(slot IN (0,1)),"
        "ability_name TEXT NOT NULL,PRIMARY KEY(title,slot),FOREIGN KEY(title) REFERENCES cards(title) ON DELETE CASCADE,"
        "FOREIGN KEY(ability_name) REFERENCES activated_abilities(name))");
    database.exec("CREATE TABLE IF NOT EXISTS card_ability_object_migrations(title TEXT PRIMARY KEY NOT NULL,FOREIGN KEY(title) REFERENCES cards(title) ON DELETE CASCADE)");
    for (const auto& card:loadCards(database))
    {
        if (card.abilityReferencesExplicit) continue;
        const auto gameCard=game_data::toGameCard(card);
        auto normalName=card.activatedAbilityName, sacrificeName=card.sacrificeAbilityName;
        bool changed=false;
        for (bool sacrifice:{false,true})
        {
            auto& name=sacrifice?sacrificeName:normalName;
            if (!name.empty()) continue;
            auto definition=game_data::legacyAbilityDefinition(gameCard,sacrifice);
            if (!definition) continue;
            const auto baseName=card.title+(sacrifice?" Sacrifice":" Ability");
            name=baseName;
            for (int suffix=2;loadActivatedAbility(database,name);++suffix) name=baseName+" ("+std::to_string(suffix)+")";
            definition->name=name;
            storeAbility(database,*definition);changed=true;
        }
        if (changed) storeAbilityLinks(database,card.title,normalName,sacrificeName);
        SQLite::Statement mark(database,"INSERT INTO card_ability_object_migrations(title) VALUES(?)");mark.bind(1,card.title);mark.exec();
    }
    transaction.commit();
}

void saveActivatedAbility(SQLite::Database& database, const ability_data::Definition& definition, const std::string& originalName)
{
    SQLite::Transaction transaction(database);
    if (!originalName.empty() && !loadActivatedAbility(database,originalName)) throw std::runtime_error("Activated ability no longer exists");
    if (!originalName.empty() && originalName!=definition.name && loadActivatedAbility(database,definition.name))
        throw std::runtime_error("An activated ability with that name already exists");
    storeAbility(database,definition);
    if (!originalName.empty() && originalName!=definition.name)
    {
        SQLite::Statement links(database,"UPDATE card_activated_abilities SET ability_name=? WHERE ability_name=?");
        links.bind(1,definition.name);links.bind(2,originalName);links.exec();
        SQLite::Statement remove(database,"DELETE FROM activated_abilities WHERE name=?");remove.bind(1,originalName);remove.exec();
    }
    refreshAbilityProjections(database,definition.name);
    transaction.commit();
}

void deleteActivatedAbility(SQLite::Database& database, const std::string& name)
{
    SQLite::Transaction transaction(database);
    SQLite::Statement references(database,"SELECT 1 FROM card_activated_abilities WHERE ability_name=? LIMIT 1");
    references.bind(1,name);
    if (references.executeStep()) throw std::runtime_error("This activated ability is assigned to a card; remove its assignments first");
    SQLite::Statement remove(database,"DELETE FROM activated_abilities WHERE name=?");remove.bind(1,name);remove.exec();
    transaction.commit();
}

void setCardActivatedAbilities(SQLite::Database& database, const std::string& title, const std::string& abilityName, const std::string& sacrificeName, bool authoritative)
{
    // Participates in the card server's existing card-save transaction.
    storeAbilityLinks(database,title,abilityName,sacrificeName);
    if (authoritative) {
        SQLite::Statement mark(database,"INSERT OR IGNORE INTO card_ability_object_migrations(title) VALUES(?)");mark.bind(1,title);mark.exec();
    }
}
void initializeBoardMarkers(SQLite::Database& database)
{
    SQLite::Transaction txn(database);
    database.exec("CREATE TABLE IF NOT EXISTS board_markers (name TEXT NOT NULL PRIMARY KEY COLLATE NOCASE,"
        "eligibility INTEGER NOT NULL CHECK(eligibility BETWEEN 0 AND 2),"
        "effect INTEGER NOT NULL CHECK(effect BETWEEN 0 AND 1),"
        "strength INTEGER NOT NULL CHECK(strength > 0));"
        "CREATE TABLE IF NOT EXISTS card_board_markers (title TEXT NOT NULL PRIMARY KEY,"
        "marker_name TEXT NOT NULL COLLATE NOCASE,"
        "FOREIGN KEY(title) REFERENCES cards(title) ON DELETE CASCADE,"
        "FOREIGN KEY(marker_name) REFERENCES board_markers(name) ON UPDATE CASCADE ON DELETE RESTRICT)");
    if (!columnExists(database,"board_markers","image_path"))
        database.exec("ALTER TABLE board_markers ADD COLUMN image_path TEXT NOT NULL DEFAULT ''");
    txn.commit();
}

static marker_data::Definition boardMarkerFromRow(SQLite::Statement& rows)
{
    const int eligibility=rows.getColumn(1).getInt(), effect=rows.getColumn(2).getInt();
    marker_data::Definition result{rows.getColumn(0).getString(),
        static_cast<marker_data::Eligibility>(eligibility),static_cast<marker_data::Effect>(effect),
        rows.getColumn(3).getInt(), rows.getColumn(4).getString()};
    if (eligibility<0 || eligibility>2 || effect<0 || effect>1 || !marker_data::valid(result))
        throw std::runtime_error("Invalid stored Board Marker: "+result.name);
    return result;
}

std::vector<marker_data::Definition> loadBoardMarkers(SQLite::Database& database)
{
    SQLite::Statement rows(database,std::string("SELECT name,eligibility,effect,strength,") +
        (columnExists(database,"board_markers","image_path") ? "image_path" : "''") + " FROM board_markers ORDER BY name");
    std::vector<marker_data::Definition> result;
    while (rows.executeStep()) result.push_back(boardMarkerFromRow(rows));
    return result;
}

std::optional<marker_data::Definition> loadBoardMarker(SQLite::Database& database,const std::string& name)
{
    if (name.empty()) return std::nullopt;
    SQLite::Statement rows(database,std::string("SELECT name,eligibility,effect,strength,") +
        (columnExists(database,"board_markers","image_path") ? "image_path" : "''") + " FROM board_markers WHERE name=?");
    rows.bind(1,name);
    if (!rows.executeStep()) return std::nullopt;
    return boardMarkerFromRow(rows);
}

void saveBoardMarker(SQLite::Database& database,const marker_data::Definition& marker,const std::string& originalName)
{
    if (!marker_data::valid(marker)) throw std::runtime_error("Invalid Board Marker definition");
    SQLite::Transaction txn(database);
    if (originalName.empty()) {
        if (auto defined=loadBoardMarker(database,marker.name); defined && defined->name!=marker.name)
            throw std::runtime_error("A Board Marker already uses this name");
        SQLite::Statement query(database,"INSERT INTO board_markers(name,eligibility,effect,strength,image_path) VALUES(?,?,?,?,?) "
            "ON CONFLICT(name) DO UPDATE SET eligibility=excluded.eligibility,effect=excluded.effect,strength=excluded.strength,image_path=excluded.image_path");
        query.bind(1,marker.name); query.bind(2,static_cast<int>(marker.eligibility));
        query.bind(3,static_cast<int>(marker.effect)); query.bind(4,marker.strength); query.bind(5,marker.imagePath); query.exec();
    } else {
        SQLite::Statement query(database,"UPDATE board_markers SET name=?,eligibility=?,effect=?,strength=?,image_path=? WHERE name=?");
        query.bind(1,marker.name); query.bind(2,static_cast<int>(marker.eligibility));
        query.bind(3,static_cast<int>(marker.effect)); query.bind(4,marker.strength); query.bind(5,marker.imagePath); query.bind(6,originalName);
        if (query.exec()!=1) throw std::runtime_error("Board Marker not found");
        // Case-only spelling changes are equal to SQLite NOCASE so its cascade
        // is not fired. Canonicalize every persisted reference explicitly.
        SQLite::Statement references(database,"UPDATE card_board_markers SET marker_name=? WHERE marker_name=? COLLATE NOCASE");
        references.bind(1,marker.name); references.bind(2,originalName); references.exec();
    }
    txn.commit();
}

void deleteBoardMarker(SQLite::Database& database,const std::string& name)
{
    SQLite::Transaction txn(database);
    SQLite::Statement refs(database,"SELECT 1 FROM card_board_markers WHERE marker_name=? LIMIT 1");
    refs.bind(1,name);
    if (refs.executeStep()) throw std::runtime_error("Board Marker is assigned to a card");
    SQLite::Statement remove(database,"DELETE FROM board_markers WHERE name=?");
    remove.bind(1,name);
    if (remove.exec()!=1) throw std::runtime_error("Board Marker not found");
    txn.commit();
}

void setCardBoardMarker(SQLite::Database& database,const std::string& title,const std::string& name)
{
    const auto definition=loadBoardMarker(database,name);
    if (!name.empty() && !definition) throw std::runtime_error("Unknown Board Marker reference");
    SQLite::Statement remove(database,"DELETE FROM card_board_markers WHERE title=?");
    remove.bind(1,title); remove.exec();
    if (!definition) return;
    SQLite::Statement link(database,"INSERT INTO card_board_markers (title,marker_name) VALUES (?,?)");
    link.bind(1,title); link.bind(2,definition->name); link.exec();
}
}
