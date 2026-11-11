enum class MedicPhase { unbound, opening, idle, writing, proof_read, proof_verify, closing, error };
enum class MedicEffect { debit, refund, revive };
struct MedicProof {
    Medic::Attempt attempt;
    MedicEffect effect{};
    std::wstring path;
    Medic::SaveProof hash;
};
struct MedicRuntime {
    MedicSave save;
    Medic::Ledger ledger,pending;
    Medic::Attempt ready;
    MedicProof proof;
    MedicPhase phase{};
    std::uint64_t hold_token{};
    std::uint64_t ready_sent{};
    std::array<double,Medic::receipt_capacity> reservation_deadlines{};
    double next_bind{},next_send{};
};
static MedicRuntime medic_runtime;
static Medic::StorageWorker medic_worker;

static void medic_error(){
    medic_runtime.phase=MedicPhase::error;
    ++medic_errors;
}
static bool medic_cleanup(){
    auto& m=medic_runtime;
    if(m.phase==MedicPhase::unbound)return true;
    Medic::StorageResult result{};
    if(m.phase!=MedicPhase::idle&&m.phase!=MedicPhase::error){
        if(!medic_worker.poll(result))return false;
        if(m.phase==MedicPhase::closing){m={};return true;}
    }
    if(!medic_worker.close())return false;
    m.phase=MedicPhase::closing;
    return false;
}
static qdcoop::Id medic_identity(){
    const auto& m=medic_runtime;
    return m.phase>=MedicPhase::idle&&m.phase<=MedicPhase::proof_verify&&
           medic_active(m.save,local_life.actor)?m.ledger.identity:qdcoop::Id{};
}
static std::uint64_t medic_account(){
    return steam_transport?steam_local_account.load(std::memory_order_acquire):0;
}
static bool medic_commit(Medic::Ledger next){
    auto& m=medic_runtime;
    if(m.phase!=MedicPhase::idle||next.generation==UINT64_MAX)return false;
    ++next.generation;
    if(!medic_worker.commit(next)){medic_error();return false;}
    m.pending=std::move(next);
    m.phase=MedicPhase::writing;
    return true;
}
static bool medic_reserved_life(std::uint64_t token){
    const auto& m=medic_runtime;
    const auto reserved=[&](const Medic::Ledger& ledger){
        return std::ranges::any_of(ledger.incoming,[&](const auto& r){
            return r.attempt.life==token&&(r.phase==Medic::Receipt::prepared||r.phase==Medic::Receipt::applying);
        });
    };
    return medic_identity().valid()&&(reserved(m.ledger)||(m.phase==MedicPhase::writing&&reserved(m.pending)));
}
static bool medic_local_down(const Medic::Attempt& attempt){
    return local_life.locked&&local_life.life==Player::Life{attempt.life,LifePhase::down};
}
static void* medic_asset(unsigned item){return load_player_asset(medicine_paths[item-1],UINT32_MAX,false);}
static AnimU::NameKey medic_receipt_name(const Medic::Attempt& attempt){
    return medic_name(L"QDCOOPNativeRescueV1_Applied_"+Medic::rescue_suffix(attempt.donor)+L"_"+std::to_wstring(attempt.serial));
}
static std::wstring medic_proof_path(const MedicSave& save,const Medic::Attempt& attempt,MedicEffect effect){
    const auto kind=effect==MedicEffect::debit?L"-debit":effect==MedicEffect::refund?L"-refund":L"-apply";
    return save.primary+L".qdr-"+Medic::rescue_suffix(attempt.donor)+L"-"+std::to_wstring(attempt.serial)+kind+L".sav";
}
static bool medic_proof_contents(void* saved,const MedicProof& job){
    const auto& m=medic_runtime;
    qdcoop::Id identity{};
    if(!saved||!medic_identity(m.save,saved,identity)||identity!=m.ledger.identity)return false;
    if(job.effect==MedicEffect::revive)
        return medic_marker(m.save,saved,medic_receipt_name(job.attempt))==1;
    const auto index=job.attempt.item-1;
    const auto& inventory=m.ledger.inventory[index];
    const auto item=medic_asset(job.attempt.item);
    int count{};
    const bool found=item&&m.save.find(m.save.inventory.address(saved),m.save.inventory.field,&item,&count);
    return item&&(found||!inventory.quantity)&&count==inventory.quantity&&
           medic_marker(m.save,saved,m.save.revisions[index])==inventory.revision&&
           medic_marker(m.save,saved,m.save.uses[index])==inventory.uses;
}
static bool medic_begin_proof(void* pawn,const Medic::Attempt& attempt,MedicEffect effect){
    auto& m=medic_runtime;
    const auto path=medic_proof_path(m.save,attempt,effect);
    if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES&&!medic_save_snapshot(m.save,pawn,path))return false;
    if(!medic_worker.proof(path))return false;
    m.proof={attempt,effect,path,{}};
    m.phase=MedicPhase::proof_read;
    return true;
}
static bool medic_reconcile(void* pawn){
    const auto& m=medic_runtime;
    const auto save=medic_active(m.save,pawn);
    if(!save)return false;
    for(unsigned i=0;i<Medic::medicines;++i){
        const auto& desired=m.ledger.inventory[i];
        if(medic_marker(m.save,save,m.save.revisions[i])==desired.revision&&
           medic_marker(m.save,save,m.save.uses[i])==desired.uses)continue;
        if(!desired.revision||!medic_reconcile_item(m.save,pawn,i,desired,medic_asset(i+1)))return false;
    }
    return true;
}
static void medic_finish_io(void* pawn,const Medic::StorageResult& result){
    auto& m=medic_runtime;
    if(!result.ok){medic_error();return;}
    if(m.phase==MedicPhase::opening){
        m.ledger=result.ledger;
        const auto save=medic_active(m.save,pawn);
        qdcoop::Id id{};
        if(!save||!medic_identity(m.save,save,id)||
           (id.valid()?id!=m.ledger.identity:result.proof!=m.ledger.anchor)||
           (!id.valid()&&!medic_set_identity(m.save,save,m.ledger.identity))||!medic_reconcile(pawn)){
            medic_error();
            return;
        }
        m.phase=MedicPhase::idle;
    }else if(m.phase==MedicPhase::writing){
        if(result.ledger!=m.pending){medic_error();return;}
        m.ledger=result.ledger;
        m.pending={};
        m.phase=MedicPhase::idle;
        if(!m.proof.path.empty()){
            if(m.proof.effect==MedicEffect::debit)++medic_debits;
            else if(m.proof.effect==MedicEffect::refund)++medic_refunds;
            else ++medic_revives;
            DeleteFileW(m.proof.path.c_str());
            m.proof={};
        }
    }else if(m.phase==MedicPhase::proof_read){
        const auto saved=medic_load_snapshot(m.save,m.proof.path);
        if(!medic_proof_contents(saved,m.proof)||!medic_worker.proof(m.proof.path)){
            medic_error();
            return;
        }
        m.proof.hash=result.proof;
        m.phase=MedicPhase::proof_verify;
    }else if(m.phase==MedicPhase::proof_verify){
        if(result.proof!=m.proof.hash){medic_error();return;}
        auto next=m.ledger;
        bool valid{};
        switch(m.proof.effect){
        case MedicEffect::debit:valid=next.debit_saved(m.proof.attempt,result.proof);break;
        case MedicEffect::refund:valid=next.refund_saved(m.proof.attempt);break;
        case MedicEffect::revive:valid=next.receive(m.proof.attempt,Medic::Receipt::applied,result.proof);break;
        }
        m.phase=MedicPhase::idle;
        if(!valid||!medic_commit(next))medic_error();
    }
}
static bool medic_revive(void* pawn,const Medic::Attempt& attempt,double time){
    const auto& m=medic_runtime;
    const auto save=medic_active(m.save,pawn);
    if(!save)return false;
    const auto receipt=medic_receipt_name(attempt);
    if(medic_marker(m.save,save,receipt)==1)return true;
    if(!medic_local_down(attempt))return false;
    auto level=static_cast<int>((attempt.item-1)%3);
    const auto heal=world_input(attempt.item<=3?world_medic_potion:world_medic_herb);
    if(!heal)return false;
    life_unlock(false);
    api.event(pawn,heal,&level);
    if(AnimU::field<float>(pawn,local_life.layout.health)<=0)life_set_health(pawn,1);
    if(medic_active(m.save,pawn)!=save||AnimU::field<float>(pawn,local_life.layout.health)<=0)return false;
    medic_set_marker(m.save,save,receipt,1);
    local_life.life={attempt.life,LifePhase::rising};
    local_life.rise_until=time+.48;
    ++life_revived;
    return true;
}

static int medic_peer(qdcoop::Id id,double time){
    int result=-1;
    for(unsigned i=0;i<life_peers.size();++i){
        const auto& peer=life_peers[i];
        if(peer.rescue_identity!=id||!peer.generation||time-peer.received>Player::rescue_fresh||
           (host_role&&i&&!(active_peer_mask&(1u<<(i-1)))))continue;
        if(result>=0)return -1;
        result=static_cast<int>(i);
    }
    return result;
}
static void medic_receive(const Medic::Message& message,double time);
static bool medic_route(const Medic::Message& message,unsigned source,double time){
    const auto& a=message.attempt;
    const bool donor=Medic::from_donor(message.op);
    const auto origin=medic_peer(donor?a.donor:a.target,time);
    const auto destination=medic_peer(donor?a.target:a.donor,time);
    if(origin!=static_cast<int>(source)||destination<0)return false;
    if(life_peers[origin].rescue_account!=(donor?a.donor_account:a.target_account)||
       life_peers[destination].rescue_account!=(donor?a.target_account:a.donor_account))return false;
    if(message.op==Medic::Op::offer){
        const auto& from=life_peers[origin];
        const auto& to=life_peers[destination];
        if(!fresh_life(from,LifePhase::standing,time)||!fresh_life(to,LifePhase::down,time)||
           to.life.token!=a.life||!Player::within_rescue_range(from.position,to.position))return false;
    }
    if(!destination){medic_receive(message,time);return true;}
    E::Packet packet{};
    return Medic::encode_message(envelope,message,packet)&&send(packet,1u<<(destination-1));
}
static bool medic_send(Medic::Op op,const Medic::Attempt& attempt,double time){
    const Medic::Message message{attempt,op};
    if(host_role)return medic_route(message,0,time);
    E::Packet packet{};
    return Medic::encode_message(envelope,message,packet)&&send(packet,1);
}
static void medic_receive(const Medic::Message& message,double time){
    auto& m=medic_runtime;
    if(m.phase!=MedicPhase::idle||!medic_identity().valid())return;
    const auto& a=message.attempt;
    const auto op=message.op;
    auto next=m.ledger;
    if(Medic::from_donor(op)){
        if(a.target!=next.identity||a.target_account!=medic_account())return;
        const auto r=next.find(a);
        if(r&&r->attempt!=a)return;
        if(op==Medic::Op::ack){if(next.acknowledge(a))medic_commit(next);return;}
        if(op==Medic::Op::offer||op==Medic::Op::cancel){
            if(!r&&!next.prepare(a))return;
            if(op==Medic::Op::cancel||!medic_local_down(a)){
                if((!r||r->phase==Medic::Receipt::prepared)&&next.receive(a,Medic::Receipt::rejected))medic_commit(next);
            }else{
                const auto reservation=next.find(a);
                if(reservation&&reservation->phase==Medic::Receipt::prepared){
                    const auto index=static_cast<unsigned>(reservation-next.incoming.data());
                    m.reservation_deadlines[index]=time+8.;
                }
                if(!r)medic_commit(next);
            }
            return;
        }
        if(op==Medic::Op::debit&&r&&r->phase==Medic::Receipt::prepared){
            if(next.receive(a,medic_local_down(a)?Medic::Receipt::applying:Medic::Receipt::rejected))
                medic_commit(next);
        }
        return;
    }
    if(a.donor!=next.identity||a.donor_account!=medic_account())return;
    if(op==Medic::Op::ready){
        if(next.outgoing.phase==Medic::Debit::offered&&next.outgoing.attempt==a)m.ready=a;
        return;
    }
    if(a.serial<=next.serial&&a!=next.outgoing.attempt){
        medic_send(Medic::Op::ack,a,time);
        return;
    }
    if(a!=next.outgoing.attempt)return;
    const auto result=op==Medic::Op::applied?Medic::Receipt::applied:Medic::Receipt::rejected;
    int quantity{};
    if(result==Medic::Receipt::rejected&&next.outgoing.phase==Medic::Debit::persisted)
        quantity=medic_count(m.save,medic_asset(a.item));
    if(next.settle(a,result,quantity))medic_commit(next);
}
static void receive_native_medic(const ReceivedPacket& received,double time){
    Medic::Message message{};
    if(!automatic_limit||!Medic::decode_message(envelope,received.packet,message))return;
    if(!host_role){if(received.peer==0)medic_receive(message,time);return;}
    if(received.peer<3&&(active_peer_mask&(1u<<received.peer)))medic_route(message,received.peer+1,time);
}

static void medic_hold_complete(unsigned item,void* selected){
    auto& m=medic_runtime;
    const auto& hold=rescue_hold.message;
    if(m.phase!=MedicPhase::idle||native_placed_item_save_pending()||!medic_identity().valid()||!selected||
       !life_peers[hold.target].rescue_identity.valid())return;
    auto next=m.ledger;
    if(next.outgoing.phase==Medic::Debit::none){
        const auto& peer=life_peers[hold.target];
        const Medic::Attempt attempt{next.identity,peer.rescue_identity,next.serial+1,hold.token,item,
                                     medic_account(),peer.rescue_account};
        if(next.offer(attempt)&&medic_commit(next)){m.hold_token=hold.attempt;m.ready={};}
    }else if(next.outgoing.phase==Medic::Debit::offered&&m.hold_token==hold.attempt&&
             m.ready==next.outgoing.attempt&&m.ready.item==item&&m.ready.life==hold.token){
        const auto quantity=medic_count(m.save,selected);
        if(next.intend(m.ready,quantity))medic_commit(next);
    }
}
static bool medic_do_effect(void* pawn,double time){
    auto& m=medic_runtime;
    const auto& out=m.ledger.outgoing;
    if(out.phase==Medic::Debit::intended||out.phase==Medic::Debit::refunding){
        const auto index=out.attempt.item-1;
        const auto effect=out.phase==Medic::Debit::intended?MedicEffect::debit:MedicEffect::refund;
        if(!medic_reconcile_item(m.save,pawn,index,m.ledger.inventory[index],medic_asset(index+1))||
           !medic_begin_proof(pawn,out.attempt,effect))medic_error();
        return true;
    }
    for(const auto& r:m.ledger.incoming)if(r.phase==Medic::Receipt::applying){
        const auto archived=GetFileAttributesW(medic_proof_path(m.save,r.attempt,MedicEffect::revive).c_str())!=
                            INVALID_FILE_ATTRIBUTES;
        if(archived||medic_revive(pawn,r.attempt,time)){
            if(!medic_begin_proof(pawn,r.attempt,MedicEffect::revive))medic_error();
        }else if(!medic_local_down(r.attempt)){
            auto next=m.ledger;
            if(next.receive(r.attempt,Medic::Receipt::rejected))medic_commit(next);
        }else medic_error();
        return true;
    }
    return false;
}
static void medic_pump(void* pawn,double time,bool wanted){
    auto& m=medic_runtime;
    if(m.phase==MedicPhase::unbound){
        if(!wanted||time<m.next_bind)return;
        m.next_bind=time+.25;
        scan_world_inputs(2048);
        MedicSave save{};
        qdcoop::Id branch{};
        const bool bound=medic_bind_save(pawn,api.weakget(&local_life.instance),save);
        medic_save_bind_check=medic_bind_check;
        if(bound&&medic_branch(save,medic_active(save,pawn),branch)&&medic_worker.open(save.primary,branch)){
            m.save=std::move(save);
            m.phase=MedicPhase::opening;
        }
        return;
    }
    if(m.phase==MedicPhase::error)return;
    if(m.phase!=MedicPhase::idle){
        Medic::StorageResult result{};
        if(medic_worker.poll(result))medic_finish_io(pawn,result);
        return;
    }
    if(!medic_active(m.save,pawn)){medic_error();return;}
    if(medic_do_effect(pawn,time))return;
    const auto& out=m.ledger.outgoing;
    if(out.phase==Medic::Debit::offered&&(!m.hold_token||m.hold_token!=rescue_hold.message.attempt)){
        auto next=m.ledger;
        if(next.cancel(out.attempt))medic_commit(next);
        return;
    }
    for(unsigned i=0;i<m.ledger.incoming.size();++i){
        const auto& r=m.ledger.incoming[i];
        if(r.phase!=Medic::Receipt::prepared)continue;
        if(!m.reservation_deadlines[i])m.reservation_deadlines[i]=time+2.;
        if(medic_local_down(r.attempt)&&time<m.reservation_deadlines[i])continue;
        auto next=m.ledger;
        if(next.receive(r.attempt,Medic::Receipt::rejected))medic_commit(next);
        return;
    }
    if(time<m.next_send)return;
    m.next_send=time+.2;
    switch(out.phase){
    case Medic::Debit::offered:medic_send(Medic::Op::offer,out.attempt,time);break;
    case Medic::Debit::cancelled:medic_send(Medic::Op::cancel,out.attempt,time);break;
    case Medic::Debit::persisted:medic_send(Medic::Op::debit,out.attempt,time);break;
    default:break;
    }
    for(const auto& r:m.ledger.incoming){
        if(m.phase!=MedicPhase::idle)break;
        if(r.phase==Medic::Receipt::prepared){
            if(medic_send(Medic::Op::ready,r.attempt,time))++m.ready_sent;
        }
        else if(r.phase==Medic::Receipt::applied)medic_send(Medic::Op::applied,r.attempt,time);
        else if(r.phase==Medic::Receipt::rejected)medic_send(Medic::Op::rejected,r.attempt,time);
    }
}

extern "C" __declspec(dllexport) int QDEnemyInspectNativeMedic(Medic::BindingView* out){
    if(GetCurrentThreadId()!=game_thread.load()||!out)return 0;
    const auto& m=medic_runtime;
    *out={medic_identity(),static_cast<std::uint32_t>(m.phase),static_cast<std::uint32_t>(m.ledger.outgoing.phase),medic_save_bind_check,
          m.ledger.generation,medic_debits,medic_revives,medic_refunds,medic_errors,m.ready_sent};
    return 1;
}
