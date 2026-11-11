static void placed_item_request(void* actor,void* pawn);
static Weak placed_item_interact_function;
static std::atomic<KillExec> placed_item_interact_original{};
static constexpr LONG placed_item_script_flags=0x0c020000;
static constexpr LONG placed_item_native_flags=placed_item_script_flags|0x400;

static void observe_placed_item_interact(void* context,void* frame,void*){
    if(!frame)return;
    void* interacting{};
    auto& code=*reinterpret_cast<const unsigned char**>(static_cast<unsigned char*>(frame)+32);
    const auto node=AnimU::field<void*>(frame,16);
    const auto script=node?AnimU::field<const unsigned char*>(node,96):nullptr;
    if(code&&code!=script){
        native_vm_argument(frame,&interacting);
        if(code)++code;
    }else if(const auto locals=AnimU::field<void*>(frame,40)){
        interacting=AnimU::field<void*>(locals,0);
    }
    if(context&&interacting&&GetCurrentThreadId()==game_thread.load()&&active_peer_mask)
        placed_item_request(context,interacting);
}

static bool native_placed_item_unbind_interact(){
    if(const auto method=api.weakget(&placed_item_interact_function)){
        const auto flags=reinterpret_cast<volatile LONG*>(static_cast<unsigned char*>(method)+176);
        const auto previous=InterlockedCompareExchange(flags,placed_item_script_flags,placed_item_native_flags);
        if(previous!=placed_item_native_flags&&previous!=placed_item_script_flags)return false;
    }
    return native_restore_exec(placed_item_interact_function,placed_item_interact_original,&observe_placed_item_interact);
}

static bool native_placed_item_bind_interact(void* type){
    if(const auto bound=api.weakget(&placed_item_interact_function))return AnimU::field<void*>(bound,32)==type;
    if(!type||!native_bind_vm())return false;
    const auto method=declared_script_method(type,"Interact",8,3);
    const auto field=catalog_field(method,"PlayerInteracting",8);
    constexpr unsigned char body[]{0x04,0x0b,0x53};
    constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,
        0x74,0x24,0x18,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x30};
    const auto script=method?AnimU::field<const void*>(method,96):nullptr;
    if(!method||!script||std::memcmp(script,body,sizeof(body))||!medic_field_class(field,"ObjectProperty")||
       !damage_parameter(field)||*api.offset(field)||AnimU::field<LONG>(method,176)!=placed_item_script_flags||
       !native_install_exec(method,0x11a7e40,prefix,sizeof(prefix),placed_item_interact_function,
                            placed_item_interact_original,&observe_placed_item_interact))return false;
    // This exact shipped preview3 function has an empty Blueprint body.
    // The native flag routes both Blueprint and ProcessEvent calls through its
    // replacement entry point. The original flags and pointer are restored at cleanup.
    const auto flags=reinterpret_cast<volatile LONG*>(static_cast<unsigned char*>(method)+176);
    if(InterlockedCompareExchange(flags,placed_item_native_flags,placed_item_script_flags)==placed_item_script_flags)return true;
    native_restore_exec(placed_item_interact_function,placed_item_interact_original,&observe_placed_item_interact);
    return false;
}
