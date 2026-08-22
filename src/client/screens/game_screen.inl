    // A small chamfered plate, the shape the rest of the game's chrome uses.
    auto drawCutPlate = [&](sf::Vector2f position,
                            sf::Vector2f size,
                            float cut,
                            sf::Color fill,
                            sf::Color outline,
                            float outlineThickness = 1.0f) {
        const float chamfer = std::min({cut, size.x * 0.5f, size.y * 0.5f});
        sf::ConvexShape plate(8);
        plate.setPoint(0, {position.x + chamfer, position.y});
        plate.setPoint(1, {position.x + size.x - chamfer, position.y});
        plate.setPoint(2, {position.x + size.x, position.y + chamfer});
        plate.setPoint(3, {position.x + size.x, position.y + size.y - chamfer});
        plate.setPoint(4, {position.x + size.x - chamfer, position.y + size.y});
        plate.setPoint(5, {position.x + chamfer, position.y + size.y});
        plate.setPoint(6, {position.x, position.y + size.y - chamfer});
        plate.setPoint(7, {position.x, position.y + chamfer});
        plate.setFillColor(fill);
        plate.setOutlineThickness(outlineThickness);
        plate.setOutlineColor(outline);
        window.draw(plate);
    };

    // Compact health pips sit inside the lower face of the plinth. A restrained
    // dark backing keeps the pips legible over both bright fey tokens and dark
    // enemy silhouettes without placing a separate status bar across their art.
    auto drawPieceHealthPips = [&](sf::Vector2f anchor,
                                   float scale,
                                   int health,
                                   int maxHealth,
                                   int owner,
                                   bool dimmed) {
        const float dim = dimmed ? 0.58f : 1.0f;
        const int pipCount = std::clamp(maxHealth, 1, 8);
        const float fraction = maxHealth > 0
            ? std::clamp(static_cast<float>(health) / static_cast<float>(maxHealth), 0.0f, 1.0f)
            : 0.0f;
        const int filledPips = health > 0
            ? std::clamp(static_cast<int>(std::ceil(fraction * static_cast<float>(pipCount))), 1, pipCount)
            : 0;
        const float radius = std::clamp(2.1f * scale, 1.35f, 2.8f);
        const float spacing = radius * 1.85f;
        const float startX = anchor.x - spacing * static_cast<float>(pipCount - 1) * 0.5f;
        const float pipY = anchor.y + (PieceBasePipOffset - PieceBaseLift) * scale;
        const float backingWidth = spacing * static_cast<float>(pipCount - 1) + radius * 2.8f;
        drawSoftEllipse(
            window,
            {anchor.x, pipY + radius * 0.12f},
            backingWidth * 0.5f,
            radius * 1.45f,
            sf::Color(3, 7, 8, dimmed ? 154 : 204),
            4);
        drawEllipseOutline(
            window,
            {anchor.x, pipY + radius * 0.12f},
            backingWidth * 0.5f,
            radius * 1.45f,
            std::max(0.6f, 0.8f * scale),
            withAlpha(shadeColor(ownerColorBright(owner), dim), dimmed ? 118 : 184));
        for (int index = 0; index < pipCount; ++index)
        {
            sf::CircleShape pip(radius, 12);
            pip.setOrigin({radius, radius});
            pip.setScale({1.0f, 0.68f});
            pip.setPosition({startX + spacing * static_cast<float>(index), pipY});
            const bool filled = index < filledPips;
            pip.setFillColor(filled
                ? withAlpha(shadeColor(ownerColorBright(owner), dim), dimmed ? 192 : 250)
                : withAlpha(shadeColor(BoardPlate, dim), 220));
            pip.setOutlineThickness(std::max(0.65f, 0.9f * scale));
            pip.setOutlineColor(withAlpha(shadeColor(BoardBrass, dim), filled ? 220 : 130));
            window.draw(pip);
        }
    };

    // Stand-in for a piece whose token art is missing. A framed cameo standing on
    // the plinth reads as deliberate; the flat owner-coloured circle it replaces
    // read as an unfinished asset.
    auto drawPieceCameo = [&](sf::Vector2f anchor,
                              float scale,
                              int owner,
                              const std::string& imagePath,
                              sf::Color tint) {
        const float width = 46.0f * scale;
        const float height = 56.0f * scale;
        const sf::Vector2f position{
            anchor.x - width * 0.5f,
            anchor.y + PieceStandOffset * scale - height - 4.0f * scale};

        drawCutPlate(
            position,
            {width, height},
            7.0f * scale,
            withAlpha(ownerColorDeep(owner), tint.a),
            withAlpha(BoardBrass, tint.a),
            1.6f);
        if (sf::Texture* art = cardArtTexture(imagePath))
        {
            drawContainSprite(
                window,
                *art,
                {{position.x + 3.5f * scale, position.y + 3.5f * scale},
                 {width - 7.0f * scale, height - 7.0f * scale}},
                tint);
        }
        else
        {
            // No art at all: an engraved arcane sigil rather than a blank plate.
            const sf::Vector2f center{anchor.x, position.y + height * 0.5f};
            drawEllipseOutline(
                window, center, width * 0.24f, width * 0.24f, 1.6f,
                withAlpha(BoardArcane, tint.a));
            drawEllipseOutline(
                window, center, width * 0.12f, width * 0.12f, 1.2f,
                withAlpha(BoardBrassBright, static_cast<int>(tint.a * 0.7f)));
        }
        drawCutPlate(
            {position.x + 2.0f * scale, position.y + 2.0f * scale},
            {width - 4.0f * scale, height - 4.0f * scale},
            5.0f * scale,
            sf::Color::Transparent,
            withAlpha(BoardBrassBright, static_cast<int>(tint.a * 0.5f)),
            1.0f);
    };

    // A hand card. Laid out like a physical card — art window on top, name band,
    // then stat pips — rather than the stack of debug lines ("HP 8", "Actions 2")
    // over a 30px thumbnail that it replaces.
    auto drawGameCardFace = [&](sf::Vector2f position,
                               const game_data::GameCard& card,
                               bool selected,
                               bool affordable) {
        const float width = HandCardWidth;
        const float height = HandCardHeight;
        const sf::Color frame = selected
            ? BoardBrassBright
            : (affordable ? BoardBrass : sf::Color(92, 76, 58));
        const sf::Color artTint = affordable ? sf::Color::White : sf::Color(122, 118, 116);
        const sf::Color inkColor = affordable
            ? BoardParchment
            : withAlpha(BoardParchmentMuted, 190);

        if (selected)
        {
            // Lifted card: a warm bloom behind it reads as "picked up".
            drawCutPlate(
                {position.x - 3.0f, position.y - 3.0f},
                {width + 6.0f, height + 6.0f},
                9.0f,
                withAlpha(BoardBrassBright, 46),
                withAlpha(BoardBrassBright, 170),
                1.4f);
        }
        drawCutPlate(
            {position.x + 2.0f, position.y + 3.0f},
            {width, height},
            7.0f,
            sf::Color(0, 0, 0, 150),
            sf::Color::Transparent,
            0.0f);
        drawCutPlate(
            position,
            {width, height},
            7.0f,
            selected ? sf::Color(42, 32, 20, 246) : sf::Color(16, 22, 23, 244),
            frame,
            1.6f);

        // Art window fills the upper half, framed and seated in a recess.
        const sf::Vector2f artPosition{position.x + 5.0f, position.y + 5.0f};
        const sf::Vector2f artSize{width - 10.0f, height * 0.47f};
        drawCutPlate(artPosition, artSize, 4.0f, sf::Color(6, 10, 11), sf::Color(52, 40, 26), 1.0f);
        if (sf::Texture* art = cardArtTexture(card.imagePath))
        {
            drawCoverSprite(
                window, *art, {{artPosition.x + 1.0f, artPosition.y + 1.0f},
                               {artSize.x - 2.0f, artSize.y - 2.0f}}, artTint);
        }
        // Gradient scrim along the art's lower edge, so the name band never sits
        // on a bright patch of illustration.
        sf::VertexArray scrim(sf::PrimitiveType::TriangleFan, 4);
        const float scrimTop = artPosition.y + artSize.y * 0.55f;
        const float scrimBottom = artPosition.y + artSize.y - 1.0f;
        scrim[0] = {{artPosition.x + 1.0f, scrimTop}, sf::Color(6, 10, 11, 0)};
        scrim[1] = {{artPosition.x + artSize.x - 1.0f, scrimTop}, sf::Color(6, 10, 11, 0)};
        scrim[2] = {{artPosition.x + artSize.x - 1.0f, scrimBottom}, sf::Color(6, 10, 11, 226)};
        scrim[3] = {{artPosition.x + 1.0f, scrimBottom}, sf::Color(6, 10, 11, 226)};
        window.draw(scrim);

        // Name band across the art's foot. Wrapped over two lines rather than
        // elided, so a card is identifiable without hovering it.
        const std::vector<std::string> nameLines =
            wrapText(font, card.title, 9, width - 12.0f);
        const std::size_t shownLines = std::min<std::size_t>(2, nameLines.size());
        for (std::size_t line = 0; line < shownLines; ++line)
        {
            const bool lastShown = line + 1 == shownLines;
            const bool truncated = lastShown && nameLines.size() > shownLines;
            sf::Text nameText(
                font,
                truncated ? elideToWidth(font, nameLines[line] + "...", 9, width - 12.0f)
                          : nameLines[line],
                9);
            nameText.setFillColor(inkColor);
            nameText.setOutlineThickness(1.0f);
            nameText.setOutlineColor(sf::Color(0, 0, 0, 210));
            centerText(
                nameText,
                {position.x + width * 0.5f,
                 scrimBottom - 6.0f -
                     static_cast<float>(shownLines - 1 - line) * 10.0f});
            drawCrispText(window, nameText);
        }

        drawSeparatorRule(
            window, {position.x + 7.0f, artPosition.y + artSize.y + 4.0f}, width - 14.0f);

        // Stat pip: card health for bodies, an effect line for spells.
        const float statY = artPosition.y + artSize.y + 11.0f;
        const auto drawStatPip = [&](sf::Vector2f pipPosition,
                                     float pipWidth,
                                     const std::string& glyph,
                                     const std::string& value,
                                     sf::Color accent) {
            drawCutPlate(
                pipPosition,
                {pipWidth, 17.0f},
                5.0f,
                sf::Color(8, 13, 14, 232),
                withAlpha(accent, affordable ? 200 : 130),
                1.0f);
            sf::Text glyphText(font, glyph, 8);
            glyphText.setFillColor(withAlpha(accent, affordable ? 230 : 150));
            glyphText.setPosition({pipPosition.x + 4.0f, pipPosition.y + 3.0f});
            drawCrispText(window, glyphText);
            sf::Text valueText(font, value, 12);
            valueText.setFillColor(inkColor);
            centerText(
                valueText,
                {pipPosition.x + pipWidth * 0.66f, pipPosition.y + 8.5f});
            drawCrispText(window, valueText);
        };

        if (card.type == "Unit" || card.type == "Hero")
        {
            drawStatPip(
                {position.x + 7.0f, statY}, width - 14.0f, "HP", std::to_string(card.health),
                sf::Color(132, 198, 122));
        }
        else
        {
            std::string effectLabel;
            if (card.effect == "damage")
            {
                effectLabel = "Deal " + std::to_string(card.power);
            }
            else if (card.effect == "heal")
            {
                effectLabel = "Heal " + std::to_string(card.power);
            }
            else if (game_data::isResourcesEffect(card))
            {
                effectLabel = "+" + std::to_string(card.power) + " Resources";
            }
            else if (card.effect == "resourceDrain")
            {
                effectLabel = "Drain " + std::to_string(card.power);
            }
            if (!effectLabel.empty())
            {
                drawStatPip(
                    {position.x + 7.0f, statY}, width - 14.0f, "",
                    effectLabel, sf::Color(186, 138, 234));
            }
        }

        // Type footer.
        sf::Text typeText(
            font,
            elideToWidth(font, card.type == "Hero" ? std::string("HERO") : card.type, 8, width - 14.0f),
            8);
        typeText.setLetterSpacing(1.2f);
        typeText.setFillColor(withAlpha(BoardParchmentMuted, affordable ? 200 : 140));
        centerText(typeText, {position.x + width * 0.5f, position.y + height - 8.0f});
        drawCrispText(window, typeText);

        // Cost gem, overhanging the top-left corner the way a mana crystal does.
        const int displayedCost = card.type == "Hero" ? card.heroCost : card.cost;
        const sf::Vector2f gemCenter{position.x + 2.0f, position.y + 2.0f};
        sf::CircleShape gemShadow(12.0f, 20);
        gemShadow.setOrigin({12.0f, 12.0f});
        gemShadow.setPosition({gemCenter.x + 1.0f, gemCenter.y + 1.5f});
        gemShadow.setFillColor(sf::Color(0, 0, 0, 160));
        window.draw(gemShadow);
        sf::CircleShape gem(11.0f, 20);
        gem.setOrigin({11.0f, 11.0f});
        gem.setPosition(gemCenter);
        gem.setFillColor(affordable ? sf::Color(38, 62, 74, 250) : sf::Color(46, 38, 34, 250));
        gem.setOutlineThickness(1.6f);
        gem.setOutlineColor(affordable ? BoardBrassBright : sf::Color(104, 84, 62));
        window.draw(gem);
        sf::Text costText(font, std::to_string(displayedCost), 13);
        costText.setFillColor(affordable ? BoardParchment : withAlpha(BoardParchmentMuted, 200));
        costText.setOutlineThickness(1.0f);
        costText.setOutlineColor(sf::Color(0, 0, 0, 190));
        centerText(costText, gemCenter);
        drawCrispText(window, costText);

        if (!affordable)
        {
            // Unplayable cards take a cool scrim, so affordability reads without
            // having to compare the gem against the resources figure.
            drawCutPlate(
                position, {width, height}, 7.0f, sf::Color(8, 14, 20, 104),
                sf::Color::Transparent, 0.0f);
        }
    };

    auto piecePopupActionDescriptions = [&](const game_data::Piece& piece) {
        DetailRows descriptions;
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
        if (piece.controlTurnsRemaining > 0)
        {
            descriptions.push_back({"Under control: " + std::to_string(piece.controlTurnsRemaining) + " turns",
                                    ownerColor(piece.owner)});
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
        if (piece.gatherResources > 0)
        {
            descriptions.push_back({"Passive: +" + std::to_string(piece.gatherResources) + " Resources each turn",
                                    sf::Color(190, 198, 214)});
        }
        if (piece.healingAura > 0)
        {
            descriptions.push_back({"Healing aura: +" + std::to_string(piece.healingAura) +
                                        " health to adjacent units at turn end",
                                    sf::Color(190, 198, 214)});
        }
        for (const game_data::Enchantment& enchantment : gameSnapshot.enchantments)
        {
            if (enchantment.target == static_cast<std::uint8_t>(game_data::EnchantmentTarget::Piece) &&
                enchantment.targetPieceId == piece.id)
            {
                descriptions.push_back({
                    "Enchanted: " + enchantment.title + " (+" +
                        std::to_string(enchantment.power) + " " + enchantment.effect + ")",
                    sf::Color(223, 164, 255)});
            }
        }
        if (piece.actions.empty())
        {
            descriptions.push_back({"Actions: none", sf::Color(225, 170, 150)});
        }
        for (std::size_t i = 0; i < piece.actions.size(); ++i)
        {
            descriptions.push_back(actionDetailRow(
                piece.actions[i],
                i,
                piece.actions[i].state == piece.actionState
                    ? sf::Color(143, 220, 205)
                    : sf::Color(190, 198, 214)));
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
            return "Play: " + std::to_string(card.cost) + " Resources, controlled empty square";
        }
        if (card.type == "Enchantment")
        {
            return "Play: " + std::to_string(card.cost) + " Resources, attach to " + card.target;
        }
        if (card.effect == "damage")
        {
            return "Play: " + std::to_string(card.cost) + " Resources, deal " +
                std::to_string(card.power) + " damage";
        }
        if (card.effect == "heal")
        {
            return "Play: " + std::to_string(card.cost) + " Resources, restore " +
                std::to_string(card.power) + " health";
        }
        if (game_data::isResourcesEffect(card))
        {
            return "Play: " + std::to_string(card.cost) + " Resources, gain " +
                std::to_string(card.power) + " Resources";
        }
        return std::string("Play: ") + std::to_string(card.cost) + " Resources";
    };

    auto cardPopupActionDescriptions = [&](const game_data::GameCard& card) {
        DetailRows descriptions;
        descriptions.push_back({cardPlayDescription(card), sf::Color(210, 216, 228)});
        if (card.type == "Unit" || card.type == "Hero")
        {
            if (card.actions.empty())
            {
                descriptions.push_back({"Actions: none", sf::Color(225, 170, 150)});
            }
            for (std::size_t i = 0; i < card.actions.size(); ++i)
            {
                descriptions.push_back(actionDetailRow(card.actions[i], i));
            }
        }
        return descriptions;
    };

    auto popupActionContentHeight = [&](const DetailRows& descriptions) {
        return detailRowsHeight(descriptions, PiecePopupTextWidth);
    };

    auto popupMaxScroll = [&](const DetailRows& descriptions) {
        return std::max(0.0f, popupActionContentHeight(descriptions) - PiecePopupScrollHeight);
    };

    // A caption under one of the bottom-bar slots.
    auto drawSlotCaption = [&](float centerX, float y, const std::string& caption, sf::Color color) {
        sf::Text text(font, caption, 8);
        text.setLetterSpacing(1.2f);
        text.setFillColor(color);
        centerText(text, {centerX, y});
        drawCrispText(window, text);
    };

    // The draw pile, as a stack of card backs. Previously the deck simply was not
    // shown, so players had no idea how close they were to running out.
    auto drawDrawPile = [&](int remaining) {
        const float centerX = GameDeckPileX + GamePileWidth * 0.5f;
        const int layers = remaining <= 0 ? 1 : std::min(3, 1 + remaining / 8);
        for (int i = layers - 1; i >= 0; --i)
        {
            const float offset = static_cast<float>(i) * 2.2f;
            const bool top = i == 0;
            drawCutPlate(
                {GameDeckPileX - offset, GamePileY - offset},
                {GamePileWidth, GamePileHeight - 14.0f},
                6.0f,
                remaining > 0 ? sf::Color(18, 25, 30, 246) : sf::Color(18, 20, 21, 210),
                remaining > 0 ? (top ? BoardBrass : BoardBrassDim) : sf::Color(72, 62, 50),
                top ? 1.5f : 1.0f);
        }
        // Keep the remaining-card count as the only content on the face, centered
        // in the tall slot now that the decorative sigil has been removed.
        sf::Text count(font, std::to_string(std::max(0, remaining)), 16);
        count.setFillColor(remaining > 0 ? BoardParchment : withAlpha(BoardParchmentMuted, 170));
        count.setOutlineThickness(1.0f);
        count.setOutlineColor(sf::Color(0, 0, 0, 200));
        centerText(count, {centerX, GamePileY + (GamePileHeight - 14.0f) * 0.5f});
        drawCrispText(window, count);

        drawSlotCaption(
            centerX, GamePileY + GamePileHeight - 5.0f, "DECK",
            withAlpha(BoardParchmentMuted, 206));
    };

    // The discard slot, which doubles as the drop target for pitching a card.
    auto drawDiscardTrashCan = [&](bool available, bool draggingCard, bool hovered) {
        const float centerX = TrashCanX + TrashCanWidth * 0.5f;
        const sf::Color accent = !available
            ? sf::Color(150, 82, 72)
            : (hovered ? BoardBrassBright : BoardBrass);

        if (draggingCard)
        {
            drawCutPlate(
                {TrashCanX - 4.0f, TrashCanY - 4.0f},
                {TrashCanWidth + 8.0f, TrashCanHeight - 14.0f + 8.0f},
                9.0f,
                withAlpha(accent, hovered ? 88 : 40),
                withAlpha(accent, hovered ? 240 : 160),
                hovered ? 2.0f : 1.2f);
        }
        drawCutPlate(
            {TrashCanX, TrashCanY},
            {TrashCanWidth, TrashCanHeight - 14.0f},
            6.0f,
            sf::Color(14, 19, 20, 240),
            available ? accent : sf::Color(78, 66, 56),
            1.5f);

        if (sf::Texture* trashCan = textures.load("ui/trash-can.png"))
        {
            const sf::Color iconTint = available
                ? (draggingCard && hovered ? sf::Color::White : sf::Color(226, 216, 198))
                : sf::Color(112, 104, 96, 190);
            drawContainSprite(
                window,
                *trashCan,
                {{TrashCanX + 12.0f, TrashCanY + 3.0f},
                 {TrashCanWidth - 24.0f, TrashCanHeight - 20.0f}},
                iconTint);
        }

        drawSlotCaption(
            centerX,
            TrashCanY + TrashCanHeight - 5.0f,
            "DISCARD",
            withAlpha(available ? BoardParchmentMuted : sf::Color(140, 110, 98), 206));
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

        const DetailRows actionDescriptions =
            piece ? piecePopupActionDescriptions(*piece) : cardPopupActionDescriptions(*card);

        if (!piece && !card)
        {
            return;
        }

        sf::RectangleShape overlay({ui_canvas::Width, ui_canvas::Height});
        overlay.setPosition({ui_canvas::Left, 0.0f});
        overlay.setFillColor(sf::Color(0, 0, 0, 150));
        window.draw(overlay);

        // The panel is sized to its detail list instead of using a fixed height,
        // which used to leave a large empty well under short entries. Clamping to
        // the constant keeps the scroll maths in the event handler in agreement.
        const float popupContentHeight =
            detailRowsHeight(actionDescriptions, PiecePopupTextWidth) + PiecePopupScrollTextYInset * 2.0f;
        const float popupScrollHeight =
            std::clamp(popupContentHeight, 74.0f, PiecePopupScrollHeight);
        const float popupHeight =
            (PiecePopupScrollY - PiecePopupY) + popupScrollHeight + 62.0f;
        const float popupButtonY = PiecePopupY + popupHeight - 54.0f;
        closePiecePopupButton.setPosition({PiecePopupX + 358.0f, popupButtonY});
        discardCardButton.setPosition({PiecePopupX + 22.0f, popupButtonY});

        drawPanel(window, {PiecePopupX, PiecePopupY}, {PiecePopupWidth, popupHeight});

        // ---- Header: portrait, name, owner, stat chips, trait tags ------------
        const int inspectedOwner = piece ? piece->owner : gameSnapshot.yourPlayer;
        const std::string inspectedType = piece
            ? (piece->isHero ? std::string("Hero") : std::string("Unit"))
            : card->type;

        const sf::Vector2f portraitPosition{PiecePopupX + 22.0f, PiecePopupY + 20.0f};
        const sf::Vector2f portraitSize{112.0f, 130.0f};
        drawBeveledPlate(
            window,
            portraitPosition,
            portraitSize,
            withAlpha(ownerColorDeep(inspectedOwner), 246),
            BoardBrass,
            false,
            8.0f);
        if (sf::Texture* art = cardArtTexture(piece ? piece->imagePath : card->imagePath))
        {
            drawCoverSprite(
                window,
                *art,
                {{portraitPosition.x + 6.0f, portraitPosition.y + 6.0f},
                 {portraitSize.x - 12.0f, portraitSize.y - 12.0f}});
        }
        else if (piece)
        {
            drawPieceVisual(
                pieceTokenPath(*piece),
                pieceWalkAnimPath(*piece),
                "",
                pieceBasePath(*piece),
                piece->owner == 2,
                piece->walkAnimFrames,
                1,
                {portraitPosition.x + portraitSize.x * 0.5f,
                 portraitPosition.y + portraitSize.y - 10.0f},
                1.05f,
                sf::Color::White,
                -1,
                -1);
        }
        drawCutPlate(
            {portraitPosition.x + 4.0f, portraitPosition.y + 4.0f},
            {portraitSize.x - 8.0f, portraitSize.y - 8.0f},
            6.0f,
            sf::Color::Transparent,
            withAlpha(BoardBrassBright, 90),
            1.0f);

        const float headerX = portraitPosition.x + portraitSize.x + 18.0f;
        const float headerWidth = PiecePopupX + PiecePopupWidth - 24.0f - headerX;

        sf::Text nameText(
            font,
            elideToWidth(font, piece ? piece->name : card->title, 23, headerWidth),
            23);
        nameText.setFillColor(BoardParchment);
        nameText.setPosition({headerX, PiecePopupY + 20.0f});
        drawCrispText(window, nameText);

        // Type and allegiance on one muted line, replacing the "Type: Unit" label
        // that was printed in a raw off-palette blue.
        sf::Text subtitle(
            font,
            inspectedType + std::string(piece
                // ASCII only: sf::Text reads std::string as Latin-1, so a UTF-8
                // separator would render as mojibake.
                ? (piece->owner == gameSnapshot.yourPlayer ? " / Yours" : " / Enemy")
                : " / In hand"),
            12);
        subtitle.setLetterSpacing(1.1f);
        subtitle.setFillColor(withAlpha(ownerColorBright(inspectedOwner), 230));
        subtitle.setPosition({headerX + 1.0f, PiecePopupY + 48.0f});
        drawCrispText(window, subtitle);

        // Stat chips: the numbers a player checks first, as figures rather than
        // sentences.
        const auto drawPopupStat = [&](sf::Vector2f chipPosition,
                                      float chipWidth,
                                      const std::string& caption,
                                      const std::string& value,
                                      sf::Color accent) {
            drawCutPlate(
                chipPosition,
                {chipWidth, 42.0f},
                6.0f,
                sf::Color(8, 13, 14, 232),
                withAlpha(accent, 190),
                1.2f);
            sf::Text captionText(font, caption, 8);
            captionText.setLetterSpacing(1.2f);
            captionText.setFillColor(withAlpha(BoardParchmentMuted, 216));
            centerText(captionText, {chipPosition.x + chipWidth * 0.5f, chipPosition.y + 10.0f});
            drawCrispText(window, captionText);
            sf::Text valueText(font, value, 17);
            valueText.setFillColor(accent);
            centerText(valueText, {chipPosition.x + chipWidth * 0.5f, chipPosition.y + 28.0f});
            drawCrispText(window, valueText);
        };

        struct PopupStat
        {
            std::string caption;
            std::string value;
            sf::Color accent;
        };
        std::vector<PopupStat> stats;
        if (piece)
        {
            stats.push_back({
                "HEALTH",
                std::to_string(piece->health) + "/" + std::to_string(piece->maxHealth),
                sf::Color(146, 210, 136)});
            if (piece->attack > 0)
            {
                stats.push_back({"ATTACK", std::to_string(piece->attack), sf::Color(226, 132, 108)});
            }
            if (piece->tax > 0)
            {
                stats.push_back({"TAX", std::to_string(piece->tax), BoardBrassBright});
            }
            if (piece->gatherResources > 0)
            {
                stats.push_back({
                    "GATHER", "+" + std::to_string(piece->gatherResources), BoardBrassBright});
            }
            if (piece->healingAura > 0)
            {
                stats.push_back({
                    "AURA", "+" + std::to_string(piece->healingAura), BoardBrassBright});
            }
        }
        else
        {
            stats.push_back({
                card->type == "Hero" ? "HERO COST" : "RESOURCES",
                std::to_string(card->type == "Hero" ? card->heroCost : card->cost),
                BoardBrassBright});
            if (card->type == "Unit" || card->type == "Hero")
            {
                stats.push_back({
                    "HEALTH", std::to_string(card->health), sf::Color(146, 210, 136)});
                if (card->tax > 0)
                {
                    stats.push_back({"TAX", std::to_string(card->tax), BoardBrassBright});
                }
                if (card->healingAura > 0)
                {
                    stats.push_back({"AURA", "+" + std::to_string(card->healingAura), BoardBrassBright});
                }
            }
            else
            {
                stats.push_back({
                    "POWER", std::to_string(card->power), sf::Color(186, 138, 234)});
                stats.push_back({
                    "TARGET", card->target, withAlpha(BoardParchmentMuted, 240)});
            }
        }
        if (!stats.empty())
        {
            const std::size_t shown = std::min<std::size_t>(4, stats.size());
            const float chipGap = 6.0f;
            const float chipWidth =
                (headerWidth - chipGap * static_cast<float>(shown - 1)) / static_cast<float>(shown);
            for (std::size_t i = 0; i < shown; ++i)
            {
                drawPopupStat(
                    {headerX + (chipWidth + chipGap) * static_cast<float>(i), PiecePopupY + 68.0f},
                    chipWidth,
                    stats[i].caption,
                    stats[i].value,
                    stats[i].accent);
            }
        }

        // Traits and keywords as tag pills, not a comma-joined label line.
        const std::vector<std::string>& traits = piece ? piece->traits : card->traits;
        const std::vector<std::string>& keywords = piece ? piece->keywords : card->keywords;
        {
            float tagX = headerX;
            float tagY = PiecePopupY + 118.0f;
            const auto drawTag = [&](const std::string& label, sf::Color accent) {
                sf::Text tagText(font, label, 10);
                const float tagWidth = tagText.getLocalBounds().size.x + 16.0f;
                if (tagX + tagWidth > headerX + headerWidth)
                {
                    tagX = headerX;
                    tagY += 22.0f;
                }
                if (tagY > PiecePopupY + 140.0f)
                {
                    return;
                }
                drawCutPlate(
                    {tagX, tagY}, {tagWidth, 18.0f}, 5.0f,
                    withAlpha(accent, 44), withAlpha(accent, 190), 1.0f);
                tagText.setFillColor(withAlpha(accent, 246));
                centerText(tagText, {tagX + tagWidth * 0.5f, tagY + 9.0f});
                drawCrispText(window, tagText);
                tagX += tagWidth + 5.0f;
            };
            for (const std::string& trait : traits)
            {
                drawTag(trait, BoardBrassBright);
            }
            for (const std::string& keyword : keywords)
            {
                drawTag(keyword, sf::Color(146, 206, 226));
            }
        }

        inspectedPieceScroll = std::clamp(inspectedPieceScroll, 0.0f, popupMaxScroll(actionDescriptions));

        sf::Text detailHeading(font, piece ? "DETAILS" : "ACTIONS", 13);
        detailHeading.setLetterSpacing(1.3f);
        detailHeading.setFillColor(withAlpha(BoardParchmentMuted, 240));
        detailHeading.setPosition({PiecePopupTextX, PiecePopupActionHeadingY});
        drawCrispText(window, detailHeading);
        drawSeparatorRule(
            window,
            {PiecePopupTextX + 70.0f, PiecePopupActionHeadingY + 9.0f},
            PiecePopupTextWidth - 70.0f);

        drawBeveledPlate(
            window,
            {PiecePopupTextX, PiecePopupScrollY},
            {PiecePopupTextWidth, popupScrollHeight},
            sf::Color(8, 14, 15, 132),
            sf::Color(96, 66, 35, 150),
            false,
            7.0f);

        std::optional<DetailTooltip> detailTooltip;
        const sf::View previousView = window.getView();
        sf::View actionView(sf::FloatRect(
            {PiecePopupTextX, PiecePopupScrollY + inspectedPieceScroll},
            {PiecePopupTextWidth, popupScrollHeight}));
        // Map the popup's logical rectangle through the active fixed canvas.
        // Hard-coded 800x600 fractions shift the child viewport left now that
        // the 16:9 view includes logical side gutters.
        const sf::FloatRect baseViewport = previousView.getViewport();
        const sf::Vector2f baseViewSize = previousView.getSize();
        const sf::Vector2f baseViewTopLeft =
            previousView.getCenter() - baseViewSize * 0.5f;
        const sf::Vector2f popupViewportPosition{
            (PiecePopupTextX - baseViewTopLeft.x) / baseViewSize.x,
            (PiecePopupScrollY - baseViewTopLeft.y) / baseViewSize.y};
        const sf::Vector2f popupViewportSize{
            PiecePopupTextWidth / baseViewSize.x,
            popupScrollHeight / baseViewSize.y};
        actionView.setViewport(sf::FloatRect(
            {baseViewport.position.x + baseViewport.size.x * popupViewportPosition.x,
             baseViewport.position.y + baseViewport.size.y * popupViewportPosition.y},
            {baseViewport.size.x * popupViewportSize.x,
             baseViewport.size.y * popupViewportSize.y}));
        window.setView(actionView);

        std::optional<sf::Vector2f> detailPointer;
        const sf::Vector2f pointer = collectionPointer();
        if (isInsideRect(
                pointer,
                PiecePopupTextX,
                PiecePopupScrollY,
                PiecePopupTextWidth,
                popupScrollHeight))
        {
            detailPointer = pointer + sf::Vector2f(0.0f, inspectedPieceScroll);
        }
        detailTooltip = drawDetailRows(
            actionDescriptions,
            PiecePopupScrollY + PiecePopupScrollTextYInset,
            PiecePopupTextX,
            PiecePopupTextWidth,
            detailPointer);

        window.setView(previousView);

        const float maxScroll = popupMaxScroll(actionDescriptions);
        if (maxScroll > 0.0f)
        {
            const float trackX = PiecePopupX + PiecePopupWidth - 22.0f;
            sf::RectangleShape track({4.0f, popupScrollHeight - 12.0f});
            track.setPosition({trackX, PiecePopupScrollY + 6.0f});
            track.setFillColor(sf::Color(73, 96, 98, 170));
            window.draw(track);

            const float thumbHeight = std::max(
                28.0f,
                track.getSize().y * (popupScrollHeight / (popupScrollHeight + maxScroll)));
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
        drawDetailTooltip(detailTooltip);
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
        if (!sandboxMode && !storyMode && !conquestBattleMode)
        {
            leaveGameButton.setLabel(
                phase == game_data::Phase::GameOver ? "Leave" : "Resign");
        }
        const game_data::Piece* selectedPiece = selectedPieceId ? gamePieceById(*selectedPieceId) : nullptr;
        const game_data::Piece* draggedPiece =
            gameDragKind == GameDragKind::Piece && draggingPieceId ? gamePieceById(*draggingPieceId) : nullptr;
        const game_data::Piece* actingPiece = draggedPiece ? draggedPiece : selectedPiece;
        const bool previewingNextTurn = actingPiece && !sandboxMode &&
            actingPiece->owner != gameSnapshot.activePlayer;
        std::optional<game_data::Piece> nextTurnPiece;
        if (previewingNextTurn)
        {
            nextTurnPiece = *actingPiece;
            game_data::beginPieceTurn(*nextTurnPiece);
        }
        const game_data::Piece* highlightedPiece = nextTurnPiece ? &*nextTurnPiece : actingPiece;
        const std::optional<std::pair<int, int>> draggedPieceSquare = [&]()
            -> std::optional<std::pair<int, int>> {
            if (!gameDragActive || !draggedPiece) return std::nullopt;
            const auto hovered = squareAtPixel(gameDragCurrentPos);
            if (!hovered) return std::nullopt;
            return std::pair<int, int>{
                hovered->first - gameDragPieceRowOffset,
                hovered->second - gameDragPieceColumnOffset};
        }();
        bool draggedPieceDropValid = false;
        if (draggedPiece && draggedPieceSquare)
        {
            const game_data::PieceActionOutcome outcome = game_data::resolvePieceActionThroughHidden(
                gameSnapshot.pieces,
                gameSnapshot.holes,
                *draggedPiece,
                draggedPieceSquare->first,
                draggedPieceSquare->second);
            draggedPieceDropValid = phase == game_data::Phase::Playing &&
                (sandboxMode || gameSnapshot.activePlayer == me) &&
                pieceCanTakeGameAction(*draggedPiece) &&
                outcome.action.legal;
        }
        const std::optional<std::size_t> actingHandIndex =
            gameDragKind == GameDragKind::HandCard && draggingHandIndex ? draggingHandIndex : selectedHandIndex;
        const game_data::GameCard* draggedHandCard =
            gameDragActive && gameDragKind == GameDragKind::HandCard && draggingHandIndex &&
                *draggingHandIndex < gameSnapshot.hand.size()
            ? &gameSnapshot.hand[*draggingHandIndex]
            : nullptr;
        const bool draggingBoardCard = draggedHandCard &&
            ((draggedHandCard->type == "Unit" || draggedHandCard->type == "Hero") ||
             (draggedHandCard->type == "Enchantment" && draggedHandCard->target != "player"));
        const std::optional<std::pair<int, int>> draggedHandSquare =
            draggingBoardCard ? squareAtPixel(gameDragCurrentPos) : std::nullopt;
        bool draggedHandDropValid = false;
        auto cardFootprintCanDeploy = [&](const game_data::GameCard& card, int row, int column, bool starting) {
            if (row < 0 || column < 0 || row + card.height > game_data::BoardSize ||
                column + card.width > game_data::BoardSize)
            {
                return false;
            }
            for (int r = row; r < row + card.height; ++r)
            {
                for (int c = column; c < column + card.width; ++c)
                {
                    if (gamePieceAt(r, c))
                    {
                        return false;
                    }
                    if (starting)
                    {
                        const auto home = game_data::homeSquares(me);
                        if (std::find(home.begin(), home.end(), std::pair<int, int>{r, c}) == home.end())
                        {
                            return false;
                        }
                    }
                    else if (gameSnapshot.control[static_cast<std::size_t>(game_data::squareIndex(r, c))] != sandboxPlayer)
                    {
                        return false;
                    }
                }
            }
            return true;
        };
        if (draggedHandCard && draggedHandSquare)
        {
            const auto [row, column] = *draggedHandSquare;
            if (phase == game_data::Phase::HeroPlacement && draggedHandCard->type == "Hero")
            {
                draggedHandDropValid = cardFootprintCanDeploy(*draggedHandCard, row, column, true);
            }
            else if (phase == game_data::Phase::Playing &&
                     (draggedHandCard->type == "Unit" || (sandboxMode && draggedHandCard->type == "Hero")))
            {
                draggedHandDropValid = gameSnapshot.relentlessPieceId == 0 &&
                    (sandboxMode || gameSnapshot.activePlayer == me) &&
                    (sandboxMode || draggedHandCard->cost <= gameSnapshot.players[static_cast<std::size_t>(me - 1)].resources) &&
                    (sandboxMode || game_data::heroTraitsAllowCard(gameSnapshot.pieces, me, *draggedHandCard)) &&
                    cardFootprintCanDeploy(*draggedHandCard, row, column, false);
            }
            else if (phase == game_data::Phase::Playing &&
                     draggedHandCard->type == "Enchantment")
            {
                const game_data::Piece* occupant = gamePieceAt(row, column);
                const bool validTarget =
                    (draggedHandCard->target == "square" &&
                     gameSnapshot.holes[static_cast<std::size_t>(game_data::squareIndex(row, column))] == 0) ||
                    (draggedHandCard->target == "piece" && occupant != nullptr);
                draggedHandDropValid = validTarget && gameSnapshot.relentlessPieceId == 0 &&
                    (sandboxMode || gameSnapshot.activePlayer == me) &&
                    (sandboxMode || draggedHandCard->cost <=
                        gameSnapshot.players[static_cast<std::size_t>(me - 1)].resources);
            }
        }

        // Precompute highlight masks for the current selection.
        std::array<int, game_data::BoardSquares> highlight{};  // 0 none,1 move,2 attack,3 place,4 spell
        auto highlightFootprint = [&](int row, int column, int width, int height, int value) {
            for (int r = row; r < row + height && r < game_data::BoardSize; ++r)
                for (int c = column; c < column + width && c < game_data::BoardSize; ++c)
                    if (r >= 0 && c >= 0)
                        highlight[static_cast<std::size_t>(game_data::squareIndex(r, c))] = value;
        };
        if (phase == game_data::Phase::HeroPlacement &&
            gameSnapshot.players[static_cast<std::size_t>(me - 1)].heroesToPlace > 0)
        {
            const game_data::GameCard* selectedHero = actingHandIndex && *actingHandIndex < gameSnapshot.hand.size()
                ? &gameSnapshot.hand[*actingHandIndex]
                : nullptr;
            if (selectedHero && selectedHero->type == "Hero")
            {
                for (int r = 0; r < game_data::BoardSize; ++r)
                    for (int c = 0; c < game_data::BoardSize; ++c)
                        if (cardFootprintCanDeploy(*selectedHero, r, c, true))
                            highlightFootprint(r, c, selectedHero->width, selectedHero->height, 3);
            }
            else
            {
                for (const auto& [r, c] : game_data::homeSquares(me))
                    if (!gamePieceAt(r, c))
                        highlight[static_cast<std::size_t>(game_data::squareIndex(r, c))] = 3;
            }
        }
        else if (phase == game_data::Phase::Playing)
        {
            const bool pieceCanHighlight = highlightedPiece &&
                ((previewingNextTurn && !highlightedPiece->hasActed) ||
                 (!previewingNextTurn &&
                  pieceCanTakeTurnAction(*highlightedPiece, gameSnapshot.activePlayer)));
            if (pieceCanHighlight)
            {
                // Highlight against the acting piece's view of the board:
                // dematerialized enemies read as open squares (never as
                // attack targets), so nothing betrays where they hide.
                const std::vector<game_data::Piece> visiblePieces =
                    game_data::piecesVisibleTo(gameSnapshot.pieces, highlightedPiece->owner);
                for (int r = 0; r < game_data::BoardSize; ++r)
                {
                    for (int c = 0; c < game_data::BoardSize; ++c)
                    {
                        const std::size_t idx = static_cast<std::size_t>(game_data::squareIndex(r, c));
                        const game_data::ActionResolution action = game_data::resolvePieceAction(
                            visiblePieces, gameSnapshot.holes, *highlightedPiece, r, c);
                        if (action.legal)
                        {
                            // Healing, control, and non-damaging status actions
                            // still use the authoritative attack-target path, but
                            // need their own visual language from damage attacks.
                            const int highlightValue = action.attacks
                                ? ((action.heal > 0 || action.control > 0 || action.damage == 0) ? 4 : 2)
                                : 1;
                            if (action.moves)
                            {
                                highlightFootprint(
                                    r, c, highlightedPiece->width, highlightedPiece->height,
                                    highlightValue);
                            }
                            else
                            {
                                highlight[idx] = highlightValue;
                            }
                        }
                    }
                }
            }
            else if (!previewingNextTurn && (sandboxMode || gameSnapshot.activePlayer == me) &&
                     gameSnapshot.relentlessPieceId == 0 &&
                     actingHandIndex && *actingHandIndex < gameSnapshot.hand.size())
            {
                const game_data::GameCard& card = gameSnapshot.hand[*actingHandIndex];
                if (sandboxMode || game_data::heroTraitsAllowCard(gameSnapshot.pieces, me, card))
                {
                    for (int r = 0; r < game_data::BoardSize; ++r)
                    {
                        for (int c = 0; c < game_data::BoardSize; ++c)
                        {
                            const std::size_t idx = static_cast<std::size_t>(game_data::squareIndex(r, c));
                            const game_data::Piece* occupant = gamePieceAt(r, c);
                            if (card.type == "Unit" || (sandboxMode && card.type == "Hero"))
                            {
                                if (cardFootprintCanDeploy(card, r, c, false))
                                {
                                    highlightFootprint(r, c, card.width, card.height, 3);
                                }
                            }
                            else if (card.type == "Spell" && card.effect == "damage" &&
                                     occupant && occupant->owner != sandboxPlayer)
                            {
                                highlight[idx] = 2;
                            }
                            else if (card.type == "Spell" && card.effect == "heal" &&
                                     occupant && occupant->owner == sandboxPlayer)
                            {
                                highlight[idx] = 4;
                            }
                            else if (card.type == "Enchantment" && card.target == "square" &&
                                     gameSnapshot.holes[idx] == 0)
                            {
                                highlight[idx] = 4;
                            }
                            else if (card.type == "Enchantment" && card.target == "piece" && occupant)
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

        // Shrinks a quad toward its centroid, for the inset markers that keep the
        // stone visible underneath a range highlight.
        const auto insetQuad = [](std::array<sf::Vector2f, 4> corners, float factor) {
            sf::Vector2f centroid{0.0f, 0.0f};
            for (const sf::Vector2f& corner : corners)
            {
                centroid += corner;
            }
            centroid /= 4.0f;
            for (sf::Vector2f& corner : corners)
            {
                corner = centroid + (corner - centroid) * factor;
            }
            return corners;
        };

        // A band running along one edge of the board, offset outward along that
        // edge's normal. The ends overshoot by the band width so neighbouring
        // bands overlap into a mitred corner.
        const auto edgeBand = [](sf::Vector2f from, sf::Vector2f to, float thickness) {
            const sf::Vector2f delta = to - from;
            const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (length < 0.001f)
            {
                return std::array<sf::Vector2f, 4>{from, to, to, from};
            }
            const sf::Vector2f along = delta / length;
            const sf::Vector2f outward{along.y, -along.x};
            const sf::Vector2f start = from - along * thickness;
            const sf::Vector2f end = to + along * thickness;
            return std::array<sf::Vector2f, 4>{
                start, end, end + outward * thickness, start + outward * thickness};
        };

        const auto forEachBoardEdge = [&](float thickness, const auto& body) {
            for (std::size_t i = 0; i < boardTop.size(); ++i)
            {
                body(edgeBand(boardTop[i], boardTop[(i + 1) % boardTop.size()], thickness));
            }
        };

        // The board's own shadow on the swamp floor, so it reads as an object
        // resting in the scene rather than a rectangle pasted over the backdrop.
        for (int i = 0; i < 4; ++i)
        {
            const float spread = 6.0f + static_cast<float>(i) * 7.0f;
            drawQuad(
                offsetQuad(boardTop, {spread * 0.45f, spread}),
                sf::Color(0, 0, 0, static_cast<std::uint8_t>(64 - i * 13)));
        }

        const sf::Vector2f topLeft = boardTop[0];
        const sf::Vector2f topRight = boardTop[1];
        const sf::Vector2f bottomRight = boardTop[2];
        const sf::Vector2f bottomLeft = boardTop[3];

        // Slab sides. Gradients read as a thick stone edge; flat fills read as
        // three coloured strips.
        drawGradientQuad(
            window,
            {topRight, {topRight.x + 5.0f, topRight.y + BoardThickness * 0.42f},
             {bottomRight.x + 11.0f, bottomRight.y + BoardThickness}, bottomRight},
            sf::Color(16, 26, 27, 246),
            sf::Color(6, 11, 12, 246));
        drawGradientQuad(
            window,
            {topLeft, {topLeft.x - 5.0f, topLeft.y + BoardThickness * 0.42f},
             {bottomLeft.x - 11.0f, bottomLeft.y + BoardThickness}, bottomLeft},
            sf::Color(14, 23, 24, 246),
            sf::Color(5, 10, 11, 246));
        drawGradientQuad(
            window,
            {bottomLeft, bottomRight, {bottomRight.x + 11.0f, bottomRight.y + BoardThickness},
             {bottomLeft.x - 11.0f, bottomLeft.y + BoardThickness}},
            shadeColor(BoardBrass, 0.62f),
            sf::Color(24, 15, 9, 250));

        // Dark ground under the tiles, so grout gaps read as depth.
        drawQuad(boardTop, sf::Color(7, 11, 12, 250));

        // ---- Playing surface ------------------------------------------------
        for (int screenRow = 0; screenRow < game_data::BoardSize; ++screenRow)
        {
            const int row = rowForScreenRow(screenRow, me);
            for (int column = 0; column < game_data::BoardSize; ++column)
            {
                const std::size_t idx = static_cast<std::size_t>(game_data::squareIndex(row, column));
                const BoardCellMetrics metrics = boardCellMetrics(row, column);

                // Two stone tones, then a vignette that keeps the middle of the
                // board the brightest thing on screen, then a slight lift toward
                // the near edge for the viewer-side light.
                const bool paleStone = (row + column) % 2 == 0;
                const sf::Color stone = paleStone ? BoardStoneLight : BoardStoneDark;
                const float acrossBoard =
                    (static_cast<float>(column) + 0.5f) / static_cast<float>(game_data::BoardSize)
                        * 2.0f - 1.0f;
                const float intoBoard =
                    (static_cast<float>(screenRow) + 0.5f) / static_cast<float>(game_data::BoardSize)
                        * 2.0f - 1.0f;
                const float vignette = 1.0f - 0.22f * std::min(
                    1.0f, acrossBoard * acrossBoard * 0.9f + intoBoard * intoBoard * 0.5f);
                const float depth =
                    static_cast<float>(screenRow) / static_cast<float>(game_data::BoardSize - 1);
                const float lift = 0.88f + 0.22f * depth;
                drawGradientQuad(
                    window,
                    metrics.corners,
                    shadeColor(stone, vignette * lift * 0.88f),
                    shadeColor(stone, vignette * lift * 1.10f));

                // Territory is a wash plus an edge, not a paint-bucket fill: the
                // stone has to keep showing through or the board reads as two
                // blocks of colour.
                const int controller = gameSnapshot.control[idx];
                if (controller != 0)
                {
                    const sf::Color tint = ownerTint(controller);
                    drawGradientQuad(
                        window,
                        metrics.corners,
                        withAlpha(tint, static_cast<int>(tint.a * 0.72f)),
                        tint);
                    const std::array<sf::Vector2f, 4> rim = insetQuad(metrics.corners, 0.9f);
                    const bool playerTerritory = controller == 1 || controller == 2;
                    const float rimThickness = playerTerritory ? 1.25f : 1.0f;
                    const int rimAlpha = playerTerritory ? 156 : 74;
                    for (std::size_t i = 0; i < rim.size(); ++i)
                    {
                        drawEdgeLine(
                            window,
                            rim[i],
                            rim[(i + 1) % rim.size()],
                            rimThickness,
                            withAlpha(ownerColor(controller), rimAlpha));
                    }
                }

                if (gameSnapshot.holes[idx] != 0)
                {
                    // A collapsed square: a dark shaft with a lit near lip.
                    const sf::Vector2f anchor = boardCellAnchor(metrics);
                    const sf::Vector2f mouth{anchor.x, anchor.y - 6.0f * metrics.depthScale};
                    drawSoftEllipse(
                        window,
                        mouth,
                        18.0f * metrics.depthScale,
                        8.0f * metrics.depthScale,
                        sf::Color(0, 0, 0, 235),
                        5);
                    drawEllipseOutline(
                        window,
                        {mouth.x, mouth.y + 1.6f * metrics.depthScale},
                        13.0f * metrics.depthScale,
                        5.4f * metrics.depthScale,
                        1.2f,
                        sf::Color(96, 74, 46, 170));
                }
            }
        }

        // A restrained authored material pass keeps the board from reading as
        // a flat shader grid. It is intentionally low-opacity: the tactical
        // cell tones, territory washes, and state markers remain the source of
        // truth for gameplay readability.
        if (boardSurfaceTexture)
        {
            const sf::Vector2u textureSize = boardSurfaceTexture->getSize();
            if (textureSize.x > 0 && textureSize.y > 0)
            {
                sf::VertexArray surface(sf::PrimitiveType::TriangleFan, 4);
                const sf::Vector2f textureExtent{
                    static_cast<float>(textureSize.x), static_cast<float>(textureSize.y)};
                for (std::size_t i = 0; i < boardTop.size(); ++i)
                {
                    surface[i].position = boardTop[i];
                    surface[i].color = sf::Color(255, 255, 255, 66);
                }
                surface[0].texCoords = {0.0f, 0.0f};
                surface[1].texCoords = {textureExtent.x, 0.0f};
                surface[2].texCoords = textureExtent;
                surface[3].texCoords = {0.0f, textureExtent.y};

                sf::RenderStates states;
                states.texture = boardSurfaceTexture;
                window.draw(surface, states);
            }
        }

        // ---- Grout ----------------------------------------------------------
        // Drawn in one pass over the whole grid rather than as per-cell outlines,
        // so every joint is one consistent dark seam with a lit upper lip instead
        // of the doubled bright hairline an outlined quad leaves behind.
        for (int screenEdge = 0; screenEdge <= game_data::BoardSize; ++screenEdge)
        {
            const float depth =
                static_cast<float>(screenEdge) / static_cast<float>(game_data::BoardSize);
            const float weight = 0.9f + 1.5f * depth;
            const sf::Vector2f left = boardEdgePoint(screenEdge, 0);
            const sf::Vector2f right = boardEdgePoint(screenEdge, game_data::BoardSize);
            drawEdgeLine(window, left, right, weight, withAlpha(BoardGrout, 225));
            drawEdgeLine(
                window,
                {left.x, left.y - weight * 0.7f},
                {right.x, right.y - weight * 0.7f},
                std::max(0.6f, weight * 0.45f),
                sf::Color(126, 138, 128, 40));
        }
        for (int columnEdge = 0; columnEdge <= game_data::BoardSize; ++columnEdge)
        {
            for (int screenEdge = 0; screenEdge < game_data::BoardSize; ++screenEdge)
            {
                // Row spacing is non-linear, so a column seam has to be walked
                // segment by segment rather than drawn as one straight line.
                const float depth =
                    static_cast<float>(screenEdge) / static_cast<float>(game_data::BoardSize);
                drawEdgeLine(
                    window,
                    boardEdgePoint(screenEdge, columnEdge),
                    boardEdgePoint(screenEdge + 1, columnEdge),
                    0.9f + 1.3f * depth,
                    withAlpha(BoardGrout, 215));
            }
        }

        // ---- Range indicators and drop previews ------------------------------
        for (int screenRow = 0; screenRow < game_data::BoardSize; ++screenRow)
        {
            const int row = rowForScreenRow(screenRow, me);
            for (int column = 0; column < game_data::BoardSize; ++column)
            {
                const std::size_t idx = static_cast<std::size_t>(game_data::squareIndex(row, column));
                const BoardCellMetrics metrics = boardCellMetrics(row, column);

                if (highlight[idx] != 0 && highlight[idx] != 5)
                {
                    // Markers sit inside the tile so the surface stays readable.
                    // A translucent sheet over half the board is what made the old
                    // move range look like a paint-bucket accident.
                    struct RangeStyle
                    {
                        sf::Color accent;
                        enum class Marker { Move, Attack, Deploy, Effect } marker;
                    };
                    const RangeStyle styles[5] = {
                        {sf::Color::Transparent, RangeStyle::Marker::Move},
                        {sf::Color(126, 214, 178), RangeStyle::Marker::Move},
                        {sf::Color(226, 108, 88), RangeStyle::Marker::Attack},
                        {BoardBrassBright, RangeStyle::Marker::Deploy},
                        {sf::Color(186, 138, 234), RangeStyle::Marker::Effect},
                    };
                    const RangeStyle& style = styles[highlight[idx]];
                    const sf::Vector2f anchor = boardCellAnchor(metrics);
                    const std::array<sf::Vector2f, 4> face = insetQuad(metrics.corners, 0.82f);

                    drawGradientQuad(
                        window, face, withAlpha(style.accent, 26), withAlpha(style.accent, 46));
                    for (std::size_t i = 0; i < face.size(); ++i)
                    {
                        drawEdgeLine(
                            window,
                            face[i],
                            face[(i + 1) % face.size()],
                            1.3f,
                            withAlpha(style.accent, 168));
                    }
                    if (style.marker != RangeStyle::Marker::Deploy)
                    {
                        drawSoftEllipse(
                            window,
                            {anchor.x, anchor.y - 4.0f * metrics.depthScale},
                            13.0f * metrics.depthScale,
                            5.5f * metrics.depthScale,
                            withAlpha(style.accent, 132),
                            4);
                    }

                    if (style.marker == RangeStyle::Marker::Move)
                    {
                        // A chevron reads as a destination even for players who
                        // cannot distinguish the teal from the other target hues.
                        const float reach = 6.5f * metrics.depthScale;
                        drawEdgeLine(
                            window, {anchor.x - reach, anchor.y - 4.0f * metrics.depthScale},
                            {anchor.x + 1.5f * metrics.depthScale, anchor.y - 4.0f * metrics.depthScale},
                            2.0f, withAlpha(style.accent, 242));
                        drawEdgeLine(
                            window, {anchor.x + 1.5f * metrics.depthScale, anchor.y - 4.0f * metrics.depthScale},
                            {anchor.x - 1.8f * metrics.depthScale, anchor.y - 7.4f * metrics.depthScale},
                            2.0f, withAlpha(style.accent, 242));
                        drawEdgeLine(
                            window, {anchor.x + 1.5f * metrics.depthScale, anchor.y - 4.0f * metrics.depthScale},
                            {anchor.x - 1.8f * metrics.depthScale, anchor.y - 0.6f * metrics.depthScale},
                            2.0f, withAlpha(style.accent, 242));
                    }
                    else if (style.marker == RangeStyle::Marker::Attack)
                    {
                        // Attack squares get a crosshair and corner brackets, so
                        // an attack is distinguishable from movement without
                        // relying on colour alone.
                        const sf::Vector2f center{anchor.x, anchor.y - 4.0f * metrics.depthScale};
                        const float cross = 6.2f * metrics.depthScale;
                        drawEllipseOutline(window, center, cross, cross * 0.48f, 1.4f,
                                           withAlpha(style.accent, 238));
                        drawEdgeLine(window, {center.x - cross, center.y}, {center.x + cross, center.y},
                                     1.8f, withAlpha(style.accent, 242));
                        drawEdgeLine(window, {center.x, center.y - cross * 0.7f},
                                     {center.x, center.y + cross * 0.7f}, 1.8f,
                                     withAlpha(style.accent, 242));
                        const std::array<sf::Vector2f, 4> outer =
                            insetQuad(metrics.corners, 0.94f);
                        for (std::size_t i = 0; i < outer.size(); ++i)
                        {
                            const sf::Vector2f corner = outer[i];
                            const sf::Vector2f next = outer[(i + 1) % outer.size()];
                            const sf::Vector2f previous = outer[(i + 3) % outer.size()];
                            drawEdgeLine(
                                window, corner, corner + (next - corner) * 0.28f, 2.0f,
                                withAlpha(style.accent, 236));
                            drawEdgeLine(
                                window, corner, corner + (previous - corner) * 0.28f, 2.0f,
                                withAlpha(style.accent, 236));
                        }
                    }
                    else if (style.marker == RangeStyle::Marker::Effect)
                    {
                        // Effects use a four-point arcane diamond, separate from
                        // both the directional move chevron and attack crosshair.
                        const sf::Vector2f center{anchor.x, anchor.y - 4.0f * metrics.depthScale};
                        const float radius = 7.0f * metrics.depthScale;
                        sf::ConvexShape diamond(4);
                        diamond.setPoint(0, {center.x, center.y - radius});
                        diamond.setPoint(1, {center.x + radius, center.y});
                        diamond.setPoint(2, {center.x, center.y + radius});
                        diamond.setPoint(3, {center.x - radius, center.y});
                        diamond.setFillColor(withAlpha(style.accent, 54));
                        diamond.setOutlineThickness(1.5f);
                        diamond.setOutlineColor(withAlpha(style.accent, 242));
                        window.draw(diamond);
                        drawEllipseOutline(window, center, radius * 0.28f, radius * 0.28f, 1.1f,
                                           withAlpha(BoardParchment, 210));
                    }
                }

                if (storyMode && row == storyTargetRow && column == storyTargetColumn)
                {
                    // Objective marker: a pulsing brass target ring set into the
                    // square, rather than the flat rectangle it replaces.
                    const sf::Vector2f anchor = boardCellAnchor(metrics);
                    const float pulse = 0.5f + 0.5f * std::sin(animationTime * 3.2f);
                    const sf::Vector2f center{anchor.x, anchor.y - 6.0f * metrics.depthScale};
                    const std::array<sf::Vector2f, 4> face = insetQuad(metrics.corners, 0.8f);
                    drawGradientQuad(
                        window, face, withAlpha(BoardBrassBright, 30),
                        withAlpha(BoardBrassBright, 58));
                    for (std::size_t i = 0; i < face.size(); ++i)
                    {
                        drawEdgeLine(
                            window, face[i], face[(i + 1) % face.size()], 1.4f,
                            withAlpha(BoardBrassBright, 190));
                    }
                    drawSoftEllipse(
                        window,
                        center,
                        (15.0f + 3.0f * pulse) * metrics.depthScale,
                        (6.5f + 1.4f * pulse) * metrics.depthScale,
                        withAlpha(BoardBrassBright, static_cast<int>(96.0f + 54.0f * pulse)),
                        5);
                    drawEllipseOutline(
                        window,
                        center,
                        (13.0f + 2.4f * pulse) * metrics.depthScale,
                        (5.6f + 1.1f * pulse) * metrics.depthScale,
                        1.6f,
                        withAlpha(BoardBrassBright, 240));
                    drawEllipseOutline(
                        window,
                        center,
                        6.0f * metrics.depthScale,
                        2.6f * metrics.depthScale,
                        1.3f,
                        withAlpha(BoardBrassBright, 210));
                }

                const bool hoveringHandDrop = draggedHandSquare && draggedHandCard &&
                    row >= draggedHandSquare->first &&
                    row < draggedHandSquare->first + draggedHandCard->height &&
                    column >= draggedHandSquare->second &&
                    column < draggedHandSquare->second + draggedHandCard->width;
                const bool hoveringPieceDrop = draggedPieceSquare && draggedPiece &&
                    row >= draggedPieceSquare->first &&
                    row < draggedPieceSquare->first + draggedPiece->height &&
                    column >= draggedPieceSquare->second &&
                    column < draggedPieceSquare->second + draggedPiece->width;
                if (hoveringHandDrop || hoveringPieceDrop)
                {
                    const bool valid = hoveringHandDrop ? draggedHandDropValid : draggedPieceDropValid;
                    const sf::Color accent = valid
                        ? sf::Color(132, 232, 186)
                        : sf::Color(232, 104, 92);
                    drawGradientQuad(
                        window,
                        metrics.corners,
                        withAlpha(accent, 52),
                        withAlpha(accent, 88));
                    for (std::size_t i = 0; i < metrics.corners.size(); ++i)
                    {
                        drawEdgeLine(
                            window,
                            metrics.corners[i],
                            metrics.corners[(i + 1) % metrics.corners.size()],
                            2.4f,
                            withAlpha(accent, 246));
                    }
                }
            }
        }

        // ---- Board frame -----------------------------------------------------
        // Drawn over the outermost tiles so the brass reads as a rim capping the
        // slab. Bands overlap at the corners, which mitres them.
        forEachBoardEdge(3.0f, [&](const std::array<sf::Vector2f, 4>& band) {
            drawQuad(offsetQuad(band, {2.0f, 3.0f}), sf::Color(0, 0, 0, 120));
        });
        forEachBoardEdge(9.0f, [&](const std::array<sf::Vector2f, 4>& band) {
            drawGradientQuad(window, band, shadeColor(BoardBrass, 1.02f), BoardBrassDim);
        });
        forEachBoardEdge(2.0f, [&](const std::array<sf::Vector2f, 4>& band) {
            drawGradientQuad(
                window, band, withAlpha(BoardBrassBright, 168), withAlpha(BoardBrassBright, 44));
        });
        for (std::size_t i = 0; i < boardTop.size(); ++i)
        {
            // Outer keeper line, and a rivet at each mitre.
            const sf::Vector2f from = boardTop[i];
            const sf::Vector2f to = boardTop[(i + 1) % boardTop.size()];
            const sf::Vector2f delta = to - from;
            const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (length > 0.001f)
            {
                const sf::Vector2f outward{delta.y / length, -delta.x / length};
                drawEdgeLine(
                    window,
                    from + outward * 9.0f,
                    to + outward * 9.0f,
                    1.2f,
                    sf::Color(28, 18, 10, 220));
            }
            sf::CircleShape rivet(2.6f, 14);
            rivet.setOrigin({2.6f, 2.6f});
            rivet.setPosition(from);
            rivet.setFillColor(sf::Color(206, 152, 74));
            rivet.setOutlineThickness(1.0f);
            rivet.setOutlineColor(sf::Color(46, 28, 14, 210));
            window.draw(rivet);
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

        const auto drawPieceLayer = [&](bool foregroundOnly) {
        for (const game_data::Piece* piecePtr : pieceDrawOrder)
        {
            const game_data::Piece& piece = *piecePtr;
            BoardCellMetrics cell = boardCellMetrics(piece.row, piece.column);
            sf::Vector2f anchor = boardFootprintAnchor(
                piece.row, piece.column, piece.width, gameSnapshot.yourPlayer);
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
                    const sf::Vector2f start = boardFootprintAnchor(
                        animation->second.fromRow, animation->second.fromColumn,
                        piece.width, gameSnapshot.yourPlayer);
                    const sf::Vector2f end = boardFootprintAnchor(
                        animation->second.toRow, animation->second.toColumn,
                        piece.width, gameSnapshot.yourPlayer);
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
                ((piece.hasActed ||
                  (gameSnapshot.relentlessPieceId != 0 && piece.id != gameSnapshot.relentlessPieceId)) &&
                 piece.owner == gameSnapshot.activePlayer) || piece.disabledTurns > 0;

            const std::string& walkPath = pieceWalkAnimPath(piece);
            const std::string& tokenPath = pieceTokenPath(piece);
            const bool usesState1Token = game_data::pieceUsesState1Token(piece);
            sf::Color pieceTint = pieceUnavailable
                ? sf::Color(150, 150, 150, 215)
                : sf::Color::White;
            if (piece.hidden)
            {
                // Dematerialized: only its owner is sent this piece, so make
                // the hidden state obvious — a pulsing glow and a ghostly body.
                const float pulse = 0.5f + 0.5f * std::sin(animationTime * 3.2f);
                const float glowRadius = (17.0f + 4.0f * pulse) * pieceScale;
                sf::CircleShape glow(glowRadius);
                glow.setOrigin({glowRadius, glowRadius});
                glow.setScale({1.35f, 0.62f});
                glow.setPosition({anchor.x, anchor.y - 4.0f * pieceScale});
                glow.setFillColor(sf::Color(
                    140, 222, 255,
                    static_cast<std::uint8_t>(58.0f + 52.0f * pulse)));
                window.draw(glow);
                pieceTint.a = 132;
            }
            int walkFrame = -1;
            int idleFrame = -1;
            const std::string* reactionPath = nullptr;
            int reactionFrames = 1;
            int reactionFrame = -1;
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
            else if (!usesState1Token && !piece.idleAnimPath.empty())
            {
                const int idleFrameCount = std::max(1, piece.idleAnimFrames);
                const float loopProgress =
                    std::fmod(animationTime, WalkAnimationLoopSeconds) /
                    WalkAnimationLoopSeconds;
                idleFrame = std::min(
                    static_cast<int>(loopProgress * static_cast<float>(idleFrameCount)),
                    idleFrameCount - 1);
            }
            if (attackAnimationProgress >= 0.0f && !piece.attackAnimPath.empty())
            {
                reactionPath = &piece.attackAnimPath;
                reactionFrames = std::max(1, piece.attackAnimFrames);
                reactionFrame = std::min(
                    static_cast<int>(attackAnimationProgress * static_cast<float>(reactionFrames)),
                    reactionFrames - 1);
            }
            else if (const auto animation = pieceDamagedAnimations.find(piece.id);
                     animation != pieceDamagedAnimations.end())
            {
                const float elapsed = std::max(0.0f, animationTime - animation->second.startTime);
                const float progress = std::min(elapsed / animation->second.duration, 1.0f);
                if (progress < 1.0f && !piece.damagedAnimPath.empty())
                {
                    reactionPath = &piece.damagedAnimPath;
                    reactionFrames = std::max(1, piece.damagedAnimFrames);
                    reactionFrame = std::min(
                        static_cast<int>(progress * static_cast<float>(reactionFrames)),
                        reactionFrames - 1);
                }
                else
                {
                    pieceDamagedAnimations.erase(piece.id);
                }
            }
            else if (!isMoving && !usesState1Token)
            {
                const auto animation = pieceFidgetAnimations.find(piece.id);
                if (EnableFidgetAnimations &&
                    animation != pieceFidgetAnimations.end() &&
                    animation->second.playing && !piece.fidgetAnimPath.empty())
                {
                    const float elapsed = std::max(0.0f, animationTime - animation->second.startTime);
                    const float progress = std::min(elapsed / FidgetAnimationDurationSeconds, 1.0f);
                    if (progress < 1.0f)
                    {
                        reactionPath = &piece.fidgetAnimPath;
                        reactionFrames = std::max(1, piece.fidgetAnimFrames);
                        reactionFrame = std::min(
                            static_cast<int>(progress * static_cast<float>(reactionFrames)),
                            reactionFrames - 1);
                    }
                }
            }
            if (!foregroundOnly)
            {
                // Plinth and contact shadow first, so the piece stands on the board
                // instead of reading as a cut-out laid over it.
                drawPieceBase(
                    window,
                    anchor,
                    pieceScale,
                    piece.owner,
                    pieceUnavailable,
                    static_cast<float>(piece.width));

                // A clean owner-coloured outer rim survives the busy token art
                // and the perspective-shrunk far rank. It is deliberately kept
                // to the plinth, so allegiance is clearer without tinting art.
                const float footprint = std::max(1.0f, static_cast<float>(piece.width));
                const sf::Vector2f baseCenter{
                    anchor.x, anchor.y - PieceBaseLift * pieceScale};
                const float rimDim = pieceUnavailable ? 0.56f : 1.0f;
                drawEllipseOutline(
                    window,
                    baseCenter,
                    26.8f * pieceScale * footprint,
                    9.8f * pieceScale,
                    std::max(1.0f, 1.45f * pieceScale),
                    withAlpha(shadeColor(ownerColorBright(piece.owner), rimDim),
                              pieceUnavailable ? 138 : 226));

                const bool pieceIsSelected = selectedPieceId && *selectedPieceId == piece.id;
                if (pieceIsSelected)
                {
                    const float pulse = 0.5f + 0.5f * std::sin(animationTime * 3.4f);
                    drawPieceSelectionRing(window, anchor, pieceScale, pulse, BoardBrassBright);
                }
                else if (highlightedPiece && highlightedPiece->id == piece.id)
                {
                    drawPieceSelectionRing(
                        window, anchor, pieceScale, 0.35f, withAlpha(BoardBrassBright, 170));
                }
            }

            bool drewPiece = drawPieceVisual(
                tokenPath,
                reactionPath ? *reactionPath : walkPath,
                usesState1Token ? "" : piece.idleAnimPath,
                pieceBasePath(piece),
                piece.owner == 2,
                reactionPath ? reactionFrames : piece.walkAnimFrames,
                piece.idleAnimFrames,
                anchor,
                pieceScale,
                pieceTint,
                reactionPath ? reactionFrame : walkFrame,
                reactionPath ? -1 : idleFrame,
                piece.width,
                piece.height);
            if (!drewPiece)
            {
                sf::Color cameoTint = pieceUnavailable
                    ? sf::Color(150, 146, 140)
                    : sf::Color::White;
                cameoTint.a = pieceTint.a;
                drawPieceCameo(anchor, pieceScale, piece.owner, piece.imagePath, cameoTint);
                drewPiece = true;
            }
            if (!foregroundOnly && attackImpactAnchor &&
                attackAnimationProgress >= 0.22f && attackAnimationProgress <= 0.78f)
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
            if (!foregroundOnly)
            {
                drawPieceHealthPips(
                    anchor, pieceScale, piece.health, piece.maxHealth, piece.owner, pieceUnavailable);
            }

            if (!foregroundOnly && piece.isHero)
            {
                // A hero's loss ends the match, so mark it with a small crown.
                const float chipHeight = std::clamp(16.0f * pieceScale, 13.0f, 22.0f);
                const float chipWidth = std::clamp(31.0f * pieceScale, 25.0f, 44.0f);
                const float span = std::clamp(13.0f * pieceScale, 11.0f, 17.0f);
                const float crownX = anchor.x + chipWidth * 0.5f - span * 0.14f;
                const float crownY =
                    anchor.y - chipHeight * 0.34f - span * 0.34f - 5.0f * pieceScale;
                sf::ConvexShape crown(7);
                crown.setPoint(0, {crownX - span * 0.5f, crownY + span * 0.32f});
                crown.setPoint(1, {crownX - span * 0.5f, crownY - span * 0.18f});
                crown.setPoint(2, {crownX - span * 0.25f, crownY + span * 0.04f});
                crown.setPoint(3, {crownX, crownY - span * 0.32f});
                crown.setPoint(4, {crownX + span * 0.25f, crownY + span * 0.04f});
                crown.setPoint(5, {crownX + span * 0.5f, crownY - span * 0.18f});
                crown.setPoint(6, {crownX + span * 0.5f, crownY + span * 0.32f});
                crown.setFillColor(withAlpha(BoardBrassBright, pieceUnavailable ? 155 : 245));
                crown.setOutlineThickness(1.0f);
                crown.setOutlineColor(sf::Color(32, 20, 10, 230));
                window.draw(crown);
            }

            if (!foregroundOnly && piece.controlTurnsRemaining > 0)
            {
                // Held piece: a violet badge on the base's other shoulder.
                const float chipHeight = std::clamp(16.0f * pieceScale, 13.0f, 22.0f);
                const float chipWidth = std::clamp(31.0f * pieceScale, 25.0f, 44.0f);
                const float radius = std::clamp(8.0f * pieceScale, 7.0f, 11.0f);
                const sf::Vector2f center{
                    anchor.x - chipWidth * 0.5f + radius * 0.2f,
                    anchor.y - chipHeight * 0.34f - radius * 0.32f};
                sf::CircleShape badge(radius, 18);
                badge.setOrigin({radius, radius});
                badge.setPosition(center);
                badge.setFillColor(sf::Color(58, 30, 78, 244));
                badge.setOutlineThickness(1.3f);
                badge.setOutlineColor(withAlpha(sf::Color(206, 162, 240), 240));
                window.draw(badge);
                sf::Text turns(
                    font,
                    std::to_string(piece.controlTurnsRemaining),
                    static_cast<unsigned int>(std::clamp(11.0f * pieceScale, 9.0f, 14.0f)));
                turns.setFillColor(BoardParchment);
                centerText(turns, center);
                drawCrispText(window, turns);
            }

            if (!foregroundOnly)
            {
                bool highlightedAttackTarget = false;
                for (int row = piece.row;
                     row < piece.row + piece.height && !highlightedAttackTarget;
                     ++row)
                {
                    for (int column = piece.column;
                         column < piece.column + piece.width;
                         ++column)
                    {
                        if (row < 0 || column < 0 ||
                            row >= game_data::BoardSize ||
                            column >= game_data::BoardSize)
                        {
                            continue;
                        }
                        const std::size_t targetIndex =
                            static_cast<std::size_t>(game_data::squareIndex(row, column));
                        highlightedAttackTarget =
                            targetIndex < highlight.size() && highlight[targetIndex] == 2;
                        if (highlightedAttackTarget)
                        {
                            break;
                        }
                    }
                }

                if (highlightedAttackTarget)
                {
                    // Occupied targets cover the tile-level reticle, so repeat a
                    // larger crosshair over the plinth after the token is drawn.
                    // This keeps the board faithful to the command ribbon.
                    const float footprint = std::max(1.0f, static_cast<float>(piece.width));
                    const sf::Vector2f center{
                        anchor.x, anchor.y - PieceBaseLift * pieceScale};
                    const float radiusX = 31.0f * pieceScale * footprint;
                    const float radiusY = 12.5f * pieceScale;
                    const sf::Color targetRed(238, 116, 94, 248);
                    drawRadialGlow(
                        window,
                        center,
                        radiusX * 1.35f,
                        sf::Color(targetRed.r, targetRed.g, targetRed.b, 38));
                    drawEllipseOutline(
                        window,
                        center,
                        radiusX,
                        radiusY,
                        std::max(1.8f, 2.4f * pieceScale),
                        targetRed);

                    const float tick = std::max(5.0f, 7.0f * pieceScale);
                    const float horizontalGap = radiusX * 0.56f;
                    drawEdgeLine(
                        window,
                        {center.x - radiusX - tick, center.y},
                        {center.x - horizontalGap, center.y},
                        2.2f,
                        targetRed);
                    drawEdgeLine(
                        window,
                        {center.x + horizontalGap, center.y},
                        {center.x + radiusX + tick, center.y},
                        2.2f,
                        targetRed);
                    drawEdgeLine(
                        window,
                        {center.x, center.y - radiusY - tick},
                        {center.x, center.y - radiusY * 0.35f},
                        2.2f,
                        targetRed);
                    drawEdgeLine(
                        window,
                        {center.x, center.y + radiusY * 0.35f},
                        {center.x, center.y + radiusY + tick},
                        2.2f,
                        targetRed);
                }
            }
        }
        };
        drawPieceLayer(false);

        for (auto animation = pieceKilledAnimations.begin(); animation != pieceKilledAnimations.end();)
        {
            const float elapsed = std::max(0.0f, animationTime - animation->startTime);
            const float progress = std::min(elapsed / animation->duration, 1.0f);
            if (progress >= 1.0f)
            {
                animation = pieceKilledAnimations.erase(animation);
                continue;
            }

            const game_data::Piece& killedPiece = animation->piece;
            const BoardCellMetrics cell = boardCellMetrics(killedPiece.row, killedPiece.column);
            const sf::Vector2f anchor = boardFootprintAnchor(
                killedPiece.row, killedPiece.column, killedPiece.width, gameSnapshot.yourPlayer);
            const int killedFrameCount = std::max(1, killedPiece.killedAnimFrames);
            const int killedFrame = std::min(
                static_cast<int>(progress * static_cast<float>(killedFrameCount)),
                killedFrameCount - 1);
            sf::Color tint = sf::Color::White;
            tint.a = static_cast<std::uint8_t>(std::clamp(255.0f * (1.0f - progress * 0.35f), 0.0f, 255.0f));
            // The plinth fades out with the piece standing on it.
            drawPieceBase(
                window,
                anchor,
                cell.depthScale * (1.0f - progress * 0.2f),
                killedPiece.owner,
                true,
                static_cast<float>(killedPiece.width));
            const bool drewKilledPiece = drawPieceVisual(
                pieceTokenPath(killedPiece),
                killedPiece.killedAnimPath,
                "",
                pieceBasePath(killedPiece),
                killedPiece.owner == 2,
                killedPiece.killedAnimFrames,
                1,
                anchor,
                cell.depthScale,
                tint,
                killedFrame,
                -1,
                killedPiece.width,
                killedPiece.height);
            if (!drewKilledPiece)
            {
                if (sf::Texture* art = cardArtTexture(killedPiece.imagePath))
                {
                    drawContainSprite(window, *art, pieceTargetRect(
                        anchor, cell.depthScale, false, killedPiece.width, killedPiece.height), tint);
                }
            }
            ++animation;
        }

        // An enemy piece that just dematerialized blinks in place for a few
        // seconds, then is not drawn at all — wherever it moves stays secret.
        for (auto ghost = dematerializeGhosts.begin(); ghost != dematerializeGhosts.end();)
        {
            const float elapsed = animationTime - ghost->startTime;
            if (elapsed >= DematerializeBlinkSeconds)
            {
                ghost = dematerializeGhosts.erase(ghost);
                continue;
            }
            const bool blinkOn =
                std::fmod(elapsed, DematerializeBlinkPeriodSeconds) <
                DematerializeBlinkPeriodSeconds * 0.6f;
            if (blinkOn)
            {
                const game_data::Piece& ghostPiece = ghost->piece;
                const BoardCellMetrics cell = boardCellMetrics(ghostPiece.row, ghostPiece.column);
                const sf::Vector2f anchor = boardFootprintAnchor(
                    ghostPiece.row, ghostPiece.column, ghostPiece.width, gameSnapshot.yourPlayer);
                const float scale = cell.depthScale;
                const auto alpha = static_cast<std::uint8_t>(
                    std::clamp(220.0f * (1.0f - elapsed / DematerializeBlinkSeconds), 0.0f, 220.0f));
                const sf::Color tint(255, 255, 255, alpha);
                bool drewGhost = drawPieceVisual(
                    pieceTokenPath(ghostPiece),
                    pieceWalkAnimPath(ghostPiece),
                    "",
                    pieceBasePath(ghostPiece),
                    ghostPiece.owner == 2,
                    ghostPiece.walkAnimFrames,
                    1,
                    anchor,
                    scale,
                    tint,
                    -1,
                    -1,
                    ghostPiece.width,
                    ghostPiece.height);
                if (!drewGhost)
                {
                    drawPieceCameo(anchor, scale, ghostPiece.owner, ghostPiece.imagePath, tint);
                }
            }
            ++ghost;
        }

        for (auto effect = floatingNumberEffects.begin(); effect != floatingNumberEffects.end();)
        {
            const float elapsed = animationTime - effect->startTime;
            const float progress = std::clamp(elapsed / effect->duration, 0.0f, 1.0f);
            if (progress >= 1.0f)
            {
                effect = floatingNumberEffects.erase(effect);
                continue;
            }

            sf::Vector2f position = effect->boardPosition
                ? boardFootprintAnchor(effect->row, effect->column, 1, gameSnapshot.yourPlayer)
                : effect->screenPosition;
            position.y -= 28.0f * progress;
            sf::Color color = effect->color;
            color.a = static_cast<std::uint8_t>(std::clamp(255.0f * (1.0f - progress), 0.0f, 255.0f));
            drawText(window, font, effect->text, 20, position, color, 120.0f);
            ++effect;
        }

        // Persistent attachments remain visible on their board targets.
        for (const game_data::Enchantment& enchantment : gameSnapshot.enchantments)
        {
            if (enchantment.target == static_cast<std::uint8_t>(game_data::EnchantmentTarget::Player))
            {
                continue;
            }
            int row = enchantment.targetRow;
            int column = enchantment.targetColumn;
            if (enchantment.target == static_cast<std::uint8_t>(game_data::EnchantmentTarget::Piece))
            {
                const game_data::Piece* targetPiece = gamePieceById(enchantment.targetPieceId);
                if (!targetPiece)
                {
                    continue;
                }
                row = targetPiece->row;
                column = targetPiece->column;
            }
            if (!game_data::inBounds(row, column))
            {
                continue;
            }
            const sf::Vector2f anchor = boardFootprintAnchor(row, column, 1, gameSnapshot.yourPlayer);
            const sf::Vector2f badgePosition{anchor.x + 12.0f, anchor.y - 35.0f};
            sf::CircleShape badge(11.0f);
            badge.setPosition({badgePosition.x - 11.0f, badgePosition.y - 11.0f});
            badge.setFillColor(sf::Color(94, 47, 128, 230));
            badge.setOutlineThickness(2.0f);
            badge.setOutlineColor(sf::Color(223, 164, 255));
            window.draw(badge);
            if (sf::Texture* icon = cardArtTexture(enchantment.imagePath))
            {
                drawContainSprite(window, *icon, {{badgePosition.x - 8.0f, badgePosition.y - 8.0f}, {16.0f, 16.0f}});
            }
            else
            {
                drawText(window, font, "E", 12, {badgePosition.x - 4.0f, badgePosition.y - 8.0f}, sf::Color::White);
            }
        }

        // Compact game readout. Player ownership is always laid out from left
        // to right so both players' state is easy to compare at a glance.
        const game_data::PlayerSnapshot& mine = gameSnapshot.players[static_cast<std::size_t>(me - 1)];
        const game_data::PlayerSnapshot& playerOne = gameSnapshot.players[0];
        const game_data::PlayerSnapshot& playerTwo = gameSnapshot.players[1];
        const int activePlayer = std::clamp(gameSnapshot.activePlayer, 1, 2);
        const std::string activePlayerName = storyMode
            ? (activePlayer == 1 ? "Blackthorn Company" : "Mirewatch")
            : (sandboxMode
            ? "Player " + std::to_string(activePlayer)
            : (activePlayer == me ? loggedInUsername : "Opponent"));

        auto timerText = [](std::int64_t milliseconds) {
            const std::int64_t totalSeconds =
                (std::max<std::int64_t>(0, milliseconds) + 999) / 1000;
            const auto twoDigits = [](std::int64_t value) {
                return (value < 10 ? "0" : "") + std::to_string(value);
            };
            const std::int64_t days = totalSeconds / (24 * 60 * 60);
            const std::int64_t hours = (totalSeconds / (60 * 60)) % 24;
            const std::int64_t minutes = totalSeconds / 60;
            const std::int64_t seconds = totalSeconds % 60;
            if (days > 0)
            {
                return std::to_string(days) + "d " + twoDigits(hours) + ":" +
                    twoDigits(minutes % 60) + ":" + twoDigits(seconds);
            }
            if (hours > 0)
            {
                return std::to_string(hours) + ":" + twoDigits(minutes % 60) +
                    ":" + twoDigits(seconds);
            }
            return std::to_string(minutes) + ":" +
                twoDigits(seconds);
        };
        // Display-only interpolation between authoritative server snapshots.
        // Turn transitions, clock deductions, and timeout wins remain entirely
        // server-side; no locally projected value is sent back with an action.
        const std::int64_t snapshotAgeMs = gameSnapshotReceivedAt ==
                std::chrono::steady_clock::time_point{}
            ? 0
            : std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - gameSnapshotReceivedAt).count();
        const auto liveTimer = [&](std::int64_t remainingMs, bool ticking) {
            return std::max<std::int64_t>(
                0,
                remainingMs - (ticking ? snapshotAgeMs : 0));
        };
        // ---- Owner banners ---------------------------------------------------
        // The side gutters are deliberately compact. Put the identity and player
        // clock on the first row, then give Resources, Lands, and enchantments
        // separate columns below it so the long Resources caption cannot run into
        // another readout.
        const auto playerDisplayName = [&](int playerNumber) {
            if (storyMode)
            {
                return playerNumber == 1 ? std::string("Blackthorn") : std::string("Mirewatch");
            }
            if (sandboxMode)
            {
                return "Player " + std::to_string(playerNumber);
            }
            return playerNumber == me ? loggedInUsername : std::string("Opponent");
        };

        // A labelled figure: small muted caption over a large parchment number.
        const auto drawBannerFigure = [&](sf::Vector2f center,
                                          const std::string& caption,
                                          const std::string& value,
                                          sf::Color valueColor) {
            sf::Text captionText(font, caption, 8);
            captionText.setFillColor(withAlpha(BoardParchmentMuted, 208));
            centerText(captionText, {center.x, center.y - 8.0f});
            drawCrispText(window, captionText);

            sf::Text valueText(font, value, 16);
            valueText.setFillColor(valueColor);
            valueText.setOutlineThickness(1.0f);
            valueText.setOutlineColor(sf::Color(0, 0, 0, 170));
            centerText(valueText, {center.x, center.y + 8.0f});
            drawCrispText(window, valueText);
        };

        const auto drawPlayerBanner = [&](int playerNumber,
                                          const game_data::PlayerSnapshot& player) {
            const bool leftSide = playerNumber == 1;
            const float x = leftSide ? GamePlayerBannerLeftX : GamePlayerBannerRightX;
            const bool isActive = playerNumber == activePlayer &&
                phase != game_data::Phase::GameOver;
            const sf::Color accent = ownerColor(playerNumber);

            const bool enchantmentTarget = draggedHandCard &&
                draggedHandCard->type == "Enchantment" && draggedHandCard->target == "player";
            const bool hoveredForEnchantment = enchantmentTarget &&
                playerReadoutAtPixel(gameDragCurrentPos) == std::optional<int>(playerNumber);

            if (isActive)
            {
                // The side to move gets an outer bloom rather than a colour swap,
                // so turn ownership is obvious without recolouring the type.
                drawCutPlate(
                    {x - 3.0f, GameTopBarY - 3.0f},
                    {GamePlayerBannerWidth + 6.0f, GamePlayerBannerHeight + 6.0f},
                    11.0f,
                    withAlpha(accent, 40),
                    withAlpha(accent, 150),
                    1.4f);
            }
            drawBeveledPlate(
                window,
                {x, GameTopBarY},
                {GamePlayerBannerWidth, GamePlayerBannerHeight},
                withAlpha(ownerColorDeep(playerNumber), 236),
                hoveredForEnchantment ? sf::Color(223, 164, 255)
                                      : (isActive ? BoardBrassBright : BoardBrass),
                isActive,
                9.0f);
            if (enchantmentTarget)
            {
                drawCutPlate(
                    {x + 3.0f, GameTopBarY + 3.0f},
                    {GamePlayerBannerWidth - 6.0f, GamePlayerBannerHeight - 6.0f},
                    7.0f,
                    withAlpha(BoardArcane, hoveredForEnchantment ? 96 : 44),
                    withAlpha(sf::Color(223, 164, 255), hoveredForEnchantment ? 235 : 150),
                    hoveredForEnchantment ? 1.8f : 1.0f);
            }

            const int enchantmentCount = static_cast<int>(std::count_if(
                gameSnapshot.enchantments.begin(),
                gameSnapshot.enchantments.end(),
                [&](const game_data::Enchantment& enchantment) {
                    return enchantment.target ==
                            static_cast<std::uint8_t>(game_data::EnchantmentTarget::Player) &&
                        enchantment.targetPlayer == playerNumber;
                }));

            // Resources are unbounded in sandbox. The tutorial has no economy at all,
            // so it drops the figures rather than printing a placeholder in them —
            // this readout used to render the literal string "story" in both.
            const std::string resources =
                sandboxMode && !storyMode ? std::string("Free") : std::to_string(player.resources);
            const std::string control = std::to_string(player.controlledSquares);

            // Figures occupy their own lower row. All three columns use the same
            // positions on both sides so the labels remain evenly separated.
            const float figureStep = 58.0f;
            const int figureCount = storyMode ? 1 : (enchantmentCount > 0 ? 3 : 2);
            const float figureBlockX = x +
                (GamePlayerBannerWidth - figureStep * static_cast<float>(figureCount)) * 0.5f;
            const float figureY = GameTopBarY + GamePlayerBannerHeight - 21.0f;

            float slot = 0.0f;
            const auto nextFigureCenter = [&]() {
                const float center = figureBlockX + figureStep * (slot + 0.5f);
                slot += 1.0f;
                return sf::Vector2f{center, figureY};
            };
            if (storyMode)
            {
                drawBannerFigure(
                    nextFigureCenter(), "LANDS", control,
                    withAlpha(ownerColorBright(playerNumber), 255));
            }
            else
            {
                drawBannerFigure(nextFigureCenter(), "RESOURCES", resources, BoardBrassBright);
                drawBannerFigure(
                    nextFigureCenter(), "LANDS", control,
                    withAlpha(ownerColorBright(playerNumber), 255));
                if (enchantmentCount > 0)
                {
                    drawBannerFigure(
                        nextFigureCenter(), "HEX", std::to_string(enchantmentCount),
                        sf::Color(206, 162, 240));
                }
            }

            // Name and player clock share the upper row, away from the sigil.
            const float textLeft = x + 22.0f;
            const float textRight = x + GamePlayerBannerWidth - 22.0f;
            const float textWidth = std::max(30.0f, textRight - textLeft);
            sf::Text nameText(
                font,
                elideToWidth(font, playerDisplayName(playerNumber), 14, textWidth),
                14);
            nameText.setFillColor(isActive ? BoardParchment : withAlpha(BoardParchmentMuted, 226));
            nameText.setPosition({
                textLeft,
                GameTopBarY + 10.0f});
            drawCrispText(window, nameText);

            if (gameSnapshot.timersEnabled)
            {
                const std::int64_t clockMs = liveTimer(
                    player.clockRemainingMs,
                    phase == game_data::Phase::Playing &&
                        playerNumber == gameSnapshot.activePlayer);
                sf::Text clockText(font, timerText(clockMs), 13);
                clockText.setFillColor(clockMs <= 30'000
                    ? sf::Color(233, 128, 106)
                    : withAlpha(BoardParchmentMuted, 232));
                clockText.setPosition({textLeft, GameTopBarY + 36.0f});
                drawCrispText(window, clockText);
            }
        };

        drawPlayerBanner(1, playerOne);
        drawPlayerBanner(2, playerTwo);

        // ---- Active turn readout ---------------------------------------------
        {
            const bool myTurn = sandboxMode || activePlayer == me;
            const std::string turnLabel = phase == game_data::Phase::GameOver
                ? std::string("MATCH OVER")
                : (phase == game_data::Phase::HeroPlacement
                    ? std::string("DEPLOY YOUR HEROES")
                    : (storyMode ? std::string("TUTORIAL")
                                 : (myTurn ? std::string("YOUR TURN")
                                           : std::string("OPPONENT'S TURN"))));
            const int turnPlayer = activePlayer == 2 ? 2 : 1;
            const float turnX = turnPlayer == 1
                ? GamePlayerBannerLeftX
                : GamePlayerBannerRightX;
            const sf::Color accent = phase == game_data::Phase::GameOver
                ? BoardBrass
                : ownerColor(turnPlayer);

            drawBeveledPlate(
                window,
                {turnX, GameTurnPlaqueY},
                {GameTurnPlaqueWidth, GameTurnPlaqueHeight},
                withAlpha(BoardPlate, 238),
                accent,
                true,
                12.0f);

            sf::Text label(font, elideToWidth(font, turnLabel, 8, GameTurnPlaqueWidth - 16.0f), 8);
            label.setLetterSpacing(1.05f);
            label.setFillColor(BoardParchment);
            label.setOutlineThickness(1.0f);
            label.setOutlineColor(sf::Color(0, 0, 0, 190));
            const bool showTurnClock =
                gameSnapshot.timersEnabled && phase != game_data::Phase::GameOver;
            centerText(
                label, {turnX + GameTurnPlaqueWidth * 0.5f,
                        GameTurnPlaqueY + (showTurnClock ? 14.0f : 24.0f)});
            drawCrispText(window, label);

            // The turn clock is meaningless once the match is decided, so the
            // plaque drops the drain row rather than freezing a stale figure.
            if (gameSnapshot.timersEnabled && phase != game_data::Phase::GameOver)
            {
                const std::int64_t liveTurnRemainingMs = liveTimer(
                    gameSnapshot.turnRemainingMs, phase == game_data::Phase::Playing);
                const bool urgent = liveTurnRemainingMs <= 30'000;

                // A drain bar beside the figure, so time pressure is felt rather
                // than only read off a clock string.
                sf::Text turnClock(font, timerText(liveTurnRemainingMs), 13);
                turnClock.setFillColor(urgent
                    ? sf::Color(240, 152, 132)
                    : withAlpha(BoardParchmentMuted, 240));
                const float trackX = turnX + 10.0f;
                const float trackWidth = GameTurnPlaqueWidth - 20.0f;
                const float rowY = GameTurnPlaqueY + 41.0f;

                centerText(turnClock, {turnX + GameTurnPlaqueWidth * 0.5f, GameTurnPlaqueY + 29.0f});
                drawCrispText(window, turnClock);

                sf::RectangleShape track({trackWidth, 4.0f});
                track.setPosition({trackX, rowY - 2.0f});
                track.setFillColor(sf::Color(0, 0, 0, 200));
                window.draw(track);
                const float fraction = std::clamp(
                    static_cast<float>(liveTurnRemainingMs) / 60'000.0f, 0.0f, 1.0f);
                sf::RectangleShape fill({std::max(1.0f, trackWidth * fraction), 4.0f});
                fill.setPosition({trackX, rowY - 2.0f});
                fill.setFillColor(urgent ? sf::Color(233, 128, 106) : BoardBrass);
                window.draw(fill);
                drawEdgeLine(
                    window,
                    {trackX, rowY - 2.5f},
                    {trackX + trackWidth, rowY - 2.5f},
                    1.0f,
                    withAlpha(BoardBrassDim, 220));
            }
        }

        if (displayedClockWarning &&
            std::chrono::steady_clock::now() < displayedClockWarning->visibleUntil)
        {
            const bool myClock = displayedClockWarning->playerNumber == me;
            const std::int64_t thresholdSeconds = displayedClockWarning->thresholdMs / 1000;
            const std::string timeLabel = thresholdSeconds >= 60
                ? std::to_string(thresholdSeconds / 60) +
                    (thresholdSeconds == 60 ? " minute" : " minutes")
                : std::to_string(thresholdSeconds) + " seconds";
            const std::string warningText =
                (myClock ? "Your clock" : "Opponent's clock") +
                std::string(": ") + timeLabel + " remaining";
            const float pulse = 0.5f + 0.5f * std::sin(animationTime * 9.0f);
            const auto accentRed = static_cast<std::uint8_t>(235.0f + pulse * 20.0f);
            const auto accentGreen = static_cast<std::uint8_t>(92.0f + pulse * 45.0f);
            const sf::Color accent(accentRed, accentGreen, 72);
            constexpr sf::Vector2f warningSize{360.0f, 42.0f};
            const sf::Vector2f warningPosition{
                BoardCenterX - warningSize.x * 0.5f,
                BoardOriginY + 8.0f};
            drawBeveledPlate(
                window,
                warningPosition,
                warningSize,
                sf::Color(52, 14, 12, 242),
                accent,
                true,
                8.0f);
            sf::Text clockWarningText(font, warningText, 18);
            clockWarningText.setFillColor(sf::Color(255, 238, 212));
            centerText(
                clockWarningText,
                {BoardCenterX, warningPosition.y + warningSize.y * 0.5f});
            window.draw(clockWarningText);
        }

        // The upper board row can rise into the top readout band. Draw its art
        // again after the HUD so it remains visible; the foreground pass omits
        // bases, pips, crowns, and control badges so those details retain normal
        // board depth and cannot cover a piece in front of them.
        drawPieceLayer(true);

        // ---- Command bar ------------------------------------------------------
        // A rail carrying resources, the piles, the hand and the turn actions. The
        // whole region used to be unstyled screen, with the buttons running off
        // the bottom edge.
        {
            const sf::Vector2f barPosition{14.0f, GameBottomBarY};
            const sf::Vector2f barSize{772.0f, GameBottomBarHeight};
            drawCutPlate(
                barPosition, barSize, 14.0f, sf::Color(10, 15, 16, 236), BoardBrassDim, 1.6f);
            // Lit top lip, so the rail reads as a raised ledge under the board.
            drawEdgeLine(
                window,
                {barPosition.x + 16.0f, barPosition.y + 1.0f},
                {barPosition.x + barSize.x - 16.0f, barPosition.y + 1.0f},
                1.4f,
                withAlpha(BoardBrass, 176));
            drawEdgeLine(
                window,
                {barPosition.x + 20.0f, barPosition.y + 3.5f},
                {barPosition.x + barSize.x - 20.0f, barPosition.y + 3.5f},
                1.0f,
                sf::Color(0, 0, 0, 130));
        }

        // The command ribbon explains the exact visual grammar currently on the
        // board. It is presentation-only: selection, legal-square calculation,
        // and all action submission remain the existing game-state paths above.
        const auto drawCommandIntent = [&]() {
            std::string title;
            std::string instruction;
            sf::Color accent = BoardBrassBright;

            const bool hasMove = std::find(highlight.begin(), highlight.end(), 1) != highlight.end();
            const bool hasAttack = std::find(highlight.begin(), highlight.end(), 2) != highlight.end();
            const bool hasEffect = std::find(highlight.begin(), highlight.end(), 4) != highlight.end();
            const bool hasDeploy = std::find(highlight.begin(), highlight.end(), 3) != highlight.end();

            if (highlightedPiece && (hasMove || hasAttack || hasEffect))
            {
                const auto action = std::find_if(
                    highlightedPiece->actions.begin(),
                    highlightedPiece->actions.end(),
                    [&](const game_data::ActionProfile& candidate) {
                        return candidate.state == highlightedPiece->actionState;
                    });
                title = action != highlightedPiece->actions.end() && !action->name.empty()
                    ? action->name
                    : highlightedPiece->name;
                if (hasAttack && hasMove)
                {
                    instruction = "TEAL ARROWS MOVE  |  RED CROSSHAIRS ATTACK";
                    accent = sf::Color(226, 132, 108);
                }
                else if (hasAttack)
                {
                    instruction = "RED CROSSHAIRS MARK DAMAGE TARGETS";
                    accent = sf::Color(226, 132, 108);
                }
                else if (hasEffect)
                {
                    instruction = "VIOLET DIAMONDS MARK EFFECT TARGETS";
                    accent = sf::Color(186, 138, 234);
                }
                else
                {
                    instruction = "TEAL ARROWS MARK LEGAL DESTINATIONS";
                    accent = sf::Color(126, 214, 178);
                }
            }
            else if (actingHandIndex && *actingHandIndex < gameSnapshot.hand.size() &&
                     (hasAttack || hasEffect || hasDeploy))
            {
                const game_data::GameCard& card = gameSnapshot.hand[*actingHandIndex];
                title = card.title;
                if (hasAttack)
                {
                    instruction = "RED CROSSHAIRS MARK DAMAGE TARGETS";
                    accent = sf::Color(226, 132, 108);
                }
                else if (hasEffect)
                {
                    instruction = "VIOLET DIAMONDS MARK EFFECT TARGETS";
                    accent = sf::Color(186, 138, 234);
                }
                else
                {
                    instruction = "BRASS MARKERS SHOW LEGAL DEPLOYMENT";
                }
            }

            if (title.empty())
            {
                return;
            }

            constexpr sf::Vector2f ribbonSize{420.0f, 17.0f};
            const sf::Vector2f ribbonPosition{BoardCenterX - ribbonSize.x * 0.5f, 449.0f};
            drawCutPlate(
                ribbonPosition,
                ribbonSize,
                6.0f,
                sf::Color(7, 13, 14, 238),
                withAlpha(accent, 210),
                1.2f);
            sf::Text titleText(font, elideToWidth(font, title, 9, 116.0f), 9);
            titleText.setFillColor(BoardParchment);
            titleText.setPosition({ribbonPosition.x + 11.0f, ribbonPosition.y + 4.0f});
            drawCrispText(window, titleText);
            drawEdgeLine(
                window,
                {ribbonPosition.x + 136.0f, ribbonPosition.y + 4.0f},
                {ribbonPosition.x + 136.0f, ribbonPosition.y + ribbonSize.y - 4.0f},
                1.0f,
                withAlpha(accent, 180));
            sf::Text instructionText(font, instruction, 8);
            instructionText.setLetterSpacing(1.08f);
            instructionText.setFillColor(withAlpha(accent, 246));
            instructionText.setPosition({ribbonPosition.x + 147.0f, ribbonPosition.y + 5.0f});
            drawCrispText(window, instructionText);
        };
        drawCommandIntent();

        const bool commandBarActive = phase == game_data::Phase::Playing && !storyMode;
        if (commandBarActive)
        {
            drawDrawPile(mine.drawPileCount);

            const bool draggingHandCard =
                gameDragActive &&
                gameDragKind == GameDragKind::HandCard &&
                draggingHandIndex;
            const bool draggingHandOverTrash =
                draggingHandCard &&
                isDiscardTrashCanAtPixel(gameDragCurrentPos);
            drawDiscardTrashCan(playerCanDiscardThisTurn(), draggingHandCard, draggingHandOverTrash);
        }

        const bool abilityAvailable = phase == game_data::Phase::Playing &&
            (sandboxMode || gameSnapshot.activePlayer == me) && selectedPiece &&
            pieceCanTakeGameAction(*selectedPiece) &&
            game_data::pieceAbilityAvailable(gameSnapshot.pieces, *selectedPiece);
        if (!abilityAvailable && !storyMode)
        {
            // The opponent's hand size was not surfaced anywhere before, and it
            // keeps the ability slot from reading as a hole in the bar.
            drawCutPlate(
                {GameActionButtonX, GameAbilityButtonY},
                {GameActionButtonWidth, GameActionButtonHeight},
                7.0f,
                sf::Color(13, 19, 20, 226),
                withAlpha(BoardBrassDim, 220),
                1.2f);
            sf::Text caption(font, "OPPONENT'S HAND", 8);
            caption.setLetterSpacing(1.2f);
            caption.setFillColor(withAlpha(BoardParchmentMuted, 210));
            caption.setPosition({GameActionButtonX + 12.0f, GameAbilityButtonY + 9.0f});
            drawCrispText(window, caption);
            sf::Text count(
                font,
                std::to_string(gameSnapshot.players[me == 1 ? 1 : 0].handCount),
                14);
            count.setFillColor(BoardParchment);
            centerText(
                count,
                {GameActionButtonX + GameActionButtonWidth - 18.0f,
                 GameAbilityButtonY + GameActionButtonHeight * 0.5f});
            drawCrispText(window, count);
        }

        const bool storyResultAction = storyMode && storyStage != StoryStage::Objective;
        if ((phase == game_data::Phase::Playing &&
             (sandboxMode || gameSnapshot.activePlayer == me)) || storyResultAction)
        {
            if (phase == game_data::Phase::Playing && abilityAvailable)
            {
                abilityButton.setLabel(game_data::pieceAbilityLabel(*selectedPiece));
                abilityButton.draw(window);
            }
            // Warm bloom behind the primary action, so ending the turn outranks
            // leaving the match instead of the two reading as equal peers.
            drawCutPlate(
                {GameActionButtonX - 4.0f, GameActionButtonY - 4.0f},
                {GameActionButtonWidth + 8.0f, GamePrimaryButtonHeight + 8.0f},
                11.0f,
                withAlpha(BoardBrassBright, 34),
                withAlpha(BoardBrassBright, 128),
                1.4f);
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
            storyRestartButton.draw(window);
        }

        if (storyMode)
        {
            const StoryMission& mission = storyMissions()[static_cast<std::size_t>(storyMissionIndex)];
            std::string stepHeading = std::string(mission.lesson);
            std::string stepBody = std::string(mission.objective) + " " + std::string(mission.hint);
            const int stepNumber = storyMissionIndex + 1;
            sf::Color stepAccent = BoardBrassBright;
            if (storyStage == StoryStage::Complete)
            {
                stepHeading = "Mission Complete";
                stepBody = storyMissionIndex + 1 < static_cast<int>(storyMissions().size())
                    ? "The next chapter is unlocked. Continue when you are ready."
                    : "You have completed the Blackthorn tutorial arc.";
                stepAccent = sf::Color(146, 232, 166);
            }
            else if (storyStage == StoryStage::Failed)
            {
                stepHeading = "Mission Failed";
                stepBody = "The Mirewatch force stopped you. Retry and adapt your position, attack order, and powers.";
                stepAccent = sf::Color(233, 128, 106);
            }

            const bool leavesRoomForStoryHand = !gameSnapshot.hand.empty();
            const sf::Vector2f plaquePosition{
                leavesRoomForStoryHand ? 10.0f : GameBottomLeftX,
                GameBottomBarY + 14.0f};
            const sf::Vector2f plaqueSize{
                leavesRoomForStoryHand ? 170.0f : 560.0f,
                GameBottomBarHeight - 28.0f};
            drawBeveledPlate(
                window,
                plaquePosition,
                plaqueSize,
                withAlpha(BoardPlate, 242),
                stepAccent,
                true,
                12.0f);

            // Step medallion, so progress through the tutorial is legible.
            const sf::Vector2f medallion{
                plaquePosition.x + (leavesRoomForStoryHand ? 25.0f : 32.0f),
                plaquePosition.y + plaqueSize.y * 0.5f};
            drawEllipseOutline(window, medallion, 19.0f, 19.0f, 1.4f, withAlpha(stepAccent, 150));
            sf::CircleShape medallionFace(15.0f, 24);
            medallionFace.setOrigin({15.0f, 15.0f});
            medallionFace.setPosition(medallion);
            medallionFace.setFillColor(sf::Color(22, 30, 28, 246));
            medallionFace.setOutlineThickness(1.6f);
            medallionFace.setOutlineColor(withAlpha(stepAccent, 235));
            window.draw(medallionFace);
            sf::Text stepText(font, std::to_string(stepNumber), 17);
            stepText.setFillColor(stepAccent);
            centerText(stepText, medallion);
            drawCrispText(window, stepText);

            const float textX = plaquePosition.x + (leavesRoomForStoryHand ? 48.0f : 62.0f);
            if (leavesRoomForStoryHand)
            {
                switch (storyMissionIndex)
                {
                case 3:
                    stepHeading = "Deploy a Unit";
                    stepBody = "Deploy the Debt Collector on the glowing square.";
                    break;
                case 4:
                    stepHeading = "Hide & Summon";
                    stepBody = "Use Hide and Create Lumberjack.";
                    break;
                case 5:
                    stepHeading = "Combined Arms";
                    stepBody = "Defeat both Mirewatch defenders.";
                    break;
                case 6:
                    stepHeading = "Break the Escort";
                    stepBody = "Break the escort. Defeat Donella.";
                    break;
                default:
                    stepHeading = "Full Encounter";
                    stepBody = "Defeat every Mirewatch unit.";
                    break;
                }
            }
            sf::Text heading(font, stepHeading, leavesRoomForStoryHand ? 13 : 17);
            heading.setLetterSpacing(1.1f);
            heading.setFillColor(BoardParchment);
            heading.setPosition({textX, plaquePosition.y + 16.0f});
            drawCrispText(window, heading);
            drawWrappedText(
                window,
                font,
                stepBody,
                leavesRoomForStoryHand ? 10 : 13,
                {textX, plaquePosition.y + 44.0f},
                withAlpha(BoardParchmentMuted, 240),
                plaqueSize.x - (leavesRoomForStoryHand ? 56.0f : 78.0f),
                3.0f);
        }

        // ---- Hand -------------------------------------------------------------
        if (!storyMode || !gameSnapshot.hand.empty())
        {
            clampListOffset(gameHandOffset, gameSnapshot.hand.size(), VisibleGameHandCards);
            const std::size_t lastHandCard =
                std::min(gameSnapshot.hand.size(), gameHandOffset + VisibleGameHandCards);
            for (std::size_t i = gameHandOffset; i < lastHandCard; ++i)
            {
                const float x =
                    HandStartX + static_cast<float>(i - gameHandOffset) * (HandCardWidth + HandGap);
                const game_data::GameCard& card = gameSnapshot.hand[i];
                const bool affordable = phase == game_data::Phase::HeroPlacement ||
                    (gameSnapshot.relentlessPieceId == 0 &&
                     (sandboxMode || card.cost <= mine.resources) && (sandboxMode || gameSnapshot.activePlayer == me) &&
                     phase == game_data::Phase::Playing &&
                     (sandboxMode || game_data::heroTraitsAllowCard(gameSnapshot.pieces, me, card)));
                drawGameCardFace({x, HandY}, card, selectedHandIndex && *selectedHandIndex == i, affordable);
            }

            if (gameSnapshot.hand.empty())
            {
                sf::Text emptyHand(font, "No cards in hand", 13);
                emptyHand.setFillColor(withAlpha(BoardParchmentMuted, 168));
                centerText(
                    emptyHand,
                    {HandStartX + (HandCardWidth * static_cast<float>(VisibleGameHandCards) +
                                   HandGap * static_cast<float>(VisibleGameHandCards - 1)) * 0.5f,
                     HandY + HandCardHeight * 0.5f});
                drawCrispText(window, emptyHand);
            }
            else if (gameSnapshot.hand.size() > VisibleGameHandCards)
            {
                // Overflow chevrons instead of the "Cards 1-5/6" debug readout.
                const float handRight = HandStartX +
                    HandCardWidth * static_cast<float>(VisibleGameHandCards) +
                    HandGap * static_cast<float>(VisibleGameHandCards - 1);
                const float arrowY = HandY + HandCardHeight * 0.5f;
                const auto drawChevron = [&](float centerX, bool pointsLeft, bool enabled) {
                    const sf::Color accent = enabled
                        ? withAlpha(BoardBrassBright, 236)
                        : withAlpha(BoardBrassDim, 150);
                    const float reach = pointsLeft ? -4.5f : 4.5f;
                    drawEdgeLine(
                        window, {centerX - reach, arrowY - 7.0f}, {centerX + reach, arrowY}, 2.2f,
                        accent);
                    drawEdgeLine(
                        window, {centerX - reach, arrowY + 7.0f}, {centerX + reach, arrowY}, 2.2f,
                        accent);
                };
                drawChevron(HandStartX - 8.0f, true, gameHandOffset > 0);
                drawChevron(handRight + 8.0f, false, lastHandCard < gameSnapshot.hand.size());

                const std::size_t hidden = gameSnapshot.hand.size() - (lastHandCard - gameHandOffset);
                if (hidden > 0)
                {
                    sf::Text more(font, "+" + std::to_string(hidden), 10);
                    more.setFillColor(withAlpha(BoardParchmentMuted, 226));
                    centerText(more, {handRight + 8.0f, arrowY + 18.0f});
                    drawCrispText(window, more);
                }
            }
        }

        if (gameDragActive)
        {
            if (gameDragKind == GameDragKind::HandCard && draggingHandIndex &&
                *draggingHandIndex < gameSnapshot.hand.size())
            {
                const game_data::GameCard& draggedCard = gameSnapshot.hand[*draggingHandIndex];
                const bool draggingHandOverTrash =
                    canDiscardHandCard(*draggingHandIndex) && isDiscardTrashCanAtPixel(gameDragCurrentPos);
                if ((draggedCard.type == "Unit" || draggedCard.type == "Hero") && !draggingHandOverTrash)
                {
                    sf::Vector2f anchor = gameDragCurrentPos;
                    float scale = 1.0f;
                    if (draggedHandSquare)
                    {
                        const BoardCellMetrics metrics =
                            boardCellMetrics(draggedHandSquare->first, draggedHandSquare->second);
                        anchor = boardFootprintAnchor(
                            draggedHandSquare->first,
                            draggedHandSquare->second,
                            draggedCard.width,
                            gameSnapshot.yourPlayer);
                        scale = metrics.depthScale;
                    }
                    drawCardPiecePreview(draggedCard, sandboxPlayer, anchor, scale, draggedHandDropValid);
                }
                else
                {
                    const bool affordable = gameSnapshot.relentlessPieceId == 0 &&
                        (sandboxMode || draggedCard.cost <= mine.resources) &&
                        (sandboxMode || gameSnapshot.activePlayer == me) && phase == game_data::Phase::Playing &&
                        (sandboxMode || game_data::heroTraitsAllowCard(gameSnapshot.pieces, me, draggedCard));
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
                    anchor = boardFootprintAnchor(
                        draggedPieceSquare->first,
                        draggedPieceSquare->second,
                        draggedPiece->width,
                        gameSnapshot.yourPlayer);
                    scale = metrics.depthScale;
                }
                const sf::Color tint = draggedPieceDropValid
                    ? sf::Color(255, 255, 255, 220)
                    : sf::Color(220, 120, 110, 190);
                drawPieceBase(
                    window,
                    anchor,
                    scale,
                    draggedPiece->owner,
                    false,
                    static_cast<float>(draggedPiece->width));
                drawPieceSelectionRing(
                    window,
                    anchor,
                    scale,
                    0.7f,
                    draggedPieceDropValid ? BoardBrassBright : sf::Color(232, 104, 92));
                const bool drewPiece = drawPieceVisual(
                    pieceTokenPath(*draggedPiece),
                    pieceWalkAnimPath(*draggedPiece),
                    "",
                    pieceBasePath(*draggedPiece),
                    draggedPiece->owner == 2,
                    draggedPiece->walkAnimFrames,
                    1,
                    anchor,
                    scale,
                    tint,
                    -1,
                    -1,
                    draggedPiece->width,
                    draggedPiece->height);
                if (!drewPiece)
                {
                    drawPieceCameo(
                        anchor, scale, draggedPiece->owner, draggedPiece->imagePath, tint);
                }

                drawPieceHealthPips(
                    anchor,
                    scale,
                    draggedPiece->health,
                    draggedPiece->maxHealth,
                    draggedPiece->owner,
                    false);
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
            const bool victory = gameSnapshot.winner == me;
            const sf::Color accent = victory ? sf::Color(146, 232, 166) : sf::Color(233, 128, 106);
            const sf::Color accentDeep = victory ? sf::Color(28, 74, 44) : sf::Color(84, 32, 24);

            // Dim the battlefield so the result reads as a modal.
            sf::RectangleShape overlay({ui_canvas::Width, ui_canvas::Height});
            overlay.setPosition({ui_canvas::Left, 0.0f});
            overlay.setFillColor(sf::Color(5, 8, 9, 196));
            window.draw(overlay);

            // Halo in the result colour. Three discrete circles left visible
            // banding rings, so this stacks many faint layers instead.
            drawSoftEllipse(window, {400.0f, 262.0f}, 108.0f, 96.0f, withAlpha(accent, 34), 16);

            const sf::Vector2f panelPosition{252.0f, 186.0f};
            const sf::Vector2f panelSize{296.0f, 176.0f};
            drawPanel(window, panelPosition, panelSize);

            // Header plate straddles the panel's top edge and carries the result color.
            drawBeveledPlate(
                window,
                {278.0f, 162.0f},
                {244.0f, 56.0f},
                victory ? sf::Color(17, 34, 24, 250) : sf::Color(38, 19, 16, 250),
                accent,
                true,
                14.0f);

            const std::string result = victory ? "VICTORY" : "DEFEAT";
            sf::Text titleShadow(font, result, 32);
            titleShadow.setLetterSpacing(1.5f);
            titleShadow.setFillColor(sf::Color(0, 0, 0, 200));
            centerText(titleShadow, {401.5f, 192.0f});
            drawCrispText(window, titleShadow);
            sf::Text titleText(font, result, 32);
            titleText.setLetterSpacing(1.5f);
            titleText.setFillColor(accent);
            titleText.setOutlineThickness(1.5f);
            titleText.setOutlineColor(accentDeep);
            centerText(titleText, {400.0f, 190.0f});
            drawCrispText(window, titleText);

            drawSeparatorRule(window, {310.0f, 236.0f}, 180.0f);

            // Result figures, laid out as labelled rows rather than a stack of
            // loose centred sentences with dead space between them.
            const auto drawResultRow = [&](float y,
                                           const std::string& caption,
                                           const std::string& value,
                                           sf::Color valueColor) {
                sf::Text captionText(font, caption, 9);
                captionText.setLetterSpacing(1.2f);
                captionText.setFillColor(withAlpha(BoardParchmentMuted, 208));
                captionText.setPosition({panelPosition.x + 26.0f, y});
                drawCrispText(window, captionText);

                sf::Text valueText(font, value, 16);
                valueText.setFillColor(valueColor);
                sf::FloatRect bounds = valueText.getLocalBounds();
                valueText.setPosition({
                    panelPosition.x + panelSize.x - 26.0f - (bounds.position.x + bounds.size.x),
                    y - 5.0f});
                drawCrispText(window, valueText);
            };

            if (conquestBattleMode)
            {
                drawResultRow(254.0f, "CAMPAIGN", "Result recorded", BoardParchment);
            }
            else if (gameResultReceived && gameResultSuccess)
            {
                drawResultRow(
                    254.0f,
                    "RATING",
                    std::string(gameRatingChange >= 0 ? "+" : "") + std::to_string(gameRatingChange),
                    gameRatingChange >= 0 ? sf::Color(146, 232, 166) : sf::Color(233, 128, 106));
            }
            else
            {
                drawResultRow(
                    254.0f,
                    "RATING",
                    gameResultReceived ? "Unavailable" : "Pending...",
                    withAlpha(BoardParchmentMuted, 226));
            }
            if (!gameRewardText.empty())
            {
                // The reward string often already carries its own "Reward:"
                // prefix, which would repeat the caption beside it.
                std::string rewardValue = gameRewardText;
                static constexpr std::string_view RewardPrefix = "Reward:";
                if (rewardValue.compare(0, RewardPrefix.size(), RewardPrefix) == 0)
                {
                    rewardValue.erase(0, RewardPrefix.size());
                    while (!rewardValue.empty() && rewardValue.front() == ' ')
                    {
                        rewardValue.erase(0, 1);
                    }
                }
                drawResultRow(
                    282.0f, "REWARD", elideToWidth(font, rewardValue, 16, 150.0f), BoardBrassBright);
            }

            drawSeparatorRule(window, {310.0f, 312.0f}, 180.0f);

            const std::string returnAction = conquestBattleMode
                ? "Press Map to return to the campaign."
                : ((sandboxMode || phase == game_data::Phase::GameOver)
                    ? "Press Leave to return."
                    : "Press Resign to return.");
            sf::Text hintLine(font, returnAction, 12);
            hintLine.setFillColor(withAlpha(BoardParchmentMuted, 200));
            centerText(hintLine, {400.0f, 336.0f});
            drawCrispText(window, hintLine);

            // Keep the exit action bright above the dimmed battlefield.
            leaveGameButton.draw(window);
        }

        drawPiecePopup();
    };
