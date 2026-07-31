module;

#include <SFML/Graphics.hpp>

#include "client_textures.hpp"
#include "client_ui.hpp"
#include "deck_collection.hpp"
#include "../shared/account_data.hpp"
#include "../shared/card_data.hpp"
#include "../shared/conquest_data.hpp"
#include "../shared/conquest_event_data.hpp"
#include "../shared/conquest_map.hpp"
#include "../shared/game_data.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module conquest_screen;

import conquest_services;
import client_services;
import inputbox;

// Module linkage rather than an anonymous namespace: the exported class below
// defines its members inline, so importers instantiate code that references
// these helpers. Internal-linkage entities are not visible there.
using namespace bayou::client;

const sf::Color Ink(246, 232, 200);
const sf::Color Muted(181, 166, 137);
const sf::Color Panel(12, 17, 18, 242);
const sf::Color PanelAlt(25, 30, 29, 244);
const sf::Color Accent(239, 190, 98);
const sf::Color Line(145, 96, 46);
const sf::Color Good(111, 210, 137);
const sf::Color Bad(225, 104, 88);

constexpr sf::Vector2f MapPosition{20.0f, 78.0f};
constexpr sf::Vector2f MapSize{560.0f, 373.3333f};
// 70px pitch with a 62px plate: drawBeveledPlate's usable band is height-18, so a
// two-line row needs 62 to keep the badges clear of its inner rule.
constexpr float EventRowY = 116.0f;
constexpr float EventRowHeight = 70.0f;
constexpr std::size_t VisibleEventRows = 6;
constexpr float LoadoutRowY = 134.0f;
// The shared collection rows need enough vertical room for their portrait and
// metadata. Six readable deck identities are more useful than eight anonymous
// text strips, and the same rhythm now carries through the marching army.
constexpr float LoadoutRowHeight = 54.0f;
constexpr std::size_t VisibleLoadoutRows = 6;
constexpr float CardRowY = 180.0f;
constexpr float CardRowHeight = 36.0f;
constexpr std::size_t VisibleCardRows = 8;

sf::FloatRect rect(float x, float y, float width, float height)
{
    return {{x, y}, {width, height}};
}

void drawText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color = Ink)
{
    sf::Text text(font, value, size);
    text.setFillColor(color);
    text.setPosition(position);
    drawCrispText(window, text);
}

std::string elide(sf::Font& font, std::string value, unsigned int size, float width)
{
    sf::Text text(font, value, size);
    if (text.getLocalBounds().size.x <= width)
    {
        return value;
    }
    while (!value.empty())
    {
        value.pop_back();
        text.setString(value + "...");
        if (text.getLocalBounds().size.x <= width)
        {
            return value + "...";
        }
    }
    return "...";
}

void drawPanel(sf::RenderWindow& window, sf::FloatRect bounds, sf::Color fill = Panel)
{
    drawBeveledPlate(
        window, bounds.position, bounds.size, fill, Line, false,
        std::clamp(bounds.size.y * 0.04f, 5.0f, 12.0f));
}

// drawBeveledPlate carries an inner hairline 5px inside its edge plus highlight
// and shade rules 7-8px in, which is right for a large panel but leaves only
// (height - 18) of usable band. On a two-line list row ~40px tall that rule cuts
// straight through the second line. This is the same cut-corner plate with the
// interior ornament dropped, for rows small enough that the ornament does not
// fit -- and it reads cleaner in a dense list besides.
void drawCompactPlate(
    sf::RenderWindow& window,
    sf::FloatRect bounds,
    sf::Color fill,
    sf::Color outline,
    float cut = 4.0f)
{
    const sf::Vector2f position = bounds.position;
    const sf::Vector2f size = bounds.size;

    sf::ConvexShape plate(8);
    plate.setPoint(0, {position.x + cut, position.y});
    plate.setPoint(1, {position.x + size.x - cut, position.y});
    plate.setPoint(2, {position.x + size.x, position.y + cut});
    plate.setPoint(3, {position.x + size.x, position.y + size.y - cut});
    plate.setPoint(4, {position.x + size.x - cut, position.y + size.y});
    plate.setPoint(5, {position.x + cut, position.y + size.y});
    plate.setPoint(6, {position.x, position.y + size.y - cut});
    plate.setPoint(7, {position.x, position.y + cut});
    plate.setFillColor(fill);
    plate.setOutlineThickness(1.0f);
    plate.setOutlineColor(outline);
    window.draw(plate);
}

void drawButton(
    sf::RenderWindow& window,
    sf::Font& font,
    sf::FloatRect bounds,
    const std::string& label,
    bool hovered,
    bool enabled = true,
    bool primary = false,
    bool destructive = false)
{
    const sf::Color fill = !enabled
        ? sf::Color(45, 48, 47, 235)
        : destructive
            ? (hovered ? sf::Color(112, 48, 34, 248) : sf::Color(70, 33, 28, 242))
        : primary
            ? (hovered ? sf::Color(111, 72, 30, 248) : sf::Color(76, 49, 25, 242))
            : hovered ? sf::Color(88, 54, 27, 248) : sf::Color(39, 31, 24, 246);
    const sf::Color outline = !enabled
        ? sf::Color(82, 82, 78)
        : destructive ? Bad : hovered || primary ? Accent : Line;
    drawBeveledPlate(window, bounds.position, bounds.size, fill, outline, hovered && enabled, 7.0f);
    sf::Text text(font, label, bounds.size.y <= 32.0f ? 15u : 18u);
    text.setFillColor(enabled ? Ink : sf::Color(126, 126, 120));
    centerButtonText(text, bounds.position + bounds.size * 0.5f);
    drawCrispText(window, text);
}

// Each phase gets its own identity colour so the list can be scanned by state
// rather than read line by line: brass invites entry, violet marks the arcane
// planning window, ember means battles are resolving, ash means it is over.
sf::Color phaseColor(conquest_data::EventPhase phase)
{
    switch (phase)
    {
        case conquest_data::EventPhase::Registration: return {224, 168, 74};
        case conquest_data::EventPhase::Planning: return {150, 108, 206};
        case conquest_data::EventPhase::Resolving: return {214, 106, 74};
        case conquest_data::EventPhase::Complete: return {132, 130, 122};
    }
    return Muted;
}

std::string phaseBadge(conquest_data::EventPhase phase)
{
    switch (phase)
    {
        case conquest_data::EventPhase::Registration: return "OPEN";
        case conquest_data::EventPhase::Planning: return "PLANNING";
        case conquest_data::EventPhase::Resolving: return "BATTLES";
        case conquest_data::EventPhase::Complete: return "ENDED";
    }
    return "";
}

// What the countdown on a row actually refers to, so the bare duration is not
// left for the player to guess at.
std::string deadlineCaption(conquest_data::EventPhase phase)
{
    switch (phase)
    {
        case conquest_data::EventPhase::Registration: return "Registration closes";
        case conquest_data::EventPhase::Planning: return "Orders due";
        case conquest_data::EventPhase::Resolving: return "Battles resolve";
        case conquest_data::EventPhase::Complete: return "";
    }
    return "";
}

void drawTextRight(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f rightBaseline,
    sf::Color color)
{
    sf::Text text(font, value, size);
    text.setFillColor(color);
    text.setPosition({rightBaseline.x - text.getLocalBounds().size.x, rightBaseline.y});
    drawCrispText(window, text);
}

// A small filled pill. Used for phase state and for the joined marker.
void drawBadge(
    sf::RenderWindow& window,
    sf::Font& font,
    sf::Vector2f position,
    const std::string& label,
    sf::Color color,
    unsigned int size = 11)
{
    sf::Text text(font, label, size);
    const float width = text.getLocalBounds().size.x + 16.0f;
    const float height = static_cast<float>(size) + 7.0f;

    // Chamfered: a square-cornered rectangle was the only 90-degree corner on a
    // screen where every other plate is cut.
    drawCompactPlate(
        window, {position, {width, height}},
        sf::Color(color.r, color.g, color.b, 46),
        sf::Color(color.r, color.g, color.b, 190),
        3.0f);

    text.setFillColor(color);
    centerButtonText(text, position + sf::Vector2f(width, height) * 0.5f);
    drawCrispText(window, text);
}

std::string phaseName(conquest_data::EventPhase phase)
{
    switch (phase)
    {
        case conquest_data::EventPhase::Registration: return "Registration";
        case conquest_data::EventPhase::Planning: return "Planning";
        case conquest_data::EventPhase::Resolving: return "Resolving battles";
        case conquest_data::EventPhase::Complete: return "Complete";
    }
    return "Unknown";
}

std::string remainingDurationText(std::int64_t timestamp)
{
    if (timestamp <= 0)
    {
        return "";
    }
    const std::int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::int64_t seconds = std::max<std::int64_t>(0, timestamp - now);
    const std::int64_t days = seconds / 86400;
    seconds %= 86400;
    const std::int64_t hours = seconds / 3600;
    const std::int64_t minutes = (seconds % 3600) / 60;
    // A zero-valued trailing unit reads as unfinished ("19h 0m"), so only the
    // coarse unit is shown once the finer one has run out.
    if (days > 0)
    {
        return hours > 0 ? std::to_string(days) + "d " + std::to_string(hours) + "h"
                         : std::to_string(days) + "d";
    }
    if (hours > 0)
    {
        return minutes > 0 ? std::to_string(hours) + "h " + std::to_string(minutes) + "m"
                           : std::to_string(hours) + "h";
    }
    const std::int64_t remainingSeconds = seconds % 60;
    if (minutes > 0)
    {
        return remainingSeconds > 0
            ? std::to_string(minutes) + "m " + std::to_string(remainingSeconds) + "s"
            : std::to_string(minutes) + "m";
    }
    return std::to_string(remainingSeconds) + "s";
}

std::string remainingText(std::int64_t timestamp)
{
    const std::string duration = remainingDurationText(timestamp);
    return duration.empty() ? duration : duration + " remaining";
}

template <typename T>
bool ready(const std::optional<std::future<T>>& future)
{
    return future && future->wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

// conquest_map::PlayerColors is a saturated primary set -- a web blue, a pure
// red, a cyan -- chosen for unambiguous identity in the authoritative services,
// which also read that header. On screen next to brass frames and painted swamp
// they read as debug colours, and one of them sits close enough to bright brass
// to compete with the UI itself. These are the same twelve identities restated
// as desaturated heraldic tints of the game's own palette. Presentation only:
// the shared header is untouched, so the services keep their canonical set.
constexpr std::array<sf::Color, 12> HeraldicColors{{
    {170, 62, 62},    // crimson
    {68, 96, 156},    // indigo
    {82, 130, 90},    // moss
    {182, 146, 66},   // ochre
    {123, 79, 168},   // arcane violet
    {74, 132, 136},   // verdigris
    {174, 96, 132},   // roseblood
    {180, 108, 60},   // rust
    {126, 146, 74},   // fen green
    {96, 96, 158},    // dusk blue
    {148, 74, 96},    // wine
    {138, 118, 88}    // ash brass
}};

sf::Color playerColor(std::uint8_t index, std::uint8_t alpha = 255)
{
    const sf::Color color = HeraldicColors[index % HeraldicColors.size()];
    return {color.r, color.g, color.b, alpha};
}

sf::Color brighten(sf::Color color, int amount)
{
    const auto channel = [amount](std::uint8_t value) {
        return static_cast<std::uint8_t>(std::clamp(static_cast<int>(value) + amount, 0, 255));
    };
    return {channel(color.r), channel(color.g), channel(color.b), color.a};
}

// The interface is laid out in a 4:3 logical space that is letterboxed on a
// wider display, so a wash drawn in logical coordinates stops at x=0 and x=800
// and leaves a hard vertical seam against the full-bleed backdrop. Painting
// through a full-window view instead keeps the tint edge to edge.
void drawFullWindowTint(sf::RenderWindow& window, sf::Color color)
{
    const sf::View logicalView = window.getView();
    const sf::Vector2u windowSize = window.getSize();
    const sf::Vector2f fullSize{
        static_cast<float>(std::max(windowSize.x, 1u)),
        static_cast<float>(std::max(windowSize.y, 1u))};

    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, fullSize)));
    sf::RectangleShape wash(fullSize);
    wash.setFillColor(color);
    window.draw(wash);

    // A soft corner vignette pushes the eye to the centre panels without the
    // banding a single large gradient rectangle would show.
    constexpr int Bands = 7;
    for (int band = 0; band < Bands; ++band)
    {
        const float inset = static_cast<float>(band) * (fullSize.y * 0.022f);
        const float thickness = fullSize.y * 0.030f;
        const auto alpha = static_cast<std::uint8_t>(20 - band * 2);
        sf::RectangleShape edge;
        edge.setFillColor(sf::Color(0, 0, 0, alpha));

        edge.setSize({fullSize.x, thickness});
        edge.setPosition({0.0f, inset});
        window.draw(edge);
        edge.setPosition({0.0f, fullSize.y - inset - thickness});
        window.draw(edge);

        edge.setSize({thickness, fullSize.y});
        edge.setPosition({inset, 0.0f});
        window.draw(edge);
        edge.setPosition({fullSize.x - inset - thickness, 0.0f});
        window.draw(edge);
    }
    window.setView(logicalView);
}

void drawFlagCloth(
    sf::RenderWindow& window,
    sf::Vector2f position,
    sf::Color color,
    sf::Vector2f offset = {})
{
    // A slightly rippled, fork-tailed banner. Keeping this procedural makes
    // one piece of art reusable for every player color without texture swaps.
    constexpr std::array<sf::Vector2f, 9> points{{
        {0.0f, 0.0f}, {7.0f, 1.5f}, {14.0f, -0.5f}, {25.0f, 2.0f},
        {21.0f, 7.0f}, {24.0f, 12.0f}, {15.0f, 9.5f}, {7.0f, 11.0f},
        {0.0f, 9.0f}}};
    constexpr sf::Vector2f center{12.0f, 5.5f};
    sf::VertexArray triangles(sf::PrimitiveType::Triangles);
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        triangles.append({position + offset + center, color});
        triangles.append({position + offset + points[i], color});
        triangles.append({position + offset + points[(i + 1) % points.size()], color});
    }
    window.draw(triangles);

    sf::VertexArray outline(sf::PrimitiveType::LineStrip);
    for (const sf::Vector2f point : points)
    {
        outline.append({position + offset + point, brighten(color, -70)});
    }
    outline.append({position + offset + points.front(), brighten(color, -70)});
    window.draw(outline);
}

void drawControlFlag(sf::RenderWindow& window, sf::Vector2f regionCenter, sf::Color color)
{
    const sf::Vector2f poleTop = regionCenter + sf::Vector2f(-11.0f, -24.0f);
    const sf::Vector2f poleBottom = regionCenter + sf::Vector2f(-11.0f, 9.0f);

    sf::CircleShape groundShadow(10.0f);
    groundShadow.setOrigin({10.0f, 3.0f});
    groundShadow.setScale({1.0f, 0.3f});
    groundShadow.setPosition(poleBottom + sf::Vector2f(4.0f, 1.0f));
    groundShadow.setFillColor(sf::Color(0, 0, 0, 115));
    window.draw(groundShadow);

    sf::RectangleShape pole({2.5f, poleBottom.y - poleTop.y});
    pole.setPosition(poleTop + sf::Vector2f(-1.25f, 0.0f));
    pole.setFillColor(sf::Color(46, 34, 24));
    pole.setOutlineThickness(0.75f);
    pole.setOutlineColor(sf::Color(199, 163, 94));
    window.draw(pole);

    const sf::Vector2f clothPosition = poleTop + sf::Vector2f(1.0f, 2.0f);
    drawFlagCloth(window, clothPosition, sf::Color(0, 0, 0, 105), {2.0f, 2.0f});
    drawFlagCloth(window, clothPosition, color);

    sf::VertexArray highlight(sf::PrimitiveType::LineStrip);
    highlight.append({clothPosition + sf::Vector2f(1.5f, 1.2f), brighten(color, 70)});
    highlight.append({clothPosition + sf::Vector2f(7.0f, 2.7f), brighten(color, 70)});
    highlight.append({clothPosition + sf::Vector2f(14.0f, 0.8f), brighten(color, 70)});
    highlight.append({clothPosition + sf::Vector2f(23.0f, 2.8f), brighten(color, 70)});
    window.draw(highlight);

    sf::CircleShape finial(2.25f);
    finial.setOrigin({2.25f, 2.25f});
    finial.setPosition(poleTop);
    finial.setFillColor(sf::Color(229, 192, 111));
    finial.setOutlineThickness(0.75f);
    finial.setOutlineColor(sf::Color(54, 39, 22));
    window.draw(finial);
}

export namespace bayou::client
{
struct ConquestScreenAction
{
    enum class Kind
    {
        Close,
        JoinBattle
    };

    Kind kind = Kind::Close;
    std::uint64_t battleId = 0;
    std::uint64_t eventId = 0;
};

class ConquestScreen
{
public:
    ConquestScreen(sf::Font& screenFont, TextureStore& textureStore)
        : font(screenFont)
        , textures(textureStore)
        , deckNameInput({24.0f, 86.0f}, {340.0f, 42.0f}, "Conquest deck name", screenFont)
        , eventNameInput({48.0f, 158.0f}, {704.0f, 42.0f}, "Conquest name", screenFont)
        , registrationHoursInput({48.0f, 266.0f}, {200.0f, 42.0f}, "Registration (hours)", screenFont)
        , turnHoursInput({300.0f, 266.0f}, {200.0f, 42.0f}, "Planning turn (hours)", screenFont)
        , reinforcementHoursInput({552.0f, 266.0f}, {200.0f, 42.0f}, "Reinforcement (hours)", screenFont)
    {
        mapTexture = textures.load(std::string(conquest_map::DarkRealmsAsset));
    }

    void open(std::string token, std::string accountUsername, bool admin)
    {
        // Requests carry this generation and are discarded if a different
        // account opens the screen before they complete. We intentionally do
        // not destroy launch::async futures here: their destructors may wait on
        // a stalled socket and freeze the render thread.
        ++sessionGeneration;
        events.clear();
        decks.clear();
        army = {};
        catalog.clear();
        collection.clear();
        eventState = {};
        selectedEvent.reset();
        selectedDeck.reset();
        selectedArmySlot.reset();
        selectedEventDeckId.reset();
        selectedRegionId.reset();
        plannedOrders.clear();
        placements.clear();
        eventOffset = deckOffset = armyOffset = eventDeckOffset = battleOffset = 0;
        if (!pendingCommand)
        {
            commandKind = CommandKind::None;
        }
        accessToken = std::move(token);
        username = std::move(accountUsername);
        accountIsAdmin = admin;
        eventNameInput.clear();
        eventNameInput.setActive(false);
        registrationHoursInput.setContent("24");
        registrationHoursInput.setActive(false);
        turnHoursInput.setContent("24");
        turnHoursInput.setActive(false);
        reinforcementHoursInput.setContent("24");
        reinforcementHoursInput.setActive(false);
        view = View::Events;
        status.clear();
        statusSuccess = true;
        forceEndConfirmationVisible = false;
        pendingAction.reset();
        refreshEvents();
        refreshLoadout();
        refreshCatalog();
    }

    void refresh()
    {
        if (view == View::Event && eventState.summary.id != 0)
        {
            refreshEventState();
        }
        else if (view == View::Loadout || view == View::DeckEdit)
        {
            refreshLoadout();
            refreshCatalog();
        }
        else
        {
            refreshEvents();
        }
    }

    void setStatus(std::string message, bool success)
    {
        status = std::move(message);
        statusSuccess = success;
    }

    // Offline review support. The capture harness has no Conquest service to
    // talk to, so the screen would otherwise only ever show its empty state.
    // This fabricates a campaign mid-flight -- events at several phases, a
    // contested map, and a saved army -- so the presentation can be judged.
    // accessToken stays empty so no refresh ever reaches the network.
    void applyCaptureState(const std::string& key, const std::vector<card_data::Card>& library)
    {
        const std::int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        accessToken.clear();
        username = "Thistlewisp";
        accountIsAdmin = true;
        catalog = library;
        collection.clear();
        for (const card_data::Card& card : library)
        {
            collection.push_back({card.title, 3});
        }

        events.clear();
        const auto addEvent = [&](std::uint64_t id, const char* name,
                                  conquest_data::EventPhase phase, int turn,
                                  std::uint32_t players, bool joined,
                                  std::int64_t registrationIn, std::int64_t turnIn,
                                  const char* winner = "") {
            conquest_data::EventSummary summary;
            summary.id = id;
            summary.name = name;
            summary.mapId = std::string(conquest_map::DarkRealmsId);
            summary.phase = phase;
            summary.turn = turn;
            summary.participantCount = players;
            summary.joined = joined;
            summary.registrationEndsAt = registrationIn > 0 ? now + registrationIn : 0;
            summary.turnEndsAt = turnIn > 0 ? now + turnIn : 0;
            summary.winner = winner;
            events.push_back(summary);
        };

        addEvent(41, "Siege of the Bramble Throne", conquest_data::EventPhase::Planning,
                 3, 6, true, 0, 5 * 60 * 60 + 42 * 60);
        addEvent(42, "The Mirewatch Compact", conquest_data::EventPhase::Registration,
                 0, 4, true, 19 * 60 * 60, 0);
        addEvent(43, "Blackthorn Toll War", conquest_data::EventPhase::Resolving,
                 7, 8, false, 0, 47 * 60);
        addEvent(44, "Long Night Over Sunken Reef", conquest_data::EventPhase::Registration,
                 0, 2, false, 2 * 24 * 60 * 60 + 3 * 60 * 60, 0);
        addEvent(45, "Harrowing of Frostbourne", conquest_data::EventPhase::Planning,
                 5, 5, false, 0, 22 * 60 * 60);
        addEvent(46, "Winter Court Ascendant", conquest_data::EventPhase::Complete,
                 12, 6, true, 0, 0, "gallowglass");

        decks.clear();
        static constexpr const char* DeckNames[] = {
            "Seelie Court Tempo", "Mirewatch Attrition", "Blackthorn Toll Road",
            "Gloomfen Swarm", "Heartwood Bulwark", "Erevan's Ambush"};
        for (std::size_t i = 0; i < std::size(DeckNames); ++i)
        {
            conquest_data::ConquestDeck deck;
            deck.id = static_cast<std::int64_t>(100 + i);
            deck.revision = 3;
            deck.deck.name = DeckNames[i];

            // Seed a genuinely legal capture deck. The old units-only sample
            // produced "24/20" warning rows beside a successful-save message,
            // making the review state contradict itself even though live saves
            // correctly enforce deckRulesError.
            if (!library.empty())
            {
                for (std::size_t pass = 0; pass < library.size(); ++pass)
                {
                    const card_data::Card& card =
                        library[(pass + i * 7) % library.size()];
                    if (game_data::isHeroCard(card) && game_data::cardDeckLimit(card) > 0)
                    {
                        deck.deck.cardTitles.push_back(card.title);
                        break;
                    }
                }

                int nonHeroes = 0;
                for (std::size_t pass = 0;
                     pass < library.size() && nonHeroes < game_data::DeckCardCount;
                     ++pass)
                {
                    const card_data::Card& card =
                        library[(pass + i * 7) % library.size()];
                    if (game_data::isHeroCard(card) || game_data::isTokenCard(card) ||
                        game_data::cardDeckLimit(card) <= 0)
                    {
                        continue;
                    }
                    deck.deck.cardTitles.push_back(card.title);
                    ++nonHeroes;
                }
            }
            decks.push_back(std::move(deck));
        }
        army.revision = 4;
        army.deckIds = {100, 101, 102, 103, 104};

        if (key == "conquest-loadouts")
        {
            view = View::Loadout;
            selectedDeck = 1;
            setStatus("Army saved. 5 decks committed to Conquest.", true);
            return;
        }

        if (key == "conquest-events")
        {
            view = View::Events;
            status.clear();
            return;
        }

        // conquest-map: a contested campaign mid-planning.
        view = View::Event;
        eventState = {};
        eventState.summary = events.front();

        static constexpr const char* Rivals[] = {
            "Thistlewisp", "gallowglass", "Mirefoot", "nettlejack", "Rushlight", "sootpetal"};
        for (std::size_t i = 0; i < std::size(Rivals); ++i)
        {
            conquest_data::PlayerState player;
            player.username = Rivals[i];
            player.colorIndex = static_cast<std::uint8_t>(i);
            player.controlledRegions = static_cast<int>(4 - (i % 3));
            player.ordersSubmitted = i % 2 == 0;
            player.eliminated = i == 5;
            player.reinforcementsAvailable = i == 0 ? 2 : 1;
            player.nextReinforcementAt = i == 0 ? 0 : now + 3600;
            eventState.players.push_back(player);
        }

        // Spread the 20 regions across the six rivals, leaving a few unclaimed
        // so the map reads as genuinely contested rather than fully painted.
        static constexpr int RegionOwners[20] = {
            0, 1, 1, 0, 0, 2, 1, 3, 0, 2,
            -1, 3, 4, 3, 2, -1, 4, 1, 0, 4};
        for (std::size_t i = 0; i < std::size(RegionOwners); ++i)
        {
            if (RegionOwners[i] < 0)
            {
                continue;
            }
            conquest_data::RegionState region;
            region.regionId = static_cast<int>(i + 1);
            region.controller = Rivals[RegionOwners[i]];
            eventState.regions.push_back(region);
        }

        // Deployed armies, including three of ours so the route overlay and the
        // army panel both have something to show.
        struct SeedDeck
        {
            std::uint64_t id;
            const char* owner;
            const char* name;
            int slot;
            int region;
            int destination;
        };
        static constexpr SeedDeck SeedDecks[] = {
            {900, "Thistlewisp", "Seelie Court Tempo", 1, 5, 6},
            {901, "Thistlewisp", "Mirewatch Attrition", 2, 1, 1},
            {902, "Thistlewisp", "Blackthorn Toll Road", 3, 19, 16},
            {903, "gallowglass", "Frostbourne Vanguard", 1, 2, 2},
            {904, "gallowglass", "Ironwood Levy", 2, 7, 7},
            {905, "Mirefoot", "Bogwater Reavers", 1, 10, 10},
            {906, "Mirefoot", "Sable Column", 2, 15, 15},
            {907, "nettlejack", "Grimhold Wardens", 1, 8, 8},
            {908, "Rushlight", "Emberfall Choir", 1, 13, 13},
        };
        for (const SeedDeck& seed : SeedDecks)
        {
            conquest_data::EventDeckState deck;
            deck.id = seed.id;
            deck.sourceDeckId = seed.id - 800;
            deck.owner = seed.owner;
            deck.deckName = seed.name;
            deck.armySlot = seed.slot;
            deck.deployed = true;
            deck.regionId = seed.region;
            deck.destinationRegionId = seed.destination;
            eventState.decks.push_back(deck);
        }
        // Two committed moves, so the map shows planned routes.
        plannedOrders.clear();
        plannedOrders[900] = 6;
        plannedOrders[902] = 16;

        static constexpr struct SeedBattle
        {
            std::uint64_t id;
            int region;
            const char* one;
            const char* two;
            const char* deckOne;
            const char* deckTwo;
            conquest_data::BattleStatus status;
            bool canJoin;
        } SeedBattles[] = {
            {700, 6, "Thistlewisp", "gallowglass", "Seelie Court Tempo", "Ironwood Levy",
             conquest_data::BattleStatus::Ready, true},
            {701, 16, "Thistlewisp", "Mirefoot", "Blackthorn Toll Road", "Sable Column",
             conquest_data::BattleStatus::Ready, true},
            {702, 12, "nettlejack", "Rushlight", "Grimhold Wardens", "Emberfall Choir",
             conquest_data::BattleStatus::Queued, false},
        };
        for (const SeedBattle& seed : SeedBattles)
        {
            conquest_data::BattleState battle;
            battle.id = seed.id;
            battle.kind = conquest_data::BattleKind::Region;
            battle.status = seed.status;
            battle.regionId = seed.region;
            battle.playerOne = seed.one;
            battle.playerTwo = seed.two;
            battle.deckOneName = seed.deckOne;
            battle.deckTwoName = seed.deckTwo;
            battle.canJoin = seed.canJoin;
            eventState.battles.push_back(battle);
        }

        selectedRegionId = 6;
        selectedEventDeckId = 900;
        status.clear();
    }

    std::optional<ConquestScreenAction> takeAction()
    {
        std::optional<ConquestScreenAction> result = pendingAction;
        pendingAction.reset();
        return result;
    }

    std::uint64_t activeEventId() const
    {
        return view == View::Event ? eventState.summary.id : 0;
    }

    bool handleEvent(const sf::Event& event, sf::RenderWindow& window)
    {
        if (view == View::DeckEdit)
        {
            deckNameInput.handleEvent(event, window);
        }
        else if (view == View::EventCreate)
        {
            eventNameInput.handleEvent(event, window);
            registrationHoursInput.handleEvent(event, window);
            turnHoursInput.handleEvent(event, window);
            reinforcementHoursInput.handleEvent(event, window);
        }

        if (const auto* key = event.getIf<sf::Event::KeyPressed>())
        {
            if (key->code == sf::Keyboard::Key::Escape)
            {
                if (forceEndConfirmationVisible)
                {
                    if (!pendingCommand)
                    {
                        forceEndConfirmationVisible = false;
                    }
                    return true;
                }
                goBack();
                return true;
            }
            if (view == View::EventCreate && key->code == sf::Keyboard::Key::Enter)
            {
                submitEventCreate();
                return true;
            }
        }

        if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>())
        {
            if (forceEndConfirmationVisible)
            {
                return true;
            }
            const sf::Vector2f mouse = window.mapPixelToCoords(wheel->position);
            const int direction = wheel->delta < 0.0f ? 1 : -1;
            handleScroll(mouse, direction);
            return true;
        }

        if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>();
            pressed && pressed->button == sf::Mouse::Button::Left)
        {
            const sf::Vector2f mouse = window.mapPixelToCoords(pressed->position);
            if (view == View::DeckEdit)
            {
                deckNameInput.setActive(deckNameInput.contains(mouse));
            }
            else if (view == View::EventCreate)
            {
                eventNameInput.setActive(eventNameInput.contains(mouse));
                registrationHoursInput.setActive(registrationHoursInput.contains(mouse));
                turnHoursInput.setActive(turnHoursInput.contains(mouse));
                reinforcementHoursInput.setActive(reinforcementHoursInput.contains(mouse));
            }
            handleClick(mouse);
            return true;
        }
        return false;
    }

    void update(sf::Vector2f mouse, float deltaTime)
    {
        mousePosition = mouse;
        if (view == View::DeckEdit)
        {
            deckNameInput.updateCursor(deltaTime);
        }
        else if (view == View::EventCreate)
        {
            eventNameInput.updateCursor(deltaTime);
            registrationHoursInput.updateCursor(deltaTime);
            turnHoursInput.updateCursor(deltaTime);
            reinforcementHoursInput.updateCursor(deltaTime);
        }
        pollRequests();
    }

    void draw(sf::RenderWindow& window)
    {
        // A tint rather than the near-opaque plate this used to draw: the swamp
        // backdrop every other screen shows should still read through here.
        drawFullWindowTint(window, sf::Color(5, 8, 9, 76));

        // The event detail view supplies its own two-line toolbar. Drawing the
        // generic Conquest banner behind it makes the title, phase, and nav
        // controls compete for the same pixels.
        if (view != View::Event)
        {
            // drawTitlePlaque widens itself to its label and throws pipes and
            // rivets ~110px past its nominal box, which ran straight through
            // the subtitle. A section header with a brass rule reads as
            // deliberately designed here and leaves the width predictable.
            // The display face, so this title belongs to the same family as every
            // other screen's; the section-header treatment above stays.
            drawText(window, displayFontOr(font), "Conquest", 30, {24.0f, 14.0f}, Accent);
            drawText(window, font, "Long-running campaigns. Each card copy belongs to one Conquest deck.",
                     13, {252.0f, 30.0f}, Muted, 520.0f);
            drawSeparatorRule(window, {24.0f, 56.0f}, 752.0f);
        }

        if (view == View::Events)
        {
            drawEvents(window);
        }
        else if (view == View::Loadout)
        {
            drawLoadout(window);
        }
        else if (view == View::DeckEdit)
        {
            drawDeckEditor(window);
        }
        else if (view == View::EventCreate)
        {
            drawEventCreate(window);
        }
        else
        {
            drawEvent(window);
        }

        if (forceEndConfirmationVisible)
        {
            drawForceEndConfirmation(window);
        }

        if (!status.empty() || busy())
        {
            drawStatusBar(window);
        }
    }

private:
    // Forwards to the shared helper rather than the module-local one so every
    // string on this screen is rasterized at device resolution, and so callers
    // can elide to a width.
    static void drawText(
        sf::RenderWindow& window,
        sf::Font& font,
        const std::string& value,
        unsigned int size,
        sf::Vector2f position,
        sf::Color color = Ink,
        float maxWidth = 0.0f)
    {
        bayou::client::drawText(window, font, value, size, position, color, maxWidth);
    }

    static void drawPanel(
        sf::RenderWindow& window,
        sf::FloatRect bounds,
        sf::Color fill = Panel)
    {
        ::drawPanel(window, bounds, fill);
    }

    void drawLoadingState(
        sf::RenderWindow& window,
        sf::FloatRect bounds,
        const std::string& heading,
        const std::string& detail)
    {
        const float cardWidth = std::min(420.0f, bounds.size.x - 32.0f);
        const sf::FloatRect card(
            {bounds.position.x + (bounds.size.x - cardWidth) * 0.5f,
             bounds.position.y + (bounds.size.y - 96.0f) * 0.5f},
            {cardWidth, 96.0f});
        drawCompactPlate(window, card, sf::Color(25, 30, 29, 248), Line, 8.0f);
        drawRadialGlow(window, card.position + sf::Vector2f(34.0f, 48.0f), 24.0f,
                       sf::Color(239, 190, 98, 38));
        drawStud(window, card.position + sf::Vector2f(34.0f, 48.0f), 8.0f, Accent);
        const float copyWidth = std::max(0.0f, card.size.x - 78.0f);
        drawText(window, font, heading, 18, card.position + sf::Vector2f(58.0f, 21.0f), Ink, copyWidth);
        drawText(window, font, detail, 13, card.position + sf::Vector2f(58.0f, 52.0f), Muted, copyWidth);
    }

    void drawStatusBar(sf::RenderWindow& window)
    {
        const sf::FloatRect bounds = rect(20.0f, 566.0f, 760.0f, 28.0f);
        const bool working = busy();
        const sf::Color accent = working ? Accent : statusSuccess ? Good : Bad;
        const sf::Color fill = working
            ? sf::Color(46, 36, 23, 246)
            : statusSuccess ? sf::Color(18, 39, 29, 246) : sf::Color(48, 24, 23, 246);
        drawCompactPlate(window, bounds, fill, accent, 6.0f);
        drawBadge(window, font, {30.0f, 571.0f},
                  working ? "WORKING" : statusSuccess ? "UPDATED" : "ERROR", accent, 10);

        sf::Text badgeProbe(
            font, working ? "WORKING" : statusSuccess ? "UPDATED" : "ERROR", 10);
        const float messageX = 30.0f + badgeProbe.getLocalBounds().size.x + 34.0f;
        if (!status.empty())
        {
            drawText(window, font, elide(font, status, 13, 590.0f), 13,
                     {messageX, 572.0f}, Ink);
        }
        if (working)
        {
            drawTextRight(window, font, "Working...", 12, {768.0f, 573.0f}, Accent);
        }
    }

    enum class View
    {
        Events,
        Loadout,
        DeckEdit,
        EventCreate,
        Event
    };

    enum class CommandKind
    {
        None,
        Join,
        StartEvent,
        Orders,
        Reinforce,
        DeleteDeck,
        CreateEvent,
        ForceEndEvent
    };

    sf::Font& font;
    TextureStore& textures;
    sf::Texture* mapTexture = nullptr;
    InputBox deckNameInput;
    InputBox eventNameInput;
    InputBox registrationHoursInput;
    InputBox turnHoursInput;
    InputBox reinforcementHoursInput;
    View view = View::Events;
    std::string accessToken;
    std::string username;
    bool accountIsAdmin = false;
    bool forceEndConfirmationVisible = false;
    std::string status;
    bool statusSuccess = true;
    sf::Vector2f mousePosition;
    std::optional<ConquestScreenAction> pendingAction;

    std::vector<conquest_data::EventSummary> events;
    conquest_data::EventState eventState;
    std::vector<conquest_data::ConquestDeck> decks;
    conquest_data::ConquestArmy army;
    std::vector<card_data::Card> catalog;
    std::vector<account_data::CollectionCard> collection;

    std::size_t eventOffset = 0;
    std::optional<std::size_t> selectedEvent;
    std::size_t deckOffset = 0;
    std::optional<std::size_t> selectedDeck;
    std::size_t armyOffset = 0;
    std::optional<std::size_t> selectedArmySlot;
    conquest_data::ConquestDeck editingDeck;
    std::size_t editingCardOffset = 0;
    std::size_t libraryOffset = 0;
    std::optional<std::size_t> selectedEditingTitle;
    std::optional<std::size_t> selectedLibraryCard;

    std::optional<std::uint64_t> selectedEventDeckId;
    std::optional<int> selectedRegionId;
    std::unordered_map<std::uint64_t, int> plannedOrders;
    std::vector<conquest_data::StartingPlacement> placements;
    std::size_t eventDeckOffset = 0;
    std::size_t battleOffset = 0;

    std::optional<std::future<ConquestEventListResult>> pendingEvents;
    std::optional<std::future<ConquestLoadoutResult>> pendingLoadout;
    std::optional<std::future<DeckEditorLoadResult>> pendingCatalog;
    std::optional<std::future<ConquestEventStateResult>> pendingState;
    std::optional<std::future<ConquestCommandResult>> pendingEventWatch;
    std::optional<std::future<ConquestCommandResult>> pendingCommand;
    std::optional<std::future<ConquestDeckResult>> pendingDeckSave;
    std::optional<std::future<ConquestArmyResult>> pendingArmySave;
    CommandKind commandKind = CommandKind::None;
    std::uint64_t sessionGeneration = 0;
    std::uint64_t pendingEventsGeneration = 0;
    std::uint64_t pendingLoadoutGeneration = 0;
    std::uint64_t pendingCatalogGeneration = 0;
    std::uint64_t pendingStateGeneration = 0;
    std::uint64_t pendingEventWatchGeneration = 0;
    std::uint64_t pendingCommandGeneration = 0;
    std::uint64_t pendingDeckSaveGeneration = 0;
    std::uint64_t pendingArmySaveGeneration = 0;
    std::uint64_t pendingStateEventId = 0;
    std::uint64_t pendingEventWatchEventId = 0;
    std::uint64_t pendingCommandEventId = 0;
    bool refreshStateAgain = false;

    bool busy() const
    {
        return pendingEvents || pendingLoadout || pendingCatalog || pendingState ||
            pendingCommand || pendingDeckSave || pendingArmySave;
    }

    static bool hovered(sf::FloatRect bounds, sf::Vector2f mouse)
    {
        return bounds.contains(mouse);
    }

    void refreshEvents()
    {
        if (pendingEvents || accessToken.empty())
        {
            return;
        }
        pendingEvents.emplace(std::async(std::launch::async, [token = accessToken] {
            return fetchConquestEvents(token);
        }));
        pendingEventsGeneration = sessionGeneration;
    }

    void refreshLoadout()
    {
        if (pendingLoadout || accessToken.empty())
        {
            return;
        }
        pendingLoadout.emplace(std::async(std::launch::async, [token = accessToken] {
            return fetchConquestLoadout(token);
        }));
        pendingLoadoutGeneration = sessionGeneration;
    }

    void refreshCatalog()
    {
        if (pendingCatalog || accessToken.empty())
        {
            return;
        }
        pendingCatalog.emplace(std::async(std::launch::async, [token = accessToken] {
            return loadDeckEditorData(token);
        }));
        pendingCatalogGeneration = sessionGeneration;
    }

    void refreshEventState()
    {
        if (pendingState)
        {
            refreshStateAgain = true;
            return;
        }
        if (eventState.summary.id == 0 || accessToken.empty())
        {
            return;
        }
        const std::uint64_t eventId = eventState.summary.id;
        pendingState.emplace(std::async(std::launch::async, [token = accessToken, eventId] {
            return fetchConquestEventState(token, eventId);
        }));
        pendingStateGeneration = sessionGeneration;
        pendingStateEventId = eventId;
        refreshStateAgain = false;
    }

    void watchEventState()
    {
        if (pendingEventWatch || pendingState || view != View::Event ||
            eventState.summary.id == 0 || accessToken.empty())
        {
            return;
        }
        const std::uint64_t eventId = eventState.summary.id;
        const std::uint64_t stateFingerprint =
            conquest_data::eventStateFingerprint(eventState);
        pendingEventWatch.emplace(std::async(
            std::launch::async,
            [token = accessToken, eventId, stateFingerprint] {
                return watchConquestEvent(token, eventId, stateFingerprint);
            }));
        pendingEventWatchGeneration = sessionGeneration;
        pendingEventWatchEventId = eventId;
    }

    void openEvent(std::uint64_t eventId)
    {
        eventState = {};
        eventState.summary.id = eventId;
        plannedOrders.clear();
        placements.clear();
        selectedEventDeckId.reset();
        selectedRegionId.reset();
        view = View::Event;
        refreshEventState();
    }

    void pollRequests()
    {
        try
        {
            if (ready(pendingEvents))
            {
                const std::uint64_t requestGeneration = pendingEventsGeneration;
                ConquestEventListResult result = pendingEvents->get();
                pendingEvents.reset();
                if (requestGeneration != sessionGeneration)
                {
                    refreshEvents();
                }
                else
                {
                    if (result.success)
                    {
                        events = std::move(result.events);
                        eventOffset = std::min(eventOffset, events.size());
                    }
                    setStatus(result.message, result.success);
                }
            }
            if (ready(pendingLoadout))
            {
                const std::uint64_t requestGeneration = pendingLoadoutGeneration;
                ConquestLoadoutResult result = pendingLoadout->get();
                pendingLoadout.reset();
                if (requestGeneration != sessionGeneration)
                {
                    refreshLoadout();
                }
                else
                {
                    if (result.success)
                    {
                        decks = std::move(result.decks);
                        army = std::move(result.army);
                        std::sort(decks.begin(), decks.end(), [](const auto& left, const auto& right) {
                            return left.deck.name < right.deck.name;
                        });
                        if (selectedDeck && *selectedDeck >= decks.size())
                        {
                            selectedDeck.reset();
                        }
                    }
                    setStatus(result.message, result.success);
                }
            }
            if (ready(pendingCatalog))
            {
                const std::uint64_t requestGeneration = pendingCatalogGeneration;
                DeckEditorLoadResult result = pendingCatalog->get();
                pendingCatalog.reset();
                if (requestGeneration != sessionGeneration)
                {
                    refreshCatalog();
                }
                else
                {
                    if (result.success)
                    {
                        catalog = std::move(result.cards);
                        collection = std::move(result.collection);
                        std::sort(catalog.begin(), catalog.end(), [](const auto& left, const auto& right) {
                            return left.title < right.title;
                        });
                    }
                    else
                    {
                        setStatus(result.message, false);
                    }
                }
            }
            if (ready(pendingState))
            {
                const std::uint64_t requestGeneration = pendingStateGeneration;
                const std::uint64_t requestedEventId = pendingStateEventId;
                const bool requestAnotherRefresh = refreshStateAgain;
                refreshStateAgain = false;
                ConquestEventStateResult result = pendingState->get();
                pendingState.reset();
                const bool currentRequest = requestGeneration == sessionGeneration &&
                    requestedEventId == eventState.summary.id;
                if (!currentRequest)
                {
                    if (view == View::Event && eventState.summary.id != 0)
                    {
                        refreshEventState();
                    }
                }
                else
                {
                    if (result.success)
                    {
                        eventState = std::move(result.state);
                        plannedOrders.clear();
                        for (const conquest_data::EventDeckState& deck : eventState.decks)
                        {
                            if (deck.owner == username && deck.deployed && !deck.eliminated)
                            {
                                plannedOrders[deck.id] = deck.destinationRegionId > 0
                                    ? deck.destinationRegionId : deck.regionId;
                            }
                        }
                    }
                    setStatus(result.message, result.success);
                }
                if (requestAnotherRefresh && view == View::Event &&
                    eventState.summary.id != 0 && !pendingState)
                {
                    refreshEventState();
                }
                else if (result.success && currentRequest)
                {
                    watchEventState();
                }
            }
            if (ready(pendingEventWatch))
            {
                const std::uint64_t requestGeneration = pendingEventWatchGeneration;
                const std::uint64_t requestedEventId = pendingEventWatchEventId;
                std::future<ConquestCommandResult> completed =
                    std::move(*pendingEventWatch);
                pendingEventWatch.reset();
                ConquestCommandResult result = completed.get();
                const bool currentRequest = requestGeneration == sessionGeneration &&
                    view == View::Event && requestedEventId == eventState.summary.id;
                if (currentRequest)
                {
                    if (!result.success)
                    {
                        setStatus(result.message, false);
                    }
                    refreshEventState();
                }
                else if (view == View::Event && eventState.summary.id != 0 &&
                         !pendingState)
                {
                    watchEventState();
                }
            }
            if (ready(pendingCommand))
            {
                const std::uint64_t requestGeneration = pendingCommandGeneration;
                const std::uint64_t requestedEventId = pendingCommandEventId;
                ConquestCommandResult result = pendingCommand->get();
                pendingCommand.reset();
                const CommandKind finishedKind = commandKind;
                commandKind = CommandKind::None;
                const bool currentRequest = requestGeneration == sessionGeneration &&
                    (requestedEventId == 0 || requestedEventId == eventState.summary.id);
                if (currentRequest)
                {
                    setStatus(result.message, result.success);
                    if (result.success)
                    {
                        if (finishedKind == CommandKind::DeleteDeck)
                        {
                            selectedDeck.reset();
                            refreshLoadout();
                        }
                        else if (finishedKind == CommandKind::CreateEvent)
                        {
                            eventNameInput.setActive(false);
                            registrationHoursInput.setActive(false);
                            turnHoursInput.setActive(false);
                            reinforcementHoursInput.setActive(false);
                            view = View::Events;
                            eventOffset = 0;
                            refreshEvents();
                        }
                        else if (finishedKind == CommandKind::ForceEndEvent)
                        {
                            forceEndConfirmationVisible = false;
                            eventState = {};
                            view = View::Events;
                            eventOffset = 0;
                            refreshEvents();
                        }
                        else
                        {
                            refreshEventState();
                            refreshEvents();
                        }
                    }
                }
            }
            if (ready(pendingDeckSave))
            {
                const std::uint64_t requestGeneration = pendingDeckSaveGeneration;
                ConquestDeckResult result = pendingDeckSave->get();
                pendingDeckSave.reset();
                if (requestGeneration == sessionGeneration)
                {
                    setStatus(result.message, result.success);
                    if (result.success)
                    {
                        editingDeck = std::move(result.deck);
                        view = View::Loadout;
                        selectedDeck.reset();
                        refreshLoadout();
                    }
                }
            }
            if (ready(pendingArmySave))
            {
                const std::uint64_t requestGeneration = pendingArmySaveGeneration;
                ConquestArmyResult result = pendingArmySave->get();
                pendingArmySave.reset();
                if (requestGeneration == sessionGeneration)
                {
                    setStatus(result.message, result.success);
                    if (result.success)
                    {
                        army = std::move(result.army);
                    }
                }
            }
        }
        catch (const std::exception& error)
        {
            pendingEvents.reset();
            pendingLoadout.reset();
            pendingCatalog.reset();
            pendingState.reset();
            pendingCommand.reset();
            pendingDeckSave.reset();
            pendingArmySave.reset();
            commandKind = CommandKind::None;
            setStatus(std::string("Conquest request failed: ") + error.what(), false);
        }
    }

    void goBack()
    {
        if (view == View::DeckEdit)
        {
            deckNameInput.setActive(false);
            view = View::Loadout;
        }
        else if (view == View::EventCreate)
        {
            eventNameInput.setActive(false);
            registrationHoursInput.setActive(false);
            turnHoursInput.setActive(false);
            reinforcementHoursInput.setActive(false);
            view = View::Events;
            refreshEvents();
        }
        else if (view == View::Event)
        {
            view = View::Events;
            refreshEvents();
        }
        else
        {
            pendingAction = ConquestScreenAction{ConquestScreenAction::Kind::Close, 0, 0};
        }
    }

    void handleScroll(sf::Vector2f mouse, int direction)
    {
        auto scroll = [direction](std::size_t& offset, std::size_t count, std::size_t visible) {
            const std::size_t maximum = count > visible ? count - visible : 0;
            if (direction > 0)
            {
                offset = std::min(offset + 1, maximum);
            }
            else if (offset > 0)
            {
                --offset;
            }
        };

        if (view == View::Events)
        {
            scroll(eventOffset, events.size(), VisibleEventRows);
        }
        else if (view == View::Loadout)
        {
            if (mouse.x < 390.0f)
            {
                scroll(deckOffset, decks.size(), VisibleLoadoutRows);
            }
            else
            {
                scroll(armyOffset, army.deckIds.size(), VisibleLoadoutRows);
            }
        }
        else if (view == View::DeckEdit)
        {
            if (mouse.x < 380.0f)
            {
                scroll(editingCardOffset, editingUniqueTitles().size(), VisibleCardRows);
            }
            else
            {
                scroll(libraryOffset, availableLibrary().size(), VisibleCardRows);
            }
        }
        else if (mouse.x > 580.0f)
        {
            scroll(eventDeckOffset, selectableEventDecks().size(), 7);
        }
        else
        {
            scroll(battleOffset, joinableBattles().size(), 2);
        }
    }

    void handleClick(sf::Vector2f mouse)
    {
        if (forceEndConfirmationVisible)
        {
            if (!pendingCommand && rect(220, 370, 160, 42).contains(mouse))
            {
                forceEndConfirmationVisible = false;
            }
            else if (!pendingCommand && rect(420, 370, 160, 42).contains(mouse))
            {
                submitForceEnd();
            }
            return;
        }
        if (view == View::Events)
        {
            clickEvents(mouse);
        }
        else if (view == View::Loadout)
        {
            clickLoadout(mouse);
        }
        else if (view == View::DeckEdit)
        {
            clickDeckEditor(mouse);
        }
        else if (view == View::EventCreate)
        {
            clickEventCreate(mouse);
        }
        else
        {
            clickEvent(mouse);
        }
    }

    void clickEvents(sf::Vector2f mouse)
    {
        if (rect(20, 64, 112, 36).contains(mouse))
        {
            goBack();
            return;
        }
        if (rect(144, 64, 130, 36).contains(mouse))
        {
            view = View::Loadout;
            refreshLoadout();
            return;
        }
        if (rect(666, 64, 114, 36).contains(mouse))
        {
            refreshEvents();
            return;
        }
        if (accountIsAdmin && !busy() && rect(498, 64, 156, 36).contains(mouse))
        {
            eventNameInput.clear();
            eventNameInput.setActive(true);
            registrationHoursInput.setContent("24");
            registrationHoursInput.setActive(false);
            turnHoursInput.setContent("24");
            turnHoursInput.setActive(false);
            reinforcementHoursInput.setContent("24");
            reinforcementHoursInput.setActive(false);
            view = View::EventCreate;
            status.clear();
            return;
        }
        // The empty-state call to action uses this same transition. Keep its
        // destination in the existing navigation flow rather than adding a
        // one-off route for presentation.
        if (events.empty() && !pendingEvents && rect(310, 504, 180, 38).contains(mouse))
        {
            view = View::Loadout;
            refreshLoadout();
            return;
        }
        for (std::size_t row = 0; row < VisibleEventRows; ++row)
        {
            const std::size_t index = eventOffset + row;
            if (index >= events.size())
            {
                break;
            }
            if (rect(34, EventRowY + row * EventRowHeight, 732, EventRowHeight - 6).contains(mouse))
            {
                selectedEvent = index;
                openEvent(events[index].id);
                return;
            }
        }
    }

    void clickLoadout(sf::Vector2f mouse)
    {
        if (rect(20, 64, 112, 36).contains(mouse))
        {
            goBack();
            return;
        }
        if (rect(144, 64, 130, 36).contains(mouse))
        {
            view = View::Events;
            refreshEvents();
            return;
        }
        if (rect(666, 64, 114, 36).contains(mouse))
        {
            refreshLoadout();
            refreshCatalog();
            return;
        }
        for (std::size_t row = 0; row < VisibleLoadoutRows; ++row)
        {
            const std::size_t index = deckOffset + row;
            if (index < decks.size() &&
                rect(30, LoadoutRowY + row * LoadoutRowHeight, 340, LoadoutRowHeight - 4).contains(mouse))
            {
                selectedDeck = index;
                return;
            }
        }
        for (std::size_t row = 0; row < VisibleLoadoutRows; ++row)
        {
            const std::size_t slot = armyOffset + row;
            if (slot < army.deckIds.size() &&
                rect(410, LoadoutRowY + row * LoadoutRowHeight, 360, LoadoutRowHeight - 4).contains(mouse))
            {
                selectedArmySlot = slot;
                const auto found = std::find_if(decks.begin(), decks.end(), [&](const auto& deck) {
                    return deck.id == army.deckIds[slot];
                });
                if (found != decks.end())
                {
                    selectedDeck = static_cast<std::size_t>(found - decks.begin());
                }
                return;
            }
        }
        if (rect(24, 492, 100, 38).contains(mouse))
        {
            beginEdit({});
        }
        else if (rect(132, 492, 100, 38).contains(mouse) && selectedDeck)
        {
            beginEdit(decks[*selectedDeck]);
        }
        else if (rect(240, 492, 124, 38).contains(mouse) && selectedDeck && !pendingCommand)
        {
            const conquest_data::ConquestDeck deck = decks[*selectedDeck];
            commandKind = CommandKind::DeleteDeck;
            pendingCommand.emplace(std::async(
                std::launch::async,
                [token = accessToken, id = deck.id, revision = deck.revision] {
                    return deleteConquestDeck(token, id, revision);
                }));
            pendingCommandGeneration = sessionGeneration;
            pendingCommandEventId = 0;
        }
        else if (rect(410, 492, 182, 38).contains(mouse) && selectedDeck)
        {
            toggleArmyDeck(decks[*selectedDeck].id);
        }
        else if (rect(600, 492, 170, 38).contains(mouse) && !pendingArmySave)
        {
            conquest_data::ConquestArmy next = army;
            pendingArmySave.emplace(std::async(
                std::launch::async,
                [token = accessToken, next] { return saveConquestArmy(token, next); }));
            pendingArmySaveGeneration = sessionGeneration;
        }
    }

    void beginEdit(conquest_data::ConquestDeck deck)
    {
        editingDeck = std::move(deck);
        deckNameInput.setContent(editingDeck.deck.name);
        editingCardOffset = 0;
        libraryOffset = 0;
        selectedEditingTitle.reset();
        selectedLibraryCard.reset();
        view = View::DeckEdit;
    }

    void toggleArmyDeck(std::int64_t id)
    {
        const auto found = std::find(army.deckIds.begin(), army.deckIds.end(), id);
        if (found != army.deckIds.end())
        {
            army.deckIds.erase(found);
            selectedArmySlot.reset();
            setStatus("Removed deck from the pending army", true);
        }
        else if (army.deckIds.size() < conquest_data::MaxConquestArmyDecks)
        {
            army.deckIds.push_back(id);
            setStatus("Added deck to the pending army", true);
        }
        else
        {
            setStatus("An army can contain at most 10 decks", false);
        }
    }

    void clickDeckEditor(sf::Vector2f mouse)
    {
        if (rect(20, 24, 112, 36).contains(mouse))
        {
            goBack();
            return;
        }
        const std::vector<std::string> titles = editingUniqueTitles();
        for (std::size_t row = 0; row < VisibleCardRows; ++row)
        {
            const std::size_t index = editingCardOffset + row;
            if (index < titles.size() &&
                rect(24, CardRowY + row * CardRowHeight, 340, CardRowHeight - 3).contains(mouse))
            {
                selectedEditingTitle = index;
                return;
            }
        }
        const std::vector<const card_data::Card*> library = availableLibrary();
        for (std::size_t row = 0; row < VisibleCardRows; ++row)
        {
            const std::size_t index = libraryOffset + row;
            if (index < library.size() &&
                rect(392, CardRowY + row * CardRowHeight, 384, CardRowHeight - 3).contains(mouse))
            {
                selectedLibraryCard = index;
                return;
            }
        }
        if (rect(24, 500, 150, 38).contains(mouse) && selectedEditingTitle)
        {
            if (*selectedEditingTitle < titles.size())
            {
                const auto found = std::find(
                    editingDeck.deck.cardTitles.begin(), editingDeck.deck.cardTitles.end(),
                    titles[*selectedEditingTitle]);
                if (found != editingDeck.deck.cardTitles.end())
                {
                    editingDeck.deck.cardTitles.erase(found);
                }
                selectedEditingTitle.reset();
            }
        }
        else if (rect(392, 500, 150, 38).contains(mouse) && selectedLibraryCard)
        {
            if (*selectedLibraryCard < library.size())
            {
                editingDeck.deck.cardTitles.push_back(library[*selectedLibraryCard]->title);
                selectedLibraryCard.reset();
            }
        }
        else if (rect(626, 500, 150, 38).contains(mouse))
        {
            saveEditingDeck();
        }
    }

    void saveEditingDeck()
    {
        if (pendingDeckSave)
        {
            return;
        }
        editingDeck.deck.name = deckNameInput.getContent();
        if (editingDeck.deck.name.empty())
        {
            setStatus("Give the Conquest deck a name", false);
            return;
        }
        const std::vector<card_data::Card> resolved =
            resolveDeckCards(editingDeck.deck, catalog);
        if (resolved.size() != editingDeck.deck.cardTitles.size())
        {
            setStatus("The deck contains a card that is no longer in the catalog", false);
            return;
        }
        if (const std::optional<std::string> error = game_data::deckRulesError(resolved))
        {
            setStatus(*error, false);
            return;
        }
        const conquest_data::ConquestDeck next = editingDeck;
        pendingDeckSave.emplace(std::async(
            std::launch::async,
            [token = accessToken, next] { return saveConquestDeck(token, next); }));
        pendingDeckSaveGeneration = sessionGeneration;
    }

    std::vector<std::string> editingUniqueTitles() const
    {
        std::vector<std::string> result;
        for (const std::string& title : editingDeck.deck.cardTitles)
        {
            if (std::find(result.begin(), result.end(), title) == result.end())
            {
                result.push_back(title);
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    int copiesInOtherConquestDecks(const std::string& title) const
    {
        int count = 0;
        for (const conquest_data::ConquestDeck& deck : decks)
        {
            if (deck.id == editingDeck.id)
            {
                continue;
            }
            count += static_cast<int>(std::count(
                deck.deck.cardTitles.begin(), deck.deck.cardTitles.end(), title));
        }
        return count;
    }

    std::vector<const card_data::Card*> availableLibrary() const
    {
        std::vector<const card_data::Card*> result;
        for (const card_data::Card& card : catalog)
        {
            if (game_data::isTokenCard(card))
            {
                continue;
            }
            const int inDeck = static_cast<int>(std::count(
                editingDeck.deck.cardTitles.begin(), editingDeck.deck.cardTitles.end(), card.title));
            const int perDeckLimit = game_data::cardDeckLimit(card);
            const int owned = collectionCopiesFor(collection, card.title);
            if (inDeck < perDeckLimit &&
                copiesInOtherConquestDecks(card.title) + inDeck < owned)
            {
                result.push_back(&card);
            }
        }
        return result;
    }

    bool joinedEvent() const
    {
        return eventState.summary.joined;
    }

    std::vector<const conquest_data::EventDeckState*> selectableEventDecks() const
    {
        std::vector<const conquest_data::EventDeckState*> result;
        for (const conquest_data::EventDeckState& deck : eventState.decks)
        {
            if (deck.owner == username && !deck.eliminated)
            {
                result.push_back(&deck);
            }
        }
        std::sort(result.begin(), result.end(), [](const auto* left, const auto* right) {
            return left->armySlot < right->armySlot;
        });
        return result;
    }

    std::vector<const conquest_data::BattleState*> joinableBattles() const
    {
        std::vector<const conquest_data::BattleState*> result;
        for (const conquest_data::BattleState& battle : eventState.battles)
        {
            if (battle.canJoin && battle.status == conquest_data::BattleStatus::Ready)
            {
                result.push_back(&battle);
            }
        }
        return result;
    }

    const conquest_data::EventDeckState* eventDeck(std::uint64_t id) const
    {
        const auto found = std::find_if(eventState.decks.begin(), eventState.decks.end(),
            [id](const auto& deck) { return deck.id == id; });
        return found == eventState.decks.end() ? nullptr : &*found;
    }

    const conquest_data::PlayerState* currentPlayer() const
    {
        const auto found = std::find_if(eventState.players.begin(), eventState.players.end(),
            [&](const auto& player) { return player.username == username; });
        return found == eventState.players.end() ? nullptr : &*found;
    }

    static std::string regionName(int id)
    {
        const conquest_map::RegionDefinition* found = conquest_map::region(id);
        return found ? std::string(found->name) : std::string("Unknown");
    }

    std::string regionController(int id) const
    {
        const auto found = std::find_if(eventState.regions.begin(), eventState.regions.end(),
            [id](const auto& region) { return region.regionId == id; });
        return found == eventState.regions.end() ? std::string() : found->controller;
    }

    std::optional<std::uint8_t> colorForUsername(const std::string& name) const
    {
        const auto found = std::find_if(eventState.players.begin(), eventState.players.end(),
            [&](const auto& player) { return player.username == name; });
        if (found == eventState.players.end())
        {
            return std::nullopt;
        }
        return found->colorIndex;
    }

    std::optional<int> regionAt(sf::Vector2f mouse) const
    {
        if (!rect(MapPosition.x, MapPosition.y, MapSize.x, MapSize.y).contains(mouse))
        {
            return std::nullopt;
        }
        std::optional<int> nearest;
        float nearestDistance = 24.0f;
        for (const conquest_map::RegionDefinition& region : conquest_map::DarkRealmsRegions)
        {
            const sf::Vector2f center = mapPoint(region.centerX, region.centerY);
            const float distance = std::hypot(mouse.x - center.x, mouse.y - center.y);
            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearest = region.id;
            }
        }
        return nearest;
    }

    // The art is inset inside its plate so the brass frame reads as a mount.
    // Markers must be projected onto that same inset rect or every flag drifts
    // off its territory.
    static sf::FloatRect mapArtBounds()
    {
        constexpr float Inset = 5.0f;
        return {MapPosition + sf::Vector2f(Inset, Inset),
                MapSize - sf::Vector2f(Inset * 2.0f, Inset * 2.0f)};
    }

    static sf::Vector2f mapPoint(int x, int y)
    {
        const sf::FloatRect art = mapArtBounds();
        return {
            art.position.x +
                static_cast<float>(x) / conquest_map::DarkRealmsImageWidth * art.size.x,
            art.position.y +
                static_cast<float>(y) / conquest_map::DarkRealmsImageHeight * art.size.y};
    }

    void clickEvent(sf::Vector2f mouse)
    {
        if (rect(20, 24, 112, 36).contains(mouse))
        {
            goBack();
            return;
        }
        if (rect(666, 24, 114, 36).contains(mouse))
        {
            refreshEventState();
            return;
        }
        if (accountIsAdmin && !pendingCommand &&
            eventState.summary.id != 0 &&
            eventState.summary.phase != conquest_data::EventPhase::Complete &&
            rect(530, 24, 124, 36).contains(mouse))
        {
            forceEndConfirmationVisible = true;
            return;
        }
        if (accountIsAdmin &&
            eventState.summary.phase == conquest_data::EventPhase::Registration &&
            rect(606, 520, 158, 32).contains(mouse))
        {
            submitForceStart();
            return;
        }

        const std::vector<const conquest_data::EventDeckState*> eventDecks = selectableEventDecks();
        if (joinedEvent())
        {
            for (std::size_t row = 0; row < 7; ++row)
            {
                const std::size_t index = eventDeckOffset + row;
                if (index < eventDecks.size() &&
                    rect(590, 146 + row * 42.0f, 188, 38).contains(mouse))
                {
                    selectedEventDeckId = eventDecks[index]->id;
                    return;
                }
            }
        }
        else
        {
            const std::vector<const conquest_data::ConquestDeck*> armyDecks = armyDeckList();
            for (std::size_t row = 0; row < 7; ++row)
            {
                const std::size_t index = eventDeckOffset + row;
                if (index < armyDecks.size() &&
                    rect(590, 146 + row * 42.0f, 188, 38).contains(mouse))
                {
                    selectedEventDeckId = static_cast<std::uint64_t>(armyDecks[index]->id);
                    return;
                }
            }
        }

        const std::vector<const conquest_data::BattleState*> battles = joinableBattles();
        for (std::size_t row = 0; row < 2; ++row)
        {
            const std::size_t index = battleOffset + row;
            if (index < battles.size() && rect(24, 492 + row * 33.0f, 552, 29).contains(mouse))
            {
                pendingAction = ConquestScreenAction{
                    ConquestScreenAction::Kind::JoinBattle,
                    battles[index]->id,
                    eventState.summary.id};
                return;
            }
        }

        if (const std::optional<int> region = regionAt(mouse))
        {
            selectEventRegion(*region);
            return;
        }

        if (!joinedEvent() && rect(606, 414, 158, 36).contains(mouse))
        {
            submitJoin();
        }
        else if (joinedEvent() && rect(606, 414, 158, 36).contains(mouse))
        {
            // Orders (including an explicit empty/pass order) are independent
            // of reinforcement deployment and never require selecting a deck.
            submitOrders();
        }
        else if (joinedEvent() && rect(606, 520, 158, 32).contains(mouse))
        {
            submitReinforcement();
        }
    }

    std::vector<const conquest_data::ConquestDeck*> armyDeckList() const
    {
        std::vector<const conquest_data::ConquestDeck*> result;
        for (const std::int64_t id : army.deckIds)
        {
            const auto found = std::find_if(decks.begin(), decks.end(),
                [id](const auto& deck) { return deck.id == id; });
            if (found != decks.end())
            {
                result.push_back(&*found);
            }
        }
        return result;
    }

    void selectEventRegion(int regionId)
    {
        selectedRegionId = regionId;
        if (!selectedEventDeckId)
        {
            return;
        }
        if (!joinedEvent())
        {
            if (eventState.summary.phase != conquest_data::EventPhase::Registration)
            {
                setStatus("Registration for this campaign has closed", false);
                return;
            }
            if (!conquest_map::isEdgeRegion(regionId))
            {
                setStatus("Starting decks must be placed on an edge region", false);
                return;
            }
            const std::uint64_t sourceDeckId = *selectedEventDeckId;
            placements.erase(std::remove_if(placements.begin(), placements.end(),
                [sourceDeckId, regionId](const auto& placement) {
                    return placement.deckId == sourceDeckId || placement.regionId == regionId;
                }), placements.end());
            if (placements.size() >= 2)
            {
                setStatus("A player can start with at most two decks", false);
                return;
            }
            if (!placements.empty() &&
                !conquest_map::areAdjacent(placements.front().regionId, regionId))
            {
                setStatus("The two starting regions must touch", false);
                return;
            }
            placements.push_back({sourceDeckId, regionId});
            setStatus("Starting placement selected", true);
            return;
        }

        const conquest_data::EventDeckState* deck = eventDeck(*selectedEventDeckId);
        if (eventState.summary.phase != conquest_data::EventPhase::Planning)
        {
            setStatus("Moves can be changed during the planning phase", false);
            return;
        }
        if (!deck || deck->eliminated)
        {
            return;
        }
        if (!deck->deployed)
        {
            setStatus("Reserve deck selected; choose a controlled edge region", true);
            return;
        }
        if (regionId != deck->regionId && !conquest_map::areAdjacent(deck->regionId, regionId))
        {
            setStatus("A deck can stay or move to one touching region", false);
            return;
        }

        // Temporary duplicates are allowed while editing: this is what makes
        // simultaneous A-vacates-X/B-enters-X orders possible. Final projected
        // positions are checked atomically when the player submits.
        plannedOrders[deck->id] = regionId;
        setStatus(regionId == deck->regionId ? "Deck will hold position" : "Secret move planned", true);
    }

    void submitJoin()
    {
        if (eventState.summary.phase != conquest_data::EventPhase::Registration)
        {
            setStatus("Registration for this campaign has closed", false);
            return;
        }
        if (pendingCommand || placements.empty() || placements.size() > 2)
        {
            if (placements.empty())
            {
                setStatus("Place one or two army decks first", false);
            }
            return;
        }
        const std::uint64_t eventId = eventState.summary.id;
        const std::vector<conquest_data::StartingPlacement> next = placements;
        commandKind = CommandKind::Join;
        pendingCommand.emplace(std::async(
            std::launch::async,
            [token = accessToken, eventId, next] {
                return joinConquestEvent(token, eventId, next);
            }));
        pendingCommandGeneration = sessionGeneration;
        pendingCommandEventId = eventId;
    }

    void submitOrders()
    {
        if (eventState.summary.phase != conquest_data::EventPhase::Planning)
        {
            setStatus("Orders can only be submitted during planning", false);
            return;
        }
        if (pendingCommand)
        {
            return;
        }
        const conquest_data::PlayerState* player = currentPlayer();
        if (player && player->eliminated)
        {
            setStatus("Your conquest army has been defeated", false);
            return;
        }
        std::vector<conquest_data::MoveOrder> orders;
        std::unordered_map<int, std::uint64_t> projectedOccupants;
        for (const conquest_data::EventDeckState& deck : eventState.decks)
        {
            if (deck.owner != username || !deck.deployed || deck.eliminated)
            {
                continue;
            }
            const auto found = plannedOrders.find(deck.id);
            const int destination = found == plannedOrders.end() ? deck.regionId : found->second;
            if (projectedOccupants.contains(destination))
            {
                setStatus("Two of your decks would end in region " +
                          std::to_string(destination) + ". Change one order first.", false);
                return;
            }
            projectedOccupants[destination] = deck.id;
            orders.push_back({deck.id, destination});
        }
        const std::uint64_t eventId = eventState.summary.id;
        commandKind = CommandKind::Orders;
        pendingCommand.emplace(std::async(
            std::launch::async,
            [token = accessToken, eventId, orders] {
                return submitConquestOrders(token, eventId, orders);
            }));
        pendingCommandGeneration = sessionGeneration;
        pendingCommandEventId = eventId;
    }

    void submitForceStart()
    {
        if (!accountIsAdmin ||
            eventState.summary.phase != conquest_data::EventPhase::Registration)
        {
            return;
        }
        if (eventState.summary.participantCount < 2)
        {
            setStatus("At least two players are required to start the conquest", false);
            return;
        }
        if (pendingCommand)
        {
            return;
        }

        const std::uint64_t eventId = eventState.summary.id;
        commandKind = CommandKind::StartEvent;
        pendingCommand.emplace(std::async(
            std::launch::async,
            [token = accessToken, eventId] {
                return forceStartConquestEvent(token, eventId);
            }));
        pendingCommandGeneration = sessionGeneration;
        pendingCommandEventId = eventId;
    }

    void submitForceEnd()
    {
        if (!accountIsAdmin || pendingCommand || eventState.summary.id == 0 ||
            eventState.summary.phase == conquest_data::EventPhase::Complete)
        {
            return;
        }
        const std::uint64_t eventId = eventState.summary.id;
        commandKind = CommandKind::ForceEndEvent;
        pendingCommand.emplace(std::async(
            std::launch::async,
            [token = accessToken, eventId] {
                return forceEndConquestEvent(token, eventId);
            }));
        pendingCommandGeneration = sessionGeneration;
        pendingCommandEventId = eventId;
    }

    static std::optional<std::int64_t> scheduleSeconds(const std::string& text)
    {
        std::int64_t hours = 0;
        const char* first = text.data();
        const char* last = first + text.size();
        const auto [end, error] = std::from_chars(first, last, hours);
        if (text.empty() || error != std::errc{} || end != last || hours < 1 ||
            hours > conquest_data::MaxConquestScheduleSeconds / (60 * 60))
        {
            return std::nullopt;
        }
        return hours * 60 * 60;
    }

    void submitEventCreate()
    {
        if (!accountIsAdmin || pendingCommand)
        {
            return;
        }
        const std::string name = eventNameInput.getContent();
        if (name.find_first_not_of(" \t\r\n") == std::string::npos)
        {
            setStatus("Enter a name for the conquest", false);
            return;
        }
        if (name.size() > conquest_data::MaxConquestTextLength)
        {
            setStatus("Conquest name is too long", false);
            return;
        }
        const std::optional<std::int64_t> registration =
            scheduleSeconds(registrationHoursInput.getContent());
        const std::optional<std::int64_t> turn =
            scheduleSeconds(turnHoursInput.getContent());
        const std::optional<std::int64_t> reinforcement =
            scheduleSeconds(reinforcementHoursInput.getContent());
        if (!registration || !turn || !reinforcement)
        {
            setStatus("Each timing must be a whole number from 1 to 720 hours", false);
            return;
        }

        commandKind = CommandKind::CreateEvent;
        pendingCommand.emplace(std::async(
            std::launch::async,
            [token = accessToken, name, registration = *registration,
             turn = *turn, reinforcement = *reinforcement] {
                return createConquestEvent(
                    token, name, registration, turn, reinforcement);
            }));
        pendingCommandGeneration = sessionGeneration;
        pendingCommandEventId = 0;
    }

    void submitReinforcement()
    {
        if (eventState.summary.phase != conquest_data::EventPhase::Planning)
        {
            setStatus("Reinforcements can only deploy during planning", false);
            return;
        }
        if (pendingCommand || !selectedEventDeckId || !selectedRegionId)
        {
            setStatus("Select a reserve deck and an edge region", false);
            return;
        }
        const conquest_data::EventDeckState* deck = eventDeck(*selectedEventDeckId);
        if (!deck || deck->deployed || deck->eliminated)
        {
            setStatus("Select an undeployed reserve deck", false);
            return;
        }
        const conquest_data::PlayerState* player = currentPlayer();
        const std::int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (!player || player->eliminated)
        {
            setStatus("Your conquest army has been defeated", false);
            return;
        }
        if (player->reinforcementsAvailable <= 0)
        {
            setStatus("Control four regions for each reinforcement", false);
            return;
        }
        if (player->nextReinforcementAt > now)
        {
            setStatus("The reinforcement cooldown is still active", false);
            return;
        }
        if (!conquest_map::isEdgeRegion(*selectedRegionId) ||
            regionController(*selectedRegionId) != username)
        {
            setStatus("Reinforcements require a controlled edge region", false);
            return;
        }
        const bool occupied = std::any_of(
            eventState.decks.begin(), eventState.decks.end(), [&](const auto& other) {
                return other.deployed && !other.eliminated && other.regionId == *selectedRegionId;
            });
        if (occupied)
        {
            setStatus("That edge region already contains a deck", false);
            return;
        }
        const std::uint64_t eventId = eventState.summary.id;
        const std::uint64_t deckId = deck->id;
        const int regionId = *selectedRegionId;
        commandKind = CommandKind::Reinforce;
        pendingCommand.emplace(std::async(
            std::launch::async,
            [token = accessToken, eventId, deckId, regionId] {
                return reinforceConquestEvent(token, eventId, deckId, regionId);
            }));
        pendingCommandGeneration = sessionGeneration;
        pendingCommandEventId = eventId;
    }

    void drawHeaderButtons(sf::RenderWindow& window, bool loadoutActive)
    {
        drawButton(window, font, rect(20, 64, 112, 36), "Back",
                   hovered(rect(20, 64, 112, 36), mousePosition));
        drawButton(window, font, rect(144, 64, 130, 36),
                   loadoutActive ? "Events" : "Loadouts",
                   hovered(rect(144, 64, 130, 36), mousePosition));
        drawButton(window, font, rect(666, 64, 114, 36), "Refresh",
                   hovered(rect(666, 64, 114, 36), mousePosition), !busy());
    }

    void drawEvents(sf::RenderWindow& window)
    {
        drawHeaderButtons(window, false);
        if (accountIsAdmin)
        {
            drawButton(window, font, rect(498, 64, 156, 36), "New Conquest",
                       hovered(rect(498, 64, 156, 36), mousePosition), !busy(), true);
        }
        drawPanel(window, rect(20, 104, 760, 444));
        if (events.empty() && !pendingEvents)
        {
            drawEventsEmptyState(window);
            return;
        }
        if (events.empty() && pendingEvents)
        {
            drawLoadingState(window, rect(20, 104, 760, 444),
                             "Summoning campaigns...", "Reading the current war table.");
            return;
        }

        // The CAMPAIGN / DEADLINE column captions are gone: they cost the rows
        // 18px of height they needed, and each row already labels its own
        // countdown ("Orders due", "Registration closes").
        for (std::size_t row = 0; row < VisibleEventRows; ++row)
        {
            const std::size_t index = eventOffset + row;
            if (index >= events.size())
            {
                break;
            }
            drawEventRow(window, events[index],
                         rect(34, EventRowY + row * EventRowHeight, 732, EventRowHeight - 8));
        }
        drawEventScrollTrack(window);

        drawText(window, font, "Select a campaign to inspect its map, plan moves, or resume battles.",
                 13, {24.0f, 556.0f}, Muted);
    }

    // Without a track the list silently truncates at six rows and nothing tells
    // the player more campaigns exist below. The thumb is sized to the visible
    // fraction so its length reads as "how much of the list am I seeing".
    void drawEventScrollTrack(sf::RenderWindow& window)
    {
        if (events.size() <= VisibleEventRows)
        {
            return;
        }

        const float top = EventRowY;
        const float height = VisibleEventRows * EventRowHeight - 8.0f;
        const float width = 4.0f;
        const float x = 770.0f;

        sf::RectangleShape track({width, height});
        track.setPosition({x, top});
        track.setFillColor(sf::Color(6, 10, 11, 210));
        window.draw(track);

        const float visibleFraction =
            static_cast<float>(VisibleEventRows) / static_cast<float>(events.size());
        const float thumbHeight = std::max(24.0f, height * visibleFraction);
        const std::size_t maximumOffset = events.size() - VisibleEventRows;
        const float progress = maximumOffset == 0
            ? 0.0f
            : static_cast<float>(eventOffset) / static_cast<float>(maximumOffset);

        sf::RectangleShape thumb({width, thumbHeight});
        thumb.setPosition({x, top + (height - thumbHeight) * progress});
        thumb.setFillColor(Accent);
        window.draw(thumb);
    }

    void drawEventRow(
        sf::RenderWindow& window,
        const conquest_data::EventSummary& event,
        sf::FloatRect bounds)
    {
        const bool isHovered = hovered(bounds, mousePosition);
        const bool ended = event.phase == conquest_data::EventPhase::Complete;
        const sf::Color phase = phaseColor(event.phase);

        drawBeveledPlate(
            window, bounds.position, bounds.size,
            isHovered ? sf::Color(46, 38, 28, 242) : sf::Color(17, 24, 25, 232),
            isHovered ? Accent : sf::Color(96, 68, 38),
            isHovered, 6.0f);

        // A phase-coloured spine down the leading edge: the fastest read of
        // state in the list, and it doubles as the row's visual anchor.
        sf::RectangleShape spine({3.0f, bounds.size.y - 12.0f});
        spine.setPosition(bounds.position + sf::Vector2f(6.0f, 6.0f));
        spine.setFillColor(ended ? sf::Color(phase.r, phase.g, phase.b, 150) : phase);
        window.draw(spine);

        // A crop of the campaign map stands in for per-event key art.
        // Clear of the plate's inner rule at +5, which the thumbnail's own border
        // was doubling up against.
        const sf::FloatRect thumbnail(
            bounds.position + sf::Vector2f(24.0f, 11.0f), {76.0f, bounds.size.y - 22.0f});
        if (mapTexture)
        {
            drawMapThumbnail(window, thumbnail, event.id,
                             ended ? sf::Color(120, 120, 120) : sf::Color(210, 210, 210));
        }
        sf::RectangleShape thumbnailFrame(thumbnail.size);
        thumbnailFrame.setPosition(thumbnail.position);
        thumbnailFrame.setFillColor(sf::Color::Transparent);
        thumbnailFrame.setOutlineThickness(1.0f);
        thumbnailFrame.setOutlineColor(sf::Color(103, 72, 39));
        window.draw(thumbnailFrame);

        const float textX = bounds.position.x + 112.0f;
        drawText(window, font, elide(font, event.name, 19, 344.0f), 19,
                 {textX, bounds.position.y + 10.0f}, ended ? Muted : Ink);

        // One shared centre line for every element on the metadata row, so the
        // badges, the roster bar and the turn count sit on a single axis. Kept
        // inside y+8 .. y+height-10, the band drawBeveledPlate leaves free.
        constexpr float BadgeHeight = 18.0f;
        const float metaTop = bounds.position.y + 34.0f;
        const float metaCenterY = metaTop + BadgeHeight * 0.5f;

        drawBadge(window, font, {textX, metaTop}, phaseBadge(event.phase), phase);
        sf::Text badgeProbe(font, phaseBadge(event.phase), 11);
        float detailX = textX + badgeProbe.getLocalBounds().size.x + 16.0f + 8.0f;

        if (event.joined)
        {
            drawBadge(window, font, {detailX, metaTop}, "JOINED", Good);
            sf::Text joinedProbe(font, "JOINED", 11);
            detailX += joinedProbe.getLocalBounds().size.x + 16.0f + 8.0f;
        }

        detailX += drawPlayerPips(window, event.participantCount, {detailX, metaCenterY}, ended) + 14.0f;

        // Turn number carries the campaign's progress once it is under way.
        if (event.turn > 0)
        {
            drawText(window, font, "Turn " + std::to_string(event.turn), 12,
                     {detailX, metaCenterY - 8.0f}, Muted);
        }

        // Inset past the plate's inner rule and its right-hand rivet, which the
        // countdown was sitting 3px from.
        const float rightEdge = bounds.position.x + bounds.size.x - 26.0f;
        if (ended)
        {
            drawTextRight(window, font, "Final", 11, {rightEdge, bounds.position.y + 14.0f}, Muted);
            drawTextRight(window, font,
                          event.winner.empty() ? "No winner" : event.winner + " won",
                          15, {rightEdge, bounds.position.y + 32.0f}, Accent);
            return;
        }

        const std::int64_t deadline = event.phase == conquest_data::EventPhase::Registration
            ? event.registrationEndsAt : event.turnEndsAt;
        const std::string duration = remainingDurationText(deadline);
        if (duration.empty())
        {
            return;
        }
        drawTextRight(window, font, deadlineCaption(event.phase), 11,
                      {rightEdge, bounds.position.y + 13.0f}, sf::Color(150, 132, 104));
        // Under an hour the countdown turns urgent. Bright brass rather than a
        // UI warning red, which was the one pure red left in the palette.
        const bool urgent = duration.find('d') == std::string::npos &&
            duration.find('h') == std::string::npos;
        drawTextRight(window, font, duration, 17,
                      {rightEdge, bounds.position.y + 30.0f}, urgent ? Accent : Ink);
    }

    // There is one map asset, so drawing the same crop on every row reads as
    // repeated wallpaper. Each campaign instead gets a stable crop keyed off
    // its id, which makes the rows feel like distinct theatres of war.
    void drawMapThumbnail(
        sf::RenderWindow& window, sf::FloatRect bounds, std::uint64_t eventId, sf::Color tint)
    {
        if (!mapTexture)
        {
            return;
        }
        const sf::Vector2f textureSize{
            static_cast<float>(mapTexture->getSize().x),
            static_cast<float>(mapTexture->getSize().y)};

        // Cover the destination, then slide the source window within whatever
        // slack the aspect difference leaves.
        const float scale = std::max(
            bounds.size.x / textureSize.x, bounds.size.y / textureSize.y) * 2.2f;
        const sf::Vector2f windowSize{bounds.size.x / scale, bounds.size.y / scale};
        const float slackX = std::max(0.0f, textureSize.x - windowSize.x);
        const float slackY = std::max(0.0f, textureSize.y - windowSize.y);
        const float fractionX = static_cast<float>(eventId * 37 % 100) / 100.0f;
        const float fractionY = static_cast<float>(eventId * 61 % 100) / 100.0f;

        sf::Sprite sprite(*mapTexture);
        sprite.setTextureRect(sf::IntRect(
            {static_cast<int>(slackX * fractionX), static_cast<int>(slackY * fractionY)},
            {static_cast<int>(windowSize.x), static_cast<int>(windowSize.y)}));
        sprite.setPosition(bounds.position);
        sprite.setScale({scale, scale});
        sprite.setColor(tint);
        window.draw(sprite);
    }

    // Roster strength against the lobby cap. A slim fill bar rather than one pip
    // per seat: twelve dots at this size degrade into a dotted rule and the
    // filled/empty distinction stops reading.
    // Returns the width consumed so callers can lay out after it.
    float drawPlayerPips(
        sf::RenderWindow& window, std::uint32_t count, sf::Vector2f center, bool dim)
    {
        const auto seats = static_cast<float>(conquest_data::MaxConquestPlayers);
        const float filled = std::clamp(static_cast<float>(count) / seats, 0.0f, 1.0f);
        constexpr float BarWidth = 54.0f;
        constexpr float BarHeight = 5.0f;

        sf::RectangleShape track({BarWidth, BarHeight});
        track.setPosition({center.x, center.y - BarHeight * 0.5f});
        track.setFillColor(sf::Color(58, 52, 43, 235));
        track.setOutlineThickness(1.0f);
        track.setOutlineColor(sf::Color(88, 66, 40));
        window.draw(track);

        sf::RectangleShape fill({BarWidth * filled, BarHeight});
        fill.setPosition({center.x, center.y - BarHeight * 0.5f});
        fill.setFillColor(dim ? sf::Color(132, 130, 122) : Accent);
        window.draw(fill);

        const std::string label = std::to_string(count) + " / " +
            std::to_string(static_cast<int>(seats)) + " players";
        drawText(window, font, label, 12, {center.x + BarWidth + 8.0f, center.y - 8.0f}, Muted);
        sf::Text probe(font, label, 12);
        return BarWidth + 8.0f + probe.getLocalBounds().size.x;
    }

    // A designed empty state: the mode still has to sell itself when there is
    // nothing to join, so this shows the map it is played on and what happens
    // next rather than a bare apology.
    void drawEventsEmptyState(sf::RenderWindow& window)
    {
        UiContext ui{window, font, font, textures};
        const sf::FloatRect art(rect(190.0f, 120.0f, 420.0f, 280.0f));
        drawBeveledPlate(window, art.position - sf::Vector2f(6.0f, 6.0f),
                         art.size + sf::Vector2f(12.0f, 12.0f),
                         sf::Color(10, 15, 16, 232), Line, false, 8.0f);
        if (mapTexture)
        {
            drawContainSprite(window, *mapTexture, art, sf::Color(172, 178, 174));
        }
        drawVerticalScrim(ui, art, sf::Color(5, 9, 10, 0), sf::Color(5, 9, 10, 128));

        drawSectionHeading(ui, {248.0f, 416.0f}, "The Dark Realms lie quiet", 304.0f);

        drawWrappedText(
            window, font,
            "No campaign is mustering right now. Build the army you will march "
            "with, and you will be ready the moment the next war is called.",
            14, {220.0f, 446.0f}, Muted, 360.0f);

        drawButton(window, font, rect(310, 504, 180, 38), "Prepare Loadouts",
                   hovered(rect(310, 504, 180, 38), mousePosition), true, true);
    }

    void clickEventCreate(sf::Vector2f mouse)
    {
        if (rect(20, 64, 112, 36).contains(mouse))
        {
            goBack();
            return;
        }
        if (rect(566, 464, 186, 42).contains(mouse))
        {
            submitEventCreate();
        }
    }

    void drawEventCreate(sf::RenderWindow& window)
    {
        drawButton(window, font, rect(20, 64, 112, 36), "Back",
                   hovered(rect(20, 64, 112, 36), mousePosition));
        drawText(window, font, "Set Up a New Conquest", 23, {154.0f, 70.0f}, Accent);
        drawPanel(window, rect(20, 116, 760, 432));

        eventNameInput.draw(window);
        registrationHoursInput.draw(window);
        turnHoursInput.draw(window);
        reinforcementHoursInput.draw(window);

        drawText(window, font, "Map", 16, {48.0f, 348.0f}, Muted);
        drawText(window, font, "Dark Realms", 22, {48.0f, 374.0f}, Ink);
        drawText(window, font,
                 "Registration opens immediately. Each event freezes the current card catalog",
                 14, {48.0f, 418.0f}, Muted);
        drawText(window, font,
                 "so later card edits do not change battles already in this conquest.",
                 14, {48.0f, 440.0f}, Muted);
        drawButton(window, font, rect(566, 464, 186, 42), "Create Conquest",
                    hovered(rect(566, 464, 186, 42), mousePosition), !pendingCommand, true);
    }

    std::string armyDeckName(std::int64_t id) const
    {
        const auto found = std::find_if(decks.begin(), decks.end(),
            [id](const auto& deck) { return deck.id == id; });
        return found == decks.end() ? "Missing deck" : found->deck.name;
    }

    void drawLoadout(sf::RenderWindow& window)
    {
        drawHeaderButtons(window, true);
        // Headings sat exactly on the panel's top edge, so the border cut
        // through the glyphs. They now sit above their panels as captions.
        drawText(window, font, "CONQUEST DECKS", 13, {24.0f, 106.0f}, sf::Color(150, 132, 104));
        drawTextRight(window, font, std::to_string(decks.size()) + " saved", 13,
                      {376.0f, 106.0f}, Muted);
        drawText(window, font, "MARCHING ARMY", 13, {404.0f, 106.0f}, sf::Color(150, 132, 104));
        drawTextRight(window, font,
                      std::to_string(army.deckIds.size()) + " of " +
                          std::to_string(conquest_data::MaxConquestArmyDecks),
                      13, {776.0f, 106.0f}, Muted);
        drawPanel(window, rect(20, 124, 360, 354));
        drawPanel(window, rect(400, 124, 380, 354));

        for (std::size_t row = 0; row < VisibleLoadoutRows; ++row)
        {
            const std::size_t index = deckOffset + row;
            if (index >= decks.size())
            {
                break;
            }
            const sf::FloatRect bounds = rect(30, LoadoutRowY + row * LoadoutRowHeight, 340, LoadoutRowHeight - 4);
            UiContext ui{window, font, font, textures};
            drawDeckRosterRow(ui, bounds, summarizeDeck(decks[index].deck, catalog),
                               selectedDeck == index, hovered(bounds, mousePosition));
            // Decks already marching are marked so the two lists can be
            // reconciled without counting back and forth between them.
            const bool marching = std::find(army.deckIds.begin(), army.deckIds.end(),
                                            decks[index].id) != army.deckIds.end();
            if (marching)
            {
                // Keep the marching state as a small green seal in the outer
                // margin; a word badge would compete with the roster's curve.
                drawStud(window, bounds.position + sf::Vector2f(7.0f, bounds.size.y * 0.5f), 2.8f, Good);
            }
        }
        if (decks.empty() && !pendingLoadout)
        {
            drawText(window, font, "No Conquest decks yet.", 16, {36.0f, 150.0f}, Muted);
            drawWrappedText(window, font,
                            "Build one with New below. Conquest decks draw on their own "
                            "pool of card copies.",
                            13, {36.0f, 176.0f}, sf::Color(150, 132, 104), 320.0f);
        }
        else if (decks.empty() && pendingLoadout)
        {
            drawLoadingState(window, rect(20, 124, 360, 354),
                             "Loading deck vault...", "Checking your saved Conquest decks.");
        }

        for (std::size_t row = 0; row < VisibleLoadoutRows; ++row)
        {
            const std::size_t slot = armyOffset + row;
            if (slot >= army.deckIds.size())
            {
                break;
            }
            const bool isSelected = selectedArmySlot == slot;
            const sf::FloatRect bounds = rect(410, LoadoutRowY + row * LoadoutRowHeight, 360, LoadoutRowHeight - 4);
            const auto found = std::find_if(decks.begin(), decks.end(), [&](const auto& deck) {
                return deck.id == army.deckIds[slot];
            });
            if (found != decks.end())
            {
                UiContext ui{window, font, font, textures};
                drawDeckRosterRow(ui, bounds, summarizeDeck(found->deck, catalog),
                                   isSelected, hovered(bounds, mousePosition));
                // The order is semantic, not merely the list's visual order,
                // so stamp it onto the roster card without sacrificing its art.
                drawBadge(window, font, bounds.position + sf::Vector2f(8.0f, 7.0f),
                          "#" + std::to_string(slot + 1), Accent, 9);
            }
            else
            {
                drawCompactPlate(window, bounds, PanelAlt, Line, 5.0f);
                drawText(window, font, armyDeckName(army.deckIds[slot]), 16,
                         bounds.position + sf::Vector2f(16.0f, 16.0f));
            }
        }
        if (army.deckIds.empty())
        {
            drawText(window, font, "No army committed.", 16, {416.0f, 150.0f}, Muted);
            drawWrappedText(window, font,
                            "Select a Conquest deck and add it to your army. Every deck you "
                            "commit marches as a separate force on the map.",
                            13, {416.0f, 176.0f}, sf::Color(150, 132, 104), 340.0f);
        }

        drawButton(window, font, rect(24, 492, 100, 38), "New",
                   hovered(rect(24, 492, 100, 38), mousePosition), !busy());
        drawButton(window, font, rect(132, 492, 100, 38), "Edit",
                   hovered(rect(132, 492, 100, 38), mousePosition), selectedDeck.has_value());
        drawButton(window, font, rect(240, 492, 124, 38), "Delete",
                   hovered(rect(240, 492, 124, 38), mousePosition), selectedDeck.has_value() && !busy(), false, true);
        const bool selectedInArmy = selectedDeck &&
            std::find(army.deckIds.begin(), army.deckIds.end(), decks[*selectedDeck].id) != army.deckIds.end();
        // "Remove from Army" overran its plate at 166px wide.
        drawButton(window, font, rect(410, 492, 182, 38),
                   selectedInArmy ? "Remove" : "Add to Army",
                   hovered(rect(410, 492, 182, 38), mousePosition), selectedDeck.has_value());
        drawButton(window, font, rect(600, 492, 170, 38), "Save Army",
                   hovered(rect(600, 492, 170, 38), mousePosition),
                   !pendingArmySave && !army.deckIds.empty(), true);
        drawSeparatorRule(window, {24.0f, 538.0f}, 752.0f);
        drawText(window, font,
                 "Conquest decks draw on their own pool of copies. Your regular decks are untouched.",
                 13, {24.0f, 550.0f}, Muted);
    }

    void drawDeckEditor(sf::RenderWindow& window)
    {
        drawButton(window, font, rect(20, 24, 112, 36), "Back",
                   hovered(rect(20, 24, 112, 36), mousePosition));
        drawText(window, font, editingDeck.id == 0 ? "New Conquest Deck" : "Edit Conquest Deck",
                 23, {154.0f, 29.0f}, Accent);
        deckNameInput.draw(window);
        UiContext ui{window, font, font, textures};
        drawPanel(window, rect(20, 138, 350, 346));
        drawPanel(window, rect(388, 138, 392, 346));
        drawSectionHeading(ui, {32.0f, 146.0f}, "Deck", 310.0f);
        drawSectionHeading(ui, {400.0f, 146.0f}, "Available Collection", 348.0f);

        const std::vector<std::string> titles = editingUniqueTitles();
        for (std::size_t row = 0; row < VisibleCardRows; ++row)
        {
            const std::size_t index = editingCardOffset + row;
            if (index >= titles.size())
            {
                break;
            }
            const int copies = static_cast<int>(std::count(
                editingDeck.deck.cardTitles.begin(), editingDeck.deck.cardTitles.end(), titles[index]));
            const auto card = std::find_if(catalog.begin(), catalog.end(), [&](const card_data::Card& candidate) {
                return candidate.title == titles[index];
            });
            const int copyLimit = card == catalog.end() ? 0 : game_data::cardDeckLimit(*card);
            const sf::FloatRect bounds = rect(24, CardRowY + row * CardRowHeight, 340, CardRowHeight - 3);
            if (card != catalog.end())
            {
                CardRow rowView;
                rowView.card = &*card;
                rowView.rect = bounds;
                rowView.selected = selectedEditingTitle == index;
                rowView.hovered = hovered(bounds, mousePosition);
                rowView.copies = copies;
                rowView.copyLimit = copyLimit;
                drawCardRow(ui, rowView);
            }
            else
            {
                drawCompactPlate(window, bounds, PanelAlt, Line, 4.0f);
                drawText(window, font, elide(font, titles[index], 15, 270.0f), 15,
                         bounds.position + sf::Vector2f(10.0f, 8.0f));
            }
        }

        const std::vector<const card_data::Card*> library = availableLibrary();
        for (std::size_t row = 0; row < VisibleCardRows; ++row)
        {
            const std::size_t index = libraryOffset + row;
            if (index >= library.size())
            {
                break;
            }
            const card_data::Card& card = *library[index];
            const int committed = copiesInOtherConquestDecks(card.title) +
                static_cast<int>(std::count(editingDeck.deck.cardTitles.begin(),
                                            editingDeck.deck.cardTitles.end(), card.title));
            const int owned = collectionCopiesFor(collection, card.title);
            const sf::FloatRect bounds = rect(392, CardRowY + row * CardRowHeight, 384, CardRowHeight - 3);
            CardRow rowView;
            rowView.card = &card;
            rowView.rect = bounds;
            rowView.selected = selectedLibraryCard == index;
            rowView.hovered = hovered(bounds, mousePosition);
            // The shared count becomes the committed-versus-owned capacity in
            // Conquest. availableLibrary has already excluded exhausted cards.
            rowView.copies = committed;
            rowView.copyLimit = owned;
            drawCardRow(ui, rowView);
        }

        drawButton(window, font, rect(24, 500, 150, 38), "Remove Copy",
                   hovered(rect(24, 500, 150, 38), mousePosition), selectedEditingTitle.has_value());
        drawButton(window, font, rect(392, 500, 150, 38), "Add Copy",
                   hovered(rect(392, 500, 150, 38), mousePosition), selectedLibraryCard.has_value());
        drawButton(window, font, rect(626, 500, 150, 38), "Save Deck",
                   hovered(rect(626, 500, 150, 38), mousePosition), !pendingDeckSave);
        const std::vector<card_data::Card> resolved =
            resolveDeckCards(editingDeck.deck, catalog);
        const int nonHeroes = static_cast<int>(std::count_if(
            resolved.begin(), resolved.end(),
            [](const card_data::Card& card) { return !game_data::isHeroCard(card); }));
        drawText(window, font,
                 std::to_string(nonHeroes) + "/20 cards  |  " +
                 std::to_string(static_cast<int>(editingDeck.deck.cardTitles.size()) - nonHeroes) + " heroes",
                 15, {24.0f, 548.0f}, Muted);
    }

    void drawEvent(sf::RenderWindow& window)
    {
        drawButton(window, font, rect(20, 24, 112, 36), "Events",
                   hovered(rect(20, 24, 112, 36), mousePosition));
        drawButton(window, font, rect(666, 24, 114, 36), "Refresh",
                   hovered(rect(666, 24, 114, 36), mousePosition), !pendingState);
        const bool canForceEnd = accountIsAdmin && eventState.summary.id != 0 &&
            !eventState.summary.name.empty() &&
            eventState.summary.phase != conquest_data::EventPhase::Complete;
        if (canForceEnd)
        {
            drawButton(window, font, rect(530, 24, 124, 36), "Force End",
                       hovered(rect(530, 24, 124, 36), mousePosition), !pendingCommand, false, true);
        }
        drawText(window, font,
                  elide(font, eventState.summary.name.empty() ? "Loading campaign..." : eventState.summary.name,
                       22, canForceEnd ? 365.0f : 500.0f),
                 22, {150.0f, 13.0f}, Accent);
        if (!eventState.summary.name.empty())
        {
            const conquest_data::EventPhase phase = eventState.summary.phase;
            drawBadge(window, font, {150.0f, 44.0f}, phaseBadge(phase), phaseColor(phase));
            sf::Text badgeProbe(font, phaseBadge(phase), 11);
            float detailX = 150.0f + badgeProbe.getLocalBounds().size.x + 16.0f + 10.0f;
            if (eventState.summary.turn > 0)
            {
                drawText(window, font, "Turn " + std::to_string(eventState.summary.turn), 13,
                         {detailX, 46.0f}, Muted);
                detailX += 56.0f;
            }

            // The countdown was previously shown only during registration, so a
            // planning turn gave no hint that orders were about to auto-resolve
            // -- the single most important number on this screen.
            const std::int64_t deadline = phase == conquest_data::EventPhase::Registration
                ? eventState.summary.registrationEndsAt : eventState.summary.turnEndsAt;
            const std::string duration = remainingDurationText(deadline);
            if (!duration.empty())
            {
                const std::string caption = deadlineCaption(phase);
                const bool urgent = duration.find('d') == std::string::npos &&
                    duration.find('h') == std::string::npos;
                drawText(window, font, caption, 11, {detailX, 40.0f}, sf::Color(150, 132, 104));
                drawText(window, font, duration, 15, {detailX, 53.0f}, urgent ? Bad : Ink);
            }
        }
        else
        {
            drawPanel(window, rect(20, 78, 760, 485));
            drawLoadingState(window, rect(20, 78, 760, 485),
                             "Loading campaign...", "Reading the latest map and battle state.");
            return;
        }

        // The sprite used to cover the plate exactly, hiding its brass frame and
        // leaving the map as a bare rectangle. Inset so the frame reads as a
        // mount around the art, the way every other panel on the screen does.
        drawPanel(window, rect(MapPosition.x, MapPosition.y, MapSize.x, MapSize.y));
        if (mapTexture)
        {
            drawContainSprite(window, *mapTexture, mapArtBounds(), sf::Color(225, 225, 225));
        }
        drawOwnership(window);
        drawRegionMarkers(window);
        drawEventDeckPanel(window);
        drawBattlePanel(window);
    }

    void drawForceEndConfirmation(sf::RenderWindow& window)
    {
        sf::RectangleShape shade({ui_canvas::Width, ui_canvas::Height});
        shade.setPosition({ui_canvas::Left, 0.0f});
        shade.setFillColor(sf::Color(0, 0, 0, 185));
        window.draw(shade);
        drawPanel(window, rect(170, 190, 460, 250), sf::Color(22, 18, 16, 252));
        drawText(window, font, "Force End Conquest?", 25, {205.0f, 215.0f}, Bad);
        drawText(window, font,
                 "This campaign will end immediately with no winner reward.",
                 15, {205.0f, 265.0f}, Ink);
        drawText(window, font,
                 std::to_string(eventState.summary.participantCount) +
                     (eventState.summary.participantCount == 1 ? " player receives " : " players receive ") +
                     std::to_string(conquest_data::ConquestEntryFeeCoins) + " coins back each.",
                 15, {205.0f, 295.0f}, Accent);
        drawText(window, font, "This cannot be undone.", 14, {205.0f, 326.0f}, Muted);
        drawButton(window, font, rect(220, 370, 160, 42), "Cancel",
                   hovered(rect(220, 370, 160, 42), mousePosition), !pendingCommand);
        drawButton(window, font, rect(420, 370, 160, 42), "End Conquest",
                   hovered(rect(420, 370, 160, 42), mousePosition), !pendingCommand, false, true);
    }

    void drawOwnership(sf::RenderWindow& window)
    {
        for (const conquest_data::RegionState& region : eventState.regions)
        {
            const std::optional<std::uint8_t> colorIndex = colorForUsername(region.controller);
            const conquest_map::RegionDefinition* definition =
                conquest_map::region(region.regionId);
            if (!colorIndex || !definition)
            {
                continue;
            }
            drawControlFlag(
                window,
                mapPoint(definition->centerX, definition->centerY),
                playerColor(*colorIndex));
        }
    }

    void drawRegionMarkers(sf::RenderWindow& window)
    {
        // Secret orders are visible only for this client in EventState and in
        // the local plan. Draw every projected route so the whole simultaneous
        // turn can be reviewed before it is committed.
        for (const conquest_data::EventDeckState& deck : eventState.decks)
        {
            if (deck.owner != username || !deck.deployed || deck.eliminated)
            {
                continue;
            }
            const auto order = plannedOrders.find(deck.id);
            const int destination = order == plannedOrders.end() ? deck.regionId : order->second;
            if (destination == deck.regionId)
            {
                continue;
            }
            const conquest_map::RegionDefinition* originRegion = conquest_map::region(deck.regionId);
            const conquest_map::RegionDefinition* destinationRegion = conquest_map::region(destination);
            if (!originRegion || !destinationRegion)
            {
                continue;
            }
            const sf::Vector2f origin = mapPoint(originRegion->centerX, originRegion->centerY);
            const sf::Vector2f target = mapPoint(destinationRegion->centerX, destinationRegion->centerY);
            const sf::Vector2f delta = target - origin;
            const float length = std::hypot(delta.x, delta.y);
            sf::RectangleShape route({length, 2.5f});
            route.setOrigin({0.0f, 1.25f});
            route.setPosition(origin);
            route.setRotation(sf::radians(std::atan2(delta.y, delta.x)));
            route.setFillColor(sf::Color(255, 230, 150, 205));
            window.draw(route);

            sf::CircleShape destinationMarker(7.0f);
            destinationMarker.setOrigin({7.0f, 7.0f});
            destinationMarker.setPosition(target);
            destinationMarker.setFillColor(sf::Color(255, 230, 150, 70));
            destinationMarker.setOutlineThickness(2.0f);
            destinationMarker.setOutlineColor(Accent);
            window.draw(destinationMarker);
        }

        // The map art already names every territory just below its centre. A
        // numbered disc drawn at that centre sat straight on top of the label
        // in all twenty regions, and the id it showed is not something a player
        // ever needs -- the battle panel names regions now. So: no discs, no
        // numbers. Uncontrolled regions get a small neutral pin, controlled
        // ones are marked by their owner's flag, and selection is shown by a
        // caret above the region where nothing can collide with it.
        for (const conquest_map::RegionDefinition& region : conquest_map::DarkRealmsRegions)
        {
            const sf::Vector2f center = mapPoint(region.centerX, region.centerY);
            if (regionController(region.id).empty())
            {
                sf::CircleShape pin(4.0f);
                pin.setOrigin({4.0f, 4.0f});
                pin.setPosition(center + sf::Vector2f(0.0f, -4.0f));
                pin.setFillColor(sf::Color(14, 16, 16, 215));
                pin.setOutlineThickness(1.25f);
                pin.setOutlineColor(sf::Color(196, 174, 130, 225));
                window.draw(pin);
            }
            if (selectedRegionId == region.id)
            {
                drawSelectionHalo(window, center);
            }
        }

        // Every deck in a region used to draw at one fixed offset, so a stack of
        // armies rendered as a single dot. They now fan out in a row beside the
        // flag, clear of both the cloth and the region's baked-in name.
        std::unordered_map<int, int> piecesInRegion;
        for (const conquest_data::EventDeckState& deck : eventState.decks)
        {
            if (!deck.deployed || deck.eliminated || deck.regionId == 0)
            {
                continue;
            }
            const conquest_map::RegionDefinition* region = conquest_map::region(deck.regionId);
            const std::optional<std::uint8_t> colorIndex = colorForUsername(deck.owner);
            if (!region || !colorIndex)
            {
                continue;
            }
            const int slot = piecesInRegion[deck.regionId]++;
            const bool isSelected = selectedEventDeckId == deck.id;
            sf::CircleShape piece(4.5f);
            piece.setOrigin({4.5f, 4.5f});
            piece.setPosition(
                mapPoint(region->centerX, region->centerY) +
                sf::Vector2f(17.0f + static_cast<float>(slot) * 10.0f, -13.0f));
            piece.setFillColor(playerColor(*colorIndex));
            piece.setOutlineThickness(isSelected ? 2.0f : 1.0f);
            piece.setOutlineColor(isSelected ? Accent : sf::Color(10, 10, 10, 230));
            window.draw(piece);
        }

        for (const conquest_data::StartingPlacement& placement : placements)
        {
            const conquest_map::RegionDefinition* region = conquest_map::region(placement.regionId);
            if (!region)
            {
                continue;
            }
            const int slot = piecesInRegion[placement.regionId]++;
            sf::CircleShape piece(5.5f);
            piece.setOrigin({5.5f, 5.5f});
            piece.setPosition(
                mapPoint(region->centerX, region->centerY) +
                sf::Vector2f(17.0f + static_cast<float>(slot) * 10.0f, -13.0f));
            piece.setFillColor(Good);
            piece.setOutlineThickness(1.5f);
            piece.setOutlineColor(Accent);
            window.draw(piece);
        }
    }

    // Selection as a soft brass halo bloomed around the region rather than a
    // marker placed near it. Anything drawn above a region lands on the label of
    // the region above -- the map is packed tightly enough that only a glow
    // centred on the territory itself collides with nothing.
    void drawSelectionHalo(sf::RenderWindow& window, sf::Vector2f regionCenter)
    {
        // Biased up a few pixels: centred exactly, the rim reached the top of
        // the region's baked-in name and the flag pole beside it.
        const sf::Vector2f origin = regionCenter + sf::Vector2f(0.0f, -3.0f);
        constexpr int Rings = 6;
        for (int ring = Rings; ring > 0; --ring)
        {
            const float radius = 7.0f + static_cast<float>(ring) * 3.2f;
            const auto alpha = static_cast<std::uint8_t>(9 + (Rings - ring) * 5);
            sf::CircleShape glow(radius);
            glow.setOrigin({radius, radius});
            glow.setPosition(origin);
            glow.setFillColor(sf::Color(Accent.r, Accent.g, Accent.b, alpha));
            window.draw(glow);
        }

        sf::CircleShape rim(8.0f);
        rim.setOrigin({8.0f, 8.0f});
        rim.setPosition(origin);
        rim.setFillColor(sf::Color::Transparent);
        rim.setOutlineThickness(1.5f);
        rim.setOutlineColor(sf::Color(Accent.r, Accent.g, Accent.b, 225));
        window.draw(rim);
    }

    void drawEventDeckPanel(sf::RenderWindow& window)
    {
        drawPanel(window, rect(588, 78, 192, 373));
        drawText(window, font, joinedEvent() ? "Your Army" : "Starting Army", 18,
                 {598.0f, 88.0f}, Accent);
        const conquest_data::PlayerState* player = currentPlayer();
        if (player)
        {
            const std::string armyStatus = player->eliminated
                ? "Army defeated"
                : std::to_string(player->controlledRegions) + " regions held, " +
                    std::to_string(player->reinforcementsAvailable) + " in reserve";
            drawText(window, font, armyStatus, 12, {598.0f, 115.0f},
                     player->eliminated ? Bad : Muted);
            if (!player->eliminated && player->nextReinforcementAt > 0)
            {
                const std::int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                if (player->nextReinforcementAt > now)
                {
                    drawText(window, font,
                             "Cooldown: " + remainingText(player->nextReinforcementAt),
                             11, {598.0f, 130.0f}, Muted);
                }
            }
        }
        else
        {
            drawText(window, font, "Choose 1-2 adjacent edges", 12, {598.0f, 115.0f}, Muted);
        }

        if (joinedEvent())
        {
            const std::vector<const conquest_data::EventDeckState*> eventDecks = selectableEventDecks();
            for (std::size_t row = 0; row < 7; ++row)
            {
                const std::size_t index = eventDeckOffset + row;
                if (index >= eventDecks.size())
                {
                    break;
                }
                const auto& deck = *eventDecks[index];
                const bool isSelected = selectedEventDeckId == deck.id;
                const sf::FloatRect bounds = rect(590, 146 + row * 42.0f, 188, 38);
                drawBeveledPlate(
                    window, bounds.position, bounds.size,
                    isSelected ? sf::Color(76, 49, 25, 240) : sf::Color(17, 24, 25, 228),
                    isSelected ? Accent : sf::Color(96, 68, 38), isSelected, 4.0f);

                // Region names, not "R5>R6": the shorthand was internal
                // notation leaking into a player-facing panel.
                std::string state = "In reserve";
                bool moving = false;
                if (deck.deployed)
                {
                    const auto order = plannedOrders.find(deck.id);
                    const int destination = order == plannedOrders.end()
                        ? deck.regionId : order->second;
                    state = regionName(deck.regionId);
                    if (destination != deck.regionId)
                    {
                        state += " to " + regionName(destination);
                        moving = true;
                    }
                }
                drawText(window, font, elide(font, deck.deckName, 13, 172.0f), 13,
                         bounds.position + sf::Vector2f(8.0f, 4.0f));
                drawText(window, font, elide(font, state, 11, 172.0f), 11,
                         bounds.position + sf::Vector2f(8.0f, 21.0f), moving ? Accent : Muted);
            }
        }
        else
        {
            const std::vector<const conquest_data::ConquestDeck*> armyDecks = armyDeckList();
            for (std::size_t row = 0; row < 7; ++row)
            {
                const std::size_t index = eventDeckOffset + row;
                if (index >= armyDecks.size())
                {
                    break;
                }
                const auto& deck = *armyDecks[index];
                const bool isSelected =
                    selectedEventDeckId == static_cast<std::uint64_t>(deck.id);
                const sf::FloatRect bounds = rect(590, 146 + row * 42.0f, 188, 38);
                drawBeveledPlate(
                    window, bounds.position, bounds.size,
                    isSelected ? sf::Color(76, 49, 25, 240) : sf::Color(17, 24, 25, 228),
                    isSelected ? Accent : sf::Color(96, 68, 38), isSelected, 4.0f);
                drawText(window, font, elide(font, deck.deck.name, 13, 172.0f), 13,
                         bounds.position + sf::Vector2f(8.0f, 4.0f));
                drawText(window, font, std::to_string(deck.deck.cardTitles.size()) + " cards", 11,
                         bounds.position + sf::Vector2f(8.0f, 21.0f), Muted);
            }
        }

        // The flags on the map are the only colour key there is, and nothing
        // said which colour was yours. The slack below the deck rows is enough
        // for a proper roster, which also carries who has locked in orders.
        const std::size_t rowsShown = std::min<std::size_t>(
            7, joinedEvent() ? selectableEventDecks().size() : armyDeckList().size());
        drawPlayerLegend(window, 146.0f + static_cast<float>(rowsShown) * 42.0f + 18.0f);

        std::string actionLabel;
        bool actionEnabled = !pendingCommand;
        if (eventState.summary.phase == conquest_data::EventPhase::Registration)
        {
            actionLabel = joinedEvent()
                ? "Waiting for Start"
                : "Join - " + std::to_string(conquest_data::ConquestEntryFeeCoins) + " Coins";
            actionEnabled = actionEnabled && !joinedEvent() && !placements.empty();
        }
        else if (eventState.summary.phase == conquest_data::EventPhase::Planning && joinedEvent())
        {
            const conquest_data::PlayerState* me = currentPlayer();
            if (me && me->eliminated)
            {
                actionLabel = "Eliminated";
                actionEnabled = false;
            }
            else
            {
                actionLabel = "Submit Orders";
            }
        }
        else if (eventState.summary.phase == conquest_data::EventPhase::Resolving)
        {
            actionLabel = "Battles Resolving";
            actionEnabled = false;
        }
        else if (eventState.summary.phase == conquest_data::EventPhase::Complete)
        {
            actionLabel = eventState.summary.winner.empty()
                ? "Campaign Complete"
                : eventState.summary.winner == username
                    ? "You Won +" +
                        std::to_string(conquest_data::ConquestWinnerRewardCoins)
                    : eventState.summary.winner + " Won";
            actionEnabled = false;
        }
        else
        {
            actionLabel = "Registration Closed";
            actionEnabled = false;
        }
        drawButton(window, font, rect(606, 414, 158, 36), actionLabel,
                   hovered(rect(606, 414, 158, 36), mousePosition), actionEnabled, true);
    }

    // Campaign roster: colour swatch, name, regions held, and whether that
    // player has committed this turn's orders. Stops short of the action button.
    void drawPlayerLegend(sf::RenderWindow& window, float top)
    {
        if (eventState.players.empty())
        {
            return;
        }
        // Sized so a full twelve-seat roster is not silently truncated at the
        // action button; dropping a player from the key is worse than tight rows.
        constexpr float RowHeight = 17.0f;
        constexpr float BottomLimit = 409.0f;
        if (top + 16.0f + RowHeight > BottomLimit)
        {
            return;
        }

        drawText(window, font, "WARLORDS", 10, {598.0f, top}, sf::Color(150, 132, 104));
        float y = top + 15.0f;
        for (const conquest_data::PlayerState& player : eventState.players)
        {
            if (y + RowHeight > BottomLimit)
            {
                break;
            }
            const sf::Color color = playerColor(player.colorIndex);
            const bool isYou = player.username == username;

            sf::RectangleShape swatch({8.0f, 8.0f});
            swatch.setPosition({598.0f, y + 3.0f});
            swatch.setFillColor(player.eliminated ? sf::Color(color.r, color.g, color.b, 90) : color);
            swatch.setOutlineThickness(1.0f);
            swatch.setOutlineColor(sf::Color(12, 12, 12, 200));
            window.draw(swatch);

            const std::string name = isYou ? player.username + " (you)" : player.username;
            drawText(window, font, elide(font, name, 12, 108.0f), 12, {612.0f, y - 1.0f},
                     player.eliminated ? sf::Color(120, 116, 110) : (isYou ? Accent : Ink));

            if (player.eliminated)
            {
                drawTextRight(window, font, "out", 11, {772.0f, y}, sf::Color(120, 116, 110));
            }
            else
            {
                drawTextRight(window, font, std::to_string(player.controlledRegions), 12,
                              {772.0f, y - 1.0f}, Muted);
                // A filled tick means this player's orders are already locked in.
                if (player.ordersSubmitted &&
                    eventState.summary.phase == conquest_data::EventPhase::Planning)
                {
                    sf::CircleShape done(3.0f);
                    done.setOrigin({3.0f, 3.0f});
                    done.setPosition({754.0f, y + 6.0f});
                    done.setFillColor(Good);
                    window.draw(done);
                }
            }
            y += RowHeight;
        }
    }

    void drawBattlePanel(sf::RenderWindow& window)
    {
        drawPanel(window, rect(20, 462, 760, 101));
        const conquest_map::RegionDefinition* region = selectedRegionId
            ? conquest_map::region(*selectedRegionId) : nullptr;
        const std::string controller = selectedRegionId ? regionController(*selectedRegionId) : "";
        // The region id was exposed as a "(6)" suffix; the map already labels
        // every territory by name, so the number told the player nothing.
        // This heading has to clear the panel's inner hairline above it *and*
        // the battle rows below, which start at 492. 14px at y=470 fits both.
        drawText(window, font, region ? std::string(region->name) : "Ready Battles",
                 14, {30.0f, 470.0f}, Accent);
        if (region)
        {
            sf::Text nameProbe(font, std::string(region->name), 14);
            const float captionX = 30.0f + nameProbe.getLocalBounds().size.x + 14.0f;
            if (controller.empty())
            {
                drawBadge(window, font, {captionX, 469.0f}, "UNCLAIMED", Muted, 10);
            }
            else
            {
                const std::optional<std::uint8_t> colorIndex = colorForUsername(controller);
                drawBadge(window, font, {captionX, 469.0f}, controller,
                          colorIndex ? playerColor(*colorIndex) : Muted, 10);
            }
        }
        const conquest_data::EventDeckState* selected = selectedEventDeckId
            ? eventDeck(*selectedEventDeckId) : nullptr;
        if (accountIsAdmin &&
            eventState.summary.phase == conquest_data::EventPhase::Registration)
        {
            const bool enabled = !pendingCommand && eventState.summary.participantCount >= 2;
            if (!enabled && eventState.summary.participantCount < 2)
            {
                drawText(window, font, "Requires 2 players", 11, {616.0f, 502.0f}, Muted);
            }
            drawButton(window, font, rect(606, 520, 158, 32), "Force Start",
                       hovered(rect(606, 520, 158, 32), mousePosition), enabled);
        }
        if (eventState.summary.phase == conquest_data::EventPhase::Planning &&
            selected && !selected->deployed && !selected->eliminated)
        {
            const conquest_data::PlayerState* me = currentPlayer();
            const std::int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            const bool enabled = !pendingCommand && selectedRegionId && me && !me->eliminated &&
                me->reinforcementsAvailable > 0 && me->nextReinforcementAt <= now;
            drawButton(window, font, rect(606, 520, 158, 32), "Deploy Reserve",
                       hovered(rect(606, 520, 158, 32), mousePosition), enabled);
        }
        const std::vector<const conquest_data::BattleState*> battles = joinableBattles();
        if (battles.empty())
        {
            drawText(window, font, "No battles are waiting for you this turn.", 14, {30.0f, 502.0f}, Muted);
            return;
        }
        for (std::size_t row = 0; row < 2; ++row)
        {
            const std::size_t index = battleOffset + row;
            if (index >= battles.size())
            {
                break;
            }
            const auto& battle = *battles[index];
            const sf::FloatRect bounds = rect(24, 492 + row * 33.0f, 552, 29);
            drawButton(window, font, bounds,
                       elide(font, battle.deckOneName + "  vs  " + battle.deckTwoName, 14, 420.0f),
                       hovered(bounds, mousePosition));
        }
        drawText(window, font,
                 battles.size() == 1 ? "1 battle ready" : std::to_string(battles.size()) + " battles ready",
                 13, {600.0f, 505.0f}, Good);
    }
};
}
