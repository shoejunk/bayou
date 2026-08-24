    auto drawStoryMissionSelect = [&]() {
        const auto& missions = storyMissions(storyCampaign);
        const int completed = storyCampaignProgress[storyProgressIndex(storyCampaign)];
        const int missionCount = static_cast<int>(missions.size());
        const int unlockedCount =
            std::min(missionCount, completed + (completed < missionCount ? 1 : 0));

        drawTitlePlaque(
            window,
            font,
            std::string(storyCampaignName(storyCampaign)),
            {400.0f, 42.0f},
            {430.0f, 58.0f});
        drawCenteredText(
            window,
            font,
            "Choose a completed chapter to replay, or continue the current story.",
            type::Body,
            {400.0f, 88.0f},
            palette::InkMuted);

        for (std::size_t index = 0; index < missions.size(); ++index)
        {
            Button& button = storyMissionButtons[index];
            const bool unlocked = static_cast<int>(index) < unlockedCount;
            const bool complete = static_cast<int>(index) < completed;
            button.setEnabled(unlocked);
            button.setLabel(
                std::to_string(index + 1) + ". " + std::string(missions[index].title));
            button.draw(window, animationTime);

            const sf::Vector2f position = button.shape.getPosition();
            const sf::Vector2f size = button.shape.getSize();
            const std::string status = complete ? "COMPLETED" : unlocked ? "NEXT" : "LOCKED";
            const sf::Color statusColor = complete
                ? sf::Color(103, 188, 153)
                : unlocked ? sf::Color(226, 164, 74) : sf::Color(116, 118, 116);
            drawText(
                window,
                font,
                status,
                type::Caption,
                {position.x + size.x - 100.0f, position.y + 8.0f},
                statusColor,
                82.0f);
        }

        storyMissionSelectBackButton.draw(window, animationTime);
        storyRestartCampaignButton.draw(window, animationTime);
    };
