// Preview3 guest travel guards are void Blueprint wrappers. Returning before
// their persistent-frame assignment prevents the independent travel action.
struct NativeTravelMethod {
    unsigned owner;
    std::string_view name,parameter,ubergraph,persistent;
    unsigned width;
    std::uint32_t entry;
};
static constexpr NativeTravelMethod native_travel_methods[]{
    {world_player_type,"DoTeleporterWingStone","Consumable","ExecuteUbergraph_FS_PlayerChar","K2Node_CustomEvent_Consumable",1,106986},
    {world_travel_ui,"GoToDestination","Teleporter","ExecuteUbergraph_TeleporterUI","K2Node_CustomEvent_Teleporter",8,3554},
    {world_travel_save_ui,"BndEvt__SavedPopup_ShopButton_1_K2Node_ComponentBoundEvent_2_CommonButtonBaseClicked__DelegateSignature",
     "Button","ExecuteUbergraph_SaveMenu","K2Node_ComponentBoundEvent_Button_3",8,2957}
};
struct NativeTravelGuard {Weak function;volatile SHORT* entry{};SHORT original{};};
static std::array<NativeTravelGuard,std::size(native_travel_methods)> native_travel_guards{};
static constexpr SHORT travel_return=0x0b04;
static double native_travel_next{};

static bool writable_travel_code(const unsigned char* code){
    MEMORY_BASIC_INFORMATION page{};
    const auto address=reinterpret_cast<std::uintptr_t>(code);
    return code&&address%alignof(SHORT)==0&&VirtualQuery(code,&page,sizeof(page))&&page.State==MEM_COMMIT&&
           (page.Protect&PAGE_READWRITE)&&!(page.Protect&PAGE_GUARD)&&
           reinterpret_cast<std::uintptr_t>(page.BaseAddress)+page.RegionSize-address>=36;
}
static bool restore_travel_guard(NativeTravelGuard& guard){
    if(!guard.entry)return true;
    const auto fn=api.weakget(&guard.function);
    if(!fn){guard={};return true;}
    const auto code=AnimU::field<unsigned char*>(fn,96);
    if(reinterpret_cast<volatile SHORT*>(code)!=guard.entry||AnimU::field<int>(fn,104)!=36||!writable_travel_code(code))return false;
    const auto previous=InterlockedCompareExchange16(guard.entry,guard.original,travel_return);
    if(previous!=travel_return&&previous!=guard.original)return false;
    if(previous==travel_return)++travel_guards_restored;
    guard={};
    return true;
}
static bool native_travel_guard_cleanup(){
    bool clean=true;
    for(auto& guard:native_travel_guards)if(!restore_travel_guard(guard))clean=false;
    native_travel_next=0;
    return clean;
}
static bool install_travel_guard(const NativeTravelMethod& method,void* type,NativeTravelGuard& guard){
    if(!type)return false;
    const auto fn=declared_script_method(type,method.name,method.width,36);
    const auto argument=fn?catalog_field(fn,method.parameter,method.width):nullptr;
    if(!argument||*api.offset(argument)!=0||(AnimU::field<std::uint64_t>(argument,64)&0x580u)!=0x80u)return false;
    const auto code=AnimU::field<unsigned char*>(fn,96);
    if(!writable_travel_code(code))return false;
    const auto original=AnimU::field<SHORT>(code,0);
    if((original&0xff)!=0x64||code[9]!=0x00||AnimU::field<void*>(code,10)!=argument||code[18]!=0x46||code[27]!=0x1d||
       AnimU::field<std::uint32_t>(code,28)!=method.entry||code[32]!=0x16||code[33]!=0x04||code[34]!=0x0b||code[35]!=0x53)return false;
    const auto uber=AnimU::field<void*>(code,19);
    std::string path;
    if(!uber||AnimU::field<void*>(uber,32)!=type||!catalog_path(uber,path)||catalog_leaf(path)!=method.ubergraph||
       AnimU::field<void*>(code,1)!=catalog_field(uber,method.persistent,method.width))return false;
    NativeTravelGuard candidate{};
    if(!makeweak(candidate.function,fn))return false;
    candidate.entry=reinterpret_cast<volatile SHORT*>(code);
    candidate.original=original;
    if(InterlockedCompareExchange16(candidate.entry,travel_return,candidate.original)!=candidate.original)return false;
    guard=candidate;
    ++travel_guards_installed;
    return true;
}
static void native_travel_guard_step(double time){
    if(host_role||!active_peer_mask||time<native_travel_next)return;
    native_travel_next=time+.5;
    if(!world_input(world_travel_ui)||!world_input(world_travel_save_ui))scan_world_inputs(4096);
    for(unsigned i=0;i<native_travel_guards.size();++i){
        auto& guard=native_travel_guards[i];
        if(guard.entry&&api.weakget(&guard.function))continue;
        if(!restore_travel_guard(guard))continue;
        const auto& method=native_travel_methods[i];
        auto type=world_input(method.owner);
        if(!type&&i){
            const auto widget=world_input(world_modal_type);
            if(widget)type=load_world_asset(world_paths[method.owner],catalog_blueprint,true,widget);
            if(type)makeweak(world_inputs[method.owner],type);
        }
        install_travel_guard(method,type,guard);
    }
}
