    auto drawDeckUnsavedChangesPopup = [&]() {
        if (!deckUnsavedChangesPopupVisible)
        {
            return;
        }

        sf::RectangleShape overlay({800.0f, 600.0f});
        overlay.setFillColor(sf::Color(0, 0, 0, 170));
        window.draw(overlay);
        drawPanel(window, {220.0f, 188.0f}, {360.0f, 220.0f});
        drawText(window, font, "Discard Changes?", 28, {252.0f, 218.0f}, sf::Color(248, 224, 172), 300.0f);
        drawText(window, font, "This deck has unsaved changes.", 16, {260.0f, 276.0f}, sf::Color(220, 224, 230), 280.0f);
        drawText(window, font, "Discard them and go back?", 16, {274.0f, 302.0f}, sf::Color(220, 224, 230), 250.0f);
        keepEditingDeckButton.draw(window);
        discardDeckChangesButton.draw(window);
    };

    auto drawDeckEditor = [&]() {
        layoutDeckEditorControls();
        drawText(window, font, "Deck Editor", 30, {24.0f, 18.0f}, sf::Color::White);
        drawText(window, font, "Signed in as " + signedInLabel(), 14, {270.0f, 22.0f}, sf::Color(178, 186, 202), 360.0f);
        drawText(window, font, "Coins " + std::to_string(playerCoins), 13, {270.0f, 45.0f}, sf::Color(248, 214, 112), 160.0f);
        drawText(window, font, "Card server " + endpointText(clientConfig().card), 13, {390.0f, 45.0f}, sf::Color(148, 158, 176), 240.0f);
        deckBackButton.draw(window);

        if (deckEditorMode == DeckEditorMode::DeckList)
        {
            drawPanel(window, {DeckPickerPanelX, DeckPickerPanelY}, {DeckPickerPanelWidth, DeckPickerPanelHeight});
            drawText(window, font, "Your Decks", 22, {244.0f, 107.0f}, sf::Color::White);
            newDeckButton.draw(window);
            refreshDeckButton.draw(window);

            const std::size_t lastDeck = std::min(playerDecks.size(), deckListOffset + VisibleDeckRows);
            for (std::size_t i = deckListOffset; i < lastDeck; ++i)
            {
                const float y = DeckListY + static_cast<float>(i - deckListOffset) * DeckRowHeight;
                drawRow(
                    window,
                    font,
                    {DeckListX, y},
                    {DeckListWidth, DeckRowHeight - 4.0f},
                    playerDecks[i].name,
                    std::to_string(playerDecks[i].cardTitles.size()) + " cards",
                    selectedDeck && *selectedDeck == i);
            }
            if (playerDecks.empty() && !deckEditorBusy())
            {
                drawText(window, font, "No saved decks", 16, {332.0f, 306.0f}, sf::Color(178, 186, 202));
            }
            editDeckButton.draw(window);
            deleteDeckButton.draw(window);
            window.draw(messageText);
            return;
        }

        drawPanel(window, {CurrentDeckPanelX, DeckEditorPanelY}, {CurrentDeckPanelWidth, DeckEditorPanelHeight});
        deckNameInput.draw(window);

        const DeckStats stats = computeDeckStats();
        const bool cardsOk = stats.cardCount == game_data::DeckCardCount;
        const bool heroesOk = stats.heroCount >= game_data::MinHeroes && stats.heroCount <= game_data::MaxHeroes;
        const bool costOk = stats.heroCost <= game_data::HeroCostLimit;
        const sf::Color okColor(120, 220, 150);
        const sf::Color badColor(224, 130, 110);
        drawText(window, font,
                 std::to_string(stats.cardCount) + "/" + std::to_string(game_data::DeckCardCount),
                 14, {270.0f, 112.0f}, cardsOk ? okColor : badColor, 96.0f);
        drawText(window, font,
                 "Heroes " + std::to_string(stats.heroCost) + "/" + std::to_string(game_data::HeroCostLimit),
                 12, {270.0f, 135.0f}, (heroesOk && costOk) ? okColor : badColor, 96.0f);
        float warningY = 164.0f;
        for (const std::string& warning : stats.warnings)
        {
            drawText(window, font, warning, 11, {42.0f, warningY}, sf::Color(248, 190, 105), 324.0f);
            warningY += 13.0f;
        }

        const std::size_t lastDeckCard = std::min(editingDeck.cardTitles.size(), deckCardListOffset + VisibleDeckCardRows);
        for (std::size_t i = deckCardListOffset; i < lastDeckCard; ++i)
        {
            const float y = DeckCardsY + static_cast<float>(i - deckCardListOffset) * DeckCardRowHeight;
            const card_data::Card* card = cardByTitle(editingDeck.cardTitles[i]);
            std::string secondary;
            if (card)
            {
                const std::string copies = "  " + std::to_string(deckCopies(editingDeck.cardTitles[i])) +
                    "/" + std::to_string(ownedCopies(editingDeck.cardTitles[i]));
                secondary = game_data::isHeroCard(*card)
                    ? "Hero  cost " + std::to_string(game_data::cardInt(*card, "heroCost", 0)) + copies
                    : card->type + "  " + std::to_string(game_data::cardInt(*card, "cost", 0)) + " steam" + copies;
            }
            drawRow(
                window,
                font,
                {DeckCardsX, y},
                {DeckCardsWidth, DeckCardRowHeight - 4.0f},
                editingDeck.cardTitles[i],
                secondary,
                selectedDeckCard && *selectedDeckCard == i);
            if (std::find(
                    stats.keywordMismatchCardIndices.begin(),
                    stats.keywordMismatchCardIndices.end(),
                    i) != stats.keywordMismatchCardIndices.end())
            {
                sf::RectangleShape warningStripe({4.0f, DeckCardRowHeight - 4.0f});
                warningStripe.setPosition({DeckCardsX, y});
                warningStripe.setFillColor(sf::Color(238, 120, 80, 230));
                window.draw(warningStripe);

                sf::RectangleShape warningOutline({DeckCardsWidth, DeckCardRowHeight - 4.0f});
                warningOutline.setPosition({DeckCardsX, y});
                warningOutline.setFillColor(sf::Color::Transparent);
                warningOutline.setOutlineThickness(1.0f);
                warningOutline.setOutlineColor(sf::Color(238, 120, 80, 210));
                window.draw(warningOutline);
            }
        }
        if (editingDeck.cardTitles.empty() && !deckEditorBusy())
        {
            drawText(window, font, "No cards in this deck", 15, {108.0f, 304.0f}, sf::Color(178, 186, 202), 180.0f);
        }
        removeCardButton.draw(window);

        drawPanel(window, {LibraryPanelX, DeckEditorPanelY}, {LibraryPanelWidth, DeckEditorPanelHeight});
        drawText(window, font, "Collection", 22, {430.0f, 107.0f}, sf::Color::White);
        const std::string collectionCountText = filteredCardLibrary.size() == cardLibrary.size()
            ? std::to_string(cardLibrary.size()) + " owned card types"
            : std::to_string(filteredCardLibrary.size()) + "/" + std::to_string(cardLibrary.size()) + " owned card types";
        drawText(
            window,
            font,
            collectionCountText,
            14,
            {430.0f, 138.0f},
            sf::Color(178, 186, 202));
        drawText(window, font, "Type", 12, {430.0f, 154.0f}, sf::Color(248, 214, 112));
        collectionHeroFilterCheckbox.draw(window, collectionTypeFilterChecked[0]);
        collectionUnitFilterCheckbox.draw(window, collectionTypeFilterChecked[1]);
        collectionSpellFilterCheckbox.draw(window, collectionTypeFilterChecked[2]);
        drawText(window, font, "Keywords", 12, {430.0f, 199.0f}, sf::Color(248, 214, 112));
        collectionBioFilterCheckbox.draw(window, collectionKeywordFilterChecked[0]);
        collectionMechanicalFilterCheckbox.draw(window, collectionKeywordFilterChecked[1]);
        collectionOccultFilterCheckbox.draw(window, collectionKeywordFilterChecked[2]);
        collectionRiffraffFilterCheckbox.draw(window, collectionKeywordFilterChecked[3]);

        const std::size_t lastCard = std::min(filteredCardLibrary.size(), libraryOffset + VisibleLibraryRows);
        for (std::size_t i = libraryOffset; i < lastCard; ++i)
        {
            const float y = LibraryY + static_cast<float>(i - libraryOffset) * LibraryRowHeight;
            const card_data::Card& libCard = filteredCardLibrary[i];
            const std::string secondary = game_data::isHeroCard(libCard)
                ? "Hero  hc " + std::to_string(game_data::cardInt(libCard, "heroCost", 0)) +
                    "  owned " + std::to_string(ownedCopies(libCard.title))
                : libCard.type + "  " + std::to_string(game_data::cardInt(libCard, "cost", 0)) +
                    " steam  owned " + std::to_string(ownedCopies(libCard.title));
            drawRow(
                window,
                font,
                {LibraryX, y},
                {LibraryWidth, LibraryRowHeight - 4.0f},
                libCard.title,
                secondary,
                selectedLibraryCard && *selectedLibraryCard == i);
        }
        if (filteredCardLibrary.empty() && !deckEditorBusy())
        {
            drawText(
                window,
                font,
                cardLibrary.empty() ? "No owned cards" : "No matching cards",
                16,
                {514.0f, 330.0f},
                sf::Color(178, 186, 202));
        }
        addCardButton.draw(window);
        saveDeckButton.draw(window);

        const bool hoveringDropTarget = dragActive && draggingLibraryCard &&
            isInsideRect(dragCurrentPos, CurrentDeckPanelX, DeckEditorPanelY, CurrentDeckPanelWidth, DeckEditorPanelHeight);
        if (hoveringDropTarget)
        {
            sf::RectangleShape dropTarget({CurrentDeckPanelWidth, DeckEditorPanelHeight});
            dropTarget.setPosition({CurrentDeckPanelX, DeckEditorPanelY});
            dropTarget.setFillColor(sf::Color(80, 140, 130, 45));
            dropTarget.setOutlineThickness(3.0f);
            dropTarget.setOutlineColor(sf::Color(103, 198, 184));
            window.draw(dropTarget);
        }

        if (dragActive && draggingLibraryCard && *draggingLibraryCard < filteredCardLibrary.size())
        {
            const sf::Vector2f ghostPosition{dragCurrentPos.x - 96.0f, dragCurrentPos.y - 15.0f};
            sf::RectangleShape ghost({192.0f, 30.0f});
            ghost.setPosition(ghostPosition);
            ghost.setFillColor(sf::Color(60, 88, 102, 220));
            ghost.setOutlineThickness(1.0f);
            ghost.setOutlineColor(sf::Color(130, 220, 205));
            window.draw(ghost);
            drawText(
                window,
                font,
                filteredCardLibrary[*draggingLibraryCard].title,
                15,
                {ghostPosition.x + 10.0f, ghostPosition.y + 6.0f},
                sf::Color::White,
                172.0f);
        }

        window.draw(messageText);
    };

