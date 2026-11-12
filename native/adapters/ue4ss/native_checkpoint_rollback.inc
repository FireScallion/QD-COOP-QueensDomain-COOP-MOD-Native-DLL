using CP::RollbackStage;
struct NativeRollback {
    qdcoop::WipeGate gate;
    CP::Manifest plan;
    E::Envelope scope;
    RollbackStage stage{};
    std::array<std::uint64_t,Player::slots> down_tokens{},down_generations{};
    qdcoop::TransactionId checkpoint;
    MedicSave save;
    Weak world,forked_save;
    AnimU::NameKey level;
    std::wstring archive;
    std::uint64_t canonical_world{};
    unsigned slot{},ready{},acks{},arrived{};
    int base_at{-1};
    double deadline{},next_send{},launch_at{};
    ULONGLONG travel_deadline{};
    bool abort_notice{},draining{};
};
static NativeRollback rollback;
static Medic::FileHandle rollback_archive;
static std::uint64_t checkpoint_rollbacks;

static void checkpoint_rollback_reset(){
    rollback_archive.close();
    rollback={};
}
static void rollback_fail(){
    rollback.draining=(rollback.stage==RollbackStage::verifying||rollback.stage==RollbackStage::writing)&&checkpoint_worker.busy();
    rollback.abort_notice=host_role&&rollback.plan.rollback&&rollback.stage<RollbackStage::committed;
    rollback_archive.close();
    rollback.stage=RollbackStage::failed;
    rollback.deadline=now()+3.;
    ++checkpoint_errors;
}
static bool rollback_busy(){
    return rollback.draining||(rollback.stage>RollbackStage::idle&&rollback.stage<RollbackStage::arrived);
}
static bool checkpoint_wipe_ready(const CP::Roster& roster,double time){
    return checkpoint_roster_ready(roster,3,time)&&checkpoint_roster_ready(roster,4,time);
}
static bool checkpoint_can_wipe(double time){
    const auto& cp=native_checkpoint;
    if(!cp.latest.token||!cp.latest_here||!active_peer_mask||
       (host_role&&cp.latest.roster!=checkpoint_host_roster()))return false;
    const auto self=checkpoint_self();
    bool found_self=false;
    for(unsigned i=0;i<cp.latest.roster.members.size();++i){
        if(!(cp.latest.roster.mask&(1u<<i)))continue;
        const auto member=cp.latest.roster.members[i];
        if(member==self){found_self=true;continue;}
        const auto& peer=life_peers[i];
        if(member!=checkpoint_member(peer)||time-peer.received>Player::rescue_fresh||
           (peer.life.phase!=LifePhase::standing&&peer.life.phase!=LifePhase::down))return false;
    }
    return found_self;
}
static bool rollback_send(CP::Op op,unsigned peers){
    auto m=rollback.plan;
    if(rollback.slot||op==CP::Op::wipe_abort)for(unsigned i=0;i<m.saves.size();++i)
        if(i!=rollback.slot||op==CP::Op::wipe_reject||op==CP::Op::wipe_abort)m.saves[i]={};
    E::Packet packet{};
    return peers&&CP::encode(rollback.scope,{m,op,rollback.slot},packet)&&send(packet,peers);
}
static bool rollback_same(const CP::Manifest& other){
    return other.rollback==rollback.plan.rollback&&checkpoint_same(other,rollback.plan);
}
static bool rollback_context(void* pawn){
    auto& r=rollback;
    const auto gi=api.weakget(&r.save.gi),world=api.weakget(&r.world);
    const auto active=medic_active(r.save,pawn);
    if(!gi||!world||!active||!checkpoint_medic_idle()||
       !medic_params(world_cp_travel,40,{{"WorldContextObject",8,0},{"LevelName",8,8},
                                        {"bAbsolute",1,16},{"Options",16,24}}))return false;
    r.base_at=life_offset(AnimU::field<void*>(gi,16),"BaseActiveSave",8);
    if(r.base_at<0||r.base_at==r.save.active_at||AnimU::field<void*>(gi,r.base_at)!=active)return false;
    const auto loaded=medic_load_snapshot(r.save,r.archive);
    std::string name;
    if(!checkpoint_loaded(loaded,name))return false;
    const auto path=checkpoint_level_package(name,world);
    if(path.empty())return false;
    const std::wstring package(path.begin(),path.end());
    r.level=medic_name(package);
    return medic_name_is(r.level,package);
}
static bool rollback_prepare(const CP::Manifest& plan,unsigned slot,double time){
    auto& r=rollback;
    if(checkpoint_worker.busy()||reward_busy()||receipt_enabled||!checkpoint_wipe_ready(plan.roster,time)||
       !checkpoint_roster_ready(plan.roster,2,time))return false;
    r.scope=envelope;
    r.plan=plan;
    r.slot=slot;
    r.save=medic_runtime.save;
    r.world=attached_world;
    r.canonical_world=native_world_epoch(api.weakget(&r.world));
    r.archive=checkpoint_path(plan,L".sav");
    r.deadline=time+12.;
    r.next_send=0;
    r.ready=r.acks=r.arrived=0;
    rollback_archive.value=CreateFileW(r.archive.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    BY_HANDLE_FILE_INFORMATION file{};
    if(!r.canonical_world||!rollback_archive||!GetFileInformationByHandle(rollback_archive.value,&file)||
       (file.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||!checkpoint_worker.verify(r.archive)){
        rollback_fail();
        return false;
    }
    r.stage=RollbackStage::verifying;
    return true;
}
static bool rollback_write(double time){
    auto& r=rollback;
    auto record=r.plan;
    record.token=record.rollback;
    E::Packet packet{};
    if(!CP::encode(r.scope,{r.plan,CP::Op::wipe_commit,0},packet)||
       !checkpoint_worker.commit(checkpoint_path(record,L".wipe"),packet))return false;
    r.stage=RollbackStage::writing;
    r.deadline=time+15.;
    return true;
}
static bool rollback_receive(const ReceivedPacket& received,double time){
    auto& r=rollback;
    auto& cp=native_checkpoint;
    CP::Message message{};
    if(!CP::decode(envelope,received.packet,message)&&
       (!r.plan.rollback||!CP::decode(r.scope,received.packet,message)))return false;
    if(!CP::wipe_op(message.op))return false;
    const auto& m=message.manifest;
    if(host_role){
        const unsigned slot=received.peer+1;
        if(!rollback_same(m)||slot>=Player::slots||message.sender!=slot||!(active_peer_mask&(1u<<received.peer)))return true;
        if(message.op==CP::Op::wipe_arrived&&r.stage>=RollbackStage::launch&&r.stage<=RollbackStage::arrived&&
           m.saves[slot]==r.plan.saves[slot])r.arrived|=1u<<slot;
        if(m.roster.members[slot]!=checkpoint_member(life_peers[slot])||
           time-life_peers[slot].received>Player::rescue_fresh)return true;
        if(message.op==CP::Op::wipe_ready&&r.stage==RollbackStage::prepared&&m.saves[slot]==r.plan.saves[slot])
            r.ready|=1u<<slot;
        if(message.op==CP::Op::wipe_reject&&r.stage==RollbackStage::prepared)rollback_fail();
        if(message.op==CP::Op::wipe_ack&&r.stage==RollbackStage::committed&&m.saves[slot]==r.plan.saves[slot])
            r.acks|=1u<<slot;
        return true;
    }
    if(received.peer||message.sender)return true;
    if(message.op==CP::Op::wipe_prepare&&(r.stage==RollbackStage::idle||r.stage==RollbackStage::arrived||
       (r.stage==RollbackStage::failed&&time>=r.deadline&&!r.draining))&&
       cp.stage==CheckpointStage::idle){
        auto latest=m;
        latest.rollback=0;
        if(latest==cp.latest&&m.roster.members[cp.self]==checkpoint_self()){
            if(r.stage==RollbackStage::arrived)checkpoint_rollback_reset();
            rollback_prepare(m,cp.self,time);
        }
    }else if(message.op==CP::Op::wipe_abort&&rollback_same(m)&&r.stage<RollbackStage::writing){
        rollback_fail();
    }else if(rollback_same(m)&&m.saves==r.plan.saves){
        if(message.op==CP::Op::wipe_commit&&r.stage==RollbackStage::prepared&&
           checkpoint_roster_ready(m.roster,2,time)&&!rollback_write(time))rollback_fail();
        if(message.op==CP::Op::wipe_launch&&r.stage==RollbackStage::committed){
            r.stage=RollbackStage::launch;
            r.launch_at=time+.25;
        }
    }
    return true;
}
static void rollback_downed(double time){
    auto& r=rollback;
    auto& cp=native_checkpoint;
    if(!host_role||!cp.session||!cp.latest.token||cp.stage!=CheckpointStage::idle||
       cp.latest.roster!=checkpoint_host_roster()||!checkpoint_wipe_ready(cp.latest.roster,time))return;
    for(unsigned i=0;i<Player::slots;++i){
        if(!(cp.latest.roster.mask&(1u<<i)))continue;
        const auto life=i?life_peers[i].life:local_life.life;
        if(life.phase!=LifePhase::down&&r.down_tokens[i]&&
           cp.session->revive(cp.members[i],envelope.world,r.down_generations[i])==qdcoop::Result::ok)r.down_tokens[i]=0;
    }
    for(unsigned i=0;i<Player::slots;++i){
        if(!(cp.latest.roster.mask&(1u<<i)))continue;
        const auto life=i?life_peers[i].life:local_life.life;
        if(life.phase!=LifePhase::down||life.token==r.down_tokens[i])continue;
        const auto generation=++r.down_generations[i];
        if(cp.session->down(cp.members[i],envelope.world,generation)!=qdcoop::Result::ok)continue;
        r.down_tokens[i]=life.token;
        if(r.gate.on_downed(*cp.session,cp.coordinator,cp.members[i],generation)==qdcoop::Result::ok)
            r.checkpoint=r.gate.checkpoint();
    }
    if(!r.checkpoint.sequence||!checkpoint_roster_ready(cp.latest.roster,2,time))return;
    auto plan=cp.latest;
    plan.rollback=new_life_token();
    if(plan.rollback)rollback_prepare(plan,0,time);
}
static bool rollback_step(void* pawn,double time){
    auto& r=rollback;
    if(r.stage==RollbackStage::idle){rollback_downed(time);return false;}
    if(r.stage==RollbackStage::arrived&&(time>=r.deadline||(host_role&&r.arrived==r.plan.roster.mask))){
        checkpoint_rollback_reset();
        return false;
    }
    CP::JobResult result{};
    if(rollback_busy()&&checkpoint_worker.poll(result)){
        if(r.draining)r.draining=false;
        else if(r.stage==RollbackStage::verifying&&result.op==CP::Job::verify&&result.ok&&
           result.proof==r.plan.saves[r.slot]&&rollback_context(pawn)){
            r.stage=RollbackStage::prepared;
            r.ready=1;
        }else if(r.stage==RollbackStage::writing&&result.op==CP::Job::manifest&&result.ok){
            r.stage=RollbackStage::committed;
            r.acks=1;
        }else rollback_fail();
        r.next_send=0;
    }
    if(r.stage==RollbackStage::failed&&!r.draining&&time>=r.deadline){
        r.stage=RollbackStage::idle;
        return false;
    }
    if((r.stage==RollbackStage::prepared||r.stage==RollbackStage::committed)&&time>=r.deadline)rollback_fail();
    if(r.stage==RollbackStage::prepared&&host_role&&r.ready==r.plan.roster.mask&&
       checkpoint_roster_ready(r.plan.roster,2,time)&&!rollback_write(time))rollback_fail();
    if(r.stage==RollbackStage::committed&&host_role&&r.acks==r.plan.roster.mask&&
       checkpoint_roster_ready(r.plan.roster,2,time)){
        r.stage=RollbackStage::launch;
        r.launch_at=time+.8;
        r.next_send=0;
    }
    if(time>=r.next_send){
        r.next_send=time+.15;
        switch(r.stage){
        case RollbackStage::prepared:rollback_send(host_role?CP::Op::wipe_prepare:CP::Op::wipe_ready,host_role?active_peer_mask:1);break;
        case RollbackStage::committed:rollback_send(host_role?CP::Op::wipe_commit:CP::Op::wipe_ack,host_role?active_peer_mask:1);break;
        case RollbackStage::launch:if(host_role)rollback_send(CP::Op::wipe_launch,active_peer_mask);break;
        case RollbackStage::arrived:
            if(host_role&&r.arrived!=r.plan.roster.mask&&time<r.deadline)rollback_send(CP::Op::wipe_launch,active_peer_mask);
            if(!host_role&&time<r.deadline)rollback_send(CP::Op::wipe_arrived,1);
            break;
        case RollbackStage::failed:
            if(time<r.deadline&&r.abort_notice)rollback_send(CP::Op::wipe_abort,active_peer_mask);
            if(time<r.deadline&&!host_role)rollback_send(CP::Op::wipe_reject,1);
            break;
        default:break;
        }
    }
    if(r.stage==RollbackStage::launch&&time>=r.launch_at){
        if(!(checkpoint_flags()&2)||!rollback_archive){rollback_fail();return false;}
        r.stage=RollbackStage::cleaning;
        r.travel_deadline=GetTickCount64()+30000;
        return true;
    }
    return false;
}

static NativeWorld::Phase checkpoint_world_transition(const LocalWorld& local,ULONGLONG time){
    using Phase=NativeWorld::Phase;
    auto& r=rollback;
    if(r.stage==RollbackStage::cleaning){
        if(time>=r.travel_deadline){rollback_fail();return Phase::failed;}
        if(!cleanup_native_actors())return Phase::suspended;
        const auto gi=api.weakget(&r.save.gi);
        const auto gs=world_input(static_cast<unsigned>(world_object_count)+factory_gameplay);
        const auto open=world_input(world_cp_travel);
        const auto restored=medic_load_snapshot(r.save,r.archive);
        std::string level;
        const auto previous=r.plan.roster.members[r.slot].character;
        const qdcoop::Id branch{previous.high^std::rotl(r.plan.rollback,17),previous.low^r.plan.rollback};
        if(!gi||gi!=local.instance||!gs||!open||!checkpoint_loaded(restored,level)||
           !medic_fork_save(r.save,restored,branch)||!makeweak(r.forked_save,restored)){
            rollback_fail();
            return Phase::failed;
        }
        auto set=[&](int offset){std::memcpy(static_cast<unsigned char*>(gi)+offset,&restored,sizeof(restored));};
        set(r.save.active_at);
        set(r.base_at);
        rollback_archive.close();
        alignas(16) FactoryParameters args{};
        factory_put(args,0,gi);
        factory_put(args,8,r.level);
        args[16]=1;
        r.stage=RollbackStage::loading;
        r.travel_deadline=time+30000;
        api.event(gs,open,args.data());
        return Phase::suspended;
    }
    if(r.stage!=RollbackStage::loading)return Phase::idle;
    if(time>=r.travel_deadline){rollback_fail();return Phase::failed;}
    if(!local.pawn||local.world==api.weakget(&r.world))return Phase::suspended;
    const auto restored=api.weakget(&r.forked_save);
    if(!restored||local.instance!=api.weakget(&r.save.gi)||native_world_epoch(local.world)!=r.canonical_world||
       AnimU::field<void*>(local.instance,r.save.active_at)!=restored||
       AnimU::field<void*>(local.instance,r.base_at)!=restored){rollback_fail();return Phase::failed;}
    reset_native_gameplay_scope();
    checkpoint_world_nonce=r.plan.rollback;
    if(host_role)r.gate.restored(r.checkpoint);
    r.stage=RollbackStage::arrived;
    r.arrived|=1u<<r.slot;
    r.deadline=now()+20.;
    r.next_send=0;
    ++checkpoint_rollbacks;
    return Phase::suspended;
}
