#include "purify_tests.hpp"
#include "rooted_tests.hpp"
#include "dash_rescue_engine_tests.hpp"
#include "trace_marker_tests.hpp"
#include "board_marker_wire_tests.hpp"
#include "board_marker_catalog_tests.hpp"
#include "dash_rescue_wire_tests.hpp"
#include "runtime_passive_tests.hpp"
#include "passives_catalog_tests.hpp"
#include "blight_engine_tests.hpp"
#include "trap_snapshot_tests.hpp"
#include "corrupt_ground_tests.hpp"
#include "../client/client_sandbox.hpp"
#include "../gameserver/game_engine.hpp"
#include "../shared/card_database.hpp"
#include "../shared/game_data.hpp"
#include <fmt/core.h>
#include <fstream>
#include <iterator>

namespace
{
int failures = 0, checks = 0;
void check(bool result, const char *message)
{
    ++checks;
    if (!result)
    {
        ++failures;
        fmt::println("[FAIL] {}", message);
    }
}
void schema(SQLite::Database &db)
{
    db.exec(R"SQL(
        PRAGMA foreign_keys=ON;
        CREATE TABLE cards(title TEXT PRIMARY KEY,type TEXT,image_path TEXT);
        CREATE TABLE card_traits(title TEXT,trait TEXT);
        CREATE TABLE card_keywords(title TEXT,keyword TEXT);
        CREATE TABLE card_integer_values(title TEXT,key TEXT,value INTEGER);
        CREATE TABLE card_string_values(title TEXT,key TEXT,value TEXT);
        CREATE TABLE card_string_lists(title TEXT,key TEXT,item_index INTEGER,value TEXT);
        CREATE TABLE card_actions(title TEXT,action_name TEXT,display_name TEXT,item_index INTEGER);
        CREATE TABLE actions(name TEXT PRIMARY KEY,state INTEGER,kind TEXT,pattern TEXT,min_range INTEGER,max_range INTEGER,
            damage INTEGER,heal INTEGER,can_move INTEGER,can_attack INTEGER,pass_through INTEGER,line_of_sight INTEGER,
            status_turns INTEGER,cooldown_turns INTEGER,push INTEGER,control INTEGER,repeat INTEGER,next_state INTEGER,
            infest TEXT,pull INTEGER,lifedrain INTEGER,slow INTEGER);
        CREATE TABLE action_target_filters(action_name TEXT,item_index INTEGER,value TEXT);
    )SQL");
}
void insert(SQLite::Database &db, const card_data::Card &card)
{
    SQLite::Statement root(db, "INSERT INTO cards VALUES(?,?,?)");
    root.bind(1, card.title);
    root.bind(2, card.type);
    root.bind(3, card.imagePath);
    root.exec();
    for (const auto &value : card.traits)
    {
        SQLite::Statement q(db, "INSERT INTO card_traits VALUES(?,?)");
        q.bind(1, card.title);
        q.bind(2, value);
        q.exec();
    }
    for (const auto &value : card.keywords)
    {
        SQLite::Statement q(db, "INSERT INTO card_keywords VALUES(?,?)");
        q.bind(1, card.title);
        q.bind(2, value);
        q.exec();
    }
    for (const auto &field : card.integerValues)
    {
        SQLite::Statement q(db, "INSERT INTO card_integer_values VALUES(?,?,?)");
        q.bind(1, card.title);
        q.bind(2, field.key);
        q.bind(3, field.value);
        q.exec();
    }
    for (const auto &field : card.stringValues)
    {
        SQLite::Statement q(db, "INSERT INTO card_string_values VALUES(?,?,?)");
        q.bind(1, card.title);
        q.bind(2, field.key);
        q.bind(3, field.value);
        q.exec();
    }
    for (const auto &field : card.stringLists)
        for (std::size_t i = 0; i < field.values.size(); ++i)
        {
            SQLite::Statement q(db, "INSERT INTO card_string_lists VALUES(?,?,?,?)");
            q.bind(1, card.title);
            q.bind(2, field.key);
            q.bind(3, static_cast<int>(i));
            q.bind(4, field.values[i]);
            q.exec();
        }
    for (std::size_t i = 0; i < card.actions.size(); ++i)
    {
        const auto &a = card.actions[i];
        SQLite::Statement q(db, "INSERT OR IGNORE INTO actions VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
        q.bind(1, a.name);
        q.bind(2, a.state);
        q.bind(3, a.kind);
        q.bind(4, a.pattern);
        q.bind(5, a.minRange);
        q.bind(6, a.maxRange);
        q.bind(7, a.damage);
        q.bind(8, a.heal);
        q.bind(9, a.canMove);
        q.bind(10, a.canAttack);
        q.bind(11, a.passThrough);
        q.bind(12, a.lineOfSight);
        q.bind(13, a.statusTurns);
        q.bind(14, a.cooldownTurns);
        q.bind(15, a.push);
        q.bind(16, a.control);
        q.bind(17, a.repeat);
        q.bind(18, card_data::actionNextState(a));
        q.bind(19, a.infest);
        q.bind(20, a.pull);
        q.bind(21, a.lifedrain);
        q.bind(22, a.slow);
        q.exec();
        for (std::size_t j = 0; j < a.targetFilter.size(); ++j)
        {
            SQLite::Statement f(db, "INSERT INTO action_target_filters SELECT ?,?,? WHERE NOT EXISTS(SELECT 1 FROM "
                                    "action_target_filters WHERE action_name=? AND item_index=?)");
            f.bind(1, a.name);
            f.bind(2, static_cast<int>(j));
            f.bind(3, a.targetFilter[j]);
            f.bind(4, a.name);
            f.bind(5, static_cast<int>(j));
            f.exec();
        }
        SQLite::Statement link(db, "INSERT INTO card_actions VALUES(?,?,?,?)");
        link.bind(1, card.title);
        link.bind(2, a.name);
        link.bind(3, i < card.actionDisplayNames.size() ? card.actionDisplayNames[i] : a.name);
        link.bind(4, static_cast<int>(i));
        link.exec();
    }
}
std::string gameplay(card_data::Card card)
{
    auto game = game_data::toGameCard(card);
    if (auto definition = game_data::abilityDefinition(game))
        game_data::applyAbilityDefinition(game, *definition);
    sf::Packet packet;
    game_data::writeGameCard(packet, game, true, true, true, false);
    return std::string(static_cast<const char *>(packet.getData()), packet.getDataSize());
}
void migration(const std::vector<card_data::Card> &cards)
{
    SQLite::Database db(":memory:", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    schema(db);
    for (const auto &card : cards)
        insert(db, card);
    const auto before = card_database::loadCards(db);
    card_database::initializeActivatedAbilities(db);
    auto after = card_database::loadCards(db);
    check(before.size() == after.size(), "migration preserves catalog cardinality");
    for (std::size_t i = 0; i < before.size() && i < after.size(); ++i)
    {
        check(gameplay(before[i]) == gameplay(after[i]),
              "migrated card retains its normalized runtime fields and actions");
        for (bool sacrifice : {false, true})
        {
            const auto expected = game_data::legacyAbilityDefinition(game_data::toGameCard(before[i]), sacrifice);
            auto actual = sacrifice ? after[i].sacrificeAbility : after[i].activatedAbility;
            if (actual)
                actual->name.clear();
            check(actual == expected, "migration carries every ability setting and explicit state transition");
        }
    }
    const auto definitions = card_database::loadActivatedAbilities(db);
    card_database::initializeActivatedAbilities(db);
    check(card_database::loadActivatedAbilities(db) == definitions, "migration is idempotent");
    if (!after.empty())
    {
        const auto original = after.front();
        {
            SQLite::Transaction transaction(db);
            card_database::setCardActivatedAbilities(db, original.title, "", "");
            transaction.commit();
        }
        card_database::initializeActivatedAbilities(db);
        const auto cleared = card_database::loadCards(db).front();
        check(cleared.abilityReferencesExplicit && !cleared.activatedAbility && !cleared.sacrificeAbility &&
                  game_data::abilityKind(game_data::toGameCard(cleared)).empty(),
              "explicitly clearing assignments remains cleared after migration runs again");
        {
            SQLite::Transaction transaction(db);
            card_database::setCardActivatedAbilities(db, original.title, original.activatedAbilityName,
                                                     original.sacrificeAbilityName);
            transaction.commit();
        }
    }
    if (!definitions.empty())
    {
        auto changed = definitions.front();
        const auto oldName = changed.name;
        changed.name += " renamed";
        changed.strength = 9;
        card_database::saveActivatedAbility(db, changed, oldName);
        check(!card_database::loadActivatedAbility(db, oldName) &&
                  card_database::loadActivatedAbility(db, changed.name) == changed,
              "definition rename preserves the reusable object and retargets references atomically");
        bool blocked = false;
        try
        {
            card_database::deleteActivatedAbility(db, changed.name);
        }
        catch (...)
        {
            blocked = true;
        }
        check(blocked, "deleting an assigned definition is rejected");
        auto invalid = changed;
        invalid.kind = "undefined";
        blocked = false;
        try
        {
            card_database::saveActivatedAbility(db, invalid, changed.name);
        }
        catch (...)
        {
            blocked = true;
        }
        check(blocked && card_database::loadActivatedAbility(db, changed.name) == changed,
              "invalid edit rolls back without corrupting a definition");
    }
    SQLite::Statement integrity(db, "PRAGMA integrity_check");
    integrity.executeStep();
    check(integrity.getColumn(0).getString() == "ok", "migrated database passes integrity_check");
}
} // namespace

int main(int argc, char **argv)
{
    testTrapSnapshotWire(check);
    testCorruptGroundPrimitives(check);
    testBlightEngine(check);
    testPurifyingRadiance(check);
    testRooted(check);
    testDashRescueEngine(check);
    testTraceMarkers(check);
    testBoardMarkerWire(check);
    testBoardMarkerCatalog(check);
    testDashRescueWire(check);
    testRuntimePassives(check);
    testPassivesCatalog(check);
    using namespace game_data;
    card_data::Card legacy;
    legacy.title = "Ability migration fixture";
    legacy.traits = {"fey"};
    legacy.integerValues = {
        {"health", 4}, {"cost", 12}, {"ability range", 3}, {"ability strength", 2}, {"ability duration", 4}};
    legacy.stringValues = {{"ability", "area heal"}, {"CommandRestriction", "Fey"}, {"detail", "preserve this"}};
    legacy.stringLists = {{"heal filter", {"Fey", "Mechanical"}}, {"abilityLabels", {"Repair"}}};
    card_data::Action step;
    step.name = "fixture step";
    step.maxRange = 1;
    legacy.actions = {step};
    legacy.actionNames = {step.name};
    card_data::Card transform = legacy;
    transform.title = "Transform fixture";
    transform.stringValues = {{"ability", "transform"}};
    transform.stringLists = {{"abilityLabels", {"Raise Gun", "Lower Gun"}}};
    transform.actions[0].state = 1;
    card_data::Card sacrifice = legacy;
    sacrifice.title = "Sacrifice fixture";
    sacrifice.stringValues = {{"Sacrifice", "shriek"}};
    migration({legacy, transform, sacrifice});

    auto definition = *legacyAbilityDefinition(toGameCard(legacy));
    definition.name = "Reusable heal";
    sf::Packet data;
    ability_data::write(data, definition);
    ability_data::Definition decoded;
    check(ability_data::read(data, decoded) && data.endOfPacket() && decoded == definition,
          "definition roundtrip retains ranges, optional filters, labels and restrictions");
    auto empty = definition;
    empty.targetFilter = std::vector<std::string>{};
    sf::Packet zero;
    ability_data::write(zero, empty);
    check(ability_data::read(zero, decoded) && decoded.targetFilter && decoded.targetFilter->empty(),
          "explicit empty filter remains distinct from an omitted filter");
    sf::Packet truncated;
    truncated.append(data.getData(), data.getDataSize() - 1);
    check(!ability_data::read(truncated, decoded), "truncated definition is rejected");
    auto bad = *legacyAbilityDefinition(toGameCard(transform));
    bad.states[1].nextState = 15;
    check(!ability_data::valid(bad), "undefined state transitions are rejected");

    legacy.activatedAbilityName = definition.name;
    legacy.activatedAbility = definition;
    sf::Packet cardPacket;
    card_data::writeCard(cardPacket, legacy, true, true);
    card_data::Card native;
    check(card_data::readCard(cardPacket, native, true, true, true, true, true, true, true, true) &&
              cardPacket.endOfPacket() && native.activatedAbility == definition && native.stringLists.empty(),
          "native card wire format keeps settings in its definition instead of generic arrays");
    const auto actor = toGameCard(native);
    GameCard friendCard;
    friendCard.title = "Healing recipient";
    friendCard.type = "Unit";
    friendCard.health = 8;
    friendCard.traits = {"Mechanical"};
    GameEngine engine(2011, {});
    check(engine.loadScenario({{1, actor, 3, 3, false}, {1, friendCard, 3, 5, false, 2}}, {}, {}, 0, 0, 1,
                              "Native area heal", false) &&
              engine.useAbility(1, engine.boardPieces()[0].id) && engine.boardPieces()[1].health == 4,
          "native definition drives the authoritative ability with its range, strength and string-array filter");
    Snapshot snapshot = engine.snapshotFor(1);
    sf::Packet snapshotPacket;
    writeSnapshot(snapshotPacket, snapshot);
    Snapshot received;
    check(readSnapshot(snapshotPacket, received) && received.pieces[0].activatedAbility == definition,
          "negotiated snapshot preserves complete native ability definitions");
    sf::Packet old;
    writeSnapshot(old, snapshot, true, true, true, true, true, true, true, true, true, true, false);
    check(readSnapshot(old, received) && !received.pieces[0].activatedAbility,
          "older snapshot projection remains parseable without native fields");
    auto nativeTransform = toGameCard(transform);
    nativeTransform.activatedAbility = *legacyAbilityDefinition(nativeTransform);
    nativeTransform.activatedAbility->states = {{0, 1, "Raise"}, {1, 0, "Lower"}};
    Piece form;
    populatePieceFromCard(form, nativeTransform, false);
    advanceActivatedAbilityState(form);
    check(form.actionState == 1 && pieceAbilityLabel(form) == "Lower",
          "native state selects the alternate button label");
    advanceActivatedAbilityState(form);
    check(form.actionState == 0, "native Transform remains reversible from its alternate state");
    card_data::Card mangle;
    mangle.title = "Maggie Mudroot";
    check(abilityKind(toGameCard(mangle)) == "raise undead",
          "legacy built-in ability remains available before migration");
    mangle.abilityReferencesExplicit = true;
    check(abilityKind(toGameCard(mangle)).empty(), "explicit native none overrides a legacy hard-coded ability");
    auto sacrificeActor = actor;
    auto sacrificeDefinition = definition;
    sacrificeDefinition.name = "Sacrifice Heal";
    sacrificeDefinition.strength = 5;
    sacrificeActor.sacrificeAbility = sacrificeDefinition;
    sacrificeActor.sacrifice = sacrificeDefinition.kind;
    sacrificeActor.passives = {"Undying"};
    GameEngine sacrificeEngine(2012, {});
    check(sacrificeEngine.loadScenario({{1, sacrificeActor, 3, 3, false}, {1, friendCard, 3, 5, false, 2}}, {}, {}, 0,
                                       0, 1, "Native sacrifice", false) &&
              sacrificeEngine.useAbility(1, sacrificeEngine.boardPieces()[0].id, true) &&
              sacrificeEngine.boardPieces()[1].health == 7 && sacrificeEngine.boardPieces()[0].health == 1,
          "sacrifice uses its own definition settings and retains ordinary defeat handling");
    auto digCard = actor;
    auto dig = definition;
    dig.kind = "dig";
    dig.initialUses = 1;
    digCard.activatedAbility = dig;
    digCard.ability = "dig";
    dig.initialUses = 2;
    digCard.sacrificeAbility = dig;
    digCard.sacrifice = "dig";
    digCard.passives = {"Undying"};
    GameEngine digging(2013, {});
    check(digging.loadScenario({{1, digCard, 3, 3, false}}, {}, {}, 0, 0, 1, "Independent charges", false) &&
              digging.useAbility(1, digging.boardPieces()[0].id, true) &&
              digging.boardPieces()[0].sacrificeAbilityUses == 1 && digging.boardPieces()[0].abilityUses == 1,
          "normal and sacrifice abilities retain independent per-piece use counts");
    Piece commander;
    populatePieceFromCard(commander, actor, false);
    commander.id = 10;
    commander.owner = 1;
    commander.row = 3;
    commander.column = 3;
    auto command = definition;
    command.kind = "command";
    command.range = 1;
    command.commandRestriction = "Fey";
    applyAbilityDefinition(commander, command);
    command.range = 3;
    command.commandRestriction = "Mechanical";
    commander.sacrificeAbility = command;
    commander.sacrificePending = true;
    Piece recipient;
    populatePieceFromCard(recipient, friendCard, false);
    recipient.id = 11;
    recipient.owner = 1;
    recipient.row = 3;
    recipient.column = 5;
    check(pieceCanReceiveCommand(commander, recipient) &&
              commandTargetQualifier(commander).find("Mechanical") != std::string::npos,
          "pending sacrifice Command eligibility and prompt use the sacrifice definition");
    for (bool sacrifice : {false, true})
    {
        card_data::Card trail;
        trail.title = "Independent Trail";
        trail.type = "Unit";
        trail.passives = {"Trail"};
        trail.stringValues = {{"summon", "Trail Sapling"}};
        auto summon = definition;
        summon.name = "Active Summon";
        summon.kind = "summon";
        summon.summonTitle = "Active Sapling";
        if (sacrifice)
        {
            trail.sacrificeAbility = summon;
            trail.sacrificeAbilityName = summon.name;
        }
        else
        {
            trail.activatedAbility = summon;
            trail.activatedAbilityName = summon.name;
        }
        sf::Packet wire;
        card_data::writeCard(wire, trail, true, true);
        card_data::Card decodedTrail;
        check(card_data::readCard(wire, decodedTrail, true, true, true, true, true, true, true, true),
              "native Trail card reads its ability objects");
        const auto trailCard = toGameCard(decodedTrail);
        Piece trailPiece;
        populatePieceFromCard(trailPiece, trailCard, false);
        check(trailPiece.summonTitle == "Trail Sapling" && pieceHasTrailAbility(trailPiece) &&
                  activatedAbilitySource(trailPiece, sacrifice).summonTitle == "Active Sapling",
              "native normal/sacrifice Summon targets stay independent of the unit's passive Trail target");
    }
    auto spent = digging.boardPieces()[0];
    spent.sacrificeAbilityUses = 0;
    check(activatedAbilitySource(spent, true).abilityUses == 0 && activatedAbilitySource(spent).abilityUses == 1,
          "ability detail context shows the selected slot's actual remaining charges");
    auto defaults = actor;
    ability_data::Definition defaultDefinition;
    defaultDefinition.kind = "weaken";
    defaults.activatedAbility = defaultDefinition;
    defaults.abilityRange = 6;
    defaults.abilityStrength = 9;
    defaults.abilityDuration = 7;
    check(activatedRange(defaults) == 1 && activatedStrength(defaults) == 1 && activatedDuration(defaults) == 1,
          "native default settings cannot leak unrelated legacy projection values");
    if (argc == 2)
    {
        std::ifstream file(argv[1], std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(file)), {});
        sf::Packet packet;
        packet.append(bytes.data(), bytes.size());
        std::uint32_t count = 0;
        bool legacyFormat = false;
        bool next = false, control = false, repeat = false, infest = false, pull = false, lifedrain = false,
             slow = false, objects = false;
        check(card_data::readCardListHeader(packet, count, legacyFormat, &next, &control, &repeat, &infest, &pull,
                                            &lifedrain, &slow, &objects),
              "archived catalog header reads");
        std::vector<card_data::Card> cards;
        for (std::uint32_t i = 0; i < count; ++i)
        {
            card_data::Card card;
            if (!card_data::readListedCard(packet, card, legacyFormat, next, control, repeat, infest, pull, lifedrain,
                                           slow, objects))
            {
                check(false, "archived card parses");
                break;
            }
            cards.push_back(std::move(card));
        }
        check(packet.endOfPacket() && cards.size() == count, "archived catalog consumed completely");
        migration(cards);
        fmt::println("Archived migration coverage: {} cards", cards.size());
    }
    if (argc == 5 && std::string(argv[1]) == "--verify-fey-foresight")
    {
        const auto before=card_database::loadCardsFromFile(argv[2]);
        const auto after=card_database::loadCardsFromFile(argv[3]);
        const auto find=[](const auto& cards,const std::string& title) -> const card_data::Card& {
            const auto it=std::find_if(cards.begin(),cards.end(),[&](const auto& c){return c.title==title;});
            if(it==cards.end()) throw std::runtime_error("Missing rehearsal card: "+title);
            return *it;
        };
        const auto& reference=find(after,"Archivist Mosswake");
        const auto& messenger=find(after,"Fey Messenger");
        check(before.size()==after.size(),"Fey assignment preserves catalog size");
        for(const auto& card:before) {
            auto expected=card;
            if(card.title=="Fey Messenger") expected.passives=reference.passives;
            sf::Packet a,b;
            card_data::writeCard(a,expected,true,true,true,true,true);
            card_data::writeCard(b,find(after,card.title),true,true,true,true,true);
            check(a.getDataSize()==b.getDataSize() &&
                std::equal(static_cast<const char*>(a.getData()),static_cast<const char*>(a.getData())+a.getDataSize(),
                    static_cast<const char*>(b.getData())),"catalog readback preserves every unrelated card field/action");
        }
        const auto unit=game_data::toGameCard(messenger);
        check(unit.passives==std::vector<std::string>{"foresight"} && unit.ability.empty() && unit.sacrifice.empty(),
            "Fey Messenger uses Mosswake's Foresight passive without an activated or sacrifice ability");
        GameEngine engine(77,after);
        engine.loadScenario({{1,unit,3,3,false}}, {}, {},100,100,1,"Fey Messenger Foresight");
        card_data::Card first; first.title="First draw"; first.type="Unit";
        card_data::Card second=first; second.title="Second draw";
        engine.submitDeck(1,{first,second});
        check(engine.drawCard(1) && engine.snapshotFor(1).foresightChoices.size()==2 &&
            engine.playerState(1).resources==50 && engine.snapshotFor(2).foresightChoices.empty(),
            "Fey Messenger's normal paid draw offers two private choices for 50 Resources");
        const auto chosen=engine.snapshotFor(1).foresightChoices.at(1).title;
        check(!engine.endTurn(1) && engine.chooseForesightCard(1,1) &&
            engine.playerState(1).hand.back().title==chosen && engine.playerState(1).drawPile.size()==1 &&
            !engine.snapshotFor(1).players[0].pieceActionUsedThisTurn,
            "Fey Messenger Foresight choice resolves without spending either regular action");
        sf::Packet fixture; game_data::writeGameCard(fixture,unit);
        std::ofstream output(argv[4],std::ios::binary);
        output.write(static_cast<const char*>(fixture.getData()),fixture.getDataSize());
        check(static_cast<bool>(output),"isolated catalog card exported for normal client renderer");
        fmt::println("Fey Messenger isolated catalog comparison: {} cards",after.size());
    }
    if (argc == 4 && std::string(argv[1]) == "--compare-db")
    {
        const auto before = card_database::loadCardsFromFile(argv[2]);
        const auto after = card_database::loadCardsFromFile(argv[3]);
        check(before.size() == after.size(), "database migration preserves catalog cardinality");
        for (std::size_t i = 0; i < before.size() && i < after.size(); ++i)
        {
            const auto label = before[i].title + ": database migration preserves runtime fields and actions";
            check(gameplay(before[i]) == gameplay(after[i]), label.c_str());
            check(after[i].abilityReferencesExplicit, "every migrated card has an explicit assignment marker");
            for (bool sacrifice : {false, true})
            {
                const auto expected = game_data::legacyAbilityDefinition(game_data::toGameCard(before[i]), sacrifice);
                auto actual = sacrifice ? after[i].sacrificeAbility : after[i].activatedAbility;
                if (actual)
                    actual->name.clear();
                const auto detail = before[i].title + (sacrifice ? ": sacrifice definition preserved" : ": normal definition preserved");
                check(actual == expected, detail.c_str());
            }
        }
        fmt::println("Database migration comparison: {} cards", before.size());
    }
    fmt::println("Ability object checks: {}, failures: {}", checks, failures);
    return failures ? 1 : 0;
}
