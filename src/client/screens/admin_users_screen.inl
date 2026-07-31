    // The shared drawRow puts its secondary line at y+25 with a 12px face, so at
    // this row height the descenders were cut by the plate's bottom bevel. A
    // single-line row also lets the role and the gold figure be columns instead
    // of one run-together "Admin  |  Gold: 120" string.
    auto drawAdminUserRow =
        [&](float y, const network::AdminUserSummary& user, bool selected) {
        constexpr float RowX = 38.0f;
        constexpr float RowWidth = 704.0f;
        constexpr float RowHeight = 40.0f;

        drawBeveledPlate(
            window,
            {RowX, y},
            {RowWidth, RowHeight},
            selected ? sf::Color(76, 49, 25, 240) : sf::Color(19, 29, 30, 226),
            selected ? sf::Color(239, 190, 98) : sf::Color(103, 72, 39),
            selected,
            5.0f);

        drawText(
            window, font, user.username, 16, {RowX + 20.0f, y + 10.0f},
            sf::Color(246, 238, 218), 340.0f);

        // Only admins are badged; "Player" on every other row was noise.
        if (user.isAdmin)
        {
            sf::Text badgeText(font, "ADMIN", 10);
            const float badgeWidth = badgeText.getLocalBounds().size.x + 16.0f;
            sf::RectangleShape badge({badgeWidth, 18.0f});
            badge.setPosition({RowX + 400.0f, y + 11.0f});
            badge.setFillColor(sf::Color(123, 79, 168, 60));
            badge.setOutlineThickness(1.0f);
            badge.setOutlineColor(sf::Color(150, 108, 206, 200));
            window.draw(badge);
            badgeText.setFillColor(sf::Color(178, 143, 224));
            centerButtonText(badgeText, {RowX + 400.0f + badgeWidth * 0.5f, y + 20.0f});
            drawCrispText(window, badgeText);
        }

        sf::Text gold(font, std::to_string(user.gold), 15);
        gold.setFillColor(sf::Color(239, 190, 98));
        gold.setPosition(
            {RowX + RowWidth - 24.0f - gold.getLocalBounds().size.x, y + 11.0f});
        drawCrispText(window, gold);
    };

    auto drawAdminUsers = [&]() {
        adminTabs.draw(window);
        drawText(
            window,
            font,
            "Signed in as " + signedInLabel(),
            14,
            {452.0f, 22.0f},
            sf::Color(178, 186, 202),
            212.0f);
        drawText(
            window,
            font,
            adminUsersTotalCount == 1
                ? "1 account"
                : std::to_string(adminUsersTotalCount) + " accounts",
            14,
            {452.0f, 44.0f},
            sf::Color(248, 214, 112),
            212.0f);
        adminBackButton.draw(window);

        drawPanel(window, {24.0f, 78.0f}, {752.0f, 68.0f});
        drawText(window, font, "Search", 16, {42.0f, 98.0f}, sf::Color::White);
        adminSearchInput.draw(window);

        // The list plate starts 8px higher than its first row so the column
        // captions have a header band of their own inside it. Previously the
        // plate was exactly as tall as its rows and any caption placed inside
        // was cut by the top border.
        drawPanel(window, {24.0f, 152.0f}, {752.0f, 286.0f});
        drawText(window, font, "ACCOUNT", 11, {46.0f, 158.0f}, sf::Color(150, 132, 104));
        sf::Text goldCaption(font, "GOLD", 11);
        goldCaption.setFillColor(sf::Color(150, 132, 104));
        goldCaption.setPosition({718.0f - goldCaption.getLocalBounds().size.x, 158.0f});
        drawCrispText(window, goldCaption);
        const std::size_t lastUser =
            std::min(adminUsers.size(), static_cast<std::size_t>(AdminUsersPageSize));
        for (std::size_t i = 0; i < lastUser; ++i)
        {
            const float y = AdminUserRowY + static_cast<float>(i) * AdminUserRowHeight;
            const bool selected = selectedAdminUser && *selectedAdminUser == i;
            drawAdminUserRow(y, adminUsers[i], selected);
        }
        if (adminUsers.empty() && !pendingAdminUsersLoad)
        {
            drawText(window, font, "No matching users", 18, {292.0f, 294.0f}, sf::Color(178, 186, 202));
        }

        adminPrevPageButton.draw(window);
        adminRefreshButton.draw(window);
        adminNextPageButton.draw(window);
        // There was no page indicator at all, so paging through more accounts
        // than fit on one page gave no sense of position.
        if (adminUsersTotalCount > AdminUsersPageSize)
        {
            const std::uint32_t pageCount =
                (adminUsersTotalCount + AdminUsersPageSize - 1) / AdminUsersPageSize;
            sf::Text pageLabel(
                font,
                "Page " + std::to_string(adminUsersPage + 1) + " of " + std::to_string(pageCount),
                11);
            pageLabel.setFillColor(sf::Color(150, 132, 104));
            // Centred in the list's caption band. At y=137 it lay across the
            // bottom border of the search plate.
            pageLabel.setPosition(
                {400.0f - pageLabel.getLocalBounds().size.x * 0.5f, 158.0f});
            drawCrispText(window, pageLabel);
        }
        if (selectedAdminUser && *selectedAdminUser < adminUsers.size())
        {
            // The action controls used to float loose on the backdrop, which is
            // the one place on this screen with no containing plate. Panelling
            // them also scopes them visibly to the selected account.
            drawPanel(window, {24.0f, 448.0f}, {752.0f, 118.0f});
            drawText(
                window,
                font,
                "MANAGE " + adminUsers[*selectedAdminUser].username,
                11,
                {46.0f, 460.0f},
                sf::Color(150, 132, 104),
                180.0f);
            drawText(window, font, "GOLD AMOUNT", 11, {236.0f, 460.0f},
                     sf::Color(150, 132, 104));
            adminGoldInput.draw(window);
            adminGrantGoldButton.draw(window);
            adminRemoveGoldButton.draw(window);
            adminAddCardButton.draw(window);
            adminGiveStarterDeckButton.draw(window);
            const bool targetIsAdmin = adminUsers[*selectedAdminUser].isAdmin;
            if (targetIsAdmin)
            {
                if (adminUsers[*selectedAdminUser].username == loggedInUsername)
                {
                    // Sits in the slot the Revoke button would occupy, wrapped
                    // to that width. At y=466 in 190px it was truncated
                    // mid-sentence to "You cannot revoke your own ...".
                    drawWrappedText(
                        window,
                        font,
                        "You cannot revoke your own admin status.",
                        13,
                        {40.0f, 484.0f},
                        sf::Color(248, 214, 112),
                        176.0f);
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
            sf::RectangleShape overlay({ui_canvas::Width, ui_canvas::Height});
            overlay.setPosition({ui_canvas::Left, 0.0f});
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
            // Deeper than the old alpha 170: at that level the action buttons
            // behind stayed bright enough to compete with the dialog.
            sf::RectangleShape overlay({ui_canvas::Width, ui_canvas::Height});
            overlay.setPosition({ui_canvas::Left, 0.0f});
            overlay.setFillColor(sf::Color(0, 0, 0, 205));
            window.draw(overlay);

            const std::vector<std::string> cardTitles =
                (pendingAdminCardListLoad || !adminCardLoadError.empty())
                    ? std::vector<std::string>{}
                    : visibleAdminCardTitles();
            const bool showsMessage =
                pendingAdminCardListLoad || !adminCardLoadError.empty() || cardTitles.empty();

            // Sized to the number of suggestions actually shown. At a fixed
            // 430px a single match left ~300px of empty panel below it.
            const float buttonsY = layoutAddCardPopupButtons();
            drawPanel(window, {190.0f, 132.0f}, {420.0f, buttonsY + 62.0f - 132.0f});

            drawText(window, font, "Add Card", 26, {220.0f, 150.0f}, sf::Color(248, 224, 172), 360.0f);
            drawText(
                window,
                font,
                "Add one copy to " + adminUsers[*selectedAdminUser].username + "'s collection",
                14,
                {220.0f, 182.0f},
                sf::Color(220, 224, 230),
                360.0f);
            drawText(window, font, "CARD NAME", 11, {240.0f, 208.0f},
                     sf::Color(150, 132, 104));
            adminCardInput.draw(window);

            if (pendingAdminCardListLoad)
            {
                drawText(window, font, "Searching the catalog...", 15, {222.0f, AdminCardRowY + 4.0f},
                         sf::Color(248, 214, 112));
            }
            else if (!adminCardLoadError.empty())
            {
                drawText(window, font, adminCardLoadError, 14, {222.0f, AdminCardRowY + 4.0f},
                         sf::Color(230, 120, 110), 356.0f);
            }
            else if (cardTitles.empty())
            {
                drawText(window, font, "No matching collectible cards", 15,
                         {222.0f, AdminCardRowY + 4.0f}, sf::Color(178, 186, 202));
            }
            else
            {
                const std::string normalizedSelection =
                    game_data::normalizedAbility(adminCardInput.getContent());
                for (std::size_t i = 0; i < cardTitles.size(); ++i)
                {
                    const bool selected =
                        game_data::normalizedTrait(cardTitles[i]) == normalizedSelection;
                    const auto card = std::find_if(
                        adminCardLibrary.begin(),
                        adminCardLibrary.end(),
                        [&](const card_data::Card& candidate) { return candidate.title == cardTitles[i]; });
                    const float rowY = AdminCardRowY + static_cast<float>(i) * AdminCardRowHeight;

                    // Drawn here rather than via the shared drawRow, whose
                    // secondary line at y+25 was clipped by a 32px row. Type
                    // becomes a right-hand column on one line instead.
                    drawBeveledPlate(
                        window,
                        {220.0f, rowY},
                        {360.0f, 32.0f},
                        selected ? sf::Color(76, 49, 25, 240) : sf::Color(19, 29, 30, 226),
                        selected ? sf::Color(239, 190, 98) : sf::Color(103, 72, 39),
                        selected,
                        5.0f);
                    drawText(window, font, cardTitles[i], 15, {240.0f, rowY + 7.0f},
                             sf::Color(246, 238, 218), 232.0f);
                    if (card != adminCardLibrary.end())
                    {
                        sf::Text type(font, card->type, 12);
                        type.setFillColor(sf::Color(181, 166, 137));
                        type.setPosition({552.0f - type.getLocalBounds().size.x, rowY + 10.0f});
                        drawCrispText(window, type);
                    }
                }
            }

            cancelAddCardButton.draw(window);
            if (!pendingAdminUserCard)
            {
                confirmAddCardButton.draw(window);
            }
        }
        else if (giveStarterDeckPopupVisible && selectedAdminUser && *selectedAdminUser < adminUsers.size())
        {
            sf::RectangleShape overlay({ui_canvas::Width, ui_canvas::Height});
            overlay.setPosition({ui_canvas::Left, 0.0f});
            overlay.setFillColor(sf::Color(0, 0, 0, 170));
            window.draw(overlay);
            drawPanel(window, {190.0f, 138.0f}, {420.0f, 348.0f});
            drawText(window, font, "Give Starter Deck", 28, {220.0f, 162.0f}, sf::Color(248, 224, 172), 360.0f);
            drawText(
                window,
                font,
                "Adds every card in the deck to " + adminUsers[*selectedAdminUser].username + "'s collection",
                15,
                {220.0f, 200.0f},
                sf::Color(220, 224, 230),
                360.0f);

            for (std::size_t i = 0; i < starter_decks::Names.size(); ++i)
            {
                drawRow(
                    window,
                    font,
                    {220.0f, AdminStarterDeckRowY + static_cast<float>(i) * AdminStarterDeckRowHeight},
                    {360.0f, AdminStarterDeckRowHeight - 4.0f},
                    starter_decks::Names[i],
                    "Starter deck",
                    selectedAdminStarterDeck && *selectedAdminStarterDeck == i);
            }

            cancelGiveStarterDeckButton.draw(window);
            if (!pendingAdminUserStarterDeck)
            {
                confirmGiveStarterDeckButton.draw(window);
            }
        }
        window.draw(messageText);
    };
