    std::unordered_map<std::string, sf::FloatRect> storyTokenVisibleBounds;
    const auto storyPieceArtBounds = [&](const game_data::Piece& piece, int viewer) {
        auto cell = boardCellMetricsForViewer(piece.row, piece.column, viewer);
        sf::Vector2f anchor = boardFootprintAnchor(
            piece.row, piece.column, piece.width, piece.height, viewer);
        float depth = cell.depthScale;
        if (const auto move = pieceMoveAnimations.find(piece.id); move != pieceMoveAnimations.end())
        {
            const float t = std::clamp((animationTime - move->second.startTime) / move->second.duration, 0.0f, 1.0f);
            const auto from = boardFootprintAnchor(move->second.fromRow, move->second.fromColumn,
                piece.width, piece.height, viewer);
            const auto fromCell = boardCellMetricsForViewer(move->second.fromRow, move->second.fromColumn, viewer);
            anchor = from + (anchor - from) * t;
            depth = fromCell.depthScale + (depth - fromCell.depthScale) * t;
        }
        const auto target = pieceTargetRect(anchor, depth, true, piece.width, piece.height);
        const auto visibleBounds = [&](const std::string& path, int frames,
                                        bool animated) -> std::optional<sf::FloatRect> {
            if (path.empty()) return std::nullopt;
            sf::Texture* texture = animated ? walkAnimTexture(path) : textures.load(path);
            if (!texture) return std::nullopt;
            const auto dimensions = texture->getSize();
            const unsigned int frameCount = static_cast<unsigned int>(std::max(1, frames));
            const unsigned int frameWidth = dimensions.x / frameCount;
            if (frameWidth == 0 || dimensions.y == 0) return std::nullopt;
            const std::string cacheKey = path + ":" + std::to_string(frameCount);
            auto found = storyTokenVisibleBounds.find(cacheKey);
            if (found == storyTokenVisibleBounds.end())
            {
                // Union the visible pixels of every frame once. Wings and idle
                // motion need room too; transparent padding is not a hover target.
                const sf::Image pixels = texture->copyToImage();
                const auto* rgba = pixels.getPixelsPtr();
                unsigned int left = frameWidth, top = dimensions.y, right = 0, bottom = 0;
                for (unsigned int y = 0; y < dimensions.y; y += 2)
                    for (unsigned int x = 0; x < frameWidth * frameCount; x += 2)
                        if (rgba[(static_cast<std::size_t>(y) * dimensions.x + x) * 4 + 3] > 32)
                        {
                            const unsigned int frameX = x % frameWidth;
                            left = std::min(left, frameX); right = std::max(right, frameX);
                            top = std::min(top, y); bottom = std::max(bottom, y);
                        }
                sf::FloatRect visible{{0.0f, 0.0f}, {1.0f, 1.0f}};
                if (left <= right && top <= bottom)
                    visible = {{static_cast<float>(left) / frameWidth, static_cast<float>(top) / dimensions.y},
                        {static_cast<float>(std::min(right + 2, frameWidth) - left) / frameWidth,
                         static_cast<float>(std::min(bottom + 2, dimensions.y) - top) / dimensions.y}};
                found = storyTokenVisibleBounds.emplace(cacheKey, visible).first;
            }
            const float scale = std::min(target.size.x / frameWidth, target.size.y / dimensions.y);
            const sf::Vector2f fitted{frameWidth * scale, dimensions.y * scale};
            const auto visible = found->second;
            const float x = piece.owner == 2 ? 1.0f - visible.position.x - visible.size.x : visible.position.x;
            return sf::FloatRect{target.position + (target.size - fitted) * 0.5f +
                sf::Vector2f{x * fitted.x - 4.0f, visible.position.y * fitted.y - 4.0f},
                {visible.size.x * fitted.x + 8.0f, visible.size.y * fitted.y + 8.0f}};
        };
        auto bounds = visibleBounds(game_data::pieceTokenPathForState(piece), 1, false);
        const auto include = [&](const std::optional<sf::FloatRect>& other) {
            if (!other) return;
            if (!bounds) { bounds = other; return; }
            const sf::Vector2f topLeft{std::min(bounds->position.x, other->position.x),
                std::min(bounds->position.y, other->position.y)};
            const sf::Vector2f bottomRight{
                std::max(bounds->position.x + bounds->size.x, other->position.x + other->size.x),
                std::max(bounds->position.y + bounds->size.y, other->position.y + other->size.y)};
            bounds = sf::FloatRect{topLeft, bottomRight - topLeft};
        };
        if (!game_data::pieceUsesState1Token(piece))
            include(visibleBounds(piece.idleAnimPath, piece.idleAnimFrames, true));
        if (pieceMoveAnimations.contains(piece.id))
            include(visibleBounds(pieceWalkAnimPath(piece), piece.walkAnimFrames, true));
        return bounds.value_or(target);
    };

    // Search open space around the speaker before settling for a distant gap.
    // One bubble is visible at a time, with a strong penalty for hiding its speaker.
    const auto placeStoryOverlay = [&](sf::Vector2f size,
                                       std::span<const game_data::Piece> pieces,
                                       std::optional<int> speakerId, int viewer) {
        const sf::FloatRect safe{{20.0f, 76.0f}, {760.0f, 378.0f}};
        sf::Vector2f anchor{400.0f, 228.0f};
        std::vector<sf::FloatRect> artwork;
        for (const auto& piece : pieces)
        {
            const auto bounds = storyPieceArtBounds(piece, viewer);
            artwork.push_back(bounds);
            if (speakerId == piece.id)
                anchor = {bounds.position.x + bounds.size.x * 0.5f, bounds.position.y + 18.0f};
        }
        std::vector<float> xs{safe.position.x, safe.position.x + safe.size.x - size.x,
            anchor.x - size.x * 0.5f, anchor.x + 40.0f, anchor.x - size.x - 40.0f};
        std::vector<float> ys{safe.position.y, safe.position.y + safe.size.y - size.y,
            anchor.y - size.y - 18.0f, anchor.y + 42.0f};
        for (const auto& rect : artwork)
        {
            xs.push_back(rect.position.x - size.x - 8.0f);
            xs.push_back(rect.position.x + rect.size.x + 8.0f);
            ys.push_back(rect.position.y - size.y - 8.0f);
            ys.push_back(rect.position.y + rect.size.y + 8.0f);
        }
        sf::Vector2f best = safe.position;
        float bestScore = std::numeric_limits<float>::max();
        for (float x : xs) for (float y : ys)
        {
            sf::FloatRect candidate{{
                std::clamp(x, safe.position.x, safe.position.x + safe.size.x - size.x),
                std::clamp(y, safe.position.y, safe.position.y + safe.size.y - size.y)}, size};
            const sf::Vector2f center = candidate.position + size * 0.5f;
            float score = std::abs(center.x - anchor.x) + std::abs(center.y - anchor.y);
            for (std::size_t i = 0; i < pieces.size(); ++i)
            {
                if (const auto overlap = candidate.findIntersection(artwork[i]))
                    score += overlap->size.x * overlap->size.y *
                        (speakerId == pieces[i].id ? 100.0f : 10.0f);
            }
            if (score < bestScore) { bestScore = score; best = candidate.position; }
        }
        return sf::FloatRect{best, size};
    };

    const auto drawStorySpeechBubble = [&](const StoryPanel& panel,
                                           std::span<const game_data::Piece> pieces,
                                           int viewer) {
        const std::optional<int> speakerId = storySpeakingPiece(panel, pieces);
        const auto speaker = std::find_if(pieces.begin(), pieces.end(),
            [&](const auto& piece) { return speakerId == piece.id; });
        std::string portraitPath(panel.artPath);
        if (speaker != pieces.end() && !(portraitPath.starts_with("cards/") ||
            portraitPath.starts_with("characters/"))) portraitPath = speaker->imagePath;
        // A guide heading or narrator never borrows the face in its artwork.
        const bool portrait = !portraitPath.empty() && storyPanelHasCharacterSpeaker(panel) &&
            (portraitPath.starts_with("cards/") || portraitPath.starts_with("characters/"));
        constexpr float width = 354.0f;
        constexpr float bodyWidth = width - 36.0f;
        unsigned int bodySize = 17;
        float bodyHeight = 0.0f;
        const auto measure = [&]() {
            const auto lines = wrapText(font, std::string(panel.text), bodySize, bodyWidth);
            return static_cast<float>(lines.size()) * (static_cast<float>(bodySize) + 4.0f);
        };
        bodyHeight = measure();
        while (bodySize > 15 && bodyHeight > 256.0f) { --bodySize; bodyHeight = measure(); }
        const float headerHeight = portrait ? 62.0f : 43.0f;
        const sf::Vector2f size{width, headerHeight + bodyHeight + 14.0f};
        storySpeechBubbleBounds = placeStoryOverlay(size, pieces, speakerId, viewer);
        const auto position = storySpeechBubbleBounds.position;
        const sf::Color paper(245, 237, 216);
        const sf::Color edge(88, 71, 46);
        if (speaker != pieces.end())
        {
            const auto art = storyPieceArtBounds(*speaker, viewer);
            const sf::Vector2f center = position + size * 0.5f;
            const sf::Vector2f artCenter = art.position + art.size * 0.5f;
            sf::Vector2f tip{artCenter.x, center.y < artCenter.y ? art.position.y - 4.0f :
                art.position.y + art.size.y + 4.0f};
            if (center.x < art.position.x || center.x > art.position.x + art.size.x)
                tip = {center.x < artCenter.x ? art.position.x - 4.0f : art.position.x + art.size.x + 4.0f,
                    std::clamp(center.y, art.position.y, art.position.y + art.size.y)};
            sf::Vector2f base{std::clamp(tip.x, position.x + 18.0f, position.x + size.x - 18.0f),
                tip.y < center.y ? position.y + 2.0f : position.y + size.y - 2.0f};
            sf::Vector2f across{11.0f, 0.0f};
            if (tip.x < position.x || tip.x > position.x + size.x)
            {
                base = {tip.x < position.x ? position.x + 2.0f : position.x + size.x - 2.0f,
                    std::clamp(tip.y, position.y + 18.0f, position.y + size.y - 18.0f)};
                across = {0.0f, 11.0f};
            }
            const sf::Vector2f direction = tip - base;
            const float distance = std::sqrt(direction.x * direction.x + direction.y * direction.y);
            const sf::Vector2f tailTip = distance > 42.0f ? base + direction * (32.0f / distance) : tip;
            if (distance > 42.0f) drawEdgeLine(window, tailTip, tip, 1.5f, edge);
            sf::ConvexShape tail(3);
            tail.setPoint(0, base - across); tail.setPoint(1, tailTip); tail.setPoint(2, base + across);
            tail.setFillColor(paper); tail.setOutlineColor(edge); tail.setOutlineThickness(1.5f);
            window.draw(tail);
            drawPieceSelectionRing(window, boardFootprintCenter(speaker->row, speaker->column,
                speaker->width, speaker->height, viewer),
                boardCellMetricsForViewer(speaker->row, speaker->column, viewer).depthScale,
                0.4f, BoardBrassBright, static_cast<float>(speaker->width),
                static_cast<float>(speaker->height));
        }
        // Soft corners and a quiet outline replace the large ornamental modal.
        sf::ConvexShape bubble(24);
        constexpr float radius = 13.0f;
        for (int corner = 0; corner < 4; ++corner)
        {
            const sf::Vector2f center = position + sf::Vector2f{
                corner == 0 || corner == 3 ? radius : size.x - radius,
                corner < 2 ? radius : size.y - radius};
            for (int step = 0; step < 6; ++step)
            {
                const float angle = (-180.0f + static_cast<float>(corner) * 90.0f +
                    static_cast<float>(step) * 18.0f) * Pi / 180.0f;
                bubble.setPoint(static_cast<std::size_t>(corner * 6 + step),
                    center + sf::Vector2f{std::cos(angle) * radius, std::sin(angle) * radius});
            }
        }
        bubble.setFillColor(sf::Color(0, 0, 0, 65)); bubble.move({3.0f, 4.0f}); window.draw(bubble);
        bubble.move({-3.0f, -4.0f}); bubble.setFillColor(paper);
        bubble.setOutlineColor(edge); bubble.setOutlineThickness(1.5f); window.draw(bubble);
        if (portrait)
            drawStorySpeakerPortraitInset("dialogue", portraitPath,
                position + sf::Vector2f{37.0f, 31.0f}, 25.0f);
        drawText(window, font, std::string(panel.speaker), 16,
            position + sf::Vector2f{portrait ? 72.0f : 18.0f, portrait ? 23.0f : 15.0f},
            sf::Color(103, 68, 29), portrait ? 264.0f : bodyWidth);
        const float textBottom = drawWrappedText(window, font, std::string(panel.text), bodySize,
            position + sf::Vector2f{18.0f, headerHeight}, sf::Color(33, 33, 29), bodyWidth, 4.0f);
        if (captureRequest && (textBottom > position.y + size.y - 7.0f ||
            position.y + size.y > 454.5f))
            failCaptureValidation("Story speech bubble text exceeds its board-safe bounds.");
    };

    const auto drawStoryDialogueFooter = [&]() {
        drawBeveledPlate(window, {18.0f, 514.0f}, {764.0f, 80.0f},
            sf::Color(12, 21, 21, 248), BoardBrassDim, false, 8.0f);
        drawText(window, font, "Click the bubble or Continue", 14,
            {38.0f, 534.0f}, BoardParchment, 320.0f);
        drawText(window, font, std::to_string(storyPopupPage + 1) + " of " +
            std::to_string(storyPopupPanels.size()), 12,
            {38.0f, 557.0f}, BoardParchmentMuted, 200.0f);
        storyPopupPreviousButton.setPosition({454.0f, 527.0f});
        storyPopupPreviousButton.setSize({146.0f, 40.0f});
        storyPopupContinueButton.setPosition({616.0f, 527.0f});
        storyPopupContinueButton.setSize({146.0f, 40.0f});
        storyPopupContinueButton.setLabel(storyPopupPage + 1 >= storyPopupPanels.size() &&
            storyCompleteAfterPopup ? "Finish Mission" : "Continue");
        storyPopupContinueButton.setLabelSize(15);
        storyPopupPreviousButton.setLabelSize(15);
        if (storyPopupPage > 0) storyPopupPreviousButton.draw(window, animationTime);
        storyPopupContinueButton.draw(window, animationTime);
        drawCenteredText(window, font,
            "LEFT/RIGHT: PAGE  |  TAB: BUTTON  |  ENTER: ACTIVATE", 12,
            {400.0f, 585.0f}, BoardParchmentMuted);
    };

    const StoryMission* cachedStorySceneMission = nullptr;
    std::size_t cachedStorySceneLibrarySize = 0;
    game_data::Snapshot storyScenePreview;
    const auto prepareStoryScenePreview = [&](const StoryMission& mission) {
        const std::size_t librarySize = allCardLibrary.size() + cardLibrary.size();
        if (cachedStorySceneMission == &mission && cachedStorySceneLibrarySize == librarySize) return;
        std::vector<game_data::GameCard> cards;
        for (std::string_view title : packagedStoryCardTitles())
            if (auto card = resolvedStoryCardNamed(title)) cards.push_back(std::move(*card));
        storyScenePreview = storyDialoguePreview(mission, cards);
        cachedStorySceneMission = &mission;
        cachedStorySceneLibrarySize = librarySize;
    };
