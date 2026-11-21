// Included inside the card-editor module after its shared controls and endpoint.
class ActivatedAbilityEditor
{
  public:
    explicit ActivatedAbilityEditor(sf::Font &font, CardEditorEndpoint endpoint)
        : font(font), endpoint(std::move(endpoint))
    {
        search = field({42, 166}, {228, 34});
        name = field({350, 166}, {520, 34});
        kind = field({900, 166}, {320, 34});
        range = field({350, 246}, {160, 34});
        strength = field({530, 246}, {160, 34});
        duration = field({710, 246}, {160, 34});
        uses = field({890, 246}, {330, 34});
        summon = field({350, 326}, {400, 34});
        filter = field({780, 326}, {440, 34});
        restriction = field({350, 406}, {400, 34});
        create = EditorButton(font, "New", {42, 690}, {96, 42}, AccentDark);
        refresh = EditorButton(font, "Refresh", {150, 690}, {120, 42}, AccentDark);
        saveButton = EditorButton(font, "Save Ability", {1050, 690}, {170, 42}, AccentDark);
        copyButton = EditorButton(font, "Copy", {920, 690}, {116, 42}, AccentDark);
        removeButton = EditorButton(font, "Delete", {790, 690}, {116, 42}, Warn);
        addState = EditorButton(font, "Add State", {1030, 444}, {190, 32}, AccentDark);
        modalSave = EditorButton(font, "Save", {414, 412}, {136, 42}, AccentDark);
        modalDiscard = EditorButton(font, "Discard", {572, 412}, {136, 42}, Warn);
        modalCancel = EditorButton(font, "Keep Editing", {730, 412}, {154, 42}, AccentDark);
        confirmDelete = EditorButton(font, "Delete", {572, 412}, {136, 42}, Warn);
        draft();
    }
    void setEndpoint(CardEditorEndpoint value)
    {
        endpoint = std::move(value);
    }
    bool hasUnsavedChanges() const
    {
        return snapshot() != saved;
    }
    bool hasModal() const
    {
        return pending != 0;
    }
    const std::string &message() const
    {
        return status;
    }
    void restore()
    {
        if (selected && *selected < definitions.size())
            select(*selected);
        else
            draft();
    }
    std::optional<ability_data::Definition> definitionNamed(const std::string &name) const
    {
        for (const auto &definition : definitions)
            if (definition.name == name)
                return definition;
        return std::nullopt;
    }
    bool consumeLibraryChanges()
    {
        const bool changed = libraryChanges;
        libraryChanges = false;
        return changed;
    }
    void refreshLibrary()
    {
        if (!hasUnsavedChanges())
            load();
    }
    void ensureLoaded()
    {
        if (!loaded)
            load();
    }
    void capture()
    {
        ability_data::Definition heal;
        heal.name = "Repair the Pattern";
        heal.kind = "area heal";
        heal.range = 2;
        heal.strength = 1;
        heal.targetFilter = std::vector<std::string>{"Fey", "Mechanical"};
        heal.states = {{0, 0, "Repair the Pattern"}};
        ability_data::Definition transform;
        transform.name = "Raise Gun";
        transform.kind = "transform";
        transform.states = {{0, 1, "Raise Gun"}, {1, 0, "Lower Gun"}};
        definitions = {heal, transform};
        supported = loaded = true;
        select(0);
        status = "Offline review library";
    }
    bool save()
    {
        if (!supported)
        {
            status = "This server needs activated-ability support before saving";
            return false;
        }
        try
        {
            auto definition = fromForm();
            if (!ability_data::valid(definition, true))
                throw std::runtime_error("Check the kind, nonnegative amounts and state transitions");
            sf::Packet request;
            const bool update = selected && *selected < definitions.size();
            request << static_cast<std::uint8_t>(update ? network::MessageType::ActivatedAbilityUpdateRequest
                                                        : network::MessageType::ActivatedAbilityUpsertRequest);
            if (update)
                request << definitions[*selected].name;
            ability_data::write(request, definition);
            command(request, update ? network::MessageType::ActivatedAbilityUpdateResponse
                                    : network::MessageType::ActivatedAbilityUpsertResponse);
            saved = snapshot();
            const auto target = definition.name;
            load();
            for (std::size_t i = 0; i < definitions.size(); ++i)
                if (definitions[i].name == target)
                    select(i);
            status = "Saved reusable ability";
            return true;
        }
        catch (const std::exception &error)
        {
            status = error.what();
            return false;
        }
    }
    void event(const sf::Event &event, sf::RenderWindow &window)
    {
        layout();
        if (pending)
        {
            if (const auto *click = event.getIf<sf::Event::MouseButtonPressed>();
                click && click->button == sf::Mouse::Button::Left)
            {
                const auto mouse = window.mapPixelToCoords(click->position);
                if (modalCancel.clicked(mouse))
                    pending = 0;
                else if (pending == 4 && confirmDelete.clicked(mouse))
                    executePending();
                else if (pending != 4 && modalDiscard.clicked(mouse))
                {
                    restore();
                    executePending();
                }
                else if (pending != 4 && modalSave.clicked(mouse) && save())
                    executePending();
            }
            if (const auto *key = event.getIf<sf::Event::KeyPressed>(); key && key->code == sf::Keyboard::Key::Escape)
                pending = 0;
            return;
        }
        auto all = fields();
        if (active < all.size())
            all[active]->handleEvent(event, window);
        if (const auto *key = event.getIf<sf::Event::KeyPressed>())
        {
            if (key->code == sf::Keyboard::Key::Tab)
                focus((active + 1) % all.size());
            if (key->code == sf::Keyboard::Key::Enter)
                save();
        }
        if (const auto *wheel = event.getIf<sf::Event::MouseWheelScrolled>())
        {
            const auto mouse = window.mapPixelToCoords(wheel->position);
            if (mouse.x < 300)
                offset = clampedOffset(offset, wheel->delta, visible().size(), 7);
            else
                stateOffset = clampedOffset(stateOffset, wheel->delta, rows.size(), 4);
            layout();
        }
        if (const auto *click = event.getIf<sf::Event::MouseButtonPressed>();
            click && click->button == sf::Mouse::Button::Left)
        {
            const auto mouse = window.mapPixelToCoords(click->position);
            if (create.clicked(mouse))
                request(1);
            else if (refresh.clicked(mouse))
                request(2);
            else if (saveButton.clicked(mouse))
                save();
            else if (copyButton.clicked(mouse))
            {
                name.setValue(uniqueName(name.getValue()));
                selected.reset();
                status = "Draft copy; save to create it";
            }
            else if (removeButton.clicked(mouse))
                request(4);
            else if (addState.clicked(mouse))
            {
                addRow(static_cast<int>(rows.size()), 0, "");
                focus(1);
                stateOffset = rows.size() > 4 ? rows.size() - 4 : 0;
            }
            else
            {
                const auto list = visible();
                bool handled = false;
                for (std::size_t i = offset; i < list.size() && i < offset + 7; ++i)
                    if (sf::FloatRect({42, 222 + static_cast<float>(i - offset) * 56}, {228, 50}).contains(mouse))
                    {
                        target = list[i];
                        request(3);
                        handled = true;
                        break;
                    }
                if (!handled)
                    for (std::size_t i = stateOffset; i < rows.size() && i < stateOffset + 4; ++i)
                        if (rows[i].remove.clicked(mouse))
                        {
                            rows.erase(rows.begin() + i);
                            focus(1);
                            stateOffset = std::min(stateOffset, rows.size() > 4 ? rows.size() - 4 : 0);
                            handled = true;
                            break;
                        }
                if (!handled)
                {
                    all = fields();
                    for (std::size_t i = 0; i < all.size(); ++i)
                        if (all[i]->contains(mouse))
                        {
                            focus(i);
                            all[i]->beginMouseSelection(mouse, false);
                            break;
                        }
                }
            }
        }
    }
    void update(sf::Vector2f mouse, float delta)
    {
        layout();
        saveButton.setEnabled(supported);
        removeButton.setEnabled(supported && selected.has_value());
        for (auto *field : fields())
            field->update(mouse);
        for (auto *button : {&create, &refresh, &saveButton, &copyButton, &removeButton, &addState, &modalSave,
                             &modalDiscard, &modalCancel, &confirmDelete})
            button->update(mouse);
        for (auto &row : rows)
            row.remove.update(mouse);
    }
    void draw(sf::RenderWindow &window)
    {
        layout();
        drawRoundedPanel(window, {24, 100}, {276, 640}, Panel);
        drawRoundedPanel(window, {318, 100}, {938, 640}, Panel);
        drawText(window, font, "ACTIVATED ABILITIES", 18, {42, 122}, Ink);
        drawText(window, font, "Reusable definitions", 17, {350, 119}, Ink);
        drawText(window, font, "Changes affect every assigned card", 14, {780, 119}, Muted);
        drawText(window, font, "Search", 12, {42, 147}, Muted);
        search.draw(window);
        const auto list = visible();
        for (std::size_t i = offset; i < list.size() && i < offset + 7; ++i)
        {
            const auto index = list[i];
            const float y = 222 + static_cast<float>(i - offset) * 56;
            sf::RectangleShape row({228, 50});
            row.setPosition({42, y});
            row.setFillColor(selected == index ? AccentDark : PanelAlt);
            window.draw(row);
            drawText(window, font, elideToWidth(font, definitions[index].name, 16, 212), 16, {50, y + 5}, Ink);
            drawText(window, font, definitions[index].kind, 13, {50, y + 29}, Muted);
        }
        auto labeled = [&](InputBox &field, const char *label, float x, float y) {
            drawText(window, font, label, 13, {x, y - 24}, Muted);
            field.draw(window);
        };
        labeled(name, "NAME", 350, 166);
        labeled(kind, "KIND", 900, 166);
        labeled(range, "RANGE (blank: default)", 350, 246);
        labeled(strength, "STRENGTH", 530, 246);
        labeled(duration, "DURATION", 710, 246);
        labeled(uses, "INITIAL USES (Dig; -1: unlimited)", 890, 246);
        labeled(summon, "SUMMON CARD", 350, 326);
        labeled(filter, "HEAL FILTER (*: any; blank: none; comma separated)", 780, 326);
        labeled(restriction, "COMMAND TRAIT / KEYWORD (*: any; blank: none)", 350, 406);
        drawText(window, font, "Save a definition, then enter its name in a card's ability reference.", 14, {780, 390},
                 Muted);
        drawText(window, font, "STATE", 13, {350, 450}, Muted);
        drawText(window, font, "NEXT STATE", 13, {465, 450}, Muted);
        drawText(window, font, "BUTTON LABEL", 13, {600, 450}, Muted);
        for (std::size_t i = stateOffset; i < rows.size() && i < stateOffset + 4; ++i)
        {
            rows[i].state.draw(window);
            rows[i].next.draw(window);
            rows[i].label.draw(window);
            rows[i].remove.draw(window);
        }
        drawText(window, font,
                 "Kinds: area heal, command, corrupt the ground, dematerialize, dig, power boost, raise undead, shriek, silence, summon, "
                 "swap, transform, weaken",
                 12, {350, 662}, Muted);
        for (auto *button : {&create, &refresh, &saveButton, &copyButton, &removeButton, &addState})
            button->draw(window);
        drawText(window, font, elideToWidth(font, status, 14, 430), 14, {350, 704}, Muted);
        if (pending)
        {
            sf::RectangleShape shade({EditorWidth, EditorHeight});
            shade.setFillColor(sf::Color(0, 0, 0, 185));
            window.draw(shade);
            drawRoundedPanel(window, {380, 292}, {540, 190}, Panel);
            drawText(window, font, pending == 4 ? "Delete this reusable ability?" : "Save ability changes?", 23,
                     {414, 318}, Ink);
            drawText(window, font,
                     pending == 4 ? "Assigned abilities cannot be deleted." : "Choose Save, Discard, or Keep Editing.",
                     15, {414, 361}, Muted);
            if (pending == 4)
                confirmDelete.draw(window);
            else
            {
                modalSave.draw(window);
                modalDiscard.draw(window);
            }
            modalCancel.draw(window);
        }
    }

    bool runCaptureChecks(sf::RenderWindow &window, std::string &failure)
    {
        auto check = [&](bool result, const char *message) {
            if (!result)
                failure = message;
            return result;
        };
        capture();
        const auto original = name.getValue();
        event(sf::Event{sf::Event::TextEntered{'X'}}, window);
        if (!check(name.getValue() == original + "X" && hasUnsavedChanges(),
                   "Ability text edits must mark the definition dirty"))
            return false;
        request(1);
        if (!check(hasModal(), "Selecting a new ability must guard unsaved edits"))
            return false;
        sf::Event::KeyPressed escape{};
        escape.code = sf::Keyboard::Key::Escape;
        event(sf::Event{escape}, window);
        if (!check(!hasModal() && name.getValue() == original + "X", "Cancel must preserve ability edits"))
            return false;
        restore();
        filter.setValue("");
        if (!check(fromForm().targetFilter && fromForm().targetFilter->empty(),
                   "Blank native filter must mean no matches"))
            return false;
        filter.setValue("*");
        if (!check(!fromForm().targetFilter, "Wildcard native filter must remain unrestricted"))
            return false;
        select(1);
        if (!check(fromForm().states.size() == 2 && fromForm().states[1].nextState == 0,
                   "Editor must preserve a return transition from the alternate state"))
            return false;
        addRow(1, 0, "duplicate");
        if (!check(!ability_data::valid(fromForm()), "Editor must reject duplicate ability state IDs"))
            return false;
        restore();
        return check(!hasUnsavedChanges(), "Discard must restore the saved reusable definition");
    }

  private:
    struct Row
    {
        InputBox state, next, label;
        EditorButton remove;
    };
    sf::Font &font;
    CardEditorEndpoint endpoint;
    InputBox search, name, kind, range, strength, duration, uses, summon, filter, restriction;
    std::vector<Row> rows;
    std::vector<ability_data::Definition> definitions;
    EditorButton create, refresh, saveButton, copyButton, removeButton, addState, modalSave, modalDiscard, modalCancel,
        confirmDelete;
    std::optional<std::size_t> selected;
    std::size_t active = 1, offset = 0, stateOffset = 0, target = 0;
    std::string status = "Ready", saved;
    int pending = 0;
    bool loaded = false, supported = false, libraryChanges = false;
    InputBox field(sf::Vector2f position, sf::Vector2f size)
    {
        InputBox box(font, "", position, size);
        return box;
    }
    std::vector<InputBox *> fields()
    {
        std::vector<InputBox *> result = {&search,   &name, &kind,   &range,  &strength,
                                          &duration, &uses, &summon, &filter, &restriction};
        for (auto &row : rows)
        {
            result.push_back(&row.state);
            result.push_back(&row.next);
            result.push_back(&row.label);
        }
        return result;
    }
    void focus(std::size_t index)
    {
        auto all = fields();
        active = std::min(index, all.size() - 1);
        for (std::size_t i = 0; i < all.size(); ++i)
            all[i]->setActive(i == active);
        if (active >= 10)
        {
            const auto row = (active - 10) / 3;
            if (row < stateOffset)
                stateOffset = row;
            else if (row >= stateOffset + 4)
                stateOffset = row - 3;
        }
    }
    static std::size_t clampedOffset(std::size_t offset, float delta, std::size_t count, std::size_t visible)
    {
        return static_cast<std::size_t>(std::clamp(static_cast<int>(offset) + (delta < 0 ? 1 : -1), 0,
                                                   static_cast<int>(count > visible ? count - visible : 0)));
    }
    void layout()
    {
        const auto count = visible().size();
        offset = std::min(offset, count > 7 ? count - 7 : 0);
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const float y =
                i >= stateOffset && i < stateOffset + 4 ? 486 + static_cast<float>(i - stateOffset) * 42 : -2000;
            rows[i].state.setPosition({350, y});
            rows[i].next.setPosition({465, y});
            rows[i].label.setPosition({600, y});
            rows[i].remove.setPosition({1178, y});
        }
    }
    std::vector<std::size_t> visible() const
    {
        std::vector<std::size_t> result;
        const auto query = lowerKey(search.getValue());
        for (std::size_t i = 0; i < definitions.size(); ++i)
            if (lowerKey(definitions[i].name + " " + definitions[i].kind).find(query) != std::string::npos)
                result.push_back(i);
        return result;
    }
    std::string snapshot() const
    {
        std::string result;
        for (const auto *item : {&name, &kind, &range, &strength, &duration, &uses, &summon, &filter, &restriction})
            result += std::to_string(item->getValue().size()) + ":" + item->getValue();
        for (const auto &row : rows)
            for (const auto *item : {&row.state, &row.next, &row.label})
                result += std::to_string(item->getValue().size()) + ":" + item->getValue();
        return result;
    }
    static int number(const std::string &text)
    {
        std::size_t end = 0;
        const auto value = trim(text);
        const int n = std::stoi(value, &end);
        if (end != value.size())
            throw std::runtime_error("Enter whole numbers for amounts and states");
        return n;
    }
    ability_data::Definition fromForm() const
    {
        ability_data::Definition d;
        d.name = trim(name.getValue());
        d.kind = lowerKey(trim(kind.getValue()));
        if (!trim(range.getValue()).empty())
            d.range = number(range.getValue());
        if (!trim(strength.getValue()).empty())
            d.strength = number(strength.getValue());
        if (!trim(duration.getValue()).empty())
            d.duration = number(duration.getValue());
        d.initialUses = number(uses.getValue());
        d.summonTitle = trim(summon.getValue());
        if (trim(filter.getValue()) != "*")
        {
            d.targetFilter.emplace();
            std::string values = filter.getValue();
            std::size_t from = 0;
            while (from <= values.size())
            {
                const auto end = values.find(',', from);
                auto value = trim(values.substr(from, end == std::string::npos ? end : end - from));
                if (!value.empty())
                    d.targetFilter->push_back(value);
                if (end == std::string::npos)
                    break;
                from = end + 1;
            }
        }
        if (trim(restriction.getValue()) != "*")
            d.commandRestriction = trim(restriction.getValue());
        for (const auto &row : rows)
            d.states.push_back({number(row.state.getValue()), number(row.next.getValue()), row.label.getValue()});
        return d;
    }
    void addRow(int state, int next, const std::string &label)
    {
        Row row{field({0, 0}, {92, 32}), field({0, 0}, {112, 32}), field({0, 0}, {558, 32}),
                EditorButton(font, "X", {0, 0}, {36, 32}, Warn)};
        row.state.setValue(std::to_string(state));
        row.next.setValue(std::to_string(next));
        row.label.setValue(label);
        rows.push_back(std::move(row));
    }
    void draft()
    {
        selected.reset();
        name.setValue("");
        kind.setValue("area heal");
        range.setValue("");
        strength.setValue("");
        duration.setValue("");
        uses.setValue("0");
        summon.setValue("");
        filter.setValue("*");
        restriction.setValue("*");
        rows.clear();
        addRow(0, 0, "");
        stateOffset = 0;
        focus(1);
        saved = snapshot();
        status = "Draft ability";
    }
    void select(std::size_t i)
    {
        if (i >= definitions.size())
            return;
        selected = i;
        const auto &d = definitions[i];
        name.setValue(d.name);
        kind.setValue(d.kind);
        range.setValue(d.range ? std::to_string(*d.range) : "");
        strength.setValue(d.strength ? std::to_string(*d.strength) : "");
        duration.setValue(d.duration ? std::to_string(*d.duration) : "");
        uses.setValue(std::to_string(d.initialUses));
        summon.setValue(d.summonTitle);
        filter.setValue(d.targetFilter ? joinStrings(*d.targetFilter, ", ") : "*");
        restriction.setValue(d.commandRestriction.value_or("*"));
        rows.clear();
        for (const auto &state : d.states)
            addRow(state.state, state.nextState, state.label);
        stateOffset = 0;
        focus(1);
        saved = snapshot();
    }
    std::string uniqueName(const std::string &base) const
    {
        std::set<std::string> names;
        for (const auto &d : definitions)
            names.insert(d.name);
        return uniqueCopyName(base, names);
    }
    sf::Packet exchange(sf::Packet &request)
    {
        bayou::tls::Socket socket;
        const auto address = sf::IpAddress::resolve(endpoint.host);
        if (!address)
            throw std::runtime_error("Cannot resolve card server");
        socket.setServerName(endpoint.host);
        if (socket.connect(*address, endpoint.port) != sf::Socket::Status::Done ||
            socket.send(request) != sf::Socket::Status::Done)
            throw std::runtime_error("Cannot reach card server");
        sf::Packet response;
        if (socket.receive(response) != sf::Socket::Status::Done)
            throw std::runtime_error("No card server response");
        return response;
    }
    void command(sf::Packet &request, network::MessageType expected)
    {
        auto response = exchange(request);
        std::uint8_t type = 0;
        bool ok = false;
        std::string message;
        response >> type >> ok >> message;
        if (!response || type != static_cast<std::uint8_t>(expected) || !ok)
            throw std::runtime_error(message.empty() ? "Unsupported card server response" : message);
    }
    void load()
    {
        try
        {
            sf::Packet request;
            request << static_cast<std::uint8_t>(network::MessageType::ActivatedAbilityListRequest);
            auto response = exchange(request);
            std::uint8_t type = 0;
            bool ok = false;
            std::string message;
            std::uint32_t count = 0;
            response >> type >> ok >> message >> count;
            if (!response || !ok ||
                type != static_cast<std::uint8_t>(network::MessageType::ActivatedAbilityListResponse) ||
                count > card_data::MaxSerializedItems)
                throw std::runtime_error("This server does not support activated-ability objects yet");
            std::vector<ability_data::Definition> loadedDefinitions;
            for (std::uint32_t i = 0; i < count; ++i)
            {
                ability_data::Definition d;
                if (!ability_data::read(response, d))
                    throw std::runtime_error("Invalid ability library payload");
                loadedDefinitions.push_back(std::move(d));
            }
            if (!response.endOfPacket())
                throw std::runtime_error("Unexpected ability library data");
            if (loaded && definitions != loadedDefinitions)
                libraryChanges = true;
            definitions = std::move(loadedDefinitions);
            supported = loaded = true;
            offset = 0;
            if (definitions.empty())
                draft();
            else
                select(0);
            status = "Loaded reusable definitions";
        }
        catch (const std::exception &error)
        {
            supported = false;
            status = error.what();
        }
    }
    void request(int operation)
    {
        if (operation == 4 || hasUnsavedChanges())
            pending = operation;
        else
        {
            pending = operation;
            executePending();
        }
    }
    void executePending()
    {
        const int operation = pending;
        pending = 0;
        if (operation == 1)
            draft();
        else if (operation == 2)
            load();
        else if (operation == 3)
            select(target);
        else if (operation == 4 && selected)
        {
            try
            {
                sf::Packet request;
                request << static_cast<std::uint8_t>(network::MessageType::ActivatedAbilityDeleteRequest)
                        << definitions[*selected].name;
                command(request, network::MessageType::ActivatedAbilityDeleteResponse);
                load();
                status = "Deleted ability";
            }
            catch (const std::exception &error)
            {
                status = error.what();
            }
        }
    }
};
