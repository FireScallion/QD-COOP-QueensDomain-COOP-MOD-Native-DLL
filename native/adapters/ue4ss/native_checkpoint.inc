namespace CP=qdcoop::checkpoint;
enum class CheckpointStage { idle, capture, verify, ready, manifest, settle, rejected };
struct CheckpointSave {
    Weak gi,pawn;
    void* script{};
    const unsigned char* code{};
    int suspend{},save_to_write{};
    std::array<void*,3> slot_properties{};
    std::array<void*,3> travel_flags{};
    void* async_flag{};
    int level{},location{};
    void* stock{};
    std::wstring backup;
};
struct NativeCheckpoint {
    CheckpointSave save;
    std::optional<qdcoop::Session> session;
    qdcoop::Checkpoints coordinator;
    std::array<qdcoop::MemberRef,Player::slots> members{};
    CP::Manifest pending,latest,abort_notice;
    CP::SaveProof archive_proof,manifest_proof;
    CheckpointStage stage{};
    std::array<std::uint64_t,Player::slots> requests{};
    std::wstring archive;
    std::uint64_t serial{},last_prepare{},stock{},seen{},captured{};
    unsigned self{},commit_acks{},availability{},failure{};
    double next_bind{},next_send{},deadline{},retry{};
    bool internal_save{},wanted{},pending_here{},latest_here{};
};
static NativeCheckpoint native_checkpoint;
static CP::Worker checkpoint_worker;
static std::atomic<KillExec> checkpoint_original_save{};
static void* volatile* checkpoint_save_cell;
static bool rollback_receive(const ReceivedPacket& received,double time);
static bool rollback_step(void* pawn,double time);
static bool rollback_busy();
static void* guest_safe_save(void* pawn,void* active,void* outgoing);

static bool checkpoint_medic_idle(){
    const auto& med=medic_runtime;
    return med.phase==MedicPhase::idle&&med.ledger.outgoing.phase==Medic::Debit::none&&
           std::ranges::none_of(med.ledger.incoming,[](const auto& entry){return entry.phase!=Medic::Receipt::none;});
}
static bool checkpoint_save_name(const MedicString& value,const std::wstring& primary){
    constexpr std::wstring_view prefix=L"FULLPATH:";
    if(!value.data||value.size<1||value.capacity<value.size||
       static_cast<std::size_t>(value.size)!=prefix.size()+primary.size()+1||value.data[value.size-1]!=0||
       std::wstring_view(value.data,prefix.size())!=prefix)return false;
    auto path=std::wstring(value.data+prefix.size(),primary.size());
    std::replace(path.begin(),path.end(),L'/',L'\\');
    return _wcsicmp(path.c_str(),primary.c_str())==0;
}
static void observe_checkpoint_save(void* context,void* frame,void* result){
    const auto original=checkpoint_original_save.load(std::memory_order_acquire);
    auto& cp=native_checkpoint;
    const auto& binding=cp.save;
    bool primary=false;
    void** argument{};
    void* outgoing{};
    void* disk_copy{};
    if(frame&&result&&GetCurrentThreadId()==game_thread.load()&&checkpoint_save_cell&&
       AnimU::field<void*>(frame,16)==binding.script&&AnimU::field<void*>(frame,24)==api.weakget(&binding.gi)){
        const auto code=AnimU::field<const unsigned char*>(frame,32);
        const auto locals=AnimU::field<void*>(frame,40);
        if(locals&&code&&code>=binding.code+9&&code<=binding.code+2387-24&&code[-9]==0x68&&
           AnimU::field<void*>(code-8,0)==binding.stock&&code[0]==0x00&&code[9]==0x00&&code[18]==0x1d&&
           !AnimU::field<std::uint8_t>(locals,binding.suspend)){
            const auto variable=AnimU::field<void*>(code,1),slot=AnimU::field<void*>(code,10);
            const auto pawn=api.weakget(&binding.pawn);
            const auto active=medic_active(medic_runtime.save,pawn);
            if(active&&variable&&*api.offset(variable)==binding.save_to_write&&
               std::ranges::find(binding.slot_properties,slot)!=binding.slot_properties.end()){
                argument=reinterpret_cast<void**>(static_cast<unsigned char*>(locals)+binding.save_to_write);
                outgoing=*argument;
                if(outgoing&&AnimU::field<void*>(outgoing,16)==AnimU::field<void*>(active,16)){
                    const auto name=AnimU::field<MedicString>(locals,*api.offset(slot));
                    primary=checkpoint_save_name(name,medic_runtime.save.primary);
                    if(!host_role&&active_peer_mask&&(primary||checkpoint_save_name(name,binding.backup)))
                        disk_copy=guest_safe_save(pawn,active,outgoing);
                }
            }
        }
    }
    if(disk_copy)*argument=disk_copy;
    original(context,frame,result);
    if(disk_copy)*argument=outgoing;
    if(primary&&*static_cast<std::uint8_t*>(result)==1){
        ++checkpoint_stock_writes;
        if(!cp.internal_save)++cp.stock;
        if(disk_copy)++guest_safe_save_writes;
    }
}
static bool checkpoint_install_save(const CheckpointSave& binding){
    if(checkpoint_save_cell)return true;
    if(!life_opcode_slot||build_state.load()!=1)return false;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto entry=reinterpret_cast<void*>(image+0x2dc30f0);
    constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x20,0x57,0x48,0x83,0xec,0x30};
    auto cell=reinterpret_cast<void* volatile*>(static_cast<unsigned char*>(binding.stock)+216);
    MEMORY_BASIC_INFORMATION data{};
    const auto address=reinterpret_cast<std::uintptr_t>(cell);
    if(AnimU::field<void*>(binding.stock,216)!=entry||std::memcmp(entry,prefix,sizeof(prefix))||
       !VirtualQuery(reinterpret_cast<const void*>(address),&data,sizeof(data))||data.State!=MEM_COMMIT||
       !(data.Protect&PAGE_READWRITE)||(data.Protect&PAGE_GUARD)||
       reinterpret_cast<std::uintptr_t>(data.BaseAddress)+data.RegionSize-address<sizeof(void*))return false;
    checkpoint_original_save.store(reinterpret_cast<KillExec>(entry),std::memory_order_release);
    if(InterlockedCompareExchangePointer(cell,reinterpret_cast<void*>(&observe_checkpoint_save),entry)!=entry)return false;
    checkpoint_save_cell=cell;
    return true;
}
static bool checkpoint_unhook_save(){
    if(!checkpoint_save_cell)return true;
    const auto entry=reinterpret_cast<void*>(checkpoint_original_save.load(std::memory_order_acquire));
    const auto hook=reinterpret_cast<void*>(&observe_checkpoint_save);
    const auto previous=InterlockedCompareExchangePointer(checkpoint_save_cell,entry,hook);
    checkpoint_save_cell=nullptr;
    return previous==entry||previous==hook;
}
static bool checkpoint_cleanup(){
    const bool restored=checkpoint_unhook_save();
    CP::JobResult result{};
    if(checkpoint_worker.busy()&&!checkpoint_worker.poll(result))return false;
    return restored;
}
static void checkpoint_reset(){native_checkpoint={};}

static bool checkpoint_bind(void* pawn,double time){
    auto& cp=native_checkpoint;
    const auto& med=medic_runtime.save;
    const auto gi=api.weakget(&med.gi);
    const auto save=medic_active(med,pawn);
    if(cp.save.script&&api.weakget(&cp.save.gi)==gi&&api.weakget(&cp.save.pawn)==pawn&&save)return true;
    if(!save||!medic_identity().valid()||time<cp.next_bind)return false;
    cp.next_bind=time+.25;
    for(unsigned i=world_cp_first;i<world_progress_remove;++i)if(!world_input(i)){
        scan_world_inputs(2048);
        break;
    }
    if(!medic_params(world_cp_save,0,{})||!medic_params(world_cp_status,0,{})||
       !medic_params(world_cp_save_bp,2,{{"Suspend",1,0},{"SuspendIsSync",1,1}}))return false;
    const auto script=world_input(world_cp_save_bp);
    const auto code=AnimU::field<const unsigned char*>(script,96);
    if(!code||AnimU::field<int>(script,104)!=2387)return false;
    CheckpointSave binding{};
    if(!makeweak(binding.gi,gi)||!makeweak(binding.pawn,pawn))return false;
    binding.script=script;
    binding.code=code;
    binding.stock=world_input(world_medic_write);
    binding.save_to_write=life_offset(script,"SaveToWrite",8);
    binding.slot_properties={catalog_field(script,"CallFunc_GetSaveSlotName_ReturnValue",16),
        catalog_field(script,"CallFunc_GetSaveSlotName_ReturnValue_1",16),
        catalog_field(script,"CallFunc_GetSaveSlotName_ReturnValue_2",16)};
    const auto type=AnimU::field<void*>(pawn,16);
    binding.travel_flags={catalog_field(type,"Teleporting",1),catalog_field(type,"QuickTeleporting",1),
                          catalog_field(type,"DoingTeleportCrystal",1)};
    binding.async_flag=catalog_field(AnimU::field<void*>(gi,16),"SuspendSaveCurrentlyWriting",1);
    const auto save_type=AnimU::field<void*>(save,16);
    binding.level=life_offset(save_type,"SaveLevel",8);
    binding.location=life_offset(save_type,"SaveLocation",48);
    binding.backup=med.primary.substr(0,med.primary.size()-4)+L"_backup.sav";
    if(!binding.stock||binding.save_to_write<0||
       std::ranges::any_of(binding.slot_properties,[](const auto item){return !item;})||
       std::ranges::any_of(binding.travel_flags,[](const auto item){return !item;})||!binding.async_flag||
       binding.level<0||binding.location<0||!checkpoint_install_save(binding))return false;
    cp.save=std::move(binding);
    return true;
}
static unsigned checkpoint_flags(){return native_checkpoint.availability;}
static unsigned checkpoint_availability(void* pawn){
    const auto& binding=native_checkpoint.save;
    const auto gi=api.weakget(&binding.gi);
    if(!binding.script||!gi||!world_progress_ready()||!checkpoint_medic_idle()||
       native_placed_item_custody_busy()||!medic_active(medic_runtime.save,pawn)||
       api.getbool(binding.async_flag,gi,0)||
       std::ranges::any_of(binding.travel_flags,[&](auto field){return api.getbool(field,pawn,0);}))return 0;
    if(local_life.life.phase==LifePhase::standing)return 1;
    if(local_life.life.phase==LifePhase::down)return 2;
    return 0;
}
static bool checkpoint_loaded(void* object,std::string& name){
    const auto& binding=native_checkpoint.save;
    if(!object)return false;
    const auto level=AnimU::field<AnimU::NameKey>(object,binding.level);
    const auto xyz=AnimU::field<std::array<float,3>>(object,binding.location+16);
    return AnimU::name_string(catalog_name_pool,level,catalog_read,name)&&!name.empty()&&name!="None"&&
           std::ranges::all_of(name,[](char c){
               return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='/'||c=='-'||c=='.';
           })&&std::ranges::all_of(xyz,[](float f){return std::isfinite(f);});
}
static std::string checkpoint_level_package(std::string_view level,void* world){
    std::string path;
    if(!catalog_path(world,path)||!path.starts_with("/Game/"))return {};
    const auto dot=path.rfind('.');
    if(dot==std::string::npos||dot<=path.rfind('/'))return {};
    const auto package=path.substr(0,dot);
    return level==path.substr(dot+1)||level==package||level==path?package:std::string{};
}
static std::wstring checkpoint_path(const CP::Manifest& manifest,const wchar_t* suffix){
    return medic_runtime.save.primary+L".qdc-"+Medic::rescue_suffix({envelope.room_high,envelope.room_low})+
           L"-"+Medic::rescue_suffix({envelope.world,manifest.token})+suffix;
}
static CP::Participant checkpoint_member(const LifePeer& peer){
    return {peer.rescue_identity,peer.rescue_account,peer.generation};
}
static CP::Participant checkpoint_self(){
    return {medic_identity(),medic_account(),local_life_pose.generation};
}
static bool checkpoint_roster_ready(const CP::Roster& roster,unsigned flag,double time){
    if(!roster.valid())return false;
    for(unsigned slot=0;slot<roster.members.size();++slot){
        if(!(roster.mask&(1u<<slot)))continue;
        const auto& member=roster.members[slot];
        if(member==checkpoint_self()){
            if(!(checkpoint_flags()&flag))return false;
            continue;
        }
        const auto& peer=life_peers[slot];
        if(member!=checkpoint_member(peer)||time-peer.received>Player::rescue_fresh||
           !(peer.checkpoint_flags&flag))return false;
    }
    return true;
}
static CP::Roster checkpoint_host_roster(){
    CP::Roster roster{};
    roster.mask=1|(active_peer_mask<<1);
    roster.members[0]=checkpoint_self();
    for(unsigned slot=1;slot<roster.members.size();++slot)
        if(roster.mask&(1u<<slot))roster.members[slot]=checkpoint_member(life_peers[slot]);
    return roster;
}
static bool checkpoint_same(const CP::Manifest& a,const CP::Manifest& b){
    return a.token==b.token&&a.serial==b.serial&&a.roster==b.roster&&a.refresh==b.refresh;
}
static bool checkpoint_send(CP::Op op,CP::Manifest manifest,unsigned slot,unsigned peers){
    if(op!=CP::Op::commit)
        for(unsigned i=0;i<manifest.saves.size();++i)
            if(i!=slot||(op!=CP::Op::ready&&op!=CP::Op::committed))manifest.saves[i]={};
    E::Packet packet{};
    return peers&&CP::encode(envelope,{manifest,op,slot},packet)&&send(packet,peers);
}
static qdcoop::DurableReceipt checkpoint_receipt(const CP::SaveProof& proof){
    qdcoop::Id digest{};
    std::memcpy(&digest,proof.sha256.data(),sizeof(digest));
    return {digest,proof.bytes};
}
static void checkpoint_abort(double time,bool error){
    auto& cp=native_checkpoint;
    if(error)++checkpoint_errors;
    if(cp.stage<CheckpointStage::manifest&&!checkpoint_worker.busy()&&!cp.archive.empty()){
        DeleteFileW(cp.archive.c_str());
        cp.archive.clear();
    }
    if(host_role){
        if(cp.session)cp.coordinator.abort(cp.coordinator.pending().id);
        cp.wanted=cp.wanted||cp.pending.refresh;
        cp.abort_notice=cp.pending;
        cp.stage=CheckpointStage::idle;
        cp.retry=time+1.;
        cp.next_send=0;
    }else{
        cp.stage=CheckpointStage::rejected;
        cp.next_send=0;
    }
}
static bool checkpoint_capture(void* pawn){
    auto& cp=native_checkpoint;
    cp.archive.clear();
    cp.failure=1;
    if(!checkpoint_flags()||checkpoint_worker.busy()||!medic_active(medic_runtime.save,pawn))return false;
    if(cp.pending.refresh&&cp.stock==cp.seen){
        const auto gi=api.weakget(&cp.save.gi),status=world_input(world_cp_status),save=world_input(world_cp_save);
        if(!gi||!status||!save||!checkpoint_save_cell)return false;
        const auto before=checkpoint_stock_writes;
        cp.internal_save=true;
        api.event(pawn,status,nullptr);
        api.event(gi,save,nullptr);
        cp.internal_save=false;
        cp.failure=2;
        if(checkpoint_stock_writes==before)return false;
    }
    cp.captured=cp.stock;
    cp.archive=checkpoint_path(cp.pending,L".sav");
    cp.archive_proof={};
    cp.manifest_proof={};
    cp.pending_here=false;
    cp.failure=3;
    const auto& binding=medic_runtime.save;
    if(checkpoint_world_nonce&&!cp.latest.token&&!cp.pending.refresh){
        if(GetFileAttributesW(cp.archive.c_str())!=INVALID_FILE_ATTRIBUTES||
           !medic_save_snapshot(binding,pawn,cp.archive)||!checkpoint_worker.verify(cp.archive))return false;
    }else if(!checkpoint_worker.capture(binding.primary,cp.archive))return false;
    cp.stage=CheckpointStage::capture;
    return true;
}
static bool checkpoint_prepare_session(const CP::Roster& roster){
    auto& cp=native_checkpoint;
    if(cp.session&&cp.latest.token&&cp.latest.roster==roster)return true;
    cp.coordinator={};
    cp.session.emplace(qdcoop::Id{envelope.room_high,envelope.room_low},roster.members[0].character);
    cp.members={};
    cp.members[0]=cp.session->host();
    if(cp.session->travel(envelope.world)!=qdcoop::Result::ok&&cp.session->world()!=envelope.world)return false;
    for(unsigned i=0;i<roster.members.size();++i){
        if(!(roster.mask&(1u<<i)))continue;
        if(i){
            const auto admitted=cp.session->admit_authenticated(roster.members[i].character,qdcoop::protocol_version);
            if(admitted.result!=qdcoop::Result::ok)return false;
            cp.members[i]=admitted.ref;
        }
        if(cp.session->ready(cp.members[i],envelope.world)!=qdcoop::Result::ok)return false;
    }
    cp.latest={};
    cp.latest_here=false;
    return true;
}
static bool checkpoint_manifest(){
    auto& cp=native_checkpoint;
    E::Packet packet{};
    if(!CP::encode(envelope,{cp.pending,CP::Op::commit,0},packet)||
       !checkpoint_worker.commit(checkpoint_path(cp.pending,L".commit"),packet))return false;
    cp.stage=CheckpointStage::manifest;
    return true;
}
static void checkpoint_io(void* pawn,double time,const CP::JobResult& result){
    auto& cp=native_checkpoint;
    cp.failure=4;
    if(!result.ok||!medic_active(medic_runtime.save,pawn)){checkpoint_abort(time,true);return;}
    if(cp.stage==CheckpointStage::capture&&(result.op==CP::Job::capture||result.op==CP::Job::verify)){
        cp.failure=5;
        const auto loaded=medic_load_snapshot(medic_runtime.save,cp.archive);
        if(!loaded){checkpoint_abort(time,true);return;}
        cp.failure=6;
        std::string level;
        if(!checkpoint_loaded(loaded,level)||!checkpoint_worker.verify(cp.archive)){checkpoint_abort(time,true);return;}
        cp.pending_here=!checkpoint_level_package(level,api.weakget(&attached_world)).empty();
        cp.archive_proof=result.proof;
        cp.stage=CheckpointStage::verify;
    }else if(cp.stage==CheckpointStage::verify&&result.op==CP::Job::verify){
        cp.failure=7;
        if(result.proof!=cp.archive_proof){checkpoint_abort(time,true);return;}
        ++checkpoint_archives;
        cp.pending.saves[cp.self]=result.proof;
        if(host_role&&cp.coordinator.saved(*cp.session,cp.coordinator.pending().id,cp.members[0],
                                           checkpoint_receipt(result.proof))!=qdcoop::Result::ok){
            checkpoint_abort(time,true);
            return;
        }
        cp.stage=CheckpointStage::ready;
        cp.next_send=0;
        cp.failure=0;
    }else if(cp.stage==CheckpointStage::manifest&&result.op==CP::Job::manifest){
        cp.manifest_proof=result.proof;
        if(host_role){cp.stage=CheckpointStage::settle;cp.commit_acks=1;cp.deadline=time+15.;}
        else{
            cp.latest=cp.pending;
            cp.latest_here=cp.pending_here;
            cp.seen=cp.captured;
            cp.stage=CheckpointStage::idle;
            ++checkpoint_commits;
            checkpoint_send(CP::Op::committed,cp.latest,cp.self,1);
        }
        cp.next_send=0;
        cp.failure=0;
    }else checkpoint_abort(time,true);
}
static void receive_checkpoint_host(const CP::Message& message,unsigned slot,double time){
    auto& cp=native_checkpoint;
    const auto& m=message.manifest;
    if(!slot||slot>=Player::slots||!(active_peer_mask&(1u<<(slot-1)))||message.sender!=slot||
       m.roster.members[slot]!=checkpoint_member(life_peers[slot])||
       time-life_peers[slot].received>Player::rescue_fresh)return;
    if(message.op==CP::Op::request){
        if(m.roster==checkpoint_host_roster()&&m.serial>cp.requests[slot]){
            cp.requests[slot]=m.serial;
            cp.wanted=true;
        }
        return;
    }
    if(!checkpoint_same(m,cp.pending))return;
    if(message.op==CP::Op::reject&&(cp.stage==CheckpointStage::ready||cp.stage==CheckpointStage::verify||
                                  cp.stage==CheckpointStage::capture)){
        cp.deadline=0;
    }else if(message.op==CP::Op::ready&&cp.stage==CheckpointStage::ready&&cp.session){
        const auto status=cp.coordinator.saved(*cp.session,cp.coordinator.pending().id,cp.members[slot],
                                               checkpoint_receipt(m.saves[slot]));
        if(status==qdcoop::Result::ok){
            cp.pending.saves[slot]=m.saves[slot];
            ++checkpoint_ready_acks;
        }
    }else if(message.op==CP::Op::committed&&cp.stage==CheckpointStage::settle&&m.saves[slot]==cp.pending.saves[slot]){
        cp.commit_acks|=1u<<slot;
    }
}
static void receive_checkpoint_guest(const CP::Message& message,double time){
    auto& cp=native_checkpoint;
    const auto& m=message.manifest;
    if(message.sender||!checkpoint_save_cell||!medic_identity().valid())return;
    if(message.op==CP::Op::abort){
        if(checkpoint_same(m,cp.pending)&&cp.stage!=CheckpointStage::idle&&
           cp.stage!=CheckpointStage::manifest&&!checkpoint_worker.busy()){
            checkpoint_abort(time,false);
            cp.stage=CheckpointStage::idle;
        }
        return;
    }
    if(message.op==CP::Op::commit){
        if(cp.latest==m){checkpoint_send(CP::Op::committed,m,cp.self,1);return;}
        if(cp.stage!=CheckpointStage::ready||!checkpoint_same(m,cp.pending)||m.saves[cp.self]!=cp.archive_proof)return;
        cp.pending=m;
        if(!checkpoint_manifest())checkpoint_abort(time,true);
        return;
    }
    if(message.op!=CP::Op::prepare||m.serial<=cp.last_prepare||checkpoint_worker.busy()||
       !checkpoint_roster_ready(m.roster,1,time))return;
    const auto own=checkpoint_self();
    for(unsigned slot=1;slot<m.roster.members.size();++slot)if(m.roster.members[slot]==own){
        cp.self=slot;
        cp.pending=m;
        cp.last_prepare=m.serial;
        cp.deadline=time+17.;
        if(!checkpoint_capture(local_life.actor))checkpoint_abort(time,true);
        return;
    }
}
static void receive_checkpoint(const ReceivedPacket& received,double time){
    if(rollback_receive(received,time))return;
    CP::Message message{};
    if(!automatic_limit||!CP::decode(envelope,received.packet,message))return;
    if(host_role)receive_checkpoint_host(message,received.peer+1,time);
    else if(!received.peer)receive_checkpoint_guest(message,time);
}

static void checkpoint_start(void* pawn,double time){
    auto& cp=native_checkpoint;
    const auto roster=checkpoint_host_roster();
    if(time<cp.retry||!checkpoint_roster_ready(roster,1,time)||cp.serial==UINT64_MAX)return;
    const auto refresh=cp.stock!=cp.seen||cp.wanted;
    if(!checkpoint_prepare_session(roster)||
       cp.coordinator.begin(*cp.session,cp.members[0],cp.serial+1)!=qdcoop::Result::ok)return;
    cp.pending={new_life_token(),++cp.serial,roster,refresh,{}};
    cp.self=0;
    cp.wanted=false;
    cp.deadline=time+15.;
    cp.abort_notice={};
    cp.next_send=0;
    if(!cp.pending.token||!checkpoint_capture(pawn))checkpoint_abort(time,true);
}
static bool checkpoint_step(void* pawn,double time){
    auto& cp=native_checkpoint;
    cp.availability=0;
    if(checkpoint_bind(pawn,time))cp.availability=checkpoint_availability(pawn);
    if(cp.availability&&cp.latest_here)cp.availability|=4;
    if(rollback_step(pawn,time))return true;
    if(rollback_busy()||!cp.save.script)return false;
    CP::JobResult result{};
    if(checkpoint_worker.poll(result))checkpoint_io(pawn,time,result);
    if(checkpoint_worker.busy())return false;
    if(host_role){
        if(cp.stage==CheckpointStage::ready){
            if(time>=cp.deadline||checkpoint_host_roster()!=cp.pending.roster){
                checkpoint_abort(time,false);
            }else if(cp.coordinator.pending().phase==qdcoop::CheckpointPhase::prepared&&
                     checkpoint_roster_ready(cp.pending.roster,1,time)){
                if(!checkpoint_manifest())checkpoint_abort(time,true);
            }else if(time>=cp.next_send){
                cp.next_send=time+.2;
                checkpoint_send(CP::Op::prepare,cp.pending,0,active_peer_mask);
            }
        }else if(cp.stage==CheckpointStage::settle){
            if(cp.commit_acks==cp.pending.roster.mask){
                if(cp.coordinator.commit(*cp.session,cp.coordinator.pending().id,
                                         checkpoint_receipt(cp.manifest_proof))!=qdcoop::Result::ok){
                    checkpoint_abort(time,true);
                    return false;
                }
                cp.latest=cp.pending;
                cp.latest_here=cp.pending_here;
                cp.seen=cp.captured;
                cp.stage=CheckpointStage::idle;
                ++checkpoint_commits;
            }else if(time>=cp.deadline||checkpoint_host_roster()!=cp.pending.roster){
                checkpoint_abort(time,false);
            }else if(time>=cp.next_send){
                cp.next_send=time+.2;
                checkpoint_send(CP::Op::commit,cp.pending,0,active_peer_mask);
            }
        }else if(cp.stage==CheckpointStage::idle){
            if(cp.abort_notice.token&&time>=cp.next_send){
                cp.next_send=time+.2;
                checkpoint_send(CP::Op::abort,cp.abort_notice,0,active_peer_mask);
            }
            if(!cp.latest.token||cp.stock!=cp.seen||cp.wanted||cp.latest.roster!=checkpoint_host_roster())
                checkpoint_start(pawn,time);
        }
    }else if(time>=cp.next_send){
        if((cp.stage==CheckpointStage::ready||cp.stage==CheckpointStage::rejected)&&time>=cp.deadline){
            checkpoint_abort(time,false);
            cp.stage=CheckpointStage::idle;
        }
        cp.next_send=time+.2;
        if(cp.stage==CheckpointStage::ready)checkpoint_send(CP::Op::ready,cp.pending,cp.self,1);
        else if(cp.stage==CheckpointStage::rejected)checkpoint_send(CP::Op::reject,cp.pending,cp.self,1);
        else if(cp.stage==CheckpointStage::idle&&cp.latest.token&&cp.stock!=cp.seen){
            CP::Manifest request{0,cp.stock,cp.latest.roster,false,{}};
            checkpoint_send(CP::Op::request,request,cp.self,1);
        }
    }
    return false;
}

#include "native_checkpoint_rollback.inc"

extern "C" __declspec(dllexport) int QDEnemyInspectNativeCheckpoint(CP::BindingView* out){
    if(GetCurrentThreadId()!=game_thread.load()||!out)return 0;
    const auto& cp=native_checkpoint;
    *out={cp.latest,envelope,cp.stock,checkpoint_archives,checkpoint_commits,checkpoint_errors,
          checkpoint_rollbacks,checkpoint_world_nonce,
          static_cast<std::uint32_t>(cp.stage),cp.self,cp.availability,cp.failure,
          rollback.stage};
    return 1;
}
