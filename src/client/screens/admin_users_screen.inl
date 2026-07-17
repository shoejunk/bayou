    auto drawAdminUsers = [&]() {
        adminTabs.draw(window);
        drawText(
            window,
            font,
            "Signed in as " + signedInLabel(),
            14,
            {344.0f, 22.0f},
            sf::Color(178, 186, 202),
            300.0f);
        drawText(
            window,
            font,
            "Users " + std::to_string(adminUsersTotalCount),
            14,
            {344.0f, 44.0f},
            sf::Color(248, 214, 112),
            150.0f);
        adminBackButton.draw(window);

        drawPanel(window, {24.0f, 78.0f}, {752.0f, 68.0f});
        drawText(window, font, "Search", 16, {42.0f, 98.0f}, sf::Color::White);
        adminSearchInput.draw(window);

        drawPanel(window, {24.0f, 160.0f}, {752.0f, 278.0f});
        const std::size_t lastUser =
            std::min(adminUsers.size(), static_cast<std::size_t>(AdminUsersPageSize));
        for (std::size_t i = 0; i < lastUser; ++i)
        {
            const float y = AdminUserRowY + static_cast<float>(i) * AdminUserRowHeight;
            const bool selected = selectedAdminUser && *selectedAdminUser == i;
            drawRow(
                window,
                font,
                {38.0f, y},
                {704.0f, 40.0f},
                adminUsers[i].username,
                std::string(adminUsers[i].isAdmin ? "Admin" : "Player") +
                    "  |  Gold: " + std::to_string(adminUsers[i].gold),
                selected);
        }
        if (adminUsers.empty() && !pendingAdminUsersLoad)
        {
            drawText(window, font, "No matching users", 18, {292.0f, 294.0f}, sf::Color(178, 186, 202));
        }

        adminPrevPageButton.draw(window);
        adminRefreshButton.draw(window);
        adminNextPageButton.draw(window);
        if (selectedAdminUser && *selectedAdminUser < adminUsers.size())
        {
            adminGoldInput.draw(window);
            adminGrantGoldButton.draw(window);
            adminRemoveGoldButton.draw(window);
            adminAddCardButton.draw(window);
            const bool targetIsAdmin = adminUsers[*selectedAdminUser].isAdmin;
            if (targetIsAdmin)
            {
                if (adminUsers[*selectedAdminUser].username == loggedInUsername)
                {
                    drawText(
                        window,
                        font,
                        "You cannot revoke your own admin status",
                        14,
                        {42.0f, 466.0f},
                        sf::Color(248, 214, 112),
                        190.0f);
                }
                else
                {
                    adminRevokeButton.draw(window);
                }
            }
            else
            {
                adminGrantButton.draw(window);
            }
            if (adminUsers[*selectedAdminUser].username != loggedInUsername)
            {
                adminDeleteButton.draw(window);
            }
        }
        if (deleteUserPopupVisible)
        {
            sf::RectangleShape overlay({800.0f, 600.0f});
            overlay.setFillColor(sf::Color(0, 0, 0, 170));
            window.draw(overlay);
            drawPanel(window, {220.0f, 176.0f}, {360.0f, 248.0f});
            drawText(window, font, "Delete User", 28, {250.0f, 200.0f}, sf::Color(248, 224, 172), 300.0f);
            drawText(window, font, "Permanently delete account:", 16, {250.0f, 252.0f}, sf::Color(220, 224, 230), 300.0f);
            drawText(window, font, adminUserDeleteTarget, 20, {250.0f, 276.0f}, sf::Color(248, 214, 112), 300.0f);
            drawText(window, font, "This also removes their decks and", 13, {250.0f, 314.0f}, sf::Color(214, 150, 140), 300.0f);
            drawText(window, font, "cannot be undone.", 13, {250.0f, 332.0f}, sf::Color(214, 150, 140), 300.0f);
            cancelDeleteUserButton.draw(window);
            confirmDeleteUserButton.draw(window);
        }
        else if (addCardPopupVisible && selectedAdminUser && *selectedAdminUser < adminUsers.size())
        {
            sf::RectangleShape overlay({800.0f, 600.0f});
            overlay.setFillColor(sf::Color(0, 0, 0, 170));
            window.draw(overlay);
            drawPanel(window, {190.0f, 112.0f}, {420.0f, 430.0f});
            drawText(window, font, "Add Card", 28, {220.0f, 136.0f}, sf::Color(248, 224, 172), 360.0f);
            drawText(
                window,
                font,
                "Add one copy to " + adminUsers[*selectedAdminUser].username + "'s collection",
                15,
                {220.0f, 178.0f},
                sf::Color(220, 224, 230),
                360.0f);
            adminCardInput.draw(window);

            if (pendingAdminCardListLoad)
            {
                drawText(window, font, "Loading cards...", 16, {220.0f, 284.0f}, sf::Color(248, 214, 112));
            }
            else if (!adminCardLoadError.empty())
            {
                drawText(window, font, adminCardLoadError, 14, {220.0f, 284.0f}, sf::Color(230, 120, 110), 360.0f);
            }
            else
            {
                const std::vector<std::string> cardTitles = visibleAdminCardTitles();
                for (std::size_t i = 0; i < cardTitles.size(); ++i)
                {
                    const std::string normalizedSelection =
                        game_data::normalizedAbility(adminCardInput.getContent());
                    const bool selected = game_data::normalizedTrait(cardTitles[i]) == normalizedSelection;
                    const auto card = std::find_if(
                        adminCardLibrary.begin(),
                        adminCardLibrary.end(),
                        [&](const card_data::Card& candidate) { return candidate.title == cardTitles[i]; });
                    drawRow(
                        window,
                        font,
                        {220.0f, AdminCardRowY + static_cast<float>(i) * AdminCardRowHeight},
                        {360.0f, 32.0f},
                        cardTitles[i],
                        card == adminCardLibrary.end() ? std::string{} : card->type,
                        selected);
                }
                if (cardTitles.empty())
                {
                    drawText(window, font, "No matching collectible cards", 15, {220.0f, 284.0f}, sf::Color(178, 186, 202));
                }
            }

            cancelAddCardButton.draw(window);
            if (!pendingAdminUserCard)
            {
                confirmAddCardButton.draw(window);
            }
        }
        window.draw(messageText);
    };

