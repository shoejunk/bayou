    // Layout for the four faction panels laid out across the screen.
    constexpr float StarterDeckPanelY = 118.0f;
    constexpr float StarterDeckPanelWidth = 176.0f;
    constexpr float StarterDeckPanelHeight = 344.0f;
    constexpr float StarterDeckPanelGap = 12.0f;
    constexpr float StarterDeckPanelFirstX = 32.0f;

    auto starterDeckPanelX = [&](std::size_t index) {
        return StarterDeckPanelFirstX +
            static_cast<float>(index) * (StarterDeckPanelWidth + StarterDeckPanelGap);
    };

    auto starterDeckOfferAt = [&](sf::Vector2f position) -> std::optional<std::size_t> {
        for (std::size_t i = 0; i < starterDeckOffers.size(); ++i)
        {
            if (isInsideRect(
                    position,
                    starterDeckPanelX(i),
                    StarterDeckPanelY,
                    StarterDeckPanelWidth,
                    StarterDeckPanelHeight))
            {
                return i;
            }
        }
        return std::nullopt;
    };

    auto selectedStarterDeckOffered = [&]() -> const network::StarterDeckOffer* {
        if (!selectedStarterDeckOffer || *selectedStarterDeckOffer >= starterDeckOffers.size())
        {
            return nullptr;
        }
        return &starterDeckOffers[*selectedStarterDeckOffer];
    };

    auto starterDeckActionLabel = [&]() {
        const network::StarterDeckOffer* offer = selectedStarterDeckOffered();
        if (!offer)
        {
            return std::string("Claim Deck");
        }
        if (offer->owned)
        {
            return std::string("Already Owned");
        }
        return offer->price == 0
            ? std::string("Claim for Free")
            : "Buy for " + std::to_string(offer->price);
    };

    auto starterDeckActionEnabled = [&]() {
        const network::StarterDeckOffer* offer = selectedStarterDeckOffered();
        return offer && !offer->owned && playerCoins >= offer->price;
    };

    auto claimSelectedStarterDeck = [&]() {
        const network::StarterDeckOffer* offer = selectedStarterDeckOffered();
        if (!offer)
        {
            setMessage(messageText, "Select a starter deck first", sf::Color::Red);
            return;
        }
        if (offer->owned)
        {
            setMessage(messageText, "You already own " + offer->name, sf::Color::Red);
            return;
        }
        if (playerCoins < offer->price)
        {
            setMessage(
                messageText,
                "Need " + std::to_string(offer->price) + " coins to buy " + offer->name,
                sf::Color::Red);
            return;
        }

        setMessage(
            messageText,
            offer->price == 0 ? "Claiming deck..." : "Buying deck...",
            sf::Color::Yellow);
        pendingStarterDeckClaim =
            std::async(std::launch::async, claimStarterDeck, activeAccessToken, offer->name);
    };

    auto drawStarterDecks = [&]() {
        drawTitlePlaque(window, font, "Starter Decks", {148.0f, 46.0f}, {248.0f, 52.0f});
        drawText(window, font, "Signed in as " + signedInLabel(), 14, {300.0f, 24.0f}, sf::Color(178, 186, 202), 220.0f);

        drawCoinIcon({534.0f, 24.0f}, 14.0f);
        drawText(window, font, std::to_string(playerCoins), 18, {570.0f, 22.0f}, sf::Color(248, 239, 216), 80.0f);
        // The free pick cannot be skipped, so the only way out of it is signing
        // back out; the shop's starter deck store keeps a plain Back button.
        starterDeckBackButton.setLabel(starterDeckPickRequired ? "Log Out" : "Back");
        starterDeckBackButton.draw(window);

        drawText(
            window,
            font,
            starterDeckPickRequired
                ? "Pick one deck to keep for free. You can buy the others later."
                : "Each deck you do not already own costs " +
                    std::to_string(starter_decks::StarterDeckPrice) + " coins, once.",
            15,
            {32.0f, 88.0f},
            sf::Color(220, 224, 230),
            736.0f);

        for (std::size_t i = 0; i < starterDeckOffers.size(); ++i)
        {
            const network::StarterDeckOffer& offer = starterDeckOffers[i];
            const bool selected = selectedStarterDeckOffer && *selectedStarterDeckOffer == i;
            const sf::Vector2f position{starterDeckPanelX(i), StarterDeckPanelY};
            drawBeveledPlate(
                window,
                position,
                {StarterDeckPanelWidth, StarterDeckPanelHeight},
                offer.owned ? sf::Color(20, 32, 26, 244) : sf::Color(18, 23, 23, 244),
                selected ? sf::Color(232, 187, 83) : sf::Color(91, 86, 75, 200),
                selected,
                12.0f);

            const float nameBottom = drawWrappedText(
                window,
                font,
                offer.name,
                20,
                {position.x + 14.0f, position.y + 20.0f},
                sf::Color(248, 239, 216),
                StarterDeckPanelWidth - 28.0f);
            drawText(
                window,
                font,
                std::to_string(offer.cardCount) + " cards",
                15,
                {position.x + 14.0f, nameBottom + 12.0f},
                sf::Color(178, 186, 202),
                StarterDeckPanelWidth - 28.0f);

            std::string status;
            sf::Color statusColor(248, 214, 112);
            if (offer.owned)
            {
                status = "Owned";
                statusColor = sf::Color(120, 220, 150);
            }
            else if (offer.price == 0)
            {
                status = "Free";
            }
            else
            {
                status = std::to_string(offer.price) + " coins";
                if (playerCoins < offer.price)
                {
                    statusColor = sf::Color(224, 130, 110);
                }
            }
            drawText(
                window,
                font,
                status,
                20,
                {position.x + 14.0f, position.y + StarterDeckPanelHeight - 44.0f},
                statusColor,
                StarterDeckPanelWidth - 28.0f);
        }

        if (starterDeckOffers.empty() && !starterDecksBusy())
        {
            drawText(window, font, "No starter decks are available", 18, {268.0f, 280.0f}, sf::Color(178, 186, 202), 300.0f);
        }

        if (starterDeckActionEnabled())
        {
            claimStarterDeckButton.setLabel(starterDeckActionLabel());
            claimStarterDeckButton.draw(window);
        }
        else if (!starterDeckOffers.empty())
        {
            const sf::Vector2f position = claimStarterDeckButton.shape.getPosition();
            const sf::Vector2f size = claimStarterDeckButton.shape.getSize();
            drawBeveledPlate(
                window,
                position,
                size,
                sf::Color(42, 41, 38, 192),
                sf::Color(91, 86, 75, 180),
                false,
                std::clamp(size.y * 0.20f, 5.0f, 11.0f));
            sf::Text label(font, starterDeckActionLabel(), 20);
            label.setFillColor(sf::Color(168, 172, 172, 210));
            centerButtonText(label, position + size * 0.5f);
            window.draw(label);
        }
        window.draw(messageText);
    };
