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

        drawPanel(window, {24.0f, 78.0f}, {752.0f, 244.0f});
        drawText(window, font, "Tools", 22, {48.0f, 98.0f}, sf::Color::White);

        adminSandboxButton.draw(window);
        drawText(
            window,
            font,
            "Free-play board with every card in the database.",
            15,
            {292.0f, 158.0f},
            sf::Color(220, 224, 230),
            460.0f);
        drawText(
            window,
            font,
            "Place any card for either player.",
            15,
            {292.0f, 182.0f},
            sf::Color(220, 224, 230),
            460.0f);

        adminCardEditorButton.draw(window);
        drawText(
            window,
            font,
            "Create and edit cards and abilities on the card server.",
            15,
            {292.0f, 248.0f},
            sf::Color(220, 224, 230),
            460.0f);
        drawText(
            window,
            font,
            "Card server " + endpointText(clientConfig().card),
            13,
            {292.0f, 272.0f},
            sf::Color(148, 158, 176),
            460.0f);

        window.draw(messageText);
    };

