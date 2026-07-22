module;

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include "tls_socket.hpp"
#include <fmt/core.h>

#include "card_editor_assets.hpp"
#include "client_string.hpp"
#include "client_ui.hpp"

#include "../shared/card_data.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

export module card_editor_screen;

import client_controls;
import inputbox;
import network;

namespace
{
using bayou::client::lowerKey;
using bayou::client::trim;
using bayou::client::card_editor_assets::assetRelativeImagePath;
using bayou::client::card_editor_assets::normalizeCardImagePath;
using bayou::client::card_editor_assets::resolveAssetImagePath;
using bayou::client::card_editor_assets::setAssetRoot;

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
constexpr float ActionEditorPanelWidth = PreviewPanelX + PreviewPanelWidth - EditorPanelX;
constexpr float ListRowStartY = 176.0f;
constexpr float ListRowHeight = 56.0f;
constexpr std::size_t VisibleCardRows = 8;
constexpr std::size_t VisibleActionRows = 8;
constexpr std::size_t VisibleActionDropdownRows = 5;
constexpr std::size_t VisibleTargetFilterRows = 12;
constexpr float ActionDropdownRowHeight = 32.0f;
constexpr float TargetFilterTop = 224.0f;
constexpr float TargetFilterRowHeight = 36.0f;

const sf::Color Ink(244, 234, 208);
const sf::Color Muted(181, 166, 137);
const sf::Color Panel(9, 15, 16, 244);
const sf::Color PanelAlt(18, 25, 25, 246);
const sf::Color Accent(239, 190, 98);
const sf::Color AccentDark(90, 58, 29);
const sf::Color Warn(213, 102, 79);
const sf::Color Line(132, 91, 47);

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

std::vector<std::string> wrapText(sf::Font& font, const std::string& value, unsigned int size, float maxWidth)
{
    std::vector<std::string> lines;
    std::string line;
    std::string word;

    auto appendWord = [&]() {
        if (word.empty())
        {
            return;
        }
        const std::string candidate = line.empty() ? word : line + " " + word;
        sf::Text text(font, candidate, size);
        if (!line.empty() && text.getLocalBounds().size.x > maxWidth)
        {
            lines.push_back(line);
            line = word;
        }
        else
        {
            line = candidate;
        }
        word.clear();
    };

    for (char ch : value)
    {
        if (ch == '\n')
        {
            appendWord();
            lines.push_back(line);
            line.clear();
        }
        else if (std::isspace(static_cast<unsigned char>(ch)) != 0)
        {
            appendWord();
        }
        else
        {
            word += ch;
        }
    }
    appendWord();
    if (!line.empty())
    {
        lines.push_back(line);
    }
    if (lines.empty())
    {
        lines.push_back("");
    }
    return lines;
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
    bayou::client::drawBeveledPlate(window, position, size, fill, outline, false, 12.0f);
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
        shape.setOutlineThickness(2.0f);
        shape.setOutlineColor(sf::Color(181, 126, 60));
        text->setFillColor(Ink);
        bayou::client::centerButtonText(*text, {position.x + size.x / 2.0f, position.y + size.y / 2.0f});
    }

    bool contains(sf::Vector2f point) const
    {
        return enabled && shape.getGlobalBounds().contains(point);
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
            bayou::client::centerButtonText(
                *text,
                {position.x + shape.getSize().x / 2.0f, position.y + shape.getSize().y / 2.0f});
        }
    }

    void setEnabled(bool value)
    {
        enabled = value;
        isHovered = false;
        shape.setFillColor(enabled ? base : sf::Color(47, 53, 54, 220));
        if (text)
        {
            text->setFillColor(enabled ? Ink : sf::Color(123, 129, 128));
        }
    }

    void update(sf::Vector2f mouse)
    {
        if (!enabled)
        {
            return;
        }
        const bool hovered = contains(mouse);
        shape.setFillColor(hovered
            ? sf::Color(std::min(base.r + 22, 255), std::min(base.g + 22, 255), std::min(base.b + 22, 255), base.a)
            : base);
        isHovered = hovered;
    }

    void draw(sf::RenderWindow& window) const
    {
        bayou::client::drawBeveledPlate(
            window,
            shape.getPosition(),
            shape.getSize(),
            shape.getFillColor(),
            enabled
                ? (isHovered ? sf::Color(239, 190, 98) : sf::Color(181, 126, 60))
                : sf::Color(77, 83, 83),
            isHovered,
            std::clamp(shape.getSize().y * 0.20f, 5.0f, 10.0f));
        window.draw(*text);
    }

private:
    std::optional<sf::Text> text;
    sf::RectangleShape shape;
    sf::Color base = AccentDark;
    bool isHovered = false;
    bool enabled = true;
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

std::string uniqueCopyName(const std::string& sourceName, const std::set<std::string>& existingNames)
{
    for (int suffix = 1;; ++suffix)
    {
        const std::string candidate = fmt::format("{} ({})", sourceName, suffix);
        if (!existingNames.contains(candidate))
        {
            return candidate;
        }
    }
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
    explicit CardEditorScreen(
        sf::Font& screenFont,
        CardEditorEndpoint screenEndpoint = {},
        std::filesystem::path assetRoot = "assets")
        : font(screenFont)
        , endpoint(std::move(screenEndpoint))
        , editorTabs({340.0f, 14.0f}, {128.0f, 44.0f}, {"Cards", "Actions"}, screenFont)
    {
        setAssetRoot(std::move(assetRoot));
        buildControls();
    }

    void setEndpoint(CardEditorEndpoint screenEndpoint)
    {
        endpoint = std::move(screenEndpoint);
    }

    void open()
    {
        unsavedChangesPopupVisible = false;
        pendingTransition = PendingTransition::None;
        pendingSelectionName.clear();
        unsavedChangesError.clear();
        loadCards();
        loadActions();
    }

    bool handleEvent(const sf::Event& event, sf::RenderWindow& window)
    {
        const sf::View previousView = window.getView();
        window.setView(makeEditorView(window));

        bool shouldClose = false;
        if (instructionsVisible)
        {
            if (const auto* keyEvent = event.getIf<sf::Event::KeyPressed>())
            {
                if (keyEvent->code == sf::Keyboard::Key::Escape)
                {
                    instructionsVisible = false;
                }
                else if (keyEvent->code == sf::Keyboard::Key::PageDown)
                {
                    scrollInstructions(480.0f);
                }
                else if (keyEvent->code == sf::Keyboard::Key::PageUp)
                {
                    scrollInstructions(-480.0f);
                }
                else if (keyEvent->code == sf::Keyboard::Key::Home)
                {
                    instructionScroll = 0.0f;
                }
                else if (keyEvent->code == sf::Keyboard::Key::End)
                {
                    instructionScroll = instructionMaxScroll();
                }
            }

            if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>())
            {
                const sf::Vector2f mouse = window.mapPixelToCoords(mousePressed->position);
                if (mousePressed->button == sf::Mouse::Button::Left && instructionsBackButton.contains(mouse))
                {
                    instructionsVisible = false;
                }
            }

            if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>())
            {
                scrollInstructions(wheel->delta < 0.0f ? 64.0f : -64.0f);
            }

            window.setView(previousView);
            return false;
        }

        if (unsavedChangesPopupVisible)
        {
            bool shouldClose = false;
            if (const auto* keyEvent = event.getIf<sf::Event::KeyPressed>())
            {
                if (keyEvent->code == sf::Keyboard::Key::Escape)
                {
                    keepEditing();
                }
                else if (keyEvent->code == sf::Keyboard::Key::Enter)
                {
                    shouldClose = saveAndContinue();
                }
            }
            if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>())
            {
                const sf::Vector2f mouse = window.mapPixelToCoords(mousePressed->position);
                if (mousePressed->button == sf::Mouse::Button::Left)
                {
                    if (saveChangesButton.contains(mouse))
                    {
                        shouldClose = saveAndContinue();
                    }
                    else if (discardChangesButton.contains(mouse))
                    {
                        shouldClose = discardAndContinue();
                    }
                    else if (keepEditingButton.contains(mouse))
                    {
                        keepEditing();
                    }
                }
            }
            window.setView(previousView);
            return shouldClose;
        }

        std::optional<std::size_t> activeActionReference;
        std::string previousActionReferenceValue;
        if (!focusOrder.empty())
        {
            const std::string previousImagePath = imageField.getValue();
            if (editorMode == EditorMode::Cards)
            {
                activeActionReference = actionReferenceIndex(focusOrder[focusIndex]);
                if (activeActionReference)
                {
                    previousActionReferenceValue = actionRefFields[*activeActionReference].getValue();
                }
            }
            focusOrder[focusIndex]->handleEvent(event, window);
            if (imageField.isActive() && imageField.getValue() != previousImagePath)
            {
                loadPreviewImage();
            }
            if (activeActionReference &&
                actionRefFields[*activeActionReference].getValue() != previousActionReferenceValue)
            {
                openActionDropdown(*activeActionReference, true);
            }
        }

        if (const auto* keyEvent = event.getIf<sf::Event::KeyPressed>())
        {
            bool handledDropdownKey = false;
            if (editorMode == EditorMode::Cards && activeActionReference)
            {
                if (keyEvent->code == sf::Keyboard::Key::Escape && openActionDropdownIndex)
                {
                    closeActionDropdown();
                    handledDropdownKey = true;
                }
                else if (keyEvent->code == sf::Keyboard::Key::Up ||
                         keyEvent->code == sf::Keyboard::Key::Down)
                {
                    if (!openActionDropdownIndex)
                    {
                        openActionDropdown(*activeActionReference, false);
                    }
                    moveActionDropdownSelection(keyEvent->code == sf::Keyboard::Key::Down ? 1 : -1);
                    handledDropdownKey = true;
                }
                else if (keyEvent->code == sf::Keyboard::Key::Enter && openActionDropdownIndex)
                {
                    chooseActionDropdownItem(actionDropdownSelection);
                    handledDropdownKey = true;
                }
            }

            if (!handledDropdownKey && keyEvent->code == sf::Keyboard::Key::Escape)
            {
                shouldClose = requestTransition(PendingTransition::CloseScreen);
            }
            else if (!handledDropdownKey && keyEvent->code == sf::Keyboard::Key::Tab)
            {
                moveFocus(keyEvent->shift ? -1 : 1);
                ensureActiveFieldVisible();
            }
            else if (!handledDropdownKey && keyEvent->code == sf::Keyboard::Key::Enter)
            {
                if (editorMode == EditorMode::Cards)
                {
                    if (hasUnsavedCardChanges())
                    {
                        saveCurrentCard();
                    }
                }
                else
                {
                    saveCurrentAction();
                }
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
            const std::optional<ActionDropdownLayout> dropdownLayout = actionDropdownLayout();
            if (editorMode == EditorMode::Cards && dropdownLayout && dropdownLayout->bounds.contains(mouse))
            {
                moveActionDropdownSelection(wheel->delta < 0.0f ? 1 : -1);
            }
            else if (isInListPanel(mouse))
            {
                if (editorMode == EditorMode::Cards)
                {
                    scrollCardList(wheel->delta < 0.0f ? 1 : -1);
                }
                else
                {
                    scrollActionList(wheel->delta < 0.0f ? 1 : -1);
                }
            }
            else if (editorMode == EditorMode::Cards && isInArrayViewport(mouse))
            {
                scrollEditorForm(wheel->delta < 0.0f ? 1 : -1);
            }
            else if (editorMode == EditorMode::Actions && isInActionTargetFilterViewport(mouse))
            {
                scrollActionTargetFilters(wheel->delta < 0.0f ? 1 : -1);
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
        instructionsButton.update(mouse);
        instructionsBackButton.update(mouse);
        if (instructionsVisible)
        {
            return;
        }
        if (unsavedChangesPopupVisible)
        {
            saveChangesButton.update(mouse);
            discardChangesButton.update(mouse);
            keepEditingButton.update(mouse);
            return;
        }
        newButton.update(mouse);
        refreshButton.update(mouse);
        copyButton.update(mouse);
        copyActionButton.update(mouse);
        saveButton.setEnabled(hasUnsavedCardChanges());
        saveButton.update(mouse);
        saveActionButton.update(mouse);
        deleteButton.update(mouse);
        editorTabs.update(mouse);
        if (editorMode == EditorMode::Actions)
        {
            hoveredActionDropdownItem.reset();
            hoveredActionLink.reset();
            layoutActionFields();
            addActionTargetFilterButton.update(mouse);
            for (EditorButton& button : removeActionTargetFilterButtons)
            {
                button.update(mouse);
            }
            if (!focusOrder.empty() && focusIndex < focusOrder.size())
            {
                focusOrder[focusIndex]->updateCursor(deltaTime);
            }
            return;
        }
        hoveredActionDropdownItem = actionDropdownItemAt(mouse);
        hoveredActionLink.reset();
        for (std::size_t i = 0; i < actionRefFields.size(); ++i)
        {
            if (isVisibleInArrayViewport(actionRefFields[i].bounds()) && linkedActionIndex(i) &&
                actionLinkButtonBounds(i).contains(mouse))
            {
                hoveredActionLink = i;
                break;
            }
        }
        addTraitButton.update(mouse);
        addKeywordButton.update(mouse);
        addIntegerButton.update(mouse);
        addStringButton.update(mouse);
        addListButton.update(mouse);
        addActionRefButton.update(mouse);
        for (EditorButton& button : removeTraitButtons)
        {
            button.update(mouse);
        }
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
        for (EditorButton& button : removeActionRefButtons)
        {
            button.update(mouse);
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
        background.setFillColor(sf::Color(5, 13, 15, 232));
        window.draw(background);
        drawHeader(window);
        if (instructionsVisible)
        {
            drawInstructions(window);
        }
        else
        {
            drawListPanel(window);
            if (editorMode == EditorMode::Cards)
            {
                drawEditorPanel(window);
                drawPreviewPanel(window);
            }
            else
            {
                drawActionEditorPanel(window);
            }
            if (unsavedChangesPopupVisible)
            {
                drawUnsavedChangesPopup(window);
            }
        }
        window.setView(previousView);
    }

private:
    enum class EditorMode
    {
        Cards,
        Actions
    };

    enum class PendingTransition
    {
        None,
        CloseScreen,
        ShowCards,
        ShowActions,
        NewCard,
        NewAction,
        RefreshCards,
        RefreshActions,
        SelectCard,
        SelectAction
    };

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

    struct ActionListResult
    {
        bool success = false;
        std::string message;
        std::vector<card_data::Action> actions;
    };

    struct StringListEditor
    {
        InputBox keyField;
        std::vector<InputBox> valueFields;
    };

    struct ActionDropdownLayout
    {
        sf::FloatRect bounds;
        std::size_t firstRow = 0;
        std::size_t visibleRows = 0;
    };

    static constexpr float ArrayViewportTop = 372.0f;
    static constexpr float ArrayViewportBottom = 676.0f;
    static constexpr float ArrayViewportHeight = ArrayViewportBottom - ArrayViewportTop;

    sf::Font& font;
    CardEditorEndpoint endpoint;
    EditorMode editorMode = EditorMode::Cards;
    std::vector<card_data::Card> cards;
    std::optional<std::size_t> selectedCard;
    std::size_t listOffset = 0;
    std::vector<card_data::Action> actions;
    std::optional<std::size_t> selectedAction;
    std::size_t actionListOffset = 0;
    std::size_t actionTargetFilterOffset = 0;
    std::string status = "Ready";
    sf::Color statusColor = Muted;
    std::vector<InputBox*> focusOrder;
    std::size_t focusIndex = 0;
    float editorScroll = 0.0f;
    float editorContentHeight = 0.0f;
    std::vector<std::pair<std::string, sf::Vector2f>> arraySectionLabels;
    sf::Texture previewTexture;
    bool hasPreviewImage = false;
    bool instructionsVisible = false;
    float instructionScroll = 0.0f;
    float instructionContentHeight = 1920.0f;
    bool unsavedChangesPopupVisible = false;
    PendingTransition pendingTransition = PendingTransition::None;
    std::string pendingSelectionName;
    std::string unsavedChangesError;
    std::string savedCardForm;
    std::string savedActionForm;

    InputBox titleField;
    InputBox imageField;
    InputBox typeField;
    std::vector<InputBox> traitFields;
    std::vector<InputBox> keywordFields;
    std::vector<InputBox> intKeyFields;
    std::vector<InputBox> intValueFields;
    std::vector<InputBox> stringKeyFields;
    std::vector<InputBox> stringValueFields;
    std::vector<StringListEditor> listEditors;
    std::vector<InputBox> actionDisplayNameFields;
    std::vector<InputBox> actionRefFields;
    std::optional<std::size_t> openActionDropdownIndex;
    std::size_t actionDropdownOffset = 0;
    std::size_t actionDropdownSelection = 0;
    bool actionDropdownFiltering = false;
    std::optional<std::size_t> hoveredActionDropdownItem;
    std::optional<std::size_t> hoveredActionLink;
    sf::Texture actionLinkTexture;
    bool hasActionLinkTexture = false;
    InputBox actionNameField;
    InputBox actionStateField;
    InputBox actionNextStateField;
    InputBox actionKindField;
    InputBox actionPatternField;
    InputBox actionMinRangeField;
    InputBox actionMaxRangeField;
    InputBox actionDamageField;
    InputBox actionHealField;
    InputBox actionCanMoveField;
    InputBox actionCanAttackField;
    InputBox actionPassThroughField;
    InputBox actionLineOfSightField;
    InputBox actionStatusTurnsField;
    InputBox actionCooldownTurnsField;
    InputBox actionPushField;
    std::vector<InputBox> actionTargetFilterFields;
    EditorButton backButton;
    EditorButton instructionsButton;
    EditorButton instructionsBackButton;
    EditorButton newButton;
    EditorButton refreshButton;
    EditorButton copyButton;
    EditorButton copyActionButton;
    EditorButton saveButton;
    EditorButton saveActionButton;
    EditorButton deleteButton;
    TabStrip editorTabs;
    EditorButton addTraitButton;
    EditorButton addKeywordButton;
    EditorButton addIntegerButton;
    EditorButton addStringButton;
    EditorButton addListButton;
    EditorButton addActionRefButton;
    EditorButton addActionTargetFilterButton;
    EditorButton saveChangesButton;
    EditorButton discardChangesButton;
    EditorButton keepEditingButton;
    std::vector<EditorButton> removeTraitButtons;
    std::vector<EditorButton> removeKeywordButtons;
    std::vector<EditorButton> removeIntegerButtons;
    std::vector<EditorButton> removeStringButtons;
    std::vector<EditorButton> removeListButtons;
    std::vector<EditorButton> addListValueButtons;
    std::vector<std::vector<EditorButton>> removeListValueButtons;
    std::vector<EditorButton> removeActionRefButtons;
    std::vector<EditorButton> removeActionTargetFilterButtons;

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

    bool connectToServer(bayou::tls::Socket& socket) const
    {
        const std::optional<sf::IpAddress> address = sf::IpAddress::resolve(endpoint.host);
        if (!address)
        {
            return false;
        }
        socket.setServerName(endpoint.host);
        return socket.connect(*address, endpoint.port) == sf::Socket::Status::Done;
    }

    CardListResult fetchCardsFromServer() const
    {
        bayou::tls::Socket socket;
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
        response >> responseType >> success >> message;
        if (!response || static_cast<network::MessageType>(responseType) != network::MessageType::CardListResponse)
        {
            socket.disconnect();
            return {false, "Unexpected card list response"};
        }

        if (!success)
        {
            socket.disconnect();
            return {false, message};
        }

        std::uint32_t count = 0;
        bool legacyFormat = false;
        bool actionIncludesNextState = false;
        if (!card_data::readCardListHeader(
                response, count, legacyFormat, &actionIncludesNextState))
        {
            socket.disconnect();
            return {false, "Unsupported card list payload"};
        }

        std::vector<card_data::Card> loadedCards;
        loadedCards.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            card_data::Card card;
            if (!card_data::readListedCard(
                    response, card, legacyFormat, actionIncludesNextState))
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

    ActionListResult fetchActionsFromServer() const
    {
        bayou::tls::Socket socket;
        if (!connectToServer(socket))
        {
            return {false, "Failed to connect to card server " + endpointText()};
        }

        sf::Packet request;
        request << static_cast<std::uint8_t>(network::MessageType::ActionListRequest);
        if (socket.send(request) != sf::Socket::Status::Done)
        {
            socket.disconnect();
            return {false, "Failed to send action list request"};
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
        if (!response || static_cast<network::MessageType>(responseType) != network::MessageType::ActionListResponse)
        {
            socket.disconnect();
            return {false, "Unexpected action list response"};
        }

        std::vector<card_data::Action> loadedActions;
        loadedActions.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            card_data::Action action;
            if (!card_data::readAction(response, action))
            {
                socket.disconnect();
                return {false, "Invalid action list payload"};
            }
            loadedActions.push_back(action);
        }
        socket.disconnect();
        return {success, message, std::move(loadedActions)};
    }

    CommandResult readCommandResponse(bayou::tls::Socket& socket, network::MessageType expectedResponseType, const std::string& action) const
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

        bayou::tls::Socket socket;
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

        bayou::tls::Socket socket;
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
        bayou::tls::Socket socket;
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

    CommandResult saveActionToServer(const card_data::Action& action) const
    {
        bayou::tls::Socket socket;
        if (!connectToServer(socket))
        {
            return {false, "Failed to connect to card server " + endpointText()};
        }
        sf::Packet request;
        request << static_cast<std::uint8_t>(network::MessageType::ActionUpsertRequest);
        card_data::writeAction(request, action);
        if (socket.send(request) != sf::Socket::Status::Done)
        {
            socket.disconnect();
            return {false, "Failed to send action save request"};
        }
        CommandResult result = readCommandResponse(socket, network::MessageType::ActionUpsertResponse, "saving action");
        socket.disconnect();
        return result;
    }

    CommandResult updateActionOnServer(const std::string& originalName, const card_data::Action& action) const
    {
        bayou::tls::Socket socket;
        if (!connectToServer(socket))
        {
            return {false, "Failed to connect to card server " + endpointText()};
        }
        sf::Packet request;
        request << static_cast<std::uint8_t>(network::MessageType::ActionUpdateRequest) << originalName;
        card_data::writeAction(request, action);
        if (socket.send(request) != sf::Socket::Status::Done)
        {
            socket.disconnect();
            return {false, "Failed to send action update request"};
        }
        CommandResult result = readCommandResponse(socket, network::MessageType::ActionUpdateResponse, "updating action");
        socket.disconnect();
        return result;
    }

    CommandResult deleteActionFromServer(const std::string& name) const
    {
        bayou::tls::Socket socket;
        if (!connectToServer(socket))
        {
            return {false, "Failed to connect to card server " + endpointText()};
        }
        sf::Packet request;
        request << static_cast<std::uint8_t>(network::MessageType::ActionDeleteRequest) << name;
        if (socket.send(request) != sf::Socket::Status::Done)
        {
            socket.disconnect();
            return {false, "Failed to send action delete request"};
        }
        CommandResult result = readCommandResponse(socket, network::MessageType::ActionDeleteResponse, "deleting action");
        socket.disconnect();
        return result;
    }

    void buildControls()
    {
        backButton = EditorButton(font, "Back", {1124.0f, 22.0f}, {112.0f, 38.0f}, sf::Color(67, 48, 33));
        instructionsButton = EditorButton(font, "Instructions", {970.0f, 22.0f}, {142.0f, 38.0f}, AccentDark);
        instructionsBackButton = EditorButton(font, "Back to Editor", {1060.0f, 22.0f}, {176.0f, 38.0f}, AccentDark);
        editorTabs.setActive(static_cast<std::size_t>(editorMode));
        newButton = EditorButton(font, "New", {42.0f, 690.0f}, {96.0f, 42.0f}, sf::Color(67, 48, 33));
        refreshButton = EditorButton(font, "Refresh", {150.0f, 690.0f}, {120.0f, 42.0f}, sf::Color(67, 48, 33));
        copyButton = EditorButton(font, "Copy Card", {42.0f, 642.0f}, {228.0f, 38.0f}, AccentDark);
        copyActionButton = EditorButton(font, "Copy Action", {42.0f, 642.0f}, {228.0f, 38.0f}, AccentDark);
        saveButton = EditorButton(font, "Save Card", {660.0f, 690.0f}, {156.0f, 42.0f}, AccentDark);
        saveButton.setEnabled(false);
        saveActionButton = EditorButton(font, "Save Action", {660.0f, 690.0f}, {156.0f, 42.0f}, AccentDark);
        deleteButton = EditorButton(font, "Delete", {528.0f, 690.0f}, {120.0f, 42.0f}, Warn);
        addTraitButton = EditorButton(font, "+", {778.0f, 372.0f}, {32.0f, 28.0f}, AccentDark);
        addKeywordButton = EditorButton(font, "+", {778.0f, 372.0f}, {32.0f, 28.0f}, AccentDark);
        addIntegerButton = EditorButton(font, "+", {778.0f, 372.0f}, {32.0f, 28.0f}, AccentDark);
        addStringButton = EditorButton(font, "+", {778.0f, 372.0f}, {32.0f, 28.0f}, AccentDark);
        addListButton = EditorButton(font, "+", {778.0f, 372.0f}, {32.0f, 28.0f}, AccentDark);
        addActionRefButton = EditorButton(font, "+", {778.0f, 372.0f}, {32.0f, 28.0f}, AccentDark);
        addActionTargetFilterButton = EditorButton(font, "+", {1184.0f, 164.0f}, {32.0f, 28.0f}, AccentDark);
        saveChangesButton = EditorButton(font, "Save", {414.0f, 412.0f}, {136.0f, 42.0f}, AccentDark);
        discardChangesButton = EditorButton(font, "Discard", {572.0f, 412.0f}, {136.0f, 42.0f}, Warn);
        keepEditingButton = EditorButton(font, "Keep Editing", {730.0f, 412.0f}, {154.0f, 42.0f}, sf::Color(67, 48, 33));
        titleField = InputBox(font, "Title", {340.0f, 168.0f}, {470.0f, 42.0f});
        imageField = InputBox(font, "Image Path", {340.0f, 238.0f}, {470.0f, 42.0f});
        typeField = InputBox(font, "Type", {340.0f, 308.0f}, {470.0f, 42.0f});
        actionNameField = InputBox(font, "Name", {340.0f, 168.0f}, {470.0f, 42.0f});
        actionStateField = makeCompactField("0", {210.0f, 32.0f});
        actionNextStateField = makeCompactField("0", {210.0f, 32.0f});
        actionKindField = makeCompactField("slide", {210.0f, 32.0f});
        actionPatternField = makeCompactField("omni", {210.0f, 32.0f});
        actionMinRangeField = makeCompactField("1", {210.0f, 32.0f});
        actionMaxRangeField = makeCompactField("1", {210.0f, 32.0f});
        actionDamageField = makeCompactField("0", {210.0f, 32.0f});
        actionHealField = makeCompactField("0", {210.0f, 32.0f});
        actionCanMoveField = makeCompactField("1", {210.0f, 32.0f});
        actionCanAttackField = makeCompactField("0", {210.0f, 32.0f});
        actionPassThroughField = makeCompactField("0", {210.0f, 32.0f});
        actionLineOfSightField = makeCompactField("0", {210.0f, 32.0f});
        actionStatusTurnsField = makeCompactField("0", {210.0f, 32.0f});
        actionCooldownTurnsField = makeCompactField("0", {210.0f, 32.0f});
        actionPushField = makeCompactField("0", {210.0f, 32.0f});
        if (const std::optional<std::filesystem::path> path = resolveAssetImagePath("ui/action-link.png"))
        {
            hasActionLinkTexture = actionLinkTexture.loadFromFile(*path);
            actionLinkTexture.setSmooth(true);
        }
        rebuildFocusOrder();
        activateField(&titleField);
    }

    InputBox makeCompactField(const std::string& value, sf::Vector2f size)
    {
        InputBox field(font, "", {0.0f, 0.0f}, size);
        field.setValue(value);
        return field;
    }

    InputBox makeActionReferenceField(const std::string& value)
    {
        InputBox field = makeCompactField(value, {210.0f, 32.0f});
        field.setRightContentInset(28.0f);
        return field;
    }

    InputBox makeActionDisplayNameField(const std::string& value)
    {
        return makeCompactField(value, {176.0f, 32.0f});
    }

    EditorButton makeMiniButton(const std::string& label, sf::Color color)
    {
        return EditorButton(font, label, {0.0f, 0.0f}, {32.0f, 28.0f}, color);
    }

    void loadArrayFields(const card_data::Card& card)
    {
        traitFields.clear();
        keywordFields.clear();
        intKeyFields.clear();
        intValueFields.clear();
        stringKeyFields.clear();
        stringValueFields.clear();
        listEditors.clear();
        actionDisplayNameFields.clear();
        actionRefFields.clear();

        for (const std::string& trait : card.traits)
        {
            traitFields.push_back(makeCompactField(trait, {392.0f, 32.0f}));
        }
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
            if (lowerKey(item.key) == "actions")
            {
                continue;
            }
            StringListEditor editor;
            editor.keyField = makeCompactField(item.key, {210.0f, 32.0f});
            for (const std::string& value : item.values)
            {
                editor.valueFields.push_back(makeCompactField(value, {366.0f, 32.0f}));
            }
            listEditors.push_back(std::move(editor));
        }
        for (std::size_t i = 0; i < card.actionNames.size(); ++i)
        {
            const std::string& actionName = card.actionNames[i];
            const std::string displayName = i < card.actionDisplayNames.size() &&
                    !card.actionDisplayNames[i].empty()
                ? card.actionDisplayNames[i]
                : actionName;
            actionDisplayNameFields.push_back(makeActionDisplayNameField(displayName));
            actionRefFields.push_back(makeActionReferenceField(actionName));
        }
    }

    void rebuildFocusOrder()
    {
        focusOrder.clear();
        if (editorMode == EditorMode::Actions)
        {
            focusOrder = {
                &actionNameField,
                &actionStateField,
                &actionNextStateField,
                &actionKindField,
                &actionPatternField,
                &actionMinRangeField,
                &actionMaxRangeField,
                &actionDamageField,
                &actionHealField,
                &actionCanMoveField,
                &actionCanAttackField,
                &actionPassThroughField,
                &actionLineOfSightField,
                &actionStatusTurnsField,
                &actionCooldownTurnsField,
                &actionPushField,
            };
            for (InputBox& field : actionTargetFilterFields)
            {
                focusOrder.push_back(&field);
            }
            focusIndex = std::min(focusIndex, focusOrder.size() - 1);
            return;
        }
        focusOrder.push_back(&titleField);
        focusOrder.push_back(&imageField);
        focusOrder.push_back(&typeField);
        for (InputBox& field : traitFields)
        {
            focusOrder.push_back(&field);
        }
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
        for (std::size_t i = 0; i < actionRefFields.size(); ++i)
        {
            if (i < actionDisplayNameFields.size())
            {
                focusOrder.push_back(&actionDisplayNameFields[i]);
            }
            focusOrder.push_back(&actionRefFields[i]);
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

    std::optional<std::size_t> actionReferenceIndex(const InputBox* field) const
    {
        for (std::size_t i = 0; i < actionRefFields.size(); ++i)
        {
            if (&actionRefFields[i] == field)
            {
                return i;
            }
        }
        return std::nullopt;
    }

    std::optional<std::size_t> linkedActionIndex(std::size_t referenceIndex) const
    {
        if (referenceIndex >= actionRefFields.size())
        {
            return std::nullopt;
        }
        const std::string name = trim(actionRefFields[referenceIndex].getValue());
        const auto found = std::find_if(actions.begin(), actions.end(), [&](const card_data::Action& action) {
            return action.name == name;
        });
        if (found == actions.end())
        {
            return std::nullopt;
        }
        return static_cast<std::size_t>(found - actions.begin());
    }

    std::vector<std::size_t> filteredActionIndices() const
    {
        std::vector<std::size_t> matches;
        if (!openActionDropdownIndex || *openActionDropdownIndex >= actionRefFields.size())
        {
            return matches;
        }

        const std::string query = lowerKey(trim(actionRefFields[*openActionDropdownIndex].getValue()));
        for (std::size_t i = 0; i < actions.size(); ++i)
        {
            if (!actionDropdownFiltering || query.empty() || lowerKey(actions[i].name).find(query) != std::string::npos)
            {
                matches.push_back(i);
            }
        }
        return matches;
    }

    std::optional<ActionDropdownLayout> actionDropdownLayout() const
    {
        if (!openActionDropdownIndex || *openActionDropdownIndex >= actionRefFields.size())
        {
            return std::nullopt;
        }
        const sf::FloatRect fieldBounds = actionRefFields[*openActionDropdownIndex].bounds();
        if (!isVisibleInArrayViewport(fieldBounds))
        {
            return std::nullopt;
        }

        const std::vector<std::size_t> matches = filteredActionIndices();
        const std::size_t desiredRows = std::max<std::size_t>(
            1,
            std::min<std::size_t>(VisibleActionDropdownRows, matches.size()));
        const float belowSpace = std::max(
            0.0f,
            ArrayViewportBottom - (fieldBounds.position.y + fieldBounds.size.y + 2.0f));
        const float aboveSpace = std::max(
            0.0f,
            fieldBounds.position.y - ArrayViewportTop - 2.0f);
        const std::size_t belowRows = static_cast<std::size_t>(belowSpace / ActionDropdownRowHeight);
        const std::size_t aboveRows = static_cast<std::size_t>(aboveSpace / ActionDropdownRowHeight);
        const bool placeBelow = belowRows >= desiredRows || belowRows >= aboveRows;
        const std::size_t availableRows = placeBelow ? belowRows : aboveRows;
        if (availableRows == 0)
        {
            return std::nullopt;
        }

        ActionDropdownLayout layout;
        layout.visibleRows = std::min(desiredRows, availableRows);
        const std::size_t maximumOffset = matches.size() > layout.visibleRows
            ? matches.size() - layout.visibleRows
            : 0;
        layout.firstRow = std::min(actionDropdownOffset, maximumOffset);
        const float height = static_cast<float>(layout.visibleRows) * ActionDropdownRowHeight;
        const float y = placeBelow
            ? fieldBounds.position.y + fieldBounds.size.y + 2.0f
            : fieldBounds.position.y - height - 2.0f;
        layout.bounds = sf::FloatRect({fieldBounds.position.x, y}, {fieldBounds.size.x, height});
        return layout;
    }

    void closeActionDropdown()
    {
        openActionDropdownIndex.reset();
        actionDropdownOffset = 0;
        actionDropdownSelection = 0;
        actionDropdownFiltering = false;
        hoveredActionDropdownItem.reset();
    }

    void ensureActionDropdownSelectionVisible()
    {
        const std::vector<std::size_t> matches = filteredActionIndices();
        if (matches.empty())
        {
            actionDropdownSelection = 0;
            actionDropdownOffset = 0;
            return;
        }
        actionDropdownSelection = std::min(actionDropdownSelection, matches.size() - 1);
        const std::optional<ActionDropdownLayout> layout = actionDropdownLayout();
        if (!layout)
        {
            return;
        }
        if (actionDropdownSelection < actionDropdownOffset)
        {
            actionDropdownOffset = actionDropdownSelection;
        }
        else if (actionDropdownSelection >= actionDropdownOffset + layout->visibleRows)
        {
            actionDropdownOffset = actionDropdownSelection - layout->visibleRows + 1;
        }
    }

    void openActionDropdown(std::size_t referenceIndex, bool filterByCurrentValue)
    {
        if (referenceIndex >= actionRefFields.size())
        {
            closeActionDropdown();
            return;
        }
        openActionDropdownIndex = referenceIndex;
        actionDropdownFiltering = filterByCurrentValue;
        actionDropdownOffset = 0;
        actionDropdownSelection = 0;
        if (!filterByCurrentValue)
        {
            if (const std::optional<std::size_t> linkedIndex = linkedActionIndex(referenceIndex))
            {
                const std::vector<std::size_t> matches = filteredActionIndices();
                const auto found = std::find(matches.begin(), matches.end(), *linkedIndex);
                if (found != matches.end())
                {
                    actionDropdownSelection = static_cast<std::size_t>(found - matches.begin());
                }
            }
        }
        ensureActionDropdownSelectionVisible();
    }

    void moveActionDropdownSelection(int delta)
    {
        const std::vector<std::size_t> matches = filteredActionIndices();
        if (matches.empty())
        {
            return;
        }
        const int next = std::clamp(
            static_cast<int>(actionDropdownSelection) + delta,
            0,
            static_cast<int>(matches.size()) - 1);
        actionDropdownSelection = static_cast<std::size_t>(next);
        ensureActionDropdownSelectionVisible();
    }

    void chooseActionDropdownItem(std::size_t filteredIndex)
    {
        const std::vector<std::size_t> matches = filteredActionIndices();
        if (!openActionDropdownIndex || *openActionDropdownIndex >= actionRefFields.size() ||
            filteredIndex >= matches.size())
        {
            return;
        }
        const std::size_t referenceIndex = *openActionDropdownIndex;
        const std::string& actionName = actions[matches[filteredIndex]].name;
        actionRefFields[referenceIndex].setValue(actionName);
        if (referenceIndex < actionDisplayNameFields.size() &&
            trim(actionDisplayNameFields[referenceIndex].getValue()).empty())
        {
            actionDisplayNameFields[referenceIndex].setValue(actionName);
        }
        closeActionDropdown();
    }

    std::optional<std::size_t> actionDropdownItemAt(sf::Vector2f mouse) const
    {
        const std::optional<ActionDropdownLayout> layout = actionDropdownLayout();
        if (!layout || !layout->bounds.contains(mouse))
        {
            return std::nullopt;
        }
        const std::vector<std::size_t> matches = filteredActionIndices();
        if (matches.empty())
        {
            return std::nullopt;
        }
        const std::size_t row = static_cast<std::size_t>(
            (mouse.y - layout->bounds.position.y) / ActionDropdownRowHeight);
        const std::size_t filteredIndex = layout->firstRow + row;
        if (row >= layout->visibleRows || filteredIndex >= matches.size())
        {
            return std::nullopt;
        }
        return filteredIndex;
    }

    sf::FloatRect actionLinkButtonBounds(std::size_t referenceIndex) const
    {
        const sf::FloatRect fieldBounds = actionRefFields[referenceIndex].bounds();
        return sf::FloatRect(
            {fieldBounds.position.x + fieldBounds.size.x + 6.0f, fieldBounds.position.y + 2.0f},
            {32.0f, 28.0f});
    }

    static void appendSnapshotValue(std::string& snapshot, const std::string& value)
    {
        snapshot += std::to_string(value.size());
        snapshot += ':';
        snapshot += value;
        snapshot += ';';
    }

    std::string cardFormSnapshot() const
    {
        std::string snapshot;
        auto appendFields = [&](const std::vector<InputBox>& fields) {
            appendSnapshotValue(snapshot, std::to_string(fields.size()));
            for (const InputBox& field : fields)
            {
                appendSnapshotValue(snapshot, field.getValue());
            }
        };

        appendSnapshotValue(snapshot, titleField.getValue());
        appendSnapshotValue(snapshot, imageField.getValue());
        appendSnapshotValue(snapshot, typeField.getValue());
        appendFields(traitFields);
        appendFields(keywordFields);
        appendFields(intKeyFields);
        appendFields(intValueFields);
        appendFields(stringKeyFields);
        appendFields(stringValueFields);
        appendSnapshotValue(snapshot, std::to_string(listEditors.size()));
        for (const StringListEditor& editor : listEditors)
        {
            appendSnapshotValue(snapshot, editor.keyField.getValue());
            appendFields(editor.valueFields);
        }
        appendFields(actionDisplayNameFields);
        appendFields(actionRefFields);
        return snapshot;
    }

    std::string actionFormSnapshot() const
    {
        std::string snapshot;
        const std::vector<const InputBox*> fields = {
            &actionNameField,
            &actionStateField,
            &actionNextStateField,
            &actionKindField,
            &actionPatternField,
            &actionMinRangeField,
            &actionMaxRangeField,
            &actionDamageField,
            &actionHealField,
            &actionCanMoveField,
            &actionCanAttackField,
            &actionPassThroughField,
            &actionLineOfSightField,
            &actionStatusTurnsField,
            &actionCooldownTurnsField,
            &actionPushField,
        };
        for (const InputBox* field : fields)
        {
            appendSnapshotValue(snapshot, field->getValue());
        }
        appendSnapshotValue(snapshot, std::to_string(actionTargetFilterFields.size()));
        for (const InputBox& field : actionTargetFilterFields)
        {
            appendSnapshotValue(snapshot, field.getValue());
        }
        return snapshot;
    }

    void rememberCardForm()
    {
        savedCardForm = cardFormSnapshot();
    }

    void rememberActionForm()
    {
        savedActionForm = actionFormSnapshot();
    }

    bool hasUnsavedCardChanges() const
    {
        return cardFormSnapshot() != savedCardForm;
    }

    bool hasUnsavedChanges() const
    {
        return editorMode == EditorMode::Cards
            ? hasUnsavedCardChanges()
            : actionFormSnapshot() != savedActionForm;
    }

    void restoreCurrentForm()
    {
        if (editorMode == EditorMode::Cards)
        {
            if (selectedCard && *selectedCard < cards.size())
            {
                selectCard(*selectedCard);
            }
            else
            {
                createNewCard();
            }
        }
        else if (selectedAction && *selectedAction < actions.size())
        {
            selectAction(*selectedAction);
        }
        else
        {
            createNewAction();
        }
    }

    void setEditorMode(EditorMode mode)
    {
        closeActionDropdown();
        editorMode = mode;
        editorTabs.setActive(static_cast<std::size_t>(editorMode));
        rebuildFocusOrder();
        if (editorMode == EditorMode::Cards)
        {
            activateField(&titleField);
            setStatus(fmt::format("{} cards", cards.size()), Muted);
        }
        else
        {
            activateField(&actionNameField);
            setStatus(fmt::format("{} actions", actions.size()), Muted);
        }
    }

    bool executePendingTransition()
    {
        const PendingTransition transition = pendingTransition;
        const std::string selectionName = pendingSelectionName;
        pendingTransition = PendingTransition::None;
        pendingSelectionName.clear();

        switch (transition)
        {
        case PendingTransition::CloseScreen:
            return true;
        case PendingTransition::ShowCards:
            setEditorMode(EditorMode::Cards);
            break;
        case PendingTransition::ShowActions:
            setEditorMode(EditorMode::Actions);
            break;
        case PendingTransition::NewCard:
            createNewCard();
            break;
        case PendingTransition::NewAction:
            createNewAction();
            break;
        case PendingTransition::RefreshCards:
            loadCards();
            break;
        case PendingTransition::RefreshActions:
            loadActions();
            break;
        case PendingTransition::SelectCard:
        {
            const auto found = std::find_if(cards.begin(), cards.end(), [&](const card_data::Card& card) {
                return card.title == selectionName;
            });
            if (found != cards.end())
            {
                selectCard(static_cast<std::size_t>(found - cards.begin()));
            }
            else
            {
                setStatus("That card is no longer available", Warn);
            }
            break;
        }
        case PendingTransition::SelectAction:
        {
            const auto found = std::find_if(actions.begin(), actions.end(), [&](const card_data::Action& action) {
                return action.name == selectionName;
            });
            if (found != actions.end())
            {
                setEditorMode(EditorMode::Actions);
                selectAction(static_cast<std::size_t>(found - actions.begin()));
            }
            else
            {
                setStatus("That action is no longer available", Warn);
            }
            break;
        }
        case PendingTransition::None:
            break;
        }
        return false;
    }

    bool requestTransition(PendingTransition transition, std::string selectionName = {})
    {
        pendingTransition = transition;
        pendingSelectionName = std::move(selectionName);
        if (!hasUnsavedChanges())
        {
            return executePendingTransition();
        }

        unsavedChangesPopupVisible = true;
        unsavedChangesError.clear();
        for (InputBox* field : focusOrder)
        {
            field->setActive(false);
        }
        return false;
    }

    bool saveAndContinue()
    {
        const bool saved = editorMode == EditorMode::Cards
            ? saveCurrentCard()
            : saveCurrentAction();
        if (!saved)
        {
            unsavedChangesError = status;
            return false;
        }
        unsavedChangesPopupVisible = false;
        unsavedChangesError.clear();
        return executePendingTransition();
    }

    bool discardAndContinue()
    {
        restoreCurrentForm();
        unsavedChangesPopupVisible = false;
        unsavedChangesError.clear();
        return executePendingTransition();
    }

    void keepEditing()
    {
        unsavedChangesPopupVisible = false;
        pendingTransition = PendingTransition::None;
        pendingSelectionName.clear();
        unsavedChangesError.clear();
        if (!focusOrder.empty())
        {
            activateField(focusOrder[std::min(focusIndex, focusOrder.size() - 1)]);
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

    void loadActions()
    {
        const ActionListResult result = fetchActionsFromServer();
        if (!result.success)
        {
            actions.clear();
            selectedAction.reset();
            actionListOffset = 0;
            setStatus(result.message, Warn);
            return;
        }
        actions = result.actions;
        actionListOffset = 0;
        if (!actions.empty())
        {
            selectAction(0);
        }
        else
        {
            createNewAction();
        }
        if (editorMode == EditorMode::Actions)
        {
            setStatus(fmt::format("Loaded {} action{}", actions.size(), actions.size() == 1 ? "" : "s"), Muted);
        }
    }

    static int formInt(const InputBox& field, int fallback)
    {
        try
        {
            return std::stoi(trim(field.getValue()));
        }
        catch (...)
        {
            return fallback;
        }
    }

    static bool formBool(const InputBox& field)
    {
        const std::string value = lowerKey(trim(field.getValue()));
        return value == "1" || value == "true" || value == "yes";
    }

    card_data::Action actionFromForm() const
    {
        card_data::Action action;
        action.name = trim(actionNameField.getValue());
        action.state = formInt(actionStateField, 0);
        action.nextState = formInt(actionNextStateField, action.state);
        action.kind = lowerKey(trim(actionKindField.getValue()));
        action.pattern = lowerKey(trim(actionPatternField.getValue()));
        action.minRange = formInt(actionMinRangeField, 1);
        action.maxRange = formInt(actionMaxRangeField, 1);
        action.damage = std::max(0, formInt(actionDamageField, 0));
        action.heal = std::max(0, formInt(actionHealField, 0));
        action.canMove = formBool(actionCanMoveField);
        action.canAttack = formBool(actionCanAttackField);
        action.passThrough = formBool(actionPassThroughField);
        action.lineOfSight = formBool(actionLineOfSightField);
        action.statusTurns = formInt(actionStatusTurnsField, 0);
        action.cooldownTurns = formInt(actionCooldownTurnsField, 0);
        action.push = std::max(0, formInt(actionPushField, 0));
        for (const InputBox& field : actionTargetFilterFields)
        {
            const std::string value = trim(field.getValue());
            if (!value.empty())
            {
                action.targetFilter.push_back(value);
            }
        }
        return action;
    }

    void createNewAction()
    {
        closeActionDropdown();
        selectedAction.reset();
        actionNameField.setValue("");
        actionStateField.setValue("0");
        actionNextStateField.setValue("0");
        actionKindField.setValue("slide");
        actionPatternField.setValue("omni");
        actionMinRangeField.setValue("1");
        actionMaxRangeField.setValue("1");
        actionDamageField.setValue("0");
        actionHealField.setValue("0");
        actionCanMoveField.setValue("1");
        actionCanAttackField.setValue("0");
        actionPassThroughField.setValue("0");
        actionLineOfSightField.setValue("0");
        actionStatusTurnsField.setValue("0");
        actionCooldownTurnsField.setValue("0");
        actionPushField.setValue("0");
        actionTargetFilterFields.clear();
        actionTargetFilterOffset = 0;
        removeActionTargetFilterButtons.clear();
        rebuildFocusOrder();
        activateField(&actionNameField);
        rememberActionForm();
        setStatus("Draft action", Muted);
    }

    void selectAction(std::size_t index)
    {
        if (index >= actions.size())
        {
            return;
        }
        closeActionDropdown();
        selectedAction = index;
        ensureActionVisible(index);
        const card_data::Action& action = actions[index];
        actionNameField.setValue(action.name);
        actionStateField.setValue(std::to_string(action.state));
        actionNextStateField.setValue(std::to_string(card_data::actionNextState(action)));
        actionKindField.setValue(action.kind);
        actionPatternField.setValue(action.pattern);
        actionMinRangeField.setValue(std::to_string(action.minRange));
        actionMaxRangeField.setValue(std::to_string(action.maxRange));
        actionDamageField.setValue(std::to_string(action.damage));
        actionHealField.setValue(std::to_string(action.heal));
        actionCanMoveField.setValue(action.canMove ? "1" : "0");
        actionCanAttackField.setValue(action.canAttack ? "1" : "0");
        actionPassThroughField.setValue(action.passThrough ? "1" : "0");
        actionLineOfSightField.setValue(action.lineOfSight ? "1" : "0");
        actionStatusTurnsField.setValue(std::to_string(action.statusTurns));
        actionCooldownTurnsField.setValue(std::to_string(action.cooldownTurns));
        actionPushField.setValue(std::to_string(action.push));
        actionTargetFilterFields.clear();
        for (const std::string& value : action.targetFilter)
        {
            actionTargetFilterFields.push_back(makeCompactField(value, {294.0f, 32.0f}));
        }
        actionTargetFilterOffset = 0;
        rebuildFocusOrder();
        activateField(&actionNameField);
        rememberActionForm();
    }

    std::optional<std::string> selectedActionName() const
    {
        if (!selectedAction || *selectedAction >= actions.size())
        {
            return std::nullopt;
        }
        return actions[*selectedAction].name;
    }

    bool saveCurrentAction()
    {
        const card_data::Action action = actionFromForm();
        if (action.name.empty())
        {
            setStatus("Action name is required before saving", Warn);
            return false;
        }
        const std::optional<std::string> originalName = selectedActionName();
        const CommandResult result = originalName
            ? updateActionOnServer(*originalName, action)
            : saveActionToServer(action);
        if (!result.success)
        {
            setStatus(result.message, Warn);
            return false;
        }
        rememberActionForm();
        const ActionListResult listResult = fetchActionsFromServer();
        if (listResult.success)
        {
            actions = listResult.actions;
            const auto found = std::find_if(actions.begin(), actions.end(), [&](const card_data::Action& item) {
                return item.name == action.name;
            });
            if (found != actions.end())
            {
                selectAction(static_cast<std::size_t>(found - actions.begin()));
            }
        }
        loadCards();
        setStatus("Saved action", Accent);
        return true;
    }

    void copyCurrentAction()
    {
        card_data::Action action = actionFromForm();
        if (action.name.empty())
        {
            setStatus("Action name is required before copying", Warn);
            return;
        }

        const ActionListResult currentActions = fetchActionsFromServer();
        if (!currentActions.success)
        {
            setStatus(currentActions.message, Warn);
            return;
        }

        std::set<std::string> existingNames;
        for (const card_data::Action& existingAction : currentActions.actions)
        {
            existingNames.insert(existingAction.name);
        }
        action.name = uniqueCopyName(action.name, existingNames);

        const CommandResult result = saveActionToServer(action);
        if (!result.success)
        {
            setStatus(result.message, Warn);
            return;
        }

        const ActionListResult listResult = fetchActionsFromServer();
        if (!listResult.success)
        {
            setStatus(listResult.message, Warn);
            return;
        }

        actions = listResult.actions;
        const auto found = std::find_if(actions.begin(), actions.end(), [&](const card_data::Action& item) {
            return item.name == action.name;
        });
        if (found != actions.end())
        {
            selectAction(static_cast<std::size_t>(found - actions.begin()));
        }
        setStatus(fmt::format("Copied action as {}", action.name), Accent);
    }

    void deleteCurrentAction()
    {
        const std::optional<std::string> name = selectedActionName();
        if (!name)
        {
            setStatus("Select a saved action before deleting", Warn);
            return;
        }
        const CommandResult result = deleteActionFromServer(*name);
        if (!result.success)
        {
            setStatus(result.message, Warn);
            return;
        }
        const std::size_t deletedIndex = *selectedAction;
        const ActionListResult listResult = fetchActionsFromServer();
        if (!listResult.success)
        {
            setStatus(listResult.message, Warn);
            return;
        }
        actions = listResult.actions;
        if (actions.empty())
        {
            createNewAction();
        }
        else
        {
            selectAction(std::min(deletedIndex, actions.size() - 1));
        }
        setStatus("Deleted action", Accent);
    }

    void createNewCard()
    {
        closeActionDropdown();
        selectedCard.reset();
        titleField.setValue("");
        imageField.setValue("");
        typeField.setValue("Unit");
        traitFields.clear();
        keywordFields.clear();
        intKeyFields.clear();
        intValueFields.clear();
        stringKeyFields.clear();
        stringValueFields.clear();
        listEditors.clear();
        actionDisplayNameFields.clear();
        actionRefFields.clear();
        editorScroll = 0.0f;
        rebuildFocusOrder();
        activateField(&titleField);
        hasPreviewImage = false;
        rememberCardForm();
        setStatus("Draft card", Muted);
    }

    void selectCard(std::size_t index)
    {
        if (index >= cards.size())
        {
            return;
        }

        closeActionDropdown();

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
        rememberCardForm();
    }

    card_data::Card cardFromForm() const
    {
        card_data::Card card;
        card.title = trim(titleField.getValue());
        card.type = trim(typeField.getValue());
        card.imagePath = assetRelativeImagePath(imageField.getValue());
        for (const InputBox& field : traitFields)
        {
            const std::string value = trim(field.getValue());
            if (!value.empty())
            {
                card.traits.push_back(value);
            }
        }
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
            if (item.key.empty() || lowerKey(item.key) == "actions")
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
        for (std::size_t i = 0; i < actionRefFields.size(); ++i)
        {
            const std::string actionName = trim(actionRefFields[i].getValue());
            if (!actionName.empty())
            {
                card.actionNames.push_back(actionName);
                const std::string displayName = i < actionDisplayNameFields.size()
                    ? trim(actionDisplayNameFields[i].getValue())
                    : std::string();
                card.actionDisplayNames.push_back(displayName.empty() ? actionName : displayName);
            }
        }
        return card;
    }

    bool saveCurrentCard()
    {
        card_data::Card card = cardFromForm();
        if (card.title.empty())
        {
            setStatus("Title is required before saving", Warn);
            return false;
        }
        if (!card.imagePath.empty() && !resolveAssetImagePath(card.imagePath))
        {
            setStatus("Image path must stay inside assets", Warn);
            return false;
        }
        imageField.setValue(card.imagePath);

        const std::optional<std::string> originalTitle = selectedCardTitle();
        const CommandResult result = originalTitle ? updateCardOnServer(*originalTitle, card) : saveCardToServer(card);
        if (!result.success)
        {
            setStatus(result.message, Warn);
            return false;
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
        rememberCardForm();
        return true;
    }

    void copyCurrentCard()
    {
        card_data::Card card = cardFromForm();
        if (card.title.empty())
        {
            setStatus("Title is required before copying", Warn);
            return;
        }
        if (!card.imagePath.empty() && !resolveAssetImagePath(card.imagePath))
        {
            setStatus("Image path must stay inside assets", Warn);
            return;
        }

        const CardListResult currentCards = fetchCardsFromServer();
        if (!currentCards.success)
        {
            setStatus(currentCards.message, Warn);
            return;
        }

        std::set<std::string> existingTitles;
        for (const card_data::Card& existingCard : currentCards.cards)
        {
            existingTitles.insert(existingCard.title);
        }
        card.title = uniqueCopyName(card.title, existingTitles);
        imageField.setValue(card.imagePath);

        const CommandResult result = saveCardToServer(card);
        if (!result.success)
        {
            setStatus(result.message, Warn);
            return;
        }

        const CardListResult listResult = fetchCardsFromServer();
        if (!listResult.success)
        {
            setStatus(listResult.message, Warn);
            return;
        }

        cards = listResult.cards;
        const auto found = std::find_if(cards.begin(), cards.end(), [&](const card_data::Card& item) {
            return item.title == card.title;
        });
        if (found != cards.end())
        {
            selectCard(static_cast<std::size_t>(found - cards.begin()));
        }
        setStatus(fmt::format("Copied card as {}", card.title), Accent);
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

    void ensureActionVisible(std::size_t index)
    {
        if (index < actionListOffset)
        {
            actionListOffset = index;
        }
        else if (index >= actionListOffset + VisibleActionRows)
        {
            actionListOffset = index - VisibleActionRows + 1;
        }
    }

    void scrollActionList(int rows)
    {
        if (actions.size() <= VisibleActionRows)
        {
            actionListOffset = 0;
            return;
        }
        const int maxOffset = static_cast<int>(actions.size() - VisibleActionRows);
        actionListOffset = static_cast<std::size_t>(
            std::clamp(static_cast<int>(actionListOffset) + rows, 0, maxOffset));
    }

    bool isInActionTargetFilterViewport(sf::Vector2f point) const
    {
        return point.x >= 864.0f && point.x <= 1232.0f &&
            point.y >= TargetFilterTop &&
            point.y <= TargetFilterTop + TargetFilterRowHeight * VisibleTargetFilterRows;
    }

    bool isActionTargetFilterVisible(std::size_t index) const
    {
        return index >= actionTargetFilterOffset &&
            index < actionTargetFilterOffset + VisibleTargetFilterRows;
    }

    void clampActionTargetFilterOffset()
    {
        const std::size_t maxOffset = actionTargetFilterFields.size() > VisibleTargetFilterRows
            ? actionTargetFilterFields.size() - VisibleTargetFilterRows
            : 0;
        actionTargetFilterOffset = std::min(actionTargetFilterOffset, maxOffset);
    }

    void scrollActionTargetFilters(int rows)
    {
        clampActionTargetFilterOffset();
        const int maxOffset = static_cast<int>(
            actionTargetFilterFields.size() > VisibleTargetFilterRows
                ? actionTargetFilterFields.size() - VisibleTargetFilterRows
                : 0);
        actionTargetFilterOffset = static_cast<std::size_t>(std::clamp(
            static_cast<int>(actionTargetFilterOffset) + rows, 0, maxOffset));
        layoutActionTargetFilterControls();
    }

    void ensureActionTargetFilterVisible(std::size_t index)
    {
        if (index < actionTargetFilterOffset)
        {
            actionTargetFilterOffset = index;
        }
        else if (index >= actionTargetFilterOffset + VisibleTargetFilterRows)
        {
            actionTargetFilterOffset = index - VisibleTargetFilterRows + 1;
        }
        clampActionTargetFilterOffset();
        layoutActionTargetFilterControls();
    }

    std::optional<std::size_t> actionTargetFilterIndex(const InputBox* field) const
    {
        for (std::size_t i = 0; i < actionTargetFilterFields.size(); ++i)
        {
            if (&actionTargetFilterFields[i] == field)
            {
                return i;
            }
        }
        return std::nullopt;
    }

    void layoutActionTargetFilterControls()
    {
        clampActionTargetFilterOffset();
        removeActionTargetFilterButtons.clear();
        for (std::size_t i = 0; i < actionTargetFilterFields.size(); ++i)
        {
            const float y = TargetFilterTop +
                (static_cast<float>(i) - static_cast<float>(actionTargetFilterOffset)) *
                    TargetFilterRowHeight;
            actionTargetFilterFields[i].setPosition({870.0f, y});
            if (isActionTargetFilterVisible(i))
            {
                removeActionTargetFilterButtons.emplace_back(
                    font, "x", sf::Vector2f{1184.0f, y + 2.0f}, sf::Vector2f{32.0f, 28.0f}, Warn);
            }
        }
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
        removeTraitButtons.clear();
        removeKeywordButtons.clear();
        removeIntegerButtons.clear();
        removeStringButtons.clear();
        removeListButtons.clear();
        addListValueButtons.clear();
        removeListValueButtons.clear();
        removeActionRefButtons.clear();

        float y = ArrayViewportTop - editorScroll;
        auto addSection = [&](const std::string& heading, EditorButton& addButton) {
            arraySectionLabels.push_back({heading, {340.0f, y}});
            addButton.setPosition({778.0f, y + 2.0f});
            y += 30.0f;
        };

        addSection("Traits", addTraitButton);
        if (traitFields.empty())
        {
            arraySectionLabels.push_back({"No traits", {354.0f, y + 6.0f}});
            y += 34.0f;
        }
        for (InputBox& field : traitFields)
        {
            field.setPosition({340.0f, y});
            EditorButton button = makeMiniButton("X", Warn);
            button.setPosition({778.0f, y + 2.0f});
            removeTraitButtons.push_back(std::move(button));
            y += 40.0f;
        }
        y += 12.0f;

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
        y += 12.0f;

        addSection("Actions", addActionRefButton);
        if (actionRefFields.empty())
        {
            arraySectionLabels.push_back({"No action references", {354.0f, y + 6.0f}});
            y += 34.0f;
        }
        else
        {
            arraySectionLabels.push_back({"Action name", {340.0f, y}});
            arraySectionLabels.push_back({"Reusable action", {526.0f, y}});
            y += 22.0f;
        }
        for (std::size_t i = 0; i < actionRefFields.size(); ++i)
        {
            if (i < actionDisplayNameFields.size())
            {
                actionDisplayNameFields[i].setPosition({340.0f, y});
            }
            actionRefFields[i].setPosition({526.0f, y});
            EditorButton button = makeMiniButton("X", Warn);
            button.setPosition({778.0f, y + 2.0f});
            removeActionRefButtons.push_back(std::move(button));
            y += 40.0f;
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

    void addTrait()
    {
        traitFields.push_back(makeCompactField("", {392.0f, 32.0f}));
        rebuildFocusOrder();
        activateField(&traitFields.back());
        ensureActiveFieldVisible();
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

    void addActionReference()
    {
        actionDisplayNameFields.push_back(makeActionDisplayNameField(""));
        actionRefFields.push_back(makeActionReferenceField(""));
        rebuildFocusOrder();
        activateField(&actionRefFields.back());
        ensureActiveFieldVisible();
        openActionDropdown(actionRefFields.size() - 1, false);
    }

    void addActionTargetFilter()
    {
        actionTargetFilterFields.push_back(makeCompactField("", {294.0f, 32.0f}));
        rebuildFocusOrder();
        activateField(&actionTargetFilterFields.back());
        ensureActionTargetFilterVisible(actionTargetFilterFields.size() - 1);
    }

    void removeTrait(std::size_t index)
    {
        if (index < traitFields.size())
        {
            traitFields.erase(traitFields.begin() + static_cast<std::ptrdiff_t>(index));
            rebuildFocusOrder();
            activateField(&titleField);
        }
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

    void removeActionReference(std::size_t index)
    {
        if (index < actionRefFields.size())
        {
            closeActionDropdown();
            if (index < actionDisplayNameFields.size())
            {
                actionDisplayNameFields.erase(
                    actionDisplayNameFields.begin() + static_cast<std::ptrdiff_t>(index));
            }
            actionRefFields.erase(actionRefFields.begin() + static_cast<std::ptrdiff_t>(index));
            rebuildFocusOrder();
            activateField(&titleField);
        }
    }

    void removeActionTargetFilter(std::size_t index)
    {
        if (index < actionTargetFilterFields.size())
        {
            actionTargetFilterFields.erase(
                actionTargetFilterFields.begin() + static_cast<std::ptrdiff_t>(index));
            clampActionTargetFilterOffset();
            rebuildFocusOrder();
            activateField(&actionNameField);
            layoutActionTargetFilterControls();
        }
    }

    void ensureActiveFieldVisible()
    {
        if (focusOrder.empty())
        {
            return;
        }

        if (editorMode == EditorMode::Actions)
        {
            if (const std::optional<std::size_t> filterIndex =
                    actionTargetFilterIndex(focusOrder[focusIndex]))
            {
                ensureActionTargetFilterVisible(*filterIndex);
            }
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
        for (InputBox& field : traitFields)
        {
            if (isVisibleInArrayViewport(field.bounds()) && field.contains(mouse))
            {
                return &field;
            }
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
        for (InputBox& field : actionDisplayNameFields)
        {
            if (isVisibleInArrayViewport(field.bounds()) && field.contains(mouse))
            {
                return &field;
            }
        }
        for (InputBox& field : actionRefFields)
        {
            if (isVisibleInArrayViewport(field.bounds()) && field.contains(mouse))
            {
                return &field;
            }
        }
        return nullptr;
    }

    void moveFocus(int delta)
    {
        closeActionDropdown();
        focusOrder[focusIndex]->setActive(false);
        focusIndex = static_cast<std::size_t>((static_cast<int>(focusIndex) + delta + static_cast<int>(focusOrder.size())) % static_cast<int>(focusOrder.size()));
        focusOrder[focusIndex]->setActive(true);
        if (editorMode == EditorMode::Cards)
        {
            if (const std::optional<std::size_t> referenceIndex = actionReferenceIndex(focusOrder[focusIndex]))
            {
                openActionDropdown(*referenceIndex, false);
            }
        }
    }

    bool handleClick(sf::Vector2f mouse)
    {
        if (editorMode == EditorMode::Cards)
        {
            layoutArrayControls();
            if (const std::optional<std::size_t> dropdownItem = actionDropdownItemAt(mouse))
            {
                chooseActionDropdownItem(*dropdownItem);
                return false;
            }
            if (const std::optional<ActionDropdownLayout> layout = actionDropdownLayout();
                layout && layout->bounds.contains(mouse))
            {
                return false;
            }
            for (std::size_t i = 0; i < actionRefFields.size(); ++i)
            {
                if (!isVisibleInArrayViewport(actionRefFields[i].bounds()) || !linkedActionIndex(i) ||
                    !actionLinkButtonBounds(i).contains(mouse))
                {
                    continue;
                }
                const std::string actionName = actions[*linkedActionIndex(i)].name;
                closeActionDropdown();
                return requestTransition(PendingTransition::SelectAction, actionName);
            }
        }

        if (instructionsButton.contains(mouse))
        {
            instructionsVisible = true;
            instructionScroll = 0.0f;
            for (InputBox* field : focusOrder)
            {
                field->setActive(false);
            }
            return false;
        }
        if (backButton.contains(mouse))
        {
            return requestTransition(PendingTransition::CloseScreen);
        }
        if (const std::optional<std::size_t> tabIndex = editorTabs.clickedIndex(mouse))
        {
            const EditorMode requestedMode = *tabIndex == 0 ? EditorMode::Cards : EditorMode::Actions;
            if (requestedMode == editorMode)
            {
                return false;
            }
            closeActionDropdown();
            return requestTransition(requestedMode == EditorMode::Cards
                ? PendingTransition::ShowCards
                : PendingTransition::ShowActions);
        }
        if (newButton.contains(mouse))
        {
            return requestTransition(editorMode == EditorMode::Cards
                ? PendingTransition::NewCard
                : PendingTransition::NewAction);
        }
        if (refreshButton.contains(mouse))
        {
            return requestTransition(editorMode == EditorMode::Cards
                ? PendingTransition::RefreshCards
                : PendingTransition::RefreshActions);
        }
        if ((editorMode == EditorMode::Cards && copyButton.contains(mouse)) ||
            (editorMode == EditorMode::Actions && copyActionButton.contains(mouse)))
        {
            if (editorMode == EditorMode::Cards)
            {
                copyCurrentCard();
            }
            else
            {
                copyCurrentAction();
            }
            return false;
        }
        if ((editorMode == EditorMode::Cards && saveButton.contains(mouse)) ||
            (editorMode == EditorMode::Actions && saveActionButton.contains(mouse)))
        {
            if (editorMode == EditorMode::Cards)
            {
                saveCurrentCard();
            }
            else
            {
                saveCurrentAction();
            }
            return false;
        }
        if (deleteButton.contains(mouse))
        {
            if (editorMode == EditorMode::Cards)
            {
                deleteCurrentCard();
            }
            else
            {
                deleteCurrentAction();
            }
            return false;
        }
        if (editorMode == EditorMode::Actions)
        {
            layoutActionFields();
            if (addActionTargetFilterButton.contains(mouse))
            {
                addActionTargetFilter();
                return false;
            }
            for (std::size_t i = 0; i < removeActionTargetFilterButtons.size(); ++i)
            {
                if (removeActionTargetFilterButtons[i].contains(mouse))
                {
                    removeActionTargetFilter(actionTargetFilterOffset + i);
                    return false;
                }
            }
            const std::optional<std::size_t> actionIndex = actionIndexAt(mouse);
            if (actionIndex)
            {
                if (selectedAction && *selectedAction == *actionIndex)
                {
                    return false;
                }
                return requestTransition(PendingTransition::SelectAction, actions[*actionIndex].name);
            }
            for (InputBox* field : focusOrder)
            {
                if (const std::optional<std::size_t> filterIndex = actionTargetFilterIndex(field);
                    filterIndex && !isActionTargetFilterVisible(*filterIndex))
                {
                    continue;
                }
                if (field->contains(mouse))
                {
                    activateField(field);
                    field->beginMouseSelection(mouse, sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                                       sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift));
                    return false;
                }
            }
            return false;
        }
        if (isInArrayViewport(mouse))
        {
            if (isVisibleInArrayViewport(addTraitButton.bounds()) && addTraitButton.contains(mouse))
            {
                addTrait();
                return false;
            }
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
            if (isVisibleInArrayViewport(addActionRefButton.bounds()) && addActionRefButton.contains(mouse))
            {
                addActionReference();
                return false;
            }
            for (std::size_t i = 0; i < removeTraitButtons.size(); ++i)
            {
                if (isVisibleInArrayViewport(removeTraitButtons[i].bounds()) && removeTraitButtons[i].contains(mouse))
                {
                    removeTrait(i);
                    return false;
                }
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
            for (std::size_t i = 0; i < removeActionRefButtons.size(); ++i)
            {
                if (isVisibleInArrayViewport(removeActionRefButtons[i].bounds()) && removeActionRefButtons[i].contains(mouse))
                {
                    removeActionReference(i);
                    return false;
                }
            }
            if (InputBox* field = dynamicFieldAt(mouse))
            {
                activateField(field);
                field->beginMouseSelection(mouse, sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                                   sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift));
                if (const std::optional<std::size_t> referenceIndex = actionReferenceIndex(field))
                {
                    openActionDropdown(*referenceIndex, false);
                }
                else
                {
                    closeActionDropdown();
                }
                return false;
            }
        }

        const std::optional<std::size_t> listIndex = cardIndexAt(mouse);
        if (listIndex)
        {
            if (selectedCard && *selectedCard == *listIndex)
            {
                return false;
            }
            return requestTransition(PendingTransition::SelectCard, cards[*listIndex].title);
        }
        if (titleField.contains(mouse))
        {
            closeActionDropdown();
            activateField(&titleField);
            titleField.beginMouseSelection(mouse, sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                                  sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift));
            return false;
        }
        if (imageField.contains(mouse))
        {
            closeActionDropdown();
            activateField(&imageField);
            imageField.beginMouseSelection(mouse, sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                                  sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift));
            return false;
        }
        if (typeField.contains(mouse))
        {
            closeActionDropdown();
            activateField(&typeField);
            typeField.beginMouseSelection(mouse, sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                                 sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift));
            return false;
        }
        for (InputBox* field : focusOrder)
        {
            field->setActive(false);
        }
        closeActionDropdown();
        return false;
    }

    std::optional<std::size_t> actionIndexAt(sf::Vector2f mouse) const
    {
        if (mouse.x < 42.0f || mouse.x > 272.0f || mouse.y < ListRowStartY)
        {
            return std::nullopt;
        }
        const std::size_t visibleIndex = static_cast<std::size_t>((mouse.y - ListRowStartY) / ListRowHeight);
        const std::size_t index = actionListOffset + visibleIndex;
        if (visibleIndex < VisibleActionRows && index < actions.size())
        {
            return index;
        }
        return std::nullopt;
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
        bar.setFillColor(sf::Color(5, 10, 11, 236));
        window.draw(bar);
        bayou::client::drawTitlePlaque(window, font, "Card Editor", {158.0f, 44.0f}, {280.0f, 54.0f});
        if (instructionsVisible)
        {
            drawText(window, font, "Card creation reference", 15, {356.0f, 31.0f}, Muted, 500.0f);
            instructionsBackButton.draw(window);
        }
        else
        {
            drawText(window, font, fmt::format("Card server {}", endpointText()), 15, {744.0f, 31.0f}, Muted, 214.0f);
            editorTabs.draw(window);
            instructionsButton.draw(window);
            backButton.draw(window);
        }
    }

    void drawUnsavedChangesPopup(sf::RenderWindow& window)
    {
        sf::RectangleShape shade({EditorWidth, EditorHeight});
        shade.setFillColor(sf::Color(0, 0, 0, 170));
        window.draw(shade);

        drawRoundedPanel(
            window,
            {382.0f, 236.0f},
            {520.0f, 244.0f},
            sf::Color(12, 20, 21, 252),
            Accent);
        drawText(window, font, "Unsaved Changes", 28, {414.0f, 266.0f}, Accent);
        drawText(
            window,
            font,
            editorMode == EditorMode::Cards
                ? "This card has changes that have not been saved."
                : "This action has changes that have not been saved.",
            17,
            {414.0f, 316.0f},
            Ink);
        drawText(window, font, "Save or discard them before continuing.", 16, {414.0f, 346.0f}, Muted);
        if (!unsavedChangesError.empty())
        {
            drawText(window, font, unsavedChangesError, 14, {414.0f, 378.0f}, Warn, 456.0f);
        }
        saveChangesButton.draw(window);
        discardChangesButton.draw(window);
        keepEditingButton.draw(window);
    }

    float instructionMaxScroll() const
    {
        return std::max(0.0f, instructionContentHeight - 552.0f);
    }

    void scrollInstructions(float delta)
    {
        instructionScroll = std::clamp(instructionScroll + delta, 0.0f, instructionMaxScroll());
    }

    float drawInstructionParagraph(
        sf::RenderWindow& window,
        const std::string& value,
        float y,
        sf::Color color = Ink,
        unsigned int size = 16,
        float indent = 0.0f)
    {
        constexpr float Left = 68.0f;
        constexpr float Width = 1080.0f;
        const float screenY = y - instructionScroll;
        const float lineHeight = static_cast<float>(size) + 6.0f;
        const std::vector<std::string> lines = wrapText(font, value, size, Width - indent);
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            const float lineY = screenY + static_cast<float>(i) * lineHeight;
            if (lineY >= 158.0f && lineY <= 706.0f)
            {
                drawText(window, font, lines[i], size, {Left + indent, lineY}, color);
            }
        }
        return y + static_cast<float>(lines.size()) * lineHeight;
    }

    float drawInstructionSection(sf::RenderWindow& window, const std::string& title, float y)
    {
        const float screenY = y - instructionScroll;
        if (screenY >= 158.0f && screenY <= 706.0f)
        {
            drawText(window, font, title, 22, {50.0f, screenY}, Accent);
            sf::RectangleShape line({1146.0f, 1.0f});
            line.setPosition({50.0f, screenY + 32.0f});
            line.setFillColor(Line);
            window.draw(line);
        }
        return y + 44.0f;
    }

    float drawInstructionBullet(sf::RenderWindow& window, const std::string& value, float y, sf::Color color = Ink)
    {
        const float screenY = y - instructionScroll;
        if (screenY >= 158.0f && screenY <= 706.0f)
        {
            drawText(window, font, "-", 17, {68.0f, screenY}, Accent);
        }
        return drawInstructionParagraph(window, value, y, color, 16, 22.0f) + 5.0f;
    }

    void drawInstructions(sf::RenderWindow& window)
    {
        drawRoundedPanel(window, {24.0f, 100.0f}, {1232.0f, 640.0f}, Panel);
        drawText(window, font, "How to Make a Card", 28, {50.0f, 116.0f}, Ink);
        drawText(window, font, "Mouse wheel or Page Up/Page Down to scroll. Escape returns to the editor.", 14, {650.0f, 124.0f}, Muted, 548.0f);

        float y = 164.0f;
        y = drawInstructionSection(window, "1. Card identity", y);
        y = drawInstructionBullet(window, "Title: the unique card name. Renaming an existing card updates that card; duplicate titles are not valid.", y);
        y = drawInstructionBullet(window, "Type: enter exactly Hero, Unit, Spell, or Enchantment. Type matching is case-sensitive.", y);
        y = drawInstructionBullet(window, "Image Path: a path under the assets folder, such as cards/clockwork-rook.png. Do not include an absolute path or use .. to leave the assets folder.", y);
        y = drawInstructionBullet(window, "Board art: Token selects the shared resting image and WalkAnim selects its horizontal walking sprite sheet; WalkAnimFrames gives the frame count. PieceBaseBlue and PieceBaseRed select the team-colored bases drawn beneath that art. IdleAnim loops while placed pieces are not moving; AttackAnim, DamagedAnim, and KilledAnim play as one-shot combat sheets. Optional FidgetAnim with FidgetAnimFrames plays occasionally while the piece is stationary.", y);
        y += 12.0f;

        y = drawInstructionSection(window, "2. Card types", y);
        y = drawInstructionBullet(window, "Hero: selected during deck building and placed on one of the player's four starting squares before play. Use heroCost instead of cost. A deck has 1-4 Heroes and a total Hero cost limit of 100. Losing every Hero loses the game.", y, sf::Color(248, 214, 112));
        y = drawInstructionBullet(window, "Unit: goes into the 20-card main deck. It costs Resources to play and deploys to an empty square the player controls. A newly deployed Unit cannot move or attack until its owner's next turn.", y, sf::Color(150, 210, 235));
        y = drawInstructionBullet(window, "Spell: goes into the main deck, costs Resources, resolves immediately, and leaves the hand. Spells do not use health, actions, or WalkAnim.", y, sf::Color(205, 175, 235));
        y = drawInstructionBullet(window, "Enchantment: goes into the main deck and attaches permanently to a player, square, or piece. Use target=player with effect=resourceDrain, target=square with effect=resources, or target=piece with effect=damage; power is the modifier amount.", y, sf::Color(194, 150, 235));
        y += 12.0f;

        y = drawInstructionSection(window, "3. Integer Fields (key = whole number)", y);
        y = drawInstructionBullet(window, "cost: Resources paid to play a Unit, Spell, or Enchantment. Default: 1.", y);
        y = drawInstructionBullet(window, "heroCost: deck-building Hero budget. Use only on Heroes. Default: 0.", y);
        y = drawInstructionBullet(window, "health: starting and maximum hit points for a Hero or Unit. Default: 1.", y);
        y = drawInstructionBullet(window, "width and height: board squares occupied by a Hero or Unit. Both default to 1.", y);
        y = drawInstructionBullet(window, "canControl: set to 0 for pieces that do not claim their occupied square or influence adjacent territory. Default: 1.", y);
        y = drawInstructionBullet(window, "growTurns: owner turns a newly summoned piece must wait before it can act. Default: 0.", y);
        y = drawInstructionBullet(window, "abilityUses: number of uses for a limited ability such as dig; use -1 for unlimited.", y);
        y = drawInstructionBullet(window, "gatherResources: passive Resources gained at the start of each owner's turn. It does not remove Resources from the opponent. Default: 0.", y);
        y = drawInstructionBullet(window, "power: spell amount. It is damage dealt, health restored, or Resources gained depending on effect. Default: 0.", y);
        y = drawInstructionBullet(window, "Tax: optional passive Resources taken from the opponent (up to this amount) and gained by the owning player at the start of each owner's turn. Default: 0.", y);
        y += 12.0f;

        y = drawInstructionSection(window, "4. Actions", y);
        y = drawInstructionParagraph(window, "Create reusable actions in the Actions tab, then add them in the card's Actions section. Card name is flavor text shown for that card; Reusable action chooses the shared logic. Click or focus the reusable-action field to open the list; typing filters it. The chain-link button opens that action in the Actions tab.", y, Muted);
        y += 5.0f;
        y = drawInstructionBullet(window, "Pattern ortho, diag, omni, horizontal, or vertical moves along that board geometry up to the action's maximum range.", y);
        y = drawInstructionBullet(window, "Pattern jump uses the fixed knight-style L shape. Pattern none is used for ranged, teleport, and tunnel actions that do not need slide geometry.", y);
        y = drawInstructionBullet(window, "Can move lets the action target an empty destination. With Can attack enabled, positive Damage targets and hurts enemies, while positive Heal targets and restores friendlies up to maximum health. Damage and Heal must not be negative.", y);
        y = drawInstructionBullet(window, "Target filter entries restrict attacks, healing, and status effects. A target must match every listed string across its Traits and Keywords; an empty filter accepts any otherwise-valid target.", y);
        y = drawInstructionBullet(window, "Push moves each surviving enemy target up to that many squares directly away from the attack's staging square. Blocked push distance becomes 1 extra damage per prevented square.", y);
        y = drawInstructionBullet(window, "Minimum and maximum range are per action, so a card can mix short moves, long moves, ranged attacks, and state-specific actions.", y);
        y = drawInstructionParagraph(window, "For blocking slide and capture actions, every square along the path must be empty. Pass-through ignores blockers. Line of sight applies blocker checks to ranged attacks.", y + 5.0f, sf::Color(198, 210, 224));
        y += 10.0f;
        y = drawInstructionParagraph(window, "A capture action is both movement and attack, but can only target an enemy-occupied square. If it destroys the enemy, the mover occupies the enemy's square; otherwise it falls back according to that action's movement geometry.", y, sf::Color(225, 170, 150));
        y += 17.0f;

        y = drawInstructionSection(window, "5. Spell / Enchantment String Fields", y);
        y = drawInstructionBullet(window, "effect=damage with target=enemy: subtract power from an enemy Hero or Unit. If health reaches 0, that piece is destroyed.", y);
        y = drawInstructionBullet(window, "effect=heal with target=ally: restore power health to a friendly Hero or Unit, up to its maximum health.", y);
        y = drawInstructionBullet(window, "effect=resources with target=none: immediately add power Resources to the player. No board target is required. Legacy effect=steam is also accepted.", y);
        y = drawInstructionParagraph(window, "Use the lowercase values exactly. The current game resolves targeting from effect; target documents the intended target and is displayed in card details.", y + 5.0f, Muted);
        y += 17.0f;

        y = drawInstructionSection(window, "6. Rarity, Traits, Keywords, and String Lists", y);
        y = drawInstructionBullet(window, "Rarity: add a String Field named rarity with value common, rare, legendary, or token. Missing or unknown values count as common. Token cards cannot appear in collections or decks. Shop selection odds are 70% common, 25% rare, and 5% legendary; cards within a rarity are equally likely.", y);
        y = drawInstructionBullet(window, "Traits: unit cards require living friendly heroes with matching traits when played. Heroes and spells do not require matching traits. The deck editor can filter by the nine supported traits.", y);
        y = drawInstructionBullet(window, "Keywords are stored separately from traits. Relentless lets a piece act again immediately whenever it destroys another piece. Bodyguard passively redirects damage from adjacent friendly non-Bodyguard pieces to itself; multiple Bodyguards split that damage as evenly as possible. Trail passively creates the Unit named by the summon string field where this piece started whenever it moves.", y);
        y = drawInstructionBullet(window, "ability: transform, dematerialize, dig, summon, or command. Transform-style abilities switch action states; dig creates a tunnel hole; summon creates the unit named by the summon string field in the space in front; command lets one ready adjacent friendly piece take any normal action without ending the turn.", y);
        y = drawInstructionBullet(window, "summon: exact Unit card title created by the active summon ability or passive Trail keyword. Player 1's active summon goes to the right and Player 2's to the left; Trail summons into the moving piece's former position. Summoned units cannot act until their owner's next turn.", y);
        y = drawInstructionBullet(window, "Cards store ordered references to reusable action objects plus a separate display name for each reference, so the same logic can have different fantasy names on different cards.", y);
        y = drawInstructionBullet(window, "An action is available in its state and changes the piece to nextState after use. nextState defaults to the action's state, so ordinary actions keep the piece unchanged.", y);
        y = drawInstructionBullet(window, "abilityLabels is an ordered String List containing the button label for each action state.", y);
        y = drawInstructionBullet(window, "Other String Lists are free-form named lists shown in card details.", y);
        y = drawInstructionBullet(window, "Unknown Integer or String Fields are stored and displayed, but they do not change gameplay unless code is added to read them.", y);
        y += 12.0f;

        y = drawInstructionSection(window, "7. Turn and board rules that affect balance", y);
        y = drawInstructionBullet(window, "On a turn, playing a card, moving, or attacking ends the turn. A piece can therefore move or attack, not both, before the opponent acts.", y);
        y = drawInstructionBullet(window, "A piece with canControl=1 controls its occupied square and influences adjacent territory. Pieces with canControl=0 do neither. Ties keep the current controller.", y);
        y = drawInstructionBullet(window, "At the start of a turn, the player gains 1 Resource per controlled square, gains each piece's gatherResources amount, Tax pieces collect Resources from the opponent, draws one card if below the 8-card hand limit, and refreshes their pieces.", y);
        y = drawInstructionBullet(window, "Attacks use the range, pattern, line-of-sight, and blocker settings of the selected action.", y);
        y += 12.0f;

        y = drawInstructionSection(window, "8. Complete examples", y);
        y = drawInstructionParagraph(window, "Unit example - Type: Unit | Integer Fields: cost=40; health=12 | String Fields: rarity=rare; WalkAnim=animations/clockwork-rook-walk.png | Actions: RookMove1", y, sf::Color(150, 210, 235));
        y += 10.0f;
        y = drawInstructionParagraph(window, "Hero example - Type: Hero | Integer Fields: heroCost=50; health=16 | String Fields: rarity=rare; WalkAnim=animations/marsh-witch-walk.png | Actions: BishopMove2; KingAttack3", y, sf::Color(248, 214, 112));
        y += 10.0f;
        y = drawInstructionParagraph(window, "Spell example - Type: Spell | Integer Fields: cost=20; power=6 | String Fields: effect=heal; target=ally; rarity=common", y, sf::Color(205, 175, 235));
        y = drawInstructionParagraph(window, "Enchantment example - Type: Enchantment | Integer Fields: cost=15; power=5 | String Fields: effect=resourceDrain; target=player; rarity=common", y, sf::Color(194, 150, 235));
        y += 22.0f;

        y = drawInstructionSection(window, "9. Using the editor safely", y);
        y = drawInstructionBullet(window, "Use the + button beside each section to add a field and the - button beside a row to remove it. Empty keys, empty traits, empty keywords, and empty list values are not saved.", y);
        y = drawInstructionBullet(window, "Integer values must be valid whole numbers. An invalid or blank number is omitted from the saved card, causing the game to use that field's default.", y);
        y = drawInstructionBullet(window, "Tab and Shift+Tab move between fields. Enter saves. The mouse wheel scrolls the field list when the pointer is over it.", y);
        y = drawInstructionBullet(window, "Save Card creates a draft or updates the selected card. Delete removes the selected saved card. If you navigate away with edits, choose Save or Discard in the unsaved-changes prompt.", y);
        y = drawInstructionBullet(window, "Check the Preview panel before saving. It shows the stored values, but only the recognized keys documented above affect the game.", y);
        y += 22.0f;

        instructionContentHeight = y - 100.0f;
        if (instructionMaxScroll() > 0.0f)
        {
            const float trackTop = 158.0f;
            const float trackHeight = 548.0f;
            sf::RectangleShape track({5.0f, trackHeight});
            track.setPosition({1224.0f, trackTop});
            track.setFillColor(sf::Color(49, 57, 70));
            window.draw(track);

            const float thumbHeight = std::max(42.0f, trackHeight * 552.0f / instructionContentHeight);
            const float thumbY = trackTop + (trackHeight - thumbHeight) * instructionScroll / instructionMaxScroll();
            sf::RectangleShape thumb({5.0f, thumbHeight});
            thumb.setPosition({1224.0f, thumbY});
            thumb.setFillColor(Accent);
            window.draw(thumb);
        }
    }

    void drawListPanel(sf::RenderWindow& window)
    {
        drawRoundedPanel(window, {ListPanelX, ListPanelY}, {ListPanelWidth, PanelHeight}, Panel);
        drawText(window, font, "Library", 22, {42.0f, 124.0f}, Ink);
        if (editorMode == EditorMode::Actions)
        {
            drawText(window, font, fmt::format("{} actions", actions.size()), 14, {200.0f, 131.0f}, Muted);
            const std::size_t lastVisible = std::min(actions.size(), actionListOffset + VisibleActionRows);
            for (std::size_t i = actionListOffset; i < lastVisible; ++i)
            {
                const float y = ListRowStartY + static_cast<float>(i - actionListOffset) * ListRowHeight;
                const bool selected = selectedAction && *selectedAction == i;
                bayou::client::drawBeveledPlate(
                    window,
                    {42.0f, y},
                    {230.0f, 48.0f},
                    selected ? sf::Color(76, 49, 25, 238) : PanelAlt,
                    selected ? Accent : sf::Color(91, 64, 37),
                    selected,
                    6.0f);
                drawText(window, font, actions[i].name, 16, {66.0f, y + 7.0f}, Ink, 186.0f);
                drawText(window, font, actions[i].kind + " / " + actions[i].pattern, 13, {66.0f, y + 29.0f}, Muted, 186.0f);
            }
            if (actions.size() > VisibleActionRows)
            {
                drawText(window, font, fmt::format("{}-{} of {}  mouse wheel", actionListOffset + 1, lastVisible, actions.size()), 12, {46.0f, 624.0f}, Muted, 220.0f);
            }
            copyActionButton.draw(window);
            newButton.draw(window);
            refreshButton.draw(window);
            return;
        }

        drawText(window, font, fmt::format("{} cards", cards.size()), 14, {218.0f, 131.0f}, Muted);

        const std::size_t lastVisible = std::min(cards.size(), listOffset + VisibleCardRows);
        for (std::size_t i = listOffset; i < lastVisible; ++i)
        {
            const float y = ListRowStartY + static_cast<float>(i - listOffset) * ListRowHeight;
            const bool selected = selectedCard && *selectedCard == i;
            bayou::client::drawBeveledPlate(
                window,
                {42.0f, y},
                {230.0f, 48.0f},
                selected ? sf::Color(76, 49, 25, 238) : PanelAlt,
                selected ? Accent : sf::Color(91, 64, 37),
                selected,
                6.0f);
            drawText(window, font, cards[i].title, 17, {66.0f, y + 8.0f}, Ink, 186.0f);
            drawText(window, font, cards[i].type, 13, {66.0f, y + 29.0f}, Muted, 186.0f);
        }
        if (cards.size() > VisibleCardRows)
        {
            drawText(window, font, fmt::format("{}-{} of {}  mouse wheel", listOffset + 1, lastVisible, cards.size()), 12, {46.0f, 624.0f}, Muted, 220.0f);
        }
        copyButton.draw(window);
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

    void drawActionReferenceField(sf::RenderWindow& window, std::size_t referenceIndex)
    {
        InputBox& field = actionRefFields[referenceIndex];
        if (!isVisibleInArrayViewport(field.bounds()))
        {
            return;
        }
        field.draw(window);
        const sf::FloatRect bounds = field.bounds();
        const float centerX = bounds.position.x + bounds.size.x - 15.0f;
        const float centerY = bounds.position.y + bounds.size.y * 0.5f + 1.0f;
        sf::ConvexShape arrow(3);
        arrow.setPoint(0, {centerX - 5.0f, centerY - 3.0f});
        arrow.setPoint(1, {centerX + 5.0f, centerY - 3.0f});
        arrow.setPoint(2, {centerX, centerY + 3.0f});
        arrow.setFillColor(openActionDropdownIndex && *openActionDropdownIndex == referenceIndex
            ? Accent
            : Muted);
        window.draw(arrow);
    }

    void drawActionLinkButton(sf::RenderWindow& window, std::size_t referenceIndex)
    {
        if (!isVisibleInArrayViewport(actionRefFields[referenceIndex].bounds()))
        {
            return;
        }
        const bool enabled = linkedActionIndex(referenceIndex).has_value();
        const bool hovered = enabled && hoveredActionLink && *hoveredActionLink == referenceIndex;
        const sf::FloatRect bounds = actionLinkButtonBounds(referenceIndex);
        bayou::client::drawBeveledPlate(
            window,
            bounds.position,
            bounds.size,
            enabled
                ? (hovered ? sf::Color(112, 75, 35) : AccentDark)
                : sf::Color(41, 45, 45, 220),
            enabled
                ? (hovered ? sf::Color(239, 190, 98) : sf::Color(181, 126, 60))
                : sf::Color(77, 83, 83),
            hovered,
            5.0f);
        if (!hasActionLinkTexture)
        {
            return;
        }

        sf::Sprite sprite(actionLinkTexture);
        const sf::Vector2u textureSize = actionLinkTexture.getSize();
        const float scale = std::min(
            22.0f / static_cast<float>(textureSize.x),
            22.0f / static_cast<float>(textureSize.y));
        sprite.setScale({scale, scale});
        const sf::Vector2f spriteSize(
            static_cast<float>(textureSize.x) * scale,
            static_cast<float>(textureSize.y) * scale);
        sprite.setPosition({
            bounds.position.x + (bounds.size.x - spriteSize.x) * 0.5f,
            bounds.position.y + (bounds.size.y - spriteSize.y) * 0.5f});
        if (!enabled)
        {
            sprite.setColor(sf::Color(112, 112, 106, 105));
        }
        window.draw(sprite);
    }

    void drawActionDropdown(sf::RenderWindow& window)
    {
        const std::optional<ActionDropdownLayout> layout = actionDropdownLayout();
        if (!layout)
        {
            return;
        }
        sf::RectangleShape panel(layout->bounds.size);
        panel.setPosition(layout->bounds.position);
        panel.setFillColor(sf::Color(7, 13, 14, 252));
        panel.setOutlineThickness(2.0f);
        panel.setOutlineColor(Accent);
        window.draw(panel);

        const std::vector<std::size_t> matches = filteredActionIndices();
        if (matches.empty())
        {
            drawText(
                window,
                font,
                actions.empty() ? "No saved actions" : "No matching actions",
                15,
                {layout->bounds.position.x + 12.0f, layout->bounds.position.y + 7.0f},
                Muted);
            return;
        }

        for (std::size_t row = 0; row < layout->visibleRows; ++row)
        {
            const std::size_t filteredIndex = layout->firstRow + row;
            if (filteredIndex >= matches.size())
            {
                break;
            }
            const float y = layout->bounds.position.y + static_cast<float>(row) * ActionDropdownRowHeight;
            const bool highlighted = filteredIndex == actionDropdownSelection ||
                (hoveredActionDropdownItem && *hoveredActionDropdownItem == filteredIndex);
            if (highlighted)
            {
                sf::RectangleShape highlight(
                    {layout->bounds.size.x - 4.0f, ActionDropdownRowHeight - 2.0f});
                highlight.setPosition({layout->bounds.position.x + 2.0f, y + 1.0f});
                highlight.setFillColor(sf::Color(76, 49, 25, 248));
                window.draw(highlight);
            }
            const card_data::Action& action = actions[matches[filteredIndex]];
            drawText(
                window,
                font,
                action.name,
                14,
                {layout->bounds.position.x + 10.0f, y + 4.0f},
                Ink,
                layout->bounds.size.x - 20.0f);
            drawText(
                window,
                font,
                action.kind + " / " + action.pattern,
                11,
                {layout->bounds.position.x + 10.0f, y + 24.0f},
                Muted,
                layout->bounds.size.x - 20.0f);
            if (row + 1 < layout->visibleRows)
            {
                sf::RectangleShape separator({layout->bounds.size.x - 8.0f, 1.0f});
                separator.setPosition({layout->bounds.position.x + 4.0f, y + ActionDropdownRowHeight - 1.0f});
                separator.setFillColor(sf::Color(91, 64, 37));
                window.draw(separator);
            }
        }

        if (matches.size() > layout->visibleRows)
        {
            const float trackHeight = layout->bounds.size.y - 8.0f;
            const float thumbHeight = std::max(
                12.0f,
                trackHeight * static_cast<float>(layout->visibleRows) / static_cast<float>(matches.size()));
            const std::size_t maximumOffset = matches.size() - layout->visibleRows;
            const float thumbY = layout->bounds.position.y + 4.0f +
                (trackHeight - thumbHeight) * static_cast<float>(layout->firstRow) /
                    static_cast<float>(maximumOffset);
            sf::RectangleShape thumb({3.0f, thumbHeight});
            thumb.setPosition({layout->bounds.position.x + layout->bounds.size.x - 6.0f, thumbY});
            thumb.setFillColor(Accent);
            window.draw(thumb);
        }
    }

    void drawArrayEditor(sf::RenderWindow& window)
    {
        layoutArrayControls();
        sf::RectangleShape viewport({482.0f, ArrayViewportHeight});
        viewport.setPosition({334.0f, ArrayViewportTop});
        viewport.setFillColor(sf::Color(8, 18, 20, 246));
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

        drawVisibleButton(window, addTraitButton);
        drawVisibleButton(window, addKeywordButton);
        drawVisibleButton(window, addIntegerButton);
        drawVisibleButton(window, addStringButton);
        drawVisibleButton(window, addListButton);
        drawVisibleButton(window, addActionRefButton);
        for (InputBox& field : traitFields)
        {
            drawVisibleField(window, field);
        }
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
        for (InputBox& field : actionDisplayNameFields)
        {
            drawVisibleField(window, field);
        }
        for (std::size_t i = 0; i < actionRefFields.size(); ++i)
        {
            drawActionReferenceField(window, i);
        }
        for (EditorButton& button : removeTraitButtons)
        {
            drawVisibleButton(window, button);
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
        for (EditorButton& button : removeActionRefButtons)
        {
            drawVisibleButton(window, button);
        }
        for (std::size_t i = 0; i < actionRefFields.size(); ++i)
        {
            drawActionLinkButton(window, i);
        }
        drawActionDropdown(window);
    }

    void layoutActionFields()
    {
        actionStateField.setPosition({340.0f, 232.0f});
        actionNextStateField.setPosition({600.0f, 232.0f});
        actionKindField.setPosition({340.0f, 290.0f});
        actionPatternField.setPosition({600.0f, 290.0f});
        actionMinRangeField.setPosition({340.0f, 348.0f});
        actionMaxRangeField.setPosition({600.0f, 348.0f});
        actionDamageField.setPosition({340.0f, 406.0f});
        actionHealField.setPosition({600.0f, 406.0f});
        actionCanMoveField.setPosition({340.0f, 464.0f});
        actionCanAttackField.setPosition({600.0f, 464.0f});
        actionPassThroughField.setPosition({340.0f, 522.0f});
        actionLineOfSightField.setPosition({600.0f, 522.0f});
        actionStatusTurnsField.setPosition({340.0f, 580.0f});
        actionCooldownTurnsField.setPosition({600.0f, 580.0f});
        actionPushField.setPosition({340.0f, 638.0f});
        layoutActionTargetFilterControls();
    }

    void drawActionEditorPanel(sf::RenderWindow& window)
    {
        drawRoundedPanel(window, {EditorPanelX, EditorPanelY}, {ActionEditorPanelWidth, PanelHeight}, Panel);
        drawText(window, font, "Edit Action", 22, {340.0f, 124.0f}, Ink);
        drawText(window, font, "Boolean fields use 1 or 0.", 14, {610.0f, 131.0f}, Muted, 206.0f);
        actionNameField.draw(window);
        layoutActionFields();
        const std::vector<std::pair<std::string, sf::Vector2f>> labels = {
            {"State", {340.0f, 208.0f}},
            {"Next state", {600.0f, 208.0f}},
            {"Kind", {340.0f, 266.0f}},
            {"Pattern", {600.0f, 266.0f}},
            {"Minimum range", {340.0f, 324.0f}},
            {"Maximum range", {600.0f, 324.0f}},
            {"Damage", {340.0f, 382.0f}},
            {"Heal", {600.0f, 382.0f}},
            {"Can move", {340.0f, 440.0f}},
            {"Can attack", {600.0f, 440.0f}},
            {"Pass through", {340.0f, 498.0f}},
            {"Line of sight", {600.0f, 498.0f}},
            {"Status turns", {340.0f, 556.0f}},
            {"Cooldown turns", {600.0f, 556.0f}},
            {"Push", {340.0f, 614.0f}},
        };
        for (const auto& [label, position] : labels)
        {
            drawText(window, font, label, 14, position, Muted);
        }
        for (InputBox* field : focusOrder)
        {
            if (field != &actionNameField && !actionTargetFilterIndex(field))
            {
                field->draw(window);
            }
        }

        sf::RectangleShape divider({1.0f, 524.0f});
        divider.setPosition({846.0f, 152.0f});
        divider.setFillColor(Line);
        window.draw(divider);

        drawText(window, font, "Target filter", 22, {870.0f, 164.0f}, Ink);
        drawText(
            window,
            font,
            "Every entry must match a target trait or keyword.",
            14,
            {870.0f, 198.0f},
            Muted,
            306.0f);
        addActionTargetFilterButton.draw(window);

        sf::RectangleShape targetFilterArea(
            {368.0f, TargetFilterRowHeight * VisibleTargetFilterRows + 8.0f});
        targetFilterArea.setPosition({864.0f, TargetFilterTop - 4.0f});
        targetFilterArea.setFillColor(sf::Color(8, 18, 20, 246));
        targetFilterArea.setOutlineThickness(1.0f);
        targetFilterArea.setOutlineColor(Line);
        window.draw(targetFilterArea);
        if (actionTargetFilterFields.empty())
        {
            drawText(window, font, "Any target", 16, {870.0f, TargetFilterTop + 6.0f}, Ink, 294.0f);
        }
        for (std::size_t i = actionTargetFilterOffset;
             i < actionTargetFilterFields.size() &&
                 i < actionTargetFilterOffset + VisibleTargetFilterRows;
             ++i)
        {
            actionTargetFilterFields[i].draw(window);
        }
        for (EditorButton& button : removeActionTargetFilterButtons)
        {
            button.draw(window);
        }
        if (actionTargetFilterFields.size() > VisibleTargetFilterRows)
        {
            const std::size_t lastVisible = std::min(
                actionTargetFilterFields.size(),
                actionTargetFilterOffset + VisibleTargetFilterRows);
            drawText(
                window,
                font,
                fmt::format("{}-{} of {}  mouse wheel", actionTargetFilterOffset + 1,
                            lastVisible, actionTargetFilterFields.size()),
                12,
                {870.0f, 672.0f},
                Muted,
                306.0f);

            const float trackHeight = TargetFilterRowHeight * VisibleTargetFilterRows;
            sf::RectangleShape track({5.0f, trackHeight});
            track.setPosition({1224.0f, TargetFilterTop});
            track.setFillColor(sf::Color(49, 57, 70));
            window.draw(track);
            const float thumbHeight = std::max(
                TargetFilterRowHeight,
                trackHeight * static_cast<float>(VisibleTargetFilterRows) /
                    static_cast<float>(actionTargetFilterFields.size()));
            const std::size_t maxOffset = actionTargetFilterFields.size() - VisibleTargetFilterRows;
            const float thumbY = TargetFilterTop +
                (trackHeight - thumbHeight) * static_cast<float>(actionTargetFilterOffset) /
                    static_cast<float>(maxOffset);
            sf::RectangleShape thumb({5.0f, thumbHeight});
            thumb.setPosition({1224.0f, thumbY});
            thumb.setFillColor(Accent);
            window.draw(thumb);
        }
        deleteButton.draw(window);
        saveActionButton.draw(window);
        drawText(window, font, status, 16, {340.0f, 702.0f}, statusColor, 174.0f);
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
        drawRoundedPanel(window, {938.0f, 160.0f}, {230.0f, 322.0f}, sf::Color(27, 39, 38), AccentDark);
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
            imageSlot.setFillColor(sf::Color(7, 16, 18));
            imageSlot.setOutlineThickness(1.0f);
            imageSlot.setOutlineColor(Line);
            window.draw(imageSlot);
            drawText(window, font, "No Image", 20, {1016.0f, 264.0f}, Muted);
        }

        const card_data::Card card = cardFromForm();
        sf::Text title(font, elideToWidth(font, card.title.empty() ? "Untitled Card" : card.title, 22, 210.0f), 22);
        title.setFillColor(Ink);
        bayou::client::centerText(title, {1053.0f, 414.0f});
        window.draw(title);
        sf::Text type(font, card.type, 16);
        type.setFillColor(Accent);
        bayou::client::centerText(type, {1053.0f, 445.0f});
        window.draw(type);

        int cost = 0;
        for (const card_data::KeyIntPair& pair : card.integerValues)
        {
            if (pair.key == "cost")
            {
                cost = pair.value;
                break;
            }
        }

        float y = 480.0f;
        drawText(window, font, fmt::format("Cost: {}", cost), 16, {882.0f, y}, Ink);
        y += 54.0f;
        drawText(window, font, "Traits", 15, {882.0f, y}, Muted);
        drawText(window, font, joinStrings(card.traits, ", "), 16, {882.0f, y + 22.0f}, Ink, 336.0f);
        y += 54.0f;
        drawText(window, font, "Keywords", 15, {882.0f, y}, Muted);
        drawText(window, font, joinStrings(card.keywords, ", "), 16, {882.0f, y + 22.0f}, Ink, 336.0f);
    }

    void setStatus(const std::string& message, sf::Color color)
    {
        status = message;
        statusColor = color;
    }
};
