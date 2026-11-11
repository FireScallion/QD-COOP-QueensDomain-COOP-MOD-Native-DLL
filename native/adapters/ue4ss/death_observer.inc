// Pinned Win64 UFunction::Func and FFrame::Node for the verified Steam image.
using KillExec=void(*)(void*,void*,void*);
static constexpr unsigned native_function_offset=216,frame_node_offset=16;
static constexpr std::uintptr_t kill_exec_rva=0x11A7E40;
static std::atomic<KillExec> kill_original{};
static Weak kill_observed_function{};
static bool kill_observer_active;
static std::uint64_t kill_observed{},kill_queued{},kill_ignored{},kill_forwarded{};
static unsigned kill_restored{},kill_restore_conflicts{};

static void observe_kill(void* actor,void* frame,void* result){
    const auto original=kill_original.load(std::memory_order_acquire);
    const bool on_game_thread=GetCurrentThreadId()==game_thread.load();
    if(on_game_thread&&kill_observer_active&&host_role&&metadata_ready&&mode==1&&build_state.load()==1){
        void* node{};
        if(frame)std::memcpy(&node,static_cast<const unsigned char*>(frame)+frame_node_offset,sizeof(node));
        if(node&&node==api.weakget(&kill_observed_function)){
            ++kill_observed;
            bool queued=false;
            if(queue_death(reinterpret_cast<std::uintptr_t>(actor),&queued)){
                if(queued)++kill_queued;
            }else ++kill_ignored;
        }
    }
    original(actor,frame,result);
    if(on_game_thread)++kill_forwarded;
}

static void* volatile* kill_slot(void* function){
    return reinterpret_cast<void* volatile*>(static_cast<unsigned char*>(function)+native_function_offset);
}

static bool install_kill_observer(){
    if(!host_role||mode!=1||build_state.load()!=1||!death_metadata_alive()||GetModuleHandleW(L"UE4SS.dll"))return false;
    if(kill_observer_active)return api.weakget(&kill_observed_function)==death_kill;

    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto function=reinterpret_cast<std::uintptr_t>(death_kill);
    auto read=[](std::uintptr_t p,void* out,std::size_t n){std::memcpy(out,reinterpret_cast<const void*>(p),n);return true;};
    std::string path;
    if(!qdcoop::ue427::object_path(function,image+0x4A27F00,read,path)||
       path!="/Game/Enemies/EnemyBase.EnemyBase_C.Kill"||*api.parms(death_kill)!=2)return false;

    std::uint32_t flags{};
    std::memcpy(&flags,static_cast<const unsigned char*>(death_kill)+176,sizeof(flags));
    if(flags!=0x0C820000u)return false;
    auto slot=kill_slot(death_kill);
    const auto slot_address=reinterpret_cast<std::uintptr_t>(slot);
    MEMORY_BASIC_INFORMATION data{};
    if(slot_address%alignof(void*)||
       !VirtualQuery(reinterpret_cast<const void*>(slot_address),&data,sizeof(data))||
       data.State!=MEM_COMMIT||(data.Protect&PAGE_GUARD)||
       !(data.Protect&(PAGE_READWRITE|PAGE_WRITECOPY))||
       reinterpret_cast<std::uintptr_t>(data.BaseAddress)+data.RegionSize-slot_address<sizeof(void*))return false;

    const auto target=image+kill_exec_rva;
    constexpr unsigned char entry[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57};
    if(std::memcmp(reinterpret_cast<const void*>(target),entry,sizeof(entry)))return false;
    void* expected=reinterpret_cast<void*>(target);
    void* current{};
    std::memcpy(&current,reinterpret_cast<const void*>(slot_address),sizeof(current));
    if(current!=expected)return false;

    Weak key{};
    if(!makeweak(key,death_kill))return false;
    HMODULE module{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                         reinterpret_cast<LPCWSTR>(&observe_kill),&module))return false;
    KillExec original{};
    std::memcpy(&original,&expected,sizeof(original));
    kill_original.store(original,std::memory_order_release);
    if(InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(&observe_kill),expected)!=expected)return false;
    kill_observed_function=key;
    kill_observer_active=true;
    return true;
}

static void remove_kill_observer(){
    if(!kill_observer_active)return;
    kill_observer_active=false;
    auto function=api.weakget(&kill_observed_function);
    if(!function)return;
    auto original=kill_original.load(std::memory_order_acquire);
    auto hook=reinterpret_cast<void*>(&observe_kill);
    if(InterlockedCompareExchangePointer(kill_slot(function),reinterpret_cast<void*>(original),hook)==hook)++kill_restored;
    else ++kill_restore_conflicts;
}
