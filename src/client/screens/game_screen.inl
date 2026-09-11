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
                               bool affordable,
                               float readableWidth,
                               unsigned int minimumNameSize = 0) {
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
        readableWidth = std::clamp(readableWidth, 38.0f, width);
        const unsigned int nameSize = std::max(
            minimumNameSize, readableWidth < width ? 8U : 9U);
        const float nameWidth = readableWidth - 10.0f;
        const std::vector<std::string> nameLines =
            wrapText(font, card.title, nameSize, nameWidth);
        const std::size_t shownLines = std::min<std::size_t>(2, nameLines.size());
        for (std::size_t line = 0; line < shownLines; ++line)
        {
            const bool lastShown = line + 1 == shownLines;
            const bool truncated = lastShown && nameLines.size() > shownLines;
            sf::Text nameText(
                font,
                truncated ? elideToWidth(font, nameLines[line] + "...", nameSize, nameWidth)
                          : nameLines[line],
                nameSize);
            nameText.setFillColor(inkColor);
            nameText.setOutlineThickness(1.0f);
            nameText.setOutlineColor(sf::Color(0, 0, 0, 210));
            centerText(
                nameText,
                {position.x + readableWidth * 0.5f,
                  scrimBottom - 6.0f -
                      static_cast<float>(shownLines - 1 - line) *
                          std::max(10.0f, static_cast<float>(nameSize) + 1.0f)});
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

    auto appendKeywordRuleDescriptions = [&](DetailRows& descriptions,
                                             const std::vector<std::string>& keywords,
                                             const std::string& summonTitle) {
        const sf::Color keywordColor(146, 206, 226);
        if (game_data::hasKeyword(keywords, "relentless"))
        {
            descriptions.push_back({
                "Relentless: after this piece destroys a piece, only it may take one immediate extra action. Card plays, draws, discards, other pieces, and other abilities wait; another destruction can chain, or End Turn may stop the chain.",
                keywordColor});
        }
        if (game_data::hasKeyword(keywords, "bodyguard"))
        {
            descriptions.push_back({
                "Bodyguard: positive damage aimed at an adjacent friendly non-Bodyguard is redirected among all adjacent friendly Bodyguards and split as evenly as possible. Each Bodyguard assigned damage also takes that action's Disable; zero-damage Disable, Push, Pull, Infest, and Control still affect the originally selected unit. Indivisible leftover damage is assigned randomly. Bodyguard resolves before Intercept.",
                keywordColor});
        }
        if (game_data::hasKeyword(keywords, "intercept"))
        {
            descriptions.push_back({
                "Intercept: when a positive-damage enemy piece attack targets an adjacent friendly piece, this piece automatically swaps in and takes that attack if it has not intercepted since its last owner-turn start. If several legal Interceptors qualify, one is chosen uniformly at random. Both full footprints must exchange legally; otherwise the original target is hit. Acting, Disable, Sleep, and Growth do not prevent Intercept. It cannot protect a Bodyguard or another Intercept, and zero-damage attacks and spells do not trigger it.",
                keywordColor});
        }
        if (game_data::hasKeyword(keywords, "trail"))
        {
            descriptions.push_back({
                summonTitle.empty()
                    ? "Trail unavailable: this live card does not name the Unit its Trail would create."
                    : "Trail: after this piece moves, create " + summonTitle +
                        " where it started. The new Unit cannot act until its owner's next turn.",
                summonTitle.empty() ? sf::Color(225, 170, 150) : keywordColor});
        }
        if (game_data::hasKeyword(keywords, "foresight"))
        {
            descriptions.push_back({
                "Foresight: every paid 50-Resource draw reveals up to one plus the number of your Foresight pieces, limited by the cards left in your deck. Keep one; put the rest on the bottom. Choosing completes that same paid draw; it is not a second command.",
                keywordColor});
        }
        if (game_data::hasKeyword(keywords, "reveal"))
        {
            descriptions.push_back({
                "Reveal: at the end of this piece's owner's turn, every adjacent enemy in Dematerialize state 1 returns directly to visible state 0. This passive reveal does not apply Disable.",
                keywordColor});
        }
        if (game_data::hasKeyword(keywords, "dematerialize"))
        {
            descriptions.push_back({
                "Dematerialize: while in a nonzero action state, opponents cannot see this piece and it exerts no square control. Movement, Push, Pull, Unit deployment, or Summon collisions materialize it and make it Disabled for its next owner-turn activation; Reveal returns state 1 directly to state 0 without applying Disable.",
                keywordColor});
        }
    };

    auto appendAbilityRuleDescription = [&](DetailRows& descriptions,
                                            const std::string& abilityValue,
                                            const std::string& summonTitle,
                                            const std::vector<std::string>& labels,
                                            int state,
                                            int abilityUses) {
        const std::string ability = game_data::normalizedAbility(abilityValue);
        if (ability.empty())
        {
            return;
        }
        std::string label = "Use Ability";
        if (!labels.empty())
        {
            label = labels[static_cast<std::size_t>(
                std::max(0, state) % static_cast<int>(labels.size()))];
        }
        else if (ability == "dig") label = "Dig";
        else if (ability == "summon") label = "Summon";
        else if (ability == "command") label = "Command";
        else if (ability == "transform") label = "Transform";
        else if (ability == "dematerialize")
            label = state == 0 ? "Dematerialize" : "Materialize";

        std::string rule;
        sf::Color color(210, 216, 228);
        if (ability == "dig")
        {
            rule = "marks this piece's square as a hole; Tunnel actions travel between marked holes";
        }
        else if (ability == "summon")
        {
            if (summonTitle.empty())
            {
                descriptions.push_back({
                    "Ability unavailable: this live card names Summon but defines no Unit to create.",
                    sf::Color(225, 170, 150)});
                return;
            }
            rule = "creates " + summonTitle +
                " in the square directly in front (right for Player 1, left for Player 2); the new Unit cannot act until its owner's next turn. If an opposing hidden piece is there, Summon instead spends this action, creates no Unit, and materializes that piece Disabled for its next owner-turn activation";
        }
        else if (ability == "command")
        {
            rule = "spends this piece's action, then lets one ready adjacent friendly piece take an immediate action; card plays, draws, and discards wait until it resolves, End Turn may decline it, and the turn's normal piece action remains available afterward";
        }
        else if (ability == "transform")
        {
            rule = "cycles this piece to its next action state";
        }
        else if (ability == "dematerialize")
        {
            rule = "cycles action state; every nonzero state is hidden from the opponent and exerts no control";
        }
        else
        {
            rule = "uses this card's live ability";
        }
        std::string uses;
        if (ability == "dig")
        {
            uses = abilityUses < 0
                ? " Uses are unlimited."
                : " Uses remaining: " + std::to_string(std::max(0, abilityUses)) + ".";
        }
        descriptions.push_back({
            "Ability - " + label + ": " + rule +
                ". This counts as a piece action." + uses,
            color});
    };

    auto appendPassiveRuleDescriptions = [&](DetailRows& descriptions,
                                             int tax,
                                             int gatherResources,
                                             int healingAura,
                                             bool canControl,
                                             int growTurns,
                                             const std::string& rebirthTitle) {
        if (tax > 0)
        {
            descriptions.push_back({
                "Tax: at the start of this piece's owner's turn, transfer up to " +
                    std::to_string(tax) + " Resources from the opponent.",
                sf::Color(190, 198, 214)});
        }
        if (gatherResources > 0)
        {
            descriptions.push_back({
                "Gather: gain " + std::to_string(gatherResources) +
                    " Resources at the start of this piece's owner's turn.",
                sf::Color(190, 198, 214)});
        }
        if (healingAura > 0)
        {
            descriptions.push_back({
                "Healing aura: restore " + std::to_string(healingAura) +
                    " health to every other adjacent piece at this piece's owner's turn end, regardless of side.",
                sf::Color(190, 198, 214)});
        }
        if (!canControl)
        {
            descriptions.push_back({
                "No control: this piece neither controls its occupied squares nor influences adjacent territory.",
                sf::Color(225, 170, 150)});
        }
        if (growTurns > 0)
        {
            descriptions.push_back({
                "Growth: enters play unable to act for " + std::to_string(growTurns) +
                    " turn(s), then becomes ready at the start of its following turn.",
                sf::Color(210, 180, 105)});
        }
        if (!rebirthTitle.empty())
        {
            descriptions.push_back({
                "Rebirth: when destroyed, this piece is replaced on the same square by " +
                    rebirthTitle +
                    " for its original side and cannot act until its next turn. A successful Rebirth is not a kill, stops attack movement before its square, and carries attachments to the replacement.",
                sf::Color(194, 150, 235)});
        }
    };

    const auto actionStateReachable = [](
                                          const std::vector<game_data::ActionProfile>& actions,
                                          std::string_view ability,
                                          int startState,
                                          int targetState) {
        if (startState == targetState)
        {
            return true;
        }
        int stateCount = std::max(startState, targetState) + 1;
        for (const game_data::ActionProfile& action : actions)
        {
            stateCount = std::max(stateCount, action.state + 1);
            stateCount = std::max(
                stateCount, game_data::actionNextState(action) + 1);
        }
        const std::string normalized = game_data::normalizedAbility(
            std::string(ability));
        if ((normalized == "transform" || normalized == "dematerialize") &&
            targetState >= 0 && targetState < stateCount)
        {
            return true;
        }

        std::vector<bool> reached(static_cast<std::size_t>(stateCount), false);
        if (startState < 0 || startState >= stateCount ||
            targetState < 0 || targetState >= stateCount)
        {
            return false;
        }
        reached[static_cast<std::size_t>(startState)] = true;
        bool changed = true;
        while (changed)
        {
            changed = false;
            for (const game_data::ActionProfile& action : actions)
            {
                if (action.state < 0 || action.state >= stateCount ||
                    !reached[static_cast<std::size_t>(action.state)])
                {
                    continue;
                }
                const int nextState = game_data::actionNextState(action);
                if (nextState >= 0 && nextState < stateCount &&
                    !reached[static_cast<std::size_t>(nextState)])
                {
                    reached[static_cast<std::size_t>(nextState)] = true;
                    changed = true;
                }
            }
        }
        return static_cast<bool>(
            reached[static_cast<std::size_t>(targetState)]);
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
            descriptions.push_back({"Sleeping: " + std::to_string(piece.sleepTurnsRemaining) +
                                        " owner turn(s) - movement is blocked, but a stationary attack remains legal if this piece can otherwise act",
                                    sf::Color(120, 190, 230)});
        }
        if (piece.originalOwner != 0)
        {
            descriptions.push_back({piece.controlTurnsRemaining > 0
                                        ? "Under control: " +
                                              std::to_string(piece.controlTurnsRemaining) + " turns"
                                        : "Under control: final turn",
                                    ownerColor(piece.owner)});
        }
        if (!piece.infestationTitle.empty())
        {
            descriptions.push_back({"Infested: " + piece.infestationTitle,
                                    sf::Color(194, 150, 235)});
        }
        appendAbilityRuleDescription(
            descriptions, piece.ability, piece.summonTitle, piece.abilityLabels,
            piece.actionState, piece.abilityUses);
        appendKeywordRuleDescriptions(descriptions, piece.keywords, piece.summonTitle);
        appendPassiveRuleDescriptions(
            descriptions, piece.tax, piece.gatherResources, piece.healingAura,
            piece.canControl, piece.growTurnsRemaining, piece.rebirthTitle);
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
        const bool usesActionStates = std::any_of(
            piece.actions.begin(), piece.actions.end(),
            [](const game_data::ActionProfile& action) {
                return action.state != 0 ||
                    game_data::actionNextState(action) != action.state;
            });
        const auto stateName = [&](int state) {
            const std::string ability = game_data::normalizedAbility(piece.ability);
            if (ability == "dematerialize")
            {
                return state == 0
                    ? std::string("Materialized")
                    : std::string("Hidden");
            }
            if (ability == "transform" &&
                piece.abilityLabels.size() >= 2 &&
                piece.abilityLabels[0] == "Raise Gun" &&
                piece.abilityLabels[1] == "Lower Gun")
            {
                return state == 0
                    ? std::string("Gun lowered")
                    : std::string("Gun raised");
            }
            return "State " + std::to_string(state);
        };
        std::vector<std::size_t> displayOrder;
        displayOrder.reserve(piece.actions.size());
        for (std::size_t i = 0; i < piece.actions.size(); ++i)
        {
            if (piece.actions[i].state == piece.actionState)
            {
                displayOrder.push_back(i);
            }
        }
        for (std::size_t i = 0; i < piece.actions.size(); ++i)
        {
            if (piece.actions[i].state != piece.actionState)
            {
                displayOrder.push_back(i);
            }
        }
        for (std::size_t i : displayOrder)
        {
            const game_data::ActionProfile& action = piece.actions[i];
            if (usesActionStates)
            {
                const bool reachable = actionStateReachable(
                    piece.actions, piece.ability, piece.actionState, action.state);
                std::string stateRule = action.state == piece.actionState
                    ? "AVAILABLE NOW - "
                    : reachable
                        ? "INACTIVE - requires "
                        : "UNREACHABLE IN THIS CARD DEFINITION - requires ";
                stateRule += stateName(action.state);
                const int nextState = game_data::actionNextState(action);
                stateRule += nextState == action.state
                    ? "; remains in this state"
                    : "; after use: " + stateName(nextState);
                descriptions.push_back({
                    std::move(stateRule),
                    action.state == piece.actionState
                        ? sf::Color(136, 226, 190)
                        : reachable
                            ? sf::Color(225, 188, 120)
                            : sf::Color(242, 126, 112)});
            }
            descriptions.push_back(actionDetailRow(
                action,
                i,
                action.state == piece.actionState
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
        if (card.type == "Spell" && !game_data::isSupportedSpellEffect(card))
        {
            return std::string(
                "Unavailable: this Spell has no defined game effect and cannot be played.");
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
        const bool unsupportedSpell =
            card.type == "Spell" && !game_data::isSupportedSpellEffect(card);
        descriptions.push_back({
            cardPlayDescription(card),
            unsupportedSpell
                ? sf::Color(242, 126, 112)
                : sf::Color(210, 216, 228)});
        if (card.type == "Unit" || card.type == "Hero")
        {
            if (card.type == "Unit")
            {
                descriptions.push_back({
                    "Deployment collision: if an apparently empty controlled footprint contains only opposing hidden pieces, the card and Resources are spent, no Unit appears, and every hidden blocker materializes Disabled for its next owner-turn activation.",
                    sf::Color(210, 216, 228)});
            }
            if (card.type == "Unit" && !card.traits.empty())
            {
                descriptions.push_back({
                    "Deploy traits: living friendly Heroes must collectively supply every Trait - " +
                        joinStrings(card.traits, ", ") + ".",
                    sf::Color(248, 214, 112)});
            }
            if (card.width > 1 || card.height > 1)
            {
                descriptions.push_back({
                    "Footprint: every square of this " + std::to_string(card.width) +
                        "x" + std::to_string(card.height) +
                        " piece must satisfy placement, movement, occupancy, and control rules. Range, adjacency, and a clear ranged path use the nearest squares of both footprints. Movement checks the full footprint at every step; an attacking landing affects every legal enemy footprint it overlaps.",
                    sf::Color(248, 214, 112)});
            }
            appendAbilityRuleDescription(
                descriptions, card.ability, card.summonTitle, card.abilityLabels,
                0, card.abilityUses);
            appendKeywordRuleDescriptions(descriptions, card.keywords, card.summonTitle);
            appendPassiveRuleDescriptions(
                descriptions, card.tax, card.gatherResources, card.healingAura,
                card.canControl, card.growTurns, card.rebirthTitle);
            if (card.actions.empty())
            {
                descriptions.push_back({"Actions: none", sf::Color(225, 170, 150)});
            }
            const bool usesActionStates = std::any_of(
                card.actions.begin(), card.actions.end(),
                [](const game_data::ActionProfile& action) {
                    return action.state != 0 ||
                        game_data::actionNextState(action) != action.state;
                });
            const auto stateName = [&](int state) {
                const std::string ability = game_data::normalizedAbility(card.ability);
                if (ability == "dematerialize")
                {
                    return state == 0
                        ? std::string("Materialized")
                        : std::string("Hidden");
                }
                if (ability == "transform" && card.abilityLabels.size() >= 2 &&
                    card.abilityLabels[0] == "Raise Gun" &&
                    card.abilityLabels[1] == "Lower Gun")
                {
                    return state == 0
                        ? std::string("Gun lowered")
                        : std::string("Gun raised");
                }
                return "State " + std::to_string(state);
            };
            if (usesActionStates)
            {
                descriptions.push_back({
                    "Deploys in " + stateName(0) + ". Actions below state when they are available.",
                    sf::Color(210, 216, 228)});
            }
            for (std::size_t i = 0; i < card.actions.size(); ++i)
            {
                const game_data::ActionProfile& action = card.actions[i];
                if (usesActionStates)
                {
                    const int nextState = game_data::actionNextState(action);
                    const bool reachable = actionStateReachable(
                        card.actions, card.ability, 0, action.state);
                    descriptions.push_back({
                        std::string(reachable ? "AVAILABLE IN " :
                            "UNREACHABLE IN THIS CARD DEFINITION - requires ") +
                            stateName(action.state) +
                            (nextState == action.state
                                ? "; remains in this state"
                                : "; after use: " + stateName(nextState)),
                        !reachable
                            ? sf::Color(242, 126, 112)
                            : action.state == 0
                            ? sf::Color(136, 226, 190)
                            : sf::Color(190, 198, 214)});
                }
                descriptions.push_back(actionDetailRow(action, i));
            }
        }
        return descriptions;
    };

    auto popupActionContentHeight = [&](const DetailRows& descriptions) {
        return detailRowsScrollContentHeight(descriptions, PiecePopupTextWidth);
    };

    auto popupMaxScroll = [&](const DetailRows& descriptions) {
        return std::max(0.0f, popupActionContentHeight(descriptions) - PiecePopupScrollHeight);
    };

    // A caption under one of the bottom-bar slots.
    auto drawSlotCaption = [&](float centerX, float y, const std::string& caption, sf::Color color) {
        sf::Text text(font, caption, 10);
        text.setStyle(sf::Text::Bold);
        text.setLetterSpacing(1.05f);
        text.setFillColor(color);
        centerText(text, {centerX, y});
        drawCrispText(window, text);
    };

    // The deck is the paid-draw button. Keep the count, action, exact price and
    // disabled reason legible without requiring a tooltip.
    auto drawDrawPile = [&](int remaining, bool available, bool hovered, const std::string& caption) {
        const float centerX = GameDeckPileX + GamePileWidth * 0.5f;
        const float lift = available && hovered ? 3.0f : 0.0f;
        const sf::Color accent = available
            ? (hovered ? BoardBrassBright : BoardBrass)
            : BoardBrassDim;
        const int layers = remaining <= 0 ? 1 : std::min(3, 1 + remaining / 8);
        for (int i = layers - 1; i >= 0; --i)
        {
            const float offset = static_cast<float>(i) * 2.2f;
            const bool top = i == 0;
            drawCutPlate(
                {GameDeckPileX - offset, GamePileY - offset - lift},
                {GamePileWidth, GamePileHeight - 14.0f},
                6.0f,
                remaining > 0 ? sf::Color(18, 25, 30, 246) : sf::Color(18, 20, 21, 210),
                remaining > 0 ? (top ? accent : BoardBrassDim) : sf::Color(72, 62, 50),
                top ? (hovered && available ? 2.2f : 1.5f) : 1.0f);
        }

        const float faceY = GamePileY - lift;
        sf::Text count(font, std::to_string(std::max(0, remaining)) + " LEFT", 10);
        count.setLetterSpacing(1.1f);
        count.setFillColor(withAlpha(BoardParchmentMuted, remaining > 0 ? 220 : 150));
        centerText(count, {centerX, faceY + 13.0f});
        drawCrispText(window, count);

        sf::Text drawLabel(font, "DRAW", 12);
        drawLabel.setLetterSpacing(1.25f);
        drawLabel.setFillColor(available ? BoardParchment : withAlpha(BoardParchmentMuted, 170));
        centerText(drawLabel, {centerX, faceY + 34.0f});
        drawCrispText(window, drawLabel);

        sf::Text cost(font, std::to_string(game_data::DrawCardResourceCost), 22);
        cost.setStyle(sf::Text::Bold);
        cost.setFillColor(available ? accent : withAlpha(BoardParchmentMuted, 160));
        cost.setOutlineThickness(1.0f);
        cost.setOutlineColor(sf::Color(0, 0, 0, 210));
        centerText(cost, {centerX, faceY + 57.0f});
        drawCrispText(window, cost);

        sf::Text resourceLabel(font, "RESOURCES", 9);
        resourceLabel.setLetterSpacing(1.05f);
        resourceLabel.setFillColor(withAlpha(BoardParchmentMuted, available ? 220 : 145));
        centerText(resourceLabel, {centerX, faceY + 76.0f});
        drawCrispText(window, resourceLabel);

        drawSlotCaption(
            centerX, GamePileY + GamePileHeight - 5.0f, caption,
            available ? withAlpha(BoardBrassBright, 240) : withAlpha(BoardParchmentMuted, 180));
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
            available
                ? (draggingCard ? withAlpha(accent, 255) : withAlpha(BoardParchment, 244))
                : withAlpha(sf::Color(174, 143, 126), 220));
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
                if (card->gatherResources > 0)
                {
                    stats.push_back({
                        "GATHER", "+" + std::to_string(card->gatherResources), BoardBrassBright});
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

        const float maxScroll = popupMaxScroll(actionDescriptions);
        inspectedPieceScroll = std::clamp(inspectedPieceScroll, 0.0f, maxScroll);

        sf::Text detailHeading(font, piece ? "DETAILS" : "ACTIONS", 13);
        detailHeading.setLetterSpacing(1.3f);
        detailHeading.setFillColor(withAlpha(BoardParchmentMuted, 240));
        detailHeading.setPosition({PiecePopupTextX, PiecePopupActionHeadingY});
        drawCrispText(window, detailHeading);
        if (maxScroll > 0.0f)
        {
            sf::Text scrollHint(font, "SCROLL: WHEEL OR UP / DOWN", 9);
            scrollHint.setFillColor(withAlpha(sf::Color(143, 220, 205), 230));
            scrollHint.setPosition({PiecePopupTextX + PiecePopupTextWidth -
                                        scrollHint.getLocalBounds().size.x,
                                    PiecePopupActionHeadingY + 2.0f});
            drawCrispText(window, scrollHint);
        }
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
        const float actionViewportY = PiecePopupScrollY + PiecePopupScrollTextYInset;
        const float actionViewportHeight = popupScrollHeight - PiecePopupScrollTextYInset * 2.0f;
        sf::View actionView(sf::FloatRect(
            {PiecePopupTextX, actionViewportY + inspectedPieceScroll},
            {PiecePopupTextWidth, actionViewportHeight}));
        // Map the popup's logical rectangle through the active fixed canvas.
        // Hard-coded 800x600 fractions shift the child viewport left now that
        // the 16:9 view includes logical side gutters.
        const sf::FloatRect baseViewport = previousView.getViewport();
        const sf::Vector2f baseViewSize = previousView.getSize();
        const sf::Vector2f baseViewTopLeft =
            previousView.getCenter() - baseViewSize * 0.5f;
        const sf::Vector2f popupViewportPosition{
            (PiecePopupTextX - baseViewTopLeft.x) / baseViewSize.x,
            (actionViewportY - baseViewTopLeft.y) / baseViewSize.y};
        const sf::Vector2f popupViewportSize{
            PiecePopupTextWidth / baseViewSize.x,
            actionViewportHeight / baseViewSize.y};
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
                actionViewportY,
                PiecePopupTextWidth,
                actionViewportHeight))
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

    auto drawForesightChoices = [&]() {
        if (gameSnapshot.foresightChoices.empty())
        {
            foresightChoiceRowOffset = 0;
            return;
        }

        sf::RectangleShape overlay({ui_canvas::Width, ui_canvas::Height});
        overlay.setPosition({ui_canvas::Left, 0.0f});
        overlay.setFillColor(sf::Color(5, 8, 9, 196));
        window.draw(overlay);

        drawBeveledPlate(
            window,
            {24.0f, 54.0f},
            {752.0f, 524.0f},
            sf::Color(14, 24, 24, 250),
            BoardBrassBright,
            true,
            14.0f);

        sf::Text title(font, "FORESIGHT", 23);
        title.setLetterSpacing(1.8f);
        title.setFillColor(BoardBrassBright);
        centerText(title, {400.0f, 82.0f});
        drawCrispText(window, title);
        // A scripted Story choice may require one exact revealed card. Derive
        // that requirement from the active step and only advertise it when the
        // authored card is actually present. Ordinary Foresight stays truthful,
        // while malformed Story data fails closed.
        std::string requiredStoryCard;
        if (storyMode && storyStage == StoryStage::Objective &&
            storyMissionStep >= 0)
        {
            const StoryMission& mission = activeStoryMission();
            if (storyMissionStep < static_cast<int>(mission.script.size()))
            {
                const StoryScriptAction& step =
                    mission.script[static_cast<std::size_t>(storyMissionStep)];
                const bool cardIsPresent = !step.cardTitle.empty() &&
                    std::any_of(
                        gameSnapshot.foresightChoices.begin(),
                        gameSnapshot.foresightChoices.end(),
                        [&](const game_data::GameCard& card) {
                            return card.title == step.cardTitle;
                        });
                if (step.kind == StoryActionKind::ChooseForesight &&
                    step.owner == gameSnapshot.yourPlayer && cardIsPresent)
                {
                    requiredStoryCard = step.cardTitle;
                }
            }
        }

        const bool showingStoryCorrection = storyMode &&
            storyStage == StoryStage::Objective && !storyCorrection.empty();
        const std::string guidance = showingStoryCorrection
            ? "TRY AGAIN: " + storyCorrection
            : !requiredStoryCard.empty()
                ? "STORY STEP: " + requiredStoryCard +
                    " is required. Keep the card marked REQUIRED; the other returns to the bottom."
                : "Your 50-Resource draw triggered Foresight. Choose one; the rest go to the bottom.";
        if (showingStoryCorrection)
        {
            drawBeveledPlate(
                window,
                {78.0f, 103.0f},
                {644.0f, 45.0f},
                sf::Color(54, 24, 22, 252),
                sf::Color(233, 128, 106),
                true,
                8.0f);
        }
        drawWrappedText(
            window,
            font,
            guidance,
            requiredStoryCard.empty() && !showingStoryCorrection ? 13 : 14,
            {92.0f, showingStoryCorrection ? 112.0f : 106.0f},
            showingStoryCorrection ? sf::Color(255, 224, 210) : BoardParchment,
            616.0f,
            1.0f);

        const std::size_t totalRows =
            (gameSnapshot.foresightChoices.size() + ForesightChoiceColumns - 1) /
            ForesightChoiceColumns;
        clampListOffset(foresightChoiceRowOffset, totalRows, ForesightVisibleRows);
        const std::size_t visibleRows = std::min(
            ForesightVisibleRows, totalRows - foresightChoiceRowOffset);
        for (std::size_t visibleRow = 0; visibleRow < visibleRows; ++visibleRow)
        {
            const std::size_t row = foresightChoiceRowOffset + visibleRow;
            const std::size_t rowStart = row * ForesightChoiceColumns;
            const std::size_t rowCards = std::min(
                ForesightChoiceColumns, gameSnapshot.foresightChoices.size() - rowStart);
            const float rowWidth = static_cast<float>(rowCards) * HandCardWidth +
                static_cast<float>(rowCards - 1) * ForesightChoiceGap;
            const float startX = (ui_canvas::Width - rowWidth) * 0.5f;
            const float y = ForesightChoiceY + static_cast<float>(visibleRow) *
                ForesightChoiceRowPitch;
            for (std::size_t column = 0; column < rowCards; ++column)
            {
                const std::size_t index = rowStart + column;
                const float x = startX + static_cast<float>(column) *
                    (HandCardWidth + ForesightChoiceGap);
                const bool requiredChoice = !requiredStoryCard.empty() &&
                    gameSnapshot.foresightChoices[index].title == requiredStoryCard;
                drawGameCardFace(
                    {x, y},
                    gameSnapshot.foresightChoices[index],
                    requiredChoice,
                    true,
                    HandCardWidth,
                    12);
                drawCutPlate(
                    {x, y + HandCardHeight + 4.0f},
                    {HandCardWidth, 25.0f},
                    5.0f,
                    requiredChoice ? sf::Color(25, 66, 58, 248)
                                   : sf::Color(8, 14, 15, 238),
                    requiredChoice ? sf::Color(110, 244, 205)
                                   : withAlpha(BoardBrassBright, 190),
                    requiredChoice ? 2.0f : 1.0f);
                sf::Text choose(
                    font,
                    requiredChoice ? "REQUIRED" : "CHOOSE",
                    requiredChoice ? 10 : 11);
                choose.setStyle(sf::Text::Bold);
                choose.setLetterSpacing(requiredChoice ? 0.45f : 0.8f);
                choose.setFillColor(
                    requiredChoice ? sf::Color(154, 255, 222) : BoardBrassBright);
                centerText(
                    choose,
                    {x + HandCardWidth * 0.5f, y + HandCardHeight + 16.0f});
                drawCrispText(window, choose);
            }
        }
        if (totalRows > ForesightVisibleRows)
        {
            sf::Text scrollHint(font, "SCROLL FOR MORE", 9);
            scrollHint.setLetterSpacing(1.0f);
            scrollHint.setFillColor(BoardParchmentMuted);
            centerText(scrollHint, {400.0f, 558.0f});
            drawCrispText(window, scrollHint);
        }
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
            const std::size_t targetIndex =
                static_cast<std::size_t>(game_data::squareIndex(storyTargetRow, storyTargetColumn));
            bool restrictToAuthoredTarget = false;
            bool requiredActorSelected = false;
            if (storyStage == StoryStage::Objective)
            {
                const StoryMission& activeMission =
                    storyMissions(storyCampaign)[static_cast<std::size_t>(storyMissionIndex)];
                if (storyMissionStep >= 0 &&
                    storyMissionStep < static_cast<int>(activeMission.script.size()))
                {
                    const StoryScriptAction& step =
                        activeMission.script[static_cast<std::size_t>(storyMissionStep)];
                    restrictToAuthoredTarget =
                        step.kind == StoryActionKind::Move || step.kind == StoryActionKind::Attack;
                    requiredActorSelected = step.actorRole.empty() ||
                        (highlightedPiece &&
                         highlightedPiece->id == storyPieceIdForRole(step.actorRole));
                }
            }
            if (restrictToAuthoredTarget)
            {
                const int normalActionMarker =
                    requiredActorSelected ? highlight[targetIndex] : 0;
                std::fill(highlight.begin(), highlight.end(), 0);
                highlight[targetIndex] = normalActionMarker;
            }
            else
            {
                highlight[targetIndex] = 5;
            }
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

        int requiredStoryActorId = 0;
        int requiredStoryTargetId = 0;
        std::vector<int> requiredStorySurvivorIds;
        if (storyMode && storyStage == StoryStage::Objective)
        {
            const StoryMission& activeMission =
                storyMissions(storyCampaign)[static_cast<std::size_t>(storyMissionIndex)];
            requiredStorySurvivorIds.reserve(
                activeMission.requiredSurvivorRoles.size());
            for (std::string_view role : activeMission.requiredSurvivorRoles)
            {
                const int survivorId = storyPieceIdForRole(role);
                if (survivorId != 0)
                {
                    requiredStorySurvivorIds.push_back(survivorId);
                }
            }
            if (storyMissionStep >= 0 &&
                storyMissionStep < static_cast<int>(activeMission.script.size()))
            {
                const StoryScriptAction& currentStep =
                    activeMission.script[static_cast<std::size_t>(storyMissionStep)];
                if (!currentStep.actorRole.empty())
                {
                    requiredStoryActorId = storyPieceIdForRole(currentStep.actorRole);
                }
                if (!currentStep.targetRole.empty())
                {
                    requiredStoryTargetId = storyPieceIdForRole(currentStep.targetRole);
                }
                else if (!currentStep.effectRole.empty())
                {
                    requiredStoryTargetId = storyPieceIdForRole(currentStep.effectRole);
                }
            }
            else if (activeMission.objectiveSpec.kind == StoryObjectiveKind::DefeatRole &&
                     !activeMission.objectiveSpec.targetRole.empty())
            {
                requiredStoryTargetId =
                    storyPieceIdForRole(activeMission.objectiveSpec.targetRole);
            }
        }

        const auto drawPieceLayer = [&](bool foregroundOnly) {
        for (const game_data::Piece* piecePtr : pieceDrawOrder)
        {
            const game_data::Piece& piece = *piecePtr;
            BoardCellMetrics cell = boardCellMetrics(piece.row, piece.column);
            sf::Vector2f anchor = boardFootprintAnchor(
                piece.row, piece.column, piece.width, piece.height, gameSnapshot.yourPlayer);
            sf::Vector2f baseCenter = boardFootprintCenter(
                piece.row, piece.column, piece.width, piece.height, gameSnapshot.yourPlayer);
            sf::Vector2f healthBadgeCenter = boardFootprintHealthBadgeCenter(
                piece.row,
                piece.column,
                piece.width,
                piece.height,
                gameSnapshot.yourPlayer,
                piece.owner);
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
                        piece.width, piece.height, gameSnapshot.yourPlayer);
                    const sf::Vector2f end = boardFootprintAnchor(
                        animation->second.toRow, animation->second.toColumn,
                        piece.width, piece.height, gameSnapshot.yourPlayer);
                    const sf::Vector2f startBaseCenter = boardFootprintCenter(
                        animation->second.fromRow,
                        animation->second.fromColumn,
                        piece.width,
                        piece.height,
                        gameSnapshot.yourPlayer);
                    const sf::Vector2f endBaseCenter = boardFootprintCenter(
                        animation->second.toRow,
                        animation->second.toColumn,
                        piece.width,
                        piece.height,
                        gameSnapshot.yourPlayer);
                    const sf::Vector2f startHealthBadgeCenter =
                        boardFootprintHealthBadgeCenter(
                            animation->second.fromRow,
                            animation->second.fromColumn,
                            piece.width,
                            piece.height,
                            gameSnapshot.yourPlayer,
                            piece.owner);
                    const sf::Vector2f endHealthBadgeCenter =
                        boardFootprintHealthBadgeCenter(
                            animation->second.toRow,
                            animation->second.toColumn,
                            piece.width,
                            piece.height,
                            gameSnapshot.yourPlayer,
                            piece.owner);
                    anchor = {
                        start.x + (end.x - start.x) * progress,
                        start.y + (end.y - start.y) * progress};
                    baseCenter = {
                        startBaseCenter.x + (endBaseCenter.x - startBaseCenter.x) * progress,
                        startBaseCenter.y + (endBaseCenter.y - startBaseCenter.y) * progress};
                    healthBadgeCenter = {
                        startHealthBadgeCenter.x +
                            (endHealthBadgeCenter.x - startHealthBadgeCenter.x) * progress,
                        startHealthBadgeCenter.y +
                            (endHealthBadgeCenter.y - startHealthBadgeCenter.y) * progress};
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
                        const sf::Vector2f lungeOffset{
                            dx / distance * lunge,
                            dy / distance * lunge + shake};
                        anchor += lungeOffset;
                        baseCenter += lungeOffset;
                        healthBadgeCenter += lungeOffset;
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
                  (gameSnapshot.relentlessPieceId != 0 && piece.id != gameSnapshot.relentlessPieceId) ||
                  (gameSnapshot.players[static_cast<std::size_t>(
                       std::clamp(gameSnapshot.activePlayer, 1, 2) - 1)]
                       .pieceActionUsedThisTurn &&
                   gameSnapshot.relentlessPieceId == 0 &&
                   gameSnapshot.commandingPieceId == 0 &&
                   piece.repeatActionIndex < 0)) &&
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
                    baseCenter,
                    pieceScale,
                    piece.owner,
                    pieceUnavailable,
                    static_cast<float>(piece.width),
                    static_cast<float>(piece.height),
                    pieceBaseArtworkFor(piece.owner, piece.width, piece.height),
                    rarityGemArtworkFor(piece.name));

                const float footprintWidth = std::max(1.0f, static_cast<float>(piece.width));
                const float footprintHeight = std::max(1.0f, static_cast<float>(piece.height));

                const bool pieceIsSelected = selectedPieceId && *selectedPieceId == piece.id;
                const bool pieceIsRequiredStoryActor =
                    requiredStoryActorId != 0 && requiredStoryActorId == piece.id;
                const bool pieceIsRequiredStoryTarget =
                    requiredStoryTargetId != 0 && requiredStoryTargetId == piece.id;
                const bool pieceIsRequiredStorySurvivor =
                    std::find(
                        requiredStorySurvivorIds.begin(),
                        requiredStorySurvivorIds.end(),
                        piece.id) != requiredStorySurvivorIds.end();
                if (pieceIsRequiredStorySurvivor)
                {
                    const float pulse = 0.5f + 0.5f * std::sin(animationTime * 3.8f);
                    drawPieceSelectionRing(
                        window,
                        baseCenter,
                        pieceScale * 1.18f,
                        pulse,
                        sf::Color(255, 214, 104),
                        footprintWidth,
                        footprintHeight);
                }
                if (pieceIsRequiredStoryActor)
                {
                    const float pulse = 0.5f + 0.5f * std::sin(animationTime * 4.6f);
                    drawPieceSelectionRing(
                        window,
                        baseCenter,
                        pieceScale * 1.14f,
                        pulse,
                        sf::Color(255, 214, 104),
                        footprintWidth,
                        footprintHeight);
                }
                else if (pieceIsRequiredStoryTarget)
                {
                    const float pulse = 0.5f + 0.5f * std::sin(animationTime * 4.2f);
                    drawPieceSelectionRing(
                        window,
                        baseCenter,
                        pieceScale * 1.10f,
                        pulse,
                        sf::Color(238, 116, 94),
                        footprintWidth,
                        footprintHeight);
                }
                else if (pieceIsSelected)
                {
                    const float pulse = 0.5f + 0.5f * std::sin(animationTime * 3.4f);
                    drawPieceSelectionRing(
                        window,
                        baseCenter,
                        pieceScale,
                        pulse,
                        BoardBrassBright,
                        footprintWidth,
                        footprintHeight);
                }
                else if (highlightedPiece && highlightedPiece->id == piece.id)
                {
                    drawPieceSelectionRing(
                        window,
                        baseCenter,
                        pieceScale,
                        0.35f,
                        withAlpha(BoardBrassBright, 170),
                        footprintWidth,
                        footprintHeight);
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
                drawPieceHealthBadge(
                    window,
                    healthBadgeCenter,
                    pieceScale,
                    piece.health,
                    piece.owner,
                    pieceUnavailable,
                    font,
                    piece.isHero);
            }

            if (!foregroundOnly && piece.originalOwner != 0)
            {
                // Held piece: keep the violet badge opposite the health badge.
                const float chipHeight = std::clamp(16.0f * pieceScale, 13.0f, 22.0f);
                const float chipWidth = std::clamp(31.0f * pieceScale, 25.0f, 44.0f);
                const float radius = std::clamp(8.0f * pieceScale, 7.0f, 11.0f);
                const sf::Vector2f center{
                    anchor.x + (piece.owner == 1 ? 1.0f : -1.0f) *
                        (chipWidth * 0.5f - radius * 0.2f),
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
                    piece.controlTurnsRemaining > 0
                        ? std::to_string(piece.controlTurnsRemaining)
                        : std::string("F"),
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
                    const float footprintWidth = std::max(1.0f, static_cast<float>(piece.width));
                    const float footprintHeight = std::max(1.0f, static_cast<float>(piece.height));
                    const sf::Vector2f center = baseCenter;
                    const float radiusX = 31.0f * pieceScale * footprintWidth;
                    const float radiusY = 12.5f * pieceScale * footprintHeight;
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
            sf::Vector2f anchor = boardFootprintAnchor(
                killedPiece.row,
                killedPiece.column,
                killedPiece.width,
                killedPiece.height,
                gameSnapshot.yourPlayer);
            const sf::Vector2f baseCenter = boardFootprintCenter(
                killedPiece.row,
                killedPiece.column,
                killedPiece.width,
                killedPiece.height,
                gameSnapshot.yourPlayer);
            const sf::Vector2f healthBadgeCenter = boardFootprintHealthBadgeCenter(
                killedPiece.row,
                killedPiece.column,
                killedPiece.width,
                killedPiece.height,
                gameSnapshot.yourPlayer,
                killedPiece.owner);
            const int killedFrameCount = std::max(1, killedPiece.killedAnimFrames);
            const int killedFrame = std::min(
                static_cast<int>(progress * static_cast<float>(killedFrameCount)),
                killedFrameCount - 1);
            sf::Color tint = sf::Color::White;
            tint.a = static_cast<std::uint8_t>(std::clamp(255.0f * (1.0f - progress * 0.35f), 0.0f, 255.0f));
            // The plinth fades out with the piece standing on it.
            drawPieceBase(
                window,
                baseCenter,
                cell.depthScale * (1.0f - progress * 0.2f),
                killedPiece.owner,
                true,
                static_cast<float>(killedPiece.width),
                static_cast<float>(killedPiece.height),
                pieceBaseArtworkFor(killedPiece.owner, killedPiece.width, killedPiece.height),
                rarityGemArtworkFor(killedPiece.name));
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
            drawPieceHealthBadge(
                window,
                healthBadgeCenter,
                cell.depthScale * (1.0f - progress * 0.2f),
                0,
                killedPiece.owner,
                true,
                font,
                killedPiece.isHero);
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
                sf::Vector2f anchor = boardFootprintAnchor(
                    ghostPiece.row,
                    ghostPiece.column,
                    ghostPiece.width,
                    ghostPiece.height,
                    gameSnapshot.yourPlayer);
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
                ? boardFootprintAnchor(effect->row, effect->column, 1, 1, gameSnapshot.yourPlayer)
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
            int footprintWidth = 1;
            int footprintHeight = 1;
            if (enchantment.target == static_cast<std::uint8_t>(game_data::EnchantmentTarget::Piece))
            {
                const game_data::Piece* targetPiece = gamePieceById(enchantment.targetPieceId);
                if (!targetPiece)
                {
                    continue;
                }
                row = targetPiece->row;
                column = targetPiece->column;
                footprintWidth = targetPiece->width;
                footprintHeight = targetPiece->height;
            }
            if (!game_data::inBounds(row, column))
            {
                continue;
            }
            const sf::Vector2f anchor = boardFootprintAnchor(
                row,
                column,
                footprintWidth,
                footprintHeight,
                gameSnapshot.yourPlayer);
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
        const bool mirewatchStory =
            storyMode && storyCampaign == StoryCampaign::Mirewatch;
        const bool seelieStory =
            storyMode && storyCampaign == StoryCampaign::Seelie;
        const std::string activePlayerName = storyMode
            ? (activePlayer == 1
                ? (mirewatchStory
                    ? "Mirewatch Resistance"
                    : seelieStory ? "Seelie Court" : "Blackthorn Company")
                : (mirewatchStory
                    ? "Blackthorn Company"
                    : seelieStory ? "Hostile Force" : "Mirewatch Resistance"))
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
                remainingMs - (ticking && !storyMode ? snapshotAgeMs : 0));
        };
        // ---- Owner banners ---------------------------------------------------
        // The side gutters are deliberately compact. Put the identity and player
        // clock on the first row, then give Resources, Lands, and enchantments
        // separate columns below it so the long Resources caption cannot run into
        // another readout.
        const auto playerDisplayName = [&](int playerNumber) {
            if (storyMode)
            {
                return playerNumber == 1
                    ? (mirewatchStory
                        ? std::string("Mirewatch")
                        : seelieStory ? std::string("Seelie") : std::string("Blackthorn"))
                    : (mirewatchStory
                        ? std::string("Blackthorn")
                        : seelieStory ? std::string("Hostiles") : std::string("Mirewatch"));
            }
            if (sandboxMode)
            {
                return "Player " + std::to_string(playerNumber);
            }
            return playerNumber == me ? loggedInUsername : std::string("Opponent");
        };

        // A labelled figure: a high-contrast caption over a large live number.
        // These are the only always-visible economy/progress readouts in the
        // compact 800x600 story layout, so keep them readable at native size.
        const bool compactGameHud = usesCompactGameHud(window);
        const auto drawBannerFigure = [&](sf::Vector2f center,
                                          const std::string& caption,
                                          const std::string& value,
                                          sf::Color valueColor) {
            const unsigned int captionSize = storyMode
                ? (compactGameHud ? 9 : 10)
                : 8;
            const unsigned int valueSize = storyMode
                ? (compactGameHud ? 17 : 20)
                : (compactGameHud ? 14 : 16);
            sf::Text captionText(font, caption, captionSize);
            if (storyMode)
            {
                captionText.setStyle(sf::Text::Bold);
                captionText.setLetterSpacing(0.94f);
            }
            captionText.setFillColor(
                storyMode ? withAlpha(BoardParchment, 244)
                          : withAlpha(BoardParchmentMuted, 208));
            captionText.setOutlineThickness(storyMode ? 1.0f : 0.0f);
            captionText.setOutlineColor(sf::Color(0, 0, 0, 190));
            centerText(captionText, {center.x, center.y - (compactGameHud ? 7.0f : 9.0f)});
            drawCrispText(window, captionText);

            sf::Text valueText(font, value, valueSize);
            valueText.setStyle(sf::Text::Bold);
            valueText.setFillColor(valueColor);
            valueText.setOutlineThickness(1.0f);
            valueText.setOutlineColor(sf::Color(0, 0, 0, 170));
            centerText(valueText, {center.x, center.y + (compactGameHud ? 7.0f : 9.0f)});
            drawCrispText(window, valueText);
        };

        const auto authoritativeStoryEnemyProgress = [&](const StoryMission& mission) {
            if (!storyEngine)
            {
                return StoryEnemyProgress{};
            }
            return storyDefeatAllEnemiesProgress(
                mission, storyRolePieceIds, storyEngine->boardPieces());
        };
        bool storyEconomyIntroduced = !storyMode;
        if (storyMode && storyMissionIndex >= 0 &&
            storyMissionIndex < static_cast<int>(storyMissions(storyCampaign).size()))
        {
            const auto& campaignMissions = storyMissions(storyCampaign);
            const StoryMission& currentMission =
                campaignMissions[static_cast<std::size_t>(storyMissionIndex)];
            storyEconomyIntroduced = currentMission.script.empty();
            for (int index = 0; index <= storyMissionIndex && !storyEconomyIntroduced; ++index)
            {
                const StoryMission& lesson = campaignMissions[static_cast<std::size_t>(index)];
                storyEconomyIntroduced = std::any_of(
                    lesson.masteryRules.begin(), lesson.masteryRules.end(),
                    [](std::string_view rule) {
                        return rule.find("resource") != std::string::npos ||
                            rule.find("control") != std::string::npos ||
                            rule.find("deployment-cost") != std::string::npos ||
                            rule.find("draw-cost") != std::string::npos;
                    });
            }
        }
        const auto drawPlayerBanner = [&](int playerNumber,
                                          const game_data::PlayerSnapshot& player) {
            const bool leftSide = playerNumber == 1;
            const float x = gamePlayerBannerX(window, playerNumber);
            const float bannerHeight = gamePlayerBannerHeight(window);
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
                    {GamePlayerBannerWidth + 6.0f, bannerHeight + 6.0f},
                    11.0f,
                    withAlpha(accent, 40),
                    withAlpha(accent, 150),
                    1.4f);
            }
            drawBeveledPlate(
                window,
                {x, GameTopBarY},
                {GamePlayerBannerWidth, bannerHeight},
                withAlpha(ownerColorDeep(playerNumber), 236),
                hoveredForEnchantment ? sf::Color(223, 164, 255)
                                      : (isActive ? BoardBrassBright : BoardBrass),
                isActive,
                9.0f);
            if (enchantmentTarget)
            {
                drawCutPlate(
                    {x + 3.0f, GameTopBarY + 3.0f},
                    {GamePlayerBannerWidth - 6.0f, bannerHeight - 6.0f},
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

            // Resources are unbounded only in the free sandbox. Story missions use
            // the ordinary economy and control rules, so those values must stay
            // visible whenever a lesson or open objective asks the player to use them.
            const std::string resources =
                sandboxMode && !storyMode ? std::string("Free") : std::to_string(player.resources);
            const std::string control = std::to_string(player.controlledSquares);
            const std::string storyUnits = std::to_string(std::count_if(
                gameSnapshot.pieces.begin(), gameSnapshot.pieces.end(),
                [&](const game_data::Piece& piece) { return piece.owner == playerNumber; }));

            {
                // Three separate figures remain substantially more legible than
                // two sentence-like lines in the compact HUD. The same visual
                // vocabulary now works at both supported capture sizes.
                const float figureStep = storyMode && compactGameHud ? 60.0f : 58.0f;
                const int figureCount = storyMode
                    ? (storyEconomyIntroduced ? 3 : 1)
                    : (enchantmentCount > 0 ? 3 : 2);
                const float figureBlockX = x +
                    (GamePlayerBannerWidth - figureStep * static_cast<float>(figureCount)) * 0.5f;
                // Compact Story Mode places Exit/Restart directly beneath the
                // friendly banner. Keep the live figures above those buttons;
                // the opponent uses the same row for a stable comparison.
                const float figureY = compactGameHud
                    ? GameTopBarY + 40.0f
                    : GameTopBarY + GamePlayerBannerHeight - 21.0f;
                float slot = 0.0f;
                const auto nextFigureCenter = [&]() {
                    const float center = figureBlockX + figureStep * (slot + 0.5f);
                    slot += 1.0f;
                    return sf::Vector2f{center, figureY};
                };
                if (storyMode)
                {
                    if (storyEconomyIntroduced)
                    {
                        drawBannerFigure(
                            nextFigureCenter(), "RESOURCES", resources, BoardBrassBright);
                        drawBannerFigure(
                            nextFigureCenter(), "CONTROL", control,
                            withAlpha(ownerColorBright(playerNumber), 255));
                    }
                    drawBannerFigure(
                        nextFigureCenter(), "UNITS", storyUnits,
                        withAlpha(ownerColorBright(playerNumber), 255));
                }
                else
                {
                    drawBannerFigure(nextFigureCenter(), "RESOURCES", resources, BoardBrassBright);
                    drawBannerFigure(
                        nextFigureCenter(), "CONTROL", control,
                        withAlpha(ownerColorBright(playerNumber), 255));
                    if (enchantmentCount > 0)
                    {
                        drawBannerFigure(
                            nextFigureCenter(), "HEX", std::to_string(enchantmentCount),
                            sf::Color(206, 162, 240));
                    }
                }
            }

            // Name and player clock share the upper row, away from the sigil.
            const float textLeft = x + (compactGameHud ? 9.0f : 22.0f);
            const float textRight = x + GamePlayerBannerWidth -
                (compactGameHud && gameSnapshot.timersEnabled ? 62.0f
                                                              : compactGameHud ? 9.0f : 22.0f);
            const float textWidth = std::max(30.0f, textRight - textLeft);
            std::string displayName = playerDisplayName(playerNumber);
            if (storyMode && playerNumber == me)
            {
                displayName += " (YOU)";
            }
            sf::Text nameText(
                font,
                elideToWidth(font, displayName, compactGameHud ? 13 : 14, textWidth),
                compactGameHud ? 13 : 14);
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
                const bool clockPaused = storyMode && storyClockPausedForReading &&
                    playerNumber == gameSnapshot.activePlayer;
                sf::Text clockText(
                    font,
                    clockPaused ? std::string("PAUSED") : timerText(clockMs),
                    13);
                clockText.setFillColor(!clockPaused && clockMs <= 30'000
                    ? sf::Color(233, 128, 106)
                    : withAlpha(BoardParchmentMuted, 232));
                clockText.setPosition(compactGameHud
                    ? sf::Vector2f{x + GamePlayerBannerWidth - 55.0f, GameTopBarY + 10.0f}
                    : sf::Vector2f{textLeft, GameTopBarY + 36.0f});
                drawCrispText(window, clockText);
            }
        };

        drawPlayerBanner(1, playerOne);
        drawPlayerBanner(2, playerTwo);

        // Keep a copy of the top-line readout so it can be restored after the
        // foreground sprite pass.  Tall units on the upper board row are
        // deliberately redrawn above the HUD, but the current turn state must
        // remain legible over those silhouettes.
        std::string foregroundTurnLabel;
        unsigned int foregroundTurnLabelSize = 0;
        sf::Vector2f foregroundTurnLabelCenter{};

        // ---- Active turn readout ---------------------------------------------
        {
            const bool myTurn = sandboxMode || activePlayer == me;
            std::string storyTurnLabel = myTurn ? "YOUR TUTORIAL TURN" : "WATCH OPPONENT";
            if (storyMode && myTurn && phase == game_data::Phase::Playing &&
                storyMissionIndex >= 0 &&
                storyMissionIndex < static_cast<int>(storyMissions(storyCampaign).size()))
            {
                const StoryMission& mission =
                    storyMissions(storyCampaign)[static_cast<std::size_t>(storyMissionIndex)];
                if (mission.script.empty())
                {
                    storyTurnLabel = "YOUR TURN";
                }
                const bool automaticPlayerPass =
                    storyMissionStep >= 0 &&
                    storyMissionStep < static_cast<int>(mission.script.size()) &&
                    mission.script[static_cast<std::size_t>(storyMissionStep)].owner == me &&
                    storyScriptActionAutoResolves(
                        mission,
                        mission.script[static_cast<std::size_t>(storyMissionStep)]);
                if (automaticPlayerPass)
                {
                    storyTurnLabel = "AUTO-RESOLVING PASS";
                }
                else if (!mission.autoResolvePlayerEndTurns)
                {
                    int totalPlayerTurns = 0;
                    int currentPlayerTurn = 0;
                    int previousOwner = 0;
                    for (int i = 0; i < static_cast<int>(mission.script.size()); ++i)
                    {
                        const int owner = mission.script[static_cast<std::size_t>(i)].owner;
                        if (owner == me && previousOwner != me)
                        {
                            ++totalPlayerTurns;
                            if (i <= storyMissionStep)
                            {
                                currentPlayerTurn = totalPlayerTurns;
                            }
                        }
                        previousOwner = owner;
                    }
                    if (totalPlayerTurns > 1 && currentPlayerTurn > 0)
                    {
                        storyTurnLabel = "YOUR TURN " + std::to_string(currentPlayerTurn) +
                            " OF " + std::to_string(totalPlayerTurns);
                    }
                }
            }
            const std::string turnLabel = phase == game_data::Phase::GameOver
                ? std::string("MATCH OVER")
                : (phase == game_data::Phase::HeroPlacement
                    ? std::string("DEPLOY YOUR HEROES")
                    : (storyMode ? storyTurnLabel
                                 : (myTurn ? std::string("YOUR TURN")
                                           : std::string("OPPONENT'S TURN"))));
            const int turnPlayer = activePlayer == 2 ? 2 : 1;
            const float turnX = compactGameHud
                ? BoardCenterX - GameTurnPlaqueWidth * 0.5f
                : gamePlayerBannerX(window, turnPlayer);
            const float turnY = compactGameHud ? GameTopBarY : GameTurnPlaqueY;
            const sf::Color accent = phase == game_data::Phase::GameOver
                ? BoardBrass
                : ownerColor(turnPlayer);

            drawBeveledPlate(
                window,
                {turnX, turnY},
                {GameTurnPlaqueWidth, GameTurnPlaqueHeight},
                withAlpha(BoardPlate, 238),
                accent,
                true,
                12.0f);

            const unsigned int turnLabelSize = compactGameHud ? 11 : 10;
            const std::string fittedTurnLabel =
                elideToWidth(font, turnLabel, turnLabelSize, GameTurnPlaqueWidth - 16.0f);
            sf::Text label(font, fittedTurnLabel, turnLabelSize);
            label.setLetterSpacing(1.05f);
            label.setFillColor(BoardParchment);
            label.setOutlineThickness(1.0f);
            label.setOutlineColor(sf::Color(0, 0, 0, 190));
            const bool showTurnClock =
                gameSnapshot.timersEnabled && phase != game_data::Phase::GameOver;
            foregroundTurnLabel = fittedTurnLabel;
            foregroundTurnLabelSize = turnLabelSize;
            foregroundTurnLabelCenter = {
                turnX + GameTurnPlaqueWidth * 0.5f,
                turnY + (showTurnClock ? 14.0f : 24.0f)};
            centerText(label, foregroundTurnLabelCenter);
            drawCrispText(window, label);

            // The turn clock is meaningless once the match is decided, so the
            // plaque drops the drain row rather than freezing a stale figure.
            if (gameSnapshot.timersEnabled && phase != game_data::Phase::GameOver)
            {
                const std::int64_t liveTurnRemainingMs = liveTimer(
                    gameSnapshot.turnRemainingMs,
                    phase == game_data::Phase::Playing ||
                        phase == game_data::Phase::HeroPlacement);
                const bool clockPaused = storyMode && storyClockPausedForReading;
                const bool urgent = !clockPaused && liveTurnRemainingMs <= 30'000;

                // A drain bar beside the figure, so time pressure is felt rather
                // than only read off a clock string.
                sf::Text turnClock(
                    font,
                    clockPaused ? std::string("PAUSED") : timerText(liveTurnRemainingMs),
                    13);
                turnClock.setFillColor(urgent
                    ? sf::Color(240, 152, 132)
                    : withAlpha(BoardParchmentMuted, 240));
                const float trackX = turnX + 10.0f;
                const float trackWidth = GameTurnPlaqueWidth - 20.0f;
                const float rowY = turnY + 41.0f;

                centerText(turnClock, {turnX + GameTurnPlaqueWidth * 0.5f, turnY + 29.0f});
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
        // bases, health badges, crowns, and control badges so those details retain
        // normal board depth and cannot cover a piece in front of them.
        drawPieceLayer(true);

        if (!foregroundTurnLabel.empty())
        {
            sf::Text foregroundLabel(
                font, foregroundTurnLabel, foregroundTurnLabelSize);
            foregroundLabel.setLetterSpacing(1.05f);
            foregroundLabel.setFillColor(BoardParchment);
            foregroundLabel.setOutlineThickness(1.0f);
            foregroundLabel.setOutlineColor(sf::Color(0, 0, 0, 190));
            centerText(foregroundLabel, foregroundTurnLabelCenter);
            drawCrispText(window, foregroundLabel);
        }

        // Foreground sprites are deliberately redrawn above the HUD to keep
        // their silhouettes intact. Draw named survivor labels after that pass
        // so a nearer unit can never hide who must survive; the gold base ring
        // remains below the art as a second, shape-based cue.
        std::vector<sf::FloatRect> storyStatusRects;
        if (storyMode && !compactGameHud)
        {
            // The wide layout places the player rail beside and slightly over
            // the board's left edge. Reserve the complete rail plus the opposing
            // readout and turn plaque before placing any Story overlay.
            storyStatusRects.emplace_back(
                sf::Vector2f{GamePlayerBannerLeftX, GameTopBarY},
                sf::Vector2f{GamePlayerBannerWidth, GameBottomBarY - GameTopBarY});
            storyStatusRects.emplace_back(
                sf::Vector2f{GamePlayerBannerRightX, GameTopBarY},
                sf::Vector2f{GamePlayerBannerWidth, GamePlayerBannerHeight});
            storyStatusRects.emplace_back(
                sf::Vector2f{
                    BoardCenterX - GameTurnPlaqueWidth * 0.5f,
                    GameTurnPlaqueY},
                sf::Vector2f{GameTurnPlaqueWidth, GameTurnPlaqueHeight});
        }
        for (int survivorId : requiredStorySurvivorIds)
        {
            const auto survivor = std::find_if(
                gameSnapshot.pieces.begin(), gameSnapshot.pieces.end(),
                [&](const game_data::Piece& piece) { return piece.id == survivorId; });
            if (survivor == gameSnapshot.pieces.end())
            {
                continue;
            }
            const sf::Vector2f baseCenter = boardFootprintCenter(
                survivor->row,
                survivor->column,
                survivor->width,
                survivor->height,
                gameSnapshot.yourPlayer);
            const float pieceScale = boardCellMetrics(
                survivor->row, survivor->column).depthScale;
            const unsigned int survivorNameSize = compactGameHud ? 11u : 10u;
            sf::Text survivorNameMeasure(font, survivor->name, survivorNameSize);
            const sf::Vector2f protectSize{
                std::clamp(
                    survivorNameMeasure.getLocalBounds().size.x + 18.0f,
                    108.0f,
                    158.0f),
                34.0f};
            constexpr float storyBadgeSafeLeft = 18.0f;
            constexpr float storyBadgeSafeRight = 782.0f;
            constexpr float storyBadgeSafeTop = 68.0f;
            constexpr float storyBadgeSafeBottom = GameBottomBarY - 3.0f;
            constexpr float storyBadgeClearance = 6.0f;
            static_assert(
                storyBadgeSafeRight - storyBadgeSafeLeft >= 158.0f &&
                    storyBadgeSafeBottom - storyBadgeSafeTop >= 34.0f,
                "Story badge safe bounds must fit the largest survivor badge.");
            const float aboveY = baseCenter.y - 27.0f * pieceScale;
            // A far-rank unit can sit directly under the top HUD. Put its
            // status below the anchor instead of clipping the label into that
            // rail, and keep every badge inside the board-safe overlay band.
            const float preferredY = aboveY < 74.0f
                ? baseCenter.y + 17.0f * pieceScale
                : aboveY;
            const sf::Vector2f protectBasePosition{
                std::clamp(
                    baseCenter.x - protectSize.x * 0.5f,
                    storyBadgeSafeLeft,
                    storyBadgeSafeRight - protectSize.x),
                std::clamp(
                    preferredY,
                    storyBadgeSafeTop,
                    storyBadgeSafeBottom - protectSize.y)};
            const auto overlapsStatus = [&](const sf::Vector2f& candidate) {
                return std::any_of(
                    storyStatusRects.begin(),
                    storyStatusRects.end(),
                    [&](const sf::FloatRect& occupied) {
                        return candidate.x <
                                occupied.position.x + occupied.size.x +
                                    storyBadgeClearance &&
                            candidate.x + protectSize.x >
                                occupied.position.x - storyBadgeClearance &&
                            candidate.y <
                                occupied.position.y + occupied.size.y +
                                    storyBadgeClearance &&
                            candidate.y + protectSize.y >
                                occupied.position.y - storyBadgeClearance;
                    });
            };
            const auto insideBadgeSafeBounds = [&](const sf::Vector2f& candidate) {
                return candidate.x >= storyBadgeSafeLeft &&
                    candidate.x + protectSize.x <= storyBadgeSafeRight &&
                    candidate.y >= storyBadgeSafeTop &&
                    candidate.y + protectSize.y <= storyBadgeSafeBottom;
            };
            // Search both axes together. Whole-badge lane spacing handles dense
            // survivor clusters, while obstacle-edge coordinates cover the
            // remaining legal strips without resorting to a coarse pixel grid.
            std::vector<float> protectCandidateXs;
            std::vector<float> protectCandidateYs;
            const auto appendCandidate = [](std::vector<float>& candidates,
                                            float value,
                                            float minimum,
                                            float maximum) {
                const float clamped = std::clamp(value, minimum, maximum);
                const bool duplicate = std::any_of(
                    candidates.begin(), candidates.end(),
                    [&](float existing) {
                        return std::abs(existing - clamped) < 0.01f;
                    });
                if (!duplicate)
                {
                    candidates.push_back(clamped);
                }
            };
            const float maximumBadgeX = storyBadgeSafeRight - protectSize.x;
            const float maximumBadgeY = storyBadgeSafeBottom - protectSize.y;
            appendCandidate(
                protectCandidateXs,
                protectBasePosition.x,
                storyBadgeSafeLeft,
                maximumBadgeX);
            appendCandidate(
                protectCandidateYs,
                protectBasePosition.y,
                storyBadgeSafeTop,
                maximumBadgeY);

            const float horizontalLane = protectSize.x + storyBadgeClearance;
            const float verticalLane = protectSize.y + storyBadgeClearance;
            const int horizontalLaneCount = static_cast<int>(std::ceil(
                (maximumBadgeX - storyBadgeSafeLeft) / horizontalLane));
            const int verticalLaneCount = static_cast<int>(std::ceil(
                (maximumBadgeY - storyBadgeSafeTop) / verticalLane));
            for (int lane = 1; lane <= horizontalLaneCount; ++lane)
            {
                const float adjustment = static_cast<float>(lane) * horizontalLane;
                appendCandidate(
                    protectCandidateXs,
                    protectBasePosition.x + adjustment,
                    storyBadgeSafeLeft,
                    maximumBadgeX);
                appendCandidate(
                    protectCandidateXs,
                    protectBasePosition.x - adjustment,
                    storyBadgeSafeLeft,
                    maximumBadgeX);
            }
            for (int lane = 1; lane <= verticalLaneCount; ++lane)
            {
                const float adjustment = static_cast<float>(lane) * verticalLane;
                appendCandidate(
                    protectCandidateYs,
                    protectBasePosition.y + adjustment,
                    storyBadgeSafeTop,
                    maximumBadgeY);
                appendCandidate(
                    protectCandidateYs,
                    protectBasePosition.y - adjustment,
                    storyBadgeSafeTop,
                    maximumBadgeY);
            }
            appendCandidate(
                protectCandidateXs,
                storyBadgeSafeLeft,
                storyBadgeSafeLeft,
                maximumBadgeX);
            appendCandidate(
                protectCandidateXs,
                maximumBadgeX,
                storyBadgeSafeLeft,
                maximumBadgeX);
            appendCandidate(
                protectCandidateYs,
                storyBadgeSafeTop,
                storyBadgeSafeTop,
                maximumBadgeY);
            appendCandidate(
                protectCandidateYs,
                maximumBadgeY,
                storyBadgeSafeTop,
                maximumBadgeY);
            for (const sf::FloatRect& occupied : storyStatusRects)
            {
                appendCandidate(
                    protectCandidateXs,
                    occupied.position.x - protectSize.x - storyBadgeClearance,
                    storyBadgeSafeLeft,
                    maximumBadgeX);
                appendCandidate(
                    protectCandidateXs,
                    occupied.position.x + occupied.size.x + storyBadgeClearance,
                    storyBadgeSafeLeft,
                    maximumBadgeX);
                appendCandidate(
                    protectCandidateYs,
                    occupied.position.y - protectSize.y - storyBadgeClearance,
                    storyBadgeSafeTop,
                    maximumBadgeY);
                appendCandidate(
                    protectCandidateYs,
                    occupied.position.y + occupied.size.y + storyBadgeClearance,
                    storyBadgeSafeTop,
                    maximumBadgeY);
            }

            sf::Vector2f protectPosition = protectBasePosition;
            bool foundProtectPosition = false;
            float bestDistanceSquared = std::numeric_limits<float>::max();
            for (float candidateX : protectCandidateXs)
            {
                for (float candidateY : protectCandidateYs)
                {
                    const sf::Vector2f candidate{candidateX, candidateY};
                    if (!insideBadgeSafeBounds(candidate) || overlapsStatus(candidate))
                    {
                        continue;
                    }
                    const float deltaX = candidate.x - protectBasePosition.x;
                    const float deltaY = candidate.y - protectBasePosition.y;
                    const float distanceSquared =
                        deltaX * deltaX + deltaY * deltaY;
                    if (!foundProtectPosition || distanceSquared < bestDistanceSquared)
                    {
                        protectPosition = candidate;
                        bestDistanceSquared = distanceSquared;
                        foundProtectPosition = true;
                    }
                }
            }
            if (captureRequest &&
                (!foundProtectPosition || overlapsStatus(protectPosition)))
            {
                failCaptureValidation(
                    "Capture layout error: required-survivor badges overlap.");
            }
            if (captureRequest && !insideBadgeSafeBounds(protectPosition))
            {
                failCaptureValidation(
                    "Capture layout error: required-survivor badge exceeds safe bounds.");
            }
            drawBeveledPlate(
                window,
                protectPosition,
                protectSize,
                sf::Color(35, 31, 18, 242),
                sf::Color(255, 214, 104),
                false,
                4.0f);
            drawCenteredText(
                window,
                font,
                elideToWidth(
                    font,
                    survivor->name,
                    survivorNameSize,
                    protectSize.x - 12.0f),
                survivorNameSize,
                {protectPosition.x + protectSize.x * 0.5f,
                 protectPosition.y + 9.0f},
                sf::Color(255, 246, 222));
            drawCenteredText(
                window,
                font,
                "MUST SURVIVE",
                compactGameHud ? 10 : 9,
                {protectPosition.x + protectSize.x * 0.5f,
                 protectPosition.y + 24.0f},
                sf::Color(255, 230, 151));
            storyStatusRects.emplace_back(protectPosition, protectSize);
        }

        // Keep the chapter identity visible outside active play. During an
        // objective the briefing and bottom instruction plaque already supply
        // that context; omitting this overlay leaves the far rank fully visible
        // and clickable instead of covering tall unit art.
        if (storyMode && !compactGameHud && storyMissionIndex >= 0 &&
            storyMissionIndex < static_cast<int>(storyMissions(storyCampaign).size()) &&
            storyStage != StoryStage::Objective)
        {
            const StoryMission& mission =
                storyMissions(storyCampaign)[static_cast<std::size_t>(storyMissionIndex)];
            const std::string actName(storyActName(storyCampaign, storyMissionIndex));
            const std::string shortActGoal(
                storyActShortGoal(storyCampaign, storyMissionIndex));
            const bool showActOrientation = !actName.empty() && !shortActGoal.empty();
            const sf::Vector2f missionPlateSize{
                410.0f, showActOrientation ? 58.0f : 44.0f};
            const sf::Vector2f missionPlatePosition{
                BoardCenterX - missionPlateSize.x * 0.5f,
                showActOrientation ? 4.0f : 7.0f};
            drawBeveledPlate(
                window,
                missionPlatePosition,
                missionPlateSize,
                sf::Color(12, 20, 21, 244),
                withAlpha(ownerColorBright(me), 210),
                false,
                8.0f);
            if (showActOrientation)
            {
                drawCenteredText(
                    window,
                    font,
                    actName,
                    11,
                    {BoardCenterX, missionPlatePosition.y + 9.0f},
                    sf::Color(236, 204, 132));
                drawCenteredText(
                    window,
                    font,
                    "GOAL: " + shortActGoal,
                    11,
                    {BoardCenterX, missionPlatePosition.y + 22.0f},
                    sf::Color(222, 226, 218));
            }
            drawCenteredText(
                window,
                font,
                "ENTRY " + std::to_string(storyMissionIndex + 1) + " OF " +
                    std::to_string(storyMissions(storyCampaign).size()) +
                    "  -  PLAYABLE MISSION",
                compactGameHud ? 12 : 9,
                {BoardCenterX,
                 missionPlatePosition.y + (showActOrientation ? 35.0f : 11.0f)},
                withAlpha(ownerColorBright(me), 245));
            sf::Text missionTitle(
                gloomthornFontLoaded ? gloomthornFont : font,
                elideToWidth(
                    gloomthornFontLoaded ? gloomthornFont : font,
                    std::string(mission.title),
                    showActOrientation ? 14 : (compactGameHud ? 17 : 16),
                    missionPlateSize.x - 28.0f),
                showActOrientation ? 14 : (compactGameHud ? 17 : 16));
            missionTitle.setFillColor(BoardParchment);
            centerText(
                missionTitle,
                {BoardCenterX,
                 missionPlatePosition.y + (showActOrientation ? 50.0f : 29.0f)});
            drawCrispText(window, missionTitle);
        }

        // Tutorial coordinates are an input aid. Paint them over the board and
        // pieces first; the opaque guided-action tags below then remain intact
        // when a tag must share the top or left coordinate band.
        if (storyMode)
        {
            const auto drawCoordinateLabel = [&](const std::string& label, sf::Vector2f center) {
                sf::Text coordinate(font, label, 12);
                coordinate.setStyle(sf::Text::Bold);
                coordinate.setFillColor(BoardParchment);
                coordinate.setOutlineThickness(1.5f);
                coordinate.setOutlineColor(sf::Color(0, 0, 0, 230));
                centerText(coordinate, center);
                drawCrispText(window, coordinate);
            };
            for (int column = 0; column < game_data::BoardSize; ++column)
            {
                const sf::Vector2f from = boardEdgePoint(0, column);
                const sf::Vector2f to = boardEdgePoint(0, column + 1);
                drawCoordinateLabel(
                    std::string(1, static_cast<char>('A' + column)),
                    {(from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f + 10.0f});
            }
            for (int screenRow = 0; screenRow < game_data::BoardSize; ++screenRow)
            {
                const sf::Vector2f from = boardEdgePoint(screenRow, 0);
                const sf::Vector2f to = boardEdgePoint(screenRow + 1, 0);
                drawCoordinateLabel(
                    std::to_string(
                        rowForScreenRow(screenRow, gameSnapshot.yourPlayer) + 1),
                    {(from.x + to.x) * 0.5f - 13.0f, (from.y + to.y) * 0.5f});
            }
        }

        // During guided steps, label the required piece and target directly on
        // the board. A pulsing ring alone cannot tell a new player whether it
        // means actor, destination, danger, or selection.
        if (storyMode && storyStage == StoryStage::Objective)
        {
            // Guided actor/target callouts share collision space with survivor
            // badges so an adjacent destination can never cover a status tag.
            std::vector<sf::FloatRect> storyTagRects = storyStatusRects;
            const auto storyPiece = [&](int id) -> const game_data::Piece* {
                const auto found = std::find_if(
                    gameSnapshot.pieces.begin(), gameSnapshot.pieces.end(),
                    [&](const game_data::Piece& piece) { return piece.id == id; });
                return found == gameSnapshot.pieces.end() ? nullptr : &*found;
            };
            const auto drawStoryTagAt = [&](const sf::Vector2f center, const std::string& label,
                                             sf::Color accent, float verticalOffset) {
                const unsigned int tagSize = compactGameHud ? 12 : 11;
                const float tagTextWidth = compactGameHud ? 214.0f : 198.0f;
                std::vector<std::string> tagLines;
                sf::Text fullMeasure(font, label, tagSize);
                const std::size_t separator = label.find(": ");
                if (fullMeasure.getLocalBounds().size.x > tagTextWidth &&
                    separator != std::string::npos)
                {
                    tagLines.push_back(label.substr(0, separator));
                    tagLines.push_back(label.substr(separator + 2));
                }
                else
                {
                    tagLines.push_back(label);
                }
                for (std::string& line : tagLines)
                {
                    line = elideToWidth(font, line, tagSize, tagTextWidth);
                }
                float widestLine = 0.0f;
                for (const std::string& line : tagLines)
                {
                    sf::Text measure(font, line, tagSize);
                    widestLine = std::max(
                        widestLine, measure.getLocalBounds().size.x);
                }
                const float width = std::clamp(
                    widestLine + 18.0f,
                    62.0f,
                    compactGameHud ? 230.0f : 214.0f);
                const float tagHeight = tagLines.size() > 1 ? 36.0f : 22.0f;
                const float boardTagLeft = BoardOriginX;
                const float boardTagRight = BoardOriginX + BoardBottomWidth;
                const sf::Vector2f basePosition{
                    std::clamp(
                        center.x - width * 0.5f,
                        boardTagLeft,
                        boardTagRight - width),
                    std::clamp(
                        center.y + verticalOffset,
                        BoardOriginY + 2.0f,
                        GameBottomBarY - tagHeight - 3.0f)};
                const auto overlapsExistingTag = [&](sf::Vector2f candidate) {
                    return std::any_of(
                        storyTagRects.begin(), storyTagRects.end(),
                        [&](const sf::FloatRect& occupied) {
                            return candidate.x < occupied.position.x + occupied.size.x &&
                                candidate.x + width > occupied.position.x &&
                                candidate.y < occupied.position.y + occupied.size.y &&
                                candidate.y + tagHeight > occupied.position.y;
                        });
                };
                sf::Vector2f position = basePosition;
                bool foundOpenLane = false;
                const std::array<float, 5> horizontalAdjustments{
                    0.0f,
                    width + 8.0f,
                    -width - 8.0f,
                    width * 2.0f + 16.0f,
                    -width * 2.0f - 16.0f};
                const std::array<float, 9> verticalAdjustments{
                    0.0f, -28.0f, 28.0f, -56.0f, 56.0f,
                    -84.0f, 84.0f, -112.0f, 112.0f};
                for (float horizontalAdjustment : horizontalAdjustments)
                {
                    for (float verticalAdjustment : verticalAdjustments)
                    {
                        const sf::Vector2f candidate{
                            std::clamp(
                                basePosition.x + horizontalAdjustment,
                                boardTagLeft,
                                boardTagRight - width),
                            std::clamp(
                                basePosition.y + verticalAdjustment,
                                BoardOriginY + 2.0f,
                                GameBottomBarY - tagHeight - 3.0f)};
                        if (!overlapsExistingTag(candidate))
                        {
                            position = candidate;
                            foundOpenLane = true;
                            break;
                        }
                    }
                    if (foundOpenLane)
                    {
                        break;
                    }
                }
                if (captureRequest && overlapsExistingTag(position))
                {
                    failCaptureValidation(
                        "Capture layout error: guided Story callouts overlap.");
                }
                const sf::Vector2f tagCenter{
                    position.x + width * 0.5f,
                    position.y + tagHeight * 0.5f};
                if (std::abs(tagCenter.x - center.x) > width * 0.55f ||
                    std::abs(tagCenter.y - center.y) > 70.0f)
                {
                    drawEdgeLine(
                        window,
                        center,
                        tagCenter,
                        1.2f,
                        withAlpha(accent, 150));
                }
                drawCutPlate(
                    position, {width, tagHeight}, 5.0f,
                    sf::Color(9, 14, 15, 244), withAlpha(accent, 244), 1.4f);
                for (std::size_t lineIndex = 0;
                     lineIndex < tagLines.size(); ++lineIndex)
                {
                    sf::Text text(font, tagLines[lineIndex], tagSize);
                    text.setFillColor(
                        lineIndex == 0
                            ? sf::Color(255, 246, 222)
                            : withAlpha(accent, 248));
                    centerText(
                        text,
                        {position.x + width * 0.5f,
                         position.y +
                             (tagLines.size() > 1
                                 ? 9.0f + static_cast<float>(lineIndex) * 17.0f
                                 : 11.0f)});
                    drawCrispText(window, text);
                }
                storyTagRects.push_back(
                    sf::FloatRect(position, {width, tagHeight}));
            };
            const auto drawStoryTag = [&](int id, std::string_view role, sf::Color accent,
                                           float verticalOffset) {
                const game_data::Piece* piece = storyPiece(id);
                if (!piece)
                {
                    return;
                }
                const sf::Vector2f center = boardFootprintCenter(
                    piece->row, piece->column, piece->width, piece->height,
                    gameSnapshot.yourPlayer);
                drawStoryTagAt(
                    center,
                    std::string(role) + ": " + piece->name,
                    accent,
                    verticalOffset);
            };

            if (requiredStoryTargetId == 0 &&
                game_data::inBounds(storyTargetRow, storyTargetColumn))
            {
                std::string squareName(
                    1, static_cast<char>('A' + storyTargetColumn));
                squareName += std::to_string(storyTargetRow + 1);
                drawStoryTagAt(
                    boardCellAnchor(boardCellMetrics(storyTargetRow, storyTargetColumn)),
                    "TARGET: " + squareName,
                    sf::Color(126, 214, 178),
                    -28.0f);
            }

            if (requiredStoryTargetId != 0 && requiredStoryTargetId != requiredStoryActorId)
            {
                drawStoryTag(
                    requiredStoryTargetId, "TARGET", sf::Color(238, 116, 94), -18.0f);
            }
            if (requiredStoryActorId != 0)
            {
                drawStoryTag(
                    requiredStoryActorId, "ACT", sf::Color(255, 214, 104), -64.0f);
            }
        }

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

            const bool statusRibbon =
                !storyMode && title.empty() && !gameSnapshot.status.empty();
            if (statusRibbon)
            {
                title = "STATUS";
                instruction = gameSnapshot.status;
                const std::string normalizedStatus = game_data::normalizedAbility(instruction);
                if (normalizedStatus.find("cannot") != std::string::npos ||
                    normalizedStatus.find("must") != std::string::npos ||
                    normalizedStatus.find("need") != std::string::npos ||
                    normalizedStatus.find("invalid") != std::string::npos ||
                    normalizedStatus.find("not enough") != std::string::npos ||
                    normalizedStatus.find("unavailable") != std::string::npos)
                {
                    accent = sf::Color(238, 116, 94);
                }
            }
            if (title.empty())
            {
                return;
            }

            const sf::Vector2f ribbonSize = statusRibbon
                ? sf::Vector2f{620.0f, 20.0f}
                : sf::Vector2f{420.0f, 17.0f};
            const sf::Vector2f ribbonPosition{
                BoardCenterX - ribbonSize.x * 0.5f,
                statusRibbon ? 446.0f : 449.0f};
            drawCutPlate(
                ribbonPosition,
                ribbonSize,
                6.0f,
                sf::Color(7, 13, 14, 238),
                withAlpha(accent, 210),
                1.2f);
            const float titleWidth = statusRibbon ? 54.0f : 116.0f;
            sf::Text titleText(font, elideToWidth(font, title, 9, titleWidth), 9);
            titleText.setFillColor(BoardParchment);
            titleText.setPosition({ribbonPosition.x + 11.0f, ribbonPosition.y + 4.0f});
            drawCrispText(window, titleText);
            const float separatorX = ribbonPosition.x + (statusRibbon ? 66.0f : 136.0f);
            drawEdgeLine(
                window,
                {separatorX, ribbonPosition.y + 4.0f},
                {separatorX, ribbonPosition.y + ribbonSize.y - 4.0f},
                1.0f,
                withAlpha(accent, 180));
            const float instructionWidth =
                ribbonSize.x - (separatorX - ribbonPosition.x) - 20.0f;
            sf::Text instructionText(
                font,
                elideToWidth(font, instruction, 8, instructionWidth),
                8);
            instructionText.setLetterSpacing(1.08f);
            instructionText.setFillColor(withAlpha(accent, 246));
            instructionText.setPosition({separatorX + 11.0f, ribbonPosition.y + 5.0f});
            drawCrispText(window, instructionText);
        };
        drawCommandIntent();

        // Story Mode teaches the same deck, draw, and discard actions as an
        // ordinary match. Keep their real controls visible; the authored-step
        // validator still rejects actions that are not the current lesson.
        const bool commandBarActive = phase == game_data::Phase::Playing;
        if (commandBarActive)
        {
            const bool canDraw = playerCanDrawCard();
            std::string drawCaption = "WAIT";
            if (mine.drawPileCount <= 0)
            {
                drawCaption = "EMPTY";
            }
            else if (static_cast<int>(gameSnapshot.hand.size()) >= game_data::MaxHandSize)
            {
                drawCaption = "HAND FULL";
            }
            else if (!gameSnapshot.foresightChoices.empty())
            {
                drawCaption = "CHOOSE CARD";
            }
            else if (gameSnapshot.activePlayer == me &&
                     mine.resources < game_data::DrawCardResourceCost)
            {
                drawCaption = "NEED 50";
            }
            else if (canDraw)
            {
                drawCaption = "CLICK DECK";
            }
            drawDrawPile(
                mine.drawPileCount,
                canDraw,
                isDrawPileAtPixel(currentPointer),
                drawCaption);

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
            bool endTurnAvailable = true;
            bool automaticStoryStep = false;
            if (storyMode)
            {
                const StoryMission& mission =
                    storyMissions(storyCampaign)[static_cast<std::size_t>(storyMissionIndex)];
                endTurnAvailable = storyStage != StoryStage::Objective ||
                    (mission.script.empty() && storyPopupPanels.empty());
                if (!mission.script.empty() &&
                    storyStage == StoryStage::Objective && storyPopupPanels.empty() &&
                    storyMissionStep >= 0 &&
                    storyMissionStep < static_cast<int>(mission.script.size()))
                {
                    const StoryScriptAction& step =
                        mission.script[static_cast<std::size_t>(storyMissionStep)];
                    automaticStoryStep =
                        storyScriptActionAutoResolves(mission, step);
                    endTurnAvailable =
                        storyScriptActionRequiresPlayerInput(mission, step) &&
                        step.kind == StoryActionKind::EndTurn;
                }
                endTurnButton.setEnabled(endTurnAvailable);
                if (storyStage == StoryStage::Complete)
                {
                    endTurnButton.setLabel(
                        storyMissionIndex + 1 <
                                static_cast<int>(storyMissions(storyCampaign).size())
                            ? "Continue Story"
                            : "Finish Story");
                }
                else if (storyStage == StoryStage::Failed)
                {
                    endTurnButton.setLabel("Retry Mission");
                }
                else
                {
                    endTurnButton.setLabel(
                        automaticStoryStep
                            ? "Auto Resolving"
                            : endTurnAvailable ? "End Turn" : "Finish Step First");
                }
            }
            else
            {
                endTurnButton.setEnabled(true);
            }

            // Warm bloom behind the primary action, so ending the turn outranks
            // leaving the match instead of the two reading as equal peers.
            if (!storyMode || endTurnAvailable)
            {
                drawCutPlate(
                    {GameActionButtonX - 4.0f, GameActionButtonY - 4.0f},
                    {GameActionButtonWidth + 8.0f, GamePrimaryButtonHeight + 8.0f},
                    11.0f,
                    withAlpha(BoardBrassBright, 34),
                    withAlpha(BoardBrassBright, 128),
                    1.4f);
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
            storyRestartButton.draw(window);
            if (!compactGameHud)
            {
                const StoryMission& mission =
                    storyMissions(storyCampaign)[static_cast<std::size_t>(storyMissionIndex)];
                const std::string survivorNames = storyRequiredSurvivorNames(mission);
                const sf::Vector2f goalPosition{GamePlayerBannerLeftX + 12.0f, 238.0f};
                const sf::Vector2f goalSize{156.0f, 218.0f};
                drawBeveledPlate(
                    window,
                    goalPosition,
                    goalSize,
                    withAlpha(BoardPlate, 242),
                    withAlpha(ownerColorBright(me), 190),
                    false,
                    8.0f);
                drawText(
                    window,
                    font,
                    "MISSION GOAL",
                    12,
                    goalPosition + sf::Vector2f(12.0f, 12.0f),
                    BoardBrassBright,
                    goalSize.x - 24.0f);
                drawSeparatorRule(
                    window,
                    goalPosition + sf::Vector2f(12.0f, 34.0f),
                    goalSize.x - 24.0f,
                    false);
                const int totalGuidedSteps =
                    storyGuidedPlayerActionCount(mission);
                int completedGuidedSteps = 0;
                for (std::size_t index = 0; index < mission.script.size(); ++index)
                {
                    if (!storyScriptActionRequiresPlayerInput(
                            mission, mission.script[index]))
                    {
                        continue;
                    }
                    if (static_cast<int>(index) < storyMissionStep)
                    {
                        ++completedGuidedSteps;
                    }
                }
                std::string objectiveProgress;
                if (mission.standardMatch)
                {
                    if (static_cast<game_data::Phase>(gameSnapshot.phase) ==
                        game_data::Phase::HeroPlacement)
                    {
                        objectiveProgress = "PLACE HEROES - " +
                            std::to_string(mine.heroesToPlace) + " LEFT";
                    }
                    else
                    {
                        const auto& opponent = gameSnapshot.players[
                            static_cast<std::size_t>((me == 1 ? 2 : 1) - 1)];
                        objectiveProgress = "ENEMY HEROES " +
                            std::to_string(opponent.heroesAlive);
                    }
                }
                else if (!mission.script.empty())
                {
                    objectiveProgress = "DONE " + std::to_string(completedGuidedSteps) +
                        " OF " + std::to_string(totalGuidedSteps);
                }
                else if (mission.objectiveSpec.kind == StoryObjectiveKind::ControlSquares)
                {
                    objectiveProgress = "CONTROL " +
                        std::to_string(mine.controlledSquares) + " OF " +
                        std::to_string(mission.objectiveSpec.amount);
                }
                else if (mission.objectiveSpec.kind == StoryObjectiveKind::DefeatAllEnemies)
                {
                    objectiveProgress = "ENEMIES LEFT " + std::to_string(
                        authoritativeStoryEnemyProgress(mission).remaining);
                }
                else
                {
                    objectiveProgress = "OPEN OBJECTIVE - ACTIVE";
                }
                drawText(
                    window,
                    font,
                    objectiveProgress,
                    10,
                    goalPosition + sf::Vector2f(12.0f, 43.0f),
                    withAlpha(ownerColorBright(me), 235),
                    goalSize.x - 24.0f);
                float objectiveY = goalPosition.y + 64.0f;
                if (!survivorNames.empty())
                {
                    objectiveY = drawWrappedText(
                        window,
                        font,
                        "MUST SURVIVE: " + survivorNames,
                        9,
                        {goalPosition.x + 12.0f, objectiveY},
                        sf::Color(255, 214, 104),
                        goalSize.x - 24.0f,
                        2.0f) + 4.0f;
                }
                const unsigned int objectiveTextSize =
                    survivorNames.empty() ? 12u : 11u;
                constexpr float objectiveLineGap = 3.0f;
                const float objectiveTextWidth = goalSize.x - 24.0f;
                const float goalTextLimit =
                    goalPosition.y + goalSize.y - 8.0f;
                std::string sideObjective(mission.objective);
                const auto objectiveFits = [&](const std::string& text) {
                    return objectiveY +
                            static_cast<float>(wrapText(
                                font, text, objectiveTextSize,
                                objectiveTextWidth).size()) *
                                (static_cast<float>(objectiveTextSize) +
                                 objectiveLineGap) <=
                        goalTextLimit;
                };
                if (!objectiveFits(sideObjective))
                {
                    if (mission.standardMatch)
                    {
                        sideObjective =
                            static_cast<game_data::Phase>(gameSnapshot.phase) ==
                                game_data::Phase::HeroPlacement
                            ? "Place every Hero."
                            : "Win the ordinary match.";
                    }
                    else if (!mission.script.empty())
                    {
                        sideObjective = "Complete every required action.";
                    }
                    else
                    {
                        switch (mission.objectiveSpec.kind)
                        {
                        case StoryObjectiveKind::DefeatAllEnemies:
                            sideObjective = "Defeat every enemy.";
                            break;
                        case StoryObjectiveKind::DefeatRole:
                            sideObjective = "Defeat the marked enemy.";
                            break;
                        case StoryObjectiveKind::ReachSquare:
                            sideObjective = "Reach the glowing square.";
                            break;
                        case StoryObjectiveKind::DeployCard:
                            sideObjective = "Deploy the required card.";
                            break;
                        case StoryObjectiveKind::ControlSquares:
                            sideObjective = "Control " +
                                std::to_string(mission.objectiveSpec.amount) +
                                " squares.";
                            break;
                        case StoryObjectiveKind::Legacy:
                        case StoryObjectiveKind::StoryOnly:
                        case StoryObjectiveKind::Scripted:
                            sideObjective = "Complete the mission.";
                            break;
                        }
                    }
                }
                const float objectiveBottom = drawWrappedText(
                    window,
                    font,
                    sideObjective,
                    objectiveTextSize,
                    {goalPosition.x + 12.0f, objectiveY},
                    BoardParchmentMuted,
                    objectiveTextWidth,
                    objectiveLineGap);
                if (captureRequest &&
                    objectiveBottom > goalTextLimit)
                {
                    failCaptureValidation(
                        "Capture layout error: mission goal text exceeds its panel.");
                }
            }
        }

        if (storyMode)
        {
            const StoryMission& mission =
                storyMissions(storyCampaign)[static_cast<std::size_t>(storyMissionIndex)];
            const bool openObjective = mission.script.empty() &&
                storyStage == StoryStage::Objective;
            const auto controlInstructions = [](int controlled, int required) {
                return "Control " + std::to_string(controlled) + "/" +
                    std::to_string(required) +
                    ". Teal is yours; red is enemy. Visible units own their square "
                    "and influence all 8 neighbors. Higher influence wins an empty "
                    "square; a tie keeps its owner. Move or defeat units to change it.";
            };
            std::string stepHeading = "MISSION GOAL";
            std::string stepBody = std::string(mission.objective);
            if (openObjective && mission.standardMatch)
            {
                if (static_cast<game_data::Phase>(gameSnapshot.phase) ==
                    game_data::Phase::HeroPlacement)
                {
                stepHeading = "PLACE YOUR HEROES";
                stepBody =
                        "Select every Hero and place it inside your highlighted two-by-four home zone before the one-shot two-minute placement deadline. Each footprint must land on empty squares; placements do not reset this timer.";
                }
                else
                {
                    const auto& opponent = gameSnapshot.players[
                        static_cast<std::size_t>((me == 1 ? 2 : 1) - 1)];
                    stepHeading = "WIN THE ORDINARY MATCH";
                    stepBody = "Destroy all " +
                        std::to_string(opponent.heroesAlive) +
                        " remaining enemy Heroes, or win if the opponent's 15-minute match clock expires. You lose if your Heroes fall or your own clock reaches zero.";
                }
            }
            else if (openObjective &&
                mission.objectiveSpec.kind == StoryObjectiveKind::ControlSquares)
            {
                stepHeading = "TAKE CONTROL";
                stepBody = controlInstructions(
                    gameSnapshot.players[static_cast<std::size_t>(me - 1)]
                        .controlledSquares,
                    mission.objectiveSpec.amount);
            }
            else if (openObjective &&
                     mission.objectiveSpec.kind == StoryObjectiveKind::DefeatAllEnemies)
            {
                stepHeading = "CHOOSE A UNIT";
                stepBody = "Inspect a unit (double-click or focus + I), then choose one printed action. " +
                    std::to_string(authoritativeStoryEnemyProgress(mission).remaining) +
                    " enemies remain.";
                const std::string required = storyRequiredSurvivorNames(mission);
                if (!required.empty())
                {
                    stepBody += " Keep " + required + " alive.";
                }
            }
            int stepNumber = 1;
            const int totalGuidedSteps =
                storyGuidedPlayerActionCount(mission);
            bool currentStepRequiresPlayerInput = true;
            bool currentStepIsAutomaticPass = false;
            if (!mission.script.empty() && storyMissionStep >= 0 &&
                storyMissionStep < static_cast<int>(mission.script.size()))
            {
                const StoryScriptAction& scriptedStep =
                    mission.script[static_cast<std::size_t>(storyMissionStep)];
                stepHeading = scriptedStep.heading.empty()
                    ? std::string(mission.lesson)
                    : std::string(scriptedStep.heading);
                stepBody = scriptedStep.instruction.empty()
                    ? std::string(mission.objective)
                    : std::string(scriptedStep.instruction);
                currentStepRequiresPlayerInput =
                    storyScriptActionRequiresPlayerInput(mission, scriptedStep);
                currentStepIsAutomaticPass =
                    storyScriptActionAutoResolves(mission, scriptedStep) &&
                    scriptedStep.kind == StoryActionKind::EndTurn;
                stepNumber = storyGuidedPlayerActionNumber(
                    mission, storyMissionStep);
            }
            sf::Color stepAccent = BoardBrassBright;
            if (storyStage == StoryStage::Complete)
            {
                stepHeading = "Mission Complete";
                const std::string completionBody = storyMissionIndex + 1 <
                        static_cast<int>(storyMissions(storyCampaign).size())
                    ? "The next mission is unlocked. Continue when you are ready."
                    : "You have completed " + std::string(storyCampaignName(storyCampaign)) +
                        " story arc.";
                stepBody = compactGameHud
                    ? std::string(mission.title) + " complete. " + completionBody
                    : completionBody;
                stepAccent = sf::Color(146, 232, 166);
            }
            else if (storyStage == StoryStage::Failed)
            {
                stepHeading = "Mission Failed";
                const std::string failureBody = !gameSnapshot.status.empty()
                    ? gameSnapshot.status
                    : mirewatchStory
                        ? "The Company stopped the resistance. Retry and adapt your position and attack order."
                        : seelieStory
                            ? "The Seelie defense broke. Retry with a safer formation and action order."
                            : "The resistance stopped the Company. Retry and adapt your position and attack order.";
                stepBody = compactGameHud
                    ? std::string(mission.title) + ": " + failureBody
                    : failureBody;
                stepAccent = sf::Color(233, 128, 106);
            }

            const bool leavesRoomForStoryHand = !gameSnapshot.hand.empty();
            const bool preserveStoryPiles =
                storyStage == StoryStage::Objective && storyMissionStep >= 0 &&
                storyMissionStep < static_cast<int>(mission.script.size()) &&
                mission.script[static_cast<std::size_t>(storyMissionStep)].kind ==
                    StoryActionKind::DrawCard;
            const bool singleStoryHandCard = gameSnapshot.hand.size() == 1;
            const bool compactStoryPlaque =
                leavesRoomForStoryHand && !singleStoryHandCard;
            const sf::Vector2f plaquePosition{
                preserveStoryPiles
                    ? HandStartX
                    : leavesRoomForStoryHand
                    ? (singleStoryHandCard
                        ? HandStartX + HandCardWidth + 24.0f
                        : 10.0f)
                    : GameBottomLeftX,
                GameBottomBarY + 4.0f};
            const sf::Vector2f plaqueSize{
                preserveStoryPiles
                    ? GameActionButtonX - HandStartX - 12.0f
                    : leavesRoomForStoryHand
                    ? (singleStoryHandCard
                        ? GameActionButtonX - plaquePosition.x - 12.0f
                        : 170.0f)
                    : 560.0f,
                GameBottomBarHeight - 10.0f};
            drawBeveledPlate(
                window,
                plaquePosition,
                plaqueSize,
                withAlpha(BoardPlate, 242),
                stepAccent,
                true,
                12.0f);

            // The wide plaque uses a medallion. A multi-card hand leaves a
            // narrow plaque, so it uses a small progress badge and gives the
            // remaining width to readable instructions.
            const sf::Vector2f medallion{
                plaquePosition.x + (compactStoryPlaque ? 25.0f : 32.0f),
                plaquePosition.y + plaqueSize.y * 0.5f};
            const std::string stepProgress = storyStage == StoryStage::Complete
                    ? std::string("OK")
                    : storyStage == StoryStage::Failed
                        ? std::string("!")
                        : openObjective
                            ? std::string("LIVE")
                        : !currentStepRequiresPlayerInput
                            ? std::string("AUTO")
                            : std::to_string(stepNumber) + "/" +
                                std::to_string(std::max(1, totalGuidedSteps));
            if (!compactStoryPlaque)
            {
                drawEllipseOutline(
                    window, medallion, 19.0f, 19.0f, 1.4f,
                    withAlpha(stepAccent, 150));
                sf::CircleShape medallionFace(15.0f, 24);
                medallionFace.setOrigin({15.0f, 15.0f});
                medallionFace.setPosition(medallion);
                medallionFace.setFillColor(sf::Color(22, 30, 28, 246));
                medallionFace.setOutlineThickness(1.6f);
                medallionFace.setOutlineColor(withAlpha(stepAccent, 235));
                window.draw(medallionFace);
                sf::Text stepLabel(
                    font,
                    storyStage == StoryStage::Objective
                        ? (openObjective
                            ? std::string("OPEN OBJECTIVE")
                            : currentStepRequiresPlayerInput
                                ? std::string("NEXT ACTION")
                                : currentStepIsAutomaticPass
                                    ? std::string("AUTO PASS")
                                : std::string("OPPONENT"))
                        : std::string("STATUS"),
                    compactGameHud ? 12 : 11);
                stepLabel.setFillColor(withAlpha(stepAccent, 230));
                const float captionWidth =
                    stepLabel.getLocalBounds().size.x + 14.0f;
                drawCutPlate(
                    {medallion.x - captionWidth * 0.5f,
                     plaquePosition.y + 5.0f},
                    {captionWidth, 18.0f},
                    4.0f,
                    sf::Color(22, 30, 28, 255),
                    sf::Color::Transparent,
                    0.0f);
                centerText(stepLabel, {medallion.x, plaquePosition.y + 14.0f});
                drawCrispText(window, stepLabel);
                unsigned int stepProgressSize =
                    storyStage == StoryStage::Objective ? 14u : 17u;
                sf::Text stepText(font, stepProgress, stepProgressSize);
                while (stepProgressSize > 9u &&
                       stepText.getLocalBounds().size.x > 25.0f)
                {
                    stepText.setCharacterSize(--stepProgressSize);
                }
                stepText.setFillColor(stepAccent);
                centerText(stepText, medallion);
                drawCrispText(window, stepText);
            }
            else
            {
                drawCutPlate(
                    {plaquePosition.x + plaqueSize.x - 39.0f,
                     plaquePosition.y + 7.0f},
                    {29.0f, 17.0f},
                    4.0f,
                    sf::Color(22, 30, 28, 246),
                    withAlpha(stepAccent, 235),
                    1.0f);
                drawCenteredText(
                    window,
                    font,
                    stepProgress,
                    10,
                    {plaquePosition.x + plaqueSize.x - 24.5f,
                     plaquePosition.y + 15.5f},
                    stepAccent);
            }

            const float textX =
                plaquePosition.x + (compactStoryPlaque ? 12.0f : 62.0f);
            if (leavesRoomForStoryHand)
            {
                const auto storySquareName = [](int row, int column) {
                    if (!game_data::inBounds(row, column))
                    {
                        return std::string("the glowing square");
                    }
                    return std::string(1, static_cast<char>('A' + column)) +
                        std::to_string(row + 1);
                };
                switch (mission.objectiveSpec.kind)
                {
                case StoryObjectiveKind::DeployCard:
                    stepHeading = "DEPLOY CARD";
                    stepBody = "Play " +
                        (mission.objectiveSpec.cardTitle.empty()
                            ? std::string("the highlighted card")
                            : std::string(mission.objectiveSpec.cardTitle)) +
                        " on glowing " +
                        storySquareName(
                            mission.objectiveSpec.targetRow,
                            mission.objectiveSpec.targetColumn) +
                        ".";
                    break;
                case StoryObjectiveKind::ReachSquare:
                    stepHeading = "REACH THE SQUARE";
                    stepBody = "Move the marked unit to glowing " +
                        storySquareName(
                            mission.objectiveSpec.targetRow,
                            mission.objectiveSpec.targetColumn) +
                        ".";
                    break;
                case StoryObjectiveKind::ControlSquares:
                    stepHeading = "TAKE CONTROL";
                    stepBody = controlInstructions(
                        mine.controlledSquares, mission.objectiveSpec.amount);
                    break;
                case StoryObjectiveKind::DefeatRole:
                    stepHeading = "DEFEAT THE TARGET";
                    stepBody = "Defeat the marked enemy.";
                    break;
                case StoryObjectiveKind::DefeatAllEnemies:
                    if (mission.standardMatch)
                    {
                        if (static_cast<game_data::Phase>(gameSnapshot.phase) ==
                            game_data::Phase::HeroPlacement)
                        {
                            stepHeading = "PLACE HEROES - 2:00";
                            stepBody =
                                "Select and place both Heroes on lit squares before the shared 2:00 deadline.";
                        }
                        else
                        {
                            const auto& opponent = gameSnapshot.players[
                                static_cast<std::size_t>((me == 1 ? 2 : 1) - 1)];
                            stepHeading = "WIN THE MATCH";
                            stepBody = "Defeat all " +
                                std::to_string(opponent.heroesAlive) +
                                " enemy Heroes or outlast their 15:00 clock. Inspect: double-click a card/piece, or focus + I.";
                        }
                    }
                    else
                    {
                        stepHeading = "CHOOSE A UNIT";
                        stepBody = "Inspect a unit (double-click or focus + I), then choose one printed action. " +
                            std::to_string(authoritativeStoryEnemyProgress(mission).remaining) +
                            " enemies remain.";
                    }
                    break;
                case StoryObjectiveKind::Scripted:
                    break;
                case StoryObjectiveKind::StoryOnly:
                case StoryObjectiveKind::Legacy:
                    stepHeading = "MISSION GOAL";
                    stepBody = std::string(mission.objective);
                    break;
                }
            }
            const float bodyWidth =
                plaqueSize.x - (compactStoryPlaque ? 24.0f : 78.0f);
            unsigned int headingSize = compactStoryPlaque ? 14u : 18u;
            sf::Text heading(font, stepHeading, headingSize);
            heading.setLetterSpacing(1.1f);
            const float headingWidth =
                bodyWidth - (compactStoryPlaque ? 36.0f : 0.0f);
            while (headingSize > 10u &&
                   heading.getLocalBounds().size.x > headingWidth)
            {
                heading.setCharacterSize(--headingSize);
            }
            heading.setFillColor(BoardParchment);
            heading.setPosition(
                {textX, plaquePosition.y + (compactStoryPlaque ? 8.0f : 12.0f)});
            drawCrispText(window, heading);
            unsigned int bodySize = compactStoryPlaque
                ? (compactGameHud ? 13u : 11u)
                : (compactGameHud ? 16u : 14u);
            const float bodyY =
                plaquePosition.y + (compactStoryPlaque ? 29.0f : 37.0f);
            const float bodyHeight =
                std::max(
                    20.0f,
                    plaquePosition.y + plaqueSize.y - 8.0f - bodyY);
            float bodyLineGap = compactGameHud ? 1.0f : 3.0f;
            auto wrappedBodyHeight = [&]() {
                return static_cast<float>(wrapText(font, stepBody, bodySize, bodyWidth).size()) *
                    (static_cast<float>(bodySize) + bodyLineGap);
            };
            // Guided instructions are the primary task text. Keep a firm
            // 13-point floor at the compact 800x600 layout; if authored copy
            // cannot fit at that size, capture validation below must fail so
            // the copy/layout is repaired instead of silently becoming tiny.
            while (bodySize > 13u && wrappedBodyHeight() > bodyHeight)
            {
                --bodySize;
                bodyLineGap = std::min(bodyLineGap, 1.0f);
            }
            if (captureRequest && compactGameHud &&
                wrappedBodyHeight() > bodyHeight + 0.5f)
            {
                failCaptureValidation(
                    "Capture layout error: compact Story instruction exceeds its reserved text bounds.");
            }
            drawWrappedText(
                window,
                font,
                stepBody,
                bodySize,
                {textX, bodyY},
                withAlpha(BoardParchmentMuted, 240),
                bodyWidth,
                bodyLineGap);

            if (!storyCorrection.empty() && storyStage == StoryStage::Objective)
            {
                const sf::Vector2f correctionPosition{
                    GameBottomLeftX, GameBottomBarY - (compactGameHud ? 60.0f : 46.0f)};
                const sf::Vector2f correctionSize{
                    560.0f, compactGameHud ? 54.0f : 40.0f};
                drawBeveledPlate(
                    window,
                    correctionPosition,
                    correctionSize,
                    sf::Color(54, 24, 22, 248),
                    sf::Color(233, 128, 106),
                    true,
                    8.0f);
                drawWrappedText(
                    window,
                    font,
                    "TRY AGAIN: " + storyCorrection,
                    compactGameHud ? 16 : 12,
                    correctionPosition + sf::Vector2f(12.0f, 9.0f),
                    sf::Color(255, 224, 210),
                    correctionSize.x - 24.0f,
                    compactGameHud ? 1.0f : 2.0f);
            }
        }

        // ---- Hand -------------------------------------------------------------
        if (!storyMode || !gameSnapshot.hand.empty())
        {
            clampListOffset(gameHandOffset, gameSnapshot.hand.size(), VisibleGameHandCards);
            const std::size_t lastHandCard =
                std::min(gameSnapshot.hand.size(), gameHandOffset + VisibleGameHandCards);
            const std::size_t visibleHandCards = lastHandCard - gameHandOffset;
            const std::optional<std::size_t> hoveredHandCard =
                gameDragActive ? std::nullopt : handCardAtPixel(currentPointer);
            const auto drawHandCard = [&](std::size_t i, bool hovered) {
                const std::size_t visibleIndex = i - gameHandOffset;
                const float x = gameHandCardX(visibleIndex, visibleHandCards);
                const game_data::GameCard& card = gameSnapshot.hand[i];
                const bool affordable = phase == game_data::Phase::HeroPlacement ||
                    (gameSnapshot.relentlessPieceId == 0 &&
                     (sandboxMode || card.cost <= mine.resources) &&
                     (sandboxMode || gameSnapshot.activePlayer == me) &&
                     phase == game_data::Phase::Playing &&
                     (sandboxMode || game_data::heroTraitsAllowCard(gameSnapshot.pieces, me, card)));
                drawGameCardFace(
                    {x, HandY - (hovered ? HandHoverLift : 0.0f)},
                    card,
                    selectedHandIndex && *selectedHandIndex == i,
                    affordable,
                    hovered || visibleIndex + 1 == visibleHandCards
                        ? HandCardWidth
                        : gameHandCardPitch(visibleHandCards));
            };
            for (std::size_t i = gameHandOffset; i < lastHandCard; ++i)
            {
                if (!hoveredHandCard || *hoveredHandCard != i)
                {
                    drawHandCard(i, false);
                }
            }
            if (hoveredHandCard && *hoveredHandCard >= gameHandOffset &&
                *hoveredHandCard < lastHandCard)
            {
                drawHandCard(*hoveredHandCard, true);
            }

            if (gameSnapshot.hand.empty())
            {
                sf::Text emptyHand(font, "No cards in hand", 13);
                emptyHand.setFillColor(withAlpha(BoardParchmentMuted, 168));
                centerText(
                    emptyHand,
                    {HandStartX + (HandRightX - HandStartX) * 0.5f,
                     HandY + HandCardHeight * 0.5f});
                drawCrispText(window, emptyHand);
            }
            else if (gameSnapshot.hand.size() > VisibleGameHandCards)
            {
                // Overflow chevrons instead of the "Cards 1-5/6" debug readout.
                const float handRight =
                    gameHandCardX(visibleHandCards - 1, visibleHandCards) +
                    HandCardWidth;
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
                    sf::Text more(font, std::to_string(hidden) + " MORE", 10);
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
                    sf::Vector2f baseCenter = gameDragCurrentPos;
                    sf::Vector2f healthBadgeCenter{
                        gameDragCurrentPos.x + (sandboxPlayer == 1 ? -34.0f : 34.0f) *
                            static_cast<float>(std::max(1, draggedCard.width)),
                        gameDragCurrentPos.y + 18.0f * static_cast<float>(std::max(1, draggedCard.height))};
                    float scale = 1.0f;
                    if (draggedHandSquare)
                    {
                        const BoardCellMetrics metrics =
                            boardCellMetrics(draggedHandSquare->first, draggedHandSquare->second);
                        anchor = boardFootprintAnchor(
                            draggedHandSquare->first,
                            draggedHandSquare->second,
                            draggedCard.width,
                            draggedCard.height,
                            gameSnapshot.yourPlayer);
                        baseCenter = boardFootprintCenter(
                            draggedHandSquare->first,
                            draggedHandSquare->second,
                            draggedCard.width,
                            draggedCard.height,
                            gameSnapshot.yourPlayer);
                        healthBadgeCenter = boardFootprintHealthBadgeCenter(
                            draggedHandSquare->first,
                            draggedHandSquare->second,
                            draggedCard.width,
                            draggedCard.height,
                            gameSnapshot.yourPlayer,
                            sandboxPlayer);
                        scale = metrics.depthScale;
                    }
                    drawCardPiecePreview(
                        draggedCard,
                        sandboxPlayer,
                        anchor,
                        baseCenter,
                        healthBadgeCenter,
                        scale,
                        draggedHandDropValid);
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
                        affordable,
                        HandCardWidth);
                }
            }
            else if (gameDragKind == GameDragKind::Piece && draggedPiece)
            {
                sf::Vector2f anchor = gameDragCurrentPos;
                sf::Vector2f baseCenter = gameDragCurrentPos;
                sf::Vector2f healthBadgeCenter{
                    gameDragCurrentPos.x + (draggedPiece->owner == 1 ? -34.0f : 34.0f) *
                        static_cast<float>(std::max(1, draggedPiece->width)),
                    gameDragCurrentPos.y + 18.0f * static_cast<float>(std::max(1, draggedPiece->height))};
                float scale = 1.0f;
                if (draggedPieceSquare)
                {
                    const BoardCellMetrics metrics =
                        boardCellMetrics(draggedPieceSquare->first, draggedPieceSquare->second);
                    anchor = boardFootprintAnchor(
                        draggedPieceSquare->first,
                        draggedPieceSquare->second,
                        draggedPiece->width,
                        draggedPiece->height,
                        gameSnapshot.yourPlayer);
                    baseCenter = boardFootprintCenter(
                        draggedPieceSquare->first,
                        draggedPieceSquare->second,
                        draggedPiece->width,
                        draggedPiece->height,
                        gameSnapshot.yourPlayer);
                    healthBadgeCenter = boardFootprintHealthBadgeCenter(
                        draggedPieceSquare->first,
                        draggedPieceSquare->second,
                        draggedPiece->width,
                        draggedPiece->height,
                        gameSnapshot.yourPlayer,
                        draggedPiece->owner);
                    scale = metrics.depthScale;
                }
                const sf::Color tint = draggedPieceDropValid
                    ? sf::Color(255, 255, 255, 220)
                    : sf::Color(220, 120, 110, 190);
                drawPieceBase(
                    window,
                    baseCenter,
                    scale,
                    draggedPiece->owner,
                    false,
                    static_cast<float>(draggedPiece->width),
                    static_cast<float>(draggedPiece->height),
                    pieceBaseArtworkFor(
                        draggedPiece->owner,
                        draggedPiece->width,
                        draggedPiece->height),
                    rarityGemArtworkFor(draggedPiece->name));
                drawPieceSelectionRing(
                    window,
                    baseCenter,
                    scale,
                    0.7f,
                    draggedPieceDropValid ? BoardBrassBright : sf::Color(232, 104, 92),
                    static_cast<float>(draggedPiece->width),
                    static_cast<float>(draggedPiece->height));
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

                drawPieceHealthBadge(
                    window,
                    healthBadgeCenter,
                    scale,
                    draggedPiece->health,
                    draggedPiece->owner,
                    false,
                    font,
                    draggedPiece->isHero);
            }
        }

        if (storyMode)
        {
            if (!storyPopupPanels.empty())
            {
                sf::RectangleShape veil({ui_canvas::Width, ui_canvas::Height});
                veil.setPosition({ui_canvas::Left, 0.0f});
                veil.setFillColor(sf::Color(4, 8, 9, 214));
                window.draw(veil);

                const StoryPanel& panel = storyPopupPanels[storyPopupPage];
                const StoryMission& popupMission =
                    storyMissions(storyCampaign)[static_cast<std::size_t>(storyMissionIndex)];
                const std::string_view displayedArtPath = popupMission.scenarioArtPath.empty()
                    ? panel.artPath
                    : popupMission.scenarioArtPath;
                drawBeveledPlate(
                    window,
                    {72.0f, 66.0f},
                    {656.0f, 466.0f},
                    sf::Color(20, 29, 28, 252),
                    BoardBrassBright,
                    true,
                    16.0f);
                drawBeveledPlate(
                    window,
                    {94.0f, 100.0f},
                    {226.0f, 342.0f},
                    sf::Color(31, 40, 37, 250),
                    BoardBrassDim,
                    false,
                    10.0f);
                if (!displayedArtPath.empty())
                {
                    if (sf::Texture* art = textures.load(std::string(displayedArtPath)))
                    {
                        drawCoverSprite(window, *art, {{108.0f, 114.0f}, {198.0f, 314.0f}});
                    }
                }
                else
                {
                    drawCenteredText(
                        window, font, "STORY SCENE", 15, {207.0f, 271.0f}, BoardBrassBright);
                }
                drawStorySpeakerPortraitInset(
                    popupMission.scenarioArtPath,
                    panel.artPath,
                    {151.0f, 386.0f},
                    40.0f);
                sf::Font& popupSpeakerFont =
                    gloomthornFontLoaded ? gloomthornFont : font;
                const std::string popupSpeaker(panel.speaker);
                unsigned int popupSpeakerSize = 25;
                sf::Text popupSpeakerMeasure(
                    popupSpeakerFont, popupSpeaker, popupSpeakerSize);
                while (popupSpeakerSize > 15u &&
                       popupSpeakerMeasure.getLocalBounds().size.x > 338.0f)
                {
                    popupSpeakerMeasure.setCharacterSize(--popupSpeakerSize);
                }
                drawText(
                    window,
                    popupSpeakerFont,
                    popupSpeaker,
                    popupSpeakerSize,
                    {350.0f, 112.0f},
                    BoardBrassBright,
                    338.0f);
                drawSeparatorRule(window, {350.0f, 154.0f}, 338.0f);
                unsigned int popupBodySize = 18;
                float popupBodyGap = 8.0f;
                constexpr float PopupBodyWidth = 338.0f;
                constexpr float PopupBodyHeight = 236.0f;
                while (popupBodySize > 14)
                {
                    const float requiredHeight = static_cast<float>(
                        wrapText(font, std::string(panel.text), popupBodySize, PopupBodyWidth).size()) *
                        (static_cast<float>(popupBodySize) + popupBodyGap);
                    if (requiredHeight <= PopupBodyHeight)
                    {
                        break;
                    }
                    --popupBodySize;
                    popupBodyGap = std::max(4.0f, popupBodyGap - 1.0f);
                }
                drawWrappedText(
                    window,
                    font,
                    std::string(panel.text),
                    popupBodySize,
                    {350.0f, 178.0f},
                    BoardParchment,
                    PopupBodyWidth,
                    popupBodyGap);
                drawText(
                    window,
                    font,
                    "IN-MISSION BEAT " + std::to_string(storyPopupPage + 1) + " OF " +
                        std::to_string(storyPopupPanels.size()),
                    12,
                    {350.0f, 426.0f},
                    BoardParchmentMuted,
                    160.0f);
                storyPopupContinueButton.setLabel(
                    storyPopupPage + 1 >= storyPopupPanels.size() && storyCompleteAfterPopup
                        ? "Finish Mission"
                        : "Continue");
                if (storyPopupPage > 0)
                {
                    storyPopupPreviousButton.draw(window, animationTime);
                }
                storyPopupContinueButton.draw(window, animationTime);
            }
            else if (gameSnapshot.foresightChoices.empty())
            {
                drawPiecePopup();
            }
            else
            {
                drawForesightChoices();
            }
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

        if (gameSnapshot.foresightChoices.empty())
        {
            drawPiecePopup();
        }
        else
        {
            drawForesightChoices();
        }
    };
