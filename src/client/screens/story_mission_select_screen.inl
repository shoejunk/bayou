    auto drawStoryMissionSelect = [&]() {
        const auto& missions = storyMissions(storyCampaign);
        const std::size_t progressIndex = storyProgressIndex(storyCampaign);
        const int completed = storyCampaignProgress[progressIndex];
        const StoryProgress& progress = storyCampaignProgressDetails[progressIndex];
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
            {400.0f, 31.0f},
            {430.0f, 48.0f});
        drawCenteredText(
            window,
            font,
            completed == 0
                ? (storyCampaign == StoryCampaign::Mirewatch
                    ? "Start aboard Telos's skiff as three gators attack."
                    : storyCampaign == StoryCampaign::Seelie
                        ? "Begin with the surviving Cathedral road, then defend the last lights."
                        : "Start with the first playable Blackthorn mission.")
                : "Replay an earlier entry, or continue the current one.",
            type::Body,
            {400.0f, 65.0f},
            palette::InkMuted);
        if (missionCount > 0)
        {
            const int currentIndex = std::min(completed, missionCount - 1);
            const std::string currentAct(storyActName(storyCampaign, currentIndex));
            if (!currentAct.empty())
            {
                drawCenteredText(
                    window,
                    font,
                    currentAct,
                    14,
                    {400.0f, 83.0f},
                    sf::Color(236, 204, 132));
                drawBeveledPlate(
                    window,
                    {84.0f, 96.0f},
                    {632.0f, 30.0f},
                    sf::Color(12, 20, 21, 238),
                    sf::Color(181, 126, 60),
                    false,
                    7.0f);
                drawCenteredText(
                    window,
                    font,
                    "CURRENT GOAL: " + std::string(storyActShortGoal(storyCampaign, currentIndex)),
                    13,
                    {400.0f, 111.0f},
                    sf::Color(222, 226, 218));
            }
        }

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
            const bool completedByPlay = index < completed &&
                static_cast<std::size_t>(index) < progress.completedByPlay.size() &&
                progress.completedByPlay[static_cast<std::size_t>(index)];
            const bool continuedWithoutMastery =
                progress.continuedWithoutMastery(index);
            const bool current = completed < missionCount && index == completed;
            button.setEnabled(unlocked);
            button.setFocused(
                storyKeyboardNavigationActive &&
                storyMissionKeyboardFocus == slot);
            button.setVariant(current ? ButtonVariant::Primary : ButtonVariant::Secondary);
            button.setLabel(unlocked
                ? std::to_string(index + 1) + ". " +
                    std::string(missions[static_cast<std::size_t>(index)].title)
                : std::to_string(index + 1) + ". LOCKED ENTRY");
            button.draw(window, animationTime);
            if (!unlocked)
            {
                // Keep the disabled entry legible without revealing later story
                // beats before the player earns them.
                sf::Text lockedTitle = button.text;
                lockedTitle.setFillColor(sf::Color(176, 172, 158, 232));
                drawCrispText(window, lockedTitle);
            }

            const sf::Vector2f position = button.shape.getPosition();
            const sf::Vector2f size = button.shape.getSize();
            const bool storyScene =
                missions[static_cast<std::size_t>(index)].objectiveSpec.kind ==
                StoryObjectiveKind::StoryOnly;
            const bool guided =
                !storyScene &&
                !missions[static_cast<std::size_t>(index)].script.empty();
            const bool masteryCatchUpMayBeSkipped =
                storyMasteryCatchUpMayBeSkipped(missions, index, progress);
            const bool optionalDrill =
                missions[static_cast<std::size_t>(index)].optionalRehearsal ||
                masteryCatchUpMayBeSkipped;
            const std::string typeLabel =
                storyScene ? "STORY" : masteryCatchUpMayBeSkipped ? "OPTIONAL REPLAY" :
                    optionalDrill ? "RECOMMENDED DRILL" :
                    guided ? "GUIDED PLAY" : "OPEN BATTLE";
            const float typeWidth = storyScene ? 58.0f : masteryCatchUpMayBeSkipped ? 112.0f :
                optionalDrill ? 128.0f :
                guided ? 86.0f : 91.0f;
            const sf::Color typeColor = storyScene
                ? sf::Color(150, 184, 218)
                : guided ? sf::Color(214, 190, 113) : sf::Color(146, 232, 166);
            drawBeveledPlate(
                window,
                {position.x + 10.0f, position.y + 7.0f},
                {typeWidth, 19.0f},
                sf::Color(23, 31, 32, 230),
                typeColor,
                false,
                5.0f);
            drawCenteredText(
                window,
                font,
                typeLabel,
                11,
                {position.x + 10.0f + typeWidth * 0.5f, position.y + 16.0f},
                typeColor);
            const std::string status = completedByPlay
                ? "COMPLETED"
                : continuedWithoutMastery
                    ? (optionalDrill ? "SKIPPED" : "CONTINUED")
                    : current && index == 0
                        ? "BEGIN"
                        : unlocked ? "CONTINUE" : "LOCKED";
            const sf::Color statusColor = completedByPlay
                ? sf::Color(103, 188, 153)
                : continuedWithoutMastery
                    ? sf::Color(150, 184, 218)
                    : unlocked ? sf::Color(226, 164, 74) : sf::Color(166, 168, 164);
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
        storyMissionSelectBackButton.setFocused(
            storyKeyboardNavigationActive && storyMissionKeyboardFocus == 8);
        storyMissionPreviousPageButton.setFocused(
            storyKeyboardNavigationActive && storyMissionKeyboardFocus == 9);
        storyMissionNextPageButton.setFocused(
            storyKeyboardNavigationActive && storyMissionKeyboardFocus == 10);
        storyRestartCampaignButton.setFocused(
            storyKeyboardNavigationActive && storyMissionKeyboardFocus == 11);
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
        drawCenteredText(
            window,
            font,
            "ARROWS/TAB: ENTRY  |  ENTER: OPEN  |  PAGE UP/DOWN: PAGE",
            13,
            {400.0f, 588.0f},
            palette::Ink);
    };
