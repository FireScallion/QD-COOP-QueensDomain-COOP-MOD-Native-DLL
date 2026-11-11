// Only factory-owned replicas may expose native weapon queries. The display
// stays inert and the real replica remains frozen under host authority.
static constexpr std::uint8_t projectile_channel=14,hitbox_object=16,hitbox_trace=17;
static bool enable_native_hit_queries(Enrollment& entry){
    const auto primitive=world_input(world_hit_primitive),object_type=world_input(world_hit_object_type);
    const auto response=world_input(world_hit_response),all=world_input(world_hit_set_all);
    const auto set=world_input(world_hit_set_response),mode=world_input(world_component_collision);
    const auto profile=world_input(world_hit_profile);
    const auto overlap=world_input(world_component_overlap),actor_collision=factory_object(factory_collision);
    const auto components=factory_object(factory_audio),owner=factory_object(factory_owner);
    const auto actor=api.weakget(&entry.replica);
    const auto mesh=api.weakget(&entities[entry.offer.id-1].mesh);
    hit_queries_stage=1;
    if(!actor||!mesh||!primitive||!object_type||!response||!all||!set||!mode||!overlap||!profile||!actor_collision||
       !components||!owner||!native_free||!world_input(world_string_name)||!world_input(world_strings))return false;
    hit_queries_stage=2;
    if(*api.parms(object_type)!=1||*api.parms(response)!=2||*api.parms(all)!=1||*api.parms(set)!=2||
       *api.parms(mode)!=1||*api.parms(overlap)!=1||factory_parameter(response,"Channel",1)!=0||
       factory_parameter(response,"ReturnValue",1)!=1||factory_parameter(set,"Channel",1)!=0||
       factory_parameter(set,"NewResponse",1)!=1||*api.parms(profile)!=9||
       factory_parameter(profile,"InCollisionProfileName",8)!=0||factory_parameter(profile,"bUpdateOverlaps",1)!=8)return false;
    hit_queries_stage=3;
    alignas(16) FactoryParameters args{};
    factory_put(args,factory_offsets.audio_class,primitive);
    api.event(actor,components,args.data());
    const auto values=AnimU::field<NativeActorArray>(args.data(),factory_offsets.audio_result);
    struct HitComponent {void* object{};std::uint8_t trace{},projectile{};};
    std::array<HitComponent,64> shapes{};
    bool valid=values.count>0&&values.count<=static_cast<int>(shapes.size())&&values.capacity>=values.count&&values.data;
    unsigned enabled{};
    for(int i=0;valid&&i<values.count;++i){
        auto component=values.data[i];
        if(!component||!api.isa(component,primitive)){valid=false;break;}
        void* owned{};
        api.event(component,owner,&owned);
        if(owned!=actor){valid=false;break;}
        auto& shape=shapes[i];
        shape.object=component;
        if(component!=mesh)continue;
        std::uint8_t type{};
        api.event(component,object_type,&type);
        if(type!=hitbox_object)continue;
        std::uint8_t trace[]{hitbox_trace,0},projectile[]{projectile_channel,0};
        api.event(component,response,trace);
        api.event(component,response,projectile);
        if(trace[1]>2||projectile[1]>2){valid=false;break;}
        shape.trace=trace[1];
        shape.projectile=projectile[1];
        enabled+=shape.trace||shape.projectile;
    }
    if(values.data)native_free(values.data);
    if(!valid)return false;
    hit_queries_stage=4;
    if(!enabled)return false;
    alignas(16) FactoryParameters inert{};
    const auto no_collision=medic_name(L"NoCollision");
    if(!no_collision.index)return false;
    factory_put(inert,0,no_collision);
    for(int i=0;i<values.count;++i){
        const auto& shape=shapes[i];
        std::uint8_t disabled{};
        api.event(shape.object,overlap,&disabled);
        if(shape.object!=mesh){
            // SetCollisionEnabled(0) compares the effective mode, already zero
            // while the actor is disabled. Set the underlying component profile.
            api.event(shape.object,profile,inert.data());
            continue;
        }
        api.event(shape.object,all,&disabled);
        std::uint8_t trace[]{hitbox_trace,shape.trace},projectile[]{projectile_channel,shape.projectile},query_only=1;
        api.event(shape.object,set,trace);
        api.event(shape.object,set,projectile);
        api.event(shape.object,mode,&query_only);
    }
    own_set(actor,actor_collision,true);
    hit_queries_stage=5;
    return true;
}
static bool disable_native_hit_queries(unsigned index,void* actor){
    auto& entry=enrollments[index];
    if(!entry.hit_queries)return true;
    if(actor){
        const auto collision=factory_object(factory_collision);
        if(!collision)return false;
        own_set(actor,collision,false);
    }
    entry.hit_queries=false;
    return true;
}
