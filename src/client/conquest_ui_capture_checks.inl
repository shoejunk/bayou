    // Included inside ConquestScreen so the offline harness can assert outcomes
    // without exposing mutable UI state to the client. Every action below uses
    // the same SFML event entry point as a real pointer or keyboard event.
    bool runCaptureInteractionChecks(
        sf::RenderWindow& window,
        const std::vector<card_data::Card>& library,
        std::string& failure)
    {
        failure.clear();
        const auto check = [&](bool condition, const char* message) {
            if (!condition) failure = message;
            return condition;
        };
        const auto click = [&](float x, float y) {
            const sf::Vector2i pixel = window.mapCoordsToPixel({x, y});
            handleEvent(sf::Event{sf::Event::MouseButtonPressed{
                sf::Mouse::Button::Left, pixel}}, window);
        };
        const auto escape = [&]() {
            sf::Event::KeyPressed key{};
            key.code = sf::Keyboard::Key::Escape;
            handleEvent(sf::Event{key}, window);
        };
        const auto regionClick = [&](int regionId) {
            const conquest_map::RegionDefinition* region = conquest_map::region(regionId);
            const sf::Vector2f point = mapPoint(region->centerX, region->centerY);
            click(point.x, point.y);
        };
        const auto deckClick = [&](std::size_t row) {
            click(684.0f, 165.0f + static_cast<float>(row) * 42.0f);
        };
        const auto offline = [&]() {
            return captureMode && accessToken.empty() && !busy() && !pendingEventWatch;
        };

        applyCaptureState("conquest-preview", library);
        if (!check(offline() && !joinedEvent() && !joinSetupActive,
                   "Preview must begin offline, outside the campaign and outside setup.")) return false;
        deckClick(0);
        regionClick(1);
        if (!check(placements.empty() && !joinedEvent(),
                   "Browsing the preview must not place an army or join the campaign.")) return false;

        click(685.0f, 432.0f);
        if (!check(joinSetupActive && !joinedEvent() && selectedEventDeckId.has_value(),
                   "Join must start guided placement with a selected deck before committing membership.")) return false;
        click(685.0f, 432.0f);
        if (!check(!joinConfirmationVisible && !joinedEvent() && offline(),
                   "An empty deployment must not open confirmation, join, or contact a service.")) return false;
        regionClick(5);
        if (!check(placements.empty(),
                   "A non-edge starting region must be rejected.")) return false;

        conquest_data::EventDeckState occupiedStartingRegion;
        occupiedStartingRegion.id = 9999;
        occupiedStartingRegion.owner = "Rival";
        occupiedStartingRegion.deployed = true;
        occupiedStartingRegion.regionId = 1;
        eventState.decks.push_back(occupiedStartingRegion);
        regionClick(1);
        if (!check(placements.empty() && !statusSuccess,
                   "An occupied edge region must explain its rejection without creating a draft placement.")) return false;
        eventState.decks.pop_back();

        deckClick(0);
        regionClick(1);
        if (!check(placements.size() == 1 && placements.front().deckId == 100 &&
                       placements.front().regionId == 1 && !joinedEvent(),
                   "Choosing an edge region must create one visible draft placement only.")) return false;
        click(685.0f, 432.0f);
        if (!check(joinConfirmationVisible && !joinedEvent(),
                   "One starting deck must be sufficient to review the join commitment.")) return false;
        regionClick(20);
        if (!check(joinConfirmationVisible && placements.size() == 1 &&
                       placements.front().regionId == 1,
                   "Map clicks behind join confirmation must not alter the reviewed deployment.")) return false;
        escape();
        if (!check(!joinConfirmationVisible && placements.size() == 1 && joinSetupActive,
                   "Escape must dismiss join confirmation and preserve the deployment draft.")) return false;

        deckClick(1);
        regionClick(20);
        if (!check(placements.size() == 1 && placements.front().regionId == 1,
                   "A nonadjacent second region must not erase the valid first placement.")) return false;
        regionClick(2);
        if (!check(placements.size() == 2,
                   "A second deck on an adjacent edge region must be accepted.")) return false;

        // Repositioning a placed deck to an invalid region must be reversible:
        // validation cannot silently delete the old valid placement first.
        deckClick(0);
        regionClick(20);
        const auto firstPlacement = std::find_if(placements.begin(), placements.end(),
            [](const auto& placement) { return placement.deckId == 100; });
        if (!check(placements.size() == 2 && firstPlacement != placements.end() &&
                       firstPlacement->regionId == 1,
                   "An invalid replacement must retain both existing starting placements.")) return false;
        deckClick(2);
        regionClick(3);
        if (!check(placements.size() == 2,
                   "A third deck cannot silently replace either of the two chosen starting decks.")) return false;

        deckClick(1);
        click(685.0f, 516.0f);
        if (!check(placements.size() == 1 && placements.front().deckId == 100,
                   "Remove Placement must remove only the selected deck's draft placement.")) return false;
        regionClick(2);
        if (!check(placements.size() == 2,
                   "A removed placement must be easy to restore without restarting setup.")) return false;

        click(685.0f, 432.0f);
        if (!check(joinConfirmationVisible && !joinedEvent(),
                   "Two selected starting positions must lead to confirmation before joining.")) return false;
        click(295.0f, 428.0f);
        if (!check(!joinConfirmationVisible && placements.size() == 2 && !joinedEvent(),
                   "Edit Placement must dismiss confirmation without losing either placement.")) return false;
        click(685.0f, 432.0f);
        click(500.0f, 428.0f);
        if (!check(joinedEvent() && !joinConfirmationVisible && offline(),
                   "Confirm Join must commit membership exactly once using only the offline fixture.")) return false;
        click(685.0f, 432.0f);
        if (!check(joinedEvent() && !joinSetupActive && !joinConfirmationVisible &&
                       placements.empty() && offline(),
                   "Waiting for Start must remain inactive after joining and cannot reopen deployment.")) return false;
        const std::uint64_t joinedId = eventState.summary.id;
        click(90.0f, 42.0f);
        if (!check(view == View::Events && joinedEvent() && !pendingAction,
                   "Events must leave the map while retaining campaign membership.")) return false;
        escape();
        const std::optional<ConquestScreenAction> closeAction = takeAction();
        if (!check(closeAction && closeAction->kind == ConquestScreenAction::Kind::Close &&
                       eventState.summary.id == joinedId && joinedEvent() && offline(),
                   "Escape from Events must request the main menu without surrendering or contacting a service.")) return false;

        applyCaptureState("conquest-registration-ready", library);
        const std::uint64_t draftEventId = eventState.summary.id;
        click(90.0f, 42.0f);
        if (!check(view == View::Events && placements.size() == 2 && !joinedEvent(),
                   "Backing out of setup must keep its draft without joining.")) return false;
        click(400.0f, EventRowY + 3.0f * EventRowHeight + 30.0f);
        if (!check(view == View::Event && eventState.summary.id == draftEventId &&
                       joinSetupActive && placements.size() == 2,
                   "Reopening the same campaign must restore its in-progress deployment.")) return false;

        click(685.0f, 476.0f);
        if (!check(view == View::Loadout && returnToEventAfterLoadout,
                   "Army Setup must preserve a return route to the campaign being prepared.")) return false;
        click(590.0f, LoadoutRowY + 24.0f);
        click(500.0f, 511.0f);
        if (!check(army.deckIds.size() == 4 && army.deckIds != savedArmyDeckIds,
                   "Removing a deck must visibly change the pending army roster.")) return false;
        click(723.0f, 82.0f);
        if (!check(view == View::Loadout && army.deckIds.size() == 4 &&
                       army.deckIds != savedArmyDeckIds && !statusSuccess && offline(),
                   "Refresh must preserve an unsaved army and explain Save Army or Undo Changes.")) return false;
        click(76.0f, 82.0f);
        if (!check(view == View::Loadout && returnToEventAfterLoadout && !statusSuccess,
                   "Backing out of an unsaved army change must explain Save Army or Undo Changes.")) return false;
        click(576.0f, 82.0f);
        if (!check(army.deckIds == savedArmyDeckIds && army.deckIds.size() == 5,
                   "Undo Changes must restore the saved army without a service request.")) return false;
        click(76.0f, 82.0f);
        if (!check(view == View::Event && eventState.summary.id == draftEventId &&
                       placements.size() == 2 && offline(),
                   "Returning from an unchanged army must preserve the campaign and both placements.")) return false;

        click(685.0f, 476.0f);
        click(590.0f, LoadoutRowY + 24.0f);
        click(500.0f, 511.0f);
        click(685.0f, 511.0f);
        if (!check(army.deckIds == savedArmyDeckIds && army.deckIds.size() == 4 && offline(),
                   "Save Army must accept the changed roster using only the offline fixture.")) return false;
        click(76.0f, 82.0f);
        if (!check(view == View::Event && eventState.summary.id == draftEventId &&
                       placements.size() == 1 && placements.front().deckId == 101,
                   "Returning after removing a deck must clear only that deck's obsolete placement.")) return false;

        applyCaptureState("conquest-loadouts-empty", library);
        escape();
        if (!check(view == View::Events && offline(),
                   "An empty first-time army must not trap the player behind an unsaved-changes warning.")) return false;

        applyCaptureState("conquest-map", library);
        deckClick(0);
        regionClick(4);
        if (!check(ordersDirty && plannedOrders.contains(900) && plannedOrders.at(900) == 4,
                   "Editing a planning destination must mark the new order as unsaved.")) return false;
        click(685.0f, 432.0f);
        if (!check(!ordersDirty && currentPlayer() && currentPlayer()->ordersSubmitted &&
                       plannedOrders.at(900) == 4 && offline(),
                   "Submitting valid orders must acknowledge the plan, clear the unsaved indicator, and remain offline.")) return false;
        regionClick(1);
        click(685.0f, 432.0f);
        if (!check(ordersDirty && !statusSuccess && offline(),
                   "Conflicting friendly destinations must keep the unsaved indicator and explain why submission failed.")) return false;

        deckClick(0);
        regionClick(2);
        click(90.0f, 42.0f);
        if (!check(exitOrdersConfirmationVisible && view == View::Event && ordersDirty,
                   "Back with unsaved orders must explain the choice before leaving the campaign.")) return false;
        escape();
        if (!check(!exitOrdersConfirmationVisible && view == View::Event && ordersDirty &&
                       plannedOrders.at(900) == 2,
                   "Escape from the unsaved-orders dialog must keep the draft and the map open.")) return false;
        click(90.0f, 42.0f);
        click(295.0f, 428.0f);
        if (!check(!exitOrdersConfirmationVisible && view == View::Event && ordersDirty &&
                       plannedOrders.at(900) == 2,
                   "Keep Editing must dismiss the exit warning without altering the unsaved orders.")) return false;
        escape();
        if (!check(exitOrdersConfirmationVisible && view == View::Event,
                   "Escape from the map must protect unsaved orders just like Back to Events.")) return false;
        click(500.0f, 428.0f);
        if (!check(!exitOrdersConfirmationVisible && view == View::Events && !ordersDirty &&
                       joinedEvent() && plannedOrders.at(900) == 4 && offline(),
                   "Discard & Back must restore the latest submitted orders and return to Events without surrendering.")) return false;

        // A long army previously rendered a seventh row over the primary button.
        // Scrolling must expose every deck while footer actions keep their own hit areas.
        applyCaptureState("conquest-many-decks", library);
        if (!check(armyDeckList().size() == 10 && eventDeckOffset == 0,
                   "The long-army fixture must start with ten reachable army decks.")) return false;
        for (int step = 0; step < 12; ++step)
        {
            handleEvent(sf::Event{sf::Event::MouseWheelScrolled{
                sf::Mouse::Wheel::Vertical, -1.0f,
                window.mapCoordsToPixel({684.0f, 270.0f})}}, window);
        }
        if (!check(eventDeckOffset == armyDeckList().size() - VisibleEventDeckRows,
                   "Army scrolling must reach the last six decks without an inaccessible tail.")) return false;
        deckClick(VisibleEventDeckRows - 1);
        if (!check(selectedEventDeckId && *selectedEventDeckId == 109,
                   "The last army deck must be selectable after scrolling.")) return false;
        const auto lastDeck = selectedEventDeckId;
        click(685.0f, 432.0f);
        if (!check(selectedEventDeckId == lastDeck && !joinConfirmationVisible && offline(),
                   "The primary join control must not be intercepted by a hidden seventh deck row.")) return false;

        // Leave the screenshot on a representative stable outcome instead of
        // the final scroll assertion's partial draft.
        applyCaptureState("conquest-joined", library);
        return check(joinedEvent() && offline(),
                     "The completed interaction capture must remain fully offline and joined.");
    }
