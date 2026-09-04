    auto drawStoryMissionSelect = [&]() {
        const auto& missions = storyMissions(storyCampaign);
        const int completed = storyCampaignProgress[storyProgressIndex(storyCampaign)];
        const int missionCount = static_cast<int>(missions.size());
        const int unlockedCount =
            std::min(missionCount, completed + (completed < missionCount ? 1 : 0));
        const int pageCount =
            std::max(1, (missionCount + StoryMissionPageSize - 1) / StoryMissionPageSize);
        storyMissionPage = std::clamp(storyMissionPage, 0, pageCount - 1);

        drawTitlePlaque(
            window,
            font,
            std::string(storyCampaignName(storyCampaign)),
            {400.0f, 42.0f},
            {430.0f, 58.0f});
        drawCenteredText(
            window,
            font,
            completed == 0
                ? (storyCampaign == StoryCampaign::Mirewatch
                    ? "Start aboard Telos's skiff as three gators attack."
                    : "Start with the first playable Blackthorn mission.")
                : "Replay a completed entry, or continue the current one.",
            type::Body,
            {400.0f, 88.0f},
            palette::InkMuted);

        for (int slot = 0; slot < StoryMissionPageSize; ++slot)
        {
            Button& button = storyMissionButtons[static_cast<std::size_t>(slot)];
            const int index = storyMissionPage * StoryMissionPageSize + slot;
            if (index >= missionCount)
            {
                button.hovered = false;
                continue;
            }
            const bool unlocked = index < unlockedCount;
            const bool complete = index < completed;
            const bool current = completed < missionCount && index == completed;
            button.setEnabled(unlocked);
            button.setVariant(current ? ButtonVariant::Primary : ButtonVariant::Secondary);
            button.setLabel(
                std::to_string(index + 1) + ". " +
                std::string(missions[static_cast<std::size_t>(index)].title));
            button.draw(window, animationTime);

            const sf::Vector2f position = button.shape.getPosition();
            const sf::Vector2f size = button.shape.getSize();
            const std::string status = complete
                ? "COMPLETED"
                : current && index == 0
                    ? "BEGIN"
                    : unlocked ? "CONTINUE" : "LOCKED";
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

        storyMissionPreviousPageButton.setEnabled(storyMissionPage > 0);
        storyMissionNextPageButton.setEnabled(storyMissionPage + 1 < pageCount);
        storyMissionPreviousPageButton.draw(window, animationTime);
        storyMissionNextPageButton.draw(window, animationTime);
        drawCenteredText(
            window,
            font,
            "Page " + std::to_string(storyMissionPage + 1) + " of " +
                std::to_string(pageCount),
            type::Caption,
            {443.0f, 544.0f},
            palette::InkMuted);
        storyMissionSelectBackButton.draw(window, animationTime);
        const int nextMission = std::min(completed, missionCount - 1);
        storyRestartCampaignButton.setVariant(ButtonVariant::Primary);
        storyRestartCampaignButton.setLabel(
            completed < missionCount
                ? completed == 0
                    ? "Begin Entry 1"
                    : "Continue Entry " + std::to_string(nextMission + 1)
                : "Replay Finale");
        storyRestartCampaignButton.draw(window, animationTime);
    };
