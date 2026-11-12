namespace EnemyRest=qdcoop::enemy_rest_intent;

struct EnemyRestPeer {std::uint64_t sequence{};std::uint32_t generation{};qdcoop::Id identity;};
struct EnemyRestState {
    Weak pawn;
    void* resting_field{};
    bool resting{},refresh_requested{};
    std::uint64_t sequence{};
    EnemyRest::Message pending;
    double started{},next_send{},expires{},next_bind{},last_refresh{-10.};
    std::array<EnemyRestPeer,Player::slots-1> peers{};
    std::vector<Weak> spawners;
    std::size_t cursor{};
};
static EnemyRestState enemy_rest;

static bool enemy_rest_send(const EnemyRest::Message& message,unsigned mask){
    E::Packet packet{};
    return EnemyRest::encode(envelope,message,packet)&&send(packet,mask);
}
static bool receive_enemy_rest(const ReceivedPacket& received,unsigned kind,double time){
    if(kind!=EnemyRest::kind)return false;
    EnemyRest::Message message;
    if(!active_peer_mask||!EnemyRest::decode(envelope,received.packet,message))return true;
    auto& state=enemy_rest;
    if(!host_role){
        if(!received.peer&&message.ack&&message.sequence==state.pending.sequence&&
           message.generation==state.pending.generation)state.pending={};
        return true;
    }
    enemy_rest_stage=10;
    if(message.ack||received.peer>=state.peers.size()||!(active_peer_mask&(1u<<received.peer))||
       !world_progress_ready())return true;
    const auto& peer=life_peers[received.peer+1];
    if(message.generation!=peer.generation||!fresh_life(peer,LifePhase::standing,time))return true;
    enemy_rest_stage=11;
    auto& seen=state.peers[received.peer];
    if(seen.generation!=peer.generation||seen.identity!=peer.rescue_identity)
        seen={0,peer.generation,peer.rescue_identity};
    if(message.sequence>seen.sequence){
        seen.sequence=message.sequence;
        if(state.spawners.empty()&&time-state.last_refresh>3.)state.refresh_requested=true;
        ++enemy_rest_requests;
    }
    message.ack=true;
    enemy_rest_send(message,1u<<received.peer);
    return true;
}
static void enemy_rest_collect(void* pawn,double time){
    auto& state=enemy_rest;
    const auto spawner=world_input(world_rest_spawner),rest=world_input(world_rest_event),
               utils=world_input(world_rest_utils),clear=world_input(world_rest_clear);
    if(!spawner||!rest||!utils||!clear||*api.parms(rest)||*api.parms(clear)!=8||
       !api.isa(utils,AnimU::field<void*>(clear,32))||!native_get_actors||!native_free)return;
    enemy_rest_stage=12;
    NativeActorArray actors{};
    native_get_actors(pawn,spawner,&actors);
    std::vector<std::pair<double,Weak>> found;
    std::array<std::array<float,3>,Player::slots> origins{};
    const auto origin_count=native_enrollment_origins(pawn,origins);
    if(actors.data&&actors.count>=0&&actors.count<=8192&&actors.capacity>=actors.count)
        for(int i=0;i<actors.count;++i){
            const auto actor=actors.data[i];
            if(!actor||!api.isa(actor,spawner)||actor_is_destroying(actor))continue;
            const auto point=native_actor_location(actor);
            auto distance=native_enemy_distance(point,origins[0]);
            for(unsigned j=1;j<origin_count;++j)distance=std::min(distance,native_enemy_distance(point,origins[j]));
            Weak key{};
            if(std::isfinite(distance)&&makeweak(key,actor))found.emplace_back(distance,key);
        }
    if(actors.data)native_free(actors.data);
    api.event(utils,clear,&pawn);
    std::ranges::sort(found,{},&std::pair<double,Weak>::first);
    state.spawners.reserve(found.size());
    for(const auto& entry:found)state.spawners.push_back(entry.second);
    state.cursor=0;
    state.last_refresh=time;
    state.refresh_requested=false;
    ++enemy_rest_refreshes;
}
static void native_enemy_rest_step(void* pawn,double time){
    auto& state=enemy_rest;
    if(!active_peer_mask){state={};return;}
    if(host_role){
        if(time>=state.next_bind){
            state.next_bind=time+.5;
            for(unsigned input=world_rest_spawner;input<=world_rest_clear;++input)
                if(!world_input(input)){scan_world_inputs(2048);break;}
        }
        if(state.refresh_requested)enemy_rest_collect(pawn,time);
        const auto rest=world_input(world_rest_event);
        if(!rest)return;
        const auto began=now();
        for(unsigned i=0;i<8&&state.cursor<state.spawners.size()&&now()-began<.0015;++i){
            const auto actor=api.weakget(&state.spawners[state.cursor++]);
            if(!actor||actor_is_destroying(actor))continue;
            api.event(actor,rest,nullptr);
            ++enemy_rest_spawners;
        }
        if(state.cursor==state.spawners.size()){state.spawners.clear();state.cursor=0;}
        return;
    }
    if(api.weakget(&state.pawn)!=pawn){
        const auto field=catalog_field(AnimU::field<void*>(pawn,16),"Resting",1);
        if(!medic_field_class(field,"BoolProperty")||!makeweak(state.pawn,pawn))return;
        state.resting_field=field;
        state.resting=api.getbool(field,pawn,0);
        state.started=0.;
        enemy_rest_stage=1;
    }
    const bool resting=api.getbool(state.resting_field,pawn,0);
    if(resting&&!state.resting){state.started=time;enemy_rest_stage=2;}
    state.resting=resting;
    if(!resting)state.started=0.;
    else if(state.started&&time-state.started>=1.&&local_life_pose.generation&&
            local_life.life.phase==LifePhase::standing&&state.sequence<UINT64_MAX){
        state.pending={++state.sequence,local_life_pose.generation,false};
        state.expires=time+6.;
        state.next_send=0.;
        state.started=0.;
        enemy_rest_stage=3;
    }
    if(state.pending.sequence){
        if(time>=state.expires||state.pending.generation!=local_life_pose.generation)state.pending={};
        else if(time>=state.next_send&&enemy_rest_send(state.pending,1)){
            state.next_send=time+.2;
            ++enemy_rest_sent;
            enemy_rest_stage=4;
        }
    }
}
