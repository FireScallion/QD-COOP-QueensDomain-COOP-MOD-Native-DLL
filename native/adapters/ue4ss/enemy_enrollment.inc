namespace Roster=qdcoop::enemy::enrollment;
struct Enrollment {
    Roster::Offer offer{};
    unsigned peers{},waiting{};
    double retry{};
    bool confirmed{};
    std::uint32_t tombstone{},retiring{};
    unsigned retire_waiting{};
    double retire_retry{},death_at{};
    Weak replica{},display{};
    bool factory_ready{},hit_queries{};
    double factory_retry{};
};
static std::array<Enrollment,E::capacity> enrollments{};
static std::uint64_t enrollment_sent,enrollment_received,enrollment_acked,enrollment_confirmed,enrollment_rejected;
static bool managed_enrollment;
static unsigned unconfirmed_offers;
static Weak replica_destroy_method;
static bool client_retire_pending;
static unsigned client_retire_cursor;
static std::uint64_t retire_sent,retire_acked,retire_applied,retire_duplicates,retire_rejected,
                     retire_deferred,retire_automatic,retire_destroyed,roster_stale;

static bool retirement_requested(unsigned index){
    return enrollments[index].retiring!=0;
}

static bool retired_generation(unsigned index,std::uint32_t generation){
    return generation&&generation<=enrollments[index].tombstone;
}

static bool stale_replica_generation(unsigned index,std::uint32_t generation){
    const auto& entry=enrollments[index];
    return retired_generation(index,generation)||(entry.retiring&&generation<=entry.retiring);
}

static bool owns_actor_elsewhere(unsigned index,void* actor){
    for(unsigned i=0;i<E::capacity;++i){
        const auto& entry=enrollments[i];
        if(i!=index&&entry.offer.id&&(api.weakget(&entry.replica)==actor||api.weakget(&entry.display)==actor))
            return true;
    }
    return false;
}

static bool actor_is_destroying(void* actor){
    alignas(16) unsigned char args[256]{};
    api.event(actor,getters[3].fn,args);
    return api.getbool(getters[3].ret,args,0);
}

static bool destroy_owned_actor(Weak& key,bool enemy=true){
    auto actor=api.weakget(&key);
    if(!actor)return true;
    const auto destroy=api.weakget(&replica_destroy_method);
    if(!destroy||api.weakget(&metadata_weak[2])!=actor_class||
       api.weakget(&metadata_weak[10])!=getters[3].fn||
       !api.isa(actor,actor_class)||(AnimU::field<unsigned>(actor,8)&0x10u))return false;
    if(!actor_is_destroying(actor)){
        api.event(actor,destroy,nullptr);
        if(enemy)++retire_destroyed;
        if(api.weakget(&key)==actor&&!actor_is_destroying(actor))return false;
    }
    key={};
    return true;
}

static bool destroy_owned_replica(Enrollment& entry){
    const bool display_gone=destroy_owned_actor(entry.display);
    const bool replica_gone=destroy_owned_actor(entry.replica);
    return display_gone&&replica_gone;
}

static void complete_retirement(unsigned index){
    auto& entry=enrollments[index];
    const auto generation=entry.retiring;
    if(!host_role&&entry.offer.id&&!entry.confirmed)--unconfirmed_offers;
    entities[index]={};
    anim_bindings[index]={};
    deaths[index]={};
    ownership[index]={};
    entry={};
    entry.tombstone=generation;
}

static bool begin_host_retirement(unsigned index,std::uint32_t generation){
    auto& entry=enrollments[index];
    if(entry.offer.generation!=generation)return false;
    if(entry.retiring)return entry.retiring==generation;
    entry.retiring=generation;
    entry.retire_waiting=entry.peers;
    if(deaths[index].waiting)++retire_deferred;
    return true;
}

static unsigned enrollment_peer_mask(unsigned index){
    const auto& entry=enrollments[index];
    return entry.offer.id?entry.peers&~entry.waiting:7u;
}

static void pump_enrollment(double time){
    if(!managed_enrollment)return;
    constexpr double death_grace=10.0;
    for(unsigned i=0;i<E::capacity;++i){
        auto& entry=enrollments[i];
        if(!entry.offer.id)continue;
        auto& entity=entities[i];
        const auto& death=deaths[i];
        if(death.terminal&&!entry.death_at)entry.death_at=time;
        const bool death_grace_finished=!death.terminal||time-entry.death_at>=death_grace;
        if(!entry.retiring){
            void* mesh{};
            // Unconfirmed actors are outside the capture loop. Check them
            // when the offer becomes due so expiry still sends a tombstone.
            const bool expired=!death.terminal&&
                (!entity.used||(entry.waiting&&time>=entry.retry&&!resolve(entity,mesh)));
            if(expired||(death.terminal&&death_grace_finished)){
                begin_host_retirement(i,entry.offer.generation);
                ++retire_automatic;
            }
        }
        if(entry.retiring&&!death.waiting){
            if(!death_grace_finished||time<entry.retire_retry)continue;
            E::Packet packet{};
            if(Roster::encode(envelope,Roster::Retirement{i+1,entry.retiring},false,packet)&&
               send(packet,entry.retire_waiting)){
                ++retire_sent;
                entry.retire_retry=time+.2;
            }
            continue;
        }
        if(!entry.waiting||time<entry.retry)continue;
        E::Packet packet{};
        if(Roster::encode(envelope,entry.offer,false,packet)&&send(packet,entry.waiting)){
            ++enrollment_sent;
            entry.retry=time+.2;
        }
    }
}

static void ack_retirement(const Roster::Retirement& value){
    E::Packet ack{};
    if(Roster::encode(envelope,value,true,ack))send(ack);
}

static void receive_retirement(const ReceivedPacket& received){
    Roster::Retirement value{};
    if(!Roster::decode(envelope,received.packet,host_role,value)){
        ++retire_rejected;
        return;
    }
    const auto index=value.id-1;
    auto& entry=enrollments[index];
    if(host_role){
        if(retired_generation(index,value.generation))return;
        if(entry.retiring!=value.generation||!entry.retire_retry||deaths[index].waiting||
           received.peer>=3||!(entry.peers&(1u<<received.peer))){
            ++retire_rejected;
            return;
        }
        const auto peer=1u<<received.peer;
        if(entry.retire_waiting&peer)++retire_acked;
        entry.retire_waiting&=~peer;
        if(!entry.retire_waiting)complete_retirement(index);
    }else if(retired_generation(index,value.generation)){
        ++retire_duplicates;
        ack_retirement(value);
    }else if(entry.retiring==value.generation){
        ++retire_duplicates;
    }else if(entry.retiring||(entry.offer.id&&entry.offer.generation!=value.generation)||
             (!entry.offer.id&&entities[index].used)){
        ++retire_rejected;
    }else{
        entry.retiring=value.generation;
        disable_native_hit_queries(index,api.weakget(&entry.replica));
        client_retire_pending=true;
        if(auto visual=api.weakget(&entry.display)){
            unsigned char hidden=1;
            api.event(visual,hide_fn,&hidden);
        }
    }
}

static void pump_client_retirements(double time){
    if(!client_retire_pending)return;
    client_retire_pending=false;
    constexpr unsigned cleanup_per_tick=4;
    unsigned visited=0,attempted=0;
    for(;visited<E::capacity&&attempted<cleanup_per_tick;++visited){
        const auto i=client_retire_cursor;
        client_retire_cursor=(client_retire_cursor+1)%E::capacity;
        auto& entry=enrollments[i];
        if(!entry.retiring)continue;
        if(time<entry.retire_retry){
            client_retire_pending=true;
            continue;
        }
        ++attempted;
        if(!destroy_owned_replica(entry)){
            entry.retire_retry=time+.2;
            client_retire_pending=true;
            continue;
        }
        const Roster::Retirement value{i+1,entry.retiring};
        complete_retirement(i);
        ++retire_applied;
        ack_retirement(value);
    }
    if(visited<E::capacity)client_retire_pending=true;
}

static bool receive_enrollment(const ReceivedPacket& received){
    const auto& packet=received.packet;
    if(!native_bootstrap||packet.size<40)return false;
    const auto kind=A::wire::get<std::uint32_t>(packet,32);
    if(kind==Roster::retire_kind||kind==Roster::retire_ack_kind){
        receive_retirement(received);
        return true;
    }
    if(kind!=Roster::offer_kind&&kind!=Roster::ack_kind)return false;
    Roster::Offer offer{};
    if(!Roster::decode(envelope,packet,host_role,offer)){
        ++enrollment_rejected;
        return true;
    }
    auto& entry=enrollments[offer.id-1];
    if(host_role){
        if(retired_generation(offer.id-1,offer.generation))return true;
        if(entry.offer==offer&&received.peer<3&&(entry.peers&(1u<<received.peer))){
            const unsigned peer=1u<<received.peer;
            if(entry.waiting&peer)++enrollment_acked;
            entry.waiting&=~peer;
        }else ++enrollment_rejected;
    }else if(stale_replica_generation(offer.id-1,offer.generation)){
        ++roster_stale;
    }else if(!entry.offer.id&&!entry.retiring){
        entry.offer=offer;
        ++unconfirmed_offers;
        ++enrollment_received;
    }else if(offer.generation==entry.offer.generation){
        if(!(entry.offer==offer))++enrollment_rejected;
        else if(entry.confirmed){
            E::Packet ack{};
            if(Roster::encode(envelope,offer,true,ack))send(ack);
        }
    }else if(offer.generation>entry.offer.generation)++enrollment_rejected;
    return true;
}
