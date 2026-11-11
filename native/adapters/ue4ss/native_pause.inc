struct NativePause {
    Weak pawn,mode;
    void* force_overlay{};
    void* menu_paused{};
    void* enemy_paused{};
    double next_bind{},next_poll{},next_modal_bind{};
    bool original_overlay{},correcting{},modal_correcting{};
};
static NativePause native_pause;
static std::atomic<KillExec> native_pause_original{};
static void* volatile* native_pause_cell;
static std::atomic<KillExec> native_modal_original{};
static void* volatile* native_modal_cell;

static void native_pause_unpause(void* pawn){
    auto& state=native_pause;
    const auto gs=world_input(static_cast<unsigned>(world_object_count)+factory_gameplay);
    const auto get=world_input(world_pause_game_get),set=world_input(world_pause_game_set);
    if(!pawn||!gs||!get||!set||state.correcting)return;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,pawn);
    api.event(gs,get,args.data());
    if(!args[8])return;
    args[8]=0;
    state.correcting=true;
    api.event(gs,set,args.data());
    state.correcting=false;
    if(args[9])++native_unpaused;
}
static void native_pause_observe(void* context,void* frame,void* result){
    const auto original=native_pause_original.load(std::memory_order_acquire);
    original(context,frame,result);
    if(GetCurrentThreadId()!=game_thread.load()||!native_pause_cell||!automatic_limit||!active_peer_mask)return;
    native_pause_unpause(api.weakget(&native_pause.pawn));
}
static bool native_pause_install(void* setter,std::uintptr_t rva,std::span<const unsigned char> prefix,
                                 KillExec handler,std::atomic<KillExec>& original,void* volatile*& target){
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(!setter||build_state.load()!=1)return false;
    const auto expected=reinterpret_cast<void*>(image+rva);
    auto cell=reinterpret_cast<void* volatile*>(static_cast<unsigned char*>(setter)+216);
    if(AnimU::field<void*>(setter,216)!=expected||std::memcmp(expected,prefix.data(),prefix.size())||
       !(AnimU::field<std::uint32_t>(setter,176)&0x400u))return false;
    MEMORY_BASIC_INFORMATION page{};
    const auto address=reinterpret_cast<std::uintptr_t>(cell);
    if(address%alignof(void*)||!VirtualQuery(reinterpret_cast<const void*>(address),&page,sizeof(page))||page.State!=MEM_COMMIT||
       !(page.Protect&PAGE_READWRITE)||(page.Protect&PAGE_GUARD)||
       reinterpret_cast<std::uintptr_t>(page.BaseAddress)+page.RegionSize-address<sizeof(void*))return false;
    original.store(reinterpret_cast<KillExec>(expected),std::memory_order_release);
    if(InterlockedCompareExchangePointer(cell,reinterpret_cast<void*>(handler),expected)!=expected)return false;
    target=cell;
    return true;
}
static bool native_pause_restore(void* volatile*& slot,const std::atomic<KillExec>& original,
                                 KillExec handler,std::uint64_t& count){
    if(!slot)return true;
    const auto expected=reinterpret_cast<void*>(original.load(std::memory_order_acquire));
    const auto hook=reinterpret_cast<void*>(handler);
    const auto previous=InterlockedCompareExchangePointer(slot,expected,hook);
    slot=nullptr;
    if(previous==hook)++count;
    return previous==hook||previous==expected;
}
static bool native_modal_visible(void* pawn){
    const auto find=world_input(world_modal_find),library=world_input(world_modal_library),type=world_input(world_modal_type);
    const auto viewport=world_input(world_modal_viewport),visible=world_input(world_modal_visible);
    if(!find||!library||!type||!viewport||!visible||!native_free)return false;
    constexpr std::string_view modal_paths[]{
        "/Game/UI/Menus/PauseMenu.PauseMenu_C",
        "/Game/UI/Menus/GameMenu.GameMenu_C",
        "/Game/UI/Menus/Journal/JournalMenu.JournalMenu_C",
        "/Game/UI/Menus/Journal/Guides_Menu.Guides_Menu_C",
        "/Game/UI/Menus/Challenges/Challenges_Menu.Challenges_Menu_C",
        "/Game/UI/Menus/OptionsMenu.OptionsMenu_C",
        "/Game/UI/PhotoMode/PhotoMode_HUD.PhotoMode_HUD_C"
    };
    alignas(16) FactoryParameters args{};
    factory_put(args,0,pawn);
    factory_put(args,24,type);
    args[32]=1;
    api.event(library,find,args.data());
    const auto widgets=AnimU::field<NativeActorArray>(args.data(),8);
    bool active=false;
    if(widgets.data&&widgets.count>=0&&widgets.count<=4096&&widgets.capacity>=widgets.count){
        for(int i=0;i<widgets.count&&!active;++i){
            const auto widget=widgets.data[i];
            std::string path;
            if(!widget||!api.isa(widget,type)||!catalog_path(AnimU::field<void*>(widget,16),path)||
               std::ranges::find(modal_paths,path)==std::end(modal_paths))continue;
            unsigned char in_viewport{},shown{};
            api.event(widget,viewport,&in_viewport);
            api.event(widget,visible,&shown);
            active=in_viewport&&shown;
        }
    }
    if(widgets.data)native_free(widgets.data);
    return active;
}
static void native_modal_observe(void* context,void* frame,void* result){
    const auto original=native_modal_original.load(std::memory_order_acquire);
    original(context,frame,result);
    if(GetCurrentThreadId()!=game_thread.load()||!native_modal_cell||!automatic_limit||!active_peer_mask||
       native_pause.modal_correcting)return;
    const auto pawn=api.weakget(&native_pause.pawn);
    const auto gs=world_input(static_cast<unsigned>(world_object_count)+factory_gameplay);
    const auto get=world_input(world_modal_global_get),set=world_input(world_modal_global_set);
    if(!pawn||!gs||!get||!set)return;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,pawn);
    api.event(gs,get,args.data());
    const auto scale=AnimU::field<float>(args.data(),8);
    if(!(scale>=0.f&&scale<.01f)||!native_modal_visible(pawn))return;
    factory_put(args,8,1.f);
    native_pause.modal_correcting=true;
    api.event(gs,set,args.data());
    native_pause.modal_correcting=false;
    ++native_modal_dilation;
}
static void native_modal_bind(double time){
    auto& state=native_pause;
    if(native_modal_cell||time<state.next_modal_bind)return;
    state.next_modal_bind=time+.25;
    for(unsigned i=world_modal_first;i<world_damage_first;++i)if(!world_input(i)){
        scan_world_inputs(2048);
        break;
    }
    if(!world_input(world_modal_type)||!world_input(world_modal_library)||
       !medic_params(world_modal_global_get,12,{{"WorldContextObject",8,0},{"ReturnValue",4,8}})||
       !medic_params(world_modal_global_set,12,{{"WorldContextObject",8,0},{"TimeDilation",4,8}})||
       !medic_params(world_modal_find,33,{{"WorldContextObject",8,0},{"FoundWidgets",16,8},
                                         {"WidgetClass",8,24},{"TopLevelOnly",1,32}})||
       !medic_params(world_modal_viewport,1,{{"ReturnValue",1,0}})||
       !medic_params(world_modal_visible,1,{{"ReturnValue",1,0}}))return;
    constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x33,0xff,0x48,0x8b,0xda};
    native_pause_install(world_input(world_modal_global_set),0x2dc39f0,prefix,&native_modal_observe,
                         native_modal_original,native_modal_cell);
}
static bool native_pause_cleanup(){
    auto& state=native_pause;
    if(state.force_overlay)if(const auto pawn=api.weakget(&state.pawn))
        api.setbool(state.force_overlay,pawn,state.original_overlay,0);
    state={};
    const auto modal=native_pause_restore(native_modal_cell,native_modal_original,&native_modal_observe,native_modal_restored);
    const auto paused=native_pause_restore(native_pause_cell,native_pause_original,&native_pause_observe,native_pause_restored);
    return modal&&paused;
}
static bool native_pause_bind(void* pawn,double time){
    auto& state=native_pause;
    if(api.weakget(&state.pawn)==pawn&&api.weakget(&state.mode)&&native_pause_cell)return true;
    if(time<state.next_bind)return false;
    if(native_pause_cell&&!native_pause_cleanup())return false;
    state.next_bind=time+.25;
    for(unsigned i=world_pause_first;i<world_modal_first;++i)if(!world_input(i)){
        scan_world_inputs(2048);
        break;
    }
    const auto overlay=world_input(world_pause_overlay);
    if(!overlay||AnimU::field<int>(overlay,104)!=18||!medic_params(world_pause_overlay,0,{})||
       !medic_params(world_pause_mode,16,{{"WorldContextObject",8,0},{"ReturnValue",8,8}})||
       !medic_params(world_pause_game_get,9,{{"WorldContextObject",8,0},{"ReturnValue",1,8}})||
       !medic_params(world_pause_game_set,10,{{"WorldContextObject",8,0},{"bPaused",1,8},{"ReturnValue",1,9}})||
       !medic_params(world_pause_menu_set,1,{{"Pause",1,0}})||!medic_params(world_pause_enemy_set,1,{{"Paused",1,0}})||
       AnimU::field<int>(world_input(world_pause_menu_set),104)!=36||
       AnimU::field<int>(world_input(world_pause_enemy_set),104)!=36)return false;
    const auto gs=world_input(static_cast<unsigned>(world_object_count)+factory_gameplay);
    if(!gs)return false;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,pawn);
    api.event(gs,world_input(world_pause_mode),args.data());
    const auto gm=AnimU::field<void*>(args.data(),8);
    if(!gm||!api.isa(gm,AnimU::field<void*>(world_input(world_pause_menu_set),32)))return false;
    const auto type=AnimU::field<void*>(gm,16);
    NativePause binding{};
    binding.force_overlay=catalog_field(AnimU::field<void*>(pawn,16),"ForceNoOverlayPause",1);
    binding.menu_paused=catalog_field(type,"PauseTD",1);
    binding.enemy_paused=catalog_field(type,"EnemyPauseTDLEnabled",1);
    constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x20,0x57,0x48,0x83,0xec,0x20};
    if(!medic_field_class(binding.force_overlay,"BoolProperty")||!medic_field_class(binding.menu_paused,"BoolProperty")||
       !medic_field_class(binding.enemy_paused,"BoolProperty")||!makeweak(binding.pawn,pawn)||!makeweak(binding.mode,gm)||
       !native_pause_install(world_input(world_pause_game_set),0x2dc3580,prefix,&native_pause_observe,
                             native_pause_original,native_pause_cell))return false;
    binding.original_overlay=api.getbool(binding.force_overlay,pawn,0);
    api.setbool(binding.force_overlay,pawn,true,0);
    state=binding;
    native_pause_unpause(pawn);
    return true;
}
static void native_pause_step(void* pawn,double time){
    auto& state=native_pause;
    if(!active_peer_mask){if(native_pause_cell)native_pause_cleanup();return;}
    if(!native_pause_bind(pawn,time)||time<state.next_poll)return;
    state.next_poll=time+.04;
    native_modal_bind(time);
    const auto gm=api.weakget(&state.mode);
    if(!gm)return;
    unsigned char resume{};
    if(api.getbool(state.menu_paused,gm,0)){
        api.event(gm,world_input(world_pause_menu_set),&resume);
        ++native_menu_dilation;
    }
    if(api.getbool(state.enemy_paused,gm,0)){
        api.event(gm,world_input(world_pause_enemy_set),&resume);
        ++native_enemy_dilation;
    }
}
