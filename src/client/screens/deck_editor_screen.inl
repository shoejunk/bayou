    auto drawDeckUnsavedChangesPopup = [&]() {
        if (!deckUnsavedChangesPopupVisible)
        {
            return;
        }

        sf::RectangleShape overlay({ui_canvas::Width, ui_canvas::Height});
        overlay.setPosition({ui_canvas::Left, 0.0f});
        overlay.setFillColor(sf::Color(0, 0, 0, 182));
        window.draw(overlay);

        constexpr float DialogX = 232.0f;
        constexpr float DialogY = 194.0f;
        constexpr float DialogWidth = 336.0f;
        constexpr float DialogHeight = 208.0f;
        drawPanel(window, {DialogX, DialogY}, {DialogWidth, DialogHeight});

        const sf::Vector2f crest{DialogX + DialogWidth * 0.5f, DialogY + 40.0f};
        // Short enough to fit the slot at any deck name length; the name is on the
        // deck-name field behind the dialog.
        drawValidationSlot(
            collectionUi,
            {{DialogX + 22.0f, DialogY + 96.0f}, {DialogWidth - 44.0f, 44.0f}},
            {"This deck has unsaved changes.", "Going back now discards them."},
            false);

        sf::Text heading(
            gloomthornFontLoaded ? gloomthornFont : font,
            "Discard Changes?",
            26);
        centerText(heading, {crest.x, DialogY + 46.0f});
        sf::Text headingShadow(heading);
        headingShadow.move({1.0f, 2.0f});
        headingShadow.setFillColor(sf::Color(0, 0, 0, 200));
        drawCrispText(window, headingShadow);
        heading.setFillColor(sf::Color(248, 224, 172));
        drawCrispText(window, heading);

        drawInnerRule(collectionUi, {DialogX + 34.0f, DialogY + 74.0f}, DialogWidth - 68.0f);

        keepEditingDeckButton.draw(window);
        discardDeckChangesButton.draw(window);
    };

    auto drawDeckEditor = [&]() {
        layoutDeckEditorControls();
        if (starterDeckMode)
        {
            adminTabs.draw(window);
            drawText(window, font, "Signed in as " + signedInLabel(), 14, {452.0f, 22.0f}, sf::Color(178, 186, 202), 212.0f);
            drawText(window, font, "New accounts pick one for free", 13, {452.0f, 45.0f}, sf::Color(248, 214, 112), 212.0f);
        }
        else
        {
            // The card server endpoint used to be printed here. It is diagnostic
            // detail, not something a player has any use for.
            drawScreenHeader(collectionUi, "Deck Editor", signedInLabel(), playerCoins, 244.0f);
        }
        deckBackButton.draw(window);

        if (deckEditorMode == DeckEditorMode::DeckList)
        {
            const sf::Vector2f pointer = collectionPointer();
            drawPanel(window, {DeckPickerPanelX, DeckPickerPanelY}, {DeckPickerPanelWidth, DeckPickerPanelHeight});
            drawSectionHeading(
                collectionUi,
                {DeckListX, DeckPickerPanelY + 12.0f},
                starterDeckMode ? "Starter Decks" : "Your Decks",
                DeckListWidth * 0.62f);

            // Roster count, right-aligned against the heading rule.
            const std::string rosterCount = std::to_string(playerDecks.size()) +
                (playerDecks.size() == 1 ? " deck" : " decks");
            sf::Text rosterCountText(font, rosterCount, 11);
            drawCaption(
                collectionUi,
                {DeckListX + DeckListWidth - rosterCountText.getLocalBounds().size.x, DeckPickerPanelY + 24.0f},
                rosterCount);

            const std::size_t lastDeck = std::min(playerDecks.size(), deckListOffset + VisibleDeckRows);
            const std::optional<std::size_t> hovered = hoveredRow(
                DeckListX, DeckListY, DeckListWidth, DeckRowHeight,
                VisibleDeckRows, deckListOffset, playerDecks.size());
            for (std::size_t i = deckListOffset; i < lastDeck; ++i)
            {
                const float y = DeckListY + static_cast<float>(i - deckListOffset) * DeckRowHeight;
                drawDeckRosterRow(
                    collectionUi,
                    {{DeckListX, y}, {DeckListWidth, DeckRowHeight - 4.0f}},
                    deckSummaryFor(playerDecks[i]),
                    selectedDeck && *selectedDeck == i,
                    hovered && *hovered == i);
            }
            drawListScrollTrack(
                DeckListX + DeckListWidth + 6.0f, DeckListY, DeckRowHeight * VisibleDeckRows - 4.0f,
                deckListOffset, VisibleDeckRows, playerDecks.size());

            if (playerDecks.empty() && !deckEditorBusy())
            {
                drawEmptyState(
                    collectionUi,
                    {{DeckListX, DeckListY}, {DeckListWidth, DeckPickerPanelY + DeckPickerPanelHeight - DeckListY - 14.0f}},
                    starterDeckMode ? "No Starter Decks" : "No Decks Yet",
                    starterDeckMode
                        ? "Starter decks are configured on the card server."
                        : "Build one with New, or claim a faction deck from the shop.");
            }

            // The selected deck's portrait fills what used to be dead space.
            if (selectedDeck && *selectedDeck < playerDecks.size())
            {
                drawDeckDetailPanel(
                    collectionUi,
                    {{DeckDetailPanelX, DeckPickerPanelY}, {DeckDetailPanelWidth, DeckPickerPanelHeight}},
                    deckSummaryFor(playerDecks[*selectedDeck]));
            }
            else
            {
                drawPanel(window, {DeckDetailPanelX, DeckPickerPanelY}, {DeckDetailPanelWidth, DeckPickerPanelHeight});
                const sf::FloatRect slot{
                    {DeckDetailPanelX + 16.0f, DeckPickerPanelY + 22.0f},
                    {DeckDetailPanelWidth - 32.0f, DeckPickerPanelHeight - 44.0f}};
                if (playerDecks.empty())
                {
                    drawFactionRoster(
                        collectionUi,
                        slot,
                        "Claim a faction deck from the shop, or build your own from scratch.");
                }
                else
                {
                    drawEmptyState(
                        collectionUi,
                        slot,
                        "No Deck Selected",
                        "Choose a deck to see its hero, counts and resource curve.");
                }
            }

            // Verbs that act on a selection are disabled until there is one.
            const bool hasSelection = selectedDeck && *selectedDeck < playerDecks.size();
            if (!starterDeckMode)
            {
                newDeckButton.draw(window);
            }
            refreshDeckButton.draw(window);
            if (hasSelection)
            {
                editDeckButton.draw(window);
            }
            else
            {
                drawDisabledButton(
                    collectionUi,
                    editDeckButton.shape.getPosition(),
                    editDeckButton.shape.getSize(),
                    "Edit");
            }
            if (!starterDeckMode)
            {
                if (hasSelection)
                {
                    deleteDeckButton.draw(window);
                }
                else
                {
                    drawDisabledButton(
                        collectionUi,
                        deleteDeckButton.shape.getPosition(),
                        deleteDeckButton.shape.getSize(),
                        "Delete");
                }
            }
            setMessageY(messageText, 562.0f);
            drawCrispText(window, messageText);
            static_cast<void>(pointer);
            return;
        }

        drawPanel(window, {CurrentDeckPanelX, DeckEditorPanelY}, {CurrentDeckPanelWidth, DeckEditorPanelHeight});

        const DeckStats stats = computeDeckStats();
        const bool cardsOk = stats.cardCount == game_data::DeckCardCount;
        const bool heroesOk = stats.heroCount >= game_data::MinHeroes && stats.heroCount <= game_data::MaxHeroes;
        const bool costOk = stats.heroCost <= game_data::HeroCostLimit;

        // The name field was an unlabelled empty box. Caption it, and put the
        // deck's resource curve beside it so the shape of the list is visible
        // while it is being built.
        drawCaption(collectionUi, {DeckCardsX, 100.0f}, "DECK NAME");
        deckNameInput.draw(window);

        const DeckSummary editingSummary = deckSummaryFor(editingDeck);
        const FactionStyle& editingStyle =
            editingSummary.faction ? *editingSummary.faction : unalignedStyle();
        drawCaption(collectionUi, {264.0f, 100.0f}, "CURVE");
        drawManaCurve(
            collectionUi,
            {{264.0f, 114.0f}, {100.0f, 28.0f}},
            editingSummary.curve,
            editingStyle.accent,
            false);

        // Counters as labelled meters: "10/20" and "Heroes 0/100" gave a player
        // no way to know what either number was counting.
        constexpr float MeterWidth = 101.0f;
        drawMeter(
            collectionUi,
            {DeckCardsX, 152.0f},
            {MeterWidth, 20.0f},
            "CARDS",
            std::to_string(stats.cardCount) + "/" + std::to_string(game_data::DeckCardCount),
            static_cast<float>(stats.cardCount) / static_cast<float>(game_data::DeckCardCount),
            cardsOk ? palette::Good : palette::Warn);
        drawMeter(
            collectionUi,
            {DeckCardsX + MeterWidth + 10.0f, 152.0f},
            {MeterWidth, 20.0f},
            "HEROES",
            std::to_string(stats.heroCount) + "/" + std::to_string(game_data::MaxHeroes),
            static_cast<float>(stats.heroCount) / static_cast<float>(game_data::MaxHeroes),
            heroesOk ? palette::Good : palette::Warn);
        drawMeter(
            collectionUi,
            {DeckCardsX + (MeterWidth + 10.0f) * 2.0f, 152.0f},
            {MeterWidth, 20.0f},
            "HERO COST",
            std::to_string(stats.heroCost) + "/" + std::to_string(game_data::HeroCostLimit),
            static_cast<float>(stats.heroCost) / static_cast<float>(game_data::HeroCostLimit),
            costOk ? palette::BrassPale : palette::Bad);

        drawInnerRule(collectionUi, {DeckCardsX, 178.0f}, DeckCardsWidth);

        const std::vector<std::string> deckTitles = deckUniqueTitles();
        const std::size_t lastDeckCard = std::min(deckTitles.size(), deckCardListOffset + VisibleDeckCardRows);
        const std::optional<std::size_t> hoveredDeckCard = hoveredRow(
            DeckCardsX, DeckCardsY, DeckCardsWidth, DeckCardRowHeight,
            VisibleDeckCardRows, deckCardListOffset, deckTitles.size());
        for (std::size_t i = deckCardListOffset; i < lastDeckCard; ++i)
        {
            const float y = DeckCardsY + static_cast<float>(i - deckCardListOffset) * DeckCardRowHeight;
            const card_data::Card* card = cardByTitle(deckTitles[i]);
            if (!card)
            {
                card = cardInAllLibraryByTitle(deckTitles[i]);
            }
            CardRow row;
            row.card = card;
            row.rect = {{DeckCardsX, y}, {DeckCardsWidth, DeckCardRowHeight - 4.0f}};
            row.selected = selectedDeckCard && *selectedDeckCard == i;
            row.hovered = hoveredDeckCard && *hoveredDeckCard == i;
            row.copies = deckCopies(deckTitles[i]);
            row.copyLimit = card ? game_data::cardDeckLimit(*card) : 0;
            row.traitMismatch = std::find(
                                    stats.traitMismatchTitles.begin(),
                                    stats.traitMismatchTitles.end(),
                                    deckTitles[i]) != stats.traitMismatchTitles.end();
            drawCardRow(collectionUi, row);
        }
        drawListScrollTrack(
            DeckCardsX + DeckCardsWidth + 5.0f, DeckCardsY, DeckCardRowHeight * VisibleDeckCardRows - 4.0f,
            deckCardListOffset, VisibleDeckCardRows, deckTitles.size());

        if (editingDeck.cardTitles.empty() && !deckEditorBusy())
        {
            drawEmptyState(
                collectionUi,
                {{DeckCardsX, DeckCardsY}, {DeckCardsWidth, DeckCardRowHeight * VisibleDeckCardRows - 4.0f}},
                "Empty Deck",
                "Pick cards from your collection and press Add, or drag them across.");
        }

        // Validation lives in a reserved slot at the foot of the deck panel. It
        // used to be drawn centred on the screen, straight over the collection.
        drawValidationSlot(
            collectionUi,
            {{DeckCardsX, DeckValidationY}, {DeckCardsWidth, DeckValidationHeight}},
            stats.warnings,
            cardsOk && heroesOk && costOk);

        removeCardButton.draw(window);

        drawPanel(window, {LibraryPanelX, DeckEditorPanelY}, {LibraryPanelWidth, DeckEditorPanelHeight});
        drawSectionHeading(
            collectionUi,
            {LibraryX, DeckEditorPanelY + 12.0f},
            starterDeckMode ? "All Cards" : "Collection",
            LibraryWidth * 0.55f);

        const std::string libraryCountSuffix = starterDeckMode ? " card types" : " owned card types";
        const std::string collectionCountText = filteredCardLibrary.size() == cardLibrary.size()
            ? std::to_string(cardLibrary.size()) + libraryCountSuffix
            : std::to_string(filteredCardLibrary.size()) + " of " + std::to_string(cardLibrary.size()) + libraryCountSuffix;
        sf::Text collectionCountLabel(font, collectionCountText, 11);
        drawCaption(
            collectionUi,
            {LibraryX + LibraryWidth - collectionCountLabel.getLocalBounds().size.x, DeckEditorPanelY + 24.0f},
            collectionCountText);

        drawCaption(collectionUi, {LibraryX, CollectionTypeChipsY - 14.0f}, "TYPE");
        for (std::size_t i = 0; i < collectionTypeChips.size(); ++i)
        {
            drawFilterChip(
                collectionUi,
                collectionTypeChips[i],
                collectionTypeFilterChecked[i],
                collectionTypeChips[i].rect.contains(collectionPointer()),
                palette::BrassBright,
                CollectionChipTextSize);
        }

        drawCaption(collectionUi, {LibraryX, CollectionTraitChipsY - 14.0f}, "TRAITS");
        for (std::size_t i = 0; i < collectionTraitChips.size(); ++i)
        {
            drawFilterChip(
                collectionUi,
                collectionTraitChips[i],
                collectionTraitFilterChecked[i],
                collectionTraitChips[i].rect.contains(collectionPointer()),
                traitAccent(collectionTraitChips[i].label),
                CollectionChipTextSize);
        }

        drawInnerRule(collectionUi, {LibraryX, LibraryY - 12.0f}, LibraryWidth);

        const std::size_t lastCard = std::min(filteredCardLibrary.size(), libraryOffset + VisibleLibraryRows);
        const std::optional<std::size_t> hoveredLibraryCard = hoveredRow(
            LibraryX, LibraryY, LibraryWidth, LibraryRowHeight,
            VisibleLibraryRows, libraryOffset, filteredCardLibrary.size());
        for (std::size_t i = libraryOffset; i < lastCard; ++i)
        {
            const float y = LibraryY + static_cast<float>(i - libraryOffset) * LibraryRowHeight;
            CardRow row;
            row.card = &filteredCardLibrary[i];
            row.rect = {{LibraryX, y}, {LibraryWidth, LibraryRowHeight - 4.0f}};
            row.selected = selectedLibraryCard && *selectedLibraryCard == i;
            row.hovered = hoveredLibraryCard && *hoveredLibraryCard == i;
            // Starter-deck editing works from the whole catalogue, where holdings
            // are meaningless; a player's collection shows what they own.
            row.showOwned = !starterDeckMode;
            row.owned = starterDeckMode ? 0 : ownedCopies(filteredCardLibrary[i].title);
            drawCardRow(collectionUi, row);
        }
        drawListScrollTrack(
            LibraryX + LibraryWidth + 5.0f, LibraryY, LibraryRowHeight * VisibleLibraryRows - 4.0f,
            libraryOffset, VisibleLibraryRows, filteredCardLibrary.size());

        if (filteredCardLibrary.empty() && !deckEditorBusy())
        {
            const bool nothingOwned = cardLibrary.empty();
            drawEmptyState(
                collectionUi,
                {{LibraryX, LibraryY}, {LibraryWidth, LibraryRowHeight * VisibleLibraryRows - 4.0f}},
                nothingOwned ? (starterDeckMode ? "No Cards" : "Nothing Collected") : "No Matches",
                nothingOwned
                    ? "Open a pack in the shop to start a collection."
                    : "No card matches these filters. Clear one to widen the search.");
        }
        addCardButton.draw(window);
        if (deckHasUnsavedChanges())
        {
            saveDeckButton.draw(window);
        }
        else
        {
            const sf::Vector2f position = saveDeckButton.shape.getPosition();
            const sf::Vector2f size = saveDeckButton.shape.getSize();
            drawBeveledPlate(
                window,
                position,
                size,
                sf::Color(42, 41, 38, 192),
                sf::Color(91, 86, 75, 180),
                false,
                std::clamp(size.y * 0.20f, 5.0f, 11.0f));

            sf::Text label = saveDeckButton.text;
            label.setFillColor(sf::Color(168, 172, 172, 210));
            window.draw(label);
        }

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

        const bool hoveringRemoveTarget = dragActive && draggingDeckCard &&
            isInsideRect(dragCurrentPos, LibraryPanelX, DeckEditorPanelY, LibraryPanelWidth, DeckEditorPanelHeight);
        if (hoveringRemoveTarget)
        {
            sf::RectangleShape removeTarget({LibraryPanelWidth, DeckEditorPanelHeight});
            removeTarget.setPosition({LibraryPanelX, DeckEditorPanelY});
            removeTarget.setFillColor(sf::Color(140, 80, 70, 45));
            removeTarget.setOutlineThickness(3.0f);
            removeTarget.setOutlineColor(sf::Color(224, 130, 110));
            window.draw(removeTarget);
        }

        std::optional<std::string> draggedTitle;
        if (dragActive && draggingLibraryCard && *draggingLibraryCard < filteredCardLibrary.size())
        {
            draggedTitle = filteredCardLibrary[*draggingLibraryCard].title;
        }
        else if (dragActive && draggingDeckCard && *draggingDeckCard < deckTitles.size())
        {
            draggedTitle = deckTitles[*draggingDeckCard];
        }
        if (draggedTitle)
        {
            // The dragged card carries its own art and cost, so what is in hand is
            // never ambiguous.
            const card_data::Card* ghostCard = cardByTitle(*draggedTitle);
            if (!ghostCard)
            {
                ghostCard = cardInAllLibraryByTitle(*draggedTitle);
            }
            const sf::Vector2f ghostPosition{dragCurrentPos.x - 100.0f, dragCurrentPos.y - 19.0f};
            if (ghostCard)
            {
                CardRow ghost;
                ghost.card = ghostCard;
                ghost.rect = {ghostPosition, {200.0f, 38.0f}};
                ghost.selected = true;
                drawCardRow(collectionUi, ghost);
            }
            else
            {
                drawBeveledPlate(
                    window,
                    ghostPosition,
                    {200.0f, 38.0f},
                    sf::Color(58, 40, 22, 240),
                    palette::BrassBright,
                    true,
                    4.0f);
                drawText(window, font, *draggedTitle, 15, {ghostPosition.x + 14.0f, ghostPosition.y + 10.0f},
                         palette::Ink, 172.0f);
            }
        }

        // Deck-legality warnings now live in the validation slot inside the deck
        // panel, so the message line only carries transient status.
        setMessageY(messageText, 562.0f);
        drawCrispText(window, messageText);
    };
