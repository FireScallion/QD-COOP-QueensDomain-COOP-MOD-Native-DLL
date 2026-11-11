namespace SoundLoop=Sound::loop;
struct SoundLoopSource {
    SoundLoop::Frame frame;
    Weak component,asset,anchor;
    int sound_at{},volume_at{},pitch_at{};
    double next{},expires{},began{},fade_until{};
    bool anchored{};
};
struct SoundLoopVoice {
    SoundLoop::Track track;
    Weak component;
    std::uint32_t generation{};
    std::uint32_t fade_revision{};
    SoundLoop::Phase fade_phase{SoundLoop::Phase::playing};
    double retry{};
    Sound::Event applied;
};
static std::array<SoundLoopSource,SoundLoop::capacity> sound_loop_sources;
static std::array<std::array<SoundLoopVoice,SoundLoop::capacity>,Player::slots> sound_loop_voices;
static bool sound_loop_bound;
static double sound_loop_next_update;
static double sound_ambient_next;
static unsigned sound_ambient_cursor;
static SoundHook sound_loop_fade_hook,sound_loop_fade_in_hook;

static bool sound_loop_bind(){
    if(!sound_loop_bound)
        sound_loop_bound=medic_params(world_sound_playing,1,{{"ReturnValue",1,0}})&&
            medic_params(world_view_move,153,{{"NewLocation",12,0},{"bSweep",1,12},{"bTeleport",1,152}})&&
            medic_params(world_sound_volume,4,{{"NewVolumeMultiplier",4,0}})&&
            medic_params(world_sound_pitch,4,{{"NewPitchMultiplier",4,0}});
    return sound_loop_bound;
}
static bool sound_loop_active(void* component){
    const auto playing=world_input(world_sound_playing);
    if(!component||!playing)return false;
    unsigned char result{};
    api.event(component,playing,&result);
    return result!=0;
}
static void sound_loop_end(SoundLoopSource& source,double time){
    const auto previous=source.frame.voice;
    source.frame.voice={previous.serial,previous.slot,previous.origin};
    source.frame.phase=SoundLoop::Phase::stopped;
    source.frame.fade={};
    source.component=source.asset=source.anchor={};
    source.expires=time+.55;
    source.next=time;
    ++sound_loop_retired;
}
static void sound_loop_publish(SoundLoopSource& source,double time){
    source.next=time+.10;
    ++source.frame.voice.serial;
    E::Packet packet{};
    if(SoundLoop::encode(envelope,source.frame,packet)&&send(packet,active_peer_mask))++sound_loop_sent;
}
static SoundLoopSource* sound_loop_find(void* component){
    if(!component)return nullptr;
    for(auto& source:sound_loop_sources)
        if(source.frame.id&&source.frame.phase!=SoundLoop::Phase::stopped&&api.weakget(&source.component)==component)return &source;
    return nullptr;
}
static void sound_loop_requested_fade(void* component,const SoundFadeParams& args,double time){
    auto source=sound_loop_find(component);
    if(!source||args.curve>3||!std::isfinite(args.duration)||!std::isfinite(args.target)||args.duration<=0.f)return;
    auto& fade=source->frame.fade;
    if(!++fade.revision)++fade.revision;
    fade.curve=args.curve;
    fade.target=std::clamp(args.target,0.f,8.f);
    fade.remaining=std::min(args.duration,10.f);
    source->frame.phase=SoundLoop::Phase::fading;
    source->fade_until=time+fade.remaining;
    ++sound_loop_fade_observed;
    sound_loop_publish(*source,time);
}
static void observe_sound_loop_fade(void* context,void* frame,void* result){
    const auto stock=sound_loop_fade_hook.original.load(std::memory_order_acquire);
    if(GetCurrentThreadId()!=game_thread.load()||sound_rendering||!active_peer_mask||!envelope.world||!frame||
       (!sound_loop_find(context)&&!sound_control_source(context))){
        stock(context,frame,result);
        return;
    }
    const auto function=api.weakget(&sound_loop_fade_hook.function);
    if(!function){
        stock(context,frame,result);
        return;
    }
    SoundFadeParams args{};
    auto& code=*reinterpret_cast<const unsigned char**>(static_cast<unsigned char*>(frame)+32);
    if(code){
        native_vm_argument(frame,&args.duration);
        native_vm_argument(frame,&args.target);
        native_vm_argument(frame,&args.curve);
        if(code)++code;
        sound_control_fade(context,args);
        sound_gameplay_event(context,function,&args);
    }else{
        const auto parameters=AnimU::field<const void*>(frame,40);
        if(!parameters||AnimU::field<void*>(frame,16)!=function){
            stock(context,frame,result);
            return;
        }
        args={AnimU::field<float>(parameters,0),AnimU::field<float>(parameters,4),AnimU::field<unsigned char>(parameters,8)};
        sound_control_fade(context,args);
        stock(context,frame,result);
    }
    sound_loop_requested_fade(context,args,now());
}
static void observe_sound_loop_fade_in(void* context,void* frame,void* result){
    const auto stock=sound_loop_fade_in_hook.original.load(std::memory_order_acquire);
    if(GetCurrentThreadId()!=game_thread.load()||sound_rendering||!active_peer_mask||!envelope.world||!frame||
       (sound_origin(context)==Sound::Origin{}&&!sound_prepared_author(context))){
        stock(context,frame,result);
        return;
    }
    const auto function=api.weakget(&sound_loop_fade_in_hook.function);
    if(!function){
        stock(context,frame,result);
        return;
    }
    SoundFadeInParams args{};
    auto& code=*reinterpret_cast<const unsigned char**>(static_cast<unsigned char*>(frame)+32);
    if(code){
        native_vm_argument(frame,&args.duration);
        native_vm_argument(frame,&args.target);
        native_vm_argument(frame,&args.start);
        native_vm_argument(frame,&args.curve);
        if(code)++code;
        sound_gameplay_event(context,function,&args);
    }else{
        const auto parameters=AnimU::field<const void*>(frame,40);
        if(!parameters||AnimU::field<void*>(frame,16)!=function){
            stock(context,frame,result);
            return;
        }
        args={AnimU::field<float>(parameters,0),AnimU::field<float>(parameters,4),AnimU::field<float>(parameters,8),
              AnimU::field<unsigned char>(parameters,12)};
        stock(context,frame,result);
    }
    sound_capture_component(context,context,args.start,&args);
}
static void sound_loop_install_fades(){
    if(!native_bind_vm())return;
    const auto out=world_input(world_sound_fade_out);
    if(out&&!api.weakget(&sound_loop_fade_hook.function)&&
       medic_params(world_sound_fade_out,9,{{"FadeOutDuration",4,0},{"FadeVolumeLevel",4,4},{"FadeCurve",1,8}})&&
       medic_field_class(catalog_field(out,"FadeCurve",1),"EnumProperty")){
        constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x18,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xda,0xc7,
            0x44,0x24,0x48,0x00,0x00,0x00,0x00,0x48,0x8b,0xf9};
        native_install_exec(out,0x2d76310,prefix,sizeof(prefix),sound_loop_fade_hook.function,
                            sound_loop_fade_hook.original,&observe_sound_loop_fade);
    }
    const auto in=world_input(world_sound_fade_in);
    if(in&&!api.weakget(&sound_loop_fade_in_hook.function)&&
       medic_params(world_sound_fade_in,13,{{"FadeInDuration",4,0},{"FadeVolumeLevel",4,4},{"StartTime",4,8},{"FadeCurve",1,12}})&&
       medic_field_class(catalog_field(in,"FadeCurve",1),"EnumProperty")){
        constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x18,0x57,0x48,0x83,0xec,0x40,0x48,0x8b,0xda,0xc7,
            0x44,0x24,0x30,0x00,0x00,0x00,0x00,0x48,0x8b,0xf9};
        native_install_exec(in,0x2d761b0,prefix,sizeof(prefix),sound_loop_fade_in_hook.function,
                            sound_loop_fade_in_hook.original,&observe_sound_loop_fade_in);
    }
}
static bool sound_loop_restore_fades(){
    const bool out=native_restore_exec(sound_loop_fade_hook.function,sound_loop_fade_hook.original,&observe_sound_loop_fade);
    const bool in=native_restore_exec(sound_loop_fade_in_hook.function,sound_loop_fade_in_hook.original,&observe_sound_loop_fade_in);
    return out&&in;
}
static void sound_loop_start_ramp(SoundLoopSource& source,const SoundFadeInParams* args,double time){
    if(!args||args->curve>3||!std::isfinite(args->duration)||!std::isfinite(args->target)||args->duration<=0.f)return;
    auto& frame=source.frame;
    frame.phase=SoundLoop::Phase::fading_in;
    frame.fade={1,args->curve,std::min(args->duration,10.f),std::clamp(args->target,0.f,8.f)};
    source.fade_until=time+frame.fade.remaining;
    ++sound_loop_fade_in_observed;
    sound_loop_publish(source,time);
}
static void sound_loop_capture(void* component,const Sound::Event& event,int sound_at,int volume_at,int pitch_at,
                               const SoundFadeInParams* fade,void* anchor){
    if(!sound_loop_bind()||!sound_loop_active(component)||!Sound::valid_voice(event))return;
    const auto time=now();
    for(auto& source:sound_loop_sources)if(source.frame.id&&api.weakget(&source.component)==component){
        if(source.frame.voice.asset==event.asset&&(fade?!source.frame.voice.serial:time-source.began<.025)){
            if(fade){
                source.frame.voice=event;
                sound_loop_start_ramp(source,fade,time);
            }
            return;
        }
        sound_loop_end(source,time);
    }
    Weak component_key{},asset_key{},anchor_key{};
    if(!makeweak(component_key,component)||!makeweak(asset_key,AnimU::field<void*>(component,sound_at))||
       (anchor&&!makeweak(anchor_key,anchor)))return;
    for(unsigned i=0;i<sound_loop_sources.size();++i){
        auto& source=sound_loop_sources[i];
        if(source.frame.id)continue;
        const auto generation=source.frame.generation==UINT32_MAX?1:source.frame.generation+1;
        source={{event,i+1,generation,SoundLoop::Phase::playing},component_key,asset_key,anchor_key,
                sound_at,volume_at,pitch_at,0,0,time};
        source.anchored=anchor!=nullptr;
        ++sound_loop_observed;
        sound_loop_start_ramp(source,fade,time);
        return;
    }
}
static void sound_loop_discover_ambient(double time){
    if(!host_role||time<sound_ambient_next)return;
    sound_ambient_next=time+.05;
    const auto enumerate=factory_object(factory_audio),audio=world_input(world_sound_component);
    const auto world=api.weakget(&attached_world);
    if(!enumerate||!audio||!world||!native_free||!sound_loop_bind())return;
    void* actor{};
    for(unsigned offset=0;offset<entities.size();++offset){
        const auto& candidate=entities[sound_ambient_cursor++%entities.size()];
        if(!candidate.used)continue;
        if(const auto object=api.weakget(&candidate.actor);object&&native_actor_world(object)==world){
            actor=object;
            break;
        }
    }
    if(!actor)return;
    alignas(16) FactoryParameters args{};
    factory_put(args,factory_offsets.audio_class,audio);
    api.event(actor,enumerate,args.data());
    const auto components=AnimU::field<NativeActorArray>(args.data(),factory_offsets.audio_result);
    if(components.data&&components.count>0&&components.count<=32&&components.capacity>=components.count){
        const auto before=sound_loop_observed;
        for(int i=0;i<components.count;++i){
            const auto component=components.data[i];
            if(component&&!sound_loop_find(component)&&sound_loop_active(component))
                sound_capture_component(actor,component,0,nullptr,nullptr,true);
        }
        sound_loop_ambient_discovered+=sound_loop_observed-before;
    }
    if(components.data)native_free(components.data);
}
static void sound_loop_send(double time){
    for(auto& source:sound_loop_sources){
        auto& frame=source.frame;
        if(!frame.id||time<source.next)continue;
        if(source.expires&&time>=source.expires){
            frame.id=0;
            continue;
        }
        if(frame.phase!=SoundLoop::Phase::stopped){
            const auto component=api.weakget(&source.component),asset=api.weakget(&source.asset);
            const bool active=component&&asset&&AnimU::field<void*>(component,source.sound_at)==asset&&sound_loop_active(component);
            const bool fading=frame.phase==SoundLoop::Phase::fading;
            if(fading?time>=source.fade_until:!active){
                sound_loop_end(source,time);
            }else if(active){
                if(const auto anchor=api.weakget(&source.anchor))frame.voice.position=native_actor_location(anchor);
                else if(!source.anchored)frame.voice.position=projectile_transform(component).position;
                frame.voice.volume=std::clamp(AnimU::field<float>(component,source.volume_at),0.f,8.f);
                frame.voice.pitch=std::clamp(AnimU::field<float>(component,source.pitch_at),.1f,4.f);
            }
            if(frame.phase==SoundLoop::Phase::fading_in&&time>=source.fade_until){
                frame.phase=SoundLoop::Phase::playing;
                frame.fade={};
            }else if(SoundLoop::fading(frame.phase))
                frame.fade.remaining=std::clamp(static_cast<float>(source.fade_until-time),.01f,10.f);
        }
        sound_loop_publish(source,time);
    }
}
static bool receive_sound_loop(const ReceivedPacket& received,double time){
    SoundLoop::Frame frame;
    unsigned relay{};
    if(!automatic_limit||!active_peer_mask||!SoundLoop::decode(envelope,received.packet,frame)||
       !Sound::accept_origin(frame.voice,host_role,received.peer,active_peer_mask,relay)){
        ++sound_rejected;
        return true;
    }
    auto& track=sound_loop_voices[frame.voice.slot][frame.id-1].track;
    const bool retired=frame.phase==SoundLoop::Phase::stopped&&
        (track.last.generation!=frame.generation||track.last.phase!=SoundLoop::Phase::stopped);
    const bool faded=SoundLoop::fading(frame.phase)&&
        (track.last.generation!=frame.generation||track.last.phase!=frame.phase||track.last.fade.revision!=frame.fade.revision);
    if(!track.push(frame,time))return true;
    ++sound_loop_received;
    if(retired)++sound_loop_retired;
    if(faded)++(frame.phase==SoundLoop::Phase::fading_in?sound_loop_fade_in_received:sound_loop_fade_received);
    E::Packet packet{};
    if(relay&&SoundLoop::encode(envelope,frame,packet)&&send(packet,relay))++sound_loop_sent;
    return true;
}
static bool sound_loop_stop(SoundLoopVoice& voice){
    if(voice.generation){
        if(!sound_stop_voice(voice.component))return false;
        voice.generation=0;
        voice.fade_revision=0;
        voice.fade_phase=SoundLoop::Phase::playing;
        voice.retry=0;
        ++sound_loop_stopped;
    }
    return true;
}
static void sound_loop_apply_fade(void* component,const SoundLoop::Frame& frame,SoundLoopVoice& voice){
    if(!SoundLoop::fading(frame.phase)||(voice.fade_phase==frame.phase&&voice.fade_revision==frame.fade.revision))return;
    if(frame.phase==SoundLoop::Phase::fading_in){
        const auto function=api.weakget(&sound_loop_fade_in_hook.function);
        if(!function)return;
        float volume=frame.voice.volume;
        api.event(component,world_input(world_sound_volume),&volume);
        SoundFadeInParams args{frame.fade.remaining,frame.fade.target,frame.voice.start,static_cast<unsigned char>(frame.fade.curve)};
        sound_gameplay_event(component,function,&args);
        ++sound_loop_fade_in_applied;
    }else{
        const auto function=api.weakget(&sound_loop_fade_hook.function);
        if(!function)return;
        SoundFadeParams args{frame.fade.remaining,frame.fade.target,static_cast<unsigned char>(frame.fade.curve)};
        sound_gameplay_event(component,function,&args);
        ++sound_loop_fade_applied;
    }
    voice.fade_phase=frame.phase;
    voice.fade_revision=frame.fade.revision;
}
static void sound_loop_apply(void* component,const Sound::Event& event,Sound::Event& previous){
    if(event.position!=previous.position){
        alignas(16) FactoryParameters args{};
        factory_put(args,0,event.position);
        args[152]=1;
        api.event(component,world_input(world_view_move),args.data());
        ++sound_loop_moved;
    }
    if(event.volume!=previous.volume){
        float volume=event.volume;
        api.event(component,world_input(world_sound_volume),&volume);
    }
    if(event.pitch!=previous.pitch){
        float pitch=event.pitch;
        api.event(component,world_input(world_sound_pitch),&pitch);
    }
    previous=event;
}
static void sound_loop_step(void* context,double time){
    if(time<sound_loop_next_update||!sound_loop_bind())return;
    sound_loop_next_update=time+.05;
    sound_loop_send(time);
    for(auto& seat:sound_loop_voices)for(auto& voice:seat){
        const auto& frame=voice.track.last;
        const bool playing=voice.track.playing(time);
        if(voice.generation&&(!playing||voice.generation!=frame.generation)&&!sound_loop_stop(voice))continue;
        if(!playing)continue;
        if(const auto component=api.weakget(&voice.component)){
            if(voice.applied.serial!=frame.voice.serial)sound_loop_apply(component,frame.voice,voice.applied);
            sound_loop_apply_fade(component,frame,voice);
            continue;
        }
        if(SoundLoop::fading(frame.phase)&&voice.generation==frame.generation&&
           voice.fade_phase==frame.phase&&voice.fade_revision==frame.fade.revision)continue;
        if(time<voice.retry)continue;
        voice.retry=time+.25;
        void* component{};
        auto initial=frame.voice;
        // The stock FadeIn restarts an existing voice without auto-destroying it.
        // Create its spatial component silently to avoid an un-ramped first sample.
        if(frame.phase==SoundLoop::Phase::fading_in)initial.volume=0.f;
        if(sound_spawn(context,initial,time,component)&&component&&makeweak(voice.component,component)){
            voice.generation=frame.generation;
            voice.applied=frame.voice;
            voice.fade_revision=0;
            voice.fade_phase=SoundLoop::Phase::playing;
            sound_loop_apply_fade(component,frame,voice);
            ++sound_loop_played;
        }
    }
}
static bool sound_loop_cleanup(){
    bool clean=true;
    for(auto& seat:sound_loop_voices)for(auto& voice:seat)if(!sound_loop_stop(voice))clean=false;
    return clean;
}
static void sound_loop_reset(){
    sound_loop_sources={};
    sound_loop_voices={};
    sound_loop_bound=false;
    sound_loop_next_update=0;
    sound_ambient_next=0;
    sound_ambient_cursor=0;
}
