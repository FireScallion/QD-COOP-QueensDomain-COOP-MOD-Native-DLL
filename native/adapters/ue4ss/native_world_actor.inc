namespace WorldIntent=qdcoop::world_interaction_intent;
static bool world_mechanism_method(void* type,std::string_view name,Weak& out);
static void world_mechanisms_observe(void* save,double time);
static bool world_mechanism_accept_guest(void* save,const WorldIntent::Message&,const LifePeer&);
static bool world_special_accept_guest(const WorldIntent::Message&,const LifePeer&);
static void world_special_observe(void* save,double time);
static bool world_host_lever_on(const WP::Guid& key);
using WorldArrayClear=void(*)(void*,const void*);
struct WorldArray {
    const void* data{};
    std::int32_t count{},capacity{};
    bool valid(unsigned max) const {
        return count>=0&&static_cast<unsigned>(count)<=max&&capacity>=count&&capacity<=65536&&(!count||data);
    }
};
struct WorldChestBinding {
    Weak type;
    MedicMap guids,cache;
    WorldArrayClear clear{};
    bool sound{};
};
struct WorldBreakable {WP::Guid key;Weak actor;bool barricade{};};
struct WorldInteractionPending {WP::Guid key;WorldIntent::Action action;double expires{},next_send{};};
struct WorldActorRuntime {
    WorldChestBinding chest;
    std::vector<WP::Guid> changed;
    std::vector<Weak> queue;
    double next_scan{},expires{};
    std::vector<Weak> breakable_queue,breakables_restored;
    std::vector<WorldBreakable> breakables;
    std::vector<WorldInteractionPending> pending;
    std::vector<WP::Guid> live_breaks;
    WP::Revision breakable_revision;
    double next_breakable_scan{},next_breakable_index{},next_observe{};
};
static WorldActorRuntime world_actor;

static void world_actor_reset(){world_actor={};}
static void world_actor_queue(const WP::Guid& guid){
    auto& state=world_actor;
    if(state.changed.size()>=2*WP::max_entries)return;
    state.changed.push_back(guid);
    state.expires=now()+5.;
}
static MedicMap world_actor_array(void* type,std::string_view name,unsigned size,std::string_view struct_path){
    const auto field=catalog_field(type,name,16);
    if(!medic_field_class(field,"ArrayProperty"))return {};
    const auto inner=AnimU::field<void*>(field,0x78);
    std::string path;
    if(!medic_field_class(inner,"StructProperty")||*api.size(inner)!=static_cast<int>(size)||
       !catalog_path(AnimU::field<void*>(inner,0x78),path)||path!=struct_path)return {};
    return {field,*api.offset(field)};
}
static bool world_actor_can_sound(){
    return world_input(world_actor_sound_type)&&medic_params(world_actor_play_sound,80,{
        {"WorldContextObject",8,0},{"Sound",8,8},{"Location",12,16},{"Rotation",12,28},
        {"VolumeMultiplier",4,40},{"PitchMultiplier",4,44},{"StartTime",4,48},
        {"AttenuationSettings",8,56},{"ConcurrencySettings",8,64},{"OwningActor",8,72}});
}
static WorldArrayClear world_actor_array_clear(){
    const auto clear=world_input(world_actor_clear);
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto generic_clear=image+0x297d910;
    constexpr unsigned char dispatch[]{0xe9,0xa8,0x06,0xba,0xff};
    constexpr unsigned char prefix[]{0x48,0x85,0xc9,0x0f,0x84,0xee,0x00,0x00,0x00,0x57,0x48,0x83,0xec,0x40};
    if(!clear||AnimU::field<std::uintptr_t>(clear,216)!=image+0x2ddd1e0||
       std::memcmp(reinterpret_cast<const void*>(image+0x2ddd263),dispatch,sizeof(dispatch))||
       std::memcmp(reinterpret_cast<const void*>(generic_clear),prefix,sizeof(prefix)))return nullptr;
    return reinterpret_cast<WorldArrayClear>(generic_clear);
}
static bool world_actor_bind(){
    auto& binding=world_actor.chest;
    const auto type=world_input(world_actor_chest);
    if(type&&api.weakget(&binding.type)==type&&binding.clear)return true;
    for(unsigned i=world_actor_first;i<world_mechanism_first;++i)if(!world_input(i)){
        scan_world_inputs(2048);
        break;
    }
    const auto restore=world_input(world_actor_restore);
    const auto clear=world_actor_array_clear();
    if(!type||!restore||!clear||*api.parms(restore)||AnimU::field<int>(restore,104)!=40||
       !medic_params(world_actor_collision,1,{{"ReturnValue",1,0}})||
       !medic_params(world_actor_play,0,{})||!medic_params(world_actor_playing,1,{{"ReturnValue",1,0}}))return false;
    WorldChestBinding candidate{};
    if(!makeweak(candidate.type,type))return false;
    candidate.guids=world_actor_array(type,"GUIDs",16,"/Script/CoreUObject.Guid");
    candidate.cache=world_actor_array(type,"MySaveData",8,"/Game/Blueprints/SaveSystem/SavableActorState.SavableActorState");
    if(!candidate.guids.field||!candidate.cache.field)return false;
    candidate.clear=clear;
    candidate.sound=world_actor_can_sound();
    binding=candidate;
    return true;
}
static void world_actor_scan(void* pawn){
    auto& state=world_actor;
    const auto type=api.weakget(&state.chest.type);
    if(!type||!native_get_actors||!native_free)return;
    NativeActorArray actors{};
    native_get_actors(pawn,type,&actors);
    std::ranges::sort(state.changed);
    if(actors.data&&actors.count>=0&&actors.count<=8192&&actors.capacity>=actors.count){
        for(int i=0;i<actors.count;++i){
            const auto actor=actors.data[i];
            if(!actor||AnimU::field<void*>(actor,16)!=type)continue;
            const auto keys=AnimU::field<WorldArray>(actor,state.chest.guids.offset);
            if(!keys.valid(32)||!keys.count)continue;
            const auto first=static_cast<const WP::Guid*>(keys.data);
            if(!std::any_of(first,first+keys.count,[&](const auto& key){
                return std::ranges::binary_search(state.changed,key);
            }))continue;
            if(std::ranges::any_of(state.queue,[&](const auto& key){return api.weakget(&key)==actor;}))continue;
            Weak key{};
            if(makeweak(key,actor))state.queue.push_back(key);
        }
    }
    if(actors.data)native_free(actors.data);
    state.changed.clear();
}
static void world_actor_sound(void* actor,std::string_view sound_field){
    const auto sound=catalog_object_field(actor,sound_field);
    const auto type=world_input(world_actor_sound_type),fn=world_input(world_actor_play_sound);
    const auto gs=world_input(static_cast<unsigned>(world_object_count)+factory_gameplay);
    if(!sound||!type||!fn||!gs||!api.isa(sound,type))return;
    alignas(16) FactoryParameters args{},location{};
    api.event(actor,getters[0].fn,location.data());
    factory_put(args,0,actor);
    factory_put(args,8,sound);
    factory_put(args,16,AnimU::field<std::array<float,3>>(location.data(),getters[0].at));
    factory_put(args,40,1.f);
    factory_put(args,44,1.f);
    api.event(gs,fn,args.data());
    ++world_actor_sounds;
}
static void world_actor_restore_chest(void* actor){
    const auto& binding=world_actor.chest;
    const auto restore=world_input(world_actor_restore),collision=world_input(world_actor_collision);
    const auto play=world_input(world_actor_play),playing=world_input(world_actor_playing);
    if(!restore||!collision||!play||!playing||!binding.clear||AnimU::field<void*>(actor,16)!=api.weakget(&binding.type))return;
    const auto box=catalog_object_field(actor,"InteractBox"),lid=catalog_object_field(actor,"OpenLid");
    if(!box||!lid||!api.isa(box,AnimU::field<void*>(collision,32))||!api.isa(lid,AnimU::field<void*>(play,32)))return;
    const auto cache=AnimU::field<WorldArray>(actor,binding.cache.offset);
    if(!cache.valid(64))return;
    unsigned char before{},after{},running{};
    api.event(box,collision,&before);
    api.event(lid,playing,&running);
    binding.clear(binding.cache.address(actor),binding.cache.field);
    api.event(actor,restore,nullptr);
    ++world_actor_restores;
    api.event(box,collision,&after);
    if(!before||after||running)return;
    api.event(lid,play,nullptr);
    ++world_actor_chests;
    if(binding.sound)world_actor_sound(actor,"OpenSound");
}
static bool world_actor_saved_positive(const std::vector<WP::ActorState>& states,const WP::Guid& key){
    const auto it=std::ranges::lower_bound(states,key,{},&WP::ActorState::key);
    return it!=states.end()&&it->key==key&&it->value>0;
}
static bool world_breakable_animate(void* actor,bool barricade){
    const auto type=AnimU::field<void*>(actor,16);
    const auto event=world_input(barricade?world_breakable_hit:world_breakable_break);
    if(!event||*api.parms(event)||!api.isa(actor,AnimU::field<void*>(event,32)))return false;
    if(barricade){
        const auto hp=catalog_field(type,"HP",4);
        if(!medic_field_class(hp,"FloatProperty"))return false;
        const float zero{};
        std::memcpy(static_cast<unsigned char*>(actor)+*api.offset(hp),&zero,sizeof(zero));
    }
    api.event(actor,event,nullptr);
    return true;
}
static bool world_breakable_positive(void* save,const WP::Guid& key){
    const auto& binding=medic_runtime.save;
    const auto& map=world_progress.actors;
    std::int32_t value{};
    return binding.find(map.address(save),map.field,key.data(),&value)&&value>0;
}
static bool world_interaction_enqueue(const WP::Guid& key,WorldIntent::Action action,double time){
    auto& pending=world_actor.pending;
    if(pending.size()>=64||std::ranges::any_of(pending,[&](const auto& request){return request.key==key;}))return false;
    pending.push_back({key,action,time+4.,0.});
    ++world_interactions_queued;
    return true;
}
static bool world_interaction_queue(void* save,const WP::Guid& key,WorldIntent::Action action,double time){
    return !world_actor_saved_positive(world_progress.authoritative.actors,key)&&world_breakable_positive(save,key)&&
           world_interaction_enqueue(key,action,time);
}
static void world_breakables_index(void* pawn,double time){
    auto& state=world_actor;
    if(time<state.next_breakable_index||!native_get_actors||!native_free)return;
    state.next_breakable_index=time+1.;
    const auto restore=world_input(world_actor_restore);
    for(const auto input:{world_actor_restore,world_breakable_barricade,world_breakable_permanent,
                         world_breakable_hit,world_breakable_break})
        if(!world_input(input)){scan_world_inputs(2048);break;}
    if(!restore)return;
    const auto base=AnimU::field<void*>(restore,32);
    const auto guids=world_actor_array(base,"GUIDs",16,"/Script/CoreUObject.Guid");
    if(!guids.field)return;
    state.breakables.clear();
    for(const auto kind:{world_breakable_barricade,world_breakable_permanent}){
        const auto type=world_input(kind);
        if(!type||!catalog_subclass(type,base))continue;
        NativeActorArray entries{};
        native_get_actors(pawn,type,&entries);
        if(entries.data&&entries.count>=0&&entries.count<=8192&&entries.capacity>=entries.count)
            for(int i=0;i<entries.count;++i){
                const auto actor=entries.data[i];
                if(!actor||!api.isa(actor,type)||actor_is_destroying(actor))continue;
                const auto keys=AnimU::field<WorldArray>(actor,guids.offset);
                if(!keys.valid(32)||!keys.count)continue;
                const auto key=*static_cast<const WP::Guid*>(keys.data);
                if(key==WP::Guid{}||WP::personal(key))continue;
                WorldBreakable entry{key,{},kind==world_breakable_barricade};
                if(makeweak(entry.actor,actor))state.breakables.push_back(entry);
            }
        if(entries.data)native_free(entries.data);
    }
    std::ranges::sort(state.breakables,{},&WorldBreakable::key);
    for(auto it=state.breakables.begin();it!=state.breakables.end();){
        const auto end=std::upper_bound(it,state.breakables.end(),it->key,
            [](const WP::Guid& key,const WorldBreakable& entry){return key<entry.key;});
        it=end==it+1?end:state.breakables.erase(it,end);
    }
    world_breakable_index_size=static_cast<unsigned>(state.breakables.size());
}
static void world_breakables_observe(void* pawn,void* save,double time){
    auto& state=world_actor;
    if(!host_role&&world_progress.applied.number&&!world_progress.fault&&!world_progress.initial_reload&&local_life_pose.generation&&
       local_life.life.phase==LifePhase::standing){
        const auto& host=world_progress.authoritative.actors;
        if(time>=state.next_observe||time>=world_progress.next_apply||
           (world_progress.inbox.revision().number>world_progress.applied.number&&world_progress.inbox.complete())){
            state.next_observe=time+.12;
            for(const auto& entry:state.breakables)
                if(world_interaction_queue(save,entry.key,WorldIntent::Action::breakable,time))
                    state.breakables_restored.push_back(entry.actor);
            world_mechanisms_observe(save,time);
            world_special_observe(save,time);
        }
        std::erase_if(state.pending,[&](const auto& pending){
            return time>=pending.expires||(pending.action==WorldIntent::Action::temporary_lever?
                world_host_lever_on(pending.key):world_actor_saved_positive(host,pending.key));
        });
        for(auto& pending:state.pending){
            if(time<pending.next_send)continue;
            E::Packet packet{};
            if(WorldIntent::encode(envelope,{pending.key,local_life_pose.generation,pending.action},packet)&&send(packet,1)){
                pending.next_send=time+.25;
                ++world_interactions_sent;
            }
        }
    }
    world_breakables_index(pawn,time);
}
static bool receive_world_interaction(const ReceivedPacket& received,unsigned kind,double time){
    if(kind!=WorldIntent::kind)return false;
    ++world_interactions_received;
    world_interaction_stage=1;
    if(!host_role||received.peer>=Player::slots-1||!(active_peer_mask&(1u<<received.peer))||
       !world_progress_ready()||!checkpoint_medic_idle())return true;
    WorldIntent::Message message;
    if(!WorldIntent::decode(envelope,received.packet,message))return true;
    world_interaction_stage=2;
    const auto& peer=life_peers[received.peer+1];
    if(peer.generation!=message.generation||!fresh_life(peer,LifePhase::standing,time))return true;
    world_interaction_stage=3;
    const auto save=api.weakget(&world_progress.save);
    if(!save||(message.action!=WorldIntent::Action::temporary_lever&&world_breakable_positive(save,message.key)))return true;
    if(message.action!=WorldIntent::Action::breakable){
        const bool accepted=message.action>=WorldIntent::Action::secret?world_special_accept_guest(message,peer):
            world_mechanism_accept_guest(save,message,peer);
        if(accepted){
            world_progress.next_capture=0.;
            ++world_interactions_applied;
        }
        return true;
    }
    const auto& actors=world_actor.breakables;
    const auto entry=std::ranges::lower_bound(actors,message.key,{},&WorldBreakable::key);
    if(entry==actors.end()||entry->key!=message.key)return true;
    world_interaction_stage=4;
    const auto actor=api.weakget(&entry->actor);
    const auto range=entry->barricade?12000.:4500.;
    if(actor&&!actor_is_destroying(actor)&&native_enemy_distance(native_actor_location(actor),peer.position)<=range*range){
        world_interaction_stage=5;
        if(world_breakable_animate(actor,entry->barricade)){
            world_progress.next_capture=0.;
            ++world_interactions_applied;
            world_interaction_stage=6;
        }
    }
    return true;
}
static void world_breakables_step(void* pawn,double time){
    auto& state=world_actor;
    const bool scan=state.breakable_revision!=world_progress.applied||time>=state.next_breakable_scan;
    if(!scan&&state.breakable_queue.empty())return;
    const auto restore=world_input(world_actor_restore);
    if(!restore||*api.parms(restore)||AnimU::field<int>(restore,104)!=40||!native_get_actors||!native_free)return;
    const auto base=AnimU::field<void*>(restore,32);
    if(!base)return;
    const auto guids=world_actor_array(base,"GUIDs",16,"/Script/CoreUObject.Guid");
    const auto cache=world_actor_array(base,"MySaveData",8,"/Game/Blueprints/SaveSystem/SavableActorState.SavableActorState");
    const auto clear=world_actor_array_clear();
    if(!guids.field||!cache.field||!clear)return;
    const auto& authoritative=world_progress.authoritative.actors;
    if(scan){
        if(state.breakable_revision.number&&state.breakable_revision!=world_progress.applied){
            state.live_breaks.clear();
            for(const auto& entry:state.breakables)
                if(world_actor_saved_positive(authoritative,entry.key)&&
                   std::ranges::none_of(state.breakables_restored,[&](const Weak& key){
                       const auto actor=api.weakget(&key);
                       return actor&&actor==api.weakget(&entry.actor);
                   }))state.live_breaks.push_back(entry.key);
        }
        state.breakable_revision=world_progress.applied;
        state.next_breakable_scan=time+2.;
        state.breakable_queue.clear();
        std::erase_if(state.breakables_restored,[](const Weak& key){return !api.weakget(&key);});
        if(!world_input(world_breakable_barricade)||!world_input(world_breakable_permanent))scan_world_inputs(2048);
        for(const auto kind:{world_breakable_barricade,world_breakable_permanent}){
            const auto type=world_input(kind);
            if(!type||!catalog_subclass(type,base))continue;
            NativeActorArray actors{};
            native_get_actors(pawn,type,&actors);
            if(actors.data&&actors.count>=0&&actors.count<=8192&&actors.capacity>=actors.count)
                for(int i=0;i<actors.count;++i){
                    const auto actor=actors.data[i];
                    if(!actor||!api.isa(actor,type))continue;
                    const auto keys=AnimU::field<WorldArray>(actor,guids.offset);
                    if(!keys.valid(32)||!keys.count||
                       !world_actor_saved_positive(authoritative,*static_cast<const WP::Guid*>(keys.data)))continue;
                    if(std::ranges::any_of(state.breakables_restored,[actor](const Weak& key){return api.weakget(&key)==actor;}))
                        continue;
                    Weak key{};
                    if(makeweak(key,actor))state.breakable_queue.push_back(key);
                }
            if(actors.data)native_free(actors.data);
        }
    }
    for(unsigned i=0;i<4&&!state.breakable_queue.empty();++i){
        const auto key=state.breakable_queue.back();
        state.breakable_queue.pop_back();
        const auto actor=api.weakget(&key);
        if(!actor||!AnimU::field<WorldArray>(actor,cache.offset).valid(64))continue;
        state.breakables_restored.push_back(key);
        const auto keys=AnimU::field<WorldArray>(actor,guids.offset);
        const auto live=keys.valid(32)&&keys.count?std::ranges::find(state.live_breaks,*static_cast<const WP::Guid*>(keys.data)):
            state.live_breaks.end();
        const auto barricade=world_input(world_breakable_barricade);
        const bool animated=live!=state.live_breaks.end()&&world_breakable_animate(actor,barricade&&api.isa(actor,barricade));
        if(live!=state.live_breaks.end())state.live_breaks.erase(live);
        if(!animated){
            clear(cache.address(actor),cache.field);
            api.event(actor,restore,nullptr);
        }
        ++world_actor_breakables;
    }
}
static void world_actor_step(void* pawn,double time){
    auto& state=world_actor;
    if(host_role||!world_progress_ready())return;
    world_breakables_step(pawn,time);
    if(!state.changed.empty()&&time>=state.next_scan){
        state.next_scan=time+.25;
        if(time>=state.expires)state.changed.clear();
        else if(world_actor_bind())world_actor_scan(pawn);
    }
    for(unsigned i=0;i<3&&!state.queue.empty();++i){
        const auto key=state.queue.back();
        state.queue.pop_back();
        if(const auto actor=api.weakget(&key))world_actor_restore_chest(actor);
    }
}
