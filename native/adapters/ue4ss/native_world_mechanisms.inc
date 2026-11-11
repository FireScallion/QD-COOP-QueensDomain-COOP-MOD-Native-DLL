static bool world_mechanism_method(void* type,std::string_view name,Weak& out);
#include "native_world_shrine.inc"

struct WorldMechanismActor { WP::Guid key; Weak actor,enable,disable,saved_open; ShrineGate shrine; };
struct WorldMechanismBinding {
    Weak type;
    MedicMap guids;
    void* active{};
    void* script_openable{};
    int count{-1};
    bool sound{};
    std::vector<WorldMechanismActor> actors;
};
struct WorldMechanisms {
    std::array<WorldMechanismBinding,2> kinds;
    WP::Revision desired;
    AnimU::NameKey shrine_boss;
    std::uint64_t scan{};
    std::size_t cursor{},saved_cursor{};
    double next_scan{};
};
static WorldMechanisms world_mechanisms;

static void world_mechanisms_reset(){world_mechanisms={};}
static bool world_mechanism_bind(unsigned kind){
    auto& binding=world_mechanisms.kinds[kind];
    const auto type=world_input(kind?world_mechanism_lever:world_mechanism_door);
    if(type&&api.weakget(&binding.type)==type&&binding.active)return true;
    if(!type)return false;
    const auto enable=world_input(kind?world_mechanism_pull:world_mechanism_open);
    const auto disable=world_input(kind?world_mechanism_reset:world_mechanism_close);
    if(!enable||!disable||*api.parms(enable)||*api.parms(disable)||
       AnimU::field<int>(enable,104)!=18||AnimU::field<int>(disable,104)!=18)return false;
    WorldMechanismBinding candidate{};
    candidate.guids=world_actor_array(type,"GUIDs",16,"/Script/CoreUObject.Guid");
    candidate.active=catalog_field(type,kind?"ToggledOn":"Opened",1);
    if(!candidate.guids.field||!medic_field_class(candidate.active,"BoolProperty")||!makeweak(candidate.type,type))return false;
    if(!kind){
        candidate.script_openable=catalog_field(type,"ScriptOpenable",1);
        const auto count=catalog_field(type,"CurrentActivationCount",4);
        if(!medic_field_class(candidate.script_openable,"BoolProperty")||!medic_field_class(count,"IntProperty"))return false;
        candidate.count=*api.offset(count);
    }else candidate.sound=world_actor_can_sound();
    binding=std::move(candidate);
    return true;
}
static bool world_mechanism_method(void* type,std::string_view name,Weak& out){
    const auto fn_type=api.weakget(&catalog_classes[catalog_function]);
    if(!fn_type)return false;
    for(unsigned depth=0;type&&depth<128;++depth,type=AnimU::field<void*>(type,64)){
        auto fn=AnimU::field<void*>(type,72);
        for(unsigned i=0;fn&&i<1024;++i,fn=AnimU::field<void*>(fn,40)){
            if(AnimU::field<void*>(fn,16)!=fn_type||AnimU::field<void*>(fn,32)!=type)continue;
            std::string leaf;
            if(AnimU::name_string(catalog_name_pool,AnimU::field<AnimU::NameKey>(fn,24),catalog_read,leaf)&&leaf==name)
                return !*api.parms(fn)&&makeweak(out,fn);
        }
    }
    return false;
}
static void world_mechanism_scan(void* pawn,WorldMechanismBinding& binding,unsigned kind){
    const auto type=api.weakget(&binding.type);
    binding.actors.clear();
    if(!type||!native_get_actors||!native_free)return;
    NativeActorArray entries{};
    native_get_actors(pawn,type,&entries);
    if(entries.data&&entries.count>=0&&entries.count<=8192&&entries.capacity>=entries.count){
        for(int i=0;i<entries.count;++i){
            const auto actor=entries.data[i];
            if(!actor||!api.isa(actor,type))continue;
            const auto keys=AnimU::field<WorldArray>(actor,binding.guids.offset);
            if(!keys.valid(32)||!keys.count)continue;
            const auto key=*static_cast<const WP::Guid*>(keys.data);
            if(key==WP::Guid{}||WP::personal(key))continue;
            std::string path,actor_class;
            if(!catalog_path(actor,path)||!path.starts_with("/Game/")||path.starts_with("/Game/UI/")||
               !catalog_path(AnimU::field<void*>(actor,16),actor_class))continue;
            WorldMechanismActor entry{};
            entry.key=key;
            const auto actor_type=AnimU::field<void*>(actor,16);
            const auto special=kind?ShrineGateKind::none:shrine_gate_kind(actor_class);
            if(special!=ShrineGateKind::none&&!bind_shrine_gate(actor_type,special,entry.shrine))continue;
            if(!kind&&!entry.shrine)world_mechanism_method(actor_type,"SetToOpenState",entry.saved_open);
            if(makeweak(entry.actor,actor)&&
               world_mechanism_method(actor_type,kind?"AnimateLeverPull":"OpenDoor",entry.enable)&&
               world_mechanism_method(actor_type,kind?"AnimateLeverReset":"CloseDoor",entry.disable))
                binding.actors.push_back(entry);
        }
    }
    if(entries.data)native_free(entries.data);
    std::ranges::sort(binding.actors,{},&WorldMechanismActor::key);
    // Ambiguous streamed copies cannot be assigned an authoritative actor.
    for(std::size_t i=0;i<binding.actors.size();){
        auto j=i+1;
        while(j<binding.actors.size()&&binding.actors[j].key==binding.actors[i].key)++j;
        if(j==i+1){++i;continue;}
        binding.actors.erase(binding.actors.begin()+i,binding.actors.begin()+j);
    }
}
static bool world_mechanisms_scan(void* pawn,double time){
    auto& state=world_mechanisms;
    if(time<state.next_scan)return false;
    for(unsigned i=world_mechanism_first;i<world_quest_first;++i)if(!world_input(i)){
        scan_world_inputs(2048);
        break;
    }
    bool bound=true;
    for(unsigned kind=0;kind<state.kinds.size();++kind){
        if(world_mechanism_bind(kind))world_mechanism_scan(pawn,state.kinds[kind],kind);
        else bound=false;
    }
    state.next_scan=time+(bound?2.:.25);
    ++state.scan;
    return true;
}
static void world_mechanisms_capture(void* pawn,double time,std::vector<WP::MechanismState>& out){
    world_mechanisms_scan(pawn,time);
    auto& state=world_mechanisms;
    const auto& save_binding=medic_runtime.save;
    const auto save=medic_active(save_binding,pawn);
    if(!state.shrine_boss.index&&std::ranges::any_of(state.kinds[0].actors,[](const auto& entry){
        return entry.shrine.kind==ShrineGateKind::shrine;
    })){
        const auto key=medic_name(L"ShrineBossKilled");
        if(medic_name_is(key,L"ShrineBossKilled"))state.shrine_boss=key;
    }
    const bool shrine_cleared=save&&state.shrine_boss.index&&medic_marker(save_binding,save,state.shrine_boss)>0;
    for(unsigned kind=0;kind<state.kinds.size();++kind){
        const auto& binding=state.kinds[kind];
        const auto type=api.weakget(&binding.type);
        for(const auto& item:binding.actors){
            const auto actor=api.weakget(&item.actor);
            if(!actor||!type||!api.isa(actor,type))continue;
            const auto count=kind?0:AnimU::field<int>(actor,binding.count);
            if(count<0||count>10000||out.size()>=WP::max_mechanisms)continue;
            auto active=std::optional<bool>(api.getbool(binding.active,actor,0));
            if(item.shrine)active=shrine_cleared&&item.shrine.kind==ShrineGateKind::shrine?
                std::optional<bool>(true):shrine_gate_open(actor,item.shrine,true);
            if(active)out.push_back({item.key,static_cast<WP::MechanismKind>(kind+1),static_cast<unsigned>(count),*active});
        }
    }
}
static void world_mechanism_apply(const WP::MechanismState& desired){
    const bool lever=desired.kind==WP::MechanismKind::lever;
    const auto& binding=world_mechanisms.kinds[lever];
    const auto type=api.weakget(&binding.type);
    const auto it=std::ranges::lower_bound(binding.actors,desired.key,{},&WorldMechanismActor::key);
    if(it==binding.actors.end()||it->key!=desired.key||!type)return;
    const auto actor=api.weakget(&it->actor);
    if(!actor||!api.isa(actor,type))return;
    if(!lever&&AnimU::field<int>(actor,binding.count)!=static_cast<int>(desired.activations))
        std::memcpy(static_cast<unsigned char*>(actor)+binding.count,&desired.activations,sizeof(desired.activations));
    if(it->shrine){
        if(apply_shrine_gate(actor,it->shrine,binding.active,api.weakget(&it->disable),desired.active))
            ++world_mechanism_doors;
        return;
    }
    if(api.getbool(binding.active,actor,0)==desired.active)return;
    if(!lever&&!desired.active&&!api.getbool(binding.script_openable,actor,0))return;
    const auto event=api.weakget(desired.active?&it->enable:&it->disable);
    if(!event)return;
    api.setbool(binding.active,actor,desired.active,0);
    api.event(actor,event,nullptr);
    if(lever){
        ++world_mechanism_levers;
        if(desired.active&&binding.sound)world_actor_sound(actor,"ActivateSound");
    }else ++world_mechanism_doors;
}
static void world_mechanisms_step(void* pawn,double time){
    auto& state=world_mechanisms;
    if(host_role||!world_progress_ready())return;
    const auto& desired=world_progress.authoritative.mechanisms;
    const auto refreshed=world_mechanisms_scan(pawn,time);
    if(refreshed||state.desired!=world_progress.applied){
        state.desired=world_progress.applied;
        state.cursor=state.saved_cursor=0;
    }
    for(unsigned i=0;i<6&&state.cursor<desired.size();++i)world_mechanism_apply(desired[state.cursor++]);
    const auto& doors=state.kinds[0];
    for(unsigned i=0;i<6&&state.saved_cursor<doors.actors.size();++i){
        const auto& door=doors.actors[state.saved_cursor++];
        if(!world_actor_saved_positive(world_progress.authoritative.actors,door.key))continue;
        const WP::MechanismState key{door.key,WP::MechanismKind::door};
        if(std::ranges::binary_search(desired,key,WP::mechanism_less))continue;
        const auto actor=api.weakget(&door.actor),open=api.weakget(&door.saved_open);
        if(actor&&open&&!api.getbool(doors.active,actor,0)){
            api.event(actor,open,nullptr);
            ++world_mechanism_doors;
        }
    }
}
