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

        const std::vector<std::pair<std::string, sf::Color>> details = deckEditorCardDetails(*card);

        sf::RectangleShape overlay({800.0f, 600.0f});
        overlay.setFillColor(sf::Color(0, 0, 0, 150));
        window.draw(overlay);

        drawPanel(window, {PiecePopupX, PiecePopupY}, {PiecePopupWidth, PiecePopupHeight});
        drawText(window, font, card->title, 24, {PiecePopupX + 22.0f, PiecePopupY + 18.0f},
                 sf::Color(248, 239, 216), PiecePopupWidth - 44.0f);

        drawBeveledPlate(
            window,
            {PiecePopupX + 22.0f, PiecePopupY + 62.0f},
            {104.0f, 104.0f},
            sf::Color(8, 14, 15),
            sf::Color(155, 111, 59),
            false,
            7.0f);
        if (sf::Texture* art = cardArtTexture(card->imagePath))
        {
            drawContainSprite(window, *art, {{PiecePopupX + 30.0f, PiecePopupY + 70.0f}, {88.0f, 88.0f}});
        }

        float y = PiecePopupY + 66.0f;
        const float statX = PiecePopupX + 146.0f;
        const bool hero = game_data::isHeroCard(*card);
        drawText(window, font, "Type: " + card->type + "   Location: Collection", 15, {statX, y},
                 hero ? sf::Color(248, 214, 112) : sf::Color(143, 220, 205), PiecePopupWidth - 174.0f);
        y += 24.0f;
        drawText(window, font, "Owned: " + std::to_string(ownedCopies(card->title)), 14, {statX, y},
                 sf::Color(248, 214, 112));
        y += 24.0f;
        if (hero)
        {
            drawText(window, font, "Hero cost: " + std::to_string(game_data::cardInt(*card, "heroCost", 0)),
                     14, {statX, y}, sf::Color(224, 210, 176));
        }
        else
        {
            drawText(window, font, "Cost: " + std::to_string(game_data::cardInt(*card, "cost", 0)) + " steam",
                     14, {statX, y}, sf::Color(150, 210, 235));
        }
        y += 22.0f;
        if (card->type == "Unit" || hero)
        {
            const game_data::GameCard gameCard = game_data::toGameCard(*card);
            drawText(window, font, "Health: " + std::to_string(gameCard.health),
                     14, {statX, y}, sf::Color(224, 210, 176));
            y += 22.0f;
            drawText(window, font, "Actions: " + std::to_string(gameCard.actions.size()),
                     14, {statX, y}, sf::Color(143, 220, 205), PiecePopupWidth - 174.0f);
        }
        else
        {
            drawText(window, font, "Effect: " + game_data::cardStr(*card, "effect", "none") +
                         "   Power: " + std::to_string(game_data::cardInt(*card, "power", 0)),
                     14, {statX, y}, sf::Color(224, 210, 176), PiecePopupWidth - 174.0f);
            y += 22.0f;
            drawText(window, font, "Target: " + game_data::cardStr(*card, "target", "none"),
                     14, {statX, y}, sf::Color(143, 220, 205), PiecePopupWidth - 174.0f);
        }

        inspectedDeckEditorCardScroll = std::clamp(
            inspectedDeckEditorCardScroll,
            0.0f,
            deckEditorCardDetailsMaxScroll(details));

        drawText(window, font, "Details", 17, {PiecePopupTextX, PiecePopupActionHeadingY}, sf::Color::White);

        drawBeveledPlate(
            window,
            {PiecePopupTextX, PiecePopupScrollY},
            {PiecePopupTextWidth, PiecePopupScrollHeight},
            sf::Color(8, 14, 15, 132),
            sf::Color(96, 66, 35, 150),
            false,
            7.0f);

        const sf::View previousView = window.getView();
        sf::View detailView(sf::FloatRect(
            {PiecePopupTextX, PiecePopupScrollY + inspectedDeckEditorCardScroll},
            {PiecePopupTextWidth, PiecePopupScrollHeight}));
        detailView.setViewport(sf::FloatRect(
            {PiecePopupTextX / 800.0f, PiecePopupScrollY / 600.0f},
            {PiecePopupTextWidth / 800.0f, PiecePopupScrollHeight / 600.0f}));
        window.setView(detailView);

        y = PiecePopupScrollY + 8.0f;
        for (const auto& [description, color] : details)
        {
            y = drawWrappedText(window, font, description, 14, {PiecePopupTextX + 8.0f, y}, color, PiecePopupTextWidth - 24.0f);
            y += 8.0f;
        }

        window.setView(previousView);

        const float maxScroll = deckEditorCardDetailsMaxScroll(details);
        if (maxScroll > 0.0f)
        {
            const float trackX = PiecePopupX + PiecePopupWidth - 22.0f;
            sf::RectangleShape track({4.0f, PiecePopupScrollHeight - 12.0f});
            track.setPosition({trackX, PiecePopupScrollY + 6.0f});
            track.setFillColor(sf::Color(73, 96, 98, 170));
            window.draw(track);

            const float thumbHeight = std::max(28.0f, track.getSize().y * (PiecePopupScrollHeight / (PiecePopupScrollHeight + maxScroll)));
            const float thumbY = track.getPosition().y +
                (track.getSize().y - thumbHeight) * (inspectedDeckEditorCardScroll / maxScroll);
            sf::RectangleShape thumb({4.0f, thumbHeight});
            thumb.setPosition({trackX, thumbY});
            thumb.setFillColor(sf::Color(143, 220, 205, 230));
            window.draw(thumb);
        }

        closeDeckCardPopupButton.draw(window);
    };

