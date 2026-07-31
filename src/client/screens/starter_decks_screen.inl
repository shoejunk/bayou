    // Layout for the four faction tiles laid out across the screen.
    constexpr float StarterDeckPanelY = 114.0f;
    constexpr float StarterDeckPanelWidth = 176.0f;
    constexpr float StarterDeckPanelHeight = 362.0f;
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
        const std::string price = offer->price == 0
            ? "Free"
            : std::to_string(offer->price) + " Coins";
        const std::string& court = offer->name;
        if (offer->owned)
        {
            return court + " Owned";
        }
        const std::string verb = offer->price == 0 ? "Claim " : "Buy ";
        return verb + court + " - " + price;
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
        drawScreenHeader(collectionUi, "Starter Decks", signedInLabel(), playerCoins, 248.0f);
        // The free pick cannot be skipped, so the only way out of it is signing
        // back out; the shop's starter deck store keeps a plain Back button.
        starterDeckBackButton.setLabel(starterDeckPickRequired ? "Log Out" : "Back");
        starterDeckBackButton.draw(window);

        drawText(
            window,
            font,
            starterDeckPickRequired
                ? "Pick one court to keep for free. The others can be bought later."
                : "Each court you do not already own costs " +
                    std::to_string(starter_decks::StarterDeckPrice) + " coins, once.",
            14,
            {32.0f, 92.0f},
            palette::Muted,
            736.0f);

        for (std::size_t i = 0; i < starterDeckOffers.size(); ++i)
        {
            const network::StarterDeckOffer& offer = starterDeckOffers[i];
            const bool selected = selectedStarterDeckOffer && *selectedStarterDeckOffer == i;

            std::string status;
            sf::Color statusColor = palette::BrassPale;
            if (offer.owned)
            {
                status = "Owned";
                statusColor = palette::Good;
            }
            else if (offer.price == 0)
            {
                status = "Free";
            }
            else
            {
                status = std::to_string(offer.price);
                if (playerCoins < offer.price)
                {
                    statusColor = palette::Bad;
                }
            }

            // Four unstyled rectangles gave a player no sense of choosing between
            // factions. Each tile now carries its court's art, crest and strategy.
            drawFactionTile(
                collectionUi,
                {{starterDeckPanelX(i), StarterDeckPanelY}, {StarterDeckPanelWidth, StarterDeckPanelHeight}},
                factionStyleForDeckName(offer.name),
                offer.name,
                offer.cardCount,
                status,
                statusColor,
                offer.owned,
                selected,
                offer.price == 0 || playerCoins >= offer.price);
        }

        if (starterDeckOffers.empty() && !starterDecksBusy())
        {
            drawPanel(window, {200.0f, StarterDeckPanelY}, {400.0f, StarterDeckPanelHeight});
            drawEmptyState(
                collectionUi,
                {{216.0f, StarterDeckPanelY + 40.0f}, {368.0f, StarterDeckPanelHeight - 80.0f}},
                "No Courts Offered",
                "The card server has no starter decks configured yet.");
        }

        if (starterDeckActionEnabled())
        {
            claimStarterDeckButton.setLabel(starterDeckActionLabel());
            claimStarterDeckButton.draw(window);
        }
        else if (!starterDeckOffers.empty())
        {
            drawDisabledButton(
                collectionUi,
                claimStarterDeckButton.shape.getPosition(),
                claimStarterDeckButton.shape.getSize(),
                starterDeckActionLabel());
        }
        setMessageY(messageText, 562.0f);
        drawCrispText(window, messageText);
    };
