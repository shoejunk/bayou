    auto drawStoryIntro = [&]() {
        const StoryMission& mission =
            storyMissions(storyCampaign)[static_cast<std::size_t>(storyMissionIndex)];
        const StoryPanel& panel = mission.briefing[static_cast<std::size_t>(storyComicPage)];
        const int panelCount = static_cast<int>(mission.briefing.size());
        const bool masteryCatchUpMayBeSkipped =
            activeStoryCatchUpMayBeSkipped();

        drawTitlePlaque(window, font, std::string(mission.title), {400.0f, 38.0f}, {704.0f, 58.0f});
        const bool storyOnly = mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly;
        const bool extraPractice = storyMissionIndex >= storyNarrativeEntryCount(storyCampaign);
        const bool storyEnding = storyOnly && storyMissionIndex + 1 ==
            storyNarrativeEntryCount(storyCampaign);
        // Narrative pages receive the full reading column. The compact goal
        // card appears only on the last briefing page, immediately before the
        // player chooses Start, so story prose never has to shrink merely to
        // repeat an objective that is not actionable yet.
        const bool showMissionGoal =
            !storyOnly && storyComicPage + 1 >= panelCount;
        const std::string actName(storyActName(storyCampaign, storyMissionIndex));
        if (!actName.empty())
        {
            drawText(
                window, font, actName, 14, {48.0f, 78.0f},
                sf::Color(236, 204, 132), 280.0f);
        }
        drawText(
            window,
            font,
            extraPractice
                ? "PRACTICE " + std::to_string(storyMissionIndex + 1 -
                    storyNarrativeEntryCount(storyCampaign)) + " OF " +
                    std::to_string(storyMissions(storyCampaign).size() -
                        storyNarrativeEntryCount(storyCampaign))
                : "STORY " + std::to_string(storyMissionIndex + 1) + " OF " +
                    std::to_string(storyNarrativeEntryCount(storyCampaign)),
            14,
            {48.0f, actName.empty() ? 82.0f : 96.0f},
            sf::Color(236, 204, 132),
            260.0f);
        // Show the current lesson here. Source chapter references belong in
        // author metadata, where they cannot distract a first-time player.
        drawText(
            window,
            font,
            std::string(storyCampaignName(storyCampaign)),
            10,
            {360.0f, 76.0f},
            sf::Color(190, 198, 214),
            392.0f);
        drawText(
            window,
            font,
            elideToWidth(
                font, std::string(mission.lesson), 10, 392.0f),
            10,
            {360.0f, 90.0f},
            sf::Color(190, 198, 214),
            392.0f);
        std::string actGoal(storyActShortGoal(storyCampaign, storyMissionIndex));
        if (actGoal.empty())
        {
            actGoal = std::string(storyActGoal(storyCampaign, storyMissionIndex));
        }
        if (!actGoal.empty())
        {
            drawBeveledPlate(
                window,
                {352.0f, 103.0f},
                {408.0f, 27.0f},
                sf::Color(12, 20, 21, 238),
                sf::Color(181, 126, 60),
                false,
                6.0f);
            drawWrappedText(
                window,
                font,
                "ACT GOAL: " + actGoal,
                11,
                {360.0f, 105.0f},
                sf::Color(236, 204, 132),
                392.0f,
                0.0f);
        }

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
        const std::string_view displayedArtPath = mission.scenarioArtPath.empty()
            ? panel.artPath
            : mission.scenarioArtPath;
        const bool hasPanelArt = !displayedArtPath.empty();
        if (hasPanelArt)
        {
            if (sf::Texture* art = textures.load(std::string(displayedArtPath)))
            {
                drawCoverSprite(
                    window,
                    *art,
                    {{artPosition.x + 14.0f, artPosition.y + 14.0f},
                     {artSize.x - 28.0f, artSize.y - 28.0f}});
            }
        }
        else
        {
            drawCenteredText(
                window, font, "STORY SCENE", 16,
                {artPosition.x + artSize.x * 0.5f, artPosition.y + artSize.y * 0.5f},
                sf::Color(181, 126, 60));
        }
        drawStorySpeakerPortraitInset(
            mission.scenarioArtPath,
            panel.artPath,
            {artPosition.x + 56.0f, artPosition.y + artSize.y - 56.0f},
            40.0f);

        const sf::Vector2f bubblePosition{360.0f, 132.0f};
        const sf::Vector2f bubbleSize{
            392.0f, showMissionGoal ? 246.0f : 380.0f};
        drawBeveledPlate(
            window,
            bubblePosition,
            bubbleSize,
            sf::Color(238, 226, 198, 248),
            sf::Color(105, 75, 40),
            false,
            11.0f);
        if (hasPanelArt)
        {
            sf::ConvexShape tail(3);
            tail.setPoint(0, {bubblePosition.x, bubblePosition.y + 86.0f});
            tail.setPoint(1, {bubblePosition.x - 27.0f, bubblePosition.y + 112.0f});
            tail.setPoint(2, {bubblePosition.x, bubblePosition.y + 128.0f});
            tail.setFillColor(sf::Color(238, 226, 198, 248));
            window.draw(tail);
        }

        sf::Font& panelSpeakerFont =
            gloomthornFontLoaded ? gloomthornFont : font;
        const std::string panelSpeaker(panel.speaker);
        const float panelSpeakerWidth = bubbleSize.x - 48.0f;
        unsigned int panelSpeakerSize = 23;
        sf::Text panelSpeakerMeasure(
            panelSpeakerFont, panelSpeaker, panelSpeakerSize);
        while (panelSpeakerSize > 15 &&
               panelSpeakerMeasure.getLocalBounds().size.x > panelSpeakerWidth)
        {
            --panelSpeakerSize;
            panelSpeakerMeasure.setCharacterSize(panelSpeakerSize);
        }
        drawText(
            window,
            panelSpeakerFont,
            panelSpeaker,
            panelSpeakerSize,
            bubblePosition + sf::Vector2f(24.0f, 20.0f),
            sf::Color(66, 43, 25),
            panelSpeakerWidth);
        const std::string panelBody(panel.text);
        const float panelBodyWidth = bubbleSize.x - 48.0f;
        const float panelBodyHeight = bubbleSize.y - 82.0f;
        unsigned int panelBodySize = showMissionGoal ? 18u : 20u;
        float panelLineGap = 7.0f;
        const auto wrappedPanelHeight = [&]() {
            const std::size_t lineCount =
                wrapText(font, panelBody, panelBodySize, panelBodyWidth).size();
            return lineCount == 0
                ? 0.0f
                : static_cast<float>(lineCount) *
                        (static_cast<float>(panelBodySize) + panelLineGap) -
                    panelLineGap;
        };
        while (panelBodySize > 15 && wrappedPanelHeight() > panelBodyHeight)
        {
            --panelBodySize;
            panelLineGap = std::max(3.0f, static_cast<float>(panelBodySize) * 0.3f);
        }
        drawWrappedText(
            window,
            font,
            panelBody,
            panelBodySize,
            bubblePosition + sf::Vector2f(24.0f, 64.0f),
            sf::Color(37, 31, 25),
            panelBodyWidth,
            panelLineGap);

        if (showMissionGoal)
        {
            const std::string survivorNames = storyRequiredSurvivorNames(mission);
            const bool hasRequiredSurvivors = !survivorNames.empty();
            const sf::Vector2f lessonPosition{
                360.0f, hasRequiredSurvivors ? 382.0f : 388.0f};
            const int playerInputCount =
                storyGuidedPlayerActionCount(mission);
            const bool firstGuidedMission = mission.id == "mw01_river_teeth";
            const std::string goalHeading = firstGuidedMission
                ? "FIRST BATTLE - FOLLOW THE GUIDE"
                : masteryCatchUpMayBeSkipped
                ? "PRACTICE COMPLETE - REPLAY IF YOU WANT"
                : mission.optionalRehearsal
                ? (mission.standardMatch
                    ? "OPTIONAL TIMED MATCH"
                    : mission.script.empty()
                    ? "OPTIONAL PRACTICE - YOUR PLAN"
                    : "OPTIONAL PRACTICE - " + std::to_string(playerInputCount) +
                        " STEPS")
                : mission.script.empty()
                    ? "BATTLE - CHOOSE YOUR OWN MOVES"
                    : "REQUIRED - " + std::to_string(playerInputCount) +
                        " GUIDED STEPS";
            const float lessonHeight = hasRequiredSurvivors ? 130.0f : 124.0f;
            constexpr float GoalTextWidth = 368.0f;
            constexpr float GoalTextBottomPadding = 8.0f;
            constexpr float GoalBlockGap = 5.0f;
            unsigned int survivorTextSize = 14;
            unsigned int objectiveTextSize = hasRequiredSurvivors ? 13 : 14;
            float survivorLineGap = 2.0f;
            float objectiveLineGap = 3.0f;
            const std::string survivorText = masteryCatchUpMayBeSkipped
                ? "IF YOU REPLAY, KEEP ALL ALLIES ALIVE."
                : firstGuidedMission
                    ? "MUST SURVIVE: all four crew members"
                    : "MUST SURVIVE: " + survivorNames;
            const std::string objectiveText = masteryCatchUpMayBeSkipped
                ? "Move on to the next battle, or practice these cards again."
                : firstGuidedMission
                    ? "Follow the glowing action. Keep all four friends alive. The guide will explain each move."
                    : std::string(mission.objective);
            const std::string guidanceNote = masteryCatchUpMayBeSkipped
                ? "You finished all seven practice battles. You can move on."
                : mission.optionalRehearsal
                ? (mission.standardMatch
                    ? "Try this match, or skip it before the clocks start."
                    : "Try this practice, or skip it. You can return from Missions.")
                : "Finish this battle to move on. Leaving now keeps it unfinished.";
            const auto wrappedBlockHeight = [&](const std::string& value,
                                                 unsigned int size,
                                                 float lineGap) {
                const std::size_t lines = wrapText(font, value, size, GoalTextWidth).size();
                return lines == 0
                    ? 0.0f
                    : static_cast<float>(lines) * static_cast<float>(size) +
                        static_cast<float>(lines - 1) * lineGap;
            };
            const float guidanceHeight = wrappedBlockHeight(guidanceNote, 11, 1.0f);
            const float survivorOffset = 34.0f + guidanceHeight + GoalBlockGap;
            const float objectiveWithoutSurvivorOffset =
                34.0f + guidanceHeight + GoalBlockGap;
            if (hasRequiredSurvivors)
            {
                const float availableHeight =
                    lessonHeight - survivorOffset - GoalTextBottomPadding;
                const auto combinedGoalHeight = [&]() {
                    return wrappedBlockHeight(
                               survivorText, survivorTextSize, survivorLineGap) +
                        GoalBlockGap +
                        wrappedBlockHeight(
                               objectiveText, objectiveTextSize, objectiveLineGap);
                };
                while (combinedGoalHeight() > availableHeight &&
                       (survivorTextSize > 11u || objectiveTextSize > 11u))
                {
                    const float survivorHeight = wrappedBlockHeight(
                        survivorText, survivorTextSize, survivorLineGap);
                    const float objectiveHeight = wrappedBlockHeight(
                        objectiveText, objectiveTextSize, objectiveLineGap);
                    if (objectiveTextSize > 11u &&
                        (objectiveHeight >= survivorHeight || survivorTextSize <= 11u))
                    {
                        --objectiveTextSize;
                    }
                    else if (survivorTextSize > 11u)
                    {
                        --survivorTextSize;
                    }
                }
            }
            else
            {
                const float availableHeight = lessonHeight -
                    objectiveWithoutSurvivorOffset - GoalTextBottomPadding;
                while (objectiveTextSize > 11u &&
                       wrappedBlockHeight(
                           objectiveText, objectiveTextSize, objectiveLineGap) >
                           availableHeight)
                {
                    --objectiveTextSize;
                }
            }
            drawBeveledPlate(
                window,
                lessonPosition,
                {392.0f, lessonHeight},
                sf::Color(22, 29, 28, 246),
                sf::Color(126, 163, 136),
                false,
                8.0f);
            drawText(
                window,
                font,
                goalHeading,
                14,
                lessonPosition + sf::Vector2f(18.0f, 12.0f),
                (mission.optionalRehearsal || masteryCatchUpMayBeSkipped)
                    ? sf::Color(146, 232, 166)
                    : sf::Color(255, 214, 104),
                350.0f);
            drawWrappedText(
                window,
                font,
                guidanceNote,
                11,
                lessonPosition + sf::Vector2f(18.0f, 33.0f),
                sf::Color(190, 198, 214),
                350.0f,
                1.0f);
            float objectiveOffset = objectiveWithoutSurvivorOffset;
            if (hasRequiredSurvivors)
            {
                const float survivorBottom = drawWrappedText(
                    window,
                    font,
                    survivorText,
                    survivorTextSize,
                    lessonPosition + sf::Vector2f(12.0f, survivorOffset),
                    sf::Color(255, 214, 104),
                    GoalTextWidth,
                    survivorLineGap);
                objectiveOffset =
                    survivorBottom - lessonPosition.y - survivorLineGap + GoalBlockGap;
            }
            const float objectiveBottom = drawWrappedText(
                window,
                font,
                objectiveText,
                objectiveTextSize,
                lessonPosition + sf::Vector2f(12.0f, objectiveOffset),
                sf::Color(236, 226, 203),
                GoalTextWidth,
                objectiveLineGap);
            const float objectiveGlyphBottom =
                objectiveBottom - objectiveLineGap;
            if (!captureValidationScreen.empty() &&
                objectiveGlyphBottom > lessonPosition.y + lessonHeight -
                    GoalTextBottomPadding + 0.5f)
            {
                failCaptureValidation(
                    "Story goal text extends below its bounded lesson panel.");
            }
        }

        const bool optionalSkipVisible =
            (mission.optionalRehearsal || masteryCatchUpMayBeSkipped) &&
            storyComicPage + 1 >= panelCount;
        const bool timedOptionalMatch = optionalSkipVisible && mission.standardMatch;
        storyContinueButton.setPosition(
            masteryCatchUpMayBeSkipped ? sf::Vector2f{538.0f, 520.0f}
                               : timedOptionalMatch ? sf::Vector2f{604.0f, 520.0f}
                               : sf::Vector2f{558.0f, 520.0f});
        storyContinueButton.setSize(
            masteryCatchUpMayBeSkipped ? sf::Vector2f{214.0f, 48.0f}
                               : timedOptionalMatch ? sf::Vector2f{148.0f, 48.0f}
                               : sf::Vector2f{194.0f, 48.0f});
        storySkipDrillButton.setPosition(
            timedOptionalMatch ? sf::Vector2f{298.0f, 520.0f}
                               : sf::Vector2f{326.0f, 520.0f});
        storySkipDrillButton.setSize(
            masteryCatchUpMayBeSkipped ? sf::Vector2f{196.0f, 48.0f}
                               : timedOptionalMatch ? sf::Vector2f{292.0f, 48.0f}
                               : sf::Vector2f{216.0f, 48.0f});
        storySkipDrillButton.setLabel(
            masteryCatchUpMayBeSkipped
                ? "Practice Again"
                : extraPractice
                ? (timedOptionalMatch ? "Skip Match" : "Skip Practice")
                : timedOptionalMatch
                ? "Skip Match - Continue Story"
                : "Skip Practice - Continue Story");
        storySkipDrillButton.setLabelSize(
            timedOptionalMatch || masteryCatchUpMayBeSkipped ? 14 : 12);
        storyBackButton.setFocused(
            storyKeyboardNavigationActive && storyIntroKeyboardFocus == 0);
        storySkipDrillButton.setFocused(
            storyKeyboardNavigationActive && optionalSkipVisible &&
            storyIntroKeyboardFocus == 1);
        storyContinueButton.setFocused(
            storyKeyboardNavigationActive && storyIntroKeyboardFocus == 2);
        storyContinueButton.setLabel(
            storyComicPage + 1 >= panelCount
                ? (storyOnly ? (storyEnding ? "Finish Story" : "Continue Story") :
                    timedOptionalMatch ? "Start Timed Match" :
                    masteryCatchUpMayBeSkipped ? "Continue Story" :
                    mission.optionalRehearsal ? "Start Practice" : "Start Battle")
                : "Continue");
        storyContinueButton.setLabelSize(
            masteryCatchUpMayBeSkipped
                ? 12
                : timedOptionalMatch
                ? 14
                : storyComicPage + 1 >= panelCount
                ? type::Body
                : type::Subheading);
        storyContinueButton.draw(window, animationTime);
        if (optionalSkipVisible)
        {
            storySkipDrillButton.draw(window, animationTime);
        }
        storyBackButton.setLabel(storyComicPage > 0 ? "Previous" : "Missions");
        storyBackButton.draw(window, animationTime);
        const float progressCenterX = optionalSkipVisible ? 242.0f : 400.0f;
        drawCenteredText(
            window,
            font,
            "BRIEFING " + std::to_string(storyComicPage + 1) + " OF " +
                std::to_string(panelCount),
            type::Caption,
            {progressCenterX, 548.0f},
            palette::InkMuted);
        const float dotsWidth = static_cast<float>(std::max(0, panelCount - 1)) * 14.0f;
        for (int page = 0; page < panelCount; ++page)
        {
            sf::CircleShape dot(2.5f);
            dot.setPosition({progressCenterX - dotsWidth * 0.5f + static_cast<float>(page) * 14.0f, 564.0f});
            dot.setFillColor(page == storyComicPage ? sf::Color(218, 166, 78) : sf::Color(82, 88, 84));
            window.draw(dot);
        }
        drawCenteredText(
            window,
            font,
            optionalSkipVisible
                ? "LEFT/RIGHT: PAGE  |  TAB: CHOICE  |  ENTER: ACTIVATE  |  ESC: MISSIONS"
                : "LEFT/RIGHT: PAGE  |  ENTER: CONTINUE  |  ESC: MISSIONS",
            13,
            {400.0f, 588.0f},
            palette::Ink);
    };
