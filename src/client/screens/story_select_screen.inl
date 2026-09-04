    auto drawStorySelect = [&]() {
        drawTitlePlaque(window, font, "Choose a Story", {400.0f, 48.0f}, {430.0f, 64.0f});
        drawCenteredText(
            window,
            font,
            "Switch anytime; completed missions and unlocks are saved separately for each path.",
            type::Body,
            {400.0f, 94.0f},
            palette::InkMuted);

        const auto drawCampaign = [&](StoryCampaign campaign,
                                      sf::Vector2f position,
                                      sf::Color accent,
                                      std::string_view artPath,
                                      std::string_view summary,
                                      Button& button) {
            constexpr sf::Vector2f panelSize{328.0f, 368.0f};
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
                    {{position.x + 18.0f, position.y + 18.0f}, {112.0f, 164.0f}});
            }

            drawWrappedText(
                window,
                gloomthornFontLoaded ? gloomthornFont : font,
                std::string(storyCampaignName(campaign)),
                23,
                position + sf::Vector2f(148.0f, 30.0f),
                palette::Ink,
                158.0f,
                4.0f);
            drawWrappedText(
                window,
                font,
                std::string(summary),
                14,
                position + sf::Vector2f(148.0f, 116.0f),
                palette::InkMuted,
                158.0f,
                4.0f);
            if (campaign == StoryCampaign::Mirewatch)
            {
                drawBeveledPlate(
                    window,
                    position + sf::Vector2f(140.0f, 82.0f),
                    {174.0f, 27.0f},
                    sf::Color(18, 54, 45, 248),
                    accent,
                    true,
                    6.0f);
                drawCenteredText(
                    window,
                    font,
                    "START HERE - RECOMMENDED",
                    11,
                    position + sf::Vector2f(227.0f, 95.5f),
                    sf::Color(225, 255, 238));
            }
            else
            {
                drawText(
                    window,
                    font,
                    "TACTICAL - TRAPS AND DAMAGE",
                    10,
                    position + sf::Vector2f(148.0f, 92.0f),
                    palette::InkMuted,
                    162.0f);
            }

            drawSeparatorRule(
                window,
                {position.x + 20.0f, position.y + 202.0f},
                panelSize.x - 40.0f,
                false);

            const int completed = storyCampaignProgress[storyProgressIndex(campaign)];
            const int missionCount = static_cast<int>(storyMissions(campaign).size());
            const std::string progress = completed >= missionCount
                ? "Campaign complete"
                : completed == 0
                    ? (campaign == StoryCampaign::Mirewatch
                        ? "River Teeth ready - gator attack"
                        : "First playable mission ready")
                    : "Entry " + std::to_string(completed + 1) + " ready";
            drawText(
                window,
                font,
                progress,
                type::Body,
                position + sf::Vector2f(22.0f, 226.0f),
                accent,
                panelSize.x - 44.0f);
            drawText(
                window,
                font,
                std::to_string(completed) + " / " + std::to_string(missionCount) + " complete",
                type::Caption,
                position + sf::Vector2f(22.0f, 254.0f),
                palette::InkMuted,
                panelSize.x - 44.0f);

            button.setLabel(campaign == StoryCampaign::Blackthorn
                ? "Play Blackthorn's Path"
                : "Play Mirewatch Path");
            button.draw(window, animationTime);
        };

        drawCampaign(
            StoryCampaign::Blackthorn,
            {56.0f, 124.0f},
            sf::Color(207, 151, 69),
            "cards/victorGreyshard.png",
            "Lead Victor's Company crew. Recover the stolen ledger with traps, marks, and hard-hitting allies.",
            storyBlackthornButton);
        drawCampaign(
            StoryCampaign::Mirewatch,
            {416.0f, 124.0f},
            sf::Color(103, 188, 153),
            "cards/reedBaelstone.png",
            "Lead Reed's resistance. Win Mirewatch's freedom through healing, movement, and teamwork.",
            storyMirewatchButton);

        storySelectBackButton.draw(window, animationTime);
    };
