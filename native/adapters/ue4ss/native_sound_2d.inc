struct Sound2DInput {
    void* world{};
    void* sound{};
    float volume{},pitch{},start{};
    void* concurrency{};
};
static_assert(sizeof(Sound2DInput)==40&&offsetof(Sound2DInput,concurrency)==32);
using Sound2DPlay=void(*)(void*,void*,float,float,float,void*,void*,bool);
using Sound2DSpawn=void*(*)(void*,void*,float,float,float,void*,bool,bool);
static SoundHook sound_play_2d_hook,sound_spawn_2d_hook,sound_create_2d_hook;
static Sound2DPlay sound_stock_play_2d;
static Sound2DSpawn sound_stock_spawn_2d,sound_stock_create_2d;

static void sound_2d_read(void* frame,Sound2DInput& args){
    for(auto field:{static_cast<void*>(&args.world),static_cast<void*>(&args.sound),static_cast<void*>(&args.volume),
                    static_cast<void*>(&args.pitch),static_cast<void*>(&args.start),static_cast<void*>(&args.concurrency)})
        native_vm_argument(frame,field);
}
static bool sound_emit_2d(const Sound2DInput& args,void* author,Sound::Origin origin){
    if(origin==Sound::Origin{})return false;
    const auto anchor=sound_spatial_anchor(origin,author);
    if(!anchor)return false;
    return sound_emit(sound_event(origin,args.sound,nullptr,native_actor_location(anchor),args.volume,args.pitch,args.start));
}
static void observe_sound_play_2d(void* context,void* frame,void* result){
    const auto original=sound_play_2d_hook.original.load(std::memory_order_acquire);
    if(!sound_observing_frame(frame)){
        original(context,frame,result);
        return;
    }
    Sound2DInput args;
    void* owner{};
    auto& code=*reinterpret_cast<const unsigned char**>(static_cast<unsigned char*>(frame)+32);
    const bool blueprint=code!=nullptr;
    bool muted{};
    if(blueprint){
        unsigned ui_sound{};
        sound_2d_read(frame,args);
        native_vm_argument(frame,&owner);
        native_vm_argument(frame,&ui_sound);
        if(code)++code;
        muted=sound_mute_replica(args.world,args.volume)||sound_mute_replica(owner,args.volume);
        sound_stock_play_2d(args.world,args.sound,args.volume,args.pitch,args.start,args.concurrency,owner,ui_sound!=0);
    }else{
        const auto params=sound_reflected_params(frame,sound_play_2d_hook);
        if(params){
            std::memcpy(&args,params,sizeof(args));
            owner=AnimU::field<void*>(params,40);
            muted=sound_mute_replica(args.world,args.volume)||sound_mute_replica(owner,args.volume);
        }
        sound_call_with_mute(original,context,frame,result,params,16,muted);
    }
    if(muted)return;
    auto author=args.world;
    auto origin=sound_origin(author);
    if(origin==Sound::Origin{}){
        author=owner;
        origin=sound_origin(author);
    }
    if(sound_emit_2d(args,author,origin))++(blueprint?sound_direct_blueprint:sound_direct_reflected);
}
static void observe_sound_make_2d(SoundHook& hook,Sound2DSpawn stock,bool prepared,void* context,void* frame,void* result){
    const auto original=hook.original.load(std::memory_order_acquire);
    if(!sound_observing_frame(frame)||!result){
        original(context,frame,result);
        return;
    }
    Sound2DInput args;
    auto& code=*reinterpret_cast<const unsigned char**>(static_cast<unsigned char*>(frame)+32);
    bool muted{};
    if(code){
        unsigned persist{},auto_destroy{};
        sound_2d_read(frame,args);
        native_vm_argument(frame,&persist);
        native_vm_argument(frame,&auto_destroy);
        if(code)++code;
        muted=sound_mute_replica(args.world,args.volume);
        const auto voice=stock(args.world,args.sound,args.volume,args.pitch,args.start,args.concurrency,persist!=0,auto_destroy!=0);
        std::memcpy(result,&voice,sizeof(voice));
    }else{
        const auto params=sound_reflected_params(frame,hook);
        if(params){
            std::memcpy(&args,params,sizeof(args));
            muted=sound_mute_replica(args.world,args.volume);
        }
        sound_call_with_mute(original,context,frame,result,params,16,muted);
    }
    if(muted)return;
    const auto origin=sound_origin(args.world);
    if(origin==Sound::Origin{})return;
    const auto component=AnimU::field<void*>(result,0);
    sound_prepare_2d(component,args.world);
    if(prepared)return;
    if(component)sound_capture_component(args.world,component,args.start,nullptr,sound_spatial_anchor(origin,args.world));
    else sound_emit_2d(args,args.world,origin);
}
static void observe_sound_spawn_2d(void* context,void* frame,void* result){
    observe_sound_make_2d(sound_spawn_2d_hook,sound_stock_spawn_2d,false,context,frame,result);
}
static void observe_sound_create_2d(void* context,void* frame,void* result){
    observe_sound_make_2d(sound_create_2d_hook,sound_stock_create_2d,true,context,frame,result);
}
static bool sound_2d_spawn_layout(unsigned world){
    return medic_params(world,56,{{"WorldContextObject",8,0},{"Sound",8,8},{"VolumeMultiplier",4,16},
        {"PitchMultiplier",4,20},{"StartTime",4,24},{"ConcurrencySettings",8,32},
        {"bPersistAcrossLevelTransition",1,40},{"bAutoDestroy",1,41},{"ReturnValue",8,48}});
}
static void sound_install_2d(){
    if(!native_bind_vm())return;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(!api.weakget(&sound_play_2d_hook.function)&&sound_stock_call(image,0x2dc215f,0x28ecac0)&&
       medic_params(world_sound_play_2d,49,{{"WorldContextObject",8,0},{"Sound",8,8},{"VolumeMultiplier",4,16},
        {"PitchMultiplier",4,20},{"StartTime",4,24},{"ConcurrencySettings",8,32},{"OwningActor",8,40},{"bIsUISound",1,48}})){
        constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x7c,0x24,0x18,0x55,0x48,
            0x8b,0xec,0x48,0x83,0xec,0x70,0x33,0xff,0x48,0x8b,0xda,0x48};
        sound_stock_play_2d=reinterpret_cast<Sound2DPlay>(image+0x28ecac0);
        native_install_exec(world_input(world_sound_play_2d),0x2dc1f20,prefix,sizeof(prefix),sound_play_2d_hook.function,
                            sound_play_2d_hook.original,&observe_sound_play_2d);
    }
    constexpr unsigned char prefix[]{0x40,0x55,0x53,0x56,0x57,0x41,0x56,0x48,0x8b,0xec,0x48,0x83,
        0xec,0x60,0x33,0xff,0x4d,0x8b,0xf0,0x48,0x89,0x7d,0xf8,0x48};
    if(!api.weakget(&sound_spawn_2d_hook.function)&&sound_stock_call(image,0x2dc611f,0x28f6700)&&sound_2d_spawn_layout(world_sound_spawn_2d)){
        sound_stock_spawn_2d=reinterpret_cast<Sound2DSpawn>(image+0x28f6700);
        native_install_exec(world_input(world_sound_spawn_2d),0x2dc5ee0,prefix,sizeof(prefix),sound_spawn_2d_hook.function,
                            sound_spawn_2d_hook.original,&observe_sound_spawn_2d);
    }
    if(!api.weakget(&sound_create_2d_hook.function)&&sound_stock_call(image,0x2dbd6ef,0x28d7ca0)&&sound_2d_spawn_layout(world_sound_create_2d)){
        sound_stock_create_2d=reinterpret_cast<Sound2DSpawn>(image+0x28d7ca0);
        native_install_exec(world_input(world_sound_create_2d),0x2dbd4b0,prefix,sizeof(prefix),sound_create_2d_hook.function,
                            sound_create_2d_hook.original,&observe_sound_create_2d);
    }
}
