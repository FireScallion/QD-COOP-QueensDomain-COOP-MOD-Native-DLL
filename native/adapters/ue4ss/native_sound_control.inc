struct SoundControlSource {Weak component;Sound::Control value;};
struct SoundControlRetry {Sound::Control value;double next{};unsigned peers{},remaining{};};
static std::array<SoundControlSource,64> sound_control_sources;
static std::array<SoundControlRetry,64> sound_control_retries;
static SoundHook sound_stop_hook;

static SoundControlSource* sound_control_source(void* component){
    if(!component)return nullptr;
    for(auto& source:sound_control_sources)
        if(source.value.serial&&api.weakget(&source.component)==component)return &source;
    return nullptr;
}
static bool sound_control_matches(const Sound::Control& control,const Sound::Event& event){
    return control.serial==event.serial&&control.slot==event.slot&&control.origin==event.origin;
}
static void sound_publish_control(const Sound::Control& value,unsigned peers,double time){
    for(auto& retry:sound_retries)
        if(retry.remaining&&sound_control_matches(value,retry.event))retry={};
    for(auto& retry:sound_control_retries)
        if(retry.remaining&&retry.value.serial==value.serial&&retry.value.slot==value.slot&&retry.value.origin==value.origin)
            retry={};
    E::Packet packet{};
    if(!peers||!Sound::encode(envelope,value,packet))return;
    if(send(packet,peers))++sound_control_sent;
    for(auto& retry:sound_control_retries)if(!retry.remaining){
        retry={value,time+.045,peers,2};
        return;
    }
}
static void sound_control_change(SoundControlSource& source,const SoundFadeParams* fade){
    if(fade&&(!std::isfinite(fade->duration)||!std::isfinite(fade->target)||fade->curve>3))return;
    auto& value=source.value;
    if(!++value.revision)++value.revision;
    value.duration=fade?std::clamp(fade->duration,0.f,10.f):0.f;
    value.target=value.duration>0.f?std::clamp(fade->target,0.f,8.f):0.f;
    value.curve=value.duration>0.f?fade->curve:0;
    ++sound_control_observed;
    sound_publish_control(value,active_peer_mask,now());
    if(!value.duration)source={};
}
static void sound_control_fade(void* component,const SoundFadeParams& fade){
    if(const auto source=sound_control_source(component))sound_control_change(*source,&fade);
}
static void sound_control_track(void* component,const Sound::Event& event,std::uint64_t serial){
    SoundControlSource prepared;
    if(!makeweak(prepared.component,component))return;
    prepared.value={serial,event.slot,event.origin};
    if(const auto existing=sound_control_source(component)){
        sound_control_change(*existing,nullptr);
        *existing=prepared;
        return;
    }
    for(auto& source:sound_control_sources)if(!api.weakget(&source.component)){
        source=prepared;
        return;
    }
}
static void observe_sound_stop(void* context,void* frame,void* result){
    const auto original=sound_stop_hook.original.load(std::memory_order_acquire);
    if(GetCurrentThreadId()==game_thread.load()&&!sound_rendering&&active_peer_mask&&envelope.world)
        if(const auto source=sound_control_source(context))sound_control_change(*source,nullptr);
    original(context,frame,result);
}
static void sound_control_install(){
    if(api.weakget(&sound_stop_hook.function)||!medic_params(world_sound_stop,0,{}))return;
    constexpr unsigned char prefix[]{0x48,0x8b,0x42,0x20,0x45,0x33,0xc0,0x48,0x85,0xc0,0x41,0x0f,
        0x95,0xc0,0x4c,0x03,0xc0,0x4c,0x89,0x42,0x20,0x48,0x8b,0x01};
    native_install_exec(world_input(world_sound_stop),0x2d77db0,prefix,sizeof(prefix),sound_stop_hook.function,
                        sound_stop_hook.original,&observe_sound_stop);
}
static void sound_control_apply(const Sound::Control& control){
    for(auto& voice:sound_voices){
        if(voice.serial!=control.serial||voice.slot!=control.slot||voice.origin!=control.origin||
           voice.revision>=control.revision)continue;
        const auto component=api.weakget(&voice.component);
        if(!component)return;
        const auto fade=world_input(world_sound_fade_out);
        if(control.duration>0.f&&fade&&medic_params(world_sound_fade_out,9,
           {{"FadeOutDuration",4,0},{"FadeVolumeLevel",4,4},{"FadeCurve",1,8}})){
            SoundFadeParams args{control.duration,control.target,static_cast<unsigned char>(control.curve)};
            sound_gameplay_event(component,fade,&args);
            voice.revision=control.revision;
        }else{
            if(!sound_stop_voice(voice.component))return;
            voice={};
            ++sound_stopped;
        }
        ++sound_control_applied;
        return;
    }
}
static bool receive_sound_control(const ReceivedPacket& received,double time){
    Sound::Control value;
    unsigned relay{};
    if(!automatic_limit||!active_peer_mask||!Sound::decode(envelope,received.packet,value)||
       !Sound::accept_origin(value,host_role,received.peer,active_peer_mask,relay)){
        ++sound_rejected;
        return true;
    }
    if(!sound_control_history[value.slot].push(value))return true;
    ++sound_control_received;
    for(auto& pending:sound_pending)
        if(sound_control_matches(value,pending.event))pending={};
    sound_publish_control(value,relay,time);
    sound_control_apply(value);
    return true;
}
static void sound_control_step(double time){
    for(auto& retry:sound_control_retries)if(retry.remaining&&time>=retry.next){
        E::Packet packet{};
        if(const auto peers=retry.peers&active_peer_mask;peers&&Sound::encode(envelope,retry.value,packet)&&send(packet,peers))
            ++sound_control_sent;
        --retry.remaining;
        retry.next=time+.045;
    }
}
static bool sound_control_restore(){
    return native_restore_exec(sound_stop_hook.function,sound_stop_hook.original,&observe_sound_stop);
}
static void sound_control_reset(){
    sound_control_sources={};
    sound_control_retries={};
    sound_control_history={};
}
