namespace D=qdcoop::enemy::death;
static std::array<D::Pending,E::capacity> deaths{};
static std::array<Weak,4> death_metadata{};
static void* death_kill{};static void* death_actor_tick{};static void* death_mesh_tick{};
static void* death_lizard_class{};static void* death_lizard_mode{};
static int death_dilation{};static bool death_ready,death_native_lizard;
static bool disable_native_hit_queries(unsigned index,void* actor);
static std::uint64_t death_applied{},death_duplicates{},death_rejected{},death_retries{},death_acks{};
static bool death_metadata_alive(){
 if(!death_ready)return false;
 for(unsigned i=0;i<(death_native_lizard?3u:4u);++i)
     if(!api.weakget(&death_metadata[i])){death_ready=false;return false;}
 return true;
}
static void* native_lizard_mode(void* actor){
    const auto actor_type=AnimU::field<void*>(actor,16);
    auto type=actor_type;
    for(unsigned depth=0;type&&depth<256;++depth){
        std::string path;
        if(catalog_path(type,path)&&path=="/Game/Enemies/Lizer_00/Default_Lizer_01.Default_Lizer_01_C")
            return catalog_field(actor_type,"DefaultEnemyMode",1);
        if(type==base_class)break;
        type=AnimU::field<void*>(type,64);
    }
    return nullptr;
}
static void death_set_lizard_mode(void* actor){
    if(!death_native_lizard){
        if(api.isa(actor,death_lizard_class))api.setbool(death_lizard_mode,actor,true,0);
        return;
    }
    if(const auto field=native_lizard_mode(actor))api.setbool(field,actor,true,0);
}
static bool death_receive(const D::Event& event){
 auto& e=entities[event.id-1];auto& d=deaths[event.id-1];
 if(d.terminal){if(d.event.generation!=event.generation)return false;++death_duplicates;return true;}
 if(!e.used||e.generation!=event.generation||!e.split||!death_metadata_alive()||native_boss_managed(event.id-1))return false;
 void* mesh{};auto actor=resolve(e,mesh);auto visual=api.weakget(&e.visual);auto pose=api.weakget(&e.visual_mesh);auto owner_fn=api.weakget(&e.owner_fn);
 if(!actor||!visual||!pose||!owner_fn)return false;
 void* owner{};api.event(pose,owner_fn,&owner);if(owner!=visual)return false;
 alignas(16) unsigned char alive[256]{};api.event(visual,getters[3].fn,alive);if(api.getbool(getters[3].ret,alive,0))return false;
 // Reserve before any side effect: reentrant Kill hooks and duplicate packets
 // cannot execute the presentation twice. Engine failure is not retried.
 if(!d.begin(event,0))return false;
 disable_native_hit_queries(event.id-1,actor);
 const float dilation=1,zero=0;std::memcpy(static_cast<char*>(actor)+death_dilation,&dilation,4);
 // Kill's Blueprint presentation does not itself guarantee HP=0. Terminal
 // event wins over every earlier positive-HP scalar snapshot.
 std::memcpy(static_cast<char*>(actor)+hp_at,&zero,4);
 unsigned char yes[2]{1,0},no[2]{0,0};
 api.event(visual,hide_fn,yes);api.event(pose,visible_fn,no);api.event(pose,death_mesh_tick,no);
 api.event(actor,hide_fn,no);api.event(actor,death_actor_tick,yes);api.event(mesh,visible_fn,yes);api.event(mesh,death_mesh_tick,yes);
 death_set_lizard_mode(actor);
 unsigned char args[2]{1,1};api.event(actor,death_kill,args);++death_applied;return true;
}
static bool configure_death(const std::array<std::uint64_t,6>& a,bool discover_lizard=false){
    auto ptr=[&](unsigned i){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(a[i]));};
    if(!ptr(0)||!ptr(1)||!ptr(2)||*api.parms(ptr(0))!=2||*api.parms(ptr(1))!=1||
       *api.parms(ptr(2))!=1||!property(ptr(3),4,*api.propertiesSize(base_class)))return false;
    if(discover_lizard?(ptr(4)||ptr(5)):(!ptr(4)||!property(ptr(5),1,*api.propertiesSize(ptr(4)))))
        return false;

    std::array<Weak,4> weak{};
    for(unsigned i=0;i<3;++i)if(!makeweak(weak[i],ptr(i)))return false;
    if(!discover_lizard&&!makeweak(weak[3],ptr(4)))return false;
    death_metadata=weak;
    death_kill=ptr(0);
    death_actor_tick=ptr(1);
    death_mesh_tick=ptr(2);
    death_dilation=*api.offset(ptr(3));
    death_lizard_class=ptr(4);
    death_lizard_mode=ptr(5);
    death_native_lizard=discover_lizard;
    death_ready=true;
    return true;
}

static bool queue_death(std::uintptr_t actor,bool* newly_queued=nullptr){
    if(newly_queued)*newly_queued=false;
    if(!actor||!host_role||!death_metadata_alive())return false;
    // The producer supplies the address on the game thread. Resolve the binding
    // before use and leave the terminal reservation in place on duplicate calls.
    for(unsigned i=0;i<E::capacity;++i){
        if(!entities[i].used||reinterpret_cast<std::uintptr_t>(api.weakget(&entities[i].actor))!=actor)continue;
        if(native_boss_managed(i))return true;
        auto& pending=deaths[i];
        if(pending.terminal)return true;
        if(retirement_requested(i))return false;
        const auto peers=active_peer_mask;
        const bool queued=pending.begin({i+1,entities[i].generation,1},peers);
        if(newly_queued)*newly_queued=queued;
        return queued;
    }
    return false;
}

static bool death_control(char op,char* p){
    auto number=[&](std::uint64_t& value){
        while(*p==' ')++p;
        if(*p<'0'||*p>'9')return false;
        char* end;
        value=std::strtoull(p,&end,10);
        if(end==p)return false;
        p=end;
        return true;
    };
    if(op=='D'){
        std::array<std::uint64_t,6> args{};
        for(auto& value:args)if(!number(value))return false;
        return !*p&&configure_death(args);
    }
    if(op=='K'){
        std::uint64_t actor{};
        return number(actor)&&!*p&&queue_death(static_cast<std::uintptr_t>(actor));
    }
    return false;
}
