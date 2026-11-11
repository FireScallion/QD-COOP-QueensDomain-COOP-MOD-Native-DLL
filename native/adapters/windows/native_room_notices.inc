namespace TitleUi {
enum class RoomAction { joining,joined,failed,left };
struct NoticeMember {
    std::uint64_t id{};
    std::wstring name;
    bool joined{};
};
struct NoticeMessage {std::wstring name;RoomAction action;};
struct RoomNotices {
    static constexpr auto self_id=UINT64_MAX;
    std::vector<NoticeMember> members;
    std::vector<NoticeMessage> messages;
    UE::WeakKey label,root,style;
    ULONGLONG next{},shown_until{};
    bool translated{},shown{},local_leave{};

    void emit(const std::wstring& name,RoomAction action){
        if(messages.size()<16)messages.push_back({name,action});
    }
    void begin(){
        members.clear();
        messages.clear();
        detach();
        shown=local_leave=false;
        next=0;
        if(menu.ui.ready)translated=menu.ui.chinese();
        if(preparing.operation==Room::Operation::join||preparing.operation==Room::Operation::join_lan){
            const auto self=Room::is_lan(preparing.operation)?0:social.account();
            members.push_back({self_id,self?social.room_name(self):translated?L"本机":L"You"});
            emit(members.front().name,RoomAction::joining);
        }
    }
    bool roster(std::vector<NoticeMember>& result,bool zh){
        if(preparing.operation==Room::Operation::host_lan){
            Lan::LocalRoster seats{};
            if(!core.read_lan_roster(&seats,sizeof(seats)))return false;
            for(unsigned i=0;i<seats.size();++i)if(seats[i])
                result.push_back({seats[i],std::wstring(zh?L"局域网玩家 ":L"LAN player ")+std::to_wstring(i+2)});
            return true;
        }
        if(preparing.operation==Room::Operation::join_lan){
            result.push_back({self_id,zh?L"本机":L"You"});
            if(lan_status.phase==Lan::Phase::active||
               std::ranges::find(members,std::uint64_t{1},&NoticeMember::id)!=members.end())
                result.push_back({1,zh?L"局域网房主":L"LAN host"});
            return true;
        }
        Steam::Roster steam;
        const auto self=social.account();
        if(preparing.operation==Room::Operation::join)
            result.push_back({self_id,self?social.room_name(self):zh?L"本机":L"You"});
        if(!status.room.lobby)return true;
        if(!social.room_members(status.room.lobby,steam))return false;
        for(unsigned i=0;i<steam.count;++i)if(steam.members[i]!=self)
            result.push_back({steam.members[i],social.room_name(steam.members[i])});
        return true;
    }
    void track(ULONGLONG now){
        if(now<next)return;
        next=now+160;
        const bool closed=status.result==Room::Result::closed||status.result==Room::Result::rejected;
        if(closed){
            for(const auto& member:members){
                if(local_leave){
                    if(member.id==self_id)emit(member.name,RoomAction::left);
                }else if(member.id!=self_id||!member.joined){
                    emit(member.name,member.joined?RoomAction::left:RoomAction::failed);
                }
            }
            members.clear();
            return;
        }
        if(status.result!=Room::Result::running&&status.result!=Room::Result::preparing)return;
        const bool lan=Room::is_lan(preparing.operation);
        const bool active=(lan?lan_status.phase==Lan::Phase::active:
            status.room.phase==static_cast<unsigned>(Steam::Phase::active))&&
            world_phase.load(std::memory_order_acquire)==World::Phase::active;
        std::vector<NoticeMember> current;
        if(!roster(current,translated)){
            // Transient Steam roster errors should not be shown as departures.
            for(const auto& entry:members)if(std::ranges::find(current,entry.id,&NoticeMember::id)==current.end())
                current.push_back(entry);
        }
        for(auto& member:current){
            const auto old=std::ranges::find(members,member.id,&NoticeMember::id);
            if(old==members.end())emit(member.name,RoomAction::joining);
            else member.joined=old->joined;
            if(!member.joined&&active){
                member.joined=true;
                emit(member.name,RoomAction::joined);
            }
        }
        for(const auto& previous:members)if(std::ranges::find(current,previous.id,&NoticeMember::id)==current.end())
            emit(previous.name,previous.joined?RoomAction::left:RoomAction::failed);
        members=std::move(current);
    }
    void detach(){
        auto& ui=menu.ui;
        if(const auto widget=ui.get(label))event(widget,ui.get(remove_parent),nullptr);
        label=root={};
    }
    Address widget_tree(Address engine){
        auto& ui=menu.ui;
        if(menu.on_title)return ui.object(ui.get(ui.active_title),"WidgetTree");
        const auto instance=ui.object(engine,"GameInstance");
        if(!instance)return 0;
        Address args[]{instance,0,0};
        event(ui.get(gameplay),ui.get(local_pawn),args);
        return ui.object(ui.object(args[2],"HUD"),"WidgetTree");
    }
    Address mount(Address engine){
        auto& ui=menu.ui;
        const auto tree=widget_tree(engine);
        const auto canvas=ui.object(tree,"RootWidget");
        if(!ui.is(canvas,ui.get(canvas_type)))return 0;
        if(ui.get(root)==canvas&&ui.get(label))return ui.get(label);
        detach();
        if(!ui.get(notice_justify)||!ui.get(notice_zorder))ui.step();
        if(!ui.method(notice_justify,1,{{"InJustification","ByteProperty",0,1}})||
           !ui.method(notice_zorder,4,{{"InZOrder","IntProperty",0,4}}))return 0;
        const auto text=ui.spawn(tree,ui.get(text_type));
        if(!text)return 0;
        auto font=ui.get(style);
        if(!font){
            font=ui.load_asset_path("/Game/UI/UI_Style/FontStyles/DefaultTextStyle.DefaultTextStyle_C");
            if(font)ui.remember(style,font);
        }
        if(font)event(text,ui.get(text_style),&font);
        const auto slot=ui.add(canvas,text,add_canvas);
        if(!slot)return 0;
        unsigned char center=1;
        event(text,ui.get(notice_justify),&center);
        float anchors[]{.5f,0,.5f,0},alignment[]{.5f,0},position[]{0,24},size[]{940,48};
        int zorder=200;
        event(slot,ui.get(canvas_anchors),anchors);
        event(slot,ui.get(canvas_alignment),alignment);
        event(slot,ui.get(canvas_position),position);
        event(slot,ui.get(canvas_size),size);
        event(slot,ui.get(notice_zorder),&zorder);
        ui.visibility(text,3);
        ui.remember(label,text);
        ui.remember(root,canvas);
        shown=false;
        return text;
    }
    void tick(Address engine,ULONGLONG now){
        if(messages.empty()&&members.empty()&&status.result!=Room::Result::running&&status.result!=Room::Result::preparing)
            return;
        if(menu.ui.ready&&now>=next)translated=menu.ui.chinese();
        track(now);
        if(messages.empty())return;
        if(shown&&now>=shown_until){
            messages.erase(messages.begin());
            shown=false;
        }
        if(messages.empty()){detach();return;}
        if(!menu.ui.ready)return;
        const auto text=mount(engine);
        if(!text||shown)return;
        constexpr const wchar_t* english[]{L"Joining…",L"Joined",L"Could not join",L"Left"};
        constexpr const wchar_t* chinese[]{L"加入中",L"已加入",L"加入失败",L"离开"};
        const auto& entry=messages.front();
        const auto index=static_cast<unsigned>(entry.action);
        const auto caption=entry.name+L"  ·  "+(translated?chinese[index]:english[index]);
        menu.ui.label(text,caption.c_str());
        shown=true;
        shown_until=now+(entry.action==RoomAction::joining?1000:2800);
    }
};
RoomNotices room_notices;
}
