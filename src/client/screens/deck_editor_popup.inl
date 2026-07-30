    auto drawDeckEditorCardPopup = [&]() {
        if (!inspectedDeckEditorCardTitle)
        {
            return;
        }

        const card_data::Card* card = cardByTitle(*inspectedDeckEditorCardTitle);
        if (!card)
        {
            card = cardInAllLibraryByTitle(*inspectedDeckEditorCardTitle);
        }
        if (!card)
        {
            inspectedDeckEditorCardTitle.reset();
            inspectedDeckEditorCardScroll = 0.0f;
            return;
        }

        const DetailRows details = deckEditorAbilityRows(*card);
        const bool hero = game_data::isHeroCard(*card);

        sf::RectangleShape overlay({800.0f, 600.0f});
        overlay.setFillColor(sf::Color(0, 0, 0, 186));
        window.draw(overlay);

        drawPanel(window, {CardPopupX, CardPopupY}, {CardPopupWidth, CardPopupHeight});

        // The card as an object, not a list of fields with a thumbnail beside it.
        drawCardFace(
            collectionUi,
            {{CardPopupFaceX, CardPopupFaceY}, {CardPopupFaceWidth, CardPopupFaceHeight}},
            *card,
            cardRarityLabel(*card),
            cardRarityColor(*card),
            std::string());

        // Right column: rarity, then holdings as struck badges.
        float y = CardPopupFaceY + 4.0f;
        sf::Text rarity(
            gloomthornFontLoaded ? gloomthornFont : font,
            cardRarityLabel(*card) + (hero ? " Hero" : " " + card->type),
            19);
        rarity.setPosition({CardPopupStatsX, y});
        rarity.setFillColor(cardRarityColor(*card));
        drawCrispText(window, rarity);
        y += 26.0f;
        drawInnerRule(collectionUi, {CardPopupStatsX, y}, CardPopupStatsWidth);

        y += 12.0f;
        const int limit = game_data::cardDeckLimit(*card);
        const int inDeck = deckCopies(card->title);
        const auto statBadge = [&](sf::Vector2f position, const std::string& label,
                                   const std::string& value, sf::Color color) {
            drawBeveledPlate(
                window,
                position,
                {84.0f, 38.0f},
                sf::Color(12, 17, 18, 240),
                withAlpha(color, 190),
                false,
                5.0f);
            drawText(window, font, label, 9, {position.x + 10.0f, position.y + 7.0f}, palette::MutedDim, 64.0f);
            drawText(window, font, value, 15, {position.x + 10.0f, position.y + 18.0f}, color, 64.0f);
        };
        statBadge({CardPopupStatsX, y}, "OWNED",
                  std::to_string(ownedCopies(card->title)), palette::BrassPale);
        statBadge({CardPopupStatsX + 92.0f, y}, "IN DECK",
                  std::to_string(inDeck) + " / " + std::to_string(limit),
                  inDeck >= limit ? palette::BrassPale : palette::Muted);

        y += 46.0f;
        // Traits as the same measured chips the collection filter uses, so a long
        // trait list wraps instead of being elided into a single "Traits: ..." run.
        if (!card->traits.empty())
        {
            drawCaption(collectionUi, {CardPopupStatsX, y}, "TRAITS");
            std::vector<std::pair<std::string, int>> traitChips;
            traitChips.reserve(card->traits.size());
            for (const std::string& trait : card->traits)
            {
                traitChips.emplace_back(trait, 0);
            }
            y = drawTraitChips(collectionUi, {CardPopupStatsX, y + 14.0f}, CardPopupStatsWidth, traitChips) + 6.0f;
        }

        if (!card->keywords.empty())
        {
            drawCaption(collectionUi, {CardPopupStatsX, y}, "KEYWORDS");
            y = drawWrappedText(window, font, joinStrings(card->keywords, ", "), 12,
                                {CardPopupStatsX, y + 14.0f}, palette::Muted, CardPopupStatsWidth, 3.0f) + 8.0f;
        }

        // Rules text lives here rather than on the face: at 168px wide the face
        // could only show a clipped fragment of it.
        const std::string rules = game_data::cardStr(*card, "description", "");
        if (!rules.empty())
        {
            drawCaption(collectionUi, {CardPopupStatsX, y}, "RULES");
            drawWrappedText(window, font, rules, 12, {CardPopupStatsX, y + 14.0f},
                            palette::Ink, CardPopupStatsWidth, 3.0f);
        }

        const float maxScroll = deckEditorAbilityMaxScroll(details);
        inspectedDeckEditorCardScroll = std::clamp(inspectedDeckEditorCardScroll, 0.0f, maxScroll);

        drawCaption(collectionUi, {CardPopupAbilitiesX, CardPopupAbilitiesY - 16.0f}, "ABILITIES");
        drawBeveledPlate(
            window,
            {CardPopupAbilitiesX, CardPopupAbilitiesY},
            {CardPopupAbilitiesWidth, CardPopupAbilitiesHeight},
            sf::Color(8, 14, 15, 168),
            sf::Color(96, 66, 35, 170),
            false,
            6.0f);

        if (details.empty())
        {
            drawText(window, font, "This card has no activated abilities.", 12,
                     {CardPopupAbilitiesX + 20.0f, CardPopupAbilitiesY + 14.0f},
                     palette::MutedDim, CardPopupAbilitiesWidth - 40.0f);
        }
        else
        {
            // The old clip view built its viewport as a fraction of the whole
            // window, but the logical view is letterboxed inside that window, so
            // every row landed outside the box it was meant to be clipped to.
            // Composing with the base viewport puts it back, and keeping the view
            // rect anchored at PiecePopupTextX lets the shared ability renderer
            // draw into this box at 1:1 with no horizontal squeeze.
            const sf::View previousView = window.getView();
            const sf::FloatRect baseViewport = previousView.getViewport();
            const sf::Vector2f baseSize = previousView.getSize();
            const sf::Vector2f baseTopLeft = previousView.getCenter() - baseSize * 0.5f;

            sf::View abilityView(sf::FloatRect(
                {CardPopupAbilitiesX, CardPopupAbilitiesY + inspectedDeckEditorCardScroll},
                {CardPopupAbilitiesWidth, CardPopupAbilitiesHeight}));
            abilityView.setViewport(sf::FloatRect(
                {baseViewport.position.x +
                     (CardPopupAbilitiesX - baseTopLeft.x) / baseSize.x * baseViewport.size.x,
                 baseViewport.position.y +
                     (CardPopupAbilitiesY - baseTopLeft.y) / baseSize.y * baseViewport.size.y},
                {CardPopupAbilitiesWidth / baseSize.x * baseViewport.size.x,
                 CardPopupAbilitiesHeight / baseSize.y * baseViewport.size.y}));
            window.setView(abilityView);

            drawDetailRows(details, CardPopupAbilitiesY + PiecePopupScrollTextYInset);

            window.setView(previousView);
        }

        if (maxScroll > 0.0f)
        {
            const float trackX = CardPopupAbilitiesX + CardPopupAbilitiesWidth - 8.0f;
            sf::RectangleShape track({3.0f, CardPopupAbilitiesHeight - 12.0f});
            track.setPosition({trackX, CardPopupAbilitiesY + 6.0f});
            track.setFillColor(sf::Color(0, 0, 0, 170));
            window.draw(track);

            const float thumbHeight = std::max(
                20.0f,
                track.getSize().y * (CardPopupAbilitiesHeight / (CardPopupAbilitiesHeight + maxScroll)));
            sf::RectangleShape thumb({3.0f, thumbHeight});
            thumb.setPosition({
                trackX,
                track.getPosition().y +
                    (track.getSize().y - thumbHeight) * (inspectedDeckEditorCardScroll / maxScroll)});
            thumb.setFillColor(sf::Color(198, 146, 70, 225));
            window.draw(thumb);
        }

        closeDeckCardPopupButton.draw(window);
    };

