    // Offline regressions use the same SFML events as the interactive editor.
    bool runCaptureInteractionChecks(
        sf::RenderWindow& window,
        const std::vector<card_data::Card>& library,
        std::string& failure)
    {
        failure.clear();
        const auto check = [&](bool condition, const char* message) {
            if (!condition) failure = message;
            return condition;
        };
        const auto pixelAt = [&](float x, float y) {
            return window.mapCoordsToPixel({x, y}, makeEditorView(window));
        };
        const auto click = [&](float x, float y) {
            const sf::Vector2i pixel = pixelAt(x, y);
            handleEvent(sf::Event{sf::Event::MouseButtonPressed{sf::Mouse::Button::Left, pixel}}, window);
            handleEvent(sf::Event{sf::Event::MouseButtonReleased{sf::Mouse::Button::Left, pixel}}, window);
        };
        const auto key = [&](sf::Keyboard::Key code) {
            sf::Event::KeyPressed event{};
            event.code = code;
            handleEvent(sf::Event{event}, window);
        };
        const auto type = [&](const std::string& value) {
            for (unsigned char ch : value)
                handleEvent(sf::Event{sf::Event::TextEntered{ch}}, window);
        };
        const auto wheel = [&](float delta, sf::Mouse::Wheel axis = sf::Mouse::Wheel::Vertical) {
            handleEvent(sf::Event{sf::Event::MouseWheelScrolled{axis, delta, pixelAt(150.0f, 300.0f)}}, window);
        };

        applyCaptureState("card-editor-actions", library);
        for (int i = 0; i < 20; ++i)
        {
            card_data::Action action;
            action.name = fmt::format("{} {}", i % 2 == 0 ? "Needle" : "Other", i);
            actions.push_back(action);
        }
        applyActionSearchFilter();
        click(100.0f, 180.0f);
        type("  nEeDlE  ");
        if (!check(visibleActionIndices.size() == 10 && actionListOffset == 0 &&
                   !hasUnsavedChanges(), "Action search must trim, ignore case, and leave the form clean.")) return false;
        key(sf::Keyboard::Key::Enter);
        if (!check(status == "Loaded from card server", "Enter in action search must not save to the server.")) return false;
        wheel(-1.0f);
        if (!check(actionListOffset == 1, "Filtered action list must scroll.")) return false;
        click(110.0f, CardListRowStartY + 20.0f);
        if (!check(selectedAction && actions[*selectedAction].name == "Needle 2" &&
                   actionNameField.getValue() == "Needle 2", "Click must select the filtered row's underlying action.")) return false;
        scrollActionList(100);
        if (!check(actionListOffset == 3, "Filtered list must clamp at its last page.")) return false;
        click(110.0f, CardListRowStartY + 6.0f * ListRowHeight + 20.0f);
        if (!check(selectedAction && actions[*selectedAction].name == "Needle 18",
                   "Last filtered row must select the last match.")) return false;
        click(100.0f, 180.0f);
        key(sf::Keyboard::Key::End);
        type("missing");
        wheel(-1.0f);
        if (!check(visibleActionIndices.empty() && actionListOffset == 0 &&
                   !actionIndexAt({110.0f, CardListRowStartY + 20.0f}),
                   "An empty filter result must have no clickable rows or scroll offset.")) return false;
        // Clear through the ordinary text-event path.
        for (int i = 0; i < 30; ++i) key(sf::Keyboard::Key::Backspace);
        if (!check(visibleActionIndices.size() == actions.size(), "Clearing search must restore every action.")) return false;
        type("needle");
        click(445.0f, 35.0f);
        if (!check(editorMode == EditorMode::Cards && cardSearchField.getValue().empty(),
                   "Card and action filters must be independent.")) return false;
        click(575.0f, 35.0f);
        if (!check(editorMode == EditorMode::Actions && visibleActionIndices.size() == 10,
                   "Action filter must survive tab changes.")) return false;

        click(1030.0f, 40.0f);
        if (!check(instructionsVisible, "Instructions button must open the guide.")) return false;
        key(sf::Keyboard::Key::End); // Must work even before the first render.
        if (!check(instructionScroll == instructionMaxScroll() && instructionScroll > 0.0f,
                   "End must reach the actual bottom before rendering.")) return false;
        const auto* cachedLines = instructionLines.data();
        const std::size_t lineCount = instructionLines.size();
        key(sf::Keyboard::Key::Home);
        wheel(-0.25f);
        if (!check(instructionScroll == 16.0f, "Fractional wheel input must retain its magnitude.")) return false;
        wheel(0.0f);
        wheel(-1.0f, sf::Mouse::Wheel::Horizontal);
        if (!check(instructionScroll == 16.0f, "Zero and horizontal wheel input must not move the guide.")) return false;
        key(sf::Keyboard::Key::PageDown);
        if (!check(instructionScroll == 496.0f, "Page Down must advance one page.")) return false;
        key(sf::Keyboard::Key::PageUp);
        if (!check(instructionScroll == 16.0f, "Page Up must return one page.")) return false;
        wheel(10000.0f);
        if (!check(instructionScroll == 0.0f, "Guide must clamp at the top.")) return false;
        wheel(-10000.0f);
        if (!check(instructionScroll == instructionMaxScroll(), "Guide must clamp at the bottom.")) return false;
        if (!check(instructionLines.data() == cachedLines && instructionLines.size() == lineCount,
                   "Scrolling must reuse the instruction layout.")) return false;
        key(sf::Keyboard::Key::Escape);
        if (!check(!instructionsVisible && editorMode == EditorMode::Actions &&
                   actionSearchField.getValue() == "needle", "Escape must restore the filtered editor.")) return false;

        // Measure guide rendering with/without repeated layout at the same
        // scroll positions. Excludes display/vsync and is not an FPS claim.
        instructionsVisible = true;
        render(window); // Warm glyph and panel resources before measuring.
        const auto measure = [&](bool rebuild) {
            sf::Clock timer;
            for (int frame = 0; frame < 12; ++frame)
            {
                if (rebuild) instructionLines.clear();
                instructionScroll = instructionMaxScroll() * static_cast<float>(frame) / 11.0f;
                render(window);
            }
            return timer.getElapsedTime().asSeconds() * 1000.0f / 12.0f;
        };
        const float repeatedLayoutMs = measure(true);
        const float cachedLayoutMs = measure(false);
        fmt::println("[Card editor guide render CPU] repeated layout: {:.2f} ms/frame; cached layout: {:.2f} ms/frame",
                     repeatedLayoutMs, cachedLayoutMs);
        // Leave a useful screenshot at the bottom of the guide.
        instructionScroll = instructionMaxScroll();
        return true;
    }
