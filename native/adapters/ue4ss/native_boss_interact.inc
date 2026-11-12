namespace BI=qdcoop::boss_interact;

struct BossInteractMethod {Weak function;const unsigned char* script{};};
struct BossInteractLease {Weak component;std::uint32_t generation{};double next_request{};};
struct BossInteractPending {BI::Message message;double retry{},expires{};};
struct BossInteractPeer {
    qdcoop::Id character;
    std::uint32_t generation{};
    std::uint64_t newest{};
    std::array<BI::Message,16> history{};
};
static std::array<BossInteractMethod,Boss::descriptors.size()> boss_interact_methods;
static std::array<BossInteractLease,E::capacity> boss_interact_leases;
static std::array<BossInteractPending,4> boss_interact_pending;
static std::array<BossInteractPeer,Player::slots-1> boss_interact_peers;
static std::atomic<KillExec> boss_interact_original{};
static void* volatile* boss_interact_cell;
static std::uint64_t boss_interact_serial;
static double boss_interact_next;
static bool native_door_close_observer_needed();
static void observe_native_door_close(void* context,void* frame);

static bool boss_declares_interact(void* type){
    const auto function_type=api.weakget(&catalog_classes[catalog_function]);
    if(!function_type)return true;
    auto fn=AnimU::field<void*>(type,72);
    for(unsigned i=0;fn&&i<1024;++i,fn=AnimU::field<void*>(fn,40)){
        if(AnimU::field<void*>(fn,16)!=function_type||AnimU::field<void*>(fn,32)!=type)continue;
        std::string name;
        if(AnimU::name_string(catalog_name_pool,AnimU::field<AnimU::NameKey>(fn,24),catalog_read,name)&&name=="Interact")return true;
    }
    return false;
}

static BossInteractMethod boss_interact_method(Boss::Profile profile,void* actor){
    constexpr std::array<unsigned,6> entries{0,30574,6664,27873,48822,5813};
    const auto index=static_cast<unsigned>(profile);
    const auto& descriptor=Boss::descriptors[index];
    auto type=AnimU::field<void*>(actor,16);
    for(unsigned depth=0;type&&type!=base_class&&depth<128;++depth,type=AnimU::field<void*>(type,64)){
        std::string path;
        if(!catalog_path(type,path))return {};
        if(path!=descriptor.path){
            if(boss_declares_interact(type))return {};
            continue;
        }
        auto& method=boss_interact_methods[index];
        const auto fn=declared_script_method(type,"Interact",8,36);
        if(!fn)return {};
        if(api.weakget(&method.function)==fn&&method.script==AnimU::field<const unsigned char*>(fn,96))return method;
        const auto player=catalog_field(fn,"PlayerInteracting",8);
        const auto code=AnimU::field<const unsigned char*>(fn,96);
        if(!code||!medic_field_class(player,"ObjectProperty")||*api.offset(player)||!damage_parameter(player)||
           code[0]!=0x64||code[9]!=0x00||AnimU::field<void*>(code,10)!=player||code[18]!=0x46||code[27]!=0x1d||
           AnimU::field<unsigned>(code,28)!=entries[index]||code[32]!=0x16||code[33]!=4||code[34]!=11||code[35]!=0x53)return {};
        const auto graph=AnimU::field<void*>(code,19);
        if(!graph||AnimU::field<void*>(graph,32)!=type||
           AnimU::field<void*>(code,1)!=catalog_field(graph,"K2Node_Event_PlayerInteracting",8)||
           !makeweak(method.function,fn))return {};
        method.script=code;
        return method;
    }
    return {};
}

static bool boss_interact_ready(Boss::Profile profile,unsigned flags,const std::array<std::int32_t,3>& phase,unsigned mode){
    return mode&&((flags&Boss::dead)||(profile==Boss::Profile::queen&&phase[2]&&!(flags&Boss::attacking)));
}
static bool boss_interact_send(const BI::Message& message,unsigned mask){
    E::Packet packet{};
    return BI::encode(envelope,message,packet)&&send(packet,mask);
}
static void boss_interact_queue(unsigned index,double time){
    auto& lease=boss_interact_leases[index];
    const auto& state=native_bosses[index].applied;
    if(time<lease.next_request||!local_life_pose.generation||local_life.life.phase!=LifePhase::standing||
       !boss_interact_ready(state.profile,state.flags,state.phase,state.interaction))return;
    auto pending=std::ranges::find_if(boss_interact_pending,[&](const auto& value){
        return value.message.id==state.id&&value.message.generation==state.generation&&value.expires>time;
    });
    if(pending!=boss_interact_pending.end())return;
    pending=std::ranges::find_if(boss_interact_pending,[&](const auto& value){return !value.message.serial||value.expires<=time;});
    if(pending==boss_interact_pending.end())return;
    if(!++boss_interact_serial)++boss_interact_serial;
    lease.next_request=time+.5;
    *pending={{boss_interact_serial,state.id,state.generation,local_life_pose.generation,BI::Result::request},0.,time+3.};
    ++boss_interact_requests;
}

static void boss_interact_observe(void* context,void* frame,void* result){
    const auto original=boss_interact_original.load(std::memory_order_acquire);
    if(context&&frame&&GetCurrentThreadId()==game_thread.load()){
        observe_native_door_close(context,frame);
        const auto code=AnimU::field<const unsigned char*>(frame,32);
        const auto node=AnimU::field<void*>(frame,16);
        const auto method=std::ranges::find_if(boss_interact_methods,[&](const auto& entry){
            return entry.script&&code==entry.script+19&&api.weakget(&entry.function)==node;
        });
        if(method!=boss_interact_methods.end()&&AnimU::field<void*>(frame,24)==context){
            for(unsigned i=0;i<entities.size();++i){
                if(!entities[i].used||!ownership[i].active||native_bosses[i].profile==Boss::Profile::none||
                   api.weakget(&entities[i].actor)!=context)continue;
                const auto terminal=method->script+33;
                std::memcpy(static_cast<unsigned char*>(frame)+32,&terminal,sizeof(terminal));
                ++boss_interact_intercepted;
                const auto locals=AnimU::field<void*>(frame,40);
                const auto pawn=api.weakget(&automatic_context);
                if(locals&&pawn&&AnimU::field<void*>(locals,0)==pawn&&api.weakget(&boss_interact_leases[i].component))
                    boss_interact_queue(i,now());
                return;
            }
        }
    }
    original(context,frame,result);
}
static bool boss_interact_install(){
    if(boss_interact_cell)return true;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    constexpr unsigned char prefix[]{0x48,0x83,0xec,0x38,0x48,0x8b,0x42,0x20,0x4c,0x8b,0xd1,0x4d};
    return life_opcode_slot&&build_state.load()==1&&
        !std::memcmp(reinterpret_cast<const void*>(image+0x11b6ec0),prefix,sizeof(prefix))&&
        install_damage_slot(0x46,0x11b6ec0,&boss_interact_observe,boss_interact_original,boss_interact_cell);
}

static bool boss_interact_profile(void* component,const wchar_t* profile){
    const auto set=world_input(world_hit_profile);
    if(!set||!world_input(world_string_name)||!world_input(world_strings)||
       !medic_params(world_hit_profile,9,{{"InCollisionProfileName",8,0},{"bUpdateOverlaps",1,8}}))return false;
    const auto name=medic_name(profile);
    if(!name.index)return false;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,name);
    api.event(component,set,args.data());
    return true;
}
static bool boss_interact_release(BossInteractLease& lease){
    const auto component=api.weakget(&lease.component);
    if(component&&!boss_interact_profile(component,L"NoCollision"))return false;
    lease.component={};
    lease.generation=0;
    return true;
}

static void native_boss_interact_update(unsigned index,const NativeBossSlot& slot,void* actor){
    auto& lease=boss_interact_leases[index];
    const auto& state=slot.applied;
    if(!active_peer_mask||!enrollments[index].hit_queries||!boss_interact_ready(state.profile,state.flags,state.phase,state.interaction)){
        boss_interact_release(lease);
        return;
    }
    const auto component=api.weakget(&slot.component);
    if(lease.generation==state.generation&&component&&api.weakget(&lease.component)==component)return;
    if(!boss_interact_release(lease))return;
    const auto mode=world_input(world_component_collision),
               overlap=world_input(world_component_overlap),object_type=world_input(world_hit_object_type);
    const auto method=boss_interact_method(state.profile,actor);
    if(!component||!method.script||!mode||!overlap||!object_type||!boss_interact_install())return;
    BossInteractLease candidate{};
    if(!makeweak(candidate.component,component)||!boss_interact_profile(component,L"Interactible"))return;
    std::uint8_t disabled{},query=1,type=255;
    api.event(component,overlap,&disabled);
    api.event(component,mode,&query);
    api.event(component,object_type,&type);
    if(type!=28){boss_interact_release(candidate);return;}
    candidate.generation=state.generation;
    candidate.next_request=lease.next_request;
    lease=candidate;
    ++boss_interact_leases_taken;
}

static bool boss_interact_accept(const BI::Message& message,const LifePeer& peer,double time){
    const auto index=message.id-1;
    if(retirement_requested(index)||entities[index].generation!=message.generation||!world_progress_ready()||world_scene_busy())return false;
    auto* slot=native_boss_slot(index);
    const auto actor=api.weakget(&entities[index].actor),pawn=api.weakget(&automatic_context);
    if(!slot||!actor||!pawn||actor_is_destroying(actor)||!native_boss_bind(*slot,actor,time)||
       stream_distance(native_actor_location(actor),peer.position)>1800.f*1800.f)return false;
    const auto component=api.weakget(&slot->component),collision=world_input(world_actor_collision);
    if(!component||!collision)return false;
    std::uint8_t mode{};
    api.event(component,collision,&mode);
    unsigned flags{};
    for(unsigned i=0;i<slot->flags.size();++i)if(api.getbool(slot->flags[i],actor,0))flags|=1u<<i;
    std::array<std::int32_t,3> phase{};
    if(slot->profile==Boss::Profile::queen)phase[2]=api.getbool(slot->phase[2],actor,0);
    const auto method=boss_interact_method(slot->profile,actor);
    const auto fn=api.weakget(&method.function);
    if(!fn||!boss_interact_ready(slot->profile,flags,phase,mode))return false;
    auto argument=pawn;
    api.event(actor,fn,&argument);
    return true;
}

static bool receive_boss_interact(const ReceivedPacket& received,unsigned kind,double time){
    if(kind!=BI::kind)return false;
    BI::Message message;
    if(!active_peer_mask||!BI::decode(envelope,received.packet,message))return true;
    if(!host_role){
        if(received.peer||message.result==BI::Result::request||message.member!=local_life_pose.generation)return true;
        const auto pending=std::ranges::find_if(boss_interact_pending,[&](const auto& value){
            return value.message.serial==message.serial&&value.message.id==message.id&&value.message.generation==message.generation&&
                value.message.member==message.member&&value.expires>time;
        });
        if(pending==boss_interact_pending.end())return true;
        if(message.result==BI::Result::accepted)++boss_interact_guest_accepts;
        else ++boss_interact_guest_refusals;
        *pending={};
        return true;
    }
    if(message.result!=BI::Result::request||received.peer>=boss_interact_peers.size()||
       !(active_peer_mask&(1u<<received.peer)))return true;
    const auto& peer=life_peers[received.peer+1];
    if(peer.generation!=message.member||!fresh_life(peer,LifePhase::standing,time))return true;
    auto& history=boss_interact_peers[received.peer];
    if(history.generation!=message.member||history.character!=peer.rescue_identity){
        history={};
        history.generation=message.member;
        history.character=peer.rescue_identity;
    }
    if(message.serial<=history.newest&&history.newest-message.serial>=history.history.size())return true;
    auto& response=history.history[message.serial%history.history.size()];
    if(response.serial!=message.serial){
        response=message;
        response.result=BI::Result::refused;
        history.newest=std::max(history.newest,message.serial);
        if(boss_interact_accept(message,peer,time)){
            response.result=BI::Result::accepted;
            ++boss_interact_host_accepts;
        }else ++boss_interact_host_refusals;
    }
    if(response.id==message.id&&response.generation==message.generation)boss_interact_send(response,1u<<received.peer);
    return true;
}

static void native_boss_interact_step(double time){
    if(time<boss_interact_next)return;
    boss_interact_next=time+.1;
    if(host_role){
        for(unsigned i=0;i<boss_interact_peers.size();++i)
            if(!(active_peer_mask&(1u<<i))&&boss_interact_peers[i].generation)boss_interact_peers[i]={};
        return;
    }
    for(unsigned i=0;i<boss_interact_leases.size();++i){
        auto& lease=boss_interact_leases[i];
        if(lease.generation&&(!active_peer_mask||entities[i].generation!=lease.generation||!entities[i].used||
           retirement_requested(i)||time-entities[i].received>.5))boss_interact_release(lease);
    }
    if(boss_interact_cell&&!native_door_close_observer_needed()&&
       std::ranges::none_of(boss_interact_leases,[](const auto& lease){return lease.generation!=0;}))
        restore_damage_slot(&boss_interact_observe,boss_interact_original,boss_interact_cell);
    for(auto& pending:boss_interact_pending){
        if(!pending.message.serial)continue;
        if(!active_peer_mask||pending.expires<=time||pending.message.member!=local_life_pose.generation||
           local_life.life.phase!=LifePhase::standing)pending={};
        else if(time>=pending.retry&&boss_interact_send(pending.message,1))pending.retry=time+.2;
    }
}

static bool native_boss_interact_cleanup(){
    bool clean=true;
    for(auto& lease:boss_interact_leases)if(lease.generation&&!boss_interact_release(lease))clean=false;
    if(!clean)return false;
    return restore_damage_slot(&boss_interact_observe,boss_interact_original,boss_interact_cell);
}
static void native_boss_interact_reset(){
    native_boss_interact_cleanup();
    boss_interact_methods={};
    boss_interact_leases={};
    boss_interact_pending={};
    boss_interact_peers={};
    boss_interact_serial=0;
    boss_interact_next=0;
}
