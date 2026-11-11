struct WorldBarrierBinding {
    Weak actor,component;
    WP::BarrierKind kind{};
    std::string path,component_name;
    std::optional<bool> material_hidden;
};
struct WorldBarriers {
    std::vector<WorldBarrierBinding> components;
    Weak primitive;
    void *hidden{},*visible{},*actor_hidden{};
    WP::Revision revision;
    WP::Streaming streams;
    std::uint64_t scan{};
    std::size_t cursor{};
    double retry{},discover_until{},next_discovery{};
    int transform_at{},teleport_at{},body_at{},collision_at{};
};
static WorldBarriers world_barriers;
static void world_barriers_reset(){world_barriers={};}

static bool world_barrier_clear_masked_mode(void* component,void* owner){
    const auto get=world_input(world_barrier_actor_get);
    if(!get||build_state.load()!=1)return false;
    std::uint8_t actor_collision=255;
    api.event(owner,get,&actor_collision);
    if(actor_collision!=0)return false;

    // The stock component setter compares the owner-masked value before it
    // calls FBodyInstance::SetCollisionEnabled, so NoCollision is a no-op here.
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto setter=image+0x2aeefb0;
    const auto table=AnimU::field<const void*>(component,0);
    constexpr unsigned char entry[]{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,
                                    0x57,0x48,0x83,0xec,0x20,0x0f,0xb6,0x79,0x20};
    if(!table||AnimU::field<std::uintptr_t>(table,0x638)!=image+0x2b6db00||
       std::memcmp(reinterpret_cast<const void*>(setter),entry,sizeof(entry)))return false;
    reinterpret_cast<void(*)(void*,int,bool)>(setter)(static_cast<unsigned char*>(component)+world_barriers.body_at,0,true);
    return true;
}

static std::string_view world_barrier_component(WP::BarrierKind kind){
    return kind==WP::BarrierKind::bridge?"BlockerBox":"InteractBox";
}
static bool world_barriers_bind(double time){
    auto& state=world_barriers;
    const auto primitive=world_input(world_barrier_primitive);
    if(primitive&&api.weakget(&state.primitive)==primitive&&state.hidden&&state.visible&&state.actor_hidden)return true;
    if(time<state.retry)return false;
    state.retry=time+.35;
    for(unsigned id=world_barrier_primitive;id<=world_barrier_data_vector;++id)
        if(!world_input(id)){scan_world_inputs(4096);break;}
    const auto type=world_input(world_barrier_primitive),set=world_input(world_relative_set);
    const auto hidden=type?catalog_field(type,"bHiddenInGame",1):nullptr;
    const auto visible=type?catalog_field(type,"bVisible",1):nullptr;
    const auto body=type?reinterpret_cast<void*>(AnimU::find_inherited_property(
        reinterpret_cast<std::uintptr_t>(type),catalog_name_pool,"BodyInstance",catalog_read)):nullptr;
    const auto body_type=medic_field_class(body,"StructProperty")?AnimU::field<void*>(body,0x78):nullptr;
    std::string body_path;
    if(!body_type||!catalog_path(body_type,body_path)||body_path!="/Script/Engine.BodyInstance"||
       !property(body,344,*api.propertiesSize(type))||*api.offset(body)!=712)return false;
    const auto mode=catalog_field(body_type,"CollisionEnabled",1);
    const auto actor_type=world_input(world_barrier_actor);
    const auto actor_hidden=actor_type?catalog_field(actor_type,"bHidden",1):nullptr;
    if(!type||!set||*api.parms(set)>sizeof(FactoryParameters)||!medic_field_class(hidden,"BoolProperty")||
       !medic_field_class(visible,"BoolProperty")||!medic_field_class(actor_hidden,"BoolProperty")||
       !medic_field_class(mode,"ByteProperty")||*api.offset(mode)!=32||
       !medic_params(world_barrier_relative,48,{{"ReturnValue",48,0}})||
       !medic_params(world_barrier_hidden,2,{{"NewHidden",1,0},{"bPropagateToChildren",1,1}})||
       !medic_params(world_life_visibility,2,{{"bNewVisibility",1,0},{"bPropagateToChildren",1,1}})||
       !medic_params(world_component_collision,1,{{"NewType",1,0}})||
       !medic_params(world_barrier_actor_get,1,{{"ReturnValue",1,0}})||
       !medic_params(world_barrier_actor_set,1,{{"bNewActorEnableCollision",1,0}})||
       !medic_params(world_barrier_actor_hidden,1,{{"bNewHidden",1,0}})||
       !medic_params(world_barrier_actor_parts,24,{{"ComponentClass",8,0},{"ReturnValue",16,8}}))return false;
    const auto transform=factory_parameter(set,"NewTransform",48),teleport=factory_parameter(set,"bTeleport",1);
    if(transform<0||teleport<0||!makeweak(state.primitive,type))return false;
    state.hidden=hidden;
    state.visible=visible;
    state.actor_hidden=actor_hidden;
    state.transform_at=transform;
    state.teleport_at=teleport;
    state.body_at=*api.offset(body);
    state.collision_at=*api.offset(mode);
    return true;
}
static bool barrier_binding_less(const WorldBarrierBinding& left,const WorldBarrierBinding& right){
    if(left.kind!=right.kind)return left.kind<right.kind;
    return left.path==right.path?left.component_name<right.component_name:left.path<right.path;
}
static unsigned world_arena_types(){
    unsigned count{};
    for(unsigned id=world_barrier_necro;id<=world_barrier_aqua_entry;++id)count+=world_input(id)!=nullptr;
    return count;
}
static void world_arena_scan(void* pawn,void* type,void* primitive,WP::BarrierKind kind,
                             const std::vector<WorldBarrierBinding>& previous){
    const auto enumerate=world_input(world_barrier_actor_parts);
    if(!type||!primitive||!enumerate||!native_get_actors||!native_free)return;
    auto& entries=world_barriers.components;
    NativeActorArray actors{};
    native_get_actors(pawn,type,&actors);
    if(actors.data&&actors.count>=0&&actors.count<=128&&actors.capacity>=actors.count){
        for(int i=0;i<actors.count&&entries.size()<WP::max_barriers;++i){
            const auto actor=actors.data[i];
            if(!actor||!api.isa(actor,type)||actor_is_destroying(actor))continue;
            WorldBarrierBinding entry{};
            entry.kind=kind;
            if(!catalog_path(actor,entry.path)||!makeweak(entry.actor,actor))continue;
            if(kind==WP::BarrierKind::necro){
                const auto old=std::ranges::find_if(previous,[&](const auto& item){
                    return item.kind==kind&&item.component_name.empty()&&api.weakget(&item.actor)==actor;
                });
                if(old!=previous.end())entry.material_hidden=old->material_hidden;
            }
            entries.push_back(entry);
            alignas(16) FactoryParameters args{};
            factory_put(args,0,primitive);
            api.event(actor,enumerate,args.data());
            const auto components=AnimU::field<NativeActorArray>(args.data(),8);
            if(components.data&&components.count>=0&&components.count<=128&&components.capacity>=components.count)
                for(int j=0;j<components.count&&entries.size()<WP::max_barriers;++j){
                    const auto component=components.data[j];
                    if(!component||!api.isa(component,primitive))continue;
                    auto part=entry;
                    part.material_hidden.reset();
                    if(AnimU::name_string(catalog_name_pool,AnimU::field<AnimU::NameKey>(component,24),catalog_read,part.component_name)&&
                       !part.component_name.empty()&&makeweak(part.component,component))
                        entries.push_back(std::move(part));
                }
            if(components.data)native_free(components.data);
        }
    }
    if(actors.data)native_free(actors.data);
}
static bool world_barriers_refresh(void* pawn,double time,const WP::Streaming& streams){
    auto& state=world_barriers;
    if(state.streams!=streams){state.streams=streams;state.discover_until=time+4.;state.next_discovery=0;}
    const bool discover=time<state.discover_until&&time>=state.next_discovery;
    if(state.scan==world_mechanisms.scan&&!discover)return true;
    const auto known=world_arena_types();
    if(discover){
        if(known<6)scan_world_inputs(4096);
        state.next_discovery=time+.35;
    }
    const auto arenas=discover?world_arena_types():known;
    if(arenas==known&&state.scan==world_mechanisms.scan)return true;
    const auto& doors=world_mechanisms.kinds[0].actors;
    if((arenas||std::ranges::any_of(doors,[](const auto& door){return door.shrine.kind>=ShrineGateKind::bridge;}))&&
       !world_barriers_bind(time))return false;
    auto previous=std::move(state.components);
    state.components.clear();
    state.scan=world_mechanisms.scan;
    state.cursor=0;
    const auto primitive=api.weakget(&state.primitive);
    if(!primitive)return true;
    for(const auto& door:doors){
        if(door.shrine.kind<ShrineGateKind::bridge)continue;
        const auto actor=api.weakget(&door.actor);
        if(!actor)continue;
        WorldBarrierBinding entry{};
        entry.kind=door.shrine.kind==ShrineGateKind::bridge?WP::BarrierKind::bridge:WP::BarrierKind::castle;
        entry.component_name=world_barrier_component(entry.kind);
        const auto component=catalog_object_field(actor,entry.component_name);
        if(!component||!api.isa(component,primitive)||!catalog_path(actor,entry.path)||
           !makeweak(entry.actor,actor)||!makeweak(entry.component,component))continue;
        state.components.push_back(std::move(entry));
    }
    for(unsigned id=world_barrier_necro;id<=world_barrier_aqua_entry;++id)
        world_arena_scan(pawn,world_input(id),primitive,static_cast<WP::BarrierKind>(id-world_barrier_necro+1),previous);
    auto& entries=state.components;
    std::ranges::sort(entries,barrier_binding_less);
    for(std::size_t i=0;i<entries.size();){
        auto end=i+1;
        while(end<entries.size()&&!barrier_binding_less(entries[i],entries[end]))++end;
        if(end==i+1)++i;
        else entries.erase(entries.begin()+i,entries.begin()+end);
    }
    return true;
}
static bool world_barrier_read(void* component,WP::BarrierState& state){
    const auto get=world_input(world_barrier_relative);
    const auto& binding=world_barriers;
    if(!get||!binding.hidden||!binding.visible)return false;
    alignas(16) FactoryParameters args{};
    api.event(component,get,args.data());
    const auto transform=AnimU::field<std::array<float,12>>(args.data(),0);
    state.transform={transform[0],transform[1],transform[2],transform[3],transform[4],transform[5],
                     transform[6],transform[8],transform[9],transform[10]};
    // The public getters fold in the owner, hidden-in-game, or level visibility.
    state.hidden=api.getbool(binding.hidden,component,0);
    state.visible=api.getbool(binding.visible,component,0);
    state.collision=AnimU::field<std::uint8_t>(component,binding.body_at+binding.collision_at);
    return WP::valid(state);
}
static bool world_barrier_read_actor(void* actor,WP::BarrierState& value){
    const auto get=world_input(world_barrier_actor_get);
    if(!get||!world_barriers.actor_hidden)return false;
    value.hidden=api.getbool(world_barriers.actor_hidden,actor,0);
    std::uint8_t collision=255;
    api.event(actor,get,&collision);
    value.collision=collision;
    return WP::valid(value);
}
static void world_barriers_capture(void* pawn,double time,WP::Snapshot& snapshot){
    if(!world_barriers_refresh(pawn,time,snapshot.streaming))return;
    auto& out=snapshot.barriers;
    for(const auto& binding:world_barriers.components){
        const auto actor=api.weakget(&binding.actor),component=api.weakget(&binding.component);
        if(!actor||out.size()>=WP::max_barriers)continue;
        WP::BarrierState value{};
        value.kind=binding.kind;
        value.actor=binding.path;
        value.component=binding.component_name;
        if(value.component.empty()?!world_barrier_read_actor(actor,value):
           (!component||!world_barrier_read(component,value)))continue;
        out.push_back(std::move(value));
        ++world_barrier_captured;
        if(binding.kind<WP::BarrierKind::bridge)++world_arena_captured;
    }
    std::ranges::sort(out,WP::barrier_less);
}
static bool world_necro_material(void* actor,bool hidden){
    const auto mesh=catalog_object_field(actor,"StaticMeshComponent"),type=world_input(world_static_component);
    const auto bounds=world_input(world_barrier_bounds),scale=world_input(world_barrier_scale),
               location=world_input(world_elevator_location);
    const auto scalar=world_input(world_barrier_data_float),vector=world_input(world_barrier_data_vector);
    if(!mesh||!type||!api.isa(mesh,type)||!bounds||!scale||!location||!scalar||!vector||
       !medic_params(world_barrier_bounds,24,{{"Min",12,0},{"Max",12,12}})||
       !medic_params(world_barrier_scale,12,{{"ReturnValue",12,0}})||
       !medic_params(world_elevator_location,12,{{"ReturnValue",12,0}})||
       !medic_params(world_barrier_data_float,8,{{"DataIndex",4,0},{"Value",4,4}})||
       !medic_params(world_barrier_data_vector,16,{{"DataIndex",4,0},{"Value",12,4}}))return false;
    std::array<float,6> range{};
    std::array<float,3> mesh_scale{},position{};
    api.event(mesh,bounds,range.data());
    api.event(mesh,scale,mesh_scale.data());
    api.event(mesh,location,position.data());
    float squared{};
    for(unsigned i=0;i<position.size();++i){
        const auto delta=(range[i+3]-range[i])*mesh_scale[i];
        squared+=delta*delta;
    }
    const auto radius=(std::sqrt(squared)*.5f+2000.f)*1.2f;
    if(!std::isfinite(radius)||radius>10000000.f||
       !std::ranges::all_of(position,[](float value){return std::isfinite(value);}))return false;
    struct FloatArgument {int index;float value;};
    struct VectorArgument {int index;std::array<float,3> value;};
    FloatArgument radius_args{7,hidden?0.f:radius};
    VectorArgument position_args{32,position};
    api.event(mesh,scalar,&radius_args);
    api.event(mesh,vector,&position_args);
    return true;
}
static void world_barrier_apply_actor(WorldBarrierBinding& binding,void* actor,const WP::BarrierState& wanted){
    const auto hide=world_input(world_barrier_actor_hidden),set=world_input(world_barrier_actor_set);
    WP::BarrierState current{};
    current.kind=wanted.kind;
    current.actor=wanted.actor;
    if(!hide||!set||!world_barrier_read_actor(actor,current))return;
    bool changed=false;
    if(current.hidden!=wanted.hidden){
        std::uint8_t hidden=wanted.hidden;
        api.event(actor,hide,&hidden);
        changed=true;
    }
    if(current.collision!=wanted.collision){
        std::uint8_t collision=wanted.collision!=0;
        api.event(actor,set,&collision);
        changed=true;
    }
    if(binding.kind==WP::BarrierKind::necro&&binding.material_hidden!=wanted.hidden&&world_necro_material(actor,wanted.hidden)){
        binding.material_hidden=wanted.hidden;
        ++world_arena_necro_material;
    }
    if(changed){++world_barrier_applied;++world_arena_applied;}
}
static void world_necro_after_transform(void* actor,void* component,std::string_view path){
    if(catalog_object_field(actor,"StaticMeshComponent")!=component)return;
    auto& entries=world_barriers.components;
    const auto binding=std::ranges::find_if(entries,[&](const auto& entry){
        return entry.kind==WP::BarrierKind::necro&&entry.path==path&&entry.component_name.empty();
    });
    if(binding==entries.end())return;
    binding->material_hidden.reset();
    const auto hidden=api.getbool(world_barriers.actor_hidden,actor,0);
    if(world_necro_material(actor,hidden)){
        binding->material_hidden=hidden;
        ++world_arena_necro_material;
    }
}
static bool world_barrier_same_transform(const std::array<float,10>& first,const std::array<float,10>& second){
    float alignment{};
    for(unsigned i=0;i<4;++i)alignment+=first[i]*second[i];
    const auto sign=alignment<0.f?-1.f:1.f;
    for(unsigned i=0;i<4;++i)if(std::abs(first[i]-sign*second[i])>.0001f)return false;
    for(unsigned i=4;i<first.size();++i)if(std::abs(first[i]-second[i])>.005f)return false;
    return true;
}
static void world_barrier_apply(const WP::BarrierState& wanted){
    auto& state=world_barriers;
    const auto entry=std::ranges::find_if(state.components,[&](const auto& binding){
        return binding.kind==wanted.kind&&binding.path==wanted.actor&&binding.component_name==wanted.component;
    });
    if(entry==state.components.end())return;
    const auto actor=api.weakget(&entry->actor);
    if(!actor)return;
    if(wanted.component.empty()){world_barrier_apply_actor(*entry,actor,wanted);return;}
    const auto component=api.weakget(&entry->component);
    const auto set=world_input(world_relative_set),hide=world_input(world_barrier_hidden),
               show=world_input(world_life_visibility),collision=world_input(world_component_collision);
    if(!component||!set||!hide||!show||!collision)return;
    WP::BarrierState current{};
    current.kind=wanted.kind;
    current.actor=wanted.actor;
    current.component=wanted.component;
    if(!world_barrier_read(component,current))return;
    bool changed=false;
    if(!world_barrier_same_transform(current.transform,wanted.transform)){
        const auto& t=wanted.transform;
        const std::array<float,12> transform{t[0],t[1],t[2],t[3],t[4],t[5],t[6],0,t[7],t[8],t[9],0};
        alignas(16) FactoryParameters args{};
        factory_put(args,state.transform_at,transform);
        factory_put(args,state.teleport_at,std::uint8_t{1});
        api.event(component,set,args.data());
        if(wanted.kind==WP::BarrierKind::necro)world_necro_after_transform(actor,component,wanted.actor);
        changed=true;
    }
    if(current.hidden!=wanted.hidden){
        std::uint8_t args[]{static_cast<std::uint8_t>(wanted.hidden),0};
        api.event(component,hide,args);
        changed=true;
    }
    if(current.visible!=wanted.visible){
        std::uint8_t args[]{static_cast<std::uint8_t>(wanted.visible),0};
        api.event(component,show,args);
        changed=true;
    }
    if(current.collision!=wanted.collision){
        auto mode=static_cast<std::uint8_t>(wanted.collision);
        api.event(component,collision,&mode);
        if(!mode&&api.weakget(&entry->component)==component&&api.weakget(&entry->actor)==actor&&
           AnimU::field<std::uint8_t>(component,state.body_at+state.collision_at)!=mode)
            world_barrier_clear_masked_mode(component,actor);
        if(api.weakget(&entry->component)==component&&
           AnimU::field<std::uint8_t>(component,state.body_at+state.collision_at)==mode)changed=true;
    }
    if(changed){
        ++world_barrier_applied;
        if(wanted.kind<WP::BarrierKind::bridge)++world_arena_applied;
    }
}
static void world_barriers_step(void* pawn,double time){
    auto& state=world_barriers;
    if(host_role||!world_progress_ready()||!world_barriers_refresh(pawn,time,world_progress.authoritative.streaming))return;
    if(state.revision!=world_progress.applied){
        state.revision=world_progress.applied;
        state.cursor=0;
    }
    const auto& desired=world_progress.authoritative.barriers;
    for(unsigned i=0;i<4&&state.cursor<desired.size();++i)world_barrier_apply(desired[state.cursor++]);
}
