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

namespace
{
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
constexpr float EventRowY = 112.0f;
constexpr float EventRowHeight = 54.0f;
constexpr std::size_t VisibleEventRows = 7;
constexpr float LoadoutRowY = 126.0f;
constexpr float LoadoutRowHeight = 40.0f;
constexpr std::size_t VisibleLoadoutRows = 8;
constexpr float CardRowY = 147.0f;
constexpr float CardRowHeight = 30.0f;
constexpr std::size_t VisibleCardRows = 11;

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
    window.draw(text);
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

void drawButton(
    sf::RenderWindow& window,
    sf::Font& font,
    sf::FloatRect bounds,
    const std::string& label,
    bool hovered,
    bool enabled = true)
{
    const sf::Color fill = !enabled
        ? sf::Color(45, 48, 47, 235)
        : hovered ? sf::Color(88, 54, 27, 248) : sf::Color(39, 31, 24, 246);
    const sf::Color outline = !enabled ? sf::Color(82, 82, 78) : hovered ? Accent : Line;
    drawBeveledPlate(window, bounds.position, bounds.size, fill, outline, hovered && enabled, 7.0f);
    sf::Text text(font, label, bounds.size.y <= 32.0f ? 15u : 18u);
    text.setFillColor(enabled ? Ink : sf::Color(126, 126, 120));
    centerButtonText(text, bounds.position + bounds.size * 0.5f);
    window.draw(text);
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
    if (days > 0)
    {
        return std::to_string(days) + "d " + std::to_string(hours) + "h";
    }
    if (hours > 0)
    {
        return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
    }
    const std::int64_t remainingSeconds = seconds % 60;
    if (minutes > 0)
    {
        return std::to_string(minutes) + "m " +
            std::to_string(remainingSeconds) + "s";
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

sf::Color playerColor(std::uint8_t index, std::uint8_t alpha = 255)
{
    const conquest_map::PlayerColor color =
        conquest_map::PlayerColors[index % conquest_map::PlayerColors.size()];
    return {color.red, color.green, color.blue, alpha};
}

sf::Color brighten(sf::Color color, int amount)
{
    const auto channel = [amount](std::uint8_t value) {
        return static_cast<std::uint8_t>(std::clamp(static_cast<int>(value) + amount, 0, 255));
    };
    return {channel(color.r), channel(color.g), channel(color.b), color.a};
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
        view = View::Events;
        status.clear();
        statusSuccess = true;
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

        if (const auto* key = event.getIf<sf::Event::KeyPressed>())
        {
            if (key->code == sf::Keyboard::Key::Escape)
            {
                goBack();
                return true;
            }
        }

        if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>())
        {
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
        pollRequests();
    }

    void draw(sf::RenderWindow& window)
    {
        sf::RectangleShape shade({800.0f, 600.0f});
        shade.setFillColor(sf::Color(5, 8, 9, 225));
        window.draw(shade);

        // The event detail view supplies its own two-line toolbar. Drawing the
        // generic Conquest banner behind it makes the title, phase, and nav
        // controls compete for the same pixels.
        if (view != View::Event)
        {
            drawText(window, font, "CONQUEST", 32, {24.0f, 18.0f}, Accent);
            drawText(window, font, "Long-running campaigns. Each card copy belongs to one Conquest deck.",
                     15, {212.0f, 31.0f}, Muted);
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
        else
        {
            drawEvent(window);
        }

        if (!status.empty())
        {
            drawText(window, font, elide(font, status, 15, 750.0f), 15,
                     {24.0f, 574.0f}, statusSuccess ? Good : Bad);
        }
        if (busy())
        {
            drawText(window, font, "Working...", 15, {692.0f, 574.0f}, Accent);
        }
    }

private:
    static void drawText(
        sf::RenderWindow& window,
        sf::Font& font,
        const std::string& value,
        unsigned int size,
        sf::Vector2f position,
        sf::Color color = Ink)
    {
        ::drawText(window, font, value, size, position, color);
    }

    static void drawPanel(
        sf::RenderWindow& window,
        sf::FloatRect bounds,
        sf::Color fill = Panel)
    {
        ::drawPanel(window, bounds, fill);
    }

    enum class View
    {
        Events,
        Loadout,
        DeckEdit,
        Event
    };

    enum class CommandKind
    {
        None,
        Join,
        StartEvent,
        Orders,
        Reinforce,
        DeleteDeck
    };

    sf::Font& font;
    TextureStore& textures;
    sf::Texture* mapTexture = nullptr;
    InputBox deckNameInput;
    View view = View::Events;
    std::string accessToken;
    std::string username;
    bool accountIsAdmin = false;
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
        else if (rect(410, 492, 166, 38).contains(mouse) && selectedDeck)
        {
            toggleArmyDeck(decks[*selectedDeck].id);
        }
        else if (rect(584, 492, 186, 38).contains(mouse) && !pendingArmySave)
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
            const int perDeckLimit = game_data::isHeroCard(card)
                ? game_data::MaxHeroCopies : game_data::MaxCardCopies;
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

    sf::Vector2f mapPoint(int x, int y) const
    {
        return {
            MapPosition.x + static_cast<float>(x) / conquest_map::DarkRealmsImageWidth * MapSize.x,
            MapPosition.y + static_cast<float>(y) / conquest_map::DarkRealmsImageHeight * MapSize.y};
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
                    rect(590, 146 + row * 35.0f, 190, 31).contains(mouse))
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
                    rect(590, 146 + row * 35.0f, 190, 31).contains(mouse))
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
            if (index < battles.size() && rect(24, 486 + row * 34.0f, 552, 30).contains(mouse))
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
        drawPanel(window, rect(20, 104, 760, 444));
        if (events.empty() && !pendingEvents)
        {
            drawText(window, font, "No Conquest events are available.", 19, {48.0f, 140.0f}, Muted);
        }
        for (std::size_t row = 0; row < VisibleEventRows; ++row)
        {
            const std::size_t index = eventOffset + row;
            if (index >= events.size())
            {
                break;
            }
            const conquest_data::EventSummary& event = events[index];
            const sf::FloatRect bounds = rect(34, EventRowY + row * EventRowHeight, 732, EventRowHeight - 6);
            sf::RectangleShape background(bounds.size);
            background.setPosition(bounds.position);
            background.setFillColor(hovered(bounds, mousePosition) ? sf::Color(67, 50, 31, 230) : PanelAlt);
            background.setOutlineThickness(1.0f);
            background.setOutlineColor(event.joined ? Good : Line);
            window.draw(background);
            drawText(window, font, event.name, 18, bounds.position + sf::Vector2f(12.0f, 5.0f));
            drawText(window, font,
                     phaseName(event.phase) + "  |  " + std::to_string(event.participantCount) + " players" +
                         (event.joined ? "  |  Joined" : ""),
                     14, bounds.position + sf::Vector2f(12.0f, 28.0f), event.joined ? Good : Muted);
            const std::int64_t deadline = event.phase == conquest_data::EventPhase::Registration
                ? event.registrationEndsAt : event.turnEndsAt;
            drawText(window, font, remainingText(deadline), 14,
                     bounds.position + sf::Vector2f(530.0f, 16.0f), Muted);
        }
        drawText(window, font, "Select an event to inspect its map, join, plan moves, or resume battles.",
                 14, {36.0f, 554.0f}, Muted);
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
        drawPanel(window, rect(20, 108, 360, 370));
        drawPanel(window, rect(400, 108, 380, 370));
        drawText(window, font, "Conquest Decks", 21, {32.0f, 108.0f}, Accent);
        drawText(window, font, "Army (1-10 decks)", 21, {412.0f, 108.0f}, Accent);

        for (std::size_t row = 0; row < VisibleLoadoutRows; ++row)
        {
            const std::size_t index = deckOffset + row;
            if (index >= decks.size())
            {
                break;
            }
            const sf::FloatRect bounds = rect(30, LoadoutRowY + row * LoadoutRowHeight, 340, LoadoutRowHeight - 4);
            sf::RectangleShape background(bounds.size);
            background.setPosition(bounds.position);
            background.setFillColor(selectedDeck == index ? sf::Color(91, 60, 29, 245) : PanelAlt);
            background.setOutlineThickness(1.0f);
            background.setOutlineColor(selectedDeck == index ? Accent : Line);
            window.draw(background);
            drawText(window, font, elide(font, decks[index].deck.name, 17, 230.0f), 17,
                     bounds.position + sf::Vector2f(9.0f, 7.0f));
            drawText(window, font, std::to_string(decks[index].deck.cardTitles.size()) + " cards", 13,
                     bounds.position + sf::Vector2f(260.0f, 10.0f), Muted);
        }

        for (std::size_t row = 0; row < VisibleLoadoutRows; ++row)
        {
            const std::size_t slot = armyOffset + row;
            if (slot >= army.deckIds.size())
            {
                break;
            }
            const sf::FloatRect bounds = rect(410, LoadoutRowY + row * LoadoutRowHeight, 360, LoadoutRowHeight - 4);
            sf::RectangleShape background(bounds.size);
            background.setPosition(bounds.position);
            background.setFillColor(selectedArmySlot == slot ? sf::Color(91, 60, 29, 245) : PanelAlt);
            background.setOutlineThickness(1.0f);
            background.setOutlineColor(selectedArmySlot == slot ? Accent : Line);
            window.draw(background);
            drawText(window, font, std::to_string(slot + 1) + ".  " +
                     elide(font, armyDeckName(army.deckIds[slot]), 17, 280.0f), 17,
                     bounds.position + sf::Vector2f(9.0f, 7.0f));
        }

        drawButton(window, font, rect(24, 492, 100, 38), "New",
                   hovered(rect(24, 492, 100, 38), mousePosition), !busy());
        drawButton(window, font, rect(132, 492, 100, 38), "Edit",
                   hovered(rect(132, 492, 100, 38), mousePosition), selectedDeck.has_value());
        drawButton(window, font, rect(240, 492, 124, 38), "Delete",
                   hovered(rect(240, 492, 124, 38), mousePosition), selectedDeck.has_value() && !busy());
        const bool selectedInArmy = selectedDeck &&
            std::find(army.deckIds.begin(), army.deckIds.end(), decks[*selectedDeck].id) != army.deckIds.end();
        drawButton(window, font, rect(410, 492, 166, 38),
                   selectedInArmy ? "Remove from Army" : "Add to Army",
                   hovered(rect(410, 492, 166, 38), mousePosition), selectedDeck.has_value());
        drawButton(window, font, rect(584, 492, 186, 38), "Save Army",
                   hovered(rect(584, 492, 186, 38), mousePosition),
                   !pendingArmySave && !army.deckIds.empty());
        drawText(window, font,
                 "Copies are shared across Conquest decks only. Regular decks never consume this allocation.",
                 14, {24.0f, 544.0f}, Muted);
    }

    void drawDeckEditor(sf::RenderWindow& window)
    {
        drawButton(window, font, rect(20, 24, 112, 36), "Back",
                   hovered(rect(20, 24, 112, 36), mousePosition));
        drawText(window, font, editingDeck.id == 0 ? "New Conquest Deck" : "Edit Conquest Deck",
                 23, {154.0f, 29.0f}, Accent);
        deckNameInput.draw(window);
        drawPanel(window, rect(20, 138, 350, 346));
        drawPanel(window, rect(388, 138, 392, 346));
        drawText(window, font, "Deck", 18, {30.0f, 138.0f}, Accent);
        drawText(window, font, "Available collection", 18, {398.0f, 138.0f}, Accent);

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
            const sf::FloatRect bounds = rect(24, CardRowY + row * CardRowHeight, 340, CardRowHeight - 3);
            sf::RectangleShape background(bounds.size);
            background.setPosition(bounds.position);
            background.setFillColor(selectedEditingTitle == index ? sf::Color(91, 60, 29, 245) : PanelAlt);
            window.draw(background);
            drawText(window, font, elide(font, titles[index], 15, 270.0f), 15,
                     bounds.position + sf::Vector2f(7.0f, 4.0f));
            drawText(window, font, "x" + std::to_string(copies), 14,
                     bounds.position + sf::Vector2f(302.0f, 5.0f), Accent);
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
            sf::RectangleShape background(bounds.size);
            background.setPosition(bounds.position);
            background.setFillColor(selectedLibraryCard == index ? sf::Color(91, 60, 29, 245) : PanelAlt);
            window.draw(background);
            drawText(window, font, elide(font, card.title, 15, 260.0f), 15,
                     bounds.position + sf::Vector2f(7.0f, 4.0f));
            drawText(window, font, std::to_string(committed) + "/" + std::to_string(owned), 13,
                     bounds.position + sf::Vector2f(321.0f, 6.0f), Muted);
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
        drawText(window, font,
                 elide(font, eventState.summary.name.empty() ? "Loading campaign..." : eventState.summary.name,
                       22, 500.0f),
                 22, {150.0f, 13.0f}, Accent);
        if (!eventState.summary.name.empty())
        {
            std::string phaseDetails = phaseName(eventState.summary.phase) + "  |  Turn " +
                std::to_string(eventState.summary.turn);
            if (eventState.summary.phase == conquest_data::EventPhase::Registration)
            {
                const std::string duration =
                    remainingDurationText(eventState.summary.registrationEndsAt);
                if (!duration.empty())
                {
                    phaseDetails += "  |  Starts in " + duration;
                }
            }
            drawText(window, font,
                     elide(font, phaseDetails, 14, 500.0f),
                     14, {150.0f, 47.0f}, Muted);
        }

        drawPanel(window, rect(MapPosition.x, MapPosition.y, MapSize.x, MapSize.y));
        if (mapTexture)
        {
            drawContainSprite(window, *mapTexture,
                {MapPosition, MapSize}, sf::Color(225, 225, 225));
        }
        drawOwnership(window);
        drawRegionMarkers(window);
        drawEventDeckPanel(window);
        drawBattlePanel(window);
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

        for (const conquest_map::RegionDefinition& region : conquest_map::DarkRealmsRegions)
        {
            const sf::Vector2f center = mapPoint(region.centerX, region.centerY);
            sf::CircleShape marker(selectedRegionId == region.id ? 11.0f : 8.0f);
            marker.setOrigin({marker.getRadius(), marker.getRadius()});
            marker.setPosition(center);
            marker.setFillColor(sf::Color(12, 12, 12, 205));
            marker.setOutlineThickness(selectedRegionId == region.id ? 3.0f : 1.5f);
            marker.setOutlineColor(selectedRegionId == region.id ? Accent : sf::Color(244, 222, 172));
            window.draw(marker);
            sf::Text label(font, std::to_string(region.id), 11);
            label.setFillColor(Ink);
            centerButtonText(label, center);
            window.draw(label);
        }

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
            sf::CircleShape piece(5.0f);
            piece.setOrigin({5.0f, 5.0f});
            piece.setPosition(mapPoint(region->centerX + 28, region->centerY + 2));
            piece.setFillColor(playerColor(*colorIndex));
            piece.setOutlineThickness(selectedEventDeckId == deck.id ? 3.0f : 1.0f);
            piece.setOutlineColor(selectedEventDeckId == deck.id ? Accent : sf::Color::Black);
            window.draw(piece);
        }

        for (const conquest_data::StartingPlacement& placement : placements)
        {
            const conquest_map::RegionDefinition* region = conquest_map::region(placement.regionId);
            if (!region)
            {
                continue;
            }
            sf::CircleShape piece(7.0f);
            piece.setOrigin({7.0f, 7.0f});
            piece.setPosition(mapPoint(region->centerX + 28, region->centerY + 2));
            piece.setFillColor(Good);
            piece.setOutlineThickness(2.0f);
            piece.setOutlineColor(Accent);
            window.draw(piece);
        }
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
                : std::to_string(player->controlledRegions) + " regions  |  " +
                    std::to_string(player->reinforcementsAvailable) + " deploys";
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
                const sf::FloatRect bounds = rect(590, 146 + row * 35.0f, 190, 31);
                sf::RectangleShape background(bounds.size);
                background.setPosition(bounds.position);
                background.setFillColor(selectedEventDeckId == deck.id ? sf::Color(91, 60, 29, 245) : PanelAlt);
                window.draw(background);
                std::string state = "reserve";
                if (deck.deployed)
                {
                    const auto order = plannedOrders.find(deck.id);
                    const int destination = order == plannedOrders.end()
                        ? deck.regionId : order->second;
                    state = "R" + std::to_string(deck.regionId);
                    if (destination != deck.regionId)
                    {
                        state += ">R" + std::to_string(destination);
                    }
                }
                drawText(window, font, elide(font, deck.deckName, 13, 118.0f), 13,
                         bounds.position + sf::Vector2f(5.0f, 6.0f));
                drawText(window, font, state, 11, bounds.position + sf::Vector2f(126.0f, 8.0f), Muted);
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
                const sf::FloatRect bounds = rect(590, 146 + row * 35.0f, 190, 31);
                sf::RectangleShape background(bounds.size);
                background.setPosition(bounds.position);
                background.setFillColor(selectedEventDeckId == static_cast<std::uint64_t>(deck.id)
                    ? sf::Color(91, 60, 29, 245) : PanelAlt);
                window.draw(background);
                drawText(window, font, elide(font, deck.deck.name, 13, 172.0f), 13,
                         bounds.position + sf::Vector2f(5.0f, 6.0f));
            }
        }

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
                   hovered(rect(606, 414, 158, 36), mousePosition), actionEnabled);
    }

    void drawBattlePanel(sf::RenderWindow& window)
    {
        drawPanel(window, rect(20, 462, 760, 101));
        const conquest_map::RegionDefinition* region = selectedRegionId
            ? conquest_map::region(*selectedRegionId) : nullptr;
        const std::string controller = selectedRegionId ? regionController(*selectedRegionId) : "";
        drawText(window, font,
                 region ? std::string(region->name) + " (" + std::to_string(region->id) + ")" : "Ready Battles",
                 15, {30.0f, 466.0f}, Accent);
        if (region)
        {
            drawText(window, font, controller.empty() ? "Unclaimed" : "Controlled by " + controller,
                     13, {278.0f, 468.0f}, Muted);
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
            drawText(window, font, "No tactical battles are waiting for you.", 14, {30.0f, 502.0f}, Muted);
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
            const sf::FloatRect bounds = rect(24, 486 + row * 34.0f, 552, 30);
            drawButton(window, font, bounds,
                       "Resume: " + elide(font, battle.deckOneName + " vs " + battle.deckTwoName, 14, 385.0f),
                       hovered(bounds, mousePosition));
        }
        drawText(window, font, std::to_string(battles.size()) + " asynchronous battle(s)",
                 13, {600.0f, 505.0f}, Good);

    }
};
}
