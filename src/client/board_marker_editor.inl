// Included by the card editor module after its controls and endpoint.
class BoardMarkerEditor
{
public:
    BoardMarkerEditor(sf::Font& font, CardEditorEndpoint endpoint)
        : font(font), endpoint(std::move(endpoint)),
          eligibility(font, "Friendly", {350,246}, {245,40}, AccentDark),
          effect(font, "Heal", {625,246}, {245,40}, AccentDark),
          create(font,"New",{42,690},{100,42},AccentDark),
          refresh(font,"Refresh",{154,690},{112,42},AccentDark),
          remove(font,"Delete",{780,690},{145,42},Warn),
          saveButton(font,"Save Marker",{1045,690},{175,42},AccentDark)
    {
        name = InputBox(font, "", {350,166}, {520,34});
        strength = InputBox(font, "", {900,246}, {220,40});
        restore();
    }
    void setEndpoint(CardEditorEndpoint value) { endpoint=std::move(value); loaded=false; }
    bool hasUnsavedChanges() const { return form() != saved; }
    const std::string& message() const { return status; }
    const std::vector<marker_data::Definition>& library() const { return definitions; }
    bool consumeLibraryChanges() { return std::exchange(changed, false); }
    bool hasModal() const { return deleting; }
    void restore() { select(selected); }
    void ensureLoaded() { if (!loaded) load(); }
    void invalidate() { loaded=false; }
    void capture()
    {
        definitions={{"Clover",marker_data::Eligibility::Friendly,marker_data::Effect::Heal,1},
                     {"Briar",marker_data::Eligibility::Enemy,marker_data::Effect::Damage,2}};
        loaded=true;
        select(0);
        status="Offline review library";
    }
    bool save()
    {
        try
        {
            const auto d = form();
            if (!marker_data::valid(d)) throw std::runtime_error("Enter a valid name and positive whole-number strength.");
            const bool updating = selected && *selected < definitions.size();
            if (!updating && std::any_of(definitions.begin(),definitions.end(),[&](const auto& item) {
                    return marker_data::nameKey(item.name)==marker_data::nameKey(d.name);
                })) throw std::runtime_error("A board marker with that name already exists.");
            sf::Packet request;
            request << static_cast<std::uint8_t>(updating ? network::MessageType::BoardMarkerUpdateRequest
                                                          : network::MessageType::BoardMarkerUpsertRequest);
            if (updating) request << definitions[*selected].name;
            marker_data::write(request,d);
            command(request,updating ? network::MessageType::BoardMarkerUpdateResponse
                                     : network::MessageType::BoardMarkerUpsertResponse);
            changed=true;
            if (updating) definitions[*selected]=d;
            else { definitions.push_back(d); selected=definitions.size()-1; }
            saved=d;
            load(d.name);
            status="Board marker saved.";
            return true;
        }
        catch (const std::exception& e) { status=e.what(); return false; }
    }
    void event(const sf::Event& event, sf::RenderWindow& window)
    {
        if (const auto* wheel=event.getIf<sf::Event::MouseWheelScrolled>())
        {
            if (!deleting) offset=std::clamp(offset-static_cast<int>(wheel->delta),0,std::max(0,static_cast<int>(definitions.size())-10));
            return;
        }
        if (const auto* key=event.getIf<sf::Event::KeyPressed>(); key && deleting)
        {
            if (key->code==sf::Keyboard::Key::Escape) deleting=false;
            else if (key->code==sf::Keyboard::Key::Enter) deleteSelected();
            return;
        }
        if (const auto* click=event.getIf<sf::Event::MouseButtonPressed>(); click && click->button==sf::Mouse::Button::Left)
        {
            const auto mouse=window.mapPixelToCoords(click->position);
            if (deleting)
            {
                if (sf::FloatRect({480,400},{145,42}).contains(mouse)) deleteSelected();
                else if (sf::FloatRect({650,400},{145,42}).contains(mouse)) deleting=false;
                return;
            }
            if (saveButton.clicked(mouse)) { save(); return; }
            if (remove.clicked(mouse) && selected)
            {
                if (hasUnsavedChanges()) status="Save or restore this draft before deleting.";
                else deleting=true;
                return;
            }
            if (eligibility.clicked(mouse)) { side=(side+1)%3; return; }
            if (effect.clicked(mouse)) { mode=(mode+1)%2; return; }
            if (create.clicked(mouse) || refresh.clicked(mouse) ||
                sf::FloatRect({42,166},{224,500}).contains(mouse))
            {
                if (hasUnsavedChanges()) { status="Save or restore this draft before switching."; return; }
                if (create.clicked(mouse)) select(std::nullopt);
                else if (refresh.clicked(mouse)) load(selected && *selected<definitions.size() ? definitions[*selected].name : "");
                else
                {
                    const auto index=static_cast<std::size_t>(offset+static_cast<int>((mouse.y-166)/50));
                    if (index<definitions.size()) select(index);
                }
                return;
            }
            name.setActive(name.contains(mouse));
            strength.setActive(strength.contains(mouse));
            return;
        }
        for (InputBox* field : {&name,&strength})
            if (field->isActive()) field->handleEvent(event,window);
    }
    void update(sf::Vector2f mouse, float dt)
    {
        for (auto* button : {&eligibility,&effect,&create,&refresh,&remove,&saveButton}) button->update(mouse);
        name.update(mouse); strength.update(mouse);
        name.updateCursor(dt); strength.updateCursor(dt);
    }
    void draw(sf::RenderWindow& window)
    {
        drawRoundedPanel(window, {24,100}, {276,640}, Panel);
        drawRoundedPanel(window, {318,100}, {938,640}, Panel);
        drawText(window,font,"BOARD MARKERS",18,{42,122},Ink);
        drawText(window,font,"Reusable definitions",19,{350,115},Ink);
        drawText(window,font,"Use a saved marker in a Traces card.",14,{595,119},Muted);
        for (int i=0;i<10 && offset+i<static_cast<int>(definitions.size());++i)
        {
            const auto index=static_cast<std::size_t>(offset+i);
            const float y=166.0f+50.0f*i;
            EditorButton row(font,elideToWidth(font,definitions[index].name,14,205),{42,y},{224,42},selected==index?Warn:AccentDark);
            row.draw(window);
        }
        static constexpr const char* sides[]={"Friendly","Enemy","Any"};
        static constexpr const char* effects[]={"Heal","Damage"};
        drawText(window,font,"NAME",13,{350,146},Muted);
        drawText(window,font,"ELIGIBILITY (click to cycle)",13,{350,226},Muted);
        drawText(window,font,"EFFECT (click to cycle)",13,{625,226},Muted);
        drawText(window,font,"STRENGTH",13,{900,226},Muted);
        name.draw(window);
        strength.draw(window);
        EditorButton sideValue(font,sides[side],{350,246},{245,40},AccentDark);
        EditorButton effectValue(font,effects[mode],{625,246},{245,40},AccentDark);
        sideValue.draw(window); effectValue.draw(window);
        for (auto* button : {&create,&refresh,&remove,&saveButton}) button->draw(window);
        drawText(window,font,elideToWidth(font,status,14,411),14,{345,704},Muted);
        if (deleting)
        {
            sf::RectangleShape overlay({1280,760}); overlay.setFillColor(sf::Color(0,0,0,185)); window.draw(overlay);
            sf::RectangleShape modal({540,145}); modal.setPosition({370,320});modal.setFillColor(AccentDark); window.draw(modal);
            drawText(window,font,"Delete this board marker?",22,{455,344},Ink);
            EditorButton yes(font,"Delete",{480,400},{145,42},Warn), no(font,"Cancel",{650,400},{145,42},AccentDark);
            yes.draw(window);no.draw(window);
        }
    }
private:
    sf::Font& font;
    CardEditorEndpoint endpoint;
    std::vector<marker_data::Definition> definitions;
    std::optional<std::size_t> selected;
    marker_data::Definition saved;
    InputBox name,strength;
    EditorButton eligibility,effect,create,refresh,remove,saveButton;
    int offset=0,side=0,mode=0;
    bool loaded=false,changed=false,deleting=false;
    std::string status="Ready";
    marker_data::Definition form() const
    {
        int value=0;
        try
        {
            const auto clean=trim(strength.getValue());
            std::size_t consumed=0;
            value=std::stoi(clean,&consumed);
            if (consumed!=clean.size()) value=0;
        }
        catch (const std::exception&) {}
        static constexpr marker_data::Eligibility sides[]={marker_data::Eligibility::Friendly,marker_data::Eligibility::Enemy,marker_data::Eligibility::Any};
        return {trim(name.getValue()),sides[side],mode==0?marker_data::Effect::Heal:marker_data::Effect::Damage,value};
    }
    void select(std::optional<std::size_t> index)
    {
        selected=index;
        saved=index && *index<definitions.size()?definitions[*index]:marker_data::Definition{};
        name.setValue(saved.name);
        side=saved.eligibility==marker_data::Eligibility::Friendly?0:saved.eligibility==marker_data::Eligibility::Enemy?1:2;
        mode=saved.effect==marker_data::Effect::Heal?0:1;
        strength.setValue(std::to_string(saved.strength));
        name.setActive(false);
        strength.setActive(false);
    }
    sf::Packet exchange(sf::Packet& request)
    {
        bayou::tls::Socket socket;
        const auto address=sf::IpAddress::resolve(endpoint.host);
        if (!address) throw std::runtime_error("Cannot resolve card server.");
        socket.setServerName(endpoint.host);
        if (socket.connect(*address,endpoint.port)!=sf::Socket::Status::Done || socket.send(request)!=sf::Socket::Status::Done)
            throw std::runtime_error("Cannot reach card server.");
        sf::Packet response;
        if (socket.receive(response)!=sf::Socket::Status::Done) throw std::runtime_error("No card server response.");
        return response;
    }
    void checkResponse(sf::Packet& response,network::MessageType expected)
    {
        std::uint8_t type=0;
        bool ok=false;
        std::string detail;
        response >> type >> ok >> detail;
        if (!response || type!=static_cast<std::uint8_t>(expected) || !ok)
            throw std::runtime_error(detail.empty()?"Server does not support board markers.":detail);
    }
    void command(sf::Packet& request,network::MessageType expected)
    { auto response=exchange(request); checkResponse(response,expected); }
    void load(const std::string& nameToKeep="")
    {
        try
        {
            sf::Packet request;
            request << static_cast<std::uint8_t>(network::MessageType::BoardMarkerListRequest);
            auto response=exchange(request);
            checkResponse(response,network::MessageType::BoardMarkerListResponse);
            std::uint32_t count=0;
            response >> count;
            if (!response || count>card_data::MaxSerializedItems) throw std::runtime_error("Invalid marker library size.");
            std::vector<marker_data::Definition> next;
            for (std::uint32_t i=0;i<count;++i)
            {
                marker_data::Definition d;
                if (!marker_data::read(response,d)) throw std::runtime_error("Invalid marker definition.");
                next.push_back(std::move(d));
            }
            if (!response.endOfPacket()) throw std::runtime_error("Unexpected marker library data.");
            definitions=std::move(next);
            loaded=true;
            offset=0;
            const auto found=std::find_if(definitions.begin(),definitions.end(),[&](const auto& d){ return d.name==nameToKeep; });
            select(found==definitions.end()?std::nullopt:std::optional<std::size_t>(found-definitions.begin()));
            status=fmt::format("{} board markers",definitions.size());
        }
        catch (const std::exception& e) { status=e.what(); }
    }
    void deleteSelected()
    {
        deleting=false;
        if (!selected || *selected>=definitions.size()) return;
        try
        {
            sf::Packet request;
            request << static_cast<std::uint8_t>(network::MessageType::BoardMarkerDeleteRequest) << definitions[*selected].name;
            command(request,network::MessageType::BoardMarkerDeleteResponse);
            definitions.erase(definitions.begin()+static_cast<std::ptrdiff_t>(*selected));
            changed=true;
            select(std::nullopt);
            load();
            status="Board marker deleted.";
        }
        catch (const std::exception& e) { status=e.what(); }
    }
};
