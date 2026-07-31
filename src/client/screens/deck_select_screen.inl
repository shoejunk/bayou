    auto drawDeckSelect = [&]() {
        // Same roster and portrait as the deck editor: choosing which deck to take
        // into a match is the moment a player most needs to see its hero and curve,
        // and it used to be the screen that showed the least.
        drawPanel(window, {DeckPickerPanelX, DeckSelectPanelY}, {DeckPickerPanelWidth, DeckSelectPanelHeight});
        drawSectionHeading(
            collectionUi,
            {DeckListX, DeckSelectPanelY + 10.0f},
            "Choose Your Deck",
            DeckListWidth * 0.72f);

        const std::size_t lastDeck = std::min(playerDecks.size(), deckListOffset + VisibleDeckRows);
        const std::optional<std::size_t> hovered = hoveredRow(
            DeckListX, DeckSelectListY, DeckListWidth, DeckRowHeight,
            VisibleDeckRows, deckListOffset, playerDecks.size());
        for (std::size_t i = deckListOffset; i < lastDeck; ++i)
        {
            const float rowY = DeckSelectListY + static_cast<float>(i - deckListOffset) * DeckRowHeight;
            drawDeckRosterRow(
                collectionUi,
                {{DeckListX, rowY}, {DeckListWidth, DeckRowHeight - 4.0f}},
                deckSummaryFor(playerDecks[i]),
                selectedDeck && *selectedDeck == i,
                hovered && *hovered == i);
        }
        drawListScrollTrack(
            DeckListX + DeckListWidth + 6.0f, DeckSelectListY, DeckRowHeight * VisibleDeckRows - 4.0f,
            deckListOffset, VisibleDeckRows, playerDecks.size());

        if (playerDecks.empty() && !pendingPlayLoad)
        {
            drawEmptyState(
                collectionUi,
                {{DeckListX, DeckSelectListY},
                 {DeckListWidth, DeckSelectPanelY + DeckSelectPanelHeight - DeckSelectListY - 14.0f}},
                "No Decks Yet",
                "Build a deck in the Deck Editor before looking for a match.");
        }

        const bool hasSelection = selectedDeck && *selectedDeck < playerDecks.size();
        if (hasSelection)
        {
            drawDeckDetailPanel(
                collectionUi,
                {{DeckDetailPanelX, DeckSelectPanelY}, {DeckDetailPanelWidth, DeckSelectPanelHeight}},
                deckSummaryFor(playerDecks[*selectedDeck]));
        }
        else
        {
            drawPanel(window, {DeckDetailPanelX, DeckSelectPanelY}, {DeckDetailPanelWidth, DeckSelectPanelHeight});
            const sf::FloatRect slot{
                {DeckDetailPanelX + 16.0f, DeckSelectPanelY + 22.0f},
                {DeckDetailPanelWidth - 32.0f, DeckSelectPanelHeight - 44.0f}};
            if (playerDecks.empty())
            {
                drawFactionRoster(
                    collectionUi,
                    slot,
                    "Claim a faction deck from the shop to get into a match quickly.");
            }
            else
            {
                drawEmptyState(
                    collectionUi,
                    slot,
                    "No Deck Selected",
                    "Pick a deck to see the hero you will be fielding.");
            }
        }

        // The deck-select footer has its own layout: this screen's two actions
        // need distinct plates and hitboxes beneath the panels.
        layoutDeckSelectControls();

        // Matchmaking needs a deck, so the verb waits until there is one.
        if (hasSelection)
        {
            findMatchButton.draw(window);
        }
        else
        {
            drawDisabledButton(
                collectionUi,
                findMatchButton.shape.getPosition(),
                findMatchButton.shape.getSize(),
                "Find Match");
        }
        backButton.draw(window);
        setMessageY(messageText, 562.0f);
        drawCrispText(window, messageText);
    };
