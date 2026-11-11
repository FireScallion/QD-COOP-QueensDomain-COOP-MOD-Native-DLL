namespace Social=qdcoop::steam_social;
int (*inject_steam_callback)(int,void*,unsigned){};
int (*read_social)(Social::Status*,unsigned){};
int (*read_invitation)(Social::Invitation*,unsigned){};
int (*respond)(std::uint64_t,unsigned){};
int (*open_friends)(){};
unsigned (*engine_pump_count)(){};

template<class Test>
bool poll_probe(Test test,unsigned seconds=5) {
    const auto deadline=GetTickCount64()+seconds*1000ull;
    do {
        if(test())return true;
        Sleep(50);
    }while(GetTickCount64()<deadline);
    return false;
}

struct PresenceProbe {
    void* friends{};
    Steam::SteamId self{};
    const char* (*get)(void*,Steam::SteamId,const char*){};
    bool (*set)(void*,const char*,const char*){};

    bool bind() {
        const auto module=GetModuleHandleW(L"steam_api64.dll");
        void *(*get_friends)(){},*(*get_user)(){};
        Steam::SteamId (*steam_id)(void*){};
        if(!module||!load_export(module,"SteamAPI_SteamFriends_v018",get_friends)||
           !load_export(module,"SteamAPI_SteamUser_v023",get_user)||
           !load_export(module,"SteamAPI_ISteamUser_GetSteamID",steam_id)||
           !load_export(module,"SteamAPI_ISteamFriends_GetFriendRichPresence",get)||
           !load_export(module,"SteamAPI_ISteamFriends_SetRichPresence",set))return false;
        friends=get_friends();
        const auto user=get_user();
        return friends&&user&&Steam::user_id(self=steam_id(user));
    }
    std::string value(const char* key) const {
        const auto current=get(friends,self,key);
        return current?current:"";
    }
};

DWORD social_probe(HMODULE module,bool forwarding) {
    const bool controls=load_export(module,"QDNativeReadSteamSocial",read_social)&&
        load_export(module,"QDNativeReadSteamInvitation",read_invitation)&&
        load_export(module,"QDNativeRespondSteamInvitation",respond)&&
        load_export(module,"QDNativeProbeSteamPumpCount",engine_pump_count)&&
        load_export(module,"QDNativeOpenSteamFriends",open_friends);
    Social::Status status{};
    Room::Status idle{},first{},first_closed{},second{},old_first{},old_second{};
    const bool ready=controls&&poll_probe([&]{
        return read_social(&status,sizeof(status))&&status.ready&&status.callbacks==2;
    },15);
    const bool idle_registered=ready&&read_status(&idle,sizeof(idle))&&idle.result==Room::Result::idle&&
        !idle.metadata_found&&!status.published_lobby;
    const bool engine_dispatch=idle_registered&&poll_probe([&]{return engine_pump_count()!=0;});
    const bool overlay_enabled=ready&&status.overlay;
    const bool overlay_guard=ready&&(overlay_enabled||(open_friends()&&poll_probe([&]{
        return read_social(&status,sizeof(status))&&status.action==Social::Action::unavailable;
    })));
    PresenceProbe steam{};
    const bool direct=ready&&steam.bind();
    constexpr char test_key[]="qdcoop_lab_connect";
    constexpr char foreign[]="other-test-owner";
    const auto connect_before=direct?steam.value("connect"):std::string{};
    const auto join_text=[](Steam::SteamId lobby){return std::string(Social::connect_prefix)+std::to_string(lobby);};
    const auto published=[&](Steam::SteamId lobby){
        return read_social(&status,sizeof(status))&&status.published_lobby==lobby&&
            steam.value(test_key)==join_text(lobby);
    };
    const bool first_open=idle_registered&&direct&&steam.value(test_key).empty()&&
        host_room(4,0)&&wait(first,Steam::Phase::waiting,30);
    const bool first_published=first_open&&poll_probe([&]{return published(first.room.lobby);});
    Social::Invitation pending{};
    Social::LobbyJoin own{first.room.lobby,steam.self};
    const bool same_room_ignored=first_published&&inject_steam_callback(Social::lobby_callback,&own,sizeof(own))&&
        read_invitation(&pending,sizeof(pending))&&!pending.ticket;
    const bool first_left=same_room_ignored&&leave_room()&&wait(first_closed,Steam::Phase::closed,10);
    const bool cleared=first_left&&poll_probe([&]{
        return read_social(&status,sizeof(status))&&!status.published_lobby&&steam.value(test_key).empty();
    });
    const bool second_open=cleared&&steam.set(steam.friends,test_key,foreign)&&
        host_room(4,0)&&wait(second,Steam::Phase::waiting,20);
    const bool preserved=second_open&&poll_probe([&]{
        return read_social(&status,sizeof(status))&&!status.published_lobby&&steam.value(test_key)==foreign;
    });
    const bool retried=preserved&&steam.set(steam.friends,test_key,nullptr)&&
        poll_probe([&]{return published(second.room.lobby);});
    Social::LobbyJoin invitation{first.room.lobby,steam.self};
    const bool offered=retried&&inject_steam_callback(Social::lobby_callback,&invitation,sizeof(invitation))&&
        read_invitation(&pending,sizeof(pending))&&pending.ticket&&pending.lobby==first.room.lobby&&
        pending.source==Social::Source::lobby;
    const auto first_ticket=pending.ticket;
    const bool duplicate=offered&&inject_steam_callback(Social::lobby_callback,&invitation,sizeof(invitation))&&
        read_invitation(&pending,sizeof(pending))&&pending.ticket==first_ticket;
    Room::Status still_host{};
    const bool declined=duplicate&&respond(first_ticket,0)&&!respond(first_ticket,1)&&
        read_invitation(&pending,sizeof(pending))&&!pending.ticket&&read_status(&still_host,sizeof(still_host))&&
        still_host.result==Room::Result::running&&still_host.room.lobby==second.room.lobby;
    Social::PresenceJoin rich{steam.self};
    const auto command=join_text(first.room.lobby);
    std::memcpy(rich.connect,command.data(),command.size());
    const bool accepted=declined&&inject_steam_callback(Social::presence_callback,&rich,sizeof(rich))&&
        read_invitation(&pending,sizeof(pending))&&pending.ticket!=first_ticket&&pending.lobby==first.room.lobby&&
        pending.source==Social::Source::presence&&respond(pending.ticket,1);
    const bool switched=accepted&&wait(old_first,Steam::Phase::failed,25)&&old_first.room.enter_response==2;
    const bool left_second=switched&&poll_probe([&]{
        Room::Status closed{};
        return read_status(&closed,sizeof(closed))&&closed.result==Room::Result::closed&&
            read_social(&status,sizeof(status))&&!status.published_lobby&&steam.value(test_key).empty();
    });
    const bool old_room_gone=left_second&&join_room(second.room.lobby)&&
        wait(old_second,Steam::Phase::failed,20)&&old_second.room.enter_response==2;
    const bool preserved_connect=direct&&steam.value("connect")==connect_before;
    const bool passed=forwarding&&idle_registered&&engine_dispatch&&overlay_guard&&first_published&&same_room_ignored&&
        cleared&&preserved&&retried&&declined&&switched&&old_room_gone&&preserved_connect&&
        first.room.members==1&&second.room.members==1&&first.room.limit==4&&second.room.limit==4&&
        first.game_thread&&first.game_thread==second.game_thread&&!GetModuleHandleW(L"UE4SS.dll");
    if(direct&&steam.value(test_key)==foreign)steam.set(steam.friends,test_key,nullptr);
    leave_room();
    wchar_t path[32768]{};
    if(const auto size=GetEnvironmentVariableW(L"QD_BOOT_REPORT",path,32768);size&&size<32768) {
        if(auto file=_wfopen(path,L"wb")) {
            std::fprintf(file,
                "{\"passed\":%u,\"callbacks\":%u,\"idle_registered\":%u,\"overlay_enabled\":%u,"
                "\"overlay_gate\":%u,\"direct_steam\":%u,\"engine_dispatch\":%u,\"first_open\":%u,\"first_published\":%u,"
                "\"current_room_ignored\":%u,\"presence_cleared\":%u,\"second_open\":%u,"
                "\"foreign_preserved\":%u,\"presence_retried\":%u,\"duplicate_coalesced\":%u,"
                "\"declined\":%u,\"accepted\":%u,\"switched\":%u,\"old_room_gone\":%u,"
                "\"real_connect_unchanged\":%u,\"old_first_response\":%u,\"old_second_response\":%u,"
                "\"first_phase\":%u,\"first_failure\":%u,\"engine_pumps\":%u}\n",
                passed,status.callbacks,idle_registered,overlay_enabled,overlay_guard,direct,engine_dispatch,first_open,
                first_published,same_room_ignored,cleared,second_open,preserved,retried,duplicate,
                declined,accepted,switched,old_room_gone,preserved_connect,old_first.room.enter_response,
                old_second.room.enter_response,first.room.phase,first.room.failure,
                engine_pump_count?engine_pump_count():0u);
            std::fclose(file);
        }
    }
    return schedule_unreal_exit(module)?0:1;
}
