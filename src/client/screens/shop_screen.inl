    auto drawShop = [&]() {
        drawScreenHeader(collectionUi, "Shop", signedInLabel(), playerCoins, 176.0f);
        shopBackButton.draw(window);

        constexpr float ShopPanelX = 96.0f;
        constexpr float ShopPanelY = 94.0f;
        constexpr float ShopPanelWidth = 608.0f;
        constexpr float ShopPanelHeight = 386.0f;
        drawPanel(window, {ShopPanelX, ShopPanelY}, {ShopPanelWidth, ShopPanelHeight});

        const sf::Vector2f center{400.0f, 286.0f};
        if (pendingShopLoad)
        {
            drawEmptyState(
                collectionUi,
                {{ShopPanelX + 20.0f, ShopPanelY + 40.0f}, {ShopPanelWidth - 40.0f, ShopPanelHeight - 80.0f}},
                "Opening the Shop",
                "Fetching today's stock from the card server.");
        }
        else if (revealedCardTitle)
        {
            const float t = animationTime - revealStartedAt;
            const card_data::Card* card = cardInAllLibraryByTitle(*revealedCardTitle);
            const sf::Color rarity = card ? cardRarityColor(*card) : palette::BrassPale;

            // One burst in the pulled card's own rarity colour, rather than four
            // stacked circles and a ring of dots.
            drawRevealBurst(collectionUi, center, t, rarity);

            sf::Text heading(
                gloomthornFontLoaded ? gloomthornFont : font,
                card && cardRarity(*card) == "legendary" ? "A Legendary!" : "Card Acquired",
                24);
            centerText(heading, {400.0f, ShopPanelY + 30.0f});
            sf::Text headingShadow(heading);
            headingShadow.move({1.0f, 2.0f});
            headingShadow.setFillColor(sf::Color(0, 0, 0, 205));
            drawCrispText(window, headingShadow);
            heading.setFillColor(rarity);
            drawCrispText(window, heading);

            if (card)
            {
                drawCardFace(
                    collectionUi,
                    {{318.0f, ShopPanelY + 52.0f}, {164.0f, 232.0f}},
                    *card,
                    cardRarityLabel(*card),
                    rarity,
                    std::string());
                drawRarityRibbon(
                    collectionUi,
                    {{312.0f, ShopPanelY + 296.0f}, {176.0f, 30.0f}},
                    cardRarityLabel(*card),
                    rarity);
                drawText(
                    window,
                    font,
                    "Added to your collection",
                    12,
                    {312.0f, ShopPanelY + 334.0f},
                    palette::MutedDim,
                    176.0f);
            }
            else
            {
                drawBeveledPlate(
                    window,
                    {318.0f, ShopPanelY + 52.0f},
                    {164.0f, 232.0f},
                    sf::Color(18, 23, 23, 244),
                    palette::BrassBright,
                    true,
                    12.0f);
                drawText(window, font, *revealedCardTitle, 20, {330.0f, ShopPanelY + 160.0f},
                         palette::Ink, 140.0f);
            }
        }
        else
        {
            // Left: the pack as an object worth clicking. Right: what it contains.
            const sf::FloatRect pack{{150.0f, 138.0f}, {176.0f, 244.0f}};
            drawPackObject(collectionUi, pack, animationTime, false);

            // Price as struck coin under the pack, not a flat band across its face.
            const sf::FloatRect price{{pack.position.x + 18.0f, pack.position.y + pack.size.y + 22.0f},
                                      {pack.size.x - 36.0f, 34.0f}};
            drawBeveledPlate(
                window,
                price.position,
                price.size,
                sf::Color(30, 24, 14, 246),
                withAlpha(palette::BrassBright, playerCoins >= CardPackPrice ? 225 : 150),
                true,
                6.0f);
            sf::Text amount(font, std::to_string(CardPackPrice), 16);
            const float amountWidth = amount.getLocalBounds().size.x;
            const float groupLeft = price.position.x + (price.size.x - (amountWidth + 24.0f)) * 0.5f;
            drawCoin(collectionUi, {groupLeft + 9.0f, price.position.y + price.size.y * 0.5f}, 9.0f);
            drawText(
                window,
                font,
                std::to_string(CardPackPrice),
                16,
                {groupLeft + 24.0f, price.position.y + price.size.y * 0.5f - 11.0f},
                playerCoins >= CardPackPrice ? palette::BrassPale : palette::Bad);

            constexpr float InfoX = 366.0f;
            constexpr float InfoWidth = 306.0f;
            drawSectionHeading(collectionUi, {InfoX, 128.0f}, "Mystery Card", InfoWidth * 0.6f);
            drawWrappedText(
                window,
                font,
                "A single card drawn from the whole catalogue and sealed until you open it.",
                13,
                {InfoX, 168.0f},
                palette::Muted,
                InfoWidth,
                4.0f);

            // Odds as a designed table with the same rarity gems the rows use,
            // instead of one run-on sentence.
            drawCaption(collectionUi, {InfoX, 222.0f}, "PULL CHANCES");
            drawOddsTable(collectionUi, {{InfoX, 240.0f}, {InfoWidth, 66.0f}});

            drawInnerRule(collectionUi, {InfoX, 320.0f}, InfoWidth);
            drawWrappedText(
                window,
                font,
                "Within a rarity every card is equally likely. Starter cards come from "
                "the starter decks, never from a pack.",
                12,
                {InfoX, 334.0f},
                palette::MutedDim,
                InfoWidth,
                4.0f);

            // Collection progress: the reason to keep buying, and it fills what was
            // otherwise the panel's dead lower right corner.
            const int collected = static_cast<int>(cardLibrary.size());
            const int catalogue = std::max(collected, static_cast<int>(allCardLibrary.size()));
            if (catalogue > 0)
            {
                drawInnerRule(collectionUi, {InfoX, 390.0f}, InfoWidth);
                drawMeter(
                    collectionUi,
                    {InfoX, 406.0f},
                    {InfoWidth, 20.0f},
                    "COLLECTION",
                    std::to_string(collected) + " / " + std::to_string(catalogue) + " cards",
                    static_cast<float>(collected) / static_cast<float>(catalogue),
                    palette::BrassPale);
                drawCaption(
                    collectionUi,
                    {InfoX, 432.0f},
                    collected >= catalogue
                        ? "Every card in the catalogue is yours."
                        : std::to_string(catalogue - collected) + " still to find.",
                    InfoWidth);
            }
        }

        if (revealedCardTitle)
        {
            dismissRevealedCardButton.draw(window);
        }
        else
        {
            if (EnableCoinPurchases)
            {
                buyCoinPackButton.draw(window);
                refreshShopButton.draw(window);
            }
            shopStarterDecksButton.draw(window);
            if (playerCoins >= CardPackPrice && !pendingShopLoad)
            {
                buyCardButton.draw(window);
            }
            else
            {
                drawDisabledButton(
                    collectionUi,
                    buyCardButton.shape.getPosition(),
                    buyCardButton.shape.getSize(),
                    pendingShopLoad ? "Buy Card" : "Not Enough Coins");
            }
        }
        setMessageY(messageText, 562.0f);
        drawCrispText(window, messageText);
    };

