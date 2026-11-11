namespace Boss=qdcoop::boss_phase;

struct NativeBossSlot {
    Weak type,component;
    void* component_property{};
    std::array<void*,3> flags{},phase{};
    std::array<Boss::State,E::history> inbox{};
    Boss::State applied;
    Boss::Profile profile{};
    std::uint32_t generation{};
    double retry{};
    bool classified{},bound{};
};
static std::array<NativeBossSlot,E::capacity> native_bosses;
static void native_boss_interact_update(unsigned index,const NativeBossSlot& slot,void* actor);

static Boss::Profile boss_actor_profile(unsigned class_id,void* actor){
    const auto catalog=Boss::profile_for(class_id);
    if(catalog!=Boss::Profile::none)return catalog;
    auto type=AnimU::field<void*>(actor,16);
    for(unsigned depth=0;type&&type!=base_class&&depth<128;++depth,type=AnimU::field<void*>(type,64)){
        std::string path;
        if(!catalog_path(type,path))break;
        for(unsigned i=1;i<Boss::descriptors.size();++i)
            if(path==Boss::descriptors[i].path)return static_cast<Boss::Profile>(i);
    }
    return Boss::Profile::none;
}

static bool native_encounter_managed(unsigned class_id,void* actor){
    static std::array<std::uint8_t,std::size(C::classes)> classification{};
    auto& value=classification[class_id];
    if(!value)value=boss_actor_profile(class_id,actor)==Boss::Profile::none?1:2;
    return value==2;
}

static NativeBossSlot* native_boss_slot(unsigned index){
    const auto& entity=entities[index];
    const auto& offer=enrollments[index].offer;
    if(!native_bootstrap||!entity.used||!offer.id||entity.generation!=offer.generation)return nullptr;
    auto& slot=native_bosses[index];
    if(slot.generation!=entity.generation){
        slot={};
        slot.generation=entity.generation;
    }
    if(!slot.classified){
        const auto actor=api.weakget(&entity.actor);
        if(!actor)return nullptr;
        slot.profile=boss_actor_profile(offer.class_id,actor);
        slot.classified=true;
    }
    return slot.profile==Boss::Profile::none?nullptr:&slot;
}
static bool native_boss_managed(unsigned index){return native_boss_slot(index)!=nullptr;}

static bool native_boss_bind(NativeBossSlot& slot,void* actor,double time){
    const auto type=AnimU::field<void*>(actor,16);
    if(slot.bound&&api.weakget(&slot.type)==type){
        const auto component=api.weakget(&slot.component);
        if(component&&component==AnimU::field<void*>(actor,*api.offset(slot.component_property)))return true;
    }
    if(time<slot.retry)return false;
    slot.retry=time+.25;
    const auto& descriptor=Boss::descriptors[static_cast<unsigned>(slot.profile)];
    constexpr std::array<std::string_view,3> flag_names{"bDead","DeathIsSuspended","bIsAttacking"};
    for(unsigned i=0;i<flag_names.size();++i){
        slot.flags[i]=catalog_field(type,flag_names[i],1);
        if(!medic_field_class(slot.flags[i],"BoolProperty"))return false;
        if(descriptor.kinds[i]==Boss::Field::absent)continue;
        const bool boolean=descriptor.kinds[i]==Boss::Field::boolean;
        slot.phase[i]=catalog_field(type,descriptor.names[i],boolean?1:4);
        if(!medic_field_class(slot.phase[i],boolean?"BoolProperty":"IntProperty"))return false;
    }
    const auto property=catalog_field(type,descriptor.component,8),primitive=world_input(world_hit_primitive),
               get_owner=factory_object(factory_owner);
    if(!medic_field_class(property,"ObjectProperty"))return false;
    const auto component=AnimU::field<void*>(actor,*api.offset(property));
    if(!component||!primitive||!get_owner||!api.isa(component,primitive)||
       !medic_params(world_actor_collision,1,{{"ReturnValue",1,0}}))return false;
    void* owner{};
    api.event(component,get_owner,&owner);
    if(owner!=actor||!makeweak(slot.component,component))return false;
    slot.component_property=property;
    slot.bound=makeweak(slot.type,type);
    return slot.bound;
}

static void native_boss_capture(unsigned index,const E::State& scalar,unsigned peers){
    auto* slot=native_boss_slot(index);
    if(!slot)return;
    const auto actor=api.weakget(&entities[index].actor);
    if(!actor||!native_boss_bind(*slot,actor,scalar.time))return;
    const auto component=api.weakget(&slot->component),collision=world_input(world_actor_collision);
    if(!component||!collision)return;
    Boss::State state{scalar.id,scalar.generation,scalar.sequence,slot->profile,0,0,{}};
    const auto& descriptor=Boss::descriptors[static_cast<unsigned>(slot->profile)];
    for(unsigned i=0;i<slot->flags.size();++i){
        if(api.getbool(slot->flags[i],actor,0))state.flags|=1u<<i;
        if(descriptor.kinds[i]==Boss::Field::boolean)state.phase[i]=api.getbool(slot->phase[i],actor,0);
        else if(descriptor.kinds[i]==Boss::Field::integer)state.phase[i]=AnimU::field<std::int32_t>(actor,*api.offset(slot->phase[i]));
    }
    api.event(component,collision,&state.interaction);
    E::Packet packet{};
    if(Boss::encode(envelope,state,packet)&&send(packet,peers))++boss_phase_sent;
}

static bool receive_native_boss(const ReceivedPacket& received,unsigned kind){
    if(kind!=Boss::kind)return false;
    Boss::State state;
    if(host_role||received.peer||!active_peer_mask||!Boss::decode(envelope,received.packet,state))return true;
    const auto index=state.id-1;
    if(retirement_requested(index)||entities[index].generation!=state.generation)return true;
    auto* slot=native_boss_slot(index);
    if(!slot||slot->profile!=state.profile||state.sequence<=slot->applied.sequence)return true;
    auto& entry=slot->inbox[state.sequence%slot->inbox.size()];
    if(entry.sequence<state.sequence){entry=state;++boss_phase_received;}
    return true;
}

static void native_boss_apply(unsigned index,const E::State& scalar,double time){
    auto* slot=native_boss_slot(index);
    if(!slot||slot->applied.sequence>=scalar.sequence||!ownership[index].active)return;
    const auto actor=api.weakget(&entities[index].actor);
    if(!actor||!native_boss_bind(*slot,actor,time))return;
    const Boss::State* state{};
    const auto limit=std::min<std::uint64_t>(slot->inbox.size(),scalar.sequence-slot->applied.sequence);
    for(std::uint64_t i=0;i<limit;++i){
        const auto sequence=scalar.sequence-i;
        const auto& candidate=slot->inbox[sequence%slot->inbox.size()];
        if(candidate.sequence==sequence){state=&candidate;break;}
    }
    if(!state)return;
    const auto& descriptor=Boss::descriptors[static_cast<unsigned>(slot->profile)];
    for(unsigned i=0;i<slot->flags.size();++i){
        const bool flag=(state->flags&(1u<<i))!=0;
        if(api.getbool(slot->flags[i],actor,0)!=flag)api.setbool(slot->flags[i],actor,flag,0);
        if(descriptor.kinds[i]==Boss::Field::boolean){
            const bool value=state->phase[i]!=0;
            if(api.getbool(slot->phase[i],actor,0)!=value)api.setbool(slot->phase[i],actor,value,0);
        }else if(descriptor.kinds[i]==Boss::Field::integer){
            auto* field=static_cast<unsigned char*>(actor)+*api.offset(slot->phase[i]);
            if(AnimU::field<std::int32_t>(field,0)!=state->phase[i])std::memcpy(field,&state->phase[i],sizeof(state->phase[i]));
        }
    }
    constexpr unsigned defeated=Boss::dead|Boss::suspended;
    if((state->flags&defeated)==defeated&&state->interaction&&
       ((slot->applied.flags&defeated)!=defeated||!slot->applied.interaction))++boss_phase_interactable;
    slot->applied=*state;
    native_boss_interact_update(index,*slot,actor);
    ++boss_phase_applied;
}

static void native_boss_reset(){native_bosses={};}
