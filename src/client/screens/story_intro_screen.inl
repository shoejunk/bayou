    auto drawStoryIntro = [&]() {
        const std::array<std::string, 3> headings = {
            "Chapter 1: The Loose Valve",
            "Tinkering Tom Takes the Case",
            "Into the Marshworks"};
        const std::array<std::array<std::string, 3>, 3> captions = {{
            {{
                "The bayou engine coughs smoke across the midnight water.",
                "A warning bell clatters from the old marshworks.",
                "One small valve is rattling hard enough to shake the whole dock."}},
            {{
                "Tinkering Tom grabs his wrench and lantern.",
                "No crew is nearby, so Tom will have to cross the boards alone.",
                "A careful step is better than a heroic splash."}},
            {{
                "The repair hatch glows ahead.",
                "Move Tom one square at a time until he reaches the trouble.",
                "Click anywhere to start."}}
        }};

        drawTitlePlaque(
            window,
            font,
            headings[static_cast<std::size_t>(storyComicPage)],
            {310.0f, 50.0f},
            {520.0f, 58.0f});
        drawText(window, font, "Part " + std::to_string(storyComicPage + 1) + "/3", 16, {674.0f, 40.0f}, sf::Color(190, 198, 214), 90.0f);

        sf::Texture* tomArt = textures.load("cards/tinkering-tom.png");
        for (int i = 0; i < 3; ++i)
        {
            const sf::Vector2f panelPos{42.0f + static_cast<float>(i) * 250.0f, 96.0f};
            const sf::Vector2f panelSize{216.0f, 354.0f};
            drawBeveledPlate(
                window,
                panelPos,
                panelSize,
                sf::Color(43, 34, 24, 238),
                sf::Color(181, 126, 60),
                false,
                12.0f);

            drawBeveledPlate(
                window,
                panelPos + sf::Vector2f(12.0f, 14.0f),
                {panelSize.x - 24.0f, 190.0f},
                i == 0 ? sf::Color(30, 65, 68) : (i == 1 ? sf::Color(74, 53, 34) : sf::Color(37, 61, 47)),
                sf::Color(105, 75, 40),
                false,
                8.0f);

            for (int steam = 0; steam < 3; ++steam)
            {
                sf::CircleShape puff(16.0f + static_cast<float>(steam) * 7.0f);
                puff.setPosition({
                    panelPos.x + 34.0f + static_cast<float>(steam) * 45.0f,
                    panelPos.y + 36.0f + std::sin(animationTime * 1.8f + static_cast<float>(steam)) * 4.0f});
                puff.setFillColor(sf::Color(225, 231, 220, static_cast<std::uint8_t>(42 - steam * 8)));
                window.draw(puff);
            }

            if (tomArt)
            {
                const float tomX = panelPos.x + 60.0f + static_cast<float>(i) * 16.0f;
                drawContainSprite(window, *tomArt, {{tomX, panelPos.y + 66.0f}, {94.0f, 116.0f}});
            }

            if (storyComicPage == 2 && i == 2)
            {
                sf::CircleShape glow(30.0f);
                glow.setOrigin({30.0f, 30.0f});
                glow.setPosition({panelPos.x + 152.0f, panelPos.y + 116.0f});
                glow.setFillColor(sf::Color(244, 204, 92, 88));
                window.draw(glow);
                sf::RectangleShape valve({42.0f, 12.0f});
                valve.setOrigin({21.0f, 6.0f});
                valve.setPosition({panelPos.x + 152.0f, panelPos.y + 116.0f});
                valve.setFillColor(sf::Color(93, 64, 39));
                valve.setOutlineThickness(2.0f);
                valve.setOutlineColor(sf::Color(248, 214, 112));
                window.draw(valve);
            }

            drawBeveledPlate(
                window,
                panelPos + sf::Vector2f(12.0f, 222.0f),
                {panelSize.x - 24.0f, 112.0f},
                sf::Color(23, 20, 17, 238),
                sf::Color(105, 75, 40),
                false,
                7.0f);
            drawWrappedText(
                window,
                font,
                captions[static_cast<std::size_t>(storyComicPage)][static_cast<std::size_t>(i)],
                16,
                panelPos + sf::Vector2f(24.0f, 236.0f),
                sf::Color(246, 232, 200),
                panelSize.x - 48.0f,
                5.0f);
        }

        drawText(
            window,
            font,
            storyComicPage + 1 >= 3 ? "Click to start" : "Click to continue",
            18,
            {328.0f, 476.0f},
            sf::Color(248, 239, 216),
            180.0f);
        backButton.draw(window);
    };

