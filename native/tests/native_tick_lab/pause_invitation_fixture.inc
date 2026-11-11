namespace InvitationFixture {
constexpr std::string_view close_name="BndEvt__OptionsMenu_modalCancelButton_K2Node_ComponentBoundEvent_0_CommonButtonBaseClicked__DelegateSignature";
constexpr std::string_view confirm_name="BndEvt__OptionsMenu_ModalConfirmButton_K2Node_ComponentBoundEvent_1_CommonButtonBaseClicked__DelegateSignature";
constexpr std::string_view pause_owner="/Game/UI/Menus/PauseMenu.PauseMenu_C";
constexpr std::string_view exit_cutscene_path="/Game/Blueprints/FS_GameInstance.FS_GameInstance_C.ExitCutsceneMode";
UE::WeakKey pause{},close_modal{},confirm_exit{},exit_cutscene{};
Steam::SteamId closed_lobby{},current_lobby{};
std::uint64_t canceled_ticket{},accepted_ticket{};
PresenceProbe steam;
unsigned (*probe_pause)(){};
unsigned stage{};
unsigned diagnostics{};
unsigned return_result{},return_enter{},return_pending{};
std::wstring return_level;
bool stock_modal{},custom_question{},pending_seen{};
bool canceled_room_intact{},confirmed_title{},join_failed_safely{},modal_scoped{};
bool cutscene_guarded{},ended_cutscene{};
ULONGLONG guarded_since{},confirm_ready{};

void progress() {
    static unsigned previous_stage=UINT_MAX,previous_diagnostics=UINT_MAX;
    if(stage==previous_stage&&diagnostics==previous_diagnostics)return;
    previous_stage=stage;
    previous_diagnostics=diagnostics;
    wchar_t path[32768]{};
    const auto size=GetEnvironmentVariableW(L"QD_BOOT_REPORT",path,32768);
    if(!size||size>=32750)return;
    std::wcscpy(path+size,L".progress");
    if(auto file=_wfopen(path,L"wb")){
        std::fprintf(file,"stage=%u native=%u modal=%u prompt=%u pending=%u\n",
                     stage,diagnostics,stock_modal,custom_question,pending_seen);
        std::fclose(file);
    }
}
void scan_pause(){
    const auto array=at<UE::ObjectArray>(image+TitleUi::object_array,0);
    if(!UE::object_array_shape(array))return;
    for(unsigned i=0;i<static_cast<unsigned>(array.num_elements);++i){
        const auto chunk=at<Address>(array.chunks,(i/UE::objects_per_chunk)*8);
        if(!chunk)continue;
        const auto item=at<UE::ObjectItem>(chunk,(i%UE::objects_per_chunk)*sizeof(UE::ObjectItem));
        if(!item.object||(item.flags&0x30000000u))continue;
        const auto type=at<Address>(item.object,16);
        const auto kind=leaf(type);
        if(kind=="PauseMenu_C"&&!(at<unsigned>(item.object,8)&0x10u)&&ui.shown(item.object))ui.remember(pause,item.object);
        if(kind!="Function")continue;
        const auto name=leaf(item.object);
        if(name==close_name&&path_is(at<Address>(item.object,32),pause_owner))ui.remember(close_modal,item.object);
        if(name==confirm_name&&path_is(at<Address>(item.object,32),pause_owner))ui.remember(confirm_exit,item.object);
        if(name=="ExitCutsceneMode"&&path_is(item.object,exit_cutscene_path))ui.remember(exit_cutscene,item.object);
    }
}
bool modal_open(Address menu){
    const auto field=ui.member(menu,"ModalOpen","BoolProperty",1);
    return field&&UE::get_bool(reinterpret_cast<void*>(field),reinterpret_cast<const void*>(menu),0);
}
bool controls(){
    const auto module=GetModuleHandleW(L"QDNativeBootstrap.dll");
    return module&&load_export(module,"QDNativeProbeSteamCallback",inject_steam_callback)&&
        load_export(module,"QDNativeProbePauseInvitation",probe_pause)&&
        load_export(module,"QDNativeReadSteamInvitation",read_invitation)&&
        load_export(module,"QDNativeReadSteamSocial",read_social)&&
        load_export(module,"QDNativeHostRoom",host_room)&&steam.bind();
}
bool inject(){
    Social::LobbyJoin invitation{closed_lobby,steam.self};
    return inject_steam_callback(Social::lobby_callback,&invitation,sizeof(invitation))!=0;
}
bool waiting(const Room::Status& room){
    return room.result==Room::Result::running&&room.room.phase==static_cast<unsigned>(Steam::Phase::waiting)&&
        room.room.limit==4&&room.room.members==1&&real_waiting_epoch(room);
}
bool tick(Address instance,const Room::Status& room){
    progress();
    if(stage==0){
        if(!waiting(room)||ui.title_level(instance)||!controls())return false;
        Social::Status status;
        if(!read_social(&status,sizeof(status))||!status.ready||status.callbacks!=2)return false;
        closed_lobby=room.room.lobby;
        if(!leave_room())return false;
    }else if(stage==1){
        if(room.result!=Room::Result::closed||room.room.left!=1||!host_room(4,0))return false;
    }else if(stage==2){
        if(!waiting(room)||room.room.lobby==closed_lobby)return false;
        current_lobby=room.room.lobby;
        if(!inject())return false;
    }else if(stage==3||stage==5){
        diagnostics=probe_pause();
        Social::Invitation pending;
        if(!read_invitation(&pending,sizeof(pending))||!pending.ticket||pending.lobby!=closed_lobby)return false;
        pending_seen=true;
        auto menu=ui.get(pause);
        if(!menu||!ui.shown(menu)||!ui.get(close_modal)||!ui.get(confirm_exit)){
            scan_pause();
            menu=ui.get(pause);
        }
        const auto question=ui.object(menu,"ConfirmDialogueText");
        stock_modal=menu&&ui.shown(menu)&&modal_open(menu);
        custom_question=text_is(question,L"Return to the title and join your friend? Unsaved progress may be lost.");
        const auto cutscene=ui.member(instance,"InCutscene","BoolProperty",1);
        if(stage==3&&!ended_cutscene&&cutscene&&UE::get_bool(reinterpret_cast<void*>(cutscene),reinterpret_cast<const void*>(instance),0)){
            if(stock_modal||custom_question||!(diagnostics&128u))return false;
            const auto now=GetTickCount64();
            if(!guarded_since)guarded_since=now;
            if(now-guarded_since<800)return false;
            if(!ui.get(exit_cutscene))scan_pause();
            const auto finish=ui.get(exit_cutscene);
            if(!ui.method(finish,0,{})||!ui.is(instance,at<Address>(finish,32)))return false;
            cutscene_guarded=true;
            TitleUi::event(instance,finish,nullptr);
            ended_cutscene=true;
            return false;
        }
        if(!stock_modal||!custom_question)return false;
        if(stage==5){
            const auto now=GetTickCount64();
            if(!confirm_ready)confirm_ready=now+950;
            if(now<confirm_ready)return false;
        }
        auto choice=ui.object(menu,stage==3?"ExitCancelButton":"ExitConfirmButton");
        const auto fn=ui.get(stage==3?close_modal:confirm_exit);
        if(!ui.is(choice,ui.get(TitleUi::button_type))||!ui.method(fn,8,{{"Button","ObjectProperty",0,8}}))return false;
        modal_scoped=true;
        if(stage==3)canceled_ticket=pending.ticket;
        else{
            if(pending.ticket==canceled_ticket)return false;
            accepted_ticket=pending.ticket;
        }
        TitleUi::event(choice,ui.get(TitleUi::click),nullptr);
        TitleUi::event(menu,fn,&choice);
    }else if(stage==4){
        Social::Invitation pending;
        if(!read_invitation(&pending,sizeof(pending))||pending.ticket||modal_open(ui.get(pause))||
           !waiting(room)||room.room.lobby!=current_lobby||ui.title_level(instance))return false;
        canceled_room_intact=true;
        if(!inject())return false;
    }else if(stage==6){
        diagnostics=probe_pause();
        return_result=static_cast<unsigned>(room.result);
        return_enter=room.room.enter_response;
        Social::Invitation pending;
        if(!read_invitation(&pending,sizeof(pending)))return false;
        return_pending=pending.ticket!=0;
        if(!ui.title_level(instance,&return_level))return false;
        confirmed_title=true;
        if(pending.ticket||room.result!=Room::Result::closed||
           room.room.enter_response!=2)return false;
        join_failed_safely=true;
        return canceled_room_intact&&confirmed_title&&join_failed_safely&&modal_scoped&&accepted_ticket&&(diagnostics&1024u);
    }
    ++stage;
    return false;
}
void report(FILE* file){
    std::fprintf(file,"\"invitation_stage\":%u,\"modal_scoped\":%u,\"cancel_kept_room\":%u,"
        "\"confirm_returned_to_title\":%u,\"closed_lobby_rejected\":%u,\"different_tickets\":%u,"
        "\"invite_diagnostics\":%u,\"stock_modal\":%u,\"custom_question\":%u,\"pending_seen\":%u,\"cutscene_guarded\":%u,"
        "\"return_level\":\"%ls\",\"return_result\":%u,\"return_enter\":%u,\"return_pending\":%u,",
        stage,modal_scoped,canceled_room_intact,confirmed_title,join_failed_safely,
        accepted_ticket!=0&&accepted_ticket!=canceled_ticket,diagnostics,stock_modal,custom_question,pending_seen,cutscene_guarded,
        return_level.c_str(),return_result,return_enter,return_pending);
}
}
