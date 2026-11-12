struct WorldStreamEntry {void* list{};std::int32_t priority{};std::uint8_t trigger{};};
static_assert(sizeof(WorldStreamEntry)==16);
struct WorldStreamBinding {
    Weak manager,pawn_type,list_type;
    void *payload{},*trigger{};
    std::array<int,2> stacks{};
    std::array<void*,3> teleport{};
};
struct WorldArrivalHost {
    Weak pawn;
    WP::Arrival published;
    std::array<float,3> previous{};
    bool has_position{},teleporting{},pending{};
};
struct WorldArrivalHold {
    Weak pawn,movement,controller,manager,instance;
    WP::Streaming original,installed;
    std::uint64_t serial{};
    double expires{};
    int mode_at{};
    std::array<std::uint8_t,2> modes{};
    bool changed{};
};
struct WorldStreamRuntime {
    WorldStreamBinding binding;
    WorldArrivalHost owner;
    WorldArrivalHold hold;
    WP::Streaming published,candidate;
    WP::Revision verified;
    WP::Revision failed;
    unsigned failures{};
    std::uint64_t arrived{},attempted{};
    double next_bind{},next_apply{},candidate_since{};
    bool new_world_arrival{},scripted_world{};
};
static WorldStreamRuntime world_stream;

static void world_stream_reset(){world_stream_cleanup();world_stream={};}
static void* stream_manager(void* pawn,void* type){
    const auto world=api.weakget(&attached_world),level=catalog_object_field(world,"PersistentLevel");
    if(!level||!native_get_actors||!native_free)return nullptr;
    NativeActorArray values{};
    native_get_actors(pawn,type,&values);
    void* selected{};
    if(values.data&&values.count>=0&&values.count<=64&&values.capacity>=values.count){
        for(int i=0;i<values.count;++i){
            auto actor=values.data[i];
            if(!actor||!api.isa(actor,type)||AnimU::field<void*>(actor,32)!=level||actor_is_destroying(actor))continue;
            if(selected){selected=nullptr;break;}
            selected=actor;
        }
    }
    if(values.data)native_free(values.data);
    return selected;
}
static bool bind_world_stream(void* pawn,double time){
    auto& state=world_stream;
    const auto pawn_type=AnimU::field<void*>(pawn,16);
    if(api.weakget(&state.binding.manager)&&api.weakget(&state.binding.pawn_type)==pawn_type&&
       api.weakget(&state.binding.list_type)&&world_input(world_stream_restore)&&world_input(world_stream_flush)&&world_input(world_stream_busy))return true;
    if(time<state.next_bind)return false;
    state.next_bind=time+.5;
    for(unsigned i=world_stream_manager;i<=world_stream_busy;++i)if(!world_input(i)){
        scan_world_inputs(4096);
        break;
    }
    world_stream_stage=1;
    const auto type=world_input(world_stream_manager),list_type=world_input(world_stream_list);
    const auto restore=world_input(world_stream_restore);
    if(!type||!list_type||!restore||!medic_params(world_stream_restore,32,{{"StateToRestore",32,0}})||
       !medic_params(world_stream_flush,9,{{"WorldContextObject",8,0},{"bBlocking",1,8}})||
       !medic_params(world_stream_busy,9,{{"WorldContextObject",8,0},{"ReturnValue",1,8}}))return false;
    const auto payload=catalog_field(restore,"StateToRestore",32);
    if(!damage_struct_property(payload)||!damage_type_path(payload,"/Script/QueensDomain.FS_LevelLoadSuspendState")||
       (AnimU::field<std::uint64_t>(payload,64)&0x580u)!=0x80u)return false;
    constexpr auto entry_path="/Script/QueensDomain.FS_LevelListStackEntry";
    const auto stack=world_actor_array(type,"LevelListStack",16,entry_path);
    const auto trigger=world_actor_array(type,"TriggerLevelListStack",16,entry_path);
    const auto payload_type=AnimU::field<void*>(payload,0x78);
    const auto input_stack=world_actor_array(payload_type,"SafeLocLevelListStack",16,entry_path);
    const auto input_trigger=world_actor_array(payload_type,"SafeLocTriggerLevelListStack",16,entry_path);
    world_stream_stage=2;
    if(!stack.field||!trigger.field||!input_stack.field||!input_trigger.field||input_stack.offset!=0||input_trigger.offset!=16)return false;
    const auto inner=AnimU::field<void*>(stack.field,0x78);
    const auto entry=AnimU::field<void*>(inner,0x78);
    const auto list=damage_field(entry,"List",8,0,"ObjectProperty");
    const auto flag=damage_field(entry,"bTrigger",1,12,"BoolProperty");
    if(!list||AnimU::field<void*>(list,0x78)!=list_type||!flag||!damage_field(entry,"Priority",4,8,"IntProperty"))return false;
    const auto manager=stream_manager(pawn,type);
    if(!manager)return false;
    WorldStreamBinding binding{};
    binding.payload=payload;
    binding.trigger=flag;
    binding.stacks={stack.offset,trigger.offset};
    binding.teleport={catalog_field(pawn_type,"Teleporting",1),catalog_field(pawn_type,"QuickTeleporting",1),
                       catalog_field(pawn_type,"DoingTeleportCrystal",1)};
    if(std::ranges::any_of(binding.teleport,[](auto field){return !medic_field_class(field,"BoolProperty");})||
       !makeweak(binding.manager,manager)||!makeweak(binding.pawn_type,pawn_type)||!makeweak(binding.list_type,list_type))return false;
    state.binding=binding;
    world_stream_stage=3;
    return true;
}
static bool world_stream_teleporting(void* pawn){
    return std::ranges::any_of(world_stream.binding.teleport,[&](auto field){return api.getbool(field,pawn,0);});
}
static bool world_stream_idle(void* manager,void* gi){
    if(!manager||!gi)return false;
    const auto busy=world_input(world_stream_busy);
    if(!busy)return false;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,gi);
    api.event(manager,busy,args.data());
    return !args[8];
}
static bool world_stream_stable(void* pawn,void* manager,void* gi){
    return !world_stream_teleporting(pawn)&&world_stream_idle(manager,gi);
}
static bool read_world_stream(void* manager,WP::Streaming& out){
    const auto& binding=world_stream.binding;
    const auto list_type=api.weakget(&binding.list_type);
    if(!list_type)return false;
    WP::Streaming current{true,{}};
    for(unsigned i=0;i<current.stacks.size();++i){
        const auto list=AnimU::field<WorldArray>(manager,binding.stacks[i]);
        if(!list.valid(WP::max_stream_entries))return false;
        const auto entries=static_cast<const WorldStreamEntry*>(list.data);
        for(int row=0;row<list.count;++row){
            const auto& entry=entries[row];
            std::string path;
            if(!entry.list||!api.isa(entry.list,list_type)||!catalog_path(entry.list,path)||!WP::stream_path(path))return false;
            current.stacks[i].push_back({std::move(path),entry.priority});
        }
    }
    if(!WP::valid(current))return false;
    out=std::move(current);
    return true;
}
static float stream_distance(const std::array<float,3>& a,const std::array<float,3>& b){
    float squared{};
    for(unsigned axis=0;axis<3;++axis){const auto d=a[axis]-b[axis];squared+=d*d;}
    return squared;
}
static void world_stream_observe(void* pawn,double time){
    if(world_stream.scripted_world)return;
    if(!bind_world_stream(pawn,time))return;
    auto& state=world_stream;
    auto& owner=state.owner;
    if(api.weakget(&owner.pawn)!=pawn){
        owner.has_position=owner.pending=owner.teleporting=false;
        state.candidate={};
        if(!makeweak(owner.pawn,pawn))return;
    }
    const bool moving=world_stream_teleporting(pawn);
    if(rollback_busy()||local_life.locked||local_life.pending){
        owner.has_position=owner.pending=false;
        owner.teleporting=moving;
        state.candidate={};
        return;
    }
    if(moving!=owner.teleporting){
        owner.pending|=!moving;
        owner.teleporting=moving;
        state.candidate={};
    }
    if(state.new_world_arrival&&!moving){
        owner.pending=true;
        state.new_world_arrival=false;
    }
}
static WP::Arrival world_stream_host_pose(void* pawn,std::uint64_t serial){
    WP::Arrival pose{serial,native_actor_location(pawn),0};
    const auto rotation=world_input(world_control_rotation);
    alignas(16) FactoryParameters args{};
    const auto control=rotation&&*api.parms(rotation)==12&&api.weakget(&local_life.pawn)==pawn&&
                       api.weakget(&local_life.controller);
    api.event(pawn,control?rotation:getters[1].fn,args.data());
    float yaw{};
    std::memcpy(&yaw,args.data()+(control?0:getters[1].at)+sizeof(float),sizeof(yaw));
    pose.yaw=std::remainder(yaw,360.f);
    return WP::valid(pose)?pose:WP::Arrival{};
}
static void world_stream_capture(void* pawn,double time,WP::Snapshot& out){
    auto& state=world_stream;
    auto& owner=state.owner;
    if(api.weakget(&owner.pawn)==pawn&&!rollback_busy()&&!local_life.locked&&!local_life.pending){
        const auto manager=api.weakget(&state.binding.manager),gi=api.weakget(&medic_runtime.save.gi);
        WP::Streaming captured;
        if(!owner.teleporting&&world_stream_idle(manager,gi)&&read_world_stream(manager,captured)){
            const auto pose=world_stream_host_pose(pawn,owner.published.serial?owner.published.serial:1);
            if(pose.serial){
                if(owner.has_position&&stream_distance(pose.position,owner.previous)>2500.f*2500.f){
                    owner.pending=true;
                    state.candidate={};
                }
                owner.previous=pose.position;
                owner.has_position=true;
            }
            if(!state.published.known&&!owner.pending&&captured.stacks[0].empty()&&captured.stacks[1].empty())
                captured={};
            if(captured!=state.candidate){
                state.candidate=std::move(captured);
                state.candidate_since=time;
            }else if(state.candidate.known&&time-state.candidate_since>=.35&&
                     (state.candidate!=state.published||owner.pending)&&(!owner.pending||pose.serial)){
                if(state.candidate!=state.published){
                    state.published=state.candidate;
                    ++world_stream_published;
                }
                if(pose.serial&&(owner.pending||owner.published.serial)){
                    const auto serial=owner.published.serial+owner.pending;
                    owner.published=pose;
                    owner.published.serial=serial;
                    if(owner.pending)++world_arrival_published;
                    owner.pending=false;
                }
            }
        }else state.candidate={};
    }
    out.streaming=state.published;
    out.arrival=owner.published;
}
static bool apply_world_stream(void* manager,void* gi,const WP::Streaming& desired){
    const auto& binding=world_stream.binding;
    const auto list_type=api.weakget(&binding.list_type);
    const auto restore=world_input(world_stream_restore),flush=world_input(world_stream_flush);
    if(!restore||!flush||!list_type)return false;
    std::array<std::vector<WorldStreamEntry>,2> rows;
    std::array<WorldArray,2> input{};
    for(unsigned i=0;i<rows.size();++i){
        rows[i].reserve(desired.stacks[i].size());
        for(const auto& entry:desired.stacks[i]){
            const auto list=load_world_asset(entry.path,UINT32_MAX,false,nullptr);
            if(!list||!api.isa(list,list_type))return false;
            rows[i].push_back({list,entry.priority,0});
            api.setbool(binding.trigger,&rows[i].back(),i!=0,0);
        }
        const auto size=static_cast<int>(rows[i].size());
        input[i]={rows[i].data(),size,size};
    }
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto init=reinterpret_cast<void(*)(void*,void*)>(image+0x11914f0);
    const auto destroy=reinterpret_cast<void(*)(void*,void*)>(image+0x118e4c0);
    const auto copy=reinterpret_cast<void(*)(void*,void*,const void*,int)>(image+0x118e4b0);
    alignas(16) FactoryParameters args{};
    init(binding.payload,args.data());
    copy(binding.payload,args.data(),input.data(),1);
    api.event(manager,restore,args.data());
    destroy(binding.payload,args.data());
    args={};
    factory_put(args,0,gi);
    api.event(manager,flush,args.data());
    ++world_stream_restores;
    return true;
}
static bool world_arrival_release(bool committed){
    auto& hold=world_stream.hold;
    if(!hold.serial)return true;
    const auto pawn=api.weakget(&hold.pawn),movement=api.weakget(&hold.movement);
    bool restored=true;
    if(!committed&&hold.changed&&pawn&&!actor_is_destroying(pawn)){
        const auto manager=api.weakget(&hold.manager),gi=api.weakget(&hold.instance);
        WP::Streaming current;
        if(manager&&gi&&(!world_stream_idle(manager,gi)||(read_world_stream(manager,current)&&current==hold.installed)))
            restored=apply_world_stream(manager,gi,hold.original);
    }
    if(pawn&&movement&&!actor_is_destroying(pawn)&&!local_life.locked&&
       AnimU::field<std::uint8_t>(movement,hold.mode_at)==0){
        if(const auto mode=world_input(world_life_mode))api.event(movement,mode,hold.modes.data());
        else restored=false;
    }
    hold={};
    if(!committed)++world_arrival_cancelled;
    return restored;
}
static bool world_stream_cleanup(){return world_arrival_release(false);}

static bool world_arrival_start(void* pawn,void* manager,void* gi,const WP::Streaming& current,
                                std::uint64_t serial,double time){
    auto& state=world_stream;
    const auto movement=api.weakget(&local_life.movement),controller=api.weakget(&local_life.controller);
    const auto stop=world_input(world_life_stop),disable=world_input(world_life_disable);
    const auto control=world_input(world_stream_control);
    if(api.weakget(&local_life.pawn)!=pawn||!movement||!controller||!stop||!disable||!world_input(world_life_mode))return false;
    if(!control){
        scan_world_inputs(4096);
        return false;
    }
    if(!medic_params(world_stream_control,12,{{"NewRotation",12,0}})||
       factory_parameter(actor_fn,"NewLocation",12)!=0||factory_parameter(actor_fn,"NewRotation",12)!=12||
       factory_parameter(actor_fn,"bTeleport",1)!=164||factory_parameter(actor_fn,"ReturnValue",1)!=165)return false;
    const auto& layout=local_life.layout;
    WorldArrivalHold hold;
    hold.mode_at=layout.move_mode;
    hold.modes={AnimU::field<std::uint8_t>(movement,layout.move_mode),AnimU::field<std::uint8_t>(movement,layout.custom_mode)};
    if(!hold.modes[0]||!makeweak(hold.pawn,pawn)||!makeweak(hold.movement,movement)||
       !makeweak(hold.controller,controller)||!makeweak(hold.manager,manager)||!makeweak(hold.instance,gi))return false;
    api.event(movement,stop,nullptr);
    api.event(movement,disable,nullptr);
    if(AnimU::field<std::uint8_t>(movement,hold.mode_at)!=0)return false;
    hold.serial=serial;
    hold.expires=time+5.;
    hold.original=current;
    state.hold=std::move(hold);
    state.attempted=serial;
    state.failures=0;
    return true;
}
static bool world_arrival_place(void* pawn,const WP::Arrival& desired){
    const auto& hold=world_stream.hold;
    const auto movement=api.weakget(&hold.movement),controller=api.weakget(&hold.controller);
    const auto stop=world_input(world_life_stop),control=world_input(world_stream_control);
    if(!movement||!controller||!stop||!control||AnimU::field<std::uint8_t>(movement,hold.mode_at)!=0)return false;
    const std::array<float,3> rotation{0.f,desired.yaw,0.f};
    alignas(16) FactoryParameters args{};
    factory_put(args,0,desired.position);
    factory_put(args,12,rotation);
    args[164]=1;
    api.event(pawn,actor_fn,args.data());
    if(!args[165]||!(stream_distance(native_actor_location(pawn),desired.position)<=1.f))return false;
    args={};
    factory_put(args,0,rotation);
    api.event(controller,control,args.data());
    api.event(movement,stop,nullptr);
    return true;
}
static bool world_story_shop_local(void* pawn);
static void world_stream_step(void* pawn,double time){
    auto& state=world_stream;
    const auto& desired=world_progress.authoritative.streaming;
    const auto& arrival=world_progress.authoritative.arrival;
    if(host_role)return;
    if(world_story_shop_local(pawn))return;
    const auto& owner=life_peers[0];
    const bool ready=active_peer_mask&&desired.known&&world_progress_ready()&&checkpoint_medic_idle()&&!rollback_busy();
    const bool standing=ready&&!local_life.locked&&!local_life.pending&&fresh_life(owner,LifePhase::standing,time);
    if(state.hold.serial&&(!standing||state.hold.serial!=arrival.serial||time>=state.hold.expires||
       api.weakget(&state.hold.pawn)!=pawn||api.weakget(&state.hold.movement)!=api.weakget(&local_life.movement)||
       api.weakget(&state.hold.controller)!=api.weakget(&local_life.controller)||world_stream_teleporting(pawn))){
        world_arrival_release(false);
        return;
    }
    if(!ready||time<state.next_apply)return;
    state.next_apply=time+.5;
    if(!bind_world_stream(pawn,time))return;
    const auto manager=api.weakget(&state.binding.manager),gi=api.weakget(&medic_runtime.save.gi);
    if(state.hold.serial&&(manager!=api.weakget(&state.hold.manager)||gi!=api.weakget(&state.hold.instance))){
        world_arrival_release(false);
        return;
    }
    WP::Streaming current;
    if(!world_stream_stable(pawn,manager,gi)||!read_world_stream(manager,current))return;
    if(arrival.serial>state.arrived&&!state.hold.serial){
        if(!standing||arrival.serial<=state.attempted||!world_arrival_start(pawn,manager,gi,current,arrival.serial,time))return;
    }
    if(state.hold.serial){
        if(current!=desired){
            if(state.failures>=3){world_arrival_release(false);return;}
            ++state.failures;
            if(apply_world_stream(manager,gi,desired)){
                state.hold.changed=true;
                state.hold.installed=desired;
            }
            return;
        }
        if(!world_arrival_place(pawn,arrival)){world_arrival_release(false);return;}
        state.arrived=arrival.serial;
        world_arrival_release(true);
        ++world_arrival_applied;
    }
    if(current==desired){
        state.failures=0;
        if(state.verified!=world_progress.applied){state.verified=world_progress.applied;++world_stream_verified;}
        return;
    }
    state.verified={};
    if(!fresh_life(owner,LifePhase::standing,time))return;
    const auto position=native_actor_location(pawn);
    const auto distance=stream_distance(position,owner.position);
    // Destination travel owns distant arrivals. This reconciliation may only
    // replace streaming stacks when the two pawns are already together.
    if(!std::isfinite(distance)||distance>120.f*120.f)return;
    if(state.failed!=world_progress.applied){state.failed=world_progress.applied;state.failures=0;}
    if(state.failures<3){++state.failures;apply_world_stream(manager,gi,desired);}
}
