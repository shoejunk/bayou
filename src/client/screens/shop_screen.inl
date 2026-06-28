    auto drawShop = [&]() {
        drawTitlePlaque(window, font, "Shop", {112.0f, 46.0f}, {176.0f, 52.0f});
        drawText(window, font, "Signed in as " + signedInLabel(), 14, {220.0f, 24.0f}, sf::Color(178, 186, 202), 280.0f);

        sf::CircleShape coin(14.0f);
        coin.setPosition({534.0f, 24.0f});
        coin.setFillColor(sf::Color(214, 158, 48));
        coin.setOutlineThickness(2.0f);
        coin.setOutlineColor(sf::Color(255, 225, 132));
        window.draw(coin);
        drawText(window, font, std::to_string(playerCoins), 18, {570.0f, 22.0f}, sf::Color(248, 239, 216), 80.0f);
        shopBackButton.draw(window);

        drawPanel(window, {170.0f, 96.0f}, {460.0f, 378.0f});

        const sf::Vector2f center{400.0f, 286.0f};
        if (pendingShopLoad)
        {
            drawText(window, font, "Loading shop...", 24, {306.0f, 270.0f}, sf::Color(248, 239, 216), 220.0f);
        }
        else if (revealedCardTitle)
        {
            const float t = animationTime - revealStartedAt;
            for (int i = 0; i < 4; ++i)
            {
                const float radius = 86.0f + static_cast<float>(i) * 24.0f + std::sin(t * 4.0f + static_cast<float>(i)) * 6.0f;
                sf::CircleShape glow(radius);
                glow.setOrigin({radius, radius});
                glow.setPosition(center);
                glow.setFillColor(sf::Color(229, 183, 83, static_cast<std::uint8_t>(34 - i * 6)));
                window.draw(glow);
            }

            for (int i = 0; i < 14; ++i)
            {
                const float angle = static_cast<float>(i) * 0.72f + t * 1.8f;
                const float radius = 132.0f + std::sin(t * 3.0f + static_cast<float>(i)) * 18.0f;
                sf::CircleShape sparkle(3.0f + static_cast<float>(i % 3));
                sparkle.setPosition({
                    center.x + std::cos(angle) * radius,
                    center.y + std::sin(angle) * radius * 0.72f});
                sparkle.setFillColor(sf::Color(248, 230, 150, 190));
                window.draw(sparkle);
            }

            drawText(window, font, "Revealed", 22, {350.0f, 112.0f}, sf::Color(248, 214, 112), 120.0f);
            if (const card_data::Card* card = cardInAllLibraryByTitle(*revealedCardTitle))
            {
                drawLargeCollectionCard(*card, {290.0f, 144.0f}, {220.0f, 300.0f});
            }
            else
            {
                drawBeveledPlate(
                    window,
                    {290.0f, 144.0f},
                    {220.0f, 300.0f},
                    sf::Color(18, 23, 23, 244),
                    sf::Color(232, 187, 83),
                    true,
                    12.0f);
                drawText(window, font, *revealedCardTitle, 22, {310.0f, 270.0f}, sf::Color(248, 239, 216), 180.0f);
            }
        }
        else
        {
            for (int i = 0; i < 3; ++i)
            {
                sf::CircleShape glow(86.0f + static_cast<float>(i) * 28.0f);
                glow.setOrigin({glow.getRadius(), glow.getRadius()});
                glow.setPosition(center);
                glow.setFillColor(sf::Color(42, 120, 112, static_cast<std::uint8_t>(34 - i * 8)));
                window.draw(glow);
            }

            drawBeveledPlate(
                window,
                {305.0f, 152.0f},
                {190.0f, 250.0f},
                sf::Color(22, 28, 28, 245),
                sf::Color(210, 154, 74),
                true,
                14.0f);

            sf::RectangleShape band({190.0f, 54.0f});
            band.setPosition({305.0f, 252.0f});
            band.setFillColor(sf::Color(93, 57, 28, 230));
            window.draw(band);

            drawText(window, font, "Mystery", 26, {345.0f, 190.0f}, sf::Color(248, 239, 216), 120.0f);
            drawText(window, font, "Card", 26, {370.0f, 222.0f}, sf::Color(248, 239, 216), 90.0f);
            drawText(window, font, "5 coins", 22, {362.0f, 265.0f}, sf::Color(248, 214, 112), 100.0f);
            drawText(window, font, "Odds: Common 70%  Rare 25%  Legendary 5%", 14, {248.0f, 412.0f}, sf::Color(248, 239, 216), 304.0f);
            drawText(window, font, "Cards inside each rarity are equally likely", 13, {278.0f, 436.0f}, sf::Color(190, 198, 214), 244.0f);
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
            buyCardButton.draw(window);
        }
        window.draw(messageText);
    };

