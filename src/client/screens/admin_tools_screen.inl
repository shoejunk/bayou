    auto drawAdminTools = [&]() {
        adminTabs.draw(window);
        drawText(
            window,
            font,
            "Signed in as " + signedInLabel(),
            14,
            {452.0f, 22.0f},
            sf::Color(178, 186, 202),
            212.0f);
        drawText(window, font, "Admin only", 14, {452.0f, 44.0f}, sf::Color(248, 214, 112), 212.0f);
        adminBackButton.draw(window);

        // Each tool becomes a discrete card rather than a button with two loose
        // sentences floating beside it, so the two entries read as a set.
        // No separate heading: the button beside it already names the tool, and
        // printing "Sandbox" twice on one card was pure duplication.
        const auto drawToolCard = [&](float top, const std::string& body) {
            drawBeveledPlate(
                window,
                {40.0f, top},
                {720.0f, 96.0f},
                sf::Color(17, 24, 25, 224),
                sf::Color(96, 68, 38),
                false,
                6.0f);
            drawWrappedText(window, font, body, 15, {288.0f, top + 30.0f},
                            sf::Color(214, 202, 176), 448.0f);
        };

        drawPanel(window, {24.0f, 78.0f}, {752.0f, 240.0f});
        drawText(window, font, "TOOLS", 12, {46.0f, 88.0f}, sf::Color(150, 132, 104));

        drawToolCard(112.0f,
                     "Free-play board with every card in the database. Place any card for "
                     "either player.");
        adminSandboxButton.draw(window);

        drawToolCard(212.0f,
                     "Create and edit cards, abilities and their stat fields on the card "
                     "server.");
        adminCardEditorButton.draw(window);

        // The card server endpoint used to be a loose grey sentence under the
        // Card Editor button. On an operator screen the addresses are worth
        // showing, but as a labelled block rather than prose.
        drawPanel(window, {24.0f, 334.0f}, {752.0f, 152.0f});
        drawText(window, font, "ENVIRONMENT", 12, {46.0f, 344.0f}, sf::Color(150, 132, 104));
        const auto drawEnvironmentRow = [&](float top, const std::string& key,
                                            const std::string& value) {
            drawText(window, font, key, 13, {46.0f, top}, sf::Color(181, 166, 137), 200.0f);
            drawText(window, font, value, 13, {250.0f, top}, sf::Color(226, 214, 186), 500.0f);
        };
        // Four rows only: a fifth at y=466 was cut by the panel's bottom border,
        // and it repeated the "Signed in as" line already in the header.
        drawEnvironmentRow(372.0f, "Account server", endpointText(clientConfig().account));
        drawEnvironmentRow(398.0f, "Matchmaking server", endpointText(clientConfig().matchmaking));
        drawEnvironmentRow(424.0f, "Card server", endpointText(clientConfig().card));
        drawEnvironmentRow(450.0f, "Game server", clientConfig().gameServerHost);

        window.draw(messageText);
    };

