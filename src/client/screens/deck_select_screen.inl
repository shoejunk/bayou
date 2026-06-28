    auto drawDeckSelect = [&]() {
        drawPanel(window, {250.0f, 120.0f}, {300.0f, 312.0f});
        drawText(window, font, "Your Decks", 22, {266.0f, 132.0f}, sf::Color::White);
        drawText(window, font, "Coins " + std::to_string(playerCoins), 14, {430.0f, 138.0f}, sf::Color(248, 214, 112), 100.0f);

        const std::size_t lastDeck = std::min(playerDecks.size(), deckListOffset + VisibleDeckRows);
        for (std::size_t i = deckListOffset; i < lastDeck; ++i)
        {
            const float rowY = 172.0f + static_cast<float>(i - deckListOffset) * DeckRowHeight;
            drawRow(window, font, {266.0f, rowY}, {268.0f, DeckRowHeight - 4.0f},
                    playerDecks[i].name,
                    std::to_string(playerDecks[i].cardTitles.size()) + " cards",
                    selectedDeck && *selectedDeck == i);
        }
        if (playerDecks.empty() && !pendingPlayLoad)
        {
            drawText(window, font, "No decks. Build one in the", 15, {268.0f, 220.0f}, sf::Color(190, 198, 214));
            drawText(window, font, "Deck Editor first.", 15, {268.0f, 242.0f}, sf::Color(190, 198, 214));
        }

        findMatchButton.draw(window);
        backButton.draw(window);
        window.draw(messageText);
    };

