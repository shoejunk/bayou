    auto drawGameCardFace = [&](sf::Vector2f position, const game_data::GameCard& card, bool selected, bool affordable) {
        drawBeveledPlate(
            window,
            position,
            {HandCardWidth, HandCardHeight},
            selected ? sf::Color(76, 49, 25, 238) : sf::Color(18, 24, 24, 236),
            selected ? sf::Color(239, 190, 98) : sf::Color(155, 111, 59),
            selected,
            6.0f);

        drawBeveledPlate(
            window,
            {position.x + 5.0f, position.y + 5.0f},
            {34.0f, 34.0f},
            sf::Color(8, 14, 15),
            sf::Color(114, 83, 47),
            false,
            4.0f);
        if (sf::Texture* art = cardArtTexture(card.imagePath))
        {
            drawContainSprite(window, *art, {{position.x + 7.0f, position.y + 7.0f}, {30.0f, 30.0f}},
                              affordable ? sf::Color::White : sf::Color(120, 112, 104));
        }

        sf::CircleShape costBadge(11.0f);
        costBadge.setPosition({position.x + HandCardWidth - 24.0f, position.y + 3.0f});
        costBadge.setFillColor(affordable ? sf::Color(39, 126, 139) : sf::Color(91, 66, 58));
        costBadge.setOutlineThickness(1.0f);
        costBadge.setOutlineColor(sf::Color(224, 174, 83));
        window.draw(costBadge);
        const int displayedCost = card.type == "Hero" ? card.heroCost : card.cost;
        drawText(window, font, std::to_string(displayedCost), 13, {position.x + HandCardWidth - 21.0f, position.y + 4.0f}, sf::Color(248, 239, 216));

        const sf::Color titleColor = affordable ? sf::Color(248, 239, 216) : sf::Color(158, 128, 118);
        drawText(window, font, card.title, 12, {position.x + 6.0f, position.y + 37.0f}, titleColor, HandCardWidth - 12.0f);

        std::string line2;
        std::string line3;
        if (card.type == "Unit" || card.type == "Hero")
        {
            line2 = "HP " + std::to_string(card.health);
            line3 = "Actions " + std::to_string(card.actions.size());
        }
        else
        {
            line2 = "Spell";
            if (card.effect == "damage") line3 = "Deal " + std::to_string(card.power);
            else if (card.effect == "heal") line3 = "Heal " + std::to_string(card.power);
            else if (card.effect == "steam") line3 = "+" + std::to_string(card.power) + " steam";
        }
        drawText(window, font, line2, 11, {position.x + 6.0f, position.y + 53.0f}, sf::Color(224, 210, 176), HandCardWidth - 12.0f);
        drawText(window, font, line3, 11, {position.x + 6.0f, position.y + 65.0f}, sf::Color(143, 220, 205), HandCardWidth - 12.0f);
    };

    auto readinessDescription = [&](const game_data::Piece& piece) {
        const game_data::Phase phase = static_cast<game_data::Phase>(gameSnapshot.phase);
        if (sandboxMode)
        {
            return std::string("Status: sandbox ready");
        }
        if (phase == game_data::Phase::HeroPlacement)
        {
            return std::string("Status: waiting for hero placement");
        }
        if (phase == game_data::Phase::GameOver)
        {
            return std::string("Status: game over");
        }
        if (gameSnapshot.activePlayer != piece.owner)
        {
            return "Status: waiting for Player " + std::to_string(piece.owner);
        }
        if (piece.hasActed)
        {
            return std::string("Status: acted this turn");
        }
        if (piece.sleepTurnsRemaining > 0)
        {
            return std::string("Status: sleeping, cannot move");
        }
        return std::string("Status: ready");
    };

    auto piecePopupActionDescriptions = [&](const game_data::Piece& piece) {
        std::vector<std::pair<std::string, sf::Color>> descriptions;
        descriptions.push_back({"Position: row " + std::to_string(piece.row + 1) +
                                    ", column " + std::to_string(piece.column + 1),
                                sf::Color(190, 198, 214)});
        descriptions.push_back({
            std::string("Territory: ") +
                (piece.canControl ? "controls occupied square + adjacent influence" : "no control"),
            sf::Color(198, 180, 142)});
        if (piece.growTurnsRemaining > 0)
        {
            descriptions.push_back({"Growing: " + std::to_string(piece.growTurnsRemaining) + " turns",
                                    sf::Color(210, 180, 105)});
        }
        if (piece.disabledTurns > 0)
        {
            descriptions.push_back({"Disabled: " + std::to_string(piece.disabledTurns) + " turns",
                                    sf::Color(225, 130, 110)});
        }
        if (piece.sleepTurnsRemaining > 0)
        {
            descriptions.push_back({"Sleeping: " + std::to_string(piece.sleepTurnsRemaining) + " turns",
                                    sf::Color(120, 190, 230)});
        }
        if (!piece.ability.empty())
        {
            descriptions.push_back({"Ability: " + game_data::pieceAbilityLabel(piece), sf::Color(210, 216, 228)});
            if (piece.abilityUses > 0)
            {
                descriptions.push_back({"Ability uses: " + std::to_string(piece.abilityUses),
                                        sf::Color(190, 198, 214)});
            }
            else if (piece.abilityUses < 0)
            {
                descriptions.push_back({"Ability uses: unlimited", sf::Color(190, 198, 214)});
            }
        }
        if (piece.actions.empty())
        {
            descriptions.push_back({"Actions: none", sf::Color(225, 170, 150)});
        }
        for (std::size_t i = 0; i < piece.actions.size(); ++i)
        {
            descriptions.push_back({
                actionDescription(piece.actions[i], i),
                piece.actions[i].state == piece.actionState ? sf::Color(143, 220, 205) : sf::Color(190, 198, 214)});
        }
        if (piece.isHero)
        {
            descriptions.push_back({sandboxMode ? "Hero: sandbox rules" : "Hero: defeat all enemy heroes to win",
                                    sf::Color(225, 170, 150)});
            if (!piece.keywords.empty())
            {
                descriptions.push_back({"Keywords: " + joinStrings(piece.keywords, ", "), sf::Color(198, 180, 142)});
            }
        }
        else if (!piece.keywords.empty())
        {
            descriptions.push_back({"Keywords: " + joinStrings(piece.keywords, ", "), sf::Color(198, 180, 142)});
        }
        return descriptions;
    };

    auto cardPlayDescription = [&](const game_data::GameCard& card) {
        if (card.type == "Hero")
        {
            return std::string("Play: hero placement");
        }
        if (card.type == "Unit")
        {
            return "Play: " + std::to_string(card.cost) + " steam, controlled empty square";
        }
        if (card.effect == "damage")
        {
            return "Play: " + std::to_string(card.cost) + " steam, deal " +
                std::to_string(card.power) + " damage";
        }
        if (card.effect == "heal")
        {
            return "Play: " + std::to_string(card.cost) + " steam, restore " +
                std::to_string(card.power) + " health";
        }
        if (card.effect == "steam")
        {
            return "Play: " + std::to_string(card.cost) + " steam, gain " +
                std::to_string(card.power) + " steam";
        }
        return std::string("Play: ") + std::to_string(card.cost) + " steam";
    };

    auto cardPopupActionDescriptions = [&](const game_data::GameCard& card) {
        std::vector<std::pair<std::string, sf::Color>> descriptions;
        const int me = gameSnapshot.yourPlayer;
        const game_data::Phase phase = static_cast<game_data::Phase>(gameSnapshot.phase);
        const bool yourTurn = sandboxMode ||
            (phase == game_data::Phase::Playing && gameSnapshot.activePlayer == me);
        const bool affordable = sandboxMode || card.cost <= gameSnapshot.players[static_cast<std::size_t>(me - 1)].steam;
        const std::vector<std::string> missingKeywords =
            sandboxMode ? std::vector<std::string>{} : game_data::missingHeroKeywords(gameSnapshot.pieces, me, card);

        if (phase == game_data::Phase::GameOver)
        {
            descriptions.push_back({"Status: game over", sf::Color(210, 216, 228)});
        }
        else if (!yourTurn)
        {
            descriptions.push_back({"Status: playable on your battle turn", sf::Color(210, 216, 228)});
        }
        else if (!affordable)
        {
            descriptions.push_back({"Status: not enough steam", sf::Color(225, 170, 150)});
        }
        else if (!missingKeywords.empty())
        {
            descriptions.push_back({
                "Status: requires " + joinStrings(missingKeywords, ", "),
                sf::Color(225, 170, 150)});
        }
        else
        {
            descriptions.push_back({
                sandboxMode
                    ? "Status: playable now, free in sandbox"
                    : "Status: playable now, ends turn",
                sf::Color(120, 220, 150)});
        }

        descriptions.push_back({cardPlayDescription(card), sf::Color(210, 216, 228)});
        if (!card.keywords.empty())
        {
            descriptions.push_back({
                "Required keywords: " + joinStrings(card.keywords, ", "),
                sf::Color(198, 180, 142)});
        }
        if (card.type == "Unit" || card.type == "Hero")
        {
            if (card.actions.empty())
            {
                descriptions.push_back({"Actions: none", sf::Color(225, 170, 150)});
            }
            for (std::size_t i = 0; i < card.actions.size(); ++i)
            {
                descriptions.push_back({actionDescription(card.actions[i], i), sf::Color(143, 220, 205)});
            }
            descriptions.push_back({
                "Territory: occupied square + adjacent influence",
                sf::Color(198, 180, 142)});
        }
        return descriptions;
    };

    auto popupActionContentHeight = [&](const std::vector<std::pair<std::string, sf::Color>>& descriptions) {
        float height = 0.0f;
        for (const auto& [description, color] : descriptions)
        {
            (void)color;
            height += static_cast<float>(wrapText(font, description, 14, PiecePopupTextWidth - 24.0f).size()) * 18.0f;
            height += 8.0f;
        }
        return height;
    };

    auto popupMaxScroll = [&](const std::vector<std::pair<std::string, sf::Color>>& descriptions) {
        return std::max(0.0f, popupActionContentHeight(descriptions) - PiecePopupScrollHeight);
    };

    auto drawPiecePopup = [&]() {
        if (!inspectedPieceId && !inspectedHandIndex)
        {
            return;
        }

        const game_data::Piece* piece = nullptr;
        const game_data::GameCard* card = nullptr;
        if (inspectedHandIndex)
        {
            if (*inspectedHandIndex >= gameSnapshot.hand.size())
            {
                inspectedHandIndex.reset();
                inspectedPieceScroll = 0.0f;
                return;
            }
            card = &gameSnapshot.hand[*inspectedHandIndex];
        }
        else if (inspectedPieceId)
        {
            piece = gamePieceById(*inspectedPieceId);
            if (!piece)
            {
                inspectedPieceId.reset();
                inspectedPieceScroll = 0.0f;
                return;
            }
        }

        const std::vector<std::pair<std::string, sf::Color>> actionDescriptions =
            piece ? piecePopupActionDescriptions(*piece) : cardPopupActionDescriptions(*card);

        if (!piece && !card)
        {
            return;
        }

        sf::RectangleShape overlay({800.0f, 600.0f});
        overlay.setFillColor(sf::Color(0, 0, 0, 150));
        window.draw(overlay);

        drawPanel(window, {PiecePopupX, PiecePopupY}, {PiecePopupWidth, PiecePopupHeight});
        drawText(window, font, piece ? piece->name : card->title, 24, {PiecePopupX + 22.0f, PiecePopupY + 18.0f},
                 sf::Color(248, 239, 216), PiecePopupWidth - 44.0f);

        drawBeveledPlate(
            window,
            {PiecePopupX + 22.0f, PiecePopupY + 62.0f},
            {104.0f, 104.0f},
            sf::Color(8, 14, 15),
            sf::Color(155, 111, 59),
            false,
            7.0f);

        bool drewArt = false;
        if (sf::Texture* art = cardArtTexture(piece ? piece->imagePath : card->imagePath))
        {
            drawContainSprite(window,
                *art,
                {{PiecePopupX + 30.0f, PiecePopupY + 70.0f}, {88.0f, 88.0f}});
            drewArt = true;
        }
        if (piece)
        {
            if (!drewArt)
            {
                drewArt = drawPieceVisual(
                    pieceTokenPath(*piece),
                    pieceWalkAnimPath(*piece),
                    pieceBasePath(*piece),
                    piece->separateBaseArt,
                    piece->separateBaseArt && piece->owner == 2,
                    piece->walkAnimFrames,
                    {PiecePopupX + 74.0f, PiecePopupY + 160.0f},
                    0.92f,
                    sf::Color::White,
                    -1);
            }
        }

        float y = PiecePopupY + 66.0f;
        const float statX = PiecePopupX + 146.0f;
        if (piece)
        {
            const std::string ownerLabel = piece->owner == gameSnapshot.yourPlayer ? "Yours" : "Opponent";
            const std::string typeLabel = piece->isHero ? "Hero" : "Unit";
            drawText(window, font, "Type: " + typeLabel + "   Owner: " + ownerLabel +
                         " (P" + std::to_string(piece->owner) + ")",
                     15, {statX, y}, ownerColor(piece->owner), PiecePopupWidth - 174.0f);
            y += 22.0f;
            drawText(window, font, readinessDescription(*piece), 14, {statX, y}, sf::Color(210, 216, 228),
                     PiecePopupWidth - 174.0f);
            y += 22.0f;
            drawText(window, font, "Health: " + std::to_string(piece->health) + "/" + std::to_string(piece->maxHealth),
                     14, {statX, y}, sf::Color(224, 210, 176));
            y += 22.0f;
            drawText(window, font, "Actions: " + std::to_string(piece->actions.size()),
                     14, {statX, y}, sf::Color(143, 220, 205), PiecePopupWidth - 174.0f);
        }
        else
        {
            drawText(window, font, "Type: " + card->type + "   Location: Hand", 15, {statX, y}, sf::Color(143, 220, 205), PiecePopupWidth - 174.0f);
            y += 24.0f;
            if (card->type == "Hero")
            {
                drawText(window, font, "Hero cost: " + std::to_string(card->heroCost), 14, {statX, y}, sf::Color(248, 214, 112));
            }
            else
            {
                drawText(window, font, "Cost: " + std::to_string(card->cost) + " steam", 14, {statX, y}, sf::Color(150, 210, 235));
            }
            y += 24.0f;
            if (card->type == "Unit" || card->type == "Hero")
            {
                drawText(window, font, "Health: " + std::to_string(card->health), 14, {statX, y}, sf::Color(224, 210, 176));
                y += 22.0f;
                drawText(window, font, "Actions: " + std::to_string(card->actions.size()),
                         14, {statX, y}, sf::Color(143, 220, 205), PiecePopupWidth - 174.0f);
            }
            else
            {
                drawText(window, font, "Effect: " + card->effect, 14, {statX, y}, sf::Color(224, 210, 176));
                y += 22.0f;
                drawText(window, font, "Power: " + std::to_string(card->power), 14, {statX, y}, sf::Color(224, 210, 176));
                y += 22.0f;
                drawText(window, font, "Target: " + card->target, 14, {statX, y}, sf::Color(143, 220, 205), PiecePopupWidth - 174.0f);
            }
        }

        inspectedPieceScroll = std::clamp(inspectedPieceScroll, 0.0f, popupMaxScroll(actionDescriptions));

        drawText(window, font, piece ? "Details" : "Actions", 17, {PiecePopupTextX, PiecePopupActionHeadingY}, sf::Color::White);

        drawBeveledPlate(
            window,
            {PiecePopupTextX, PiecePopupScrollY},
            {PiecePopupTextWidth, PiecePopupScrollHeight},
            sf::Color(8, 14, 15, 132),
            sf::Color(96, 66, 35, 150),
            false,
            7.0f);

        const sf::View previousView = window.getView();
        sf::View actionView(sf::FloatRect(
            {PiecePopupTextX, PiecePopupScrollY + inspectedPieceScroll},
            {PiecePopupTextWidth, PiecePopupScrollHeight}));
        actionView.setViewport(sf::FloatRect(
            {PiecePopupTextX / 800.0f, PiecePopupScrollY / 600.0f},
            {PiecePopupTextWidth / 800.0f, PiecePopupScrollHeight / 600.0f}));
        window.setView(actionView);

        y = PiecePopupScrollY + 8.0f;
        for (const auto& [description, color] : actionDescriptions)
        {
            y = drawWrappedText(window, font, description, 14, {PiecePopupTextX + 8.0f, y}, color, PiecePopupTextWidth - 24.0f);
            y += 8.0f;
        }

        window.setView(previousView);

        const float maxScroll = popupMaxScroll(actionDescriptions);
        if (maxScroll > 0.0f)
        {
            const float trackX = PiecePopupX + PiecePopupWidth - 22.0f;
            sf::RectangleShape track({4.0f, PiecePopupScrollHeight - 12.0f});
            track.setPosition({trackX, PiecePopupScrollY + 6.0f});
            track.setFillColor(sf::Color(73, 96, 98, 170));
            window.draw(track);

            const float thumbHeight = std::max(28.0f, track.getSize().y * (PiecePopupScrollHeight / (PiecePopupScrollHeight + maxScroll)));
            const float thumbY = track.getPosition().y +
                (track.getSize().y - thumbHeight) * (inspectedPieceScroll / maxScroll);
            sf::RectangleShape thumb({4.0f, thumbHeight});
            thumb.setPosition({trackX, thumbY});
            thumb.setFillColor(sf::Color(143, 220, 205, 230));
            window.draw(thumb);
        }

        if (canDiscardInspectedHandCard())
        {
            discardCardButton.draw(window);
        }
        closePiecePopupButton.draw(window);
    };

    auto drawGame = [&]() {
        if (!haveSnapshot)
        {
            drawText(
                window,
                font,
                sandboxMode ? "Loading sandbox..." : "Connecting to match...",
                24,
                {260.0f, 280.0f},
                sf::Color(200, 208, 222));
            leaveGameButton.draw(window);
            return;
        }

        const int me = gameSnapshot.yourPlayer;
        const int sandboxPlayer = sandboxMode ? sandboxPlacementPlayer : me;
        const game_data::Phase phase = static_cast<game_data::Phase>(gameSnapshot.phase);
        const game_data::Piece* selectedPiece = selectedPieceId ? gamePieceById(*selectedPieceId) : nullptr;
        const game_data::Piece* draggedPiece =
            gameDragKind == GameDragKind::Piece && draggingPieceId ? gamePieceById(*draggingPieceId) : nullptr;
        const game_data::Piece* actingPiece = draggedPiece ? draggedPiece : selectedPiece;
        const std::optional<std::pair<int, int>> draggedPieceSquare =
            gameDragActive && draggedPiece ? squareAtPixel(gameDragCurrentPos) : std::nullopt;
        bool draggedPieceDropValid = false;
        if (draggedPiece && draggedPieceSquare)
        {
            const game_data::ActionResolution action = game_data::resolvePieceAction(
                gameSnapshot.pieces,
                gameSnapshot.holes,
                *draggedPiece,
                draggedPieceSquare->first,
                draggedPieceSquare->second);
            draggedPieceDropValid = phase == game_data::Phase::Playing &&
                (sandboxMode ||
                 (gameSnapshot.activePlayer == me &&
                  draggedPiece->owner == me &&
                  !draggedPiece->hasActed)) &&
                action.legal;
        }
        const std::optional<std::size_t> actingHandIndex =
            gameDragKind == GameDragKind::HandCard && draggingHandIndex ? draggingHandIndex : selectedHandIndex;
        const game_data::GameCard* draggedHandCard =
            gameDragActive && gameDragKind == GameDragKind::HandCard && draggingHandIndex &&
                *draggingHandIndex < gameSnapshot.hand.size()
            ? &gameSnapshot.hand[*draggingHandIndex]
            : nullptr;
        const bool draggingPieceCard = draggedHandCard &&
            (draggedHandCard->type == "Unit" || draggedHandCard->type == "Hero");
        const std::optional<std::pair<int, int>> draggedHandSquare =
            draggingPieceCard ? squareAtPixel(gameDragCurrentPos) : std::nullopt;
        bool draggedHandDropValid = false;
        if (draggedHandCard && draggedHandSquare)
        {
            const auto [row, column] = *draggedHandSquare;
            const std::size_t idx = static_cast<std::size_t>(game_data::squareIndex(row, column));
            if (phase == game_data::Phase::HeroPlacement && draggedHandCard->type == "Hero")
            {
                const auto home = game_data::homeSquares(me);
                draggedHandDropValid = !gamePieceAt(row, column) &&
                    std::find(home.begin(), home.end(), std::pair<int, int>{row, column}) != home.end();
            }
            else if (phase == game_data::Phase::Playing &&
                     (draggedHandCard->type == "Unit" || (sandboxMode && draggedHandCard->type == "Hero")))
            {
                draggedHandDropValid = (sandboxMode || gameSnapshot.activePlayer == me) &&
                    (sandboxMode || draggedHandCard->cost <= gameSnapshot.players[static_cast<std::size_t>(me - 1)].steam) &&
                    (sandboxMode || game_data::heroKeywordsAllowCard(gameSnapshot.pieces, me, *draggedHandCard)) &&
                    !gamePieceAt(row, column) &&
                    gameSnapshot.control[idx] == sandboxPlayer;
            }
        }

        // Precompute highlight masks for the current selection.
        std::array<int, game_data::BoardSquares> highlight{};  // 0 none,1 move,2 attack,3 place,4 spell
        if (phase == game_data::Phase::HeroPlacement &&
            gameSnapshot.players[static_cast<std::size_t>(me - 1)].heroesToPlace > 0)
        {
            for (const auto& [r, c] : game_data::homeSquares(me))
            {
                if (!gamePieceAt(r, c))
                {
                    highlight[static_cast<std::size_t>(game_data::squareIndex(r, c))] = 3;
                }
            }
        }
        else if (phase == game_data::Phase::Playing && (sandboxMode || gameSnapshot.activePlayer == me))
        {
            if (actingPiece && (sandboxMode || !actingPiece->hasActed))
            {
                for (int r = 0; r < game_data::BoardSize; ++r)
                {
                    for (int c = 0; c < game_data::BoardSize; ++c)
                    {
                        const std::size_t idx = static_cast<std::size_t>(game_data::squareIndex(r, c));
                        const game_data::ActionResolution action = game_data::resolvePieceAction(
                            gameSnapshot.pieces, gameSnapshot.holes, *actingPiece, r, c);
                        if (action.legal)
                        {
                            highlight[idx] = action.attacks ? 2 : 1;
                        }
                    }
                }
            }
            else if (actingHandIndex && *actingHandIndex < gameSnapshot.hand.size())
            {
                const game_data::GameCard& card = gameSnapshot.hand[*actingHandIndex];
                if (sandboxMode || game_data::heroKeywordsAllowCard(gameSnapshot.pieces, me, card))
                {
                    for (int r = 0; r < game_data::BoardSize; ++r)
                    {
                        for (int c = 0; c < game_data::BoardSize; ++c)
                        {
                            const std::size_t idx = static_cast<std::size_t>(game_data::squareIndex(r, c));
                            const game_data::Piece* occupant = gamePieceAt(r, c);
                            if (card.type == "Unit" || (sandboxMode && card.type == "Hero"))
                            {
                                if (!occupant && gameSnapshot.control[idx] == sandboxPlayer)
                                {
                                    highlight[idx] = 3;
                                }
                            }
                            else if (card.effect == "damage" && occupant && occupant->owner != sandboxPlayer)
                            {
                                highlight[idx] = 2;
                            }
                            else if (card.effect == "heal" && occupant && occupant->owner == sandboxPlayer)
                            {
                                highlight[idx] = 4;
                            }
                        }
                    }
                }
            }
        }
        if (storyMode && storyTargetRow >= 0 && storyTargetColumn >= 0 &&
            game_data::inBounds(storyTargetRow, storyTargetColumn))
        {
            highlight[static_cast<std::size_t>(game_data::squareIndex(storyTargetRow, storyTargetColumn))] = 5;
        }

        const std::array<sf::Vector2f, 4> boardTop = {
            boardEdgePoint(0, 0),
            boardEdgePoint(0, game_data::BoardSize),
            boardEdgePoint(game_data::BoardSize, game_data::BoardSize),
            boardEdgePoint(game_data::BoardSize, 0)};
        drawQuad(offsetQuad(boardTop, {7.0f, 15.0f}), sf::Color(0, 0, 0, 95));

        const sf::Vector2f topLeft = boardTop[0];
        const sf::Vector2f topRight = boardTop[1];
        const sf::Vector2f bottomRight = boardTop[2];
        const sf::Vector2f bottomLeft = boardTop[3];
        drawQuad(
            {topRight, bottomRight, {bottomRight.x + 10.0f, bottomRight.y + BoardThickness},
             {topRight.x + 5.0f, topRight.y + BoardThickness * 0.42f}},
            sf::Color(7, 28, 31, 238),
            1.0f,
            sf::Color(35, 83, 77, 170));
        drawQuad(
            {topLeft, {topLeft.x - 5.0f, topLeft.y + BoardThickness * 0.42f},
             {bottomLeft.x - 10.0f, bottomLeft.y + BoardThickness}, bottomLeft},
            sf::Color(8, 24, 27, 238),
            1.0f,
            sf::Color(35, 83, 77, 170));
        drawQuad(
            {bottomLeft, bottomRight, {bottomRight.x + 10.0f, bottomRight.y + BoardThickness},
             {bottomLeft.x - 10.0f, bottomLeft.y + BoardThickness}},
            sf::Color(77, 49, 28, 246),
            1.0f,
            sf::Color(167, 112, 56, 190));
        drawQuad(boardTop, sf::Color(9, 20, 21, 232), 3.0f, sf::Color(153, 105, 51));

        // Board squares.
        for (int screenRow = 0; screenRow < game_data::BoardSize; ++screenRow)
        {
            const int row = rowForScreenRow(screenRow, me);
            for (int column = 0; column < game_data::BoardSize; ++column)
            {
                const std::size_t idx = static_cast<std::size_t>(game_data::squareIndex(row, column));
                const BoardCellMetrics metrics = boardCellMetrics(row, column);
                drawQuad(metrics.corners, ownerTint(gameSnapshot.control[idx]), 1.0f, sf::Color(81, 63, 37));

                if ((row + column) % 2 == 0)
                {
                    drawQuad(metrics.corners, sf::Color(255, 239, 190, 16));
                }

                if (gameSnapshot.holes[idx] != 0)
                {
                    const sf::Vector2f anchor = boardCellAnchor(metrics);
                    const float radius = 8.0f * metrics.depthScale;
                    sf::CircleShape hole(radius);
                    hole.setScale({1.0f, 0.48f});
                    hole.setPosition({anchor.x - radius, anchor.y - radius * 0.42f});
                    hole.setFillColor(sf::Color(3, 7, 8, 225));
                    hole.setOutlineThickness(1.5f);
                    hole.setOutlineColor(sf::Color(108, 78, 46));
                    window.draw(hole);
                }

                if (highlight[idx] != 0)
                {
                    sf::Color colors[6] = {
                        sf::Color::Transparent,
                        sf::Color(90, 200, 120, 90),
                        sf::Color(220, 90, 80, 110),
                        sf::Color(90, 200, 210, 90),
                        sf::Color(110, 200, 150, 90),
                        sf::Color(248, 214, 112, 135)};
                    drawQuad(metrics.corners, colors[highlight[idx]]);
                }

                if (storyMode && row == storyTargetRow && column == storyTargetColumn)
                {
                    const sf::Vector2f anchor = boardCellAnchor(metrics);
                    const float pulse = 1.0f + std::sin(animationTime * 4.0f) * 0.12f;
                    const float radius = 14.0f * metrics.depthScale * pulse;
                    sf::CircleShape glow(radius);
                    glow.setOrigin({radius, radius});
                    glow.setScale({1.35f, 0.62f});
                    glow.setPosition({anchor.x, anchor.y - 6.0f * metrics.depthScale});
                    glow.setFillColor(sf::Color(248, 214, 112, 92));
                    window.draw(glow);

                    sf::RectangleShape valve({30.0f * metrics.depthScale, 8.0f * metrics.depthScale});
                    valve.setOrigin({15.0f * metrics.depthScale, 4.0f * metrics.depthScale});
                    valve.setPosition({anchor.x, anchor.y - 12.0f * metrics.depthScale});
                    valve.setFillColor(sf::Color(93, 64, 39));
                    valve.setOutlineThickness(1.5f);
                    valve.setOutlineColor(sf::Color(248, 214, 112));
                    window.draw(valve);
                }

                if (draggedHandSquare &&
                    draggedHandSquare->first == row &&
                    draggedHandSquare->second == column)
                {
                    drawQuad(
                        metrics.corners,
                        draggedHandDropValid
                            ? sf::Color(90, 225, 170, 125)
                            : sf::Color(225, 75, 65, 125),
                        2.5f,
                        draggedHandDropValid
                            ? sf::Color(145, 255, 215, 235)
                            : sf::Color(255, 135, 120, 235));
                }

                if (draggedPieceSquare &&
                    draggedPieceSquare->first == row &&
                    draggedPieceSquare->second == column)
                {
                    drawQuad(
                        metrics.corners,
                        draggedPieceDropValid
                            ? sf::Color(90, 225, 170, 125)
                            : sf::Color(225, 75, 65, 125),
                        2.5f,
                        draggedPieceDropValid
                            ? sf::Color(145, 255, 215, 235)
                            : sf::Color(255, 135, 120, 235));
                }
            }
        }

        // Pieces.
        std::vector<const game_data::Piece*> pieceDrawOrder;
        pieceDrawOrder.reserve(gameSnapshot.pieces.size());
        for (const game_data::Piece& piece : gameSnapshot.pieces)
        {
            if (gameDragActive && draggedPiece && piece.id == draggedPiece->id)
            {
                continue;
            }
            pieceDrawOrder.push_back(&piece);
        }
        std::sort(pieceDrawOrder.begin(), pieceDrawOrder.end(), [&](const game_data::Piece* a, const game_data::Piece* b) {
            const BoardCellMetrics aCell = boardCellMetrics(a->row, a->column);
            const BoardCellMetrics bCell = boardCellMetrics(b->row, b->column);
            if (aCell.screenRow != bCell.screenRow)
            {
                return aCell.screenRow < bCell.screenRow;
            }
            return a->column < b->column;
        });

        for (const game_data::Piece* piecePtr : pieceDrawOrder)
        {
            const game_data::Piece& piece = *piecePtr;
            BoardCellMetrics cell = boardCellMetrics(piece.row, piece.column);
            sf::Vector2f anchor = boardCellAnchor(cell);
            float pieceScale = cell.depthScale;
            bool isMoving = false;
            float walkAnimationElapsed = 0.0f;
            float attackAnimationProgress = -1.0f;
            std::optional<sf::Vector2f> attackImpactAnchor;
            if (const auto animation = pieceMoveAnimations.find(piece.id); animation != pieceMoveAnimations.end())
            {
                walkAnimationElapsed = std::max(0.0f, animationTime - animation->second.startTime);
                const float progress = std::min(walkAnimationElapsed / animation->second.duration, 1.0f);
                if (progress < 1.0f)
                {
                    isMoving = true;
                    const BoardCellMetrics startCell = boardCellMetricsForViewer(
                        animation->second.fromRow, animation->second.fromColumn, gameSnapshot.yourPlayer);
                    const BoardCellMetrics endCell = boardCellMetricsForViewer(
                        animation->second.toRow, animation->second.toColumn, gameSnapshot.yourPlayer);
                    const sf::Vector2f start = boardCellAnchor(startCell);
                    const sf::Vector2f end = boardCellAnchor(endCell);
                    anchor = {
                        start.x + (end.x - start.x) * progress,
                        start.y + (end.y - start.y) * progress};
                    pieceScale = startCell.depthScale + (endCell.depthScale - startCell.depthScale) * progress;
                }
                else
                {
                    pieceMoveAnimations.erase(piece.id);
                }
            }

            if (const auto animation = pieceAttackAnimations.find(piece.id); animation != pieceAttackAnimations.end())
            {
                const float attackElapsed = std::max(0.0f, animationTime - animation->second.startTime);
                const float progress = std::min(attackElapsed / animation->second.duration, 1.0f);
                if (progress < 1.0f)
                {
                    const BoardCellMetrics targetCell = boardCellMetricsForViewer(
                        animation->second.targetRow,
                        animation->second.targetColumn,
                        gameSnapshot.yourPlayer);
                    const sf::Vector2f targetAnchor = boardCellAnchor(targetCell);
                    attackImpactAnchor = targetAnchor;
                    attackAnimationProgress = progress;

                    const float dx = targetAnchor.x - anchor.x;
                    const float dy = targetAnchor.y - anchor.y;
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    if (distance > 0.001f)
                    {
                        const float lunge = std::sin(progress * Pi) * AttackLungePixels * pieceScale;
                        const float shake = std::sin(progress * Pi * 6.0f) *
                            AttackShakePixels * pieceScale * (1.0f - progress);
                        anchor.x += dx / distance * lunge;
                        anchor.y += dy / distance * lunge + shake;
                        pieceScale *= 1.0f + 0.045f * std::sin(progress * Pi);
                    }
                }
                else
                {
                    pieceAttackAnimations.erase(piece.id);
                }
            }

            const bool pieceUnavailable =
                (piece.hasActed && piece.owner == gameSnapshot.activePlayer) || piece.disabledTurns > 0;
            sf::Color color = ownerColor(piece.owner);
            if (pieceUnavailable)
            {
                color = sf::Color(static_cast<std::uint8_t>(color.r * 0.55f),
                                  static_cast<std::uint8_t>(color.g * 0.55f),
                                  static_cast<std::uint8_t>(color.b * 0.55f));
            }

            const std::string& walkPath = pieceWalkAnimPath(piece);
            const std::string& tokenPath = pieceTokenPath(piece);
            const sf::Color pieceTint = pieceUnavailable
                ? sf::Color(150, 150, 150, 215)
                : sf::Color::White;
            int walkFrame = -1;
            if (isMoving)
            {
                const int walkFrameCount = std::max(1, piece.walkAnimFrames);
                const float loopProgress =
                    std::fmod(walkAnimationElapsed, WalkAnimationLoopSeconds) /
                    WalkAnimationLoopSeconds;
                walkFrame = std::min(
                    static_cast<int>(loopProgress * static_cast<float>(walkFrameCount)),
                    walkFrameCount - 1);
            }
            bool drewPiece = drawPieceVisual(
                tokenPath,
                walkPath,
                pieceBasePath(piece),
                piece.separateBaseArt,
                piece.separateBaseArt && piece.owner == 2,
                piece.walkAnimFrames,
                anchor,
                pieceScale,
                pieceTint,
                walkFrame);
            if (!drewPiece)
            {
                if (sf::Texture* art = cardArtTexture(piece.imagePath))
                {
                    drawContainSprite(window, *art, pieceTargetRect(anchor, pieceScale, false),
                                      pieceUnavailable
                                          ? sf::Color(130, 130, 130)
                                          : sf::Color::White);
                    drewPiece = true;
                }
            }
            if (!drewPiece)
            {
                const float radius = PieceBaseWidth * 0.28f * pieceScale;
                sf::CircleShape body(radius);
                body.setPosition({anchor.x - radius, anchor.y - radius * 2.0f});
                body.setFillColor(color);
                window.draw(body);
            }
            if (attackImpactAnchor && attackAnimationProgress >= 0.22f && attackAnimationProgress <= 0.78f)
            {
                const float flashProgress = (attackAnimationProgress - 0.22f) / 0.56f;
                const float flash = std::sin(flashProgress * Pi);
                const float radius = (10.0f + 11.0f * flash) * pieceScale;
                const auto alpha = static_cast<std::uint8_t>(std::clamp(210.0f * flash, 0.0f, 210.0f));
                sf::CircleShape ring(radius);
                ring.setPosition({attackImpactAnchor->x - radius, attackImpactAnchor->y - radius});
                ring.setFillColor(sf::Color::Transparent);
                ring.setOutlineThickness(std::max(2.0f, 3.0f * pieceScale));
                ring.setOutlineColor(sf::Color(255, 228, 126, alpha));
                window.draw(ring);

                const sf::Vector2f slashSize{
                    std::max(18.0f, radius * 1.45f),
                    std::max(2.0f, 4.0f * pieceScale)};
                sf::RectangleShape slashA(slashSize);
                slashA.setOrigin({slashSize.x * 0.5f, slashSize.y * 0.5f});
                slashA.setPosition(*attackImpactAnchor);
                slashA.setRotation(sf::degrees(45.0f));
                slashA.setFillColor(sf::Color(255, 246, 188, alpha));
                window.draw(slashA);

                sf::RectangleShape slashB(slashSize);
                slashB.setOrigin({slashSize.x * 0.5f, slashSize.y * 0.5f});
                slashB.setPosition(*attackImpactAnchor);
                slashB.setRotation(sf::degrees(-45.0f));
                slashB.setFillColor(sf::Color(255, 202, 102, alpha));
                window.draw(slashB);
            }
            const unsigned int healthSize = static_cast<unsigned int>(std::clamp(12.0f * pieceScale, 10.0f, 17.0f));
            drawText(window, font, std::to_string(piece.health), healthSize,
                     {anchor.x - 5.0f * pieceScale, anchor.y - 21.0f * pieceScale}, sf::Color(248, 239, 216));
        }

        // Compact game readout.
        const game_data::PlayerSnapshot& mine = gameSnapshot.players[static_cast<std::size_t>(me - 1)];
        const int activePlayer = std::clamp(gameSnapshot.activePlayer, 1, 2);
        const game_data::PlayerSnapshot& activePlayerSnapshot =
            gameSnapshot.players[static_cast<std::size_t>(activePlayer - 1)];
        const std::string activePlayerName = storyMode
            ? "Tinkering Tom"
            : (sandboxMode
            ? "Player " + std::to_string(activePlayer)
            : (activePlayer == me ? loggedInUsername : "Opponent"));
        const std::string steamText = storyMode ? "story" : (sandboxMode ? "free" : std::to_string(activePlayerSnapshot.steam));
        drawText(window, font, "Turn: " + activePlayerName, 16, {BoardOriginX, GameLabelY},
                 ownerColor(activePlayer), 240.0f);
        drawText(window, font, "Steam: " + steamText, 16, {282.0f, GameLabelY},
                 sf::Color(150, 210, 235), 100.0f);

        if (phase == game_data::Phase::Playing && (sandboxMode || gameSnapshot.activePlayer == me))
        {
            if (selectedPiece && (sandboxMode || (selectedPiece->owner == me && !selectedPiece->hasActed)) &&
                game_data::pieceAbilityAvailable(*selectedPiece))
            {
                abilityButton.setLabel(game_data::pieceAbilityLabel(*selectedPiece));
                abilityButton.draw(window);
            }
            if (sandboxMode && !storyMode)
            {
                sandboxPlayerButton.draw(window);
                sandboxAdvanceTurnButton.draw(window);
            }
            else
            {
                endTurnButton.draw(window);
            }
        }
        leaveGameButton.draw(window);

        if (storyMode)
        {
            drawPanel(window, {24.0f, 506.0f}, {752.0f, 74.0f});
            if (storyStage == StoryStage::MoveTutorial)
            {
                drawText(window, font, "Move Tinkering Tom", 18, {44.0f, 516.0f}, sf::Color(248, 224, 172), 220.0f);
                drawWrappedText(
                    window,
                    font,
                    "Click Tom, then click the glowing square beside him. You can also drag him there.",
                    15,
                    {44.0f, 542.0f},
                    sf::Color(220, 224, 230),
                    650.0f,
                    3.0f);
            }
            else if (storyStage == StoryStage::ValveChallenge)
            {
                drawText(window, font, "Repair the Valve", 18, {44.0f, 516.0f}, sf::Color(248, 224, 172), 220.0f);
                drawWrappedText(
                    window,
                    font,
                    "Guide Tom to the glowing valve. He moves one square per step, so choose a clear path.",
                    15,
                    {44.0f, 542.0f},
                    sf::Color(220, 224, 230),
                    650.0f,
                    3.0f);
            }
            else
            {
                drawText(window, font, "First Part Complete", 18, {44.0f, 516.0f}, sf::Color(120, 220, 150), 260.0f);
                drawWrappedText(
                    window,
                    font,
                    "Tom steadies the marshworks. Leave returns to the menu; the story will continue later.",
                    15,
                    {44.0f, 542.0f},
                    sf::Color(220, 224, 230),
                    650.0f,
                    3.0f);
            }
        }

        // Hand.
        if (!storyMode)
        {
            clampListOffset(gameHandOffset, gameSnapshot.hand.size(), VisibleGameHandCards);
            const std::size_t lastHandCard = std::min(gameSnapshot.hand.size(), gameHandOffset + VisibleGameHandCards);
            if (gameSnapshot.hand.size() > VisibleGameHandCards)
            {
                drawText(
                    window,
                    font,
                    "Cards " + std::to_string(gameHandOffset + 1) + "-" +
                        std::to_string(lastHandCard) + "/" + std::to_string(gameSnapshot.hand.size()),
                    12,
                    {HandStartX, HandY - 18.0f},
                    sf::Color(190, 198, 214),
                    240.0f);
            }
            for (std::size_t i = gameHandOffset; i < lastHandCard; ++i)
            {
                const float x = HandStartX + static_cast<float>(i - gameHandOffset) * (HandCardWidth + HandGap);
                const game_data::GameCard& card = gameSnapshot.hand[i];
                const bool affordable = phase == game_data::Phase::HeroPlacement ||
                    ((sandboxMode || card.cost <= mine.steam) && (sandboxMode || gameSnapshot.activePlayer == me) &&
                     phase == game_data::Phase::Playing &&
                     (sandboxMode || game_data::heroKeywordsAllowCard(gameSnapshot.pieces, me, card)));
                drawGameCardFace({x, HandY}, card, selectedHandIndex && *selectedHandIndex == i, affordable);
            }
        }

        if (gameDragActive)
        {
            if (gameDragKind == GameDragKind::HandCard && draggingHandIndex &&
                *draggingHandIndex < gameSnapshot.hand.size())
            {
                const game_data::GameCard& draggedCard = gameSnapshot.hand[*draggingHandIndex];
                if (draggedCard.type == "Unit" || draggedCard.type == "Hero")
                {
                    sf::Vector2f anchor = gameDragCurrentPos;
                    float scale = 1.0f;
                    if (draggedHandSquare)
                    {
                        const BoardCellMetrics metrics =
                            boardCellMetrics(draggedHandSquare->first, draggedHandSquare->second);
                        anchor = boardCellAnchor(metrics);
                        scale = metrics.depthScale;
                    }
                    drawCardPiecePreview(draggedCard, sandboxPlayer, anchor, scale, draggedHandDropValid);
                }
                else
                {
                    const bool affordable = (sandboxMode || draggedCard.cost <= mine.steam) &&
                        (sandboxMode || gameSnapshot.activePlayer == me) && phase == game_data::Phase::Playing &&
                        (sandboxMode || game_data::heroKeywordsAllowCard(gameSnapshot.pieces, me, draggedCard));
                    drawGameCardFace(
                        {gameDragCurrentPos.x - HandCardWidth / 2.0f,
                         gameDragCurrentPos.y - HandCardHeight / 2.0f},
                        draggedCard,
                        true,
                        affordable);
                }
            }
            else if (gameDragKind == GameDragKind::Piece && draggedPiece)
            {
                sf::Vector2f anchor = gameDragCurrentPos;
                float scale = 1.0f;
                if (draggedPieceSquare)
                {
                    const BoardCellMetrics metrics =
                        boardCellMetrics(draggedPieceSquare->first, draggedPieceSquare->second);
                    anchor = boardCellAnchor(metrics);
                    scale = metrics.depthScale;
                }
                const sf::Color tint = draggedPieceDropValid
                    ? sf::Color(255, 255, 255, 220)
                    : sf::Color(220, 120, 110, 190);
                bool drewPiece = drawPieceVisual(
                    pieceTokenPath(*draggedPiece),
                    pieceWalkAnimPath(*draggedPiece),
                    pieceBasePath(*draggedPiece),
                    draggedPiece->separateBaseArt,
                    draggedPiece->separateBaseArt && draggedPiece->owner == 2,
                    draggedPiece->walkAnimFrames,
                    anchor,
                    scale,
                    tint,
                    -1);
                if (!drewPiece)
                {
                    if (sf::Texture* art = cardArtTexture(draggedPiece->imagePath))
                    {
                        drawContainSprite(window, *art, pieceTargetRect(anchor, scale, false), tint);
                        drewPiece = true;
                    }
                }

                if (!drewPiece)
                {
                    const float radius = PieceBaseWidth * 0.28f * scale;
                    sf::CircleShape body(radius);
                    body.setPosition({anchor.x - radius, anchor.y - radius * 2.0f});
                    body.setFillColor(
                        draggedPieceDropValid
                            ? ownerColor(draggedPiece->owner)
                            : sf::Color(180, 75, 65, 210));
                    window.draw(body);
                }

                const unsigned int healthSize =
                    static_cast<unsigned int>(std::clamp(12.0f * scale, 10.0f, 17.0f));
                drawText(
                    window,
                    font,
                    std::to_string(draggedPiece->health),
                    healthSize,
                    {anchor.x - 5.0f * scale, anchor.y - 21.0f * scale},
                    sf::Color(248, 239, 216, 220));
            }
        }

        if (storyMode)
        {
            drawPiecePopup();
            return;
        }

        // Game-over banner.
        if (phase == game_data::Phase::GameOver)
        {
            drawBeveledPlate(
                window,
                {40.0f, 210.0f},
                {420.0f, 126.0f},
                sf::Color(20, 24, 24, 235),
                gameSnapshot.winner == me ? sf::Color(120, 220, 150) : sf::Color(220, 110, 90),
                true,
                12.0f);
            const std::string result = gameSnapshot.winner == me ? "Victory!" : "Defeat";
            drawText(window, font, result, 34, {60.0f, 224.0f}, gameSnapshot.winner == me ? sf::Color(140, 230, 160) : sf::Color(230, 130, 110));
            const std::string ratingText = gameResultReceived && gameResultSuccess
                ? "Rating " +
                    std::string(gameRatingChange >= 0 ? "+" : "") +
                    std::to_string(gameRatingChange)
                : (gameResultReceived
                    ? "Rating update unavailable"
                    : "Rating update pending...");
            drawText(
                window,
                font,
                ratingText,
                18,
                {60.0f, 264.0f},
                sf::Color(151, 192, 255),
                360.0f);
            if (!gameRewardText.empty())
            {
                drawText(window, font, gameRewardText, 16, {60.0f, 288.0f}, sf::Color(248, 214, 112), 360.0f);
            }
            drawText(window, font, "Press Leave to return.", 14, {60.0f, 314.0f}, sf::Color(200, 206, 220));
        }

        drawPiecePopup();
    };

