namespace Cine=qdcoop::cutscene;

struct CineCallback {Weak owner,function;};
struct CineDelegate {AnimU::WeakKey owner;AnimU::NameKey function;};
struct CineDelegateList {const CineDelegate* data;int count,capacity;};
static_assert(sizeof(CineDelegate)==16&&sizeof(CineDelegateList)==16);

struct CinePlayback {
    Cine::Message message;
    Weak pawn,actor,sequence,stream;
    std::array<CineCallback,8> callbacks;
    unsigned callback_count{},ready{},finished{},acked{};
    double born{},next_send{},finished_at{},next_bind{},settle_at{},last_message{};
    std::uint64_t commit_revision{};
    NativeTravelGuard skip_guard;
    bool started{},ended{},broadcast{},committed{},held{},recovered{},damage_before{},loaded_before{},visible_before{};
};
struct CineBinding {
    Weak pawn_type;
    void *current{},*damage{};
    std::array<void*,2> travel{};
};
static CinePlayback cine_playback;
static CineBinding cine_binding;
static std::uint64_t cine_serial,cine_completed;
static double cine_next_scan;
static bool cine_methods_ready;
static constexpr std::array<std::string_view,6> cine_stream_paths{
    "/Game/RuinsLevel/Maps/998_QueenCutscenes","/Game/RuinsLevel/Maps/998_QueenCutscenes",
    "/Game/RuinsLevel/Maps/999_SipehaCutscenes","/Game/RuinsLevel/Maps/999_SipehaCutscenes",
    "/Game/RuinsLevel/Maps/997_FinalZoneCutscene","/Game/RuinsLevel/Maps/998_QueenCutscenes"
};

static bool cine_bind_pawn(void* pawn){
    const auto type=AnimU::field<void*>(pawn,16);
    if(api.weakget(&cine_binding.pawn_type)==type&&cine_binding.current)return true;
    CineBinding binding;
    binding.current=catalog_field(type,"CurrentCutsceneManager",8);
    binding.damage=catalog_field(type,"bCanBeDamaged",1);
    binding.travel={catalog_field(type,"Teleporting",1),catalog_field(type,"QuickTeleporting",1)};
    if(!medic_field_class(binding.current,"ObjectProperty")||*api.offset(binding.current)!=5312||
       !medic_field_class(binding.damage,"BoolProperty")||!makeweak(binding.pawn_type,type)||
       std::ranges::any_of(binding.travel,[](auto field){return !medic_field_class(field,"BoolProperty");}))return false;
    cine_binding=binding;
    return true;
}
static void* cine_current(void* pawn){
    return pawn&&cine_bind_pawn(pawn)?AnimU::field<void*>(pawn,*api.offset(cine_binding.current)):nullptr;
}
static bool cine_travelling(void* pawn){
    return pawn&&cine_bind_pawn(pawn)&&std::ranges::any_of(cine_binding.travel,[&](auto field){return api.getbool(field,pawn,0);});
}
static Cine::Manager cine_id(void* manager){
    const auto type=AnimU::field<void*>(manager,16);
    std::string path;
    if(!catalog_path(type,path))return {};
    for(unsigned i=0;i<cine_stream_paths.size();++i)
        if(path==world_paths[world_cine_first+i])return static_cast<Cine::Manager>(i+1);
    return {};
}
static bool cine_bind_methods(){
    if(!life_opcode_slot||!world_input(world_cine_base))return false;
    if(cine_methods_ready)return world_input(world_cine_begin)&&world_input(world_cine_graph);
    cine_methods_ready=medic_params(world_cine_begin,16,{{"Ended",16,0}})&&medic_params(world_cine_skip,0,{})&&
        medic_params(world_cine_sequence,8,{{"SequencePlayer",8,0}})&&medic_params(world_cine_play,0,{})&&
        medic_params(world_cine_pause,0,{})&&medic_params(world_cine_time,16,{{"ReturnValue",16,0}})&&
        medic_params(world_cine_seek,40,{{"PlaybackParams",40,0}})&&
        medic_params(world_cine_lock,0,{})&&medic_params(world_cine_unlock,0,{})&&
        medic_params(world_cine_hide,0,{})&&medic_params(world_cine_unhide,0,{});
    return cine_methods_ready;
}

static bool cine_arm_skip(CinePlayback& scene){
    auto& guard=scene.skip_guard;
    if(guard.entry)return true;
    const auto fn=world_input(world_cine_skip),graph=world_input(world_cine_graph);
    if(!fn||!graph||AnimU::field<int>(fn,104)!=18)return false;
    const auto code=AnimU::field<unsigned char*>(fn,96);
    if(!writable_travel_code(code)||code[0]!=0x46||AnimU::field<void*>(code,1)!=graph||code[9]!=0x1d||
       AnimU::field<std::uint32_t>(code,10)!=576||code[14]!=0x16||code[15]!=4||code[16]!=11||code[17]!=0x53||
       !makeweak(guard.function,fn))return false;
    const auto entry=reinterpret_cast<volatile SHORT*>(code);
    guard.original=AnimU::field<SHORT>(code,0);
    if(InterlockedCompareExchange16(entry,travel_return,guard.original)!=guard.original)return false;
    guard.entry=entry;
    return true;
}
static bool cine_restore_skip(CinePlayback& scene){
    auto& guard=scene.skip_guard;
    if(!guard.entry)return true;
    if(const auto fn=api.weakget(&guard.function)){
        const auto code=AnimU::field<unsigned char*>(fn,96);
        if(AnimU::field<int>(fn,104)!=18||reinterpret_cast<volatile SHORT*>(code)!=guard.entry||!writable_travel_code(code))return false;
        const auto previous=InterlockedCompareExchange16(guard.entry,guard.original,travel_return);
        if(previous!=travel_return&&previous!=guard.original)return false;
    }
    guard={};
    return true;
}
static bool cine_call_skip(CinePlayback& scene,void* pawn,void* manager){
    const auto skip=world_input(world_cine_skip);
    if(!skip||!cine_restore_skip(scene))return false;
    api.event(manager,skip,nullptr);
    if(cine_current(pawn)==manager&&!cine_arm_skip(scene))++cine_errors;
    return true;
}

static void* cine_sequence(void* manager){
    const auto get=world_input(world_cine_sequence);
    if(!get||!manager)return nullptr;
    void* sequence{};
    api.event(manager,get,&sequence);
    return sequence;
}
static unsigned cine_position(void* sequence){
    const auto get=world_input(world_cine_time);
    if(!get||!sequence)return 0;
    struct QualifiedTime {int frame;float fraction;int numerator,denominator;};
    QualifiedTime result{};
    api.event(sequence,get,&result);
    if(result.numerator<=0||result.denominator<=0||!std::isfinite(result.fraction))return 0;
    const auto time=(static_cast<double>(result.frame)+result.fraction)*result.denominator*1000./result.numerator;
    return static_cast<unsigned>(std::clamp(time,0.,static_cast<double>(Cine::max_position_ms)));
}
static void cine_seek(void* sequence,unsigned position){
    const auto seek=world_input(world_cine_seek);
    if(!seek||!sequence||position<150)return;
    alignas(16) FactoryParameters args{};
    factory_put(args,8,position*.001f);
    args[32]=1; // seconds
    args[33]=1; // jump without evaluating past events
    api.event(sequence,seek,args.data());
}

static void* cine_stream(void* pawn,Cine::Manager id){
    const auto gs=world_input(world_cine_gameplay),get=world_input(world_cine_stream_get);
    if(!gs||!world_input(world_strings)||!world_input(world_string_name)||
       !medic_params(world_cine_stream_get,24,{{"WorldContextObject",8,0},{"PackageName",8,8},{"ReturnValue",8,16}}))return nullptr;
    AnimU::NameKey package{};
    if(!world_progress_name(cine_stream_paths[static_cast<unsigned>(id)-1],package))return nullptr;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,pawn);
    factory_put(args,8,package);
    api.event(gs,get,args.data());
    return AnimU::field<void*>(args.data(),16);
}
static bool cine_stream_flags(void* stream,bool& loaded,bool& visible){
    const auto type=AnimU::field<void*>(stream,16);
    const auto load=catalog_field(type,"bShouldBeLoaded",1),show=catalog_field(type,"bShouldBeVisible",1);
    if(!medic_field_class(load,"BoolProperty")||!medic_field_class(show,"BoolProperty"))return false;
    loaded=api.getbool(load,stream,0);
    visible=api.getbool(show,stream,0);
    return true;
}
static bool cine_stream_visible(CinePlayback& scene,void* pawn){
    auto stream=api.weakget(&scene.stream);
    if(!medic_params(world_cine_stream_load,1,{{"bInShouldBeLoaded",1,0}})||
       !medic_params(world_cine_stream_show,1,{{"bInShouldBeVisible",1,0}})||
       !medic_params(world_cine_stream_visible,1,{{"ReturnValue",1,0}}))return false;
    if(!stream){
        stream=cine_stream(pawn,scene.message.manager);
        if(!stream||!cine_stream_flags(stream,scene.loaded_before,scene.visible_before)||!makeweak(scene.stream,stream))return false;
    }
    bool loaded{},visible{};
    if(!cine_stream_flags(stream,loaded,visible))return false;
    std::uint8_t on=1,active{};
    if(!loaded)api.event(stream,world_input(world_cine_stream_load),&on);
    if(!visible)api.event(stream,world_input(world_cine_stream_show),&on);
    api.event(stream,world_input(world_cine_stream_visible),&active);
    return active!=0;
}
static void cine_release_stream(CinePlayback& scene){
    const auto stream=api.weakget(&scene.stream);
    scene.stream={};
    if(!stream)return;
    std::uint8_t off{};
    if(!scene.visible_before)
        if(const auto show=world_input(world_cine_stream_show))api.event(stream,show,&off);
    if(!scene.loaded_before)
        if(const auto load=world_input(world_cine_stream_load))api.event(stream,load,&off);
}
static void* cine_find_manager(void* pawn,Cine::Manager id){
    const auto type=world_input(world_cine_first+static_cast<unsigned>(id)-1);
    if(!type||!native_get_actors||!native_free)return nullptr;
    NativeActorArray actors{};
    native_get_actors(pawn,type,&actors);
    void* result{};
    if(actors.data&&actors.count==1&&actors.capacity>=1&&actors.data[0]&&!actor_is_destroying(actors.data[0]))
        result=actors.data[0];
    if(actors.data)native_free(actors.data);
    return result;
}

static void cine_hold(CinePlayback& scene,void* pawn){
    if(scene.held||cine_current(pawn)||cine_travelling(pawn)||!cine_bind_pawn(pawn))return;
    const auto lock=world_input(world_cine_lock),hide=world_input(world_cine_hide);
    if(!lock||!hide)return;
    scene.damage_before=api.getbool(cine_binding.damage,pawn,0);
    api.event(pawn,lock,nullptr);
    api.event(pawn,hide,nullptr);
    api.setbool(cine_binding.damage,pawn,false,0);
    scene.held=true;
}
static void cine_release(CinePlayback& scene){
    if(!scene.held)return;
    const auto pawn=api.weakget(&scene.pawn);
    scene.held=false;
    if(!pawn||cine_current(pawn)||cine_travelling(pawn)||!cine_bind_pawn(pawn))return;
    if(const auto unlock=world_input(world_cine_unlock))api.event(pawn,unlock,nullptr);
    api.setbool(cine_binding.damage,pawn,scene.damage_before,0);
    if(const auto unhide=world_input(world_cine_unhide))api.event(pawn,unhide,nullptr);
}

static bool cine_take_callbacks(CinePlayback& scene,void* manager,void* field){
    auto* list=reinterpret_cast<CineDelegateList*>(static_cast<unsigned char*>(manager)+*api.offset(field));
    if(list->count<0||static_cast<unsigned>(list->count)>scene.callbacks.size()||list->capacity<list->count||
       list->capacity>64||(list->count&&!list->data))return false;
    std::array<CineCallback,8> callbacks{};
    unsigned count{};
    for(int i=0;host_role&&i<list->count;++i){
        const auto& entry=list->data[i];
        const auto owner=api.weakget(&entry.owner);
        if(!owner)continue;
        std::string leaf;
        if(!AnimU::name_string(catalog_name_pool,entry.function,catalog_read,leaf))return false;
        if(!world_mechanism_method(AnimU::field<void*>(owner,16),leaf,callbacks[count].function)||
           !makeweak(callbacks[count].owner,owner))return false;
        ++count;
    }
    scene.callbacks=callbacks;
    scene.callback_count=count;
    list->count=0;
    return true;
}

static void world_scene_observe_finish(void* context,void* frame){
    auto& scene=cine_playback;
    if(!scene.started||scene.broadcast||!context||!frame||api.weakget(&scene.actor)!=context)return;
    const auto graph=world_input(world_cine_graph),signature=world_input(world_cine_signature),base=world_input(world_cine_base);
    if(!graph||!signature||!base||AnimU::field<void*>(frame,16)!=graph||AnimU::field<void*>(frame,24)!=context||
       AnimU::field<int>(graph,104)!=915)return;
    const auto code=AnimU::field<const unsigned char*>(graph,96);
    if(!code||AnimU::field<const unsigned char*>(frame,32)!=code+805||code[804]!=0x63||code[813]!=1||code[822]!=0x16||
       AnimU::field<void*>(code,805)!=signature||*api.parms(signature))return;
    const auto field=catalog_field(base,"CutsceneFinished",16);
    if(!medic_field_class(field,"MulticastInlineDelegateProperty")||*api.offset(field)!=560||
       AnimU::field<void*>(code,814)!=field)return;
    if(cine_take_callbacks(scene,context,field)){
        scene.broadcast=true;
        ++cine_broadcasts;
    }else ++cine_errors;
}

static void cine_commit(CinePlayback& scene){
    if(scene.committed)return;
    scene.committed=true;
    ++cine_commits;
    const auto callbacks=scene.callbacks;
    const auto count=scene.callback_count;
    scene.callback_count=0;
    for(unsigned i=0;i<count;++i){
        const auto owner=api.weakget(&callbacks[i].owner),function=api.weakget(&callbacks[i].function);
        if(!owner||!function)continue;
        ++cine_callbacks;
        api.event(owner,function,nullptr);
    }
}

static bool world_scene_cleanup(){
    auto& scene=cine_playback;
    if(!scene.message.serial)return true;
    const auto pawn=api.weakget(&scene.pawn),manager=api.weakget(&scene.actor);
    if(manager&&cine_current(pawn)==manager){
        if(host_role){
            const auto play=world_input(world_cine_play),sequence=api.weakget(&scene.sequence);
            if(play&&sequence&&scene.message.phase==Cine::Phase::prepare)api.event(sequence,play,nullptr);
        }else{
            if(!cine_call_skip(scene,pawn,manager))return false;
            if(cine_current(pawn)==manager)return false;
        }
    }
    if(!cine_restore_skip(scene))return false;
    if(host_role&&scene.broadcast)cine_commit(scene);
    cine_release(scene);
    cine_release_stream(scene);
    cine_completed=std::max(cine_completed,scene.message.serial);
    scene={};
    return true;
}

static bool cine_send(Cine::Phase phase,unsigned mask,unsigned position=0,std::uint64_t revision=0){
    const auto& scene=cine_playback;
    E::Packet packet{};
    return mask&&Cine::encode(envelope,{scene.message.serial,revision,scene.message.manager,phase,position},packet)&&send(packet,mask);
}

static bool receive_world_scene(const ReceivedPacket& received,unsigned kind,double time){
    if(kind!=Cine::kind)return false;
    Cine::Message message;
    if(!active_peer_mask||!Cine::decode(envelope,received.packet,message))return true;
    auto& scene=cine_playback;
    if(host_role){
        if(received.peer>=Player::slots-1||message.serial!=scene.message.serial||message.manager!=scene.message.manager)return true;
        const auto peer=(1u<<received.peer)&active_peer_mask;
        if(message.phase==Cine::Phase::ready)scene.ready|=peer;
        if(message.phase==Cine::Phase::finished&&(scene.ready&peer))scene.finished|=peer;
        if(message.phase==Cine::Phase::ack&&scene.message.phase==Cine::Phase::commit)scene.acked|=peer;
        return true;
    }
    if(received.peer||message.phase==Cine::Phase::ready||message.phase==Cine::Phase::finished||message.phase==Cine::Phase::ack||
       message.serial<cine_completed||message.serial<scene.message.serial)return true;
    if(message.serial!=scene.message.serial){
        if(message.serial==cine_completed||!world_scene_cleanup())return true;
        scene.message=message;
        scene.born=time;
    }else if(message.manager!=scene.message.manager||message.phase<scene.message.phase)return true;
    else if(scene.committed){
        if(message.phase==Cine::Phase::commit)cine_send(Cine::Phase::ack,1);
    }else scene.message=message;
    scene.last_message=time;
    return true;
}

static bool cine_attach(CinePlayback& scene,void* pawn,void* manager){
    const auto sequence=cine_sequence(manager);
    return sequence&&makeweak(scene.pawn,pawn)&&makeweak(scene.actor,manager)&&makeweak(scene.sequence,sequence);
}
static bool cine_begin_remote(CinePlayback& scene,void* pawn){
    if(cine_travelling(pawn)||cine_current(pawn)||!cine_stream_visible(scene,pawn))return false;
    if(!world_input(world_cine_first+static_cast<unsigned>(scene.message.manager)-1))scan_world_inputs(4096,true);
    const auto manager=cine_find_manager(pawn,scene.message.manager);
    if(!manager||!cine_attach(scene,pawn,manager)||!cine_arm_skip(scene))return false;
    alignas(16) FactoryParameters args{};
    scene.started=true;
    api.event(manager,world_input(world_cine_begin),args.data());
    if(cine_current(pawn)!=manager){scene.started=false;cine_restore_skip(scene);return false;}
    ++cine_starts;
    if(const auto sequence=api.weakget(&scene.sequence))api.event(sequence,world_input(world_cine_pause),nullptr);
    return true;
}
static void cine_detect_end(CinePlayback& scene,void* pawn,double time){
    if(!scene.started||scene.ended)return;
    const auto actor=api.weakget(&scene.actor);
    if(actor&&cine_current(pawn)==actor)return;
    scene.ended=true;
    scene.finished_at=time;
    scene.next_send=0;
    if(!cine_restore_skip(scene))++cine_errors;
    cine_hold(scene,pawn);
}

static void cine_step_host(void* pawn,double time){
    auto& scene=cine_playback;
    const auto current=cine_current(pawn);
    if(current&&!actor_is_destroying(current)&&current!=api.weakget(&scene.actor)&&(!scene.message.serial||scene.committed)){
        const auto id=cine_id(current);
        if(!static_cast<unsigned>(id)||!world_scene_cleanup()||!cine_attach(scene,pawn,current))return;
        if(!++cine_serial)++cine_serial;
        scene.message={cine_serial,0,id,Cine::Phase::prepare,0};
        scene.born=time;
        scene.started=true;
        api.event(api.weakget(&scene.sequence),world_input(world_cine_pause),nullptr);
        ++cine_starts;
    }
    if(!scene.message.serial)return;
    cine_detect_end(scene,pawn,time);
    scene.ready&=active_peer_mask;
    scene.finished&=active_peer_mask;
    scene.acked&=active_peer_mask;
    if(scene.ended&&!scene.committed&&((scene.finished&active_peer_mask)==active_peer_mask||time-scene.finished_at>=3.)){
        scene.commit_revision=world_progress.published.number+1;
        cine_commit(scene);
        scene.settle_at=time+.5;
    }
    if(scene.committed&&scene.message.phase!=Cine::Phase::commit&&time>=scene.settle_at&&!cine_travelling(pawn)){
        if(world_progress.published.number<scene.commit_revision)world_progress.force_revision=true;
        else{
            scene.message.phase=Cine::Phase::commit;
            scene.next_send=0;
            cine_release(scene);
        }
    }
    if(scene.held&&time-scene.finished_at>=15.){cine_release(scene);scene.recovered=true;}
    const auto sequence=api.weakget(&scene.sequence);
    if(!scene.ended&&sequence&&scene.message.phase==Cine::Phase::prepare&&
       ((scene.ready&active_peer_mask)==active_peer_mask||time-scene.born>=8.)){
        scene.message.phase=Cine::Phase::play;
        api.event(sequence,world_input(world_cine_play),nullptr);
        ++cine_plays;
        scene.next_send=0;
    }
    if(time<scene.next_send)return;
    scene.next_send=time+.25;
    if(scene.message.phase==Cine::Phase::commit)cine_send(Cine::Phase::commit,active_peer_mask&~scene.acked,0,scene.commit_revision);
    else if(scene.ended)cine_send(Cine::Phase::skip,active_peer_mask&~scene.finished);
    else if(scene.message.phase==Cine::Phase::play)cine_send(Cine::Phase::play,active_peer_mask,cine_position(sequence));
    else cine_send(Cine::Phase::prepare,active_peer_mask&~scene.ready);
}

static void cine_step_guest(void* pawn,double time){
    auto& scene=cine_playback;
    if(!scene.message.serial)return;
    if(!scene.committed&&time-scene.last_message>=12.){world_scene_cleanup();return;}
    const auto phase=scene.message.phase;
    if(phase<=Cine::Phase::play&&!scene.started&&time>=scene.next_bind){
        scene.next_bind=time+.2;
        if(!cine_begin_remote(scene,pawn))return;
    }
    cine_detect_end(scene,pawn,time);
    if(phase==Cine::Phase::play&&scene.started&&!scene.ended&&!(scene.ready&1)){
        if(const auto sequence=api.weakget(&scene.sequence)){
            cine_seek(sequence,scene.message.position_ms);
            api.event(sequence,world_input(world_cine_play),nullptr);
            scene.ready=1;
            ++cine_plays;
        }
    }
    if(phase>=Cine::Phase::skip&&scene.started&&!scene.ended&&time>=scene.next_send){
        if(const auto actor=api.weakget(&scene.actor);actor&&cine_current(pawn)==actor){
            if(cine_call_skip(scene,pawn,actor))++cine_skips;
        }
        cine_detect_end(scene,pawn,time);
    }
    if(phase==Cine::Phase::commit&&(!scene.started||scene.ended)&&world_progress.applied.number>=scene.message.revision&&
       world_progress_ready()&&!cine_travelling(pawn)){
        cine_completed=std::max(cine_completed,scene.message.serial);
        cine_release(scene);
        cine_release_stream(scene);
        if(!scene.committed){scene.committed=true;++cine_commits;cine_send(Cine::Phase::ack,1);}
    }
    if(scene.committed)return;
    if(scene.held&&scene.ended&&time-scene.finished_at>=15.){cine_release(scene);scene.recovered=true;}
    if(time<scene.next_send)return;
    scene.next_send=time+.25;
    if(scene.ended)cine_send(Cine::Phase::finished,1);
    else if(scene.started)cine_send(Cine::Phase::ready,1);
}

static bool world_scene_busy(){
    return cine_playback.message.serial&&!cine_playback.recovered&&
        (host_role?cine_playback.message.phase!=Cine::Phase::commit:!cine_playback.committed);
}

static void world_scene_step(void* pawn,double time){
    if(!active_peer_mask){world_scene_cleanup();return;}
    if(!cine_playback.message.serial&&(!host_role||!cine_current(pawn)))return;
    if(!cine_bind_methods()){
        if(time<cine_next_scan)return;
        cine_next_scan=time+.2;
        cine_methods_ready=false;
        scan_world_inputs(4096,true);
        if(!host_role&&cine_playback.message.phase<=Cine::Phase::play&&!cine_playback.started)
            cine_stream_visible(cine_playback,pawn);
        if(!cine_bind_methods())return;
    }
    if(host_role)cine_step_host(pawn,time);
    else cine_step_guest(pawn,time);
}

static void world_scene_reset(){
    world_scene_cleanup();
    if(!cine_restore_skip(cine_playback))++cine_errors;
    cine_release_stream(cine_playback);
    cine_playback={};
    cine_binding={};
    cine_serial=cine_completed=0;
    cine_next_scan=0;
    world_cine_cursor=0;
    cine_methods_ready=false;
}
