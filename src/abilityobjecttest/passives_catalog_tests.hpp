#pragma once
#include "../shared/card_database.hpp"

template <typename Check>
void testPassivesCatalog(Check check)
{
    using namespace card_data;
    Card source;
    source.title="Catalog taxonomy fixture";
    source.type="Unit";
    source.traits={"Fey", "Undying"};
    source.keywords={"Plant","Mounted","Undying","custom label"," Undying "};
    source.passives={"Bodyguard","fLyInG"};
    source.stringValues={{"ability","dig"}};
    sf::Packet modern;
    writeCardListHeader(modern,1,true,true,true);
    writeCard(modern,source,true,true,true);
    std::uint32_t count=0; bool old=false, objects=false, passives=false;
    check(readCardListHeader(modern,count,old,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&objects,&passives)
        && count==1 && !old && objects && passives,"schema 15 advertises separate Passives");
    Card decoded;
    check(readCard(modern,decoded,true,true,true,true,true,true,true,true,true) && modern.endOfPacket()
        && decoded.keywords==source.keywords && decoded.traits==source.traits && decoded.passives==source.passives
        && !decoded.abilityReferencesExplicit && decoded.stringValues.size()==1
        && decoded.stringValues[0].value=="dig","schema 15 preserves inert collision and legacy ability fields");
    sf::Packet prior;
    writeCardListHeader(prior,1,true,true);
    writeCard(prior,source,true,true);
    check(readCardListHeader(prior,count,old,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&objects,&passives)
        && objects && !passives,"old client gets schema 14");
    check(readCard(prior,decoded,true,true,true,true,true,true,true,true) && prior.endOfPacket()
        && decoded.passives==source.passives &&
        decoded.keywords==std::vector<std::string>{" Undying ","Mounted","Plant","custom label"} &&
        decoded.traits==source.traits,"old projection keeps mechanics but omits unrepresentable inert keyword");
    Card invalid=source;
    invalid.passives={"Plant"};
    sf::Packet bad;
    writeCard(bad,invalid,false,true,true);
    check(!readReferencedCard(bad,decoded,true,true),"modern writes reject unimplemented passives");

    SQLite::Database db(":memory:",SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);
    db.exec("PRAGMA foreign_keys=ON;"
        "CREATE TABLE cards(title TEXT PRIMARY KEY);"
        "CREATE TABLE card_traits(title TEXT,trait TEXT);"
        "CREATE TABLE card_keywords(title TEXT,keyword TEXT);"
        "INSERT INTO cards VALUES('Fixture');"
        "INSERT INTO card_traits VALUES('Fixture','Undying');"
        "INSERT INTO card_keywords VALUES('Fixture','Bodyguard'),('Fixture','Undying'),"
        "('Fixture','Plant'),('Fixture','Mounted'),('Fixture','Mystery'),('Fixture',' Undying ');");
    card_database::initializeCardPassives(db);
    check(db.execAndGet("SELECT count(*) FROM card_passives").getInt()==2 &&
        db.execAndGet("SELECT count(*) FROM card_keywords").getInt()==4 &&
        db.execAndGet("SELECT trait FROM card_traits").getString()=="Undying",
        "SQL migration moves classified behavior rows only, preserving Traits and inert labels");
    db.exec("INSERT INTO card_keywords VALUES('Fixture','Undying')");
    card_database::initializeCardPassives(db);
    check(db.execAndGet("SELECT count(*) FROM card_passives").getInt()==2 &&
        db.execAndGet("SELECT count(*) FROM card_keywords WHERE keyword='Undying'").getInt()==1,
        "restarting migrated database never reclassifies a modern inert keyword");

    SQLite::Database ancient(":memory:",SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);
    ancient.exec("CREATE TABLE cards(title TEXT PRIMARY KEY);"
        "CREATE TABLE card_keywords(title TEXT,keyword TEXT);"
        "INSERT INTO cards VALUES('Fixture'); INSERT INTO card_keywords VALUES('Fixture','Undying');");
    card_database::initializeCardPassives(ancient);
    check(ancient.execAndGet("SELECT count(*) FROM card_passives").getInt()==0 &&
        ancient.execAndGet("SELECT count(*) FROM card_keywords").getInt()==1,
        "pre-trait databases do not classify eligibility rows as passives");
}
