namespace Lift=qdcoop::elevator_intent;
struct WorldElevatorActor {
    Weak actor;
    std::array<WP::Guid,2> keys{};
    bool running{},controlled{};
    double next_motion{},finished_until{};
};
struct ElevatorMotionTrack {Lift::Motion current,previous;double received{};};
struct ElevatorLeverLease {Weak lever;bool original{};};
struct ElevatorPending {Lift::Message request;double expires{},next_send{};};
struct ElevatorPeerRequests {
    std::array<Lift::Message,16> replies{};
    qdcoop::Id character;
    std::uint64_t newest{};
    std::uint32_t generation{};
};
struct WorldElevators {
    Weak type,lever_type,root_type;
    void *running{},*end{},*reverse{},*locked{},*silence{};
    int guids{},mode{},onboard{},root{},index{-1};
    std::array<int,2> levers{};
    std::vector<WorldElevatorActor> actors;
    std::vector<ElevatorLeverLease> leases;
    std::vector<ElevatorMotionTrack> motion;
    WP::Streaming streams;
    std::array<ElevatorPending,4> pending{};
    std::array<ElevatorPeerRequests,Player::slots-1> peers{};
    std::uint64_t serial{},motion_sequence{};
    double next_scan{},next_apply{},discover_until{},next_motion_bind{};
    bool motion_bound{};
};
static WorldElevators world_elevators;

static bool world_elevator_bind(void* type,void* lever){
    auto& state=world_elevators;
    if(type&&lever&&api.weakget(&state.type)==type&&api.weakget(&state.lever_type)==lever&&state.silence)return true;
    const auto start=world_input(world_elevator_start),update=world_input(world_elevator_update);
    if(!type||!lever||!start||!update||*api.parms(start)||*api.parms(update)||
       AnimU::field<int>(start,104)!=18||AnimU::field<int>(update,104)!=727)return false;
    const auto guids=world_actor_array(type,"GUIDs",16,"/Script/CoreUObject.Guid");
    const auto mode=catalog_field(type,"LoopMode",1),onboard=catalog_field(type,"OnboardLever",8),
               root=catalog_field(type,"ElevatorPlatformRoot",8);
    const auto running=catalog_field(type,"Running",1),end=catalog_field(type,"AtEnd",1),
               reverse=catalog_field(type,"PlayingInReverse",1),locked=catalog_field(type,"LockedBecauseNoRide",1),
               silence=catalog_field(lever,"DisableLockedMessage",1);
    for(const auto field:{running,end,reverse,locked,silence})if(!medic_field_class(field,"BoolProperty"))return false;
    std::string path;
    if(!guids.field||!medic_field_class(mode,"ByteProperty")||!medic_field_class(onboard,"ObjectProperty")||
       !catalog_path(AnimU::field<void*>(mode,0x78),path)||path!="/Game/Blueprints/Elevator/ElevatorLoopMode.ElevatorLoopMode"||
       AnimU::field<void*>(onboard,0x78)!=lever)return false;
    if(!medic_field_class(root,"ObjectProperty")||
       !medic_params(world_elevator_location,12,{{"ReturnValue",12,0}}))return false;
    const auto root_type=AnimU::field<void*>(root,0x78);
    if(!catalog_path(root_type,path)||path!="/Script/Engine.SceneComponent")return false;
    std::array<int,2> levers{};
    for(unsigned i=0;i<levers.size();++i){
        const auto field=catalog_field(type,i?"EndLevers":"StartLevers",16);
        if(!medic_field_class(field,"ArrayProperty"))return false;
        const auto inner=AnimU::field<void*>(field,0x78);
        if(!medic_field_class(inner,"ObjectProperty")||*api.size(inner)!=8||AnimU::field<void*>(inner,0x78)!=lever)return false;
        levers[i]=*api.offset(field);
    }
    if(!makeweak(state.type,type)||!makeweak(state.lever_type,lever)||!makeweak(state.root_type,root_type))return false;
    state.running=running;
    state.end=end;
    state.reverse=reverse;
    state.locked=locked;
    state.silence=silence;
    state.guids=guids.offset;
    state.mode=*api.offset(mode);
    state.onboard=*api.offset(onboard);
    state.root=*api.offset(root);
    const auto index=catalog_field(type,"CurrentPathIndex",4);
    if(medic_field_class(index,"IntProperty"))state.index=*api.offset(index);
    state.levers=levers;
    return true;
}
static void world_elevator_end_control(void* actor,bool notify){
    const auto& state=world_elevators;
    api.setbool(state.running,actor,false,0);
    if(const auto stop=world_input(world_elevator_stop_fx);stop&&!*api.parms(stop))api.event(actor,stop,nullptr);
    if(notify){
        if(const auto stopped=world_input(world_elevator_stopped);stopped&&!*api.parms(stopped))
            api.event(actor,stopped,nullptr);
    }else if(const auto update=world_input(world_elevator_update))api.event(actor,update,nullptr);
}
static void elevator_release_lever(const ElevatorLeverLease& lease){
    const auto lever=api.weakget(&lease.lever),flag=world_elevators.silence;
    if(lever&&flag&&api.getbool(flag,lever,0))api.setbool(flag,lever,lease.original,0);
    ++world_elevator_releases;
}
static void world_elevator_cleanup(){
    for(const auto& entry:world_elevators.actors)
        if(entry.controlled)if(const auto actor=api.weakget(&entry.actor))world_elevator_end_control(actor,false);
    for(const auto& lease:world_elevators.leases)elevator_release_lever(lease);
    world_elevators={};
}
static void world_elevator_scan(void* pawn,void* type){
    auto& state=world_elevators;
    NativeActorArray actors{};
    native_get_actors(pawn,type,&actors);
    auto previous=std::move(state.actors);
    state.actors.clear();
    if(actors.data&&actors.count>=0&&actors.count<=256&&actors.capacity>=actors.count)
        for(int i=0;i<actors.count;++i){
            const auto actor=actors.data[i];
            if(!actor||!api.isa(actor,type)||actor_is_destroying(actor))continue;
            const auto guids=AnimU::field<WorldArray>(actor,state.guids);
            if(!guids.valid(32)||!guids.count)continue;
            const auto keys=static_cast<const WP::Guid*>(guids.data);
            if(keys[0]==WP::Guid{}||WP::personal(keys[0]))continue;
            WorldElevatorActor entry{};
            if(!makeweak(entry.actor,actor))continue;
            entry.keys[0]=keys[0];
            if(guids.count>=2&&!WP::personal(keys[1]))entry.keys[1]=keys[1];
            const auto old=std::ranges::find_if(previous,[&](const auto& prior){return api.weakget(&prior.actor)==actor;});
            entry.running=old!=previous.end()?old->running:api.getbool(state.running,actor,0);
            if(old!=previous.end()){
                entry.controlled=old->controlled;
                entry.next_motion=old->next_motion;
                entry.finished_until=old->finished_until;
            }
            state.actors.push_back(entry);
        }
    if(actors.data)native_free(actors.data);
    const auto first_key=[](const auto& entry){return entry.keys[0];};
    std::ranges::sort(state.actors,{},first_key);
    for(std::size_t i=0;i<state.actors.size();){
        auto end=i+1;
        while(end<state.actors.size()&&first_key(state.actors[i])==first_key(state.actors[end]))++end;
        if(end==i+1)++i;
        else state.actors.erase(state.actors.begin()+i,state.actors.begin()+end);
    }
}
static std::optional<std::int32_t> elevator_save_value(const WP::Guid& key){
    if(key==WP::Guid{})return std::nullopt;
    const auto& values=world_progress.authoritative.actors;
    const auto field=std::ranges::lower_bound(values,key,{},&WP::ActorState::key);
    return field!=values.end()&&field->key==key?std::optional<std::int32_t>(field->value):std::nullopt;
}
static void world_elevator_apply(void* actor,const WorldElevatorActor& entry){
    const auto& state=world_elevators;
    const auto start=world_input(world_elevator_start),update=world_input(world_elevator_update);
    if(!start||!update)return;
    const auto locked=elevator_save_value(entry.keys[1]);
    bool changed=locked&&api.getbool(state.locked,actor,0)!=(*locked<1);
    if(changed)api.setbool(state.locked,actor,*locked<1,0);
    const auto goal=elevator_save_value(entry.keys[0]);
    const auto end=api.getbool(state.end,actor,0);
    if(goal&&!AnimU::field<std::uint8_t>(actor,state.mode)&&!api.getbool(state.running,actor,0)&&end!=(*goal!=0)){
        api.setbool(state.reverse,actor,end,0);
        api.event(actor,start,nullptr);
        changed=true;
        if(api.getbool(state.running,actor,0))++world_elevator_moves;
    }
    if(changed)api.event(actor,update,nullptr);
}
template<class Visit>static void visit_elevator_levers(void* actor,Visit visit){
    const auto& state=world_elevators;
    const auto lever_type=api.weakget(&state.lever_type);
    if(!lever_type)return;
    const auto add=[&](void* lever){
        if(lever&&api.isa(lever,lever_type))visit(lever);
    };
    add(AnimU::field<void*>(actor,state.onboard));
    for(const auto offset:state.levers){
        const auto array=AnimU::field<WorldArray>(actor,offset);
        if(!array.valid(32))continue;
        const auto levers=static_cast<void* const*>(array.data);
        for(int i=0;i<array.count;++i)add(levers[i]);
    }
}
static bool world_elevator_controls(void* lever){
    for(const auto& entry:world_elevators.actors){
        const auto actor=api.weakget(&entry.actor);
        if(!actor)continue;
        bool found=false;
        visit_elevator_levers(actor,[&](void* candidate){found|=candidate==lever;});
        if(found)return true;
    }
    return false;
}
static void world_elevator_busy(){
    auto& state=world_elevators;
    std::vector<void*> needed;
    for(const auto& entry:state.actors){
        const auto actor=api.weakget(&entry.actor);
        if(!actor||!entry.running)continue;
        visit_elevator_levers(actor,[&](void* lever){
            if(std::ranges::find(needed,lever)==needed.end())needed.push_back(lever);
        });
    }
    std::erase_if(state.leases,[&](const auto& lease){
        if(std::ranges::find(needed,api.weakget(&lease.lever))!=needed.end())return false;
        elevator_release_lever(lease);
        return true;
    });
    for(const auto lever:needed){
        if(std::ranges::none_of(state.leases,[&](const auto& lease){return api.weakget(&lease.lever)==lever;})){
            ElevatorLeverLease lease{};
            if(!makeweak(lease.lever,lever))continue;
            lease.original=api.getbool(state.silence,lever,0);
            state.leases.push_back(lease);
            ++world_elevator_leases;
        }
        api.setbool(state.silence,lever,true,0);
    }
}
static bool send_elevator_intent(const Lift::Message& message,unsigned mask){
    E::Packet packet{};
    return Lift::encode(envelope,message,packet)&&send(packet,mask);
}
static void elevator_queue_request(const WorldElevatorActor& entry,bool end,double time){
    auto& state=world_elevators;
    if(!local_life_pose.generation||local_life.life.phase!=LifePhase::standing)return;
    auto selected=std::ranges::find_if(state.pending,[&](const auto& pending){return pending.request.key==entry.keys[0];});
    if(selected==state.pending.end())selected=std::ranges::find_if(state.pending,[&](const auto& pending){
        return !pending.request.serial||pending.expires<=time;
    });
    if(selected==state.pending.end())return;
    if(!++state.serial)++state.serial;
    *selected={{state.serial,entry.keys[0],local_life_pose.generation,end,Lift::Result::request},time+3.,0.};
    ++elevator_intents;
}
static void elevator_retry_requests(double time){
    for(auto& pending:world_elevators.pending){
        if(!pending.request.serial)continue;
        if(pending.expires<=time||pending.request.generation!=local_life_pose.generation||
           local_life.life.phase!=LifePhase::standing)pending={};
        else if(time>=pending.next_send&&send_elevator_intent(pending.request,1))pending.next_send=time+.2;
    }
}
static bool elevator_near(void* actor,const LifePeer& peer){
    const auto& state=world_elevators;
    const auto within_range=[&](const auto& point){
        return point!=std::array<float,3>{}&&stream_distance(point,peer.position)<=1200.f*1200.f;
    };
    if(within_range(native_actor_location(actor)))return true;
    const auto root=AnimU::field<void*>(actor,state.root),type=api.weakget(&state.root_type),
               location=world_input(world_elevator_location);
    if(root&&type&&location&&api.isa(root,type)){
        std::array<float,3> position{};
        api.event(root,location,position.data());
        if(within_range(position))return true;
    }
    bool found=false;
    visit_elevator_levers(actor,[&](void* lever){if(!found)found=within_range(native_actor_location(lever));});
    return found;
}
static bool elevator_accept_request(const Lift::Message& request,const LifePeer& peer){
    const auto& state=world_elevators;
    const auto entry=std::ranges::lower_bound(state.actors,request.key,{},[](const auto& actor){return actor.keys[0];});
    if(entry==state.actors.end()||entry->keys[0]!=request.key)return false;
    const auto actor=api.weakget(&entry->actor),start=world_input(world_elevator_start),update=world_input(world_elevator_update);
    if(!actor||!start||!update||AnimU::field<std::uint8_t>(actor,state.mode)||!elevator_near(actor,peer))return false;
    if(api.getbool(state.running,actor,0))return request.end!=api.getbool(state.reverse,actor,0);
    const auto end=api.getbool(state.end,actor,0);
    if(end==request.end)return true;
    api.setbool(state.reverse,actor,end,0);
    api.event(actor,start,nullptr);
    api.event(actor,update,nullptr);
    return api.getbool(state.running,actor,0);
}
static bool receive_elevator_intent(const ReceivedPacket& received,unsigned kind,double time){
    if(kind==Lift::motion_kind){
        Lift::Motion motion;
        if(host_role||received.peer||!(active_peer_mask&1u)||!Lift::decode(envelope,received.packet,motion))return true;
        auto& tracks=world_elevators.motion;
        auto track=std::ranges::find_if(tracks,[&](const auto& item){return item.current.key==motion.key;});
        if(track==tracks.end()){
            if(tracks.size()>=256)return true;
            tracks.push_back({motion,{},time});
            ++elevator_motion_received;
        }else if(motion.sequence>track->current.sequence&&motion.time>=track->current.time){
            *track={motion,track->current,time};
            ++elevator_motion_received;
        }
        return true;
    }
    if(kind!=Lift::kind)return false;
    Lift::Message message{};
    if(!active_peer_mask||!Lift::decode(envelope,received.packet,message)||WP::personal(message.key))return true;
    auto& state=world_elevators;
    if(!host_role){
        if(received.peer||message.result==Lift::Result::request||message.generation!=local_life_pose.generation)return true;
        const auto pending=std::ranges::find_if(state.pending,[&](const auto& entry){
            return entry.request.serial==message.serial&&entry.request.key==message.key&&entry.request.end==message.end&&
                   entry.request.generation==message.generation&&entry.expires>time;
        });
        if(pending!=state.pending.end()){
            if(message.result==Lift::Result::accepted)++elevator_guest_accepts;
            else ++elevator_guest_refusals;
            *pending={};
        }
        return true;
    }
    if(message.result!=Lift::Result::request||received.peer>=state.peers.size()||
       !(active_peer_mask&(1u<<received.peer))||!world_progress_ready())return true;
    const auto& peer=life_peers[received.peer+1];
    if(peer.generation!=message.generation||!fresh_life(peer,LifePhase::standing,time))return true;
    auto& history=state.peers[received.peer];
    if(history.generation!=message.generation||history.character!=peer.rescue_identity){
        history={};
        history.generation=message.generation;
        history.character=peer.rescue_identity;
    }
    if(message.serial<=history.newest&&history.newest-message.serial>=history.replies.size())return true;
    auto& response=history.replies[message.serial%history.replies.size()];
    if(response.serial!=message.serial){
        response=message;
        const auto accepted=elevator_accept_request(message,peer);
        response.result=accepted?Lift::Result::accepted:Lift::Result::refused;
        if(accepted)++elevator_host_accepts;
        else ++elevator_host_refusals;
        history.newest=std::max(history.newest,message.serial);
    }
    if(response.key==message.key&&response.end==message.end)send_elevator_intent(response,1u<<received.peer);
    return true;
}
static bool elevator_motion_bind(double time){
    auto& state=world_elevators;
    if(state.motion_bound)return true;
    if(state.index<0||time<state.next_motion_bind)return false;
    state.next_motion_bind=time+.5;
    if(!world_input(world_barrier_relative)||!world_input(world_relative_set)||!world_input(world_elevator_stop_fx))
        return false;
    state.motion_bound=medic_params(world_barrier_relative,48,{{"ReturnValue",48,0}})&&
        medic_params(world_relative_set,189,{{"NewTransform",48,0},{"bSweep",1,48},
            {"SweepHitResult",136,52},{"bTeleport",1,188}})&&
        medic_params(world_elevator_cancel,0,{})&&medic_params(world_elevator_started,0,{})&&
        medic_params(world_elevator_stopped,0,{})&&medic_params(world_elevator_start_fx,0,{})&&
        medic_params(world_elevator_stop_fx,0,{});
    return state.motion_bound;
}
static void* elevator_motion_root(void* actor){
    const auto root=AnimU::field<void*>(actor,world_elevators.root);
    const auto type=api.weakget(&world_elevators.root_type);
    return root&&type&&api.isa(root,type)?root:nullptr;
}
static void elevator_publish_motion(void* actor,WorldElevatorActor& entry,bool running,double time){
    auto& state=world_elevators;
    if(running!=entry.running){
        entry.next_motion=0.;
        if(!running)entry.finished_until=time+1.;
    }
    if(time<entry.next_motion||(!running&&time>=entry.finished_until)||state.motion_sequence==UINT64_MAX)return;
    const auto root=elevator_motion_root(actor);
    if(!root)return;
    const auto index=AnimU::field<int>(actor,state.index);
    if(index<0)return;
    alignas(16) FactoryParameters args{};
    api.event(root,world_input(world_barrier_relative),args.data());
    const auto transform=AnimU::field<std::array<float,12>>(args.data(),0);
    Lift::Motion motion{entry.keys[0],++state.motion_sequence,time,{transform[4],transform[5],transform[6]},
        {transform[0],transform[1],transform[2],transform[3]},static_cast<unsigned>(index),running,
        running?!api.getbool(state.reverse,actor,0):api.getbool(state.end,actor,0)};
    E::Packet packet{};
    if(Lift::encode(envelope,motion,packet)&&send(packet,active_peer_mask)){
        entry.next_motion=time+.10;
        ++elevator_motion_sent;
    }
}
static bool elevator_follow_motion(void* actor,WorldElevatorActor& entry,double time){
    auto& state=world_elevators;
    const auto track=std::ranges::find_if(state.motion,[&](const auto& item){return item.current.key==entry.keys[0];});
    if(track==state.motion.end()||time-track->received>.85){
        if(entry.controlled){
            world_elevator_end_control(actor,false);
            entry.controlled=false;
        }
        return false;
    }
    const auto& motion=track->current;
    const auto root=elevator_motion_root(actor);
    if(!root)return false;
    const bool local_running=api.getbool(state.running,actor,0);
    if(!motion.running&&!entry.controlled&&local_running)return false;
    if(motion.running&&!entry.controlled){
        if(local_running)api.event(actor,world_input(world_elevator_cancel),nullptr);
        api.setbool(state.running,actor,true,0);
        api.setbool(state.reverse,actor,!motion.end,0);
        if(!local_running){
            api.event(actor,world_input(world_elevator_started),nullptr);
            api.event(actor,world_input(world_elevator_start_fx),nullptr);
        }
        entry.controlled=true;
        ++world_elevator_moves;
    }
    auto target=motion.position;
    const auto& previous=track->previous;
    const auto period=motion.time-previous.time;
    if(motion.running&&previous.running&&previous.end==motion.end&&period>=.04&&period<=.35){
        const auto& clock=remote_players[0].clock;
        const auto sample=clock.initialized?motion.time+clock.offset:track->received;
        const auto ahead=static_cast<float>(std::clamp(time-sample,0.,.12)/period);
        for(unsigned i=0;i<target.size();++i)
            target[i]+=std::clamp((motion.position[i]-previous.position[i])*ahead,-160.f,160.f);
    }
    alignas(16) FactoryParameters args{};
    api.event(root,world_input(world_barrier_relative),args.data());
    auto transform=AnimU::field<std::array<float,12>>(args.data(),0);
    const std::array<float,3> position{transform[4],transform[5],transform[6]};
    float alignment{};
    for(unsigned i=0;i<4;++i)alignment+=motion.rotation[i]*transform[i];
    const float position_tolerance=motion.running?1.f:.0001f;
    if(stream_distance(target,position)>position_tolerance||std::abs(alignment)<.99995f){
        std::copy(motion.rotation.begin(),motion.rotation.end(),transform.begin());
        std::copy(target.begin(),target.end(),transform.begin()+4);
        args={};
        factory_put(args,0,transform);
        api.event(root,world_input(world_relative_set),args.data());
    }
    const auto path_index=static_cast<int>(motion.index);
    std::memcpy(static_cast<unsigned char*>(actor)+state.index,&path_index,sizeof(path_index));
    if(!motion.running){
        api.setbool(state.end,actor,motion.end,0);
        api.setbool(state.reverse,actor,motion.end,0);
        if(entry.controlled){
            world_elevator_end_control(actor,true);
            entry.controlled=false;
        }
    }
    return true;
}
static void world_elevator_step(void* pawn,double time){
    auto& state=world_elevators;
    if(host_role)
        for(unsigned peer=0;peer<state.peers.size();++peer)
            if(state.peers[peer].generation&&!(active_peer_mask&(1u<<peer)))state.peers[peer]={};
    if(!active_peer_mask){
        world_elevator_cleanup();
        return;
    }
    if(!native_get_actors||!native_free)return;
    bool busy_changed=false;
    if(time>=state.next_scan){
        state.next_scan=time+.5;
        const auto& streams=host_role?world_stream.published:world_progress.authoritative.streaming;
        if(state.streams!=streams){state.streams=streams;state.discover_until=time+4.;}
        if(time<state.discover_until)
            for(unsigned i=world_elevator_type;i<=world_elevator_stop_fx;++i)
                if(!world_input(i)){scan_world_inputs(4096);break;}
        std::erase_if(state.motion,[&](const auto& track){return time-track.received>2.;});
        const auto type=world_input(world_elevator_type),lever=world_input(world_elevator_lever);
        if(world_elevator_bind(type,lever)){
            world_elevator_scan(pawn,type);
            busy_changed=true;
        }
    }
    const bool apply=!host_role&&world_progress_ready()&&time>=state.next_apply;
    if(apply)state.next_apply=time+.25;
    const bool motion_bound=elevator_motion_bind(time);
    for(auto& entry:state.actors){
        const auto actor=api.weakget(&entry.actor);
        if(actor&&!AnimU::field<std::uint8_t>(actor,state.mode)){
            const bool running=api.getbool(state.running,actor,0);
            if(host_role){
                if(motion_bound)elevator_publish_motion(actor,entry,running,time);
            }else{
                if(!entry.running&&running&&!entry.controlled)
                    elevator_queue_request(entry,!api.getbool(state.reverse,actor,0),time);
                if(!(motion_bound&&elevator_follow_motion(actor,entry,time))&&apply)
                    world_elevator_apply(actor,entry);
            }
        }else if(actor&&apply)world_elevator_apply(actor,entry);
        const bool running=actor&&api.getbool(state.running,actor,0);
        busy_changed|=entry.running!=running;
        entry.running=running;
    }
    if(busy_changed)world_elevator_busy();
    if(!host_role)elevator_retry_requests(time);
}
