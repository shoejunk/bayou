    auto drawStorySelect = [&]() {
        storyBlackthornButton.setFocused(
            storyKeyboardNavigationActive && !storySpoilerConfirmationVisible &&
            storySelectKeyboardFocus == 0);
        storyMirewatchButton.setFocused(
            storyKeyboardNavigationActive && !storySpoilerConfirmationVisible &&
            storySelectKeyboardFocus == 1);
        storySeelieButton.setFocused(
            storyKeyboardNavigationActive && !storySpoilerConfirmationVisible &&
            storySelectKeyboardFocus == 2);
        storySelectBackButton.setFocused(
            storyKeyboardNavigationActive && !storySpoilerConfirmationVisible &&
            storySelectKeyboardFocus == 3);
        drawTitlePlaque(window, font, "Choose a Story", {400.0f, 48.0f}, {430.0f, 64.0f});
        drawCenteredText(
            window,
            font,
            "Start with Mirewatch; progress saves per path. Themes: coercion, injury, war, and death.",
            type::Caption,
            {400.0f, 94.0f},
            palette::InkMuted);

        const auto drawCampaign = [&](StoryCampaign campaign,
                                      sf::Vector2f position,
                                      sf::Color accent,
                                      std::string_view artPath,
                                      std::string_view summary,
                                      Button& button) {
            constexpr sf::Vector2f panelSize{224.0f, 396.0f};
            drawBeveledPlate(
                window,
                position,
                panelSize,
                sf::Color(20, 29, 29, 246),
                accent,
                campaign == StoryCampaign::Mirewatch,
                8.0f);

            if (sf::Texture* art = textures.load(std::string(artPath)))
            {
                drawContainSprite(
                    window,
                    *art,
                    {{position.x + 16.0f, position.y + 16.0f}, {192.0f, 112.0f}});
            }

            drawWrappedText(
                window,
                gloomthornFontLoaded ? gloomthornFont : font,
                std::string(storyCampaignName(campaign)),
                23,
                position + sf::Vector2f(18.0f, 140.0f),
                palette::Ink,
                188.0f,
                4.0f);
            drawWrappedText(
                window,
                font,
                std::string(summary),
                14,
                position + sf::Vector2f(18.0f, 213.0f),
                palette::InkMuted,
                188.0f,
                4.0f);
            if (campaign == StoryCampaign::Mirewatch)
            {
                drawBeveledPlate(
                    window,
                    position + sf::Vector2f(18.0f, 180.0f),
                    {188.0f, 25.0f},
                    sf::Color(18, 54, 45, 248),
                    accent,
                    true,
                    6.0f);
                drawCenteredText(
                    window,
                    font,
                    "START HERE - RECOMMENDED",
                    11,
                    position + sf::Vector2f(112.0f, 192.5f),
                    sf::Color(225, 255, 238));
            }
            else
            {
                const std::string pathLabel = campaign == StoryCampaign::Seelie
                    ? "BOOKS TWO & THREE - SPOILERS"
                    : "TACTICAL - TRAPS AND DAMAGE";
                drawText(
                    window,
                    font,
                    pathLabel,
                    11,
                    position + sf::Vector2f(18.0f, 184.0f),
                    palette::InkMuted,
                    188.0f);
            }

            drawSeparatorRule(
                window,
                {position.x + 18.0f, position.y + 300.0f},
                panelSize.x - 40.0f,
                false);

            const int completed = storyCampaignProgress[storyProgressIndex(campaign)];
            const int missionCount = static_cast<int>(storyMissions(campaign).size());
            const std::string progress = completed >= missionCount
                ? "Story ending reached"
                : completed == 0
                    ? (campaign == StoryCampaign::Mirewatch
                        ? "River Teeth ready"
                        : campaign == StoryCampaign::Seelie
                            ? "Cathedral road ready"
                            : "First mission ready")
                    : "Entry " + std::to_string(completed + 1) + " ready";
            drawText(
                window,
                font,
                progress,
                type::Body,
                position + sf::Vector2f(18.0f, 311.0f),
                accent,
                panelSize.x - 44.0f);
            drawText(
                window,
                font,
                std::to_string(completed) + " / " + std::to_string(missionCount) + " entries advanced",
                type::Caption,
                position + sf::Vector2f(18.0f, 334.0f),
                palette::InkMuted,
                panelSize.x - 44.0f);

            button.setLabel(campaign == StoryCampaign::Blackthorn
                ? "Play Blackthorn"
                : campaign == StoryCampaign::Mirewatch
                    ? "Play Mirewatch"
                    : "Play Seelie");
            button.draw(window, animationTime);
        };

        drawCampaign(
            StoryCampaign::Blackthorn,
            {32.0f, 112.0f},
            sf::Color(207, 151, 69),
            "cards/victorGreyshard.png",
            "Audit Victor's Company. Learn traps, marks, and heavy attacks without changing the novel's outcomes.",
            storyBlackthornButton);
        drawCampaign(
            StoryCampaign::Mirewatch,
            {288.0f, 112.0f},
            sf::Color(103, 188, 153),
            "cards/reedBaelstone.png",
            "Lead Reed's resistance. Win Mirewatch's freedom through healing, movement, and teamwork.",
            storyMirewatchButton);
        drawCampaign(
            StoryCampaign::Seelie,
            {544.0f, 112.0f},
            sf::Color(160, 139, 224),
            "cards/princeVesper.png",
            "Contains the Book One ending. Begin with Vesper's surviving Cathedral road, then defend the small lights and Emberhaven.",
            storySeelieButton);

        storySelectBackButton.draw(window, animationTime);
        drawCenteredText(
            window,
            font,
            "ARROWS/TAB: MOVE  |  ENTER: CHOOSE  |  ESC: BACK",
            13,
            {400.0f, 588.0f},
            palette::Ink);

        if (storySpoilerConfirmationVisible)
        {
            storySpoilerMirewatchButton.setFocused(
                storyKeyboardNavigationActive && storySpoilerKeyboardFocus == 0);
            storySpoilerContinueButton.setFocused(
                storyKeyboardNavigationActive && storySpoilerKeyboardFocus == 1);

            sf::RectangleShape overlay({ui_canvas::Width, ui_canvas::Height});
            overlay.setPosition({ui_canvas::Left, 0.0f});
            overlay.setFillColor(sf::Color(0, 0, 0, 190));
            window.draw(overlay);
            drawPanel(window, {174.0f, 154.0f}, {452.0f, 304.0f});
            drawCenteredText(
                window,
                font,
                "Seelie Spoiler Warning",
                28,
                {400.0f, 196.0f},
                sf::Color(248, 224, 172));
            drawWrappedText(
                window,
                font,
                "Seelie contains Books Two and Three and explicitly reveals the Book One ending.",
                17,
                {210.0f, 232.0f},
                sf::Color(236, 226, 203),
                380.0f,
                5.0f);
            drawWrappedText(
                window,
                font,
                "Play Mirewatch first to experience that ending in order, or continue anyway.",
                15,
                {210.0f, 304.0f},
                sf::Color(190, 198, 214),
                380.0f,
                4.0f);
            storySpoilerMirewatchButton.draw(window, animationTime);
            storySpoilerContinueButton.draw(window, animationTime);
            drawCenteredText(
                window,
                font,
                "TAB/ARROWS: CHOOSE  |  ENTER: CONFIRM  |  ESC: MIREWATCH",
                12,
                {400.0f, 444.0f},
                palette::Ink);
        }
    };
