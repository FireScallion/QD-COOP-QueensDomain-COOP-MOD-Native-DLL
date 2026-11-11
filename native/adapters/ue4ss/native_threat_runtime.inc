namespace Threat=qdcoop::threat;
struct NativeThreatSight {
    Weak component;
    void *enabled{},*visible{},*los{},*in_range{};
    int range{-1};
    float component_range{},actor_range{};
    bool tick_before{},enabled_before{},visible_before{},los_before{},in_range_before{},actor_seen{},actor_los{};
};
struct NativeThreatEnemy {
    Threat::Threat choice;
    Weak actor,previous,assigned;
    void* assigned_address{};
    void* lizard_mode{};
    std::uint32_t generation{};
    double next{},wake_until{};
    NativeThreatSight sight;
    std::array<std::array<float,3>,Player::slots> last_visible{};
    unsigned visible_mask{};
    Weak memory;
    unsigned memory_slot{Threat::none};
    std::uint32_t memory_generation{};
    bool lizard_checked{};
};
struct NativeThreatLayout {
    int target{-1},data{-1},range{-1},viewpoint{-1},buildup{-1},seen_range{-1};
    void *attacking{},*recovery{},*aggrod{},*chasing{},*disabled{},*bestiary{},*preview{};
    void *seen{},*has_los{};
    int complex_trace{-1};
};
static NativeThreatLayout threat_layout;
static std::array<NativeThreatEnemy,E::capacity> threat_enemies{};
static unsigned threat_cursor;
static double threat_next_bind;
static bool threat_memory_step(void* actor,NativeThreatEnemy& state,unsigned slot,double time);
static bool threat_memory_clear(NativeThreatEnemy& state);
static bool threat_memory_detach();

static int threat_offset(void* type,std::string_view name,int width,std::string_view kind){
    const auto field=catalog_field(type,name,width);
    return medic_field_class(field,kind)?*api.offset(field):-1;
}
static void* threat_bool(void* type,std::string_view name){
    const auto field=catalog_field(type,name,1);
    return medic_field_class(field,"BoolProperty")?field:nullptr;
}
static bool bind_native_threat(double time){
    if(threat_layout.target>=0)return true;
    if(time<threat_next_bind)return false;
    threat_next_bind=time+.5;
    for(unsigned i=world_threat_height;i<=world_threat_tick_get;++i)if(!world_input(i)){
        scan_world_inputs(4096);
        break;
    }
    threat_stage=1;
    const auto height=world_input(world_threat_height),forward=world_input(world_threat_forward);
    const auto range=world_input(world_threat_awareness),force=world_input(world_threat_aggro);
    const auto unaggro=world_input(world_threat_unaggro),los=world_input(world_threat_los_update);
    const auto stop=world_input(world_threat_stop),trace=world_input(world_life_trace);
    const auto tick=world_input(world_threat_tick_get);
    if(!height||!forward||!range||!force||!unaggro||!los||!stop||!trace||!world_input(world_life_controller)||
       !world_input(world_kismet)||!tick)return false;
    threat_stage=2;
    if(*api.parms(height)!=4||*api.parms(forward)!=12||*api.parms(range)!=8||*api.parms(force)||*api.parms(unaggro)||
       *api.parms(los)||*api.parms(stop)||*api.parms(tick)!=1||factory_parameter(height,"ReturnValue",4)!=0||
       factory_parameter(forward,"ReturnValue",12)!=0||factory_parameter(range,"ReturnValue",4)!=4)return false;
    NativeThreatLayout binding{};
    binding.target=threat_offset(base_class,"TargetPlayer",8,"ObjectProperty");
    binding.data=threat_offset(base_class,"Data",8,"ObjectProperty");
    binding.range=threat_offset(base_class,"AggroRange",4,"FloatProperty");
    binding.viewpoint=threat_offset(base_class,"LocalViewpoint",48,"StructProperty");
    binding.buildup=threat_offset(base_class,"AggroBuildup",4,"FloatProperty");
    binding.seen_range=threat_offset(base_class,"SeenPlayerRange",4,"FloatProperty");
    binding.attacking=threat_bool(base_class,"bIsAttacking");
    binding.recovery=threat_bool(base_class,"bInRecoveryTime");
    binding.aggrod=threat_bool(base_class,"bAggrod");
    binding.chasing=threat_bool(base_class,"bIsChasing");
    binding.disabled=threat_bool(base_class,"bIsAIDisabled");
    binding.bestiary=threat_bool(base_class,"BestiaryInstance");
    binding.preview=threat_bool(base_class,"bSpawnerPreviewInstance");
    binding.seen=threat_bool(base_class,"bPlayerIsSeen");
    binding.has_los=threat_bool(base_class,"HasPlayerlos");
    binding.complex_trace=factory_parameter(trace,"bTraceComplex",1);
    threat_stage=3;
    if(binding.target<0||binding.data<0||binding.range<0||binding.viewpoint<0||binding.buildup<0||binding.complex_trace<0||
       binding.seen_range<0||!binding.attacking||!binding.recovery||!binding.aggrod||!binding.disabled||!binding.seen||!binding.has_los)return false;
    threat_layout=binding;
    threat_stage=4;
    return true;
}
static bool threat_flag(void* field,void* actor){return field&&api.getbool(field,actor,0);}
static void threat_sight_restore(NativeThreatSight& state,void* actor){
    const auto component=api.weakget(&state.component);
    if(actor&&component&&catalog_object_field(actor,"EnemyLineOfSight")==component){
        const auto& layout=threat_layout;
        api.setbool(state.enabled,component,state.enabled_before,0);
        api.setbool(state.visible,component,state.visible_before,0);
        api.setbool(state.los,component,state.los_before,0);
        api.setbool(state.in_range,component,state.in_range_before,0);
        api.setbool(layout.seen,actor,state.actor_seen,0);
        api.setbool(layout.has_los,actor,state.actor_los,0);
        std::memcpy(static_cast<unsigned char*>(component)+state.range,&state.component_range,sizeof(float));
        std::memcpy(static_cast<unsigned char*>(actor)+layout.seen_range,&state.actor_range,sizeof(float));
        if(const auto tick=world_input(world_component_tick))own_set(component,tick,state.tick_before);
    }
    state={};
}
static bool threat_perception(NativeThreatEnemy& state,void* actor,const Threat::Candidate& candidate){
    auto& sight=state.sight;
    const auto component=catalog_object_field(actor,"EnemyLineOfSight");
    if(!component)return false;
    if(component!=api.weakget(&sight.component)){
        threat_sight_restore(sight,actor);
        const auto type=AnimU::field<void*>(component,16);
        const auto get=world_input(world_threat_tick_get),set=world_input(world_component_tick);
        sight.enabled=threat_bool(type,"bEnabled");
        sight.visible=threat_bool(type,"bCanSeeTarget");
        sight.los=threat_bool(type,"bHasLOS");
        sight.in_range=threat_bool(type,"bTargetIsInRange");
        sight.range=threat_offset(type,"SeenRange",4,"FloatProperty");
        if(!sight.enabled||!sight.visible||!sight.los||!sight.in_range||sight.range<0||!get||!set||!makeweak(sight.component,component))return false;
        sight.enabled_before=threat_flag(sight.enabled,component);
        sight.visible_before=threat_flag(sight.visible,component);
        sight.los_before=threat_flag(sight.los,component);
        sight.in_range_before=threat_flag(sight.in_range,component);
        sight.actor_seen=threat_flag(threat_layout.seen,actor);
        sight.actor_los=threat_flag(threat_layout.has_los,actor);
        sight.component_range=AnimU::field<float>(component,sight.range);
        sight.actor_range=AnimU::field<float>(actor,threat_layout.seen_range);
        std::uint8_t ticking{};
        api.event(component,get,&ticking);
        sight.tick_before=ticking!=0;
        api.setbool(sight.enabled,component,false,0);
        own_set(component,set,false);
    }
    const auto distance=candidate.distance;
    const bool visible=candidate.alive&&candidate.in_range&&candidate.los;
    std::memcpy(static_cast<unsigned char*>(actor)+threat_layout.seen_range,&distance,sizeof(distance));
    std::memcpy(static_cast<unsigned char*>(component)+sight.range,&distance,sizeof(distance));
    api.setbool(threat_layout.seen,actor,visible,0);
    api.setbool(threat_layout.has_los,actor,visible,0);
    api.setbool(sight.visible,component,visible,0);
    api.setbool(sight.los,component,visible,0);
    api.setbool(sight.in_range,component,candidate.in_range,0);
    return true;
}
static bool threat_flee(const NativeThreatEnemy& state,void* actor){
    return state.lizard_mode&&!threat_flag(state.lizard_mode,actor);
}
static void threat_write_target(void* actor,void* target,bool cancel_move){
    std::memcpy(static_cast<unsigned char*>(actor)+threat_layout.target,&target,sizeof(target));
    const auto update=world_input(world_threat_los_update);
    if(const auto los=catalog_object_field(actor,"EnemyLineOfSight");los&&update)api.event(los,update,nullptr);
    const auto get=world_input(world_life_controller),stop=world_input(world_threat_stop);
    void* controller{};
    if(cancel_move&&get&&stop&&threat_flag(threat_layout.chasing,actor)){
        api.event(actor,get,&controller);
        if(controller)api.event(controller,stop,nullptr);
    }
}
static void threat_restore(NativeThreatEnemy& state){
    threat_memory_clear(state);
    if(!state.assigned_address&&!api.weakget(&state.sight.component))return;
    const auto actor=api.weakget(&state.actor);
    const auto health=native_damage.binding.health;
    const bool alive=actor&&health>=0&&AnimU::field<float>(actor,health)>0.f&&!actor_is_destroying(actor);
    threat_sight_restore(state.sight,alive?actor:nullptr);
    if(actor&&state.assigned_address&&threat_layout.target>=0&&AnimU::field<void*>(actor,threat_layout.target)==state.assigned_address){
        const auto target=api.weakget(&state.previous);
        if(alive)threat_write_target(actor,target,!threat_flee(state,actor));
        else std::memcpy(static_cast<unsigned char*>(actor)+threat_layout.target,&target,sizeof(target));
        ++threat_restores;
    }
    state.assigned={};
    state.assigned_address=nullptr;
    state.previous={};
}
static void native_threat_release_proxy(void* proxy){
    if(!proxy)return;
    for(auto& state:threat_enemies)if(state.assigned_address==proxy)threat_restore(state);
}
static bool native_threat_cleanup(){
    bool clean=true;
    for(auto& state:threat_enemies){
        threat_restore(state);
        if(api.weakget(&state.memory))clean=false;
        else state={};
    }
    if(!threat_memory_detach())clean=false;
    threat_layout={};
    threat_cursor=0;
    threat_next_bind=0;
    return clean;
}
static NativeThreatEnemy& threat_enemy(unsigned index){
    auto& state=threat_enemies[index];
    const auto& entity=entities[index];
    if(state.generation!=entity.generation){
        threat_restore(state);
        const auto pending_memory=state.memory;
        const auto pending_generation=state.memory_generation;
        state={};
        state.memory=pending_memory;
        state.memory_generation=pending_generation;
        state.generation=entity.generation;
        state.actor=entity.actor;
    }
    return state;
}
static void native_threat_record(unsigned index,unsigned player,float amount,double time){
    auto& state=threat_enemy(index);
    state.choice.record(player,amount,time);
    state.wake_until=time+4.;
    state.next=0;
}
static float threat_distance(const std::array<float,3>& a,const std::array<float,3>& b){
    const auto x=a[0]-b[0],y=a[1]-b[1],z=a[2]-b[2];
    return std::sqrt(x*x+y*y+z*z);
}
static std::array<float,3> threat_view(void* actor,const std::array<float,3>& origin){
    const auto capsule=catalog_object_field(actor,"CapsuleComponent");
    if(!capsule)return origin;
    alignas(16) FactoryParameters args{};
    api.event(capsule,getters[4].fn,args.data());
    const auto transform=AnimU::field<std::array<float,12>>(args.data(),getters[4].at);
    const auto local=AnimU::field<std::array<float,3>>(actor,threat_layout.viewpoint+16);
    const float x=local[0]*transform[8],y=local[1]*transform[9],z=local[2]*transform[10];
    const float qx=transform[0],qy=transform[1],qz=transform[2],w=transform[3];
    const float dot=2.f*(qx*x+qy*y+qz*z),factor=w*w-(qx*qx+qy*qy+qz*qz);
    const std::array<float,3> point{transform[4]+dot*qx+factor*x+2.f*w*(qy*z-qz*y),
                                    transform[5]+dot*qy+factor*y+2.f*w*(qz*x-qx*z),
                                    transform[6]+dot*qz+factor*z+2.f*w*(qx*y-qy*x)};
    return std::ranges::all_of(point,[](float value){return std::isfinite(value);})?point:origin;
}
static bool threat_clear(void* actor,const std::array<float,3>& start,const std::array<float,3>& end,
                         std::span<void*> ignored){
    const auto trace=world_input(world_life_trace),kismet=world_input(world_kismet);
    if(!trace||!kismet||!local_life.actor||!local_life.layout.trace[4])return false;
    alignas(16) FactoryParameters args{};
    const auto& at=local_life.layout.trace;
    if(std::ranges::min(at)<0)return false;
    factory_put(args,at[0],actor);
    factory_put(args,at[1],start);
    factory_put(args,at[2],end);
    factory_put(args,at[3],std::uint8_t{1});
    factory_put(args,threat_layout.complex_trace,std::uint8_t{1});
    const auto count=static_cast<int>(ignored.size());
    factory_put(args,at[6],WorldArray{ignored.data(),count,count});
    api.event(kismet,trace,args.data());
    ++threat_traces;
    return !args[at[4]];
}
static float threat_awareness(void* actor){
    const auto data=AnimU::field<void*>(actor,threat_layout.data);
    const auto profile=data?catalog_object_field(data,"AwarenessProfile"):nullptr;
    const auto fn=world_input(world_threat_awareness);
    if(!profile||!fn)return 0.f;
    std::array<float,2> args{AnimU::field<float>(actor,threat_layout.range),0.f};
    api.event(profile,fn,args.data());
    return std::isfinite(args[1])?std::clamp(args[1],0.f,10000.f):0.f;
}
static void evaluate_native_threat(unsigned index,void* actor,const std::array<void*,Player::slots>& pawns,double time){
    auto& state=threat_enemy(index);
    const auto position=native_actor_location(actor);
    const float awareness=threat_awareness(actor);
    if(awareness<=0){threat_restore(state);return;}
    const float interest=std::max(3500.f,awareness+750.f)+750.f;
    if(state.choice.target==Threat::none&&time>=state.wake_until&&
       std::ranges::none_of(life_peers,[&](const auto& peer){return fresh_life(peer,LifePhase::standing,time)&&
           threat_distance(position,peer.position)<=interest;})){
        state.next=time+1.;
        return;
    }
    const auto height_fn=world_input(world_threat_height),forward_fn=world_input(world_threat_forward);
    if(!height_fn||!forward_fn)return;
    float height{};
    std::array<float,3> forward{};
    api.event(actor,height_fn,&height);
    api.event(actor,forward_fn,forward.data());
    if(!std::isfinite(height)||height<=0)return;
    const auto view=threat_view(actor,position);
    std::array<void*,Player::slots+1> ignored{actor};
    unsigned count=1;
    for(auto pawn:pawns)if(pawn)ignored[count++]=pawn;
    std::array<Threat::Candidate,Player::slots> candidates{};
    for(unsigned slot=0;slot<Player::slots;++slot){
        const auto& peer=life_peers[slot];
        if(!pawns[slot]||!fresh_life(peer,LifePhase::standing,time)||peer.health<=0||
           (slot&&!(enrollment_peer_mask(index)&(1u<<(slot-1)))))continue;
        auto& candidate=candidates[slot];
        candidate.alive=true;
        candidate.distance=threat_distance(position,peer.position);
        const bool engaged=state.choice.target==slot||state.choice.recent(slot,time)>0.f;
        candidate.in_range=candidate.distance<=Threat::range(awareness,engaged)&&std::abs(position[2]-peer.position[2])<=height;
        if(!candidate.in_range)continue;
        const auto x=peer.position[0]-position[0],y=peer.position[1]-position[1];
        const auto length=std::hypot(x,y);
        candidate.front=length<1.f||x*forward[0]+y*forward[1]>=length*Threat::front_cos;
        candidate.los=threat_clear(actor,view,peer.position,std::span(ignored).first(count));
        if(candidate.los&&(candidate.front||engaged)){
            state.last_visible[slot]=peer.position;
            state.visible_mask|=1u<<slot;
        }
    }
    const auto& layout=threat_layout;
    if(!state.lizard_checked){
        const auto field=native_lizard_mode(actor);
        if(medic_field_class(field,"BoolProperty"))state.lizard_mode=field;
        state.lizard_checked=true;
    }
    const bool flee=threat_flee(state,actor);
    const bool locked=threat_flag(layout.attacking,actor)||threat_flag(layout.recovery,actor);
    const auto data=AnimU::field<void*>(actor,layout.data);
    const auto max_health=threat_offset(data?AnimU::field<void*>(data,16):nullptr,"HP",4,"FloatProperty");
    const float hp=max_health>=0?AnimU::field<float>(data,max_health):100.f;
    const bool changed=state.choice.choose(candidates,time,locked,hp,flee&&threat_flag(layout.aggrod,actor));
    const auto slot=state.choice.target;
    const bool memory=!flee&&slot>0&&slot<Threat::none&&!candidates[slot].los&&
                      time-state.choice.seen[slot]<Threat::memory_time&&
                      (state.visible_mask&(1u<<slot))&&threat_memory_step(actor,state,slot,time);
    if(slot==0||slot==Threat::none||(!flee&&!candidates[slot].los&&!memory)){
        const bool had_target=state.assigned_address!=nullptr;
        threat_restore(state);
        const auto unaggro=world_input(world_threat_unaggro);
        if(!flee&&had_target&&changed&&slot==Threat::none&&unaggro&&threat_flag(layout.aggrod,actor))api.event(actor,unaggro,nullptr);
        return;
    }
    if(!memory)threat_memory_clear(state);
    const auto selected=pawns[slot];
    const auto actual=AnimU::field<void*>(actor,layout.target);
    if(actual!=selected){
        if(actual!=api.weakget(&state.assigned)){
            state.previous={};
            if(actual&&!makeweak(state.previous,actual))return;
        }
        if(!makeweak(state.assigned,selected))return;
        state.assigned_address=selected;
        threat_write_target(actor,selected,!flee);
        ++threat_switches;
    }
    const bool perception=threat_perception(state,actor,candidates[slot]);
    const auto force=world_input(world_threat_aggro);
    if(perception&&candidates[slot].los&&force&&!threat_flag(layout.aggrod,actor)&&
       !threat_flag(layout.disabled,actor)&&candidates[slot].front){
        const float one=1.f;
        std::memcpy(static_cast<unsigned char*>(actor)+layout.buildup,&one,sizeof(one));
        api.event(actor,force,nullptr);
    }
}
static void native_threat_step(void* pawn,double time){
    std::array<void*,Player::slots> pawns{pawn};
    bool guests=false;
    for(unsigned slot=1;slot<Player::slots;++slot){
        auto& peer=native_damage.peers[slot-1];
        if(peer.standing)pawns[slot]=api.weakget(&peer.proxy);
        guests=guests||pawns[slot];
    }
    if((!guests&&threat_layout.target<0)||!bind_native_threat(time))return;
    unsigned processed{};
    for(unsigned visited=0;visited<automatic_limit&&visited<8&&processed<2;++visited){
        const auto index=threat_cursor;
        threat_cursor=(index+1)%automatic_limit;
        auto& state=threat_enemy(index);
        if(time<state.next)continue;
        state.next=time+.2;
        const auto actor=api.weakget(&state.actor);
        if(!guests||!entities[index].used||!actor||deaths[index].terminal||enrollments[index].retiring||
           threat_flag(threat_layout.bestiary,actor)||threat_flag(threat_layout.preview,actor)||threat_flag(threat_layout.disabled,actor)){
            if(state.assigned_address||api.weakget(&state.memory))threat_restore(state);
            continue;
        }
        ++processed;
        evaluate_native_threat(index,actor,pawns,time);
        if(now()-time>=.001)break;
    }
}
