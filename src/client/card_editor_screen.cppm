module;

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <fmt/core.h>

#include "../shared/card_data.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

export module card_editor_screen;

import inputbox;
import network;

namespace
{
constexpr float EditorWidth = 1280.0f;
constexpr float EditorHeight = 760.0f;
constexpr float ListPanelX = 24.0f;
constexpr float ListPanelY = 100.0f;
constexpr float ListPanelWidth = 276.0f;
constexpr float PanelHeight = 640.0f;
constexpr float EditorPanelX = 318.0f;
constexpr float EditorPanelY = 100.0f;
constexpr float EditorPanelWidth = 520.0f;
constexpr float PreviewPanelX = 856.0f;
constexpr float PreviewPanelY = 100.0f;
constexpr float PreviewPanelWidth = 400.0f;
constexpr float ListRowStartY = 176.0f;
constexpr float ListRowHeight = 56.0f;
constexpr std::size_t VisibleCardRows = 8;

const sf::Color Ink(236, 239, 244);
const sf::Color Muted(152, 164, 181);
const sf::Color Panel(31, 36, 46);
const sf::Color PanelAlt(39, 46, 59);
const sf::Color Accent(89, 183, 169);
const sf::Color AccentDark(31, 106, 104);
const sf::Color Warn(221, 112, 92);
const sf::Color Line(75, 85, 102);

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

std::string lowerKey(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string assetRelativeImagePath(const std::string& value)
{
    const std::string trimmed = trim(value);
    if (trimmed.empty())
    {
        return "";
    }

    std::filesystem::path path(trimmed);
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
    {
        return path.generic_string();
    }

    std::filesystem::path normalizedPath;
    bool checkedFirstComponent = false;
    for (const std::filesystem::path& component : path)
    {
        if (!checkedFirstComponent)
        {
            checkedFirstComponent = true;
            if (lowerKey(component.string()) == "assets")
            {
                continue;
            }
        }
        normalizedPath /= component;
    }

    return normalizedPath.lexically_normal().generic_string();
}

bool escapesAssetsRoot(const std::filesystem::path& path)
{
    for (const std::filesystem::path& component : path)
    {
        if (component.string() == "..")
        {
            return true;
        }
    }
    return false;
}

std::filesystem::path normalizedAbsolutePath(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::path normalized = std::filesystem::weakly_canonical(path, error);
    if (!error)
    {
        return normalized.lexically_normal();
    }

    error.clear();
    normalized = std::filesystem::absolute(path, error);
    if (!error)
    {
        return normalized.lexically_normal();
    }
    return path.lexically_normal();
}

bool isInsideAssetsRoot(const std::filesystem::path& path)
{
    const std::filesystem::path assetsRoot = normalizedAbsolutePath("assets");
    const std::filesystem::path normalizedPath = normalizedAbsolutePath(path);
    const std::filesystem::path relativePath = normalizedPath.lexically_relative(assetsRoot);
    if (relativePath.empty())
    {
        return normalizedPath == assetsRoot;
    }
    if (relativePath.has_root_path())
    {
        return false;
    }
    const auto firstComponent = relativePath.begin();
    return firstComponent == relativePath.end() || firstComponent->string() != "..";
}

std::optional<std::filesystem::path> resolveAssetImagePath(const std::string& value)
{
    const std::string relativeValue = assetRelativeImagePath(value);
    if (relativeValue.empty())
    {
        return std::nullopt;
    }

    const std::filesystem::path relativePath(relativeValue);
    if (relativePath.is_absolute() || relativePath.has_root_name() || relativePath.has_root_directory() ||
        escapesAssetsRoot(relativePath))
    {
        return std::nullopt;
    }

    const std::filesystem::path resolvedPath = (std::filesystem::path("assets") / relativePath).lexically_normal();
    if (!isInsideAssetsRoot(resolvedPath))
    {
        return std::nullopt;
    }
    return resolvedPath;
}

void normalizeCardImagePath(card_data::Card& card)
{
    card.imagePath = assetRelativeImagePath(card.imagePath);
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

std::string elideToWidth(sf::Font& font, const std::string& value, unsigned int size, float maxWidth)
{
    sf::Text text(font, value, size);
    if (text.getLocalBounds().size.x <= maxWidth)
    {
        return value;
    }

    std::string display = value;
    while (!display.empty())
    {
        display.pop_back();
        text.setString(display + "...");
        if (text.getLocalBounds().size.x <= maxWidth)
        {
            return display + "...";
        }
    }
    return "...";
}

void drawText(
    sf::RenderWindow& window,
    sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    float maxWidth)
{
    drawText(window, font, elideToWidth(font, value, size, maxWidth), size, position, color);
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

class EditorButton
{
public:
    EditorButton() = default;

    EditorButton(sf::Font& font, std::string label, sf::Vector2f position, sf::Vector2f size, sf::Color color)
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

    sf::FloatRect bounds() const
    {
        return shape.getGlobalBounds();
    }

    void setPosition(sf::Vector2f position)
    {
        shape.setPosition(position);
        if (text)
        {
            centerText(*text, {position.x + shape.getSize().x / 2.0f, position.y + shape.getSize().y / 2.0f - 1.0f});
        }
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
}

export struct CardEditorEndpoint
{
    std::string host = "127.0.0.1";
    unsigned short port = 55004;
};

export class CardEditorScreen
{
public:
    explicit CardEditorScreen(sf::Font& screenFont, CardEditorEndpoint screenEndpoint = {})
        : font(screenFont), endpoint(std::move(screenEndpoint))
    {
        buildControls();
    }

    void setEndpoint(CardEditorEndpoint screenEndpoint)
    {
        endpoint = std::move(screenEndpoint);
    }

    void open()
    {
        loadCards();
    }

    bool handleEvent(const sf::Event& event, sf::RenderWindow& window)
    {
        const sf::View previousView = window.getView();
        window.setView(makeEditorView(window));

        bool shouldClose = false;
        if (!focusOrder.empty())
        {
            const std::string previousImagePath = imageField.getValue();
            focusOrder[focusIndex]->handleEvent(event, window);
            if (imageField.isActive() && imageField.getValue() != previousImagePath)
            {
                loadPreviewImage();
            }
        }

        if (const auto* keyEvent = event.getIf<sf::Event::KeyPressed>())
        {
            if (keyEvent->code == sf::Keyboard::Key::Escape)
            {
                shouldClose = true;
            }
            else if (keyEvent->code == sf::Keyboard::Key::Tab)
            {
                moveFocus(keyEvent->shift ? -1 : 1);
                ensureActiveFieldVisible();
            }
            else if (keyEvent->code == sf::Keyboard::Key::Enter)
            {
                saveCurrentCard();
            }
        }

        if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>())
        {
            const sf::Vector2f mouse = window.mapPixelToCoords(mousePressed->position);
            if (mousePressed->button == sf::Mouse::Button::Left)
            {
                shouldClose = handleClick(mouse) || shouldClose;
            }
        }

        if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>())
        {
            const sf::Vector2f mouse = window.mapPixelToCoords(wheel->position);
            if (isInListPanel(mouse))
            {
                scrollCardList(wheel->delta < 0.0f ? 1 : -1);
            }
            else if (isInArrayViewport(mouse))
            {
                scrollEditorForm(wheel->delta < 0.0f ? 1 : -1);
            }
        }

        window.setView(previousView);
        return shouldClose;
    }

    void update(sf::RenderWindow& window, float deltaTime)
    {
        layoutArrayControls();
        const sf::View previousView = window.getView();
        window.setView(makeEditorView(window));
        const sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        window.setView(previousView);

        backButton.update(mouse);
        newButton.update(mouse);
        refreshButton.update(mouse);
        saveButton.update(mouse);
        deleteButton.update(mouse);
        addKeywordButton.update(mouse);
        addIntegerButton.update(mouse);
        addStringButton.update(mouse);
        addListButton.update(mouse);
        for (EditorButton& button : removeKeywordButtons)
        {
            button.update(mouse);
        }
        for (EditorButton& button : removeIntegerButtons)
        {
            button.update(mouse);
        }
        for (EditorButton& button : removeStringButtons)
        {
            button.update(mouse);
        }
        for (EditorButton& button : removeListButtons)
        {
            button.update(mouse);
        }
        for (EditorButton& button : addListValueButtons)
        {
            button.update(mouse);
        }
        for (std::vector<EditorButton>& buttons : removeListValueButtons)
        {
            for (EditorButton& button : buttons)
            {
                button.update(mouse);
            }
        }
        if (!focusOrder.empty() && focusIndex < focusOrder.size())
        {
            focusOrder[focusIndex]->updateCursor(deltaTime);
        }
    }

    void render(sf::RenderWindow& window)
    {
        const sf::View previousView = window.getView();
        window.setView(makeEditorView(window));
        sf::RectangleShape background({EditorWidth, EditorHeight});
        background.setFillColor(sf::Color(18, 22, 30));
        window.draw(background);
        drawHeader(window);
        drawListPanel(window);
        drawEditorPanel(window);
        drawPreviewPanel(window);
        window.setView(previousView);
    }

private:
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

    struct StringListEditor
    {
        InputBox keyField;
        std::vector<InputBox> valueFields;
    };

    static constexpr float ArrayViewportTop = 372.0f;
    static constexpr float ArrayViewportBottom = 676.0f;
    static constexpr float ArrayViewportHeight = ArrayViewportBottom - ArrayViewportTop;

    sf::Font& font;
    CardEditorEndpoint endpoint;
    std::vector<card_data::Card> cards;
    std::optional<std::size_t> selectedCard;
    std::size_t listOffset = 0;
    std::string status = "Ready";
    sf::Color statusColor = Muted;
    std::vector<InputBox*> focusOrder;
    std::size_t focusIndex = 0;
    float editorScroll = 0.0f;
    float editorContentHeight = 0.0f;
    std::vector<std::pair<std::string, sf::Vector2f>> arraySectionLabels;
    sf::Texture previewTexture;
    bool hasPreviewImage = false;

    InputBox titleField;
    InputBox imageField;
    InputBox typeField;
    std::vector<InputBox> keywordFields;
    std::vector<InputBox> intKeyFields;
    std::vector<InputBox> intValueFields;
    std::vector<InputBox> stringKeyFields;
    std::vector<InputBox> stringValueFields;
    std::vector<StringListEditor> listEditors;
    EditorButton backButton;
    EditorButton newButton;
    EditorButton refreshButton;
    EditorButton saveButton;
    EditorButton deleteButton;
    EditorButton addKeywordButton;
    EditorButton addIntegerButton;
    EditorButton addStringButton;
    EditorButton addListButton;
    std::vector<EditorButton> removeKeywordButtons;
    std::vector<EditorButton> removeIntegerButtons;
    std::vector<EditorButton> removeStringButtons;
    std::vector<EditorButton> removeListButtons;
    std::vector<EditorButton> addListValueButtons;
    std::vector<std::vector<EditorButton>> removeListValueButtons;

    sf::View makeEditorView(const sf::RenderWindow& window) const
    {
        const sf::Vector2u windowSize = window.getSize();
        sf::FloatRect viewport({0.0f, 0.0f}, {1.0f, 1.0f});
        if (windowSize.x > 0 && windowSize.y > 0)
        {
            const float windowAspect = static_cast<float>(windowSize.x) / static_cast<float>(windowSize.y);
            const float editorAspect = EditorWidth / EditorHeight;
            if (windowAspect > editorAspect)
            {
                viewport.size.x = editorAspect / windowAspect;
                viewport.position.x = (1.0f - viewport.size.x) * 0.5f;
            }
            else if (windowAspect < editorAspect)
            {
                viewport.size.y = windowAspect / editorAspect;
                viewport.position.y = (1.0f - viewport.size.y) * 0.5f;
            }
        }

        sf::View view(sf::FloatRect({0.0f, 0.0f}, {EditorWidth, EditorHeight}));
        view.setViewport(viewport);
        return view;
    }

    std::string endpointText() const
    {
        return fmt::format("{}:{}", endpoint.host, endpoint.port);
    }

    bool connectToServer(sf::TcpSocket& socket) const
    {
        const std::optional<sf::IpAddress> address = sf::IpAddress::resolve(endpoint.host);
        if (!address)
        {
            return false;
        }
        return socket.connect(*address, endpoint.port) == sf::Socket::Status::Done;
    }

    CardListResult fetchCardsFromServer() const
    {
        sf::TcpSocket socket;
        if (!connectToServer(socket))
        {
            return {false, "Failed to connect to card server " + endpointText()};
        }

        sf::Packet request;
        request << static_cast<std::uint8_t>(network::MessageType::CardListRequest);
        if (socket.send(request) != sf::Socket::Status::Done)
        {
            socket.disconnect();
            return {false, "Failed to send card list request"};
        }

        sf::Packet response;
        if (socket.receive(response) != sf::Socket::Status::Done)
        {
            socket.disconnect();
            return {false, "No response from card server"};
        }

        std::uint8_t responseType = 0;
        bool success = false;
        std::string message;
        std::uint32_t count = 0;
        response >> responseType >> success >> message >> count;
        if (!response || static_cast<network::MessageType>(responseType) != network::MessageType::CardListResponse)
        {
            socket.disconnect();
            return {false, "Unexpected card list response"};
        }

        std::vector<card_data::Card> loadedCards;
        loadedCards.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            card_data::Card card;
            if (!card_data::readCard(response, card))
            {
                socket.disconnect();
                return {false, "Invalid card list payload"};
            }
            normalizeCardImagePath(card);
            loadedCards.push_back(card);
        }

        socket.disconnect();
        return {success, message, std::move(loadedCards)};
    }

    CommandResult readCommandResponse(sf::TcpSocket& socket, network::MessageType expectedResponseType, const std::string& action) const
    {
        sf::Packet response;
        if (socket.receive(response) != sf::Socket::Status::Done)
        {
            return {false, fmt::format("No response from card server while {}", action)};
        }

        std::uint8_t responseType = 0;
        bool success = false;
        std::string message;
        response >> responseType >> success >> message;
        if (!response || static_cast<network::MessageType>(responseType) != expectedResponseType)
        {
            return {false, fmt::format("Unexpected card server response while {}", action)};
        }
        return {success, message};
    }

    CommandResult saveCardToServer(const card_data::Card& inputCard) const
    {
        card_data::Card card = inputCard;
        normalizeCardImagePath(card);

        sf::TcpSocket socket;
        if (!connectToServer(socket))
        {
            return {false, "Failed to connect to card server " + endpointText()};
        }

        sf::Packet request;
        request << static_cast<std::uint8_t>(network::MessageType::CardUpsertRequest);
        card_data::writeCard(request, card);
        if (socket.send(request) != sf::Socket::Status::Done)
        {
            socket.disconnect();
            return {false, "Failed to send card save request"};
        }

        CommandResult result = readCommandResponse(socket, network::MessageType::CardUpsertResponse, "saving card");
        socket.disconnect();
        return result;
    }

    CommandResult updateCardOnServer(const std::string& originalTitle, const card_data::Card& inputCard) const
    {
        card_data::Card card = inputCard;
        normalizeCardImagePath(card);

        sf::TcpSocket socket;
        if (!connectToServer(socket))
        {
            return {false, "Failed to connect to card server " + endpointText()};
        }

        sf::Packet request;
        request << static_cast<std::uint8_t>(network::MessageType::CardUpdateRequest);
        request << originalTitle;
        card_data::writeCard(request, card);
        if (socket.send(request) != sf::Socket::Status::Done)
        {
            socket.disconnect();
            return {false, "Failed to send card update request"};
        }

        CommandResult result = readCommandResponse(socket, network::MessageType::CardUpdateResponse, "updating card");
        socket.disconnect();
        return result;
    }

    CommandResult deleteCardFromServer(const std::string& title) const
    {
        sf::TcpSocket socket;
        if (!connectToServer(socket))
        {
            return {false, "Failed to connect to card server " + endpointText()};
        }

        sf::Packet request;
        request << static_cast<std::uint8_t>(network::MessageType::CardDeleteRequest);
        request << title;
        if (socket.send(request) != sf::Socket::Status::Done)
        {
            socket.disconnect();
            return {false, "Failed to send card delete request"};
        }

        CommandResult result = readCommandResponse(socket, network::MessageType::CardDeleteResponse, "deleting card");
        socket.disconnect();
        return result;
    }

    void buildControls()
    {
        backButton = EditorButton(font, "Back", {1124.0f, 22.0f}, {112.0f, 38.0f}, sf::Color(45, 70, 83));
        newButton = EditorButton(font, "New", {42.0f, 690.0f}, {96.0f, 42.0f}, sf::Color(45, 70, 83));
        refreshButton = EditorButton(font, "Refresh", {150.0f, 690.0f}, {120.0f, 42.0f}, sf::Color(45, 70, 83));
        saveButton = EditorButton(font, "Save Card", {660.0f, 690.0f}, {156.0f, 42.0f}, AccentDark);
        deleteButton = EditorButton(font, "Delete", {528.0f, 690.0f}, {120.0f, 42.0f}, Warn);
        addKeywordButton = EditorButton(font, "+", {778.0f, 372.0f}, {32.0f, 28.0f}, AccentDark);
        addIntegerButton = EditorButton(font, "+", {778.0f, 372.0f}, {32.0f, 28.0f}, AccentDark);
        addStringButton = EditorButton(font, "+", {778.0f, 372.0f}, {32.0f, 28.0f}, AccentDark);
        addListButton = EditorButton(font, "+", {778.0f, 372.0f}, {32.0f, 28.0f}, AccentDark);
        titleField = InputBox(font, "Title", {340.0f, 168.0f}, {470.0f, 42.0f});
        imageField = InputBox(font, "Image Path", {340.0f, 238.0f}, {470.0f, 42.0f});
        typeField = InputBox(font, "Type", {340.0f, 308.0f}, {470.0f, 42.0f});
        rebuildFocusOrder();
        activateField(&titleField);
    }

    InputBox makeCompactField(const std::string& value, sf::Vector2f size)
    {
        InputBox field(font, "", {0.0f, 0.0f}, size);
        field.setValue(value);
        return field;
    }

    EditorButton makeMiniButton(const std::string& label, sf::Color color)
    {
        return EditorButton(font, label, {0.0f, 0.0f}, {32.0f, 28.0f}, color);
    }

    void loadArrayFields(const card_data::Card& card)
    {
        keywordFields.clear();
        intKeyFields.clear();
        intValueFields.clear();
        stringKeyFields.clear();
        stringValueFields.clear();
        listEditors.clear();

        for (const std::string& keyword : card.keywords)
        {
            keywordFields.push_back(makeCompactField(keyword, {392.0f, 32.0f}));
        }
        for (const card_data::KeyIntPair& item : card.integerValues)
        {
            intKeyFields.push_back(makeCompactField(item.key, {182.0f, 32.0f}));
            intValueFields.push_back(makeCompactField(std::to_string(item.value), {210.0f, 32.0f}));
        }
        for (const card_data::KeyStringPair& item : card.stringValues)
        {
            stringKeyFields.push_back(makeCompactField(item.key, {182.0f, 32.0f}));
            stringValueFields.push_back(makeCompactField(item.value, {210.0f, 32.0f}));
        }
        for (const card_data::KeyStringList& item : card.stringLists)
        {
            StringListEditor editor;
            editor.keyField = makeCompactField(item.key, {210.0f, 32.0f});
            for (const std::string& value : item.values)
            {
                editor.valueFields.push_back(makeCompactField(value, {366.0f, 32.0f}));
            }
            listEditors.push_back(std::move(editor));
        }
    }

    void rebuildFocusOrder()
    {
        focusOrder.clear();
        focusOrder.push_back(&titleField);
        focusOrder.push_back(&imageField);
        focusOrder.push_back(&typeField);
        for (InputBox& field : keywordFields)
        {
            focusOrder.push_back(&field);
        }
        for (std::size_t i = 0; i < intKeyFields.size() && i < intValueFields.size(); ++i)
        {
            focusOrder.push_back(&intKeyFields[i]);
            focusOrder.push_back(&intValueFields[i]);
        }
        for (std::size_t i = 0; i < stringKeyFields.size() && i < stringValueFields.size(); ++i)
        {
            focusOrder.push_back(&stringKeyFields[i]);
            focusOrder.push_back(&stringValueFields[i]);
        }
        for (StringListEditor& editor : listEditors)
        {
            focusOrder.push_back(&editor.keyField);
            for (InputBox& field : editor.valueFields)
            {
                focusOrder.push_back(&field);
            }
        }
        focusIndex = std::min(focusIndex, focusOrder.empty() ? 0 : focusOrder.size() - 1);
    }

    void activateField(InputBox* target)
    {
        if (focusOrder.empty())
        {
            return;
        }

        for (std::size_t i = 0; i < focusOrder.size(); ++i)
        {
            const bool active = focusOrder[i] == target;
            focusOrder[i]->setActive(active);
            if (active)
            {
                focusIndex = i;
            }
        }
    }

    void loadCards()
    {
        const CardListResult result = fetchCardsFromServer();
        if (!result.success)
        {
            cards.clear();
            selectedCard.reset();
            listOffset = 0;
            setStatus(result.message, Warn);
            return;
        }

        cards = result.cards;
        listOffset = 0;
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
        titleField.setValue("");
        imageField.setValue("");
        typeField.setValue("Unit");
        keywordFields.clear();
        intKeyFields.clear();
        intValueFields.clear();
        stringKeyFields.clear();
        stringValueFields.clear();
        listEditors.clear();
        editorScroll = 0.0f;
        rebuildFocusOrder();
        activateField(&titleField);
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
        ensureCardVisible(index);
        const card_data::Card& card = cards[index];
        titleField.setValue(card.title);
        imageField.setValue(assetRelativeImagePath(card.imagePath));
        typeField.setValue(card.type);
        loadArrayFields(card);
        editorScroll = 0.0f;
        rebuildFocusOrder();
        loadPreviewImage();
    }

    card_data::Card cardFromForm() const
    {
        card_data::Card card;
        card.title = trim(titleField.getValue());
        card.type = trim(typeField.getValue());
        card.imagePath = assetRelativeImagePath(imageField.getValue());
        for (const InputBox& field : keywordFields)
        {
            const std::string value = trim(field.getValue());
            if (!value.empty())
            {
                card.keywords.push_back(value);
            }
        }
        for (std::size_t i = 0; i < intKeyFields.size() && i < intValueFields.size(); ++i)
        {
            const std::string key = trim(intKeyFields[i].getValue());
            if (key.empty())
            {
                continue;
            }
            try
            {
                card.integerValues.push_back({key, std::stoi(trim(intValueFields[i].getValue()))});
            }
            catch (const std::exception&)
            {
            }
        }
        for (std::size_t i = 0; i < stringKeyFields.size() && i < stringValueFields.size(); ++i)
        {
            const std::string key = trim(stringKeyFields[i].getValue());
            if (!key.empty())
            {
                card.stringValues.push_back({key, trim(stringValueFields[i].getValue())});
            }
        }
        for (const StringListEditor& editor : listEditors)
        {
            card_data::KeyStringList item;
            item.key = trim(editor.keyField.getValue());
            if (item.key.empty())
            {
                continue;
            }
            for (const InputBox& field : editor.valueFields)
            {
                const std::string value = trim(field.getValue());
                if (!value.empty())
                {
                    item.values.push_back(value);
                }
            }
            card.stringLists.push_back(item);
        }
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
        if (!card.imagePath.empty() && !resolveAssetImagePath(card.imagePath))
        {
            setStatus("Image path must stay inside assets", Warn);
            return;
        }
        imageField.setValue(card.imagePath);

        const std::optional<std::string> originalTitle = selectedCardTitle();
        const CommandResult result = originalTitle ? updateCardOnServer(*originalTitle, card) : saveCardToServer(card);
        if (!result.success)
        {
            setStatus(result.message, Warn);
            return;
        }

        setStatus("Saved card", Accent);
        loadPreviewImage();
        const CardListResult listResult = fetchCardsFromServer();
        if (listResult.success)
        {
            cards = listResult.cards;
            const auto found = std::find_if(cards.begin(), cards.end(), [&](const card_data::Card& item) {
                return item.title == card.title;
            });
            if (found != cards.end())
            {
                selectedCard = static_cast<std::size_t>(found - cards.begin());
                ensureCardVisible(*selectedCard);
            }
        }
    }

    void deleteCurrentCard()
    {
        const std::optional<std::string> title = selectedCardTitle();
        if (!title)
        {
            setStatus("Select a saved card before deleting", Warn);
            return;
        }

        const CommandResult result = deleteCardFromServer(*title);
        if (!result.success)
        {
            setStatus(result.message, Warn);
            return;
        }

        const std::size_t deletedIndex = *selectedCard;
        const CardListResult listResult = fetchCardsFromServer();
        if (!listResult.success)
        {
            cards.clear();
            selectedCard.reset();
            createNewCard();
            setStatus(listResult.message, Warn);
            return;
        }

        cards = listResult.cards;
        if (cards.empty())
        {
            listOffset = 0;
            createNewCard();
        }
        else
        {
            selectCard(std::min(deletedIndex, cards.size() - 1));
        }
        setStatus("Deleted card", Accent);
    }

    std::optional<std::string> selectedCardTitle() const
    {
        if (!selectedCard || *selectedCard >= cards.size())
        {
            return std::nullopt;
        }
        return cards[*selectedCard].title;
    }

    void ensureCardVisible(std::size_t index)
    {
        if (index < listOffset)
        {
            listOffset = index;
        }
        else if (index >= listOffset + VisibleCardRows)
        {
            listOffset = index - VisibleCardRows + 1;
        }
    }

    void scrollCardList(int rows)
    {
        if (cards.size() <= VisibleCardRows)
        {
            listOffset = 0;
            return;
        }
        const int maxOffset = static_cast<int>(cards.size() - VisibleCardRows);
        const int next = std::clamp(static_cast<int>(listOffset) + rows, 0, maxOffset);
        listOffset = static_cast<std::size_t>(next);
    }

    bool isInArrayViewport(sf::Vector2f point) const
    {
        return point.x >= 330.0f && point.x <= 824.0f && point.y >= ArrayViewportTop && point.y <= ArrayViewportBottom;
    }

    bool isVisibleInArrayViewport(const sf::FloatRect& bounds) const
    {
        return bounds.position.y >= ArrayViewportTop && bounds.position.y + bounds.size.y <= ArrayViewportBottom;
    }

    void clampEditorScroll()
    {
        const float maxScroll = std::max(0.0f, editorContentHeight - ArrayViewportHeight);
        editorScroll = std::clamp(editorScroll, 0.0f, maxScroll);
    }

    void scrollEditorForm(int rows)
    {
        layoutArrayControls();
        editorScroll += static_cast<float>(rows) * 42.0f;
        clampEditorScroll();
        layoutArrayControls();
    }

    void layoutArrayControls()
    {
        arraySectionLabels.clear();
        removeKeywordButtons.clear();
        removeIntegerButtons.clear();
        removeStringButtons.clear();
        removeListButtons.clear();
        addListValueButtons.clear();
        removeListValueButtons.clear();

        float y = ArrayViewportTop - editorScroll;
        auto addSection = [&](const std::string& heading, EditorButton& addButton) {
            arraySectionLabels.push_back({heading, {340.0f, y}});
            addButton.setPosition({778.0f, y + 1.0f});
            y += 30.0f;
        };

        addSection("Keywords", addKeywordButton);
        if (keywordFields.empty())
        {
            arraySectionLabels.push_back({"No keywords", {354.0f, y + 6.0f}});
            y += 34.0f;
        }
        for (InputBox& field : keywordFields)
        {
            field.setPosition({340.0f, y});
            EditorButton button = makeMiniButton("X", Warn);
            button.setPosition({778.0f, y + 2.0f});
            removeKeywordButtons.push_back(std::move(button));
            y += 40.0f;
        }
        y += 12.0f;

        addSection("Integer Fields", addIntegerButton);
        if (intKeyFields.empty())
        {
            arraySectionLabels.push_back({"No integer fields", {354.0f, y + 6.0f}});
            y += 34.0f;
        }
        for (std::size_t i = 0; i < intKeyFields.size() && i < intValueFields.size(); ++i)
        {
            intKeyFields[i].setPosition({340.0f, y});
            intValueFields[i].setPosition({532.0f, y});
            EditorButton button = makeMiniButton("X", Warn);
            button.setPosition({778.0f, y + 2.0f});
            removeIntegerButtons.push_back(std::move(button));
            y += 40.0f;
        }
        y += 12.0f;

        addSection("String Fields", addStringButton);
        if (stringKeyFields.empty())
        {
            arraySectionLabels.push_back({"No string fields", {354.0f, y + 6.0f}});
            y += 34.0f;
        }
        for (std::size_t i = 0; i < stringKeyFields.size() && i < stringValueFields.size(); ++i)
        {
            stringKeyFields[i].setPosition({340.0f, y});
            stringValueFields[i].setPosition({532.0f, y});
            EditorButton button = makeMiniButton("X", Warn);
            button.setPosition({778.0f, y + 2.0f});
            removeStringButtons.push_back(std::move(button));
            y += 40.0f;
        }
        y += 12.0f;

        addSection("String Lists", addListButton);
        if (listEditors.empty())
        {
            arraySectionLabels.push_back({"No string lists", {354.0f, y + 6.0f}});
            y += 34.0f;
        }
        for (StringListEditor& editor : listEditors)
        {
            editor.keyField.setPosition({340.0f, y});
            EditorButton addValue = makeMiniButton("+", AccentDark);
            addValue.setPosition({560.0f, y + 2.0f});
            addListValueButtons.push_back(std::move(addValue));
            EditorButton removeList = makeMiniButton("X", Warn);
            removeList.setPosition({778.0f, y + 2.0f});
            removeListButtons.push_back(std::move(removeList));
            y += 40.0f;

            std::vector<EditorButton> valueButtons;
            if (editor.valueFields.empty())
            {
                arraySectionLabels.push_back({"No values", {368.0f, y + 6.0f}});
                y += 34.0f;
            }
            for (InputBox& field : editor.valueFields)
            {
                field.setPosition({368.0f, y});
                EditorButton removeValue = makeMiniButton("X", Warn);
                removeValue.setPosition({778.0f, y + 2.0f});
                valueButtons.push_back(std::move(removeValue));
                y += 38.0f;
            }
            removeListValueButtons.push_back(std::move(valueButtons));
            y += 8.0f;
        }

        editorContentHeight = y - (ArrayViewportTop - editorScroll);
        const float maxScroll = std::max(0.0f, editorContentHeight - ArrayViewportHeight);
        const float clampedScroll = std::clamp(editorScroll, 0.0f, maxScroll);
        if (clampedScroll != editorScroll)
        {
            editorScroll = clampedScroll;
            layoutArrayControls();
        }
    }

    void loadPreviewImage()
    {
        const std::optional<std::filesystem::path> path = resolveAssetImagePath(imageField.getValue());
        if (!path || !previewTexture.loadFromFile(*path))
        {
            previewTexture = sf::Texture();
            hasPreviewImage = false;
            return;
        }
        hasPreviewImage = true;
    }

    void addKeyword()
    {
        keywordFields.push_back(makeCompactField("", {392.0f, 32.0f}));
        rebuildFocusOrder();
        activateField(&keywordFields.back());
        ensureActiveFieldVisible();
    }

    void addIntegerField()
    {
        intKeyFields.push_back(makeCompactField("", {182.0f, 32.0f}));
        intValueFields.push_back(makeCompactField("0", {210.0f, 32.0f}));
        rebuildFocusOrder();
        activateField(&intKeyFields.back());
        ensureActiveFieldVisible();
    }

    void addStringField()
    {
        stringKeyFields.push_back(makeCompactField("", {182.0f, 32.0f}));
        stringValueFields.push_back(makeCompactField("", {210.0f, 32.0f}));
        rebuildFocusOrder();
        activateField(&stringKeyFields.back());
        ensureActiveFieldVisible();
    }

    void addStringList()
    {
        StringListEditor editor;
        editor.keyField = makeCompactField("", {210.0f, 32.0f});
        editor.valueFields.push_back(makeCompactField("", {366.0f, 32.0f}));
        listEditors.push_back(std::move(editor));
        rebuildFocusOrder();
        activateField(&listEditors.back().keyField);
        ensureActiveFieldVisible();
    }

    void addStringListValue(std::size_t listIndex)
    {
        if (listIndex >= listEditors.size())
        {
            return;
        }
        listEditors[listIndex].valueFields.push_back(makeCompactField("", {366.0f, 32.0f}));
        rebuildFocusOrder();
        activateField(&listEditors[listIndex].valueFields.back());
        ensureActiveFieldVisible();
    }

    void removeKeyword(std::size_t index)
    {
        if (index < keywordFields.size())
        {
            keywordFields.erase(keywordFields.begin() + static_cast<std::ptrdiff_t>(index));
            rebuildFocusOrder();
            activateField(&titleField);
        }
    }

    void removeIntegerField(std::size_t index)
    {
        if (index < intKeyFields.size() && index < intValueFields.size())
        {
            intKeyFields.erase(intKeyFields.begin() + static_cast<std::ptrdiff_t>(index));
            intValueFields.erase(intValueFields.begin() + static_cast<std::ptrdiff_t>(index));
            rebuildFocusOrder();
            activateField(&titleField);
        }
    }

    void removeStringField(std::size_t index)
    {
        if (index < stringKeyFields.size() && index < stringValueFields.size())
        {
            stringKeyFields.erase(stringKeyFields.begin() + static_cast<std::ptrdiff_t>(index));
            stringValueFields.erase(stringValueFields.begin() + static_cast<std::ptrdiff_t>(index));
            rebuildFocusOrder();
            activateField(&titleField);
        }
    }

    void removeStringList(std::size_t index)
    {
        if (index < listEditors.size())
        {
            listEditors.erase(listEditors.begin() + static_cast<std::ptrdiff_t>(index));
            rebuildFocusOrder();
            activateField(&titleField);
        }
    }

    void removeStringListValue(std::size_t listIndex, std::size_t valueIndex)
    {
        if (listIndex < listEditors.size() && valueIndex < listEditors[listIndex].valueFields.size())
        {
            listEditors[listIndex].valueFields.erase(listEditors[listIndex].valueFields.begin() + static_cast<std::ptrdiff_t>(valueIndex));
            rebuildFocusOrder();
            activateField(&titleField);
        }
    }

    void ensureActiveFieldVisible()
    {
        if (focusOrder.empty())
        {
            return;
        }

        layoutArrayControls();
        const sf::FloatRect bounds = focusOrder[focusIndex]->bounds();
        if (focusOrder[focusIndex] == &titleField || focusOrder[focusIndex] == &imageField || focusOrder[focusIndex] == &typeField)
        {
            return;
        }
        if (bounds.position.y < ArrayViewportTop)
        {
            editorScroll -= ArrayViewportTop - bounds.position.y;
        }
        else if (bounds.position.y + bounds.size.y > ArrayViewportBottom)
        {
            editorScroll += bounds.position.y + bounds.size.y - ArrayViewportBottom;
        }
        clampEditorScroll();
        layoutArrayControls();
    }

    InputBox* dynamicFieldAt(sf::Vector2f mouse)
    {
        if (!isInArrayViewport(mouse))
        {
            return nullptr;
        }
        for (InputBox& field : keywordFields)
        {
            if (isVisibleInArrayViewport(field.bounds()) && field.contains(mouse))
            {
                return &field;
            }
        }
        for (std::size_t i = 0; i < intKeyFields.size() && i < intValueFields.size(); ++i)
        {
            if (isVisibleInArrayViewport(intKeyFields[i].bounds()) && intKeyFields[i].contains(mouse))
            {
                return &intKeyFields[i];
            }
            if (isVisibleInArrayViewport(intValueFields[i].bounds()) && intValueFields[i].contains(mouse))
            {
                return &intValueFields[i];
            }
        }
        for (std::size_t i = 0; i < stringKeyFields.size() && i < stringValueFields.size(); ++i)
        {
            if (isVisibleInArrayViewport(stringKeyFields[i].bounds()) && stringKeyFields[i].contains(mouse))
            {
                return &stringKeyFields[i];
            }
            if (isVisibleInArrayViewport(stringValueFields[i].bounds()) && stringValueFields[i].contains(mouse))
            {
                return &stringValueFields[i];
            }
        }
        for (StringListEditor& editor : listEditors)
        {
            if (isVisibleInArrayViewport(editor.keyField.bounds()) && editor.keyField.contains(mouse))
            {
                return &editor.keyField;
            }
            for (InputBox& field : editor.valueFields)
            {
                if (isVisibleInArrayViewport(field.bounds()) && field.contains(mouse))
                {
                    return &field;
                }
            }
        }
        return nullptr;
    }

    void moveFocus(int delta)
    {
        focusOrder[focusIndex]->setActive(false);
        focusIndex = static_cast<std::size_t>((static_cast<int>(focusIndex) + delta + static_cast<int>(focusOrder.size())) % static_cast<int>(focusOrder.size()));
        focusOrder[focusIndex]->setActive(true);
    }

    bool handleClick(sf::Vector2f mouse)
    {
        layoutArrayControls();

        if (backButton.contains(mouse))
        {
            return true;
        }
        if (newButton.contains(mouse))
        {
            createNewCard();
            return false;
        }
        if (refreshButton.contains(mouse))
        {
            loadCards();
            return false;
        }
        if (saveButton.contains(mouse))
        {
            saveCurrentCard();
            return false;
        }
        if (deleteButton.contains(mouse))
        {
            deleteCurrentCard();
            return false;
        }
        if (isInArrayViewport(mouse))
        {
            if (isVisibleInArrayViewport(addKeywordButton.bounds()) && addKeywordButton.contains(mouse))
            {
                addKeyword();
                return false;
            }
            if (isVisibleInArrayViewport(addIntegerButton.bounds()) && addIntegerButton.contains(mouse))
            {
                addIntegerField();
                return false;
            }
            if (isVisibleInArrayViewport(addStringButton.bounds()) && addStringButton.contains(mouse))
            {
                addStringField();
                return false;
            }
            if (isVisibleInArrayViewport(addListButton.bounds()) && addListButton.contains(mouse))
            {
                addStringList();
                return false;
            }
            for (std::size_t i = 0; i < removeKeywordButtons.size(); ++i)
            {
                if (isVisibleInArrayViewport(removeKeywordButtons[i].bounds()) && removeKeywordButtons[i].contains(mouse))
                {
                    removeKeyword(i);
                    return false;
                }
            }
            for (std::size_t i = 0; i < removeIntegerButtons.size(); ++i)
            {
                if (isVisibleInArrayViewport(removeIntegerButtons[i].bounds()) && removeIntegerButtons[i].contains(mouse))
                {
                    removeIntegerField(i);
                    return false;
                }
            }
            for (std::size_t i = 0; i < removeStringButtons.size(); ++i)
            {
                if (isVisibleInArrayViewport(removeStringButtons[i].bounds()) && removeStringButtons[i].contains(mouse))
                {
                    removeStringField(i);
                    return false;
                }
            }
            for (std::size_t i = 0; i < addListValueButtons.size(); ++i)
            {
                if (isVisibleInArrayViewport(addListValueButtons[i].bounds()) && addListValueButtons[i].contains(mouse))
                {
                    addStringListValue(i);
                    return false;
                }
            }
            for (std::size_t i = 0; i < removeListButtons.size(); ++i)
            {
                if (isVisibleInArrayViewport(removeListButtons[i].bounds()) && removeListButtons[i].contains(mouse))
                {
                    removeStringList(i);
                    return false;
                }
            }
            for (std::size_t listIndex = 0; listIndex < removeListValueButtons.size(); ++listIndex)
            {
                for (std::size_t valueIndex = 0; valueIndex < removeListValueButtons[listIndex].size(); ++valueIndex)
                {
                    EditorButton& button = removeListValueButtons[listIndex][valueIndex];
                    if (isVisibleInArrayViewport(button.bounds()) && button.contains(mouse))
                    {
                        removeStringListValue(listIndex, valueIndex);
                        return false;
                    }
                }
            }
            if (InputBox* field = dynamicFieldAt(mouse))
            {
                activateField(field);
                field->beginMouseSelection(mouse, sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                                   sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift));
                return false;
            }
        }

        const std::optional<std::size_t> listIndex = cardIndexAt(mouse);
        if (listIndex)
        {
            selectCard(*listIndex);
            return false;
        }
        if (titleField.contains(mouse))
        {
            activateField(&titleField);
            titleField.beginMouseSelection(mouse, sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                                  sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift));
            return false;
        }
        if (imageField.contains(mouse))
        {
            activateField(&imageField);
            imageField.beginMouseSelection(mouse, sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                                  sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift));
            return false;
        }
        if (typeField.contains(mouse))
        {
            activateField(&typeField);
            typeField.beginMouseSelection(mouse, sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                                 sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift));
            return false;
        }
        for (InputBox* field : focusOrder)
        {
            field->setActive(false);
        }
        return false;
    }

    std::optional<std::size_t> cardIndexAt(sf::Vector2f mouse) const
    {
        if (mouse.x < 42.0f || mouse.x > 272.0f || mouse.y < ListRowStartY)
        {
            return std::nullopt;
        }
        const std::size_t visibleIndex = static_cast<std::size_t>((mouse.y - ListRowStartY) / ListRowHeight);
        const std::size_t index = listOffset + visibleIndex;
        if (visibleIndex < VisibleCardRows && index < cards.size())
        {
            return index;
        }
        return std::nullopt;
    }

    bool isInListPanel(sf::Vector2f mouse) const
    {
        return mouse.x >= ListPanelX && mouse.x <= ListPanelX + ListPanelWidth &&
            mouse.y >= ListPanelY && mouse.y <= ListPanelY + PanelHeight;
    }

    void drawHeader(sf::RenderWindow& window)
    {
        sf::RectangleShape bar({EditorWidth, 80.0f});
        bar.setFillColor(sf::Color(24, 29, 38));
        window.draw(bar);
        drawText(window, font, "Bayou Card Editor", 30, {30.0f, 22.0f}, Ink);
        drawText(window, font, fmt::format("Card server {}", endpointText()), 15, {890.0f, 31.0f}, Muted, 214.0f);
        backButton.draw(window);
    }

    void drawListPanel(sf::RenderWindow& window)
    {
        drawRoundedPanel(window, {ListPanelX, ListPanelY}, {ListPanelWidth, PanelHeight}, Panel);
        drawText(window, font, "Library", 22, {42.0f, 124.0f}, Ink);
        drawText(window, font, fmt::format("{} cards", cards.size()), 14, {218.0f, 131.0f}, Muted);

        const std::size_t lastVisible = std::min(cards.size(), listOffset + VisibleCardRows);
        for (std::size_t i = listOffset; i < lastVisible; ++i)
        {
            const float y = ListRowStartY + static_cast<float>(i - listOffset) * ListRowHeight;
            sf::RectangleShape row({230.0f, 48.0f});
            row.setPosition({42.0f, y});
            row.setFillColor(selectedCard && *selectedCard == i ? sf::Color(49, 68, 78) : PanelAlt);
            row.setOutlineThickness(1.0f);
            row.setOutlineColor(selectedCard && *selectedCard == i ? Accent : sf::Color(48, 56, 70));
            window.draw(row);
            drawText(window, font, cards[i].title, 17, {54.0f, y + 8.0f}, Ink, 206.0f);
            drawText(window, font, cards[i].type, 13, {54.0f, y + 29.0f}, Muted);
        }
        if (cards.size() > VisibleCardRows)
        {
            drawText(window, font, fmt::format("{}-{} of {}  mouse wheel", listOffset + 1, lastVisible, cards.size()), 12, {46.0f, 660.0f}, Muted, 220.0f);
        }
        newButton.draw(window);
        refreshButton.draw(window);
    }

    void drawEditorPanel(sf::RenderWindow& window)
    {
        drawRoundedPanel(window, {EditorPanelX, EditorPanelY}, {EditorPanelWidth, PanelHeight}, Panel);
        drawText(window, font, "Edit Card", 22, {340.0f, 124.0f}, Ink);
        drawText(window, font, "Tab moves fields. Enter saves.", 14, {610.0f, 131.0f}, Muted, 206.0f);
        titleField.draw(window);
        imageField.draw(window);
        typeField.draw(window);
        drawArrayEditor(window);
        deleteButton.draw(window);
        saveButton.draw(window);
        drawText(window, font, status, 16, {340.0f, 702.0f}, statusColor, 174.0f);
    }

    void drawArrayEditor(sf::RenderWindow& window)
    {
        layoutArrayControls();
        sf::RectangleShape viewport({482.0f, ArrayViewportHeight});
        viewport.setPosition({334.0f, ArrayViewportTop});
        viewport.setFillColor(sf::Color(26, 31, 40));
        viewport.setOutlineThickness(1.0f);
        viewport.setOutlineColor(Line);
        window.draw(viewport);

        for (const auto& [label, position] : arraySectionLabels)
        {
            if (position.y >= ArrayViewportTop && position.y <= ArrayViewportBottom - 16.0f)
            {
                drawText(window, font, label, 14, position, label.rfind("No ", 0) == 0 ? Muted : Ink, 320.0f);
            }
        }

        drawVisibleButton(window, addKeywordButton);
        drawVisibleButton(window, addIntegerButton);
        drawVisibleButton(window, addStringButton);
        drawVisibleButton(window, addListButton);
        for (InputBox& field : keywordFields)
        {
            drawVisibleField(window, field);
        }
        for (std::size_t i = 0; i < intKeyFields.size() && i < intValueFields.size(); ++i)
        {
            drawVisibleField(window, intKeyFields[i]);
            drawVisibleField(window, intValueFields[i]);
        }
        for (std::size_t i = 0; i < stringKeyFields.size() && i < stringValueFields.size(); ++i)
        {
            drawVisibleField(window, stringKeyFields[i]);
            drawVisibleField(window, stringValueFields[i]);
        }
        for (StringListEditor& editor : listEditors)
        {
            drawVisibleField(window, editor.keyField);
            for (InputBox& field : editor.valueFields)
            {
                drawVisibleField(window, field);
            }
        }
        for (EditorButton& button : removeKeywordButtons)
        {
            drawVisibleButton(window, button);
        }
        for (EditorButton& button : removeIntegerButtons)
        {
            drawVisibleButton(window, button);
        }
        for (EditorButton& button : removeStringButtons)
        {
            drawVisibleButton(window, button);
        }
        for (EditorButton& button : removeListButtons)
        {
            drawVisibleButton(window, button);
        }
        for (EditorButton& button : addListValueButtons)
        {
            drawVisibleButton(window, button);
        }
        for (std::vector<EditorButton>& buttons : removeListValueButtons)
        {
            for (EditorButton& button : buttons)
            {
                drawVisibleButton(window, button);
            }
        }
        if (editorContentHeight > ArrayViewportHeight)
        {
            drawText(window, font, "mouse wheel", 12, {730.0f, 656.0f}, Muted);
        }
    }

    void drawVisibleField(sf::RenderWindow& window, InputBox& field)
    {
        if (isVisibleInArrayViewport(field.bounds()))
        {
            field.draw(window);
        }
    }

    void drawVisibleButton(sf::RenderWindow& window, EditorButton& button)
    {
        if (isVisibleInArrayViewport(button.bounds()))
        {
            button.draw(window);
        }
    }

    void drawPreviewPanel(sf::RenderWindow& window)
    {
        drawRoundedPanel(window, {PreviewPanelX, PreviewPanelY}, {PreviewPanelWidth, PanelHeight}, Panel);
        drawText(window, font, "Preview", 22, {882.0f, 124.0f}, Ink);
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
        sf::Text title(font, elideToWidth(font, card.title.empty() ? "Untitled Card" : card.title, 22, 210.0f), 22);
        title.setFillColor(Ink);
        centerText(title, {1053.0f, 414.0f});
        window.draw(title);
        sf::Text type(font, card.type, 16);
        type.setFillColor(Accent);
        centerText(type, {1053.0f, 445.0f});
        window.draw(type);

        float y = 512.0f;
        drawText(window, font, "Keywords", 15, {882.0f, y}, Muted);
        drawText(window, font, joinStrings(card.keywords, ", "), 16, {882.0f, y + 22.0f}, Ink, 336.0f);
        y += 54.0f;
        drawText(window, font, "Integer Fields", 15, {882.0f, y}, Muted);
        drawText(window, font, integerPairsToText(card.integerValues), 16, {882.0f, y + 22.0f}, Ink, 336.0f);
        y += 54.0f;
        drawText(window, font, "String Fields", 15, {882.0f, y}, Muted);
        drawText(window, font, stringPairsToText(card.stringValues), 16, {882.0f, y + 22.0f}, Ink, 336.0f);
        y += 54.0f;
        drawText(window, font, "String Lists", 15, {882.0f, y}, Muted);
        drawText(window, font, stringListsToText(card.stringLists), 16, {882.0f, y + 22.0f}, Ink, 336.0f);
    }

    void setStatus(const std::string& message, sf::Color color)
    {
        status = message;
        statusColor = color;
    }
};
