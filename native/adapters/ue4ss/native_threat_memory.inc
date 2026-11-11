struct ThreatMemoryBinding {Weak chase,seen;};
static ThreatMemoryBinding threat_memory_binding;
static std::atomic<KillExec> threat_memory_original{};
static std::atomic<const unsigned char*> threat_chase_site{},threat_seen_site{};
static void* volatile* threat_memory_cell;
static double threat_next_memory_bind;
static unsigned threat_memory_live;
static bool threat_memory_restore_conflict;

static void threat_memory_observe(void* context,void* frame,void* result){
    const auto original=threat_memory_original.load(std::memory_order_acquire);
    const auto code=frame?AnimU::field<const unsigned char*>(frame,32):nullptr;
    if(!code||!result){original(context,frame,result);return;}
    const bool chase=code==threat_chase_site.load(std::memory_order_relaxed);
    if((chase||code==threat_seen_site.load(std::memory_order_relaxed))&&GetCurrentThreadId()==game_thread.load()&&
       host_role&&automatic_limit&&active_peer_mask&&threat_memory_cell){
        const auto fn=api.weakget(chase?&threat_memory_binding.chase:&threat_memory_binding.seen);
        if(fn&&AnimU::field<void*>(frame,16)==fn){
            for(unsigned i=0;i<automatic_limit;++i){
                auto& state=threat_enemies[i];
                if(state.memory_slot>=Player::slots||state.choice.target!=state.memory_slot||state.memory_generation!=entities[i].generation||
                   api.weakget(&state.actor)!=context||deaths[i].terminal||enrollments[i].retiring)continue;
                const auto marker=api.weakget(&state.memory);
                if(!marker)break;
                original(context,frame,result);
                std::memcpy(result,&marker,sizeof(marker));
                ++threat_memory_used;
                return;
            }
        }
    }
    original(context,frame,result);
}
static bool bind_threat_memory(double time){
    if(threat_memory_cell&&api.weakget(&threat_memory_binding.chase)&&api.weakget(&threat_memory_binding.seen))return true;
    if(time<threat_next_memory_bind)return false;
    threat_next_memory_bind=time+.25;
    if(!world_input(world_threat_chase)||!world_input(world_threat_last_seen))scan_world_inputs(4096);
    const auto chase=world_input(world_threat_chase),seen=world_input(world_threat_last_seen);
    const auto target=catalog_field(base_class,"TargetPlayer",8);
    const auto out=chase?catalog_field(chase,"TargetPlayer",8):nullptr;
    threat_memory_stage=1;
    if(!chase||!seen||!target||!out||*api.parms(chase)!=8||*api.parms(seen)!=12||
       factory_parameter(chase,"TargetPlayer",8)!=0||factory_parameter(seen,"Loc",12)!=0)return false;
    threat_memory_stage=2;
    if(AnimU::field<int>(chase,104)!=22)return false;
    const auto chase_code=AnimU::field<const unsigned char*>(chase,96),seen_code=AnimU::field<const unsigned char*>(seen,96);
    const auto seen_size=AnimU::field<int>(seen,104);
    if(!chase_code||!seen_code||seen_size!=80||chase_code[0]!=0x5f||chase_code[1]!=0x48||
       AnimU::field<void*>(chase_code,2)!=out||chase_code[10]!=0x01||AnimU::field<void*>(chase_code,11)!=target||
       chase_code[19]!=0x04||chase_code[20]!=0x0b||chase_code[21]!=0x53||
       seen_code[18]!=0x19||seen_code[19]!=0x01||AnimU::field<void*>(seen_code,20)!=target||
       seen_code[77]!=0x04||seen_code[78]!=0x0b||seen_code[79]!=0x53)return false;
    threat_memory_stage=3;
    ThreatMemoryBinding binding{};
    if(!makeweak(binding.chase,chase)||!makeweak(binding.seen,seen)||
       !install_damage_slot(0x01,0x11b5e60,&threat_memory_observe,threat_memory_original,threat_memory_cell))return false;
    threat_memory_binding=binding;
    threat_memory_stage=4;
    threat_chase_site.store(chase_code+11,std::memory_order_relaxed);
    threat_seen_site.store(seen_code+20,std::memory_order_relaxed);
    threat_next_memory_bind=0;
    return true;
}
static void remove_threat_memory_hook(){
    threat_chase_site.store(nullptr,std::memory_order_relaxed);
    threat_seen_site.store(nullptr,std::memory_order_relaxed);
    if(!restore_damage_slot(&threat_memory_observe,threat_memory_original,threat_memory_cell))threat_memory_restore_conflict=true;
}
static void idle_threat_memory(){
    if(!threat_memory_live&&threat_memory_cell)remove_threat_memory_hook();
}
static bool threat_memory_clear(NativeThreatEnemy& state){
    const auto marker=api.weakget(&state.memory);
    const bool counted=state.memory_generation!=0;
    if(!marker&&!counted){state.memory={};return true;}
    state.memory_slot=Threat::none;
    if(!destroy_owned_actor(state.memory,false))return false;
    state.memory_generation=0;
    if(counted){--threat_memory_live;++threat_memory_retired;}
    idle_threat_memory();
    return true;
}
static bool threat_memory_step(void* actor,NativeThreatEnemy& state,unsigned slot,double time){
    if(state.memory_slot==slot&&state.memory_generation==state.generation&&api.weakget(&state.memory))return true;
    if(!threat_memory_clear(state))return false;
    const auto type=factory_object(factory_visual_type),collision=factory_object(factory_collision);
    const auto tick=world_input(world_actor_tick);
    if(!type||!collision||!tick||!bind_threat_memory(time))return false;
    const auto marker=factory_spawn(actor,type,state.last_visible[slot],state.memory,true);
    if(!marker){threat_memory_clear(state);idle_threat_memory();return false;}
    own_set(marker,collision,false);
    own_set(marker,tick,false);
    state.memory_slot=slot;
    state.memory_generation=state.generation;
    ++threat_memory_live;
    ++threat_memory_created;
    threat_memory_stage=5;
    return true;
}
static bool threat_memory_detach(){
    remove_threat_memory_hook();
    threat_memory_binding={};
    threat_next_memory_bind=0;
    const bool clean=!threat_memory_live&&!threat_memory_restore_conflict;
    threat_memory_restore_conflict=false;
    return clean;
}
