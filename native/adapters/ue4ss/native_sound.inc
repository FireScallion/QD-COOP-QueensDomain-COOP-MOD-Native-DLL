namespace Sound=qdcoop::sound;
struct SoundHook {Weak function;std::atomic<KillExec> original{};};
struct SoundRetry {Sound::Event event;double next{};unsigned peers{},remaining{};};
struct SoundPending {Sound::Event event;double until{};};
struct SoundPrepared {Weak component,author;};
struct SoundVoice {Weak component;std::uint64_t serial{};unsigned slot{},revision{};Sound::Origin origin{};};
static SoundHook sound_location_hook,sound_attached_hook,sound_play_hook,sound_component_hook;
static std::array<SoundRetry,64> sound_retries;
static std::array<SoundPending,64> sound_pending;
static std::array<Hit::ReplayWindow,Player::slots> sound_replay;
static std::array<SoundVoice,64> sound_voices;
static std::array<SoundPrepared,64> sound_prepared;
static std::array<Sound::ControlHistory,Player::slots> sound_control_history;
static std::array<NativeVisualAsset,Sound::assets::sounds.size()> sound_assets;
static std::array<NativeVisualAsset,Sound::assets::attenuations.size()> sound_attenuations;
static std::uint64_t sound_serial;
static bool sound_rendering;

static void sound_gameplay_event(void* context,void* function,void* params){
    const bool previous=sound_rendering;
    sound_rendering=true;
    api.event(context,function,params);
    sound_rendering=previous;
}

static void sound_publish(const Sound::Event& event,unsigned peers,double time){
    E::Packet packet{};
    if(!peers||!Sound::encode(envelope,event,packet))return;
    if(send(packet,peers))++sound_sent;
    for(auto& retry:sound_retries)if(!retry.remaining){
        retry={event,time+.045,peers,2};
        return;
    }
}

static Sound::Origin sound_origin(void* source){
    source=native_cosmetic_actor(source);
    const auto pawn=api.weakget(&automatic_context),world=api.weakget(&attached_world);
    if(!source||!pawn||!world||!api.isa(source,actor_class)||native_actor_world(source)!=world)return {};
    if(api.isa(source,base_class)||enemy_projectile_class(AnimU::field<void*>(source,16)))
        return host_role?Sound::Origin::enemy:Sound::Origin{};
    if(source==pawn||projectile_local_owner(source,pawn))return Sound::Origin::player;
    return {};
}
static void* sound_spatial_anchor(Sound::Origin origin,void* author){
    return origin==Sound::Origin::player?api.weakget(&automatic_context):native_cosmetic_actor(author);
}
static bool sound_mute_replica(void* source,float& volume){
    if(!native_replica_author(source))return false;
    if(volume>0.f)++sound_replica_muted;
    volume=0.f;
    return true;
}
static void sound_call_with_mute(KillExec original,void* context,void* frame,void* result,void* params,
                                 unsigned volume_at,bool muted){
    if(!muted){
        original(context,frame,result);
        return;
    }
    auto volume=static_cast<unsigned char*>(params)+volume_at;
    const auto saved=AnimU::field<float>(volume,0);
    const float silence{};
    std::memcpy(volume,&silence,4);
    original(context,frame,result);
    std::memcpy(volume,&saved,4);
}
static bool sound_stock_call(std::uintptr_t image,std::uintptr_t call,std::uintptr_t stock){
    const auto instruction=reinterpret_cast<const unsigned char*>(image+call);
    return *instruction==0xe8&&image+call+5+AnimU::field<std::int32_t>(instruction,1)==image+stock;
}
static void* sound_prepared_author(void* component){
    for(const auto& voice:sound_prepared)
        if(api.weakget(&voice.component)==component)return api.weakget(&voice.author);
    return nullptr;
}
static void sound_prepare_2d(void* component,void* author){
    const auto type=world_input(world_sound_component);
    if(!component||!type||!api.isa(component,type)||sound_origin(author)==Sound::Origin{})return;
    SoundPrepared* slot{};
    for(auto& voice:sound_prepared){
        const auto current=api.weakget(&voice.component);
        if(current==component){
            slot=&voice;
            break;
        }
        if(!current&&!slot)slot=&voice;
    }
    SoundPrepared prepared;
    if(slot&&makeweak(prepared.component,component)&&makeweak(prepared.author,author))*slot=prepared;
}

static Sound::Event sound_event(Sound::Origin origin,void* asset,void* attenuation,const std::array<float,3>& location,
                                float volume,float pitch,float start){
    if(origin!=Sound::Origin::player&&origin!=Sound::Origin::enemy)return {};
    std::string path;
    if(!catalog_path(asset,path))return {};
    Sound::Event event;
    event.asset=Sound::assets::sound_id(path);
    if(!event.asset)return {};
    if(attenuation&&catalog_path(attenuation,path))event.attenuation=Sound::assets::attenuation_id(path);
    event.origin=origin;
    event.slot=host_role?0:Player::unassigned;
    event.position=location;
    event.volume=std::clamp(volume,0.f,8.f);
    event.pitch=std::clamp(pitch,.1f,4.f);
    event.start=std::clamp(start,0.f,600.f);
    return event;
}
static std::uint64_t sound_emit(Sound::Event event){
    if(!event.asset)return 0;
    event.serial=++sound_serial;
    if(!Sound::valid(event))return 0;
    ++sound_observed;
    sound_publish(event,active_peer_mask,now());
    return event.serial;
}
struct SoundFadeParams {
    float duration{},target{};
    unsigned char curve{};
};
static_assert(offsetof(SoundFadeParams,curve)==8);
struct SoundFadeInParams {
    float duration{},target{},start{};
    unsigned char curve{};
};
static_assert(offsetof(SoundFadeInParams,curve)==12);
static void sound_loop_capture(void* component,const Sound::Event& event,int sound_at,int volume_at,int pitch_at,
                               const SoundFadeInParams* fade,void* anchor);
static void sound_control_track(void* component,const Sound::Event& event,std::uint64_t serial);
static void sound_capture_component(void* caller,void* component,float start,const SoundFadeInParams* fade=nullptr,
                                    void* anchor=nullptr,bool looping_only=false){
    if(!component||!api.isa(component,world_input(world_sound_component)))return;
    if(caller==component)
        if(const auto author=sound_prepared_author(component)){
            caller=author;
            const auto origin=sound_origin(author);
            if(origin!=Sound::Origin::player&&origin!=Sound::Origin::enemy)return;
            anchor=sound_spatial_anchor(origin,author);
        }
    const auto origin=sound_origin(caller);
    if(origin!=Sound::Origin::player&&origin!=Sound::Origin::enemy)return;
    const auto type=AnimU::field<void*>(component,16);
    const auto sound=catalog_field(type,"Sound",8),volume=catalog_field(type,"VolumeMultiplier",4);
    const auto pitch=catalog_field(type,"PitchMultiplier",4);
    if(!medic_field_class(sound,"ObjectProperty")||!medic_field_class(volume,"FloatProperty")||
       !medic_field_class(pitch,"FloatProperty"))return;
    const auto sound_at=*api.offset(sound),volume_at=*api.offset(volume),pitch_at=*api.offset(pitch);
    const auto event=sound_event(origin,AnimU::field<void*>(component,sound_at),
        anchor?nullptr:catalog_object_field(component,"AttenuationSettings"),
        anchor?native_actor_location(anchor):projectile_transform(component).position,
        AnimU::field<float>(component,volume_at),AnimU::field<float>(component,pitch_at),start);
    if(!event.asset)return;
    if(Sound::assets::sounds[event.asset-1].looping)sound_loop_capture(component,event,sound_at,volume_at,pitch_at,fade,anchor);
    else if(!looping_only)
        if(const auto serial=sound_emit(event))sound_control_track(component,event,serial);
}

#include "native_sound_spatial.inc"
static void observe_sound_component(void* context,void* frame,void* result){
    const auto stock=sound_component_hook.original.load(std::memory_order_acquire);
    if(GetCurrentThreadId()!=game_thread.load()||sound_rendering||!active_peer_mask||!envelope.world){
        stock(context,frame,result);
        return;
    }
    float start{};
    if(frame&&!AnimU::field<const void*>(frame,32)&&
       AnimU::field<void*>(frame,16)==api.weakget(&sound_component_hook.function)){
        if(const auto args=AnimU::field<const void*>(frame,40))start=AnimU::field<float>(args,0);
    }
    stock(context,frame,result);
    sound_capture_component(context,context,start);
}

struct DirectSoundParams {
    void* world{};
    void* sound{};
    SoundVector location{},rotation{};
    float volume{},pitch{},start{};
    void* attenuation{};
    void* concurrency{};
    void* owner{};
};
static_assert(sizeof(SoundVector)==12&&sizeof(DirectSoundParams)==80&&
              offsetof(DirectSoundParams,volume)==40&&offsetof(DirectSoundParams,attenuation)==56);
using StockSoundPlay=void(*)(void*,void*,SoundVector,SoundVector,float,float,float,void*,void*,void*);
static StockSoundPlay stock_sound_play;

static void observe_direct_sound(void* context,void* frame,void* result){
    const auto stock=sound_play_hook.original.load(std::memory_order_acquire);
    if(GetCurrentThreadId()!=game_thread.load()||sound_rendering||!active_peer_mask||!envelope.world||!frame){
        stock(context,frame,result);
        return;
    }
    DirectSoundParams args{};
    auto& code=*reinterpret_cast<const unsigned char**>(static_cast<unsigned char*>(frame)+32);
    const bool blueprint=code!=nullptr;
    bool muted{};
    if(!blueprint){
        const auto params=AnimU::field<void*>(frame,40);
        if(!params||AnimU::field<void*>(frame,16)!=api.weakget(&sound_play_hook.function)){
            stock(context,frame,result);
            return;
        }
        std::memcpy(&args,params,sizeof(args));
        muted=sound_mute_replica(args.world,args.volume)||sound_mute_replica(args.owner,args.volume);
        sound_call_with_mute(stock,context,frame,result,params,40,muted);
    }else{
        // Match the stock exec thunk's typed parameter order. Step evaluates
        // each expression once, including Blueprint calls inside arguments.
        for(auto field:{static_cast<void*>(&args.world),static_cast<void*>(&args.sound),
                        static_cast<void*>(&args.location),static_cast<void*>(&args.rotation),
                        static_cast<void*>(&args.volume),static_cast<void*>(&args.pitch),static_cast<void*>(&args.start),
                        static_cast<void*>(&args.attenuation),static_cast<void*>(&args.concurrency),static_cast<void*>(&args.owner)})
            native_vm_argument(frame,field);
        if(code)++code;
        muted=sound_mute_replica(args.world,args.volume)||sound_mute_replica(args.owner,args.volume);
        stock_sound_play(args.world,args.sound,args.location,args.rotation,args.volume,args.pitch,args.start,
                         args.attenuation,args.concurrency,args.owner);
    }
    if(muted)return;
    auto origin=sound_origin(args.world);
    if(origin==Sound::Origin{})origin=sound_origin(args.owner);
    if(sound_emit(sound_event(origin,args.sound,args.attenuation,args.location,args.volume,args.pitch,args.start)))
        ++(blueprint?sound_direct_blueprint:sound_direct_reflected);
}
static void sound_install_direct(){
    if(api.weakget(&sound_play_hook.function)||!world_actor_can_sound()||!native_bind_vm())return;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto play=image+0x28ecca0;
    constexpr unsigned char play_prefix[]{0x48,0x85,0xd2,0x0f,0x84,0x20,0x01,0x00,0x00,0x48,0x89,0x5c,0x24,0x08};
    constexpr unsigned char exec_prefix[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x7c,0x24,0x18,0x55,0x48,0x8d,0x6c,0x24,0xa9,
        0x48,0x81,0xec,0xc0,0x00,0x00,0x00,0x33};
    if(std::memcmp(reinterpret_cast<void*>(play),play_prefix,sizeof(play_prefix)))return;
    stock_sound_play=reinterpret_cast<StockSoundPlay>(play);
    native_install_exec(world_input(world_actor_play_sound),0x2dc2180,exec_prefix,sizeof(exec_prefix),
                        sound_play_hook.function,sound_play_hook.original,&observe_direct_sound);
}

static bool sound_location_layout(){
    return medic_params(world_sound_at_location,88,{{"WorldContextObject",8,0},{"Sound",8,8},{"Location",12,16},
        {"Rotation",12,28},{"VolumeMultiplier",4,40},{"PitchMultiplier",4,44},{"StartTime",4,48},
        {"AttenuationSettings",8,56},{"ConcurrencySettings",8,64},{"bAutoDestroy",1,72},{"ReturnValue",8,80}});
}
#include "native_sound_2d.inc"
static void sound_install(){
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const bool vm=native_bind_vm();
    if(!api.weakget(&sound_location_hook.function)&&vm&&sound_location_layout()&&sound_stock_call(image,0x2dc6427,0x28f6770)){
        constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x08,0x55,0x56,0x57,0x48,0x8d,0x6c,0x24,0xb9,
            0x48,0x81,0xec,0xc0,0x00,0x00,0x00,0x33,0xff,0x49,0x8b};
        sound_stock_location=reinterpret_cast<StockSoundLocation>(image+0x28f6770);
        native_install_exec(world_input(world_sound_at_location),0x2dc6140,prefix,sizeof(prefix),sound_location_hook.function,
                            sound_location_hook.original,&observe_sound_location);
    }
    if(!api.weakget(&sound_attached_hook.function)&&vm&&sound_stock_call(image,0x2dc6806,0x28f69a0)&&
       medic_params(world_sound_attached,96,{{"Sound",8,0},
        {"AttachToComponent",8,8},{"AttachPointName",8,16},{"Location",12,24},{"Rotation",12,36},{"LocationType",1,48},
        {"bStopWhenAttachedToDestroyed",1,49},{"VolumeMultiplier",4,52},{"PitchMultiplier",4,56},{"StartTime",4,60},
        {"AttenuationSettings",8,64},{"ConcurrencySettings",8,72},{"bAutoDestroy",1,80},{"ReturnValue",8,88}})){
        constexpr unsigned char prefix[]{0x40,0x55,0x53,0x56,0x57,0x41,0x56,0x48,0x8d,0x6c,0x24,0xc9,
            0x48,0x81,0xec,0xf0,0x00,0x00,0x00,0x33,0xff,0x4d,0x8b,0xf0};
        sound_stock_attachment=reinterpret_cast<StockSoundAttachment>(image+0x28f69a0);
        native_install_exec(world_input(world_sound_attached),0x2dc6450,prefix,sizeof(prefix),sound_attached_hook.function,
                            sound_attached_hook.original,&observe_sound_attached);
    }
    sound_install_direct();
    sound_install_2d();
    if(!api.weakget(&sound_component_hook.function)&&medic_params(world_sound_play,4,{{"StartTime",4,0}})){
        constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x0f,0x57,0xc0,0x48,
            0x8b,0xda,0xf3,0x0f,0x11,0x44,0x24,0x30,0x48,0x8b};
        native_install_exec(world_input(world_sound_play),0x2d76890,prefix,sizeof(prefix),sound_component_hook.function,
                            sound_component_hook.original,&observe_sound_component);
    }
}

static bool receive_sound_loop(const ReceivedPacket& received,double time);
static bool receive_sound_control(const ReceivedPacket& received,double time);
static bool receive_sound(const ReceivedPacket& received,unsigned kind,double time){
    if(kind==Sound::loop::kind)return receive_sound_loop(received,time);
    if(kind==Sound::control_kind)return receive_sound_control(received,time);
    if(kind!=Sound::kind)return false;
    Sound::Event event;
    unsigned relay{};
    if(!automatic_limit||!active_peer_mask||!Sound::decode(envelope,received.packet,event)||
       !Sound::accept_origin(event,host_role,received.peer,active_peer_mask,relay)){
        ++sound_rejected;
        return true;
    }
    auto& replay=sound_replay[event.slot];
    if(replay.known(event.serial))return true;
    replay.accept(event.serial);
    if(sound_control_history[event.slot].find(event.serial))return true;
    ++sound_received;
    sound_publish(event,relay,time);
    for(auto& pending:sound_pending)if(!pending.event.serial){
        pending={event,time+.35};
        return true;
    }
    ++sound_rejected;
    return true;
}

static bool sound_spatial_attenuation(void* asset){
    const auto type=world_input(world_sound_attenuation);
    if(!asset||!type||!api.isa(asset,type))return false;
    const auto settings=reinterpret_cast<void*>(AnimU::find_inherited_property(
        reinterpret_cast<std::uintptr_t>(type),catalog_name_pool,"Attenuation",catalog_read));
    if(!medic_field_class(settings,"StructProperty")||*api.size(settings)<=0||
       !property(settings,*api.size(settings),*api.propertiesSize(type)))return false;
    const auto structure=AnimU::field<void*>(settings,0x78);
    const auto attenuate=catalog_field(structure,"bAttenuate",1),spatialize=catalog_field(structure,"bSpatialize",1);
    if(!medic_field_class(attenuate,"BoolProperty")||!medic_field_class(spatialize,"BoolProperty"))return false;
    const auto value=static_cast<const unsigned char*>(asset)+*api.offset(settings);
    return api.getbool(attenuate,value,0)&&api.getbool(spatialize,value,0);
}
static void* sound_load_attenuation(std::uint16_t id,double time){
    return native_visual_asset(sound_attenuations[id-1],Sound::assets::attenuations[id-1],
                               world_input(world_sound_attenuation),time);
}
static void* sound_voice_attenuation(void* sound,std::uint16_t id,double time){
    const auto asset=id?sound_load_attenuation(id,time):catalog_object_field(sound,"AttenuationSettings");
    if(sound_spatial_attenuation(asset))return asset;
    constexpr auto fallback=Sound::assets::attenuation_id("/Game/Player/Player_Action_Atten.Player_Action_Atten");
    static_assert(fallback!=0);
    return sound_load_attenuation(fallback,time);
}

static bool sound_spawn(void* context,const Sound::Event& event,double time,void*& voice){
    const auto gameplay=factory_object(factory_gameplay),spawn=api.weakget(&sound_location_hook.function);
    const auto component=world_input(world_sound_component);
    if(!gameplay||!spawn||!component)return false;
    const auto sound=native_visual_asset(sound_assets[event.asset-1],Sound::assets::sounds[event.asset-1].path,
                                        world_input(world_actor_sound_type),time);
    if(!sound)return false;
    const auto attenuation=sound_voice_attenuation(sound,event.attenuation,time);
    if(!attenuation)return false;
    alignas(16) FactoryParameters args{};
    constexpr auto heartbeat=Sound::assets::sound_id(
        "/Game/Player/StatusEffectVisualizers/Weaken/Heartbeat_Cue.Heartbeat_Cue");
    static_assert(heartbeat!=0);
    if(event.asset==heartbeat&&event.origin==Sound::Origin::player){
        if(event.slot>=remote_players.size())return true;
        const auto& source=remote_players[event.slot];
        const auto actor=api.weakget(&source.actor),attach=api.weakget(&sound_attached_hook.function);
        const auto root=catalog_object_field(actor,"RootComponent");
        const auto scene=world_input(world_projectile_scene);
        if(!source.positioned||!actor||!root||!scene||!api.isa(root,scene)||!attach)return false;
        if(native_enemy_distance(native_actor_location(context),native_actor_location(actor))>800.*800.)
            return true;
        factory_put(args,0,sound);
        factory_put(args,8,root);
        args[49]=1;
        factory_put(args,52,std::min(event.volume,1.f)*.6f);
        factory_put(args,56,event.pitch);
        factory_put(args,60,event.start);
        factory_put(args,64,attenuation);
        args[80]=1;
        sound_gameplay_event(gameplay,attach,args.data());
        const auto result=AnimU::field<void*>(args.data(),88);
        if(result&&api.isa(result,component))voice=result;
        return true;
    }
    factory_put(args,0,context);
    factory_put(args,8,sound);
    factory_put(args,16,event.position);
    factory_put(args,40,event.volume);
    factory_put(args,44,event.pitch);
    factory_put(args,48,event.start);
    factory_put(args,56,attenuation);
    args[72]=1;
    sound_gameplay_event(gameplay,spawn,args.data());
    const auto result=AnimU::field<void*>(args.data(),80);
    if(result&&api.isa(result,component))voice=result;
    return true;
}
static bool sound_play(void* context,const Sound::Event& event,double time){
    const auto empty=std::find_if(sound_voices.begin(),sound_voices.end(),
        [](const SoundVoice& voice){return !api.weakget(&voice.component);});
    if(empty==sound_voices.end())return false;
    void* voice{};
    if(!sound_spawn(context,event,time,voice))return false;
    if(voice&&makeweak(empty->component,voice)){
        empty->serial=event.serial;
        empty->slot=event.slot;
        empty->origin=event.origin;
        empty->revision=0;
        ++sound_played;
    }
    return true;
}
static bool sound_stop_voice(Weak& voice){
    if(const auto component=api.weakget(&voice)){
        const auto stop=world_input(world_sound_stop);
        if(!stop||*api.parms(stop)!=0)return false;
        sound_gameplay_event(component,stop,nullptr);
    }
    voice={};
    return true;
}

#include "native_sound_control.inc"
#include "native_sound_loop.inc"

static void native_sound_step(void* context,double time){
    if(!factory_configured||!envelope.world)return;
    if(!active_peer_mask){sound_loop_cleanup();return;}
    sound_install();
    sound_control_install();
    sound_loop_install_fades();
    sound_loop_discover_ambient(time);
    sound_loop_step(context,time);
    sound_control_step(time);
    for(auto& retry:sound_retries)if(retry.remaining&&time>=retry.next){
        E::Packet packet{};
        if(const auto peers=retry.peers&active_peer_mask;peers&&Sound::encode(envelope,retry.event,packet)&&send(packet,peers))
            ++sound_sent;
        --retry.remaining;
        retry.next=time+.045;
    }
    unsigned budget=4;
    for(auto& pending:sound_pending)if(pending.event.serial){
        if(time>=pending.until){pending={};continue;}
        if(!budget)break;
        --budget;
        if(sound_play(context,pending.event,time))pending={};
    }
}

static bool native_sound_cleanup(){
    bool clean=native_restore_exec(sound_location_hook.function,sound_location_hook.original,&observe_sound_location);
    if(!native_restore_exec(sound_attached_hook.function,sound_attached_hook.original,&observe_sound_attached))clean=false;
    if(!native_restore_exec(sound_play_hook.function,sound_play_hook.original,&observe_direct_sound))clean=false;
    if(!native_restore_exec(sound_component_hook.function,sound_component_hook.original,&observe_sound_component))clean=false;
    if(!native_restore_exec(sound_play_2d_hook.function,sound_play_2d_hook.original,&observe_sound_play_2d))clean=false;
    if(!native_restore_exec(sound_spawn_2d_hook.function,sound_spawn_2d_hook.original,&observe_sound_spawn_2d))clean=false;
    if(!native_restore_exec(sound_create_2d_hook.function,sound_create_2d_hook.original,&observe_sound_create_2d))clean=false;
    if(!sound_control_restore())clean=false;
    if(!sound_loop_restore_fades())clean=false;
    if(!sound_loop_cleanup())clean=false;
    for(auto& voice:sound_voices)if(api.weakget(&voice.component)){
        if(!sound_stop_voice(voice.component)){clean=false;continue;}
        ++sound_stopped;
    }
    return clean;
}
static void native_sound_reset(){
    sound_retries={};
    sound_pending={};
    sound_replay={};
    sound_voices={};
    sound_prepared={};
    sound_control_reset();
    sound_assets={};
    sound_attenuations={};
    sound_serial=0;
    sound_loop_reset();
}
