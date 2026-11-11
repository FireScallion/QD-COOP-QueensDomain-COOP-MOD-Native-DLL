namespace TitleUi {
enum class PauseMethod { open,quit,close,exit,suspend,options,count };
constexpr std::array<const char*,static_cast<unsigned>(PauseMethod::count)> pause_paths{
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.OpenPauseMenu",
    "/Game/UI/Menus/PauseMenu.PauseMenu_C.BndEvt__PauseMenu_QuitTomenuButton_K2Node_ComponentBoundEvent_8_CommonButtonBaseClicked__DelegateSignature",
    "/Game/UI/Menus/PauseMenu.PauseMenu_C.BndEvt__OptionsMenu_modalCancelButton_K2Node_ComponentBoundEvent_0_CommonButtonBaseClicked__DelegateSignature",
    "/Game/UI/Menus/PauseMenu.PauseMenu_C.ConfirmExitToMenu",
    "/Game/UI/Menus/PauseMenu.PauseMenu_C.ConfirmSuspendToTitle",
    "/Game/UI/Menus/PauseMenu.PauseMenu_C.OpenOptions"
};
enum class PauseDecision { none,accept,cancel };
void observe_pause_exit(void* context,void* frame,void* result);
void observe_place_button(Address object);
struct PauseInvite {
    UE::WeakKey pause_type{},pause{},row{},button{},hooked_function{};
    UE::WeakKey cancel{};
    std::array<UE::WeakKey,pause_paths.size()> methods{};
    std::array<UE::WeakKey,2> exit_hooks{};
    const unsigned char* click_script{};
    std::atomic<Exec> original{},original_exit{};
    unsigned cursor{},last_size{},thread{};
    std::uint64_t modal_ticket{};
    ULONGLONG next_step{},next_open{},modal_since{},accepted_until{};
    PauseDecision decision{};
    bool queued{},dialog_visible{},place_click_retained{},invite_chinese{};
#ifdef QDCOOP_STEAM_SOCIAL_LAB
    unsigned test_observations{};
#endif

    Address method(PauseMethod id) const {return menu.ui.get(methods[static_cast<unsigned>(id)]);}
    Address method(PauseMethod id,Address type) {
        if(const auto known=method(id))return known;
        auto& ui=menu.ui;
        const auto index=static_cast<unsigned>(id);
        const std::string_view wanted=pause_paths[index];
        const auto short_name=wanted.substr(wanted.rfind('.')+1);
        for(unsigned depth=0;type&&depth<32;++depth,type=at<Address>(type,64)){
            unsigned checked{};
            for(auto child=at<Address>(type,72);child&&checked<2048;child=at<Address>(child,40),++checked){
                std::string leaf,path;
                if(at<Address>(child,32)!=type||!UE::name_string(ui.image+name_pool,at<UE::NameKey>(child,24),read_local,leaf)||
                   leaf!=short_name||!UE::object_path(child,ui.image+name_pool,read_local,path)||path!=wanted)continue;
                ui.remember(methods[index],child);
                return child;
            }
        }
        return 0;
    }
    bool flag(Address object,const char* name) const {
        const auto field=menu.ui.member(object,name,"BoolProperty",1);
        return field&&UE::get_bool(reinterpret_cast<void*>(field),reinterpret_cast<const void*>(object),0);
    }
    bool hosting_steam_room() const {
        return preparing.operation==Room::Operation::host&&status.result==Room::Result::running&&
            status.room.lobby&&status.room.limit>=2;
    }
    void find_pause() {
        auto& ui=menu.ui;
        const auto array=at<UE::ObjectArray>(ui.image+object_array,0);
        if(!UE::object_array_shape(array))return;
        const auto size=static_cast<unsigned>(array.num_elements);
        auto type=ui.get(pause_type);
        if(!cursor||size!=last_size)cursor=size;
        last_size=size;
        for(unsigned scanned=0;scanned<2048&&cursor;++scanned){
            const auto index=--cursor;
            const auto chunk=at<Address>(array.chunks,(index/UE::objects_per_chunk)*8);
            if(!chunk)continue;
            const auto item=at<UE::ObjectItem>(chunk,(index%UE::objects_per_chunk)*sizeof(UE::ObjectItem));
            if(!item.object||(item.flags&0x30000000u))continue;
            const auto object_type=at<Address>(item.object,16);
            if(!type){
                std::string leaf,path;
                if(!object_type||!UE::name_string(ui.image+name_pool,at<UE::NameKey>(object_type,24),read_local,leaf)||
                   leaf!="PauseMenu_C"||!UE::object_path(object_type,ui.image+name_pool,read_local,path)||
                   path!="/Game/UI/Menus/PauseMenu.PauseMenu_C")continue;
                type=object_type;
                ui.remember(pause_type,type);
            }
            if(object_type==type&&!(at<unsigned>(item.object,8)&0x10u)&&ui.shown(item.object)){
                ui.remember(pause,item.object);
                break;
            }
        }
    }
    bool hook(Address fn,Exec observer,UE::WeakKey& key,std::atomic<Exec>& saved) {
        auto& ui=menu.ui;
        if(ui.get(key)==fn)return fn!=0;
        const auto stock=reinterpret_cast<void*>(ui.image+0x11a7e40);
        constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57};
        auto cell=reinterpret_cast<void* volatile*>(fn+216);
        MEMORY_BASIC_INFORMATION memory{};
        if(*cell!=stock||std::memcmp(stock,prefix,sizeof(prefix))||
           !VirtualQuery(reinterpret_cast<void*>(fn+216),&memory,sizeof(memory))||memory.State!=MEM_COMMIT||
           !(memory.Protect&PAGE_READWRITE)||(memory.Protect&PAGE_GUARD))return false;
        saved.store(reinterpret_cast<Exec>(stock),std::memory_order_release);
        if(InterlockedCompareExchangePointer(cell,reinterpret_cast<void*>(observer),stock)!=stock)return false;
        ui.remember(key,fn);
        return true;
    }
    void unhook(UE::WeakKey& key,Exec observer,const std::atomic<Exec>& saved) {
        auto& ui=menu.ui;
        if(const auto fn=ui.get(key)){
            const auto stock=reinterpret_cast<void*>(saved.load(std::memory_order_acquire));
            InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(fn+216),stock,reinterpret_cast<void*>(observer));
        }
        key={};
    }
    bool attach(Exec observer){
        auto& ui=menu.ui;
        if(ui.get(hooked_function))return true;
        const auto fn=ui.get(click);
        if(!fn||!menu.wrapper(click,button_graph,1337))return false;
        click_script=at<const unsigned char*>(fn,96);
        thread=GetCurrentThreadId();
        return hook(fn,observer,hooked_function,original);
    }
    void clear(Exec observer) {
        if(!place_click_retained)unhook(hooked_function,observer,original);
        clear_invite_button();
        pause={};
    }
    void clear_invite_button() {
        auto& ui=menu.ui;
        if(const auto container=ui.get(row))event(container,ui.get(remove_parent),nullptr);
        row={};
        button={};
        queued=false;
    }
    void forget_confirmation() {
        for(auto& key:exit_hooks)unhook(key,&observe_pause_exit,original_exit);
        cancel={};
        modal_ticket=0;
        modal_since=0;
        accepted_until=0;
        decision=PauseDecision::none;
        dialog_visible=false;
    }
    bool mount(Address context) {
        auto& ui=menu.ui;
        const auto source=ui.object(context,"Statusbutton");
        const auto source_row=source?ui.parent(source):0;
        if(!ui.is(source_row,ui.get(size_type)))return false;
        const auto box=ui.parent(source_row),tree=ui.object(context,"WidgetTree");
        if(!tree||!ui.is(box,ui.get(vertical_box_type)))return false;
        invite_chinese=menu.zh;
        const auto added=ui.button(context,source,menu.message(L"Invite players",L"邀请玩家"));
        const auto container=ui.spawn(tree,ui.get(size_type));
        if(!added||!container||!ui.add(container,added))return false;
        float size=50.f;
        if(const auto property=ui.member(source_row,"HeightOverride","FloatProperty",4)){
            const auto candidate=at<float>(source_row,at<unsigned>(property,76));
            if(candidate>=20.f&&candidate<=150.f)size=candidate;
        }
        event(container,ui.get(height),&size);
        if(!ui.insert_after(box,source_row,container))return false;
        ui.remember(row,container);
        ui.remember(button,added);
        return true;
    }
    void receive(void* context,void* frame){
        const auto stack=reinterpret_cast<Address>(frame);
        if(!stack||GetCurrentThreadId()!=thread||at<const unsigned char*>(stack,32)!=click_script)return;
        auto& ui=menu.ui;
        const auto object=reinterpret_cast<Address>(context);
        if(!object||at<Address>(stack,24)!=object||at<Address>(stack,16)!=ui.get(hooked_function))return;
        if(ui.get(button)==object)queued=true;
        if(menu.screen==Screen::invite_friends)menu.receive_control(object);
        if(dialog_visible&&modal_ticket&&ui.get(cancel)==object)decision=PauseDecision::cancel;
        if(place_click_retained)observe_place_button(object);
    }
    bool attach_confirmation(Address type) {
        auto& ui=menu.ui;
        constexpr PauseMethod actions[]{PauseMethod::exit,PauseMethod::suspend};
        constexpr unsigned entry_points[]{2798,6336};
        for(unsigned i=0;i<exit_hooks.size();++i){
            const auto fn=method(actions[i],type);
            const auto script=fn?at<Address>(fn,96):0;
            if(!ui.method(fn,0,{})||!script||at<int>(fn,104)!=18||at<unsigned char>(script,0)!=0x46||
               at<unsigned char>(script,9)!=0x1d||at<unsigned>(script,10)!=entry_points[i]||
               !hook(fn,&observe_pause_exit,exit_hooks[i],original_exit)){
                for(auto& key:exit_hooks)unhook(key,&observe_pause_exit,original_exit);
                return false;
            }
        }
        return true;
    }
    void receive_exit(void* context,void* frame) {
        if(!modal_ticket||!dialog_visible||!context||!frame||GetCurrentThreadId()!=thread)return;
        auto& ui=menu.ui;
        const auto object=reinterpret_cast<Address>(context),stack=reinterpret_cast<Address>(frame);
        if(ui.get(pause)!=object||at<Address>(stack,24)!=object)return;
        const auto function=at<Address>(stack,16);
        if(function&&(ui.get(exit_hooks[0])==function||ui.get(exit_hooks[1])==function)&&
           at<Address>(stack,32)==at<Address>(function,96)){
            decision=PauseDecision::accept;
            invitation_gate.confirmed=modal_ticket;
#ifdef QDCOOP_STEAM_SOCIAL_LAB
            test_observations|=1024;
#endif
        }
    }
    void open_pause(Address instance,ULONGLONG now) {
        if(now<next_open)return;
        next_open=now+1500;
        auto& ui=menu.ui;
        const auto statics=ui.get(gameplay);
        const auto get=ui.get(local_pawn);
        if(!statics||!get)return;
        Address args[]{instance,0,0};
        event(statics,get,args);
        if(!args[2])return;
#ifdef QDCOOP_STEAM_SOCIAL_LAB
        test_observations|=256;
#endif
        const auto open=method(PauseMethod::open,at<Address>(args[2],16));
        if(!ui.method(open,1,{{"QuickJournal","BoolProperty",0,1}})||!ui.is(args[2],at<Address>(open,32)))return;
        unsigned char quick{};
        event(args[2],open,&quick);
    }
    void open_confirmation(Address context,Social::Invitation request,ULONGLONG now,Exec observer) {
        if(!context||flag(context,"ModalOpen"))return;
        auto& ui=menu.ui;
        const auto quit=method(PauseMethod::quit,at<Address>(context,16));
        auto source=ui.object(context,"QuitTomenuButton");
        const auto accept=ui.object(context,"ExitConfirmButton"),reject=ui.object(context,"ExitCancelButton");
        const auto question=ui.object(context,"ConfirmDialogueText");
        if(!ui.method(quit,8,{{"Button","ObjectProperty",0,8}})||!ui.is(source,ui.get(button_type))||
           !ui.is(accept,ui.get(button_type))||!ui.is(reject,ui.get(button_type))||!ui.is(question,ui.get(text_type))||
           !attach(observer)||!attach_confirmation(at<Address>(context,16)))return;
        ui.remember(cancel,reject);
        modal_ticket=request.ticket;
        modal_since=now;
        event(context,quit,&source);
        service_confirmation(context,request,now);
    }
    void service_confirmation(Address context,Social::Invitation request,ULONGLONG now) {
        if(!modal_ticket)return;
        if(modal_ticket!=request.ticket){
            const auto close=context?method(PauseMethod::close,at<Address>(context,16)):0;
            auto cancel_button=menu.ui.get(cancel);
            if(context&&cancel_button&&flag(context,"ModalOpen")&&
               menu.ui.method(close,8,{{"Button","ObjectProperty",0,8}}))event(context,close,&cancel_button);
            forget_confirmation();
            return;
        }
        if(!dialog_visible){
            auto& ui=menu.ui;
            const auto question=ui.object(context,"ConfirmDialogueText");
            if(context&&flag(context,"ModalOpen")&&ui.is(question,ui.get(text_type))){
                ui.label(question,ui.chinese()?L"返回主菜单并加入好友？未保存的进度可能丢失。":
                                             L"Return to the title and join your friend? Unsaved progress may be lost.");
                dialog_visible=true;
            }else if(now-modal_since>1500)forget_confirmation();
            return;
        }
        if(decision==PauseDecision::accept){
            if(!accepted_until)accepted_until=now+20000;
            decision=PauseDecision::none;
        }
        const bool canceled=decision==PauseDecision::cancel||
            (!accepted_until&&now-modal_since>500&&(!context||!flag(context,"ModalOpen")))||
            (accepted_until&&now>accepted_until);
        if(canceled){
            respond_to_invitation(modal_ticket,0);
            forget_confirmation();
        }
    }
    void close_friends(bool close){
        menu.pending=Action::none;
        menu.restore_panel(close);
    }
    void open_friends(Address context){
        auto& ui=menu.ui;
        if(!context||!ui.shown(context)||flag(context,"InOptions")||flag(context,"ModalOpen")||ui.get(menu.panel))return;
        const auto open=method(PauseMethod::options,at<Address>(context,16));
        if(!ui.method(open,0,{}))return;
        event(context,open,nullptr);
        const auto panel=ui.object(context,"ActiveOptionsMenu");
        if(menu.adopt_panel(panel)&&menu.show_friends(Screen::invite_friends,status.room.lobby))return;
        if(ui.get(menu.panel))close_friends(true);
        else if(ui.is(panel,ui.get(options_type))&&ui.shown(panel))event(panel,ui.get(options_close),nullptr);
    }
    bool service_friends(bool can_invite,ULONGLONG now){
        if(menu.screen!=Screen::invite_friends)return false;
        const auto panel=menu.ui.get(menu.panel);
        if(!panel||!menu.ui.shown(panel)){
            close_friends(false);
            return false;
        }
        if(!can_invite||menu.invite_room!=status.room.lobby||menu.pending==Action::back){
            close_friends(true);
            return false;
        }
        const auto action=std::exchange(menu.pending,Action::none);
        if(action==Action::friend_pick||action==Action::friend_previous||action==Action::friend_next){
            if(!menu.use_friend_action(action))close_friends(true);
        }else if(!menu.refresh_language(now)||!menu.refresh_friends(now))close_friends(true);
        return menu.screen==Screen::invite_friends;
    }
    void tick(Address engine,ULONGLONG now,Exec observer) {
        auto& ui=menu.ui;
        if(!ui.ready||!menu.level_known)return;
        if(menu.on_title){
            if(menu.screen==Screen::invite_friends)close_friends(false);
            if(ui.get(row)||ui.get(hooked_function))clear(observer);
            forget_confirmation();
            return;
        }
        if(now<next_step&&!queued&&decision==PauseDecision::none&&menu.pending==Action::none)return;
        next_step=now+200;
        const auto incoming=read_invitation();
        if(incoming.ticket&&invitation_gate.offered!=incoming.ticket)invitation_gate={incoming.ticket,0};
        const bool can_invite=hosting_steam_room();
        if(service_friends(can_invite&&!incoming.ticket,now))return;
        if(!can_invite&&!incoming.ticket&&!modal_ticket){
            if(ui.get(row)||ui.get(hooked_function))clear(observer);
            return;
        }
        auto context=ui.get(pause);
        if(context&&!ui.shown(context))context=0;
        service_confirmation(context,incoming,now);
        if(!can_invite&&ui.get(row))clear_invite_button();
        if(!context){
            if(ui.get(row)||ui.get(hooked_function))clear(observer);
            find_pause();
            context=ui.get(pause);
            if(context&&!ui.shown(context))context=0;
        }
        if(can_invite&&context){
            menu.refresh_language(now);
            if(const auto invite=ui.get(button);invite&&invite_chinese!=menu.zh){
                menu.button_caption(invite,menu.message(L"Invite players",L"邀请玩家"));
                invite_chinese=menu.zh;
            }
        }
        if(can_invite&&context&&!ui.get(button)&&attach(observer)&&!mount(context)&&!modal_ticket)clear(observer);
        if(std::exchange(queued,false)&&can_invite&&context&&!incoming.ticket){
            open_friends(context);
            if(menu.screen==Screen::invite_friends)return;
        }
        const auto instance=ui.object(engine,"GameInstance");
        if(!incoming.ticket||modal_ticket||!instance||read_invitation().ticket!=incoming.ticket)return;
        const bool in_cutscene=flag(instance,"InCutscene");
#ifdef QDCOOP_STEAM_SOCIAL_LAB
        test_observations|=in_cutscene?128u:512u;
#endif
        if(in_cutscene)return;
        if(context)open_confirmation(context,incoming,now,observer);
        else open_pause(instance,now);
    }
};
PauseInvite pause_invite;
void observe_pause_click(void* context,void* frame,void* result){
    pause_invite.receive(context,frame);
    pause_invite.original.load(std::memory_order_acquire)(context,frame,result);
}
void observe_pause_exit(void* context,void* frame,void* result){
    pause_invite.receive_exit(context,frame);
    pause_invite.original_exit.load(std::memory_order_acquire)(context,frame,result);
}
}
