    auto drawStoryIntro = [&]() {
        const StoryMission& mission =
            storyMissions(storyCampaign)[static_cast<std::size_t>(storyMissionIndex)];
        const StoryPanel& panel = mission.briefing[static_cast<std::size_t>(storyComicPage)];

        drawTitlePlaque(window, font, std::string(mission.title), {214.0f, 38.0f}, {572.0f, 58.0f});
        const bool storyOnly = mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly;
        drawText(
            window,
            font,
            "ENTRY " + std::to_string(storyMissionIndex + 1) + " OF " +
                std::to_string(storyMissions(storyCampaign).size()),
            13,
            {42.0f, 45.0f},
            sf::Color(198, 146, 70),
            150.0f);
        drawWrappedText(
            window,
            font,
            std::string(storyCampaignName(storyCampaign)) + "  |  " +
                std::string(mission.sourceChapter),
            12,
            {452.0f, 82.0f},
            sf::Color(190, 198, 214),
            300.0f,
            2.0f);

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
            drawCoverSprite(
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
        int playerInputCount = 0;
        for (const StoryScriptAction& step : mission.script)
        {
            if (step.owner == 1)
            {
                ++playerInputCount;
            }
        }
        const std::string goalHeading = storyOnly
            ? "STORY CHAPTER - NO BOARD ACTIONS"
            : "MISSION GOAL - " + std::to_string(playerInputCount) +
                " GUIDED ACTIONS";
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
            goalHeading,
            14,
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

        const int panelCount = static_cast<int>(mission.briefing.size());
        storyContinueButton.setLabel(
            storyComicPage + 1 >= panelCount
                ? (storyOnly ? "Continue Story" : "Start Mission")
                : "Continue");
        storyContinueButton.draw(window, animationTime);
        storyBackButton.setLabel(storyComicPage > 0 ? "Previous" : "Missions");
        storyBackButton.draw(window, animationTime);
        drawCenteredText(
            window,
            font,
            "BRIEFING " + std::to_string(storyComicPage + 1) + " OF " +
                std::to_string(panelCount),
            type::Caption,
            {400.0f, 548.0f},
            palette::InkMuted);
        const float dotsWidth = static_cast<float>(std::max(0, panelCount - 1)) * 14.0f;
        for (int page = 0; page < panelCount; ++page)
        {
            sf::CircleShape dot(2.5f);
            dot.setPosition({400.0f - dotsWidth * 0.5f + static_cast<float>(page) * 14.0f, 564.0f});
            dot.setFillColor(page == storyComicPage ? sf::Color(218, 166, 78) : sf::Color(82, 88, 84));
            window.draw(dot);
        }
    };
