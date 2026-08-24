    auto drawStoryIntro = [&]() {
        const StoryMission& mission =
            storyMissions(storyCampaign)[static_cast<std::size_t>(storyMissionIndex)];
        const StoryPanel& panel = mission.briefing[static_cast<std::size_t>(storyComicPage)];

        drawTitlePlaque(window, font, std::string(mission.title), {214.0f, 38.0f}, {572.0f, 58.0f});
        drawText(
            window,
            font,
            "MISSION " + std::to_string(storyMissionIndex + 1) + " / " +
                std::to_string(storyMissions(storyCampaign).size()),
            13,
            {42.0f, 45.0f},
            sf::Color(198, 146, 70),
            150.0f);
        drawText(
            window,
            font,
            std::string(storyCampaignName(storyCampaign)) + "  |  " +
                std::string(mission.sourceChapter),
            12,
            {470.0f, 92.0f},
            sf::Color(190, 198, 214),
            285.0f);

        const sf::Vector2f artPosition{48.0f, 112.0f};
        const sf::Vector2f artSize{280.0f, 342.0f};
        drawBeveledPlate(
            window,
            artPosition,
            artSize,
            sf::Color(28, 37, 35, 244),
            sf::Color(181, 126, 60),
            false,
            10.0f);
        if (sf::Texture* art = textures.load(std::string(panel.artPath)))
        {
            drawContainSprite(
                window,
                *art,
                {{artPosition.x + 14.0f, artPosition.y + 14.0f},
                 {artSize.x - 28.0f, artSize.y - 28.0f}});
        }

        const sf::Vector2f bubblePosition{360.0f, 132.0f};
        const sf::Vector2f bubbleSize{392.0f, 246.0f};
        drawBeveledPlate(
            window,
            bubblePosition,
            bubbleSize,
            sf::Color(238, 226, 198, 248),
            sf::Color(105, 75, 40),
            false,
            11.0f);
        sf::ConvexShape tail(3);
        tail.setPoint(0, {bubblePosition.x, bubblePosition.y + 86.0f});
        tail.setPoint(1, {bubblePosition.x - 27.0f, bubblePosition.y + 112.0f});
        tail.setPoint(2, {bubblePosition.x, bubblePosition.y + 128.0f});
        tail.setFillColor(sf::Color(238, 226, 198, 248));
        window.draw(tail);

        drawText(
            window,
            gloomthornFontLoaded ? gloomthornFont : font,
            std::string(panel.speaker),
            23,
            bubblePosition + sf::Vector2f(24.0f, 20.0f),
            sf::Color(66, 43, 25),
            bubbleSize.x - 48.0f);
        drawWrappedText(
            window,
            font,
            std::string(panel.text),
            18,
            bubblePosition + sf::Vector2f(24.0f, 64.0f),
            sf::Color(37, 31, 25),
            bubbleSize.x - 48.0f,
            7.0f);

        const sf::Vector2f lessonPosition{360.0f, 400.0f};
        drawBeveledPlate(
            window,
            lessonPosition,
            {392.0f, 104.0f},
            sf::Color(22, 29, 28, 246),
            sf::Color(126, 163, 136),
            false,
            8.0f);
        drawText(
            window,
            font,
            std::string(mission.lesson),
            15,
            lessonPosition + sf::Vector2f(18.0f, 14.0f),
            sf::Color(146, 232, 166),
            350.0f);
        drawWrappedText(
            window,
            font,
            std::string(mission.objective),
            14,
            lessonPosition + sf::Vector2f(18.0f, 44.0f),
            sf::Color(236, 226, 203),
            350.0f,
            4.0f);

        storyContinueButton.setLabel(storyComicPage + 1 >= 3 ? "Deploy" : "Continue");
        storyContinueButton.draw(window, animationTime);
        storyBackButton.setLabel("Missions");
        storyBackButton.draw(window, animationTime);
        for (int page = 0; page < 3; ++page)
        {
            sf::CircleShape dot(4.0f);
            dot.setPosition({388.0f + static_cast<float>(page) * 18.0f, 548.0f});
            dot.setFillColor(page == storyComicPage ? sf::Color(218, 166, 78) : sf::Color(82, 88, 84));
            window.draw(dot);
        }
    };
