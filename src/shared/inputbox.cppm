module;

#include <SFML/Graphics.hpp>
#include <SFML/Window/Clipboard.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

export module inputbox;

export struct InputBoxStyle
{
    sf::Color fieldFill = sf::Color(16, 23, 25, 230);
    sf::Color inactiveOutline = sf::Color(154, 112, 61);
    sf::Color activeOutline = sf::Color(95, 219, 196);
    sf::Color labelColor = sf::Color(221, 198, 157);
    sf::Color textColor = sf::Color(246, 238, 218);
    sf::Color selectionColor = sf::Color(50, 120, 140, 160);
    sf::Color cursorColor = sf::Color(95, 219, 196);
    float outlineThickness = 2.0f;
    unsigned int labelSize = 18;
    unsigned int textSize = 20;
    sf::Vector2f labelOffset = {0.0f, -25.0f};
    sf::Vector2f textOffset = {10.0f, 10.0f};
    sf::Vector2f cursorSize = {1.5f, 24.0f};
    float cursorOffsetY = 8.0f;
    float selectionHeight = 24.0f;
    float selectionOffsetY = 8.0f;
    float horizontalPadding = 20.0f;
};

export struct InputBox
{
    std::string content;
    bool active = false;
    bool isPassword = false;

    InputBox() = default;

    InputBox(const sf::Vector2f& position, const sf::Vector2f& size, const std::string& labelText, sf::Font& font, bool password = false)
    {
        initialize(font, labelText, position, size, clientStyle(), password);
    }

    InputBox(sf::Font& font, std::string labelText, sf::Vector2f position, sf::Vector2f size)
    {
        initialize(font, std::move(labelText), position, size, editorStyle(), false);
    }

    static InputBoxStyle clientStyle()
    {
        return {};
    }

    static InputBoxStyle editorStyle()
    {
        InputBoxStyle style;
        style.fieldFill = sf::Color(13, 24, 25, 242);
        style.inactiveOutline = sf::Color(116, 82, 44);
        style.activeOutline = sf::Color(92, 202, 181);
        style.labelColor = sf::Color(198, 174, 130);
        style.textColor = sf::Color(244, 234, 208);
        style.selectionColor = sf::Color(48, 126, 124, 165);
        style.cursorColor = sf::Color(92, 202, 181);
        style.outlineThickness = 1.0f;
        style.labelSize = 15;
        style.textSize = 18;
        style.labelOffset = {0.0f, -22.0f};
        style.textOffset = {12.0f, 10.0f};
        style.cursorSize = {1.5f, 22.0f};
        style.cursorOffsetY = 9.0f;
        style.selectionHeight = 22.0f;
        style.selectionOffsetY = 9.0f;
        style.horizontalPadding = 24.0f;
        return style;
    }

    void initialize(sf::Font& font, const std::string& labelText, sf::Vector2f position, sf::Vector2f size, InputBoxStyle nextStyle, bool password = false)
    {
        style = nextStyle;
        isPassword = password;
        label.emplace(font, labelText, style.labelSize);
        text.emplace(font, "", style.textSize);

        shape.setPosition(position);
        shape.setSize(size);
        shape.setFillColor(style.fieldFill);
        shape.setOutlineThickness(style.outlineThickness);
        shape.setOutlineColor(style.inactiveOutline);

        label->setFillColor(style.labelColor);
        label->setPosition(position + style.labelOffset);

        text->setFillColor(style.textColor);
        text->setPosition(position + style.textOffset);
        refreshText();
    }

    void update(const sf::Vector2f& mousePos)
    {
        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
        {
            setActive(contains(mousePos));
        }
    }

    bool contains(const sf::Vector2f& point) const
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
        if (label)
        {
            label->setPosition(position + style.labelOffset);
        }
        if (text)
        {
            text->setPosition(position + style.textOffset);
        }
    }

    void setRightContentInset(float inset)
    {
        rightContentInset = std::max(0.0f, inset);
        refreshText();
    }

    void setActive(bool next)
    {
        active = next;
        cursorTimer = 0.0f;
        showCursor = true;
        draggingSelection = false;
        shape.setOutlineColor(active ? style.activeOutline : style.inactiveOutline);
        clampCaret();
        refreshText();
    }

    bool isActive() const
    {
        return active;
    }

    void beginMouseSelection(sf::Vector2f mouse, bool extendSelection)
    {
        if (!active)
        {
            return;
        }

        setCaretFromMouse(mouse, extendSelection);
        draggingSelection = true;
    }

    void handleEvent(const sf::Event& event, const sf::RenderWindow& window)
    {
        if (const auto* released = event.getIf<sf::Event::MouseButtonReleased>();
            released && released->button == sf::Mouse::Button::Left)
        {
            draggingSelection = false;
        }

        if (!active)
        {
            return;
        }

        if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>();
            pressed && pressed->button == sf::Mouse::Button::Left)
        {
            const sf::Vector2f mouse = window.mapPixelToCoords(pressed->position);
            if (contains(mouse))
            {
                beginMouseSelection(mouse, shiftPressed());
            }
            return;
        }

        if (const auto* moved = event.getIf<sf::Event::MouseMoved>(); moved && draggingSelection)
        {
            setCaretFromMouse(window.mapPixelToCoords(moved->position), true);
            return;
        }

        if (const auto* keyEvent = event.getIf<sf::Event::KeyPressed>())
        {
            handleKey(*keyEvent);
            return;
        }

        if (const auto* textEvent = event.getIf<sf::Event::TextEntered>())
        {
            handleText(*textEvent);
        }
    }

    void handleText(sf::Event::TextEntered textEvent)
    {
        if (!active)
        {
            return;
        }

        const char c = static_cast<char>(textEvent.unicode);
        if (c >= 32 && c < 127)
        {
            replaceSelection(std::string(1, c));
            resetCursorBlink();
            refreshText();
        }
    }

    void updateCursor(float deltaTime)
    {
        if (!active)
        {
            return;
        }

        cursorTimer += deltaTime;
        if (cursorTimer >= 0.5f)
        {
            cursorTimer = 0.0f;
            showCursor = !showCursor;
        }
    }

    void draw(sf::RenderWindow& window) const
    {
        if (label)
        {
            window.draw(*label);
        }
        window.draw(shape);
        drawSelection(window);
        if (text)
        {
            window.draw(*text);
        }
        if (active)
        {
            sf::RectangleShape cursor(style.cursorSize);
            cursor.setPosition({caretX(), shape.getPosition().y + style.cursorOffsetY});
            cursor.setFillColor(style.cursorColor);
            window.draw(cursor);
        }
    }

    const std::string& getContent() const { return content; }
    const std::string& getValue() const { return content; }

    void setPasswordMode(bool password)
    {
        isPassword = password;
        refreshText();
    }

    void setContent(const std::string& value)
    {
        setValue(value);
    }

    void setValue(const std::string& value)
    {
        content = value;
        caretIndex = content.size();
        selectionAnchor = caretIndex;
        displayStart = 0;
        refreshText();
    }

    void clear()
    {
        content.clear();
        caretIndex = 0;
        selectionAnchor = 0;
        displayStart = 0;
        refreshText();
    }

private:
    sf::RectangleShape shape;
    std::optional<sf::Text> label;
    std::optional<sf::Text> text;
    InputBoxStyle style;
    float cursorTimer = 0.0f;
    bool showCursor = true;
    std::size_t caretIndex = 0;
    std::size_t selectionAnchor = 0;
    std::size_t displayStart = 0;
    bool draggingSelection = false;
    float rightContentInset = 0.0f;

    static bool shiftPressed()
    {
        return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
    }

    void resetCursorBlink()
    {
        cursorTimer = 0.0f;
        showCursor = true;
    }

    void clampCaret()
    {
        caretIndex = std::min(caretIndex, content.size());
        selectionAnchor = std::min(selectionAnchor, content.size());
        displayStart = std::min(displayStart, content.size());
    }

    bool hasSelection() const
    {
        return caretIndex != selectionAnchor;
    }

    std::pair<std::size_t, std::size_t> selectionRange() const
    {
        return std::minmax(caretIndex, selectionAnchor);
    }

    void clearSelection()
    {
        selectionAnchor = caretIndex;
    }

    void eraseSelection()
    {
        if (!hasSelection())
        {
            return;
        }

        const auto [first, last] = selectionRange();
        content.erase(first, last - first);
        caretIndex = first;
        selectionAnchor = caretIndex;
    }

    void replaceSelection(const std::string& replacement)
    {
        eraseSelection();
        content.insert(caretIndex, replacement);
        caretIndex += replacement.size();
        selectionAnchor = caretIndex;
    }

    void refreshText()
    {
        clampCaret();
        if (!text)
        {
            return;
        }

        keepCaretVisible();
        std::string display = displayString(displayStart, content.size());
        text->setString(display);
        while (!display.empty() && text->getLocalBounds().size.x > maxTextWidth())
        {
            display.pop_back();
            text->setString(display);
        }
    }

    void keepCaretVisible()
    {
        displayStart = std::min(displayStart, caretIndex);
        while (displayStart < caretIndex)
        {
            text->setString(displayString(displayStart, caretIndex));
            if (text->getLocalBounds().size.x <= maxTextWidth())
            {
                break;
            }
            ++displayStart;
        }
    }

    float maxTextWidth() const
    {
        return std::max(1.0f, textRightEdge() - textLeftEdge());
    }

    std::string displayString(std::size_t first, std::size_t last) const
    {
        std::string display = content.substr(first, last - first);
        if (isPassword)
        {
            display.assign(display.size(), '*');
        }
        return display;
    }

    std::size_t visibleLength() const
    {
        return text ? static_cast<std::size_t>(text->getString().getSize()) : 0;
    }

    float characterX(std::size_t index) const
    {
        if (!text)
        {
            return shape.getPosition().x + style.textOffset.x;
        }

        const std::size_t visibleIndex = std::clamp(index, displayStart, displayStart + visibleLength()) - displayStart;
        return text->findCharacterPos(visibleIndex).x;
    }

    float caretX() const
    {
        return std::clamp(characterX(caretIndex), textLeftEdge(), textRightEdge());
    }

    float textLeftEdge() const
    {
        return shape.getPosition().x + style.textOffset.x;
    }

    float textRightEdge() const
    {
        const float fallbackRightPadding = std::max(0.0f, style.horizontalPadding - style.textOffset.x);
        return shape.getPosition().x + shape.getSize().x - fallbackRightPadding - rightContentInset;
    }

    std::size_t indexFromMouseX(float x) const
    {
        if (!text)
        {
            return caretIndex;
        }

        const std::size_t length = visibleLength();
        if (length == 0 || x <= text->findCharacterPos(0).x)
        {
            return displayStart;
        }

        for (std::size_t i = 0; i < length; ++i)
        {
            const float left = text->findCharacterPos(i).x;
            const float right = text->findCharacterPos(i + 1).x;
            if (x < (left + right) * 0.5f)
            {
                return displayStart + i;
            }
        }

        return std::min(displayStart + length, content.size());
    }

    void setCaretFromMouse(sf::Vector2f mouse, bool extendSelection)
    {
        caretIndex = indexFromMouseX(mouse.x);
        if (!extendSelection)
        {
            selectionAnchor = caretIndex;
        }
        resetCursorBlink();
        refreshText();
    }

    void handleKey(const sf::Event::KeyPressed& keyEvent)
    {
        if (keyEvent.control)
        {
            if (keyEvent.code == sf::Keyboard::Key::A)
            {
                selectionAnchor = 0;
                caretIndex = content.size();
            }
            else if (keyEvent.code == sf::Keyboard::Key::C && hasSelection())
            {
                const auto [first, last] = selectionRange();
                sf::Clipboard::setString(content.substr(first, last - first));
            }
            else if (keyEvent.code == sf::Keyboard::Key::X && hasSelection())
            {
                const auto [first, last] = selectionRange();
                sf::Clipboard::setString(content.substr(first, last - first));
                eraseSelection();
            }
            else if (keyEvent.code == sf::Keyboard::Key::V)
            {
                replaceSelection(sf::Clipboard::getString().toAnsiString());
            }
            resetCursorBlink();
            refreshText();
            return;
        }

        if (keyEvent.code == sf::Keyboard::Key::Left)
        {
            if (!keyEvent.shift && hasSelection())
            {
                caretIndex = selectionRange().first;
            }
            else if (caretIndex > 0)
            {
                --caretIndex;
            }
            if (!keyEvent.shift)
            {
                clearSelection();
            }
        }
        else if (keyEvent.code == sf::Keyboard::Key::Right)
        {
            if (!keyEvent.shift && hasSelection())
            {
                caretIndex = selectionRange().second;
            }
            else if (caretIndex < content.size())
            {
                ++caretIndex;
            }
            if (!keyEvent.shift)
            {
                clearSelection();
            }
        }
        else if (keyEvent.code == sf::Keyboard::Key::Home)
        {
            caretIndex = 0;
            if (!keyEvent.shift)
            {
                clearSelection();
            }
        }
        else if (keyEvent.code == sf::Keyboard::Key::End)
        {
            caretIndex = content.size();
            if (!keyEvent.shift)
            {
                clearSelection();
            }
        }
        else if (keyEvent.code == sf::Keyboard::Key::Backspace)
        {
            if (hasSelection())
            {
                eraseSelection();
            }
            else if (caretIndex > 0)
            {
                content.erase(caretIndex - 1, 1);
                --caretIndex;
                clearSelection();
            }
        }
        else if (keyEvent.code == sf::Keyboard::Key::Delete)
        {
            if (hasSelection())
            {
                eraseSelection();
            }
            else if (caretIndex < content.size())
            {
                content.erase(caretIndex, 1);
                clearSelection();
            }
        }

        resetCursorBlink();
        refreshText();
    }

    void drawSelection(sf::RenderWindow& window) const
    {
        if (!active || !hasSelection() || !text)
        {
            return;
        }

        const auto [selectionFirst, selectionLast] = selectionRange();
        const std::size_t visibleFirst = std::max(selectionFirst, displayStart);
        const std::size_t visibleLast = std::min(selectionLast, displayStart + visibleLength());
        if (visibleFirst >= visibleLast)
        {
            return;
        }

        const float x1 = characterX(visibleFirst);
        const float x2 = characterX(visibleLast);
        sf::RectangleShape selection({std::max(1.0f, x2 - x1), style.selectionHeight});
        selection.setPosition({x1, shape.getPosition().y + style.selectionOffsetY});
        selection.setFillColor(style.selectionColor);
        window.draw(selection);
    }
};
