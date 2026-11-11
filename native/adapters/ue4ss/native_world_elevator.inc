namespace Lift=qdcoop::elevator_intent;
struct WorldElevatorActor {
    Weak actor;
    std::array<WP::Guid,2> keys{};
    bool running{};
};
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
    int guids{},mode{},onboard{},root{};
    std::array<int,2> levers{};
    std::vector<WorldElevatorActor> actors;
    std::vector<ElevatorLeverLease> leases;
    WP::Streaming streams;
    std::array<ElevatorPending,4> pending{};
    std::array<ElevatorPeerRequests,Player::slots-1> peers{};
    std::uint64_t serial{};
    double next_scan{},next_apply{},discover_until{};
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
    state.levers=levers;
    return true;
}
static void elevator_release_lever(const ElevatorLeverLease& lease){
    const auto lever=api.weakget(&lease.lever),flag=world_elevators.silence;
    if(lever&&flag&&api.getbool(flag,lever,0))api.setbool(flag,lever,lease.original,0);
    ++world_elevator_releases;
}
static void world_elevator_cleanup(){
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
static void world_elevator_step(void* pawn,double time){
    auto& state=world_elevators;
    if(host_role)
        for(unsigned peer=0;peer<state.peers.size();++peer)
            if(state.peers[peer].generation&&!(active_peer_mask&(1u<<peer)))state.peers[peer]={};
    if(!active_peer_mask){
        for(const auto& lease:state.leases)elevator_release_lever(lease);
        state.leases.clear();
        return;
    }
    if(!native_get_actors||!native_free)return;
    bool busy_changed=false;
    if(time>=state.next_scan){
        state.next_scan=time+.5;
        const auto& streams=host_role?world_stream.published:world_progress.authoritative.streaming;
        if(state.streams!=streams){state.streams=streams;state.discover_until=time+4.;}
        if(time<state.discover_until)
            for(unsigned i=world_elevator_type;i<=world_elevator_location;++i)
                if(!world_input(i)){scan_world_inputs(4096);break;}
        const auto type=world_input(world_elevator_type),lever=world_input(world_elevator_lever);
        if(world_elevator_bind(type,lever)){
            world_elevator_scan(pawn,type);
            busy_changed=true;
        }
    }
    const bool apply=!host_role&&world_progress_ready()&&time>=state.next_apply;
    if(apply)state.next_apply=time+.25;
    for(auto& entry:state.actors){
        const auto actor=api.weakget(&entry.actor);
        if(actor&&!host_role&&!entry.running&&api.getbool(state.running,actor,0)&&
           !AnimU::field<std::uint8_t>(actor,state.mode))
            elevator_queue_request(entry,!api.getbool(state.reverse,actor,0),time);
        if(actor&&apply)world_elevator_apply(actor,entry);
        const bool running=actor&&api.getbool(state.running,actor,0);
        busy_changed|=entry.running!=running;
        entry.running=running;
    }
    if(busy_changed)world_elevator_busy();
    if(!host_role)elevator_retry_requests(time);
}
