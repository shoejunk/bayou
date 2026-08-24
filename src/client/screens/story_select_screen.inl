    auto drawStorySelect = [&]() {
        drawTitlePlaque(window, font, "Choose a Story", {400.0f, 48.0f}, {430.0f, 64.0f});
        drawCenteredText(
            window,
            font,
            "Two sides of Mirewatch's struggle. Progress is saved separately.",
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
                false,
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
                position + sf::Vector2f(148.0f, 104.0f),
                palette::InkMuted,
                158.0f,
                4.0f);

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
                    ? "Chapter 1 ready"
                    : "Chapter " + std::to_string(completed + 1) + " ready";
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

            button.setLabel(completed == 0 ? "Choose Chapter" : "View Chapters");
            button.draw(window, animationTime);
        };

        drawCampaign(
            StoryCampaign::Blackthorn,
            {56.0f, 124.0f},
            sf::Color(207, 151, 69),
            "cards/victorGreyshard.png",
            "The Company's pursuit, from the east-gate ledger to Victor's final stand.",
            storyBlackthornButton);
        drawCampaign(
            StoryCampaign::Mirewatch,
            {416.0f, 124.0f},
            sf::Color(103, 188, 153),
            "cards/reedBaelstone.png",
            "Reed's coalition, from the stolen cage to a town that owns itself.",
            storyMirewatchButton);

        storySelectBackButton.draw(window, animationTime);
    };
