namespace DoorTrigger=qdcoop::door_trigger_intent;
struct NativeDoorTrigger {WP::Guid key;Weak actor;};
struct DoorTriggerRequest {DoorTrigger::Message message;double expires{},retry{};};
struct DoorTriggerPeer {std::uint64_t sequence{};std::uint32_t generation{};qdcoop::Id identity;};
struct DoorTriggerTargets {
    std::array<Weak,32> doors{};
    unsigned count{};
    bool pure{true};
};
struct NativeDoorTriggers {
    Weak type,door,fire,ubergraph,hook,activate,deactivate;
    MedicMap guids;
    const unsigned char* close_script{};
    void *fired{},*only_once{},*skip_save{};
    int targets{-1},lever_targets{-1};
    std::vector<NativeDoorTrigger> actors;
    std::vector<DoorTriggerRequest> pending;
    std::array<DoorTriggerPeer,Player::slots-1> peers{};
    std::uint64_t sequence{};
    double next_scan{};
};
static NativeDoorTriggers door_triggers;
static std::atomic<KillExec> door_trigger_stock{};
static constexpr LONG door_trigger_flags=0x0c000000;
static constexpr int door_trigger_entry=3904;

static std::optional<WP::Guid> native_door_source_guid(void* actor,int offset){
    const auto keys=AnimU::field<WorldArray>(actor,offset);
    if(!keys.valid(32)||!keys.count)return {};
    const auto key=*static_cast<const WP::Guid*>(keys.data);
    if(key==WP::Guid{}||WP::personal(key))return {};
    return key;
}
static std::optional<WP::Guid> native_door_trigger_key(void* actor,DoorTriggerTargets* links=nullptr){
    const auto& state=door_triggers;
    const auto type=api.weakget(&state.type),door=api.weakget(&state.door);
    if(!actor||!type||!door||state.targets<0||!api.isa(actor,type)||actor_is_destroying(actor))return {};
    const auto key=native_door_source_guid(actor,state.guids.offset);
    const auto targets=AnimU::field<WorldArray>(actor,state.targets);
    if(!key||!targets.valid(32)||!targets.count)return {};
    DoorTriggerTargets found;
    const auto interfaces=static_cast<void* const*>(targets.data);
    const auto secret=world_input(world_special_secret);
    for(int i=0;i<targets.count;++i){
        const auto target=interfaces[2*i];
        if(!target||actor_is_destroying(target)){
            found.pure=false;
            continue;
        }
        if(api.isa(target,door)){
            if(links&&!makeweak(found.doors[found.count],target))return {};
            ++found.count;
        }else if(!secret||!api.isa(target,secret))found.pure=false;
    }
    // Mixed transient triggers can also start story or travel. Their linked
    // stock doors can be replayed independently using the actual trigger source.
    if(!found.pure&&(!found.count||(api.getbool(state.only_once,actor,0)&&!api.getbool(state.skip_save,actor,0))))
        return {};
    if(links)*links=found;
    return key;
}
static std::optional<WP::Guid> native_door_close_source(void* actor,void* door){
    const auto& state=door_triggers;
    if(!actor||!door||actor_is_destroying(actor)||actor_is_destroying(door))return {};
    const auto trigger=api.weakget(&state.type);
    const auto& lever=world_mechanisms.kinds[1];
    const auto lever_type=api.weakget(&lever.type);
    int targets{},guids{};
    if(trigger&&api.isa(actor,trigger)){
        if(api.getbool(state.only_once,actor,0)&&!api.getbool(state.skip_save,actor,0))return {};
        targets=state.targets;
        guids=state.guids.offset;
    }else if(lever_type&&state.lever_targets>=0&&api.isa(actor,lever_type)&&
             api.getbool(lever.disable_saving,actor,0)){
        targets=state.lever_targets;
        guids=lever.guids.offset;
    }else return {};
    const auto list=AnimU::field<WorldArray>(actor,targets);
    if(!list.valid(32))return {};
    const auto interfaces=static_cast<void* const*>(list.data);
    for(int i=0;i<list.count;++i)if(interfaces[2*i]==door)return native_door_source_guid(actor,guids);
    return {};
}
static void native_door_queue(const WP::Guid& source,const WP::Guid& close_door={}){
    auto& state=door_triggers;
    if(state.sequence==UINT64_MAX||state.pending.size()>=16)return;
    state.pending.push_back({{source,++state.sequence,local_life_pose.generation,false,close_door},now()+5.,0.});
    ++door_triggers_observed;
}
static bool native_door_observing(){
    return !host_role&&active_peer_mask&&!world_guest_waiting()&&!rollback_busy()&&
        local_life_pose.generation&&local_life.life.phase==LifePhase::standing;
}
static bool native_door_close_observer_needed(){
    return door_triggers.close_script&&active_peer_mask&&!host_role;
}
static void observe_native_door_close(void* context,void* frame){
    const auto& state=door_triggers;
    const auto code=state.close_script;
    if(!code||AnimU::field<const unsigned char*>(frame,32)!=code+37||!native_door_observing()||
       AnimU::field<void*>(frame,16)!=api.weakget(&state.deactivate)||AnimU::field<void*>(frame,24)!=context)return;
    const auto locals=AnimU::field<void*>(frame,40);
    const auto& doors=world_mechanisms.kinds[0];
    const auto type=api.weakget(&doors.type);
    if(!locals||AnimU::field<std::uint8_t>(locals,0)||!type||!api.isa(context,type))return;
    const auto source=native_door_close_source(AnimU::field<void*>(locals,8),context);
    const auto door=native_door_source_guid(context,doors.guids.offset);
    if(source&&door)native_door_queue(*source,*door);
}
static void observe_native_door_trigger(void* actor,void* frame,void*){
    auto& state=door_triggers;
    if(!actor||!frame)return;
    auto& code=*reinterpret_cast<const unsigned char**>(static_cast<unsigned char*>(frame)+32);
    if(code&&*code==0x16)++code;
    const auto ubergraph=api.weakget(&state.ubergraph);
    if(!ubergraph)return;
    const bool observing=GetCurrentThreadId()==game_thread.load()&&native_door_observing();
    const auto key=observing?native_door_trigger_key(actor):std::optional<WP::Guid>{};
    const bool eligible=key&&(!api.getbool(state.only_once,actor,0)||!api.getbool(state.fired,actor,0));
    int entry=door_trigger_entry;
    api.event(actor,ubergraph,&entry);
    if(eligible)native_door_queue(*key);
}
static void* native_door_link_method(void* door,std::string_view name,std::string_view by_load){
    const auto method=declared_script_method(door,name,16,54);
    const auto load=method?catalog_field(method,by_load,1):nullptr;
    const auto source=method?catalog_field(method,"ActorThatTriggered",8):nullptr;
    return medic_field_class(load,"BoolProperty")&&*api.offset(load)==0&&
           medic_field_class(source,"ObjectProperty")&&*api.offset(source)==8?method:nullptr;
}
static int native_door_link_offset(void* type,std::string_view name){
    const auto field=type?catalog_field(type,name,16):nullptr;
    if(!medic_field_class(field,"ArrayProperty"))return -1;
    const auto inner=AnimU::field<void*>(field,0x78);
    return medic_field_class(inner,"InterfaceProperty")&&*api.size(inner)==16?*api.offset(field):-1;
}
static bool native_door_triggers_bind(void* type,void* door,void* fire){
    auto& state=door_triggers;
    if(type&&door&&api.weakget(&state.type)==type&&api.weakget(&state.door)==door&&
       api.weakget(&state.ubergraph)&& (host_role||api.weakget(&state.hook)==fire))return true;
    if(!type||!door||!fire||*api.parms(fire)||AnimU::field<int>(fire,104)!=18||
       AnimU::field<void*>(fire,32)!=type||AnimU::field<LONG>(fire,176)!=door_trigger_flags)return false;
    const auto code=AnimU::field<const unsigned char*>(fire,96);
    if(!code||code[0]!=0x46||code[9]!=0x1d||AnimU::field<int>(code,10)!=door_trigger_entry||
       code[14]!=0x16||code[15]!=0x04||code[16]!=0x0b||code[17]!=0x53)return false;
    const auto ubergraph=AnimU::field<void*>(code,1);
    std::string path;
    if(!ubergraph||AnimU::field<void*>(ubergraph,32)!=type||*api.parms(ubergraph)!=4||
       !catalog_path(ubergraph,path)||catalog_leaf(path)!="ExecuteUbergraph_ActivationTrigger")return false;
    const auto guids=world_actor_array(type,"GUIDs",16,"/Script/CoreUObject.Guid");
    const int targets=native_door_link_offset(type,"ActivationTargets");
    const auto only_once=catalog_field(type,"OnlyOnce",1),fired=catalog_field(type,"HasAlreadybeenTriggered",1);
    const auto skip_save=catalog_field(type,"DisableSaveOnlyOnce",1);
    if(!guids.field||targets<0||
       !medic_field_class(only_once,"BoolProperty")||!medic_field_class(fired,"BoolProperty")||
       !medic_field_class(skip_save,"BoolProperty"))return false;
    if(!makeweak(state.type,type)||!makeweak(state.door,door)||!makeweak(state.fire,fire)||
       !makeweak(state.ubergraph,ubergraph))return false;
    state.guids=guids;
    state.targets=targets;
    state.only_once=only_once;
    state.fired=fired;
    state.skip_save=skip_save;
    if(const auto deactivate=native_door_link_method(door,"Deactivate","DeactivateByLoad"))
        makeweak(state.deactivate,deactivate);
    if(host_role){
        if(const auto activate=native_door_link_method(door,"Activate","ActivateByLoad"))makeweak(state.activate,activate);
        return true;
    }
    constexpr unsigned char process_script[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,
        0x74,0x24,0x18,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x30};
    if(!native_install_exec(fire,0x11a7e40,process_script,sizeof(process_script),state.hook,door_trigger_stock,
                            &observe_native_door_trigger))return false;
    auto flags=reinterpret_cast<volatile LONG*>(static_cast<unsigned char*>(fire)+176);
    if(InterlockedCompareExchange(flags,door_trigger_flags|0x400,door_trigger_flags)==door_trigger_flags)return true;
    native_restore_exec(state.hook,door_trigger_stock,&observe_native_door_trigger);
    return false;
}
static bool native_door_triggers_cleanup(){
    auto& state=door_triggers;
    if(const auto fire=api.weakget(&state.hook)){
        const auto flags=reinterpret_cast<volatile LONG*>(static_cast<unsigned char*>(fire)+176);
        const auto previous=InterlockedCompareExchange(flags,door_trigger_flags,door_trigger_flags|0x400);
        if(previous!=(door_trigger_flags|0x400)&&previous!=door_trigger_flags)return false;
    }
    if(!native_restore_exec(state.hook,door_trigger_stock,&observe_native_door_trigger))return false;
    state={};
    return true;
}

static bool door_trigger_send(const DoorTrigger::Message& message,unsigned mask){
    E::Packet packet{};
    return DoorTrigger::encode(envelope,message,packet)&&send(packet,mask);
}
struct DoorLinkArguments {std::uint64_t by_load{};void* source;};
static void* door_mechanism_actor(const WorldMechanismBinding& binding,const WP::Guid& key){
    const auto found=std::ranges::lower_bound(binding.actors,key,{},&WorldMechanismActor::key);
    return found!=binding.actors.end()&&found->key==key&&!found->shrine?api.weakget(&found->actor):nullptr;
}
static bool native_door_replay(const DoorTrigger::Message& message,const LifePeer& peer){
    const auto& state=door_triggers;
    const auto it=std::ranges::lower_bound(state.actors,message.trigger,{},&NativeDoorTrigger::key);
    auto actor=it!=state.actors.end()&&it->key==message.trigger?api.weakget(&it->actor):nullptr;
    const bool closing=message.close_door!=WP::Guid{};
    if(closing){
        const auto lever=door_mechanism_actor(world_mechanisms.kinds[1],message.trigger);
        if(lever&&actor&&lever!=actor)return false;
        if(lever)actor=lever;
    }
    const auto world=api.weakget(&attached_world);
    if(!actor||!world||native_actor_world(actor)!=world||
       !(native_enemy_distance(native_actor_location(actor),peer.position)<=1200.*1200.))return false;
    door_trigger_stage=3;
    if(closing){
        const auto door=door_mechanism_actor(world_mechanisms.kinds[0],message.close_door);
        const auto deactivate=api.weakget(&state.deactivate);
        if(!door||native_actor_world(door)!=world||!deactivate||native_door_close_source(actor,door)!=message.trigger)return false;
        DoorLinkArguments args{0,actor};
        api.event(door,deactivate,&args);
    }else{
        const auto fire=api.weakget(&state.fire);
        DoorTriggerTargets links;
        if(!fire||native_door_trigger_key(actor,&links)!=message.trigger)return false;
        if(links.pure)api.event(actor,fire,nullptr);
        else {
            const auto activate=api.weakget(&state.activate);
            if(!activate)return false;
            for(unsigned i=0;i<links.count;++i)
                if(const auto door=api.weakget(&links.doors[i]);door&&!actor_is_destroying(door)){
                    DoorLinkArguments args{0,actor};
                    api.event(door,activate,&args);
                }
        }
    }
    door_trigger_stage=4;
    return true;
}
static bool receive_door_trigger(const ReceivedPacket& received,unsigned kind,double time){
    if(kind!=DoorTrigger::kind)return false;
    ++door_triggers_received;
    auto& state=door_triggers;
    DoorTrigger::Message message;
    if(!active_peer_mask||!DoorTrigger::decode(envelope,received.packet,message))return true;
    if(!host_role){
        if(!received.peer&&message.ack&&!state.pending.empty()){
            const auto& sent=state.pending.front().message;
            if(sent.generation==message.generation&&sent.sequence==message.sequence&&sent.trigger==message.trigger&&
               sent.close_door==message.close_door)
                state.pending.erase(state.pending.begin());
        }
        return true;
    }
    if(message.ack||received.peer>=state.peers.size()||!(active_peer_mask&(1u<<received.peer))||
       !world_progress_ready()||!checkpoint_medic_idle()||rollback_busy())return true;
    door_trigger_stage=1;
    const auto& peer=life_peers[received.peer+1];
    if(message.generation!=peer.generation||!fresh_life(peer,LifePhase::standing,time))return true;
    door_trigger_stage=2;
    auto& seen=state.peers[received.peer];
    if(seen.identity!=peer.rescue_identity||seen.generation!=peer.generation)seen={0,peer.generation,peer.rescue_identity};
    if(message.sequence>seen.sequence){
        if(!native_door_replay(message,peer))return true;
        seen.sequence=message.sequence;
        world_progress.next_capture=0.;
        ++door_triggers_replayed;
    }
    message.ack=true;
    door_trigger_send(message,1u<<received.peer);
    return true;
}
static void native_door_bind_close_observer(){
    auto& state=door_triggers;
    const auto method=api.weakget(&state.deactivate);
    const auto script=method?AnimU::field<const unsigned char*>(method,96):nullptr;
    if(!script){state.close_script=nullptr;return;}
    if(state.close_script==script&&boss_interact_cell)return;
    state.close_script=nullptr;
    if(script[0]!=0x64||script[9]!=0||script[18]!=0x64||script[27]!=0||
       script[36]!=0x46||script[45]!=0x1d||AnimU::field<int>(script,46)!=1950||
       script[50]!=0x16||script[51]!=0x04||script[52]!=0x0b||script[53]!=0x53)return;
    const auto type=api.weakget(&state.door);
    const auto graph=AnimU::field<void*>(script,37);
    if(!type||!graph||AnimU::field<void*>(graph,32)!=type||*api.parms(graph)!=4||
       AnimU::field<void*>(script,1)!=catalog_field(graph,"K2Node_Event_DeactivateByLoad",1)||
       AnimU::field<void*>(script,19)!=catalog_field(graph,"K2Node_Event_ActorThatTriggered",8)||
       AnimU::field<void*>(script,10)!=catalog_field(method,"DeactivateByLoad",1)||
       AnimU::field<void*>(script,28)!=catalog_field(method,"ActorThatTriggered",8))return;
    if(boss_interact_install())state.close_script=script;
}
static void native_door_triggers_step(void* pawn,double time){
    auto& state=door_triggers;
    if(!active_peer_mask){
        if(state.guids.field)native_door_triggers_cleanup();
        return;
    }
    if(time>=state.next_scan&&native_get_actors&&native_free){
        state.next_scan=time+1.;
        if(!world_input(world_mechanism_trigger)||!world_input(world_mechanism_door)||
           !world_input(world_mechanism_trigger_fire))scan_world_inputs(4096);
        const auto type=world_input(world_mechanism_trigger),door=world_input(world_mechanism_door);
        auto fire=world_input(world_mechanism_trigger_fire);
        if(type&&!fire)fire=declared_script_method(type,"DoTrigger",0,18);
        door_trigger_binding=static_cast<unsigned>(type!=nullptr)|(static_cast<unsigned>(door!=nullptr)<<1)|
            (static_cast<unsigned>(fire!=nullptr)<<2);
        if(!native_door_triggers_bind(type,door,fire))return;
        door_trigger_binding|=8;
        if(state.lever_targets<0)
            state.lever_targets=native_door_link_offset(api.weakget(&world_mechanisms.kinds[1].type),"ActivatableTargets");
        if(!host_role)native_door_bind_close_observer();
        if(state.close_script)door_trigger_binding|=16;
        if(host_role){
            NativeActorArray entries{};
            native_get_actors(pawn,type,&entries);
            door_trigger_actor_count=static_cast<unsigned>(std::max(entries.count,0));
            state.actors.clear();
            if(entries.data&&entries.count>=0&&entries.count<=8192&&entries.capacity>=entries.count)
                for(int i=0;i<entries.count;++i){
                    const auto actor=entries.data[i];
                    const auto key=native_door_trigger_key(actor);
                    NativeDoorTrigger entry{};
                    if(key&&makeweak(entry.actor,actor)){entry.key=*key;state.actors.push_back(entry);}
                }
            if(entries.data)native_free(entries.data);
            std::ranges::sort(state.actors,{},&NativeDoorTrigger::key);
            for(auto it=state.actors.begin();it!=state.actors.end();){
                const auto end=std::upper_bound(it,state.actors.end(),it->key,
                    [](const auto& key,const auto& entry){return key<entry.key;});
                it=end==it+1?end:state.actors.erase(it,end);
            }
            door_trigger_index_size=static_cast<unsigned>(state.actors.size());
        }
    }
    if(host_role||state.pending.empty())return;
    std::erase_if(state.pending,[&](const auto& request){
        return request.expires<=time||request.message.generation!=local_life_pose.generation;
    });
    if(!state.pending.empty()){
        auto& front=state.pending.front();
        if(time>=front.retry&&door_trigger_send(front.message,1)){
            front.retry=time+.15;
            ++door_triggers_sent;
        }
    }
}
