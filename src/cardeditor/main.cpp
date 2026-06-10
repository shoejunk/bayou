#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <fmt/core.h>

#include "../shared/card_data.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

import network;

using namespace network;

namespace
{
constexpr unsigned short CardServerPort = 55004;
constexpr float WindowWidth = 1280.0f;
constexpr float WindowHeight = 760.0f;

const sf::Color Ink(236, 239, 244);
const sf::Color Muted(152, 164, 181);
const sf::Color Panel(31, 36, 46);
const sf::Color PanelAlt(39, 46, 59);
const sf::Color Field(22, 27, 36);
const sf::Color Accent(89, 183, 169);
const sf::Color AccentDark(31, 106, 104);
const sf::Color Warn(221, 112, 92);
const sf::Color Line(75, 85, 102);

struct CardListResult
{
    bool success = false;
    std::string message;
    std::vector<card_data::Card> cards;
};

struct CommandResult
{
    bool success = false;
    std::string message;
};

std::string trim(const std::string& value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();

    if (first >= last)
    {
        return "";
    }

    return std::string(first, last);
}

std::vector<std::string> splitCommaSeparated(const std::string& line)
{
    std::vector<std::string> values;
    std::stringstream stream(line);
    std::string item;
    while (std::getline(stream, item, ','))
    {
        item = trim(item);
        if (!item.empty())
        {
            values.push_back(item);
        }
    }

    return values;
}

std::vector<std::string> splitSemicolonSeparated(const std::string& line)
{
    std::vector<std::string> values;
    std::stringstream stream(line);
    std::string item;
    while (std::getline(stream, item, ';'))
    {
        item = trim(item);
        if (!item.empty())
        {
            values.push_back(item);
        }
    }

    return values;
}

bool connectToServer(sf::TcpSocket& socket)
{
    return socket.connect(sf::IpAddress::LocalHost, CardServerPort) == sf::Socket::Status::Done;
}

CardListResult fetchCards()
{
    sf::TcpSocket socket;
    if (!connectToServer(socket))
    {
        return {false, "Failed to connect to card server"};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(MessageType::CardListRequest);
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        return {false, "Failed to send card list request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        return {false, "No response from card server"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    std::uint32_t count = 0;
    response >> responseType >> success >> message >> count;
    if (!response || static_cast<MessageType>(responseType) != MessageType::CardListResponse)
    {
        return {false, "Unexpected card list response"};
    }

    std::vector<card_data::Card> cards;
    cards.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        card_data::Card card;
        if (!card_data::readCard(response, card))
        {
            return {false, "Invalid card list payload"};
        }
        cards.push_back(card);
    }

    return {success, message, cards};
}

CommandResult saveCard(const card_data::Card& card)
{
    sf::TcpSocket socket;
    if (!connectToServer(socket))
    {
        return {false, "Failed to connect to card server"};
    }

    sf::Packet request;
    request << static_cast<std::uint8_t>(MessageType::CardUpsertRequest);
    card_data::writeCard(request, card);
    if (socket.send(request) != sf::Socket::Status::Done)
    {
        return {false, "Failed to send card save request"};
    }

    sf::Packet response;
    if (socket.receive(response) != sf::Socket::Status::Done)
    {
        return {false, "No response from card server"};
    }

    std::uint8_t responseType = 0;
    bool success = false;
    std::string message;
    response >> responseType >> success >> message;
    if (!response || static_cast<MessageType>(responseType) != MessageType::CardUpsertResponse)
    {
        return {false, "Unexpected card save response"};
    }

    return {success, message};
}

std::string joinStrings(const std::vector<std::string>& values, const std::string& separator)
{
    std::string result;
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0)
        {
            result += separator;
        }
        result += values[i];
    }
    return result;
}

std::string integerPairsToText(const std::vector<card_data::KeyIntPair>& values)
{
    std::vector<std::string> parts;
    for (const card_data::KeyIntPair& item : values)
    {
        parts.push_back(fmt::format("{}={}", item.key, item.value));
    }
    return joinStrings(parts, "; ");
}

std::string stringPairsToText(const std::vector<card_data::KeyStringPair>& values)
{
    std::vector<std::string> parts;
    for (const card_data::KeyStringPair& item : values)
    {
        parts.push_back(fmt::format("{}={}", item.key, item.value));
    }
    return joinStrings(parts, "; ");
}

std::string stringListsToText(const std::vector<card_data::KeyStringList>& values)
{
    std::vector<std::string> parts;
    for (const card_data::KeyStringList& item : values)
    {
        parts.push_back(fmt::format("{}={}", item.key, joinStrings(item.values, ",")));
    }
    return joinStrings(parts, "; ");
}

std::vector<card_data::KeyIntPair> parseIntegerPairs(const std::string& text)
{
    std::vector<card_data::KeyIntPair> values;
    for (const std::string& entry : splitSemicolonSeparated(text))
    {
        const std::size_t delimiter = entry.find('=');
        if (delimiter == std::string::npos)
        {
            continue;
        }

        const std::string key = trim(entry.substr(0, delimiter));
        const std::string value = trim(entry.substr(delimiter + 1));
        if (key.empty())
        {
            continue;
        }

        try
        {
            values.push_back({key, std::stoi(value)});
        }
        catch (const std::exception&)
        {
        }
    }
    return values;
}

std::vector<card_data::KeyStringPair> parseStringPairs(const std::string& text)
{
    std::vector<card_data::KeyStringPair> values;
    for (const std::string& entry : splitSemicolonSeparated(text))
    {
        const std::size_t delimiter = entry.find('=');
        if (delimiter == std::string::npos)
        {
            continue;
        }

        const std::string key = trim(entry.substr(0, delimiter));
        const std::string value = trim(entry.substr(delimiter + 1));
        if (!key.empty())
        {
            values.push_back({key, value});
        }
    }
    return values;
}

std::vector<card_data::KeyStringList> parseStringLists(const std::string& text)
{
    std::vector<card_data::KeyStringList> values;
    for (const std::string& entry : splitSemicolonSeparated(text))
    {
        const std::size_t delimiter = entry.find('=');
        if (delimiter == std::string::npos)
        {
            continue;
        }

        const std::string key = trim(entry.substr(0, delimiter));
        if (!key.empty())
        {
            values.push_back({key, splitCommaSeparated(entry.substr(delimiter + 1))});
        }
    }
    return values;
}

card_data::Card makeSampleCard(const std::string& title, int revision)
{
    card_data::Card card;
    card.title = title;
    card.type = revision == 1 ? card_data::CardType::Unit : card_data::CardType::Spell;
    card.imagePath = fmt::format("assets/cards/{}_rev_{}.png", title, revision);
    card.keywords = {"starter", "sample", fmt::format("revision{}", revision)};
    card.integerValues = {{"cost", revision}, {"power", revision + 1}};
    card.stringValues = {{"faction", "bayou"}, {"rules", fmt::format("Sample rules revision {}", revision)}};
    card.stringLists = {{"tags", {"developer", "test"}}, {"slots", {"hand", "board"}}};
    return card;
}

int handleListCommand()
{
    const CardListResult result = fetchCards();
    if (!result.success)
    {
        fmt::println("{}", result.message);
        return 1;
    }

    if (result.cards.empty())
    {
        fmt::println("No cards found.");
        return 0;
    }

    for (const card_data::Card& card : result.cards)
    {
        fmt::println("{} ({})", card.title, card_data::toString(card.type));
        fmt::println("  image: {}", card.imagePath);
        fmt::println("  keywords: {}", joinStrings(card.keywords, ", "));
        fmt::println("  ints: {}", integerPairsToText(card.integerValues));
        fmt::println("  strings: {}", stringPairsToText(card.stringValues));
        fmt::println("  lists: {}", stringListsToText(card.stringLists));
    }
    return 0;
}

int handleSaveCommand(const card_data::Card& card)
{
    const CommandResult result = saveCard(card);
    fmt::println("{}", result.message);
    return result.success ? 0 : 1;
}

void centerText(sf::Text& text, const sf::Vector2f& center)
{
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin({bounds.position.x + bounds.size.x / 2.0f, bounds.position.y + bounds.size.y / 2.0f});
    text.setPosition(center);
}

void drawText(sf::RenderWindow& window, sf::Font& font, const std::string& value, unsigned int size, sf::Vector2f position, sf::Color color)
{
    sf::Text text(font, value, size);
    text.setFillColor(color);
    text.setPosition(position);
    window.draw(text);
}

void drawRoundedPanel(sf::RenderWindow& window, const sf::Vector2f& position, const sf::Vector2f& size, sf::Color fill, sf::Color outline = Line)
{
    sf::RectangleShape shape(size);
    shape.setPosition(position);
    shape.setFillColor(fill);
    shape.setOutlineThickness(1.0f);
    shape.setOutlineColor(outline);
    window.draw(shape);
}

class TextField
{
public:
    TextField() = default;

    TextField(sf::Font& font, std::string label, sf::Vector2f position, sf::Vector2f size)
        : box(size)
    {
        labelText.emplace(font, label, 15);
        valueText.emplace(font, "", 18);

        box.setPosition(position);
        box.setFillColor(Field);
        box.setOutlineThickness(1.0f);
        box.setOutlineColor(Line);

        labelText->setFillColor(Muted);
        labelText->setPosition({position.x, position.y - 22.0f});

        valueText->setFillColor(Ink);
        valueText->setPosition({position.x + 12.0f, position.y + 10.0f});
    }

    void setValue(const std::string& newValue)
    {
        value = newValue;
        refreshText();
    }

    const std::string& getValue() const
    {
        return value;
    }

    bool contains(sf::Vector2f point) const
    {
        return box.getGlobalBounds().contains(point);
    }

    void setActive(bool next)
    {
        active = next;
        box.setOutlineColor(active ? Accent : Line);
    }

    bool isActive() const
    {
        return active;
    }

    void handleText(sf::Event::TextEntered textEvent)
    {
        if (!active)
        {
            return;
        }

        const char c = static_cast<char>(textEvent.unicode);
        if (c == 8 && !value.empty())
        {
            value.pop_back();
        }
        else if (c >= 32 && c < 127)
        {
            value.push_back(c);
        }
        refreshText();
    }

    void draw(sf::RenderWindow& window) const
    {
        window.draw(*labelText);
        window.draw(box);
        window.draw(*valueText);
        if (active)
        {
            sf::RectangleShape cursor({1.5f, 22.0f});
            const sf::FloatRect textBounds = valueText->getGlobalBounds();
            cursor.setPosition({std::min(textBounds.position.x + textBounds.size.x + 2.0f, box.getPosition().x + box.getSize().x - 12.0f), box.getPosition().y + 9.0f});
            cursor.setFillColor(Accent);
            window.draw(cursor);
        }
    }

private:
    std::optional<sf::Text> labelText;
    std::optional<sf::Text> valueText;
    sf::RectangleShape box;
    std::string value;
    bool active = false;

    void refreshText()
    {
        std::string display = value;
        constexpr std::size_t MaxDisplayChars = 74;
        if (display.size() > MaxDisplayChars)
        {
            display = display.substr(display.size() - MaxDisplayChars);
        }
        valueText->setString(display);
    }
};

class Button
{
public:
    Button() = default;

    Button(sf::Font& font, std::string label, sf::Vector2f position, sf::Vector2f size, sf::Color color)
        : shape(size), base(color)
    {
        text.emplace(font, label, 18);
        shape.setPosition(position);
        shape.setFillColor(base);
        shape.setOutlineThickness(1.0f);
        shape.setOutlineColor(sf::Color(base.r + 20, base.g + 20, base.b + 20));
        text->setFillColor(Ink);
        centerText(*text, {position.x + size.x / 2.0f, position.y + size.y / 2.0f - 1.0f});
    }

    bool contains(sf::Vector2f point) const
    {
        return shape.getGlobalBounds().contains(point);
    }

    void update(sf::Vector2f mouse)
    {
        const bool hovered = contains(mouse);
        shape.setFillColor(hovered ? sf::Color(std::min(base.r + 22, 255), std::min(base.g + 22, 255), std::min(base.b + 22, 255)) : base);
    }

    void draw(sf::RenderWindow& window) const
    {
        window.draw(shape);
        window.draw(*text);
    }

private:
    std::optional<sf::Text> text;
    sf::RectangleShape shape;
    sf::Color base = AccentDark;
};

class CardEditorApp
{
public:
    CardEditorApp()
        : window(sf::VideoMode({static_cast<unsigned int>(WindowWidth), static_cast<unsigned int>(WindowHeight)}), "Bayou Card Editor")
    {
        window.setFramerateLimit(60);
    }

    int run()
    {
        if (!font.openFromFile("assets/Roboto.ttf"))
        {
            return 1;
        }

        buildControls();
        loadCards();

        while (window.isOpen())
        {
            processEvents();
            update();
            render();
        }

        return 0;
    }

private:
    sf::RenderWindow window;
    sf::Font font;
    std::vector<card_data::Card> cards;
    std::optional<std::size_t> selectedCard;
    card_data::CardType selectedType = card_data::CardType::Unit;
    std::string status = "Ready";
    sf::Color statusColor = Muted;
    std::vector<TextField*> focusOrder;
    std::size_t focusIndex = 0;
    sf::Texture previewTexture;
    bool hasPreviewImage = false;

    TextField titleField;
    TextField imageField;
    TextField keywordsField;
    TextField intPairsField;
    TextField stringPairsField;
    TextField listsField;
    Button newButton;
    Button refreshButton;
    Button saveButton;

    void buildControls()
    {
        newButton = Button(font, "New", {34.0f, 688.0f}, {92.0f, 42.0f}, sf::Color(45, 70, 83));
        refreshButton = Button(font, "Refresh", {138.0f, 688.0f}, {116.0f, 42.0f}, sf::Color(45, 70, 83));
        saveButton = Button(font, "Save Card", {830.0f, 688.0f}, {166.0f, 44.0f}, AccentDark);

        titleField = TextField(font, "Title", {332.0f, 116.0f}, {470.0f, 46.0f});
        imageField = TextField(font, "Image Path", {332.0f, 198.0f}, {470.0f, 46.0f});
        keywordsField = TextField(font, "Keywords (comma separated)", {332.0f, 280.0f}, {470.0f, 46.0f});
        intPairsField = TextField(font, "Integer Fields (cost=2; power=3)", {332.0f, 392.0f}, {470.0f, 46.0f});
        stringPairsField = TextField(font, "String Fields (faction=bayou; rules=text)", {332.0f, 474.0f}, {470.0f, 46.0f});
        listsField = TextField(font, "String Lists (tags=a,b; slots=hand,board)", {332.0f, 556.0f}, {470.0f, 46.0f});
        focusOrder = {&titleField, &imageField, &keywordsField, &intPairsField, &stringPairsField, &listsField};
        focusOrder.front()->setActive(true);
    }

    void loadCards()
    {
        const CardListResult result = fetchCards();
        if (!result.success)
        {
            cards.clear();
            selectedCard.reset();
            setStatus(result.message, Warn);
            return;
        }

        cards = result.cards;
        if (!cards.empty())
        {
            selectCard(0);
        }
        else
        {
            createNewCard();
        }
        setStatus(fmt::format("Loaded {} card{}", cards.size(), cards.size() == 1 ? "" : "s"), Muted);
    }

    void createNewCard()
    {
        selectedCard.reset();
        selectedType = card_data::CardType::Unit;
        titleField.setValue("");
        imageField.setValue("");
        keywordsField.setValue("");
        intPairsField.setValue("");
        stringPairsField.setValue("");
        listsField.setValue("");
        hasPreviewImage = false;
        setStatus("Draft card", Muted);
    }

    void selectCard(std::size_t index)
    {
        if (index >= cards.size())
        {
            return;
        }

        selectedCard = index;
        const card_data::Card& card = cards[index];
        selectedType = card.type;
        titleField.setValue(card.title);
        imageField.setValue(card.imagePath);
        keywordsField.setValue(joinStrings(card.keywords, ", "));
        intPairsField.setValue(integerPairsToText(card.integerValues));
        stringPairsField.setValue(stringPairsToText(card.stringValues));
        listsField.setValue(stringListsToText(card.stringLists));
        loadPreviewImage();
    }

    card_data::Card cardFromForm() const
    {
        card_data::Card card;
        card.title = trim(titleField.getValue());
        card.type = selectedType;
        card.imagePath = trim(imageField.getValue());
        card.keywords = splitCommaSeparated(keywordsField.getValue());
        card.integerValues = parseIntegerPairs(intPairsField.getValue());
        card.stringValues = parseStringPairs(stringPairsField.getValue());
        card.stringLists = parseStringLists(listsField.getValue());
        return card;
    }

    void saveCurrentCard()
    {
        card_data::Card card = cardFromForm();
        if (card.title.empty())
        {
            setStatus("Title is required before saving", Warn);
            return;
        }

        const CommandResult result = saveCard(card);
        if (!result.success)
        {
            setStatus(result.message, Warn);
            return;
        }

        setStatus("Saved card", Accent);
        loadPreviewImage();
        const CardListResult listResult = fetchCards();
        if (listResult.success)
        {
            cards = listResult.cards;
            const auto found = std::find_if(cards.begin(), cards.end(), [&](const card_data::Card& item) {
                return item.title == card.title;
            });
            if (found != cards.end())
            {
                selectedCard = static_cast<std::size_t>(found - cards.begin());
            }
        }
    }

    void loadPreviewImage()
    {
        const std::string path = trim(imageField.getValue());
        hasPreviewImage = !path.empty() && previewTexture.loadFromFile(path);
    }

    void processEvents()
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (const auto* textEvent = event->getIf<sf::Event::TextEntered>())
            {
                for (TextField* field : focusOrder)
                {
                    field->handleText(*textEvent);
                }
                if (&imageField == focusOrder[focusIndex])
                {
                    loadPreviewImage();
                }
            }

            if (const auto* keyEvent = event->getIf<sf::Event::KeyPressed>())
            {
                if (keyEvent->code == sf::Keyboard::Key::Tab)
                {
                    moveFocus(keyEvent->shift ? -1 : 1);
                }
                else if (keyEvent->code == sf::Keyboard::Key::Enter)
                {
                    saveCurrentCard();
                }
            }

            if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
            {
                const sf::Vector2f mouse = window.mapPixelToCoords(mousePressed->position);
                if (mousePressed->button == sf::Mouse::Button::Left)
                {
                    handleClick(mouse);
                }
            }
        }
    }

    void moveFocus(int delta)
    {
        focusOrder[focusIndex]->setActive(false);
        focusIndex = static_cast<std::size_t>((static_cast<int>(focusIndex) + delta + static_cast<int>(focusOrder.size())) % static_cast<int>(focusOrder.size()));
        focusOrder[focusIndex]->setActive(true);
    }

    void handleClick(sf::Vector2f mouse)
    {
        if (newButton.contains(mouse))
        {
            createNewCard();
            return;
        }

        if (refreshButton.contains(mouse))
        {
            loadCards();
            return;
        }

        if (saveButton.contains(mouse))
        {
            saveCurrentCard();
            return;
        }

        const std::optional<std::size_t> listIndex = cardIndexAt(mouse);
        if (listIndex)
        {
            selectCard(*listIndex);
            return;
        }

        const std::optional<card_data::CardType> type = typeAt(mouse);
        if (type)
        {
            selectedType = *type;
            return;
        }

        for (std::size_t i = 0; i < focusOrder.size(); ++i)
        {
            const bool active = focusOrder[i]->contains(mouse);
            focusOrder[i]->setActive(active);
            if (active)
            {
                focusIndex = i;
            }
        }
    }

    std::optional<std::size_t> cardIndexAt(sf::Vector2f mouse) const
    {
        const float startY = 116.0f;
        const float rowHeight = 58.0f;
        if (mouse.x < 28.0f || mouse.x > 286.0f || mouse.y < startY)
        {
            return std::nullopt;
        }

        const std::size_t index = static_cast<std::size_t>((mouse.y - startY) / rowHeight);
        if (index < cards.size() && index < 9)
        {
            return index;
        }
        return std::nullopt;
    }

    std::optional<card_data::CardType> typeAt(sf::Vector2f mouse) const
    {
        const std::vector<card_data::CardType> types = {
            card_data::CardType::Unit,
            card_data::CardType::Spell,
            card_data::CardType::Artifact,
            card_data::CardType::Reaction,
        };

        for (std::size_t i = 0; i < types.size(); ++i)
        {
            sf::FloatRect bounds({332.0f + static_cast<float>(i) * 116.0f, 324.0f}, {104.0f, 40.0f});
            if (bounds.contains(mouse))
            {
                return types[i];
            }
        }
        return std::nullopt;
    }

    void update()
    {
        const sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        newButton.update(mouse);
        refreshButton.update(mouse);
        saveButton.update(mouse);
    }

    void render()
    {
        window.clear(sf::Color(18, 22, 30));
        drawHeader();
        drawListPanel();
        drawEditorPanel();
        drawPreviewPanel();
        window.display();
    }

    void drawHeader()
    {
        sf::RectangleShape bar({WindowWidth, 80.0f});
        bar.setFillColor(sf::Color(24, 29, 38));
        window.draw(bar);

        drawText(window, font, "Bayou Card Editor", 30, {30.0f, 22.0f}, Ink);
        drawText(window, font, "Card server localhost:55004", 15, {1036.0f, 31.0f}, Muted);
    }

    void drawListPanel()
    {
        drawRoundedPanel(window, {24.0f, 100.0f}, {266.0f, 640.0f}, Panel);
        drawText(window, font, "Library", 22, {42.0f, 124.0f}, Ink);
        drawText(window, font, fmt::format("{} cards", cards.size()), 14, {220.0f, 131.0f}, Muted);

        for (std::size_t i = 0; i < cards.size() && i < 9; ++i)
        {
            const float y = 166.0f + static_cast<float>(i) * 58.0f;
            sf::RectangleShape row({230.0f, 48.0f});
            row.setPosition({42.0f, y});
            row.setFillColor(selectedCard && *selectedCard == i ? sf::Color(49, 68, 78) : PanelAlt);
            row.setOutlineThickness(1.0f);
            row.setOutlineColor(selectedCard && *selectedCard == i ? Accent : sf::Color(48, 56, 70));
            window.draw(row);

            drawText(window, font, cards[i].title, 17, {54.0f, y + 8.0f}, Ink);
            drawText(window, font, card_data::toString(cards[i].type), 13, {54.0f, y + 29.0f}, Muted);
        }

        newButton.draw(window);
        refreshButton.draw(window);
    }

    void drawEditorPanel()
    {
        drawRoundedPanel(window, {310.0f, 100.0f}, {520.0f, 640.0f}, Panel);
        drawText(window, font, "Edit Card", 22, {332.0f, 124.0f}, Ink);
        drawText(window, font, "Enter creates or updates the current title.", 14, {578.0f, 131.0f}, Muted);

        titleField.draw(window);
        imageField.draw(window);
        keywordsField.draw(window);
        drawTypePicker();
        intPairsField.draw(window);
        stringPairsField.draw(window);
        listsField.draw(window);
        saveButton.draw(window);
        drawText(window, font, status, 16, {332.0f, 696.0f}, statusColor);
    }

    void drawTypePicker()
    {
        drawText(window, font, "Type", 15, {332.0f, 306.0f}, Muted);
        const std::vector<card_data::CardType> types = {
            card_data::CardType::Unit,
            card_data::CardType::Spell,
            card_data::CardType::Artifact,
            card_data::CardType::Reaction,
        };

        for (std::size_t i = 0; i < types.size(); ++i)
        {
            const sf::Vector2f position{332.0f + static_cast<float>(i) * 116.0f, 324.0f};
            sf::RectangleShape pill({104.0f, 40.0f});
            pill.setPosition(position);
            pill.setFillColor(types[i] == selectedType ? AccentDark : Field);
            pill.setOutlineThickness(1.0f);
            pill.setOutlineColor(types[i] == selectedType ? Accent : Line);
            window.draw(pill);

            sf::Text label(font, card_data::toString(types[i]), 16);
            label.setFillColor(Ink);
            centerText(label, {position.x + 52.0f, position.y + 20.0f});
            window.draw(label);
        }
    }

    void drawPreviewPanel()
    {
        drawRoundedPanel(window, {850.0f, 100.0f}, {406.0f, 640.0f}, Panel);
        drawText(window, font, "Preview", 22, {876.0f, 124.0f}, Ink);

        drawRoundedPanel(window, {938.0f, 160.0f}, {230.0f, 322.0f}, sf::Color(46, 52, 64), AccentDark);
        if (hasPreviewImage)
        {
            sf::Sprite sprite(previewTexture);
            const sf::Vector2u imageSize = previewTexture.getSize();
            const float scale = std::min(210.0f / static_cast<float>(imageSize.x), 250.0f / static_cast<float>(imageSize.y));
            sprite.setScale({scale, scale});
            sprite.setPosition({948.0f, 176.0f});
            window.draw(sprite);
        }
        else
        {
            sf::RectangleShape imageSlot({190.0f, 188.0f});
            imageSlot.setPosition({958.0f, 184.0f});
            imageSlot.setFillColor(sf::Color(28, 34, 44));
            imageSlot.setOutlineThickness(1.0f);
            imageSlot.setOutlineColor(Line);
            window.draw(imageSlot);
            drawText(window, font, "No Image", 20, {1016.0f, 264.0f}, Muted);
        }

        const card_data::Card card = cardFromForm();
        sf::Text title(font, card.title.empty() ? "Untitled Card" : card.title, 22);
        title.setFillColor(Ink);
        centerText(title, {1053.0f, 414.0f});
        window.draw(title);

        sf::Text type(font, card_data::toString(card.type), 16);
        type.setFillColor(Accent);
        centerText(type, {1053.0f, 445.0f});
        window.draw(type);

        float y = 512.0f;
        drawText(window, font, "Keywords", 15, {876.0f, y}, Muted);
        drawText(window, font, joinStrings(card.keywords, ", "), 16, {876.0f, y + 22.0f}, Ink);
        y += 72.0f;
        drawText(window, font, "Integer Fields", 15, {876.0f, y}, Muted);
        drawText(window, font, integerPairsToText(card.integerValues), 16, {876.0f, y + 22.0f}, Ink);
        y += 72.0f;
        drawText(window, font, "String Fields", 15, {876.0f, y}, Muted);
        drawText(window, font, stringPairsToText(card.stringValues), 16, {876.0f, y + 22.0f}, Ink);
    }

    void setStatus(const std::string& message, sf::Color color)
    {
        status = message;
        statusColor = color;
    }
};
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--list")
    {
        return handleListCommand();
    }

    if (argc == 3 && std::string(argv[1]) == "--create-sample")
    {
        return handleSaveCommand(makeSampleCard(argv[2], 1));
    }

    if (argc == 3 && std::string(argv[1]) == "--edit-sample")
    {
        return handleSaveCommand(makeSampleCard(argv[2], 2));
    }

    CardEditorApp app;
    return app.run();
}
