static bool damageable_registration(void* method){
    if(!method||*api.parms(method)!=8)return false;
    const auto argument=AnimU::field<void*>(method,80);
    return damage_parameter(argument)&&medic_field_class(argument,"ObjectProperty")&&*api.size(argument)==8&&*api.offset(argument)==0;
}
static bool destroy_damage_proxy(NativeDamagePeer& state){
    const auto actor=api.weakget(&state.proxy),game_state=api.weakget(&state.damageable_world);
    if(actor&&state.armed&&game_state){
        const auto remove=world_input(world_threat_unregister);
        if(!damageable_registration(remove))return false;
        auto argument=actor;
        api.event(game_state,remove,&argument);
        ++combat_proxies_unregistered;
    }
    state.armed=false;
    return destroy_owned_actor(state.proxy,false);
}
static bool arm_threat_proxy(NativeDamagePeer& state,void* actor){
    const auto gs_type=world_input(world_threat_gs_type),get_state=world_input(world_threat_get_gs);
    const auto add=world_input(world_threat_register),remove=world_input(world_threat_unregister);
    const auto gameplay=factory_object(factory_gameplay),primitive=world_input(world_hit_primitive);
    const auto components=factory_object(factory_audio),owner=factory_object(factory_owner);
    const auto profile=world_input(world_hit_profile),mode=world_input(world_component_collision),overlap=world_input(world_component_overlap);
    const auto response=world_input(world_hit_set_response);
    const auto capsule=catalog_object_field(actor,"CapsuleComponent");
    const auto type=AnimU::field<void*>(actor,16);
    const auto dead=catalog_field(type,"IsDead",1),health=catalog_field(type,"Health",4);
    combat_proxy_stage=1;
    if(!gs_type||!get_state||!gameplay||!primitive||!components||!owner||!profile||!mode||!overlap||!response||!capsule||
       !native_free||!world_input(world_strings)||!world_input(world_string_name))return false;
    combat_proxy_stage=2;
    if(!medic_field_class(dead,"BoolProperty")||!medic_field_class(health,"FloatProperty"))return false;
    combat_proxy_stage=3;
    if(!damageable_registration(add)||!damageable_registration(remove))return false;
    combat_proxy_stage=4;
    if(
       *api.parms(get_state)!=16||factory_parameter(get_state,"WorldContextObject",8)!=0||
       factory_parameter(get_state,"ReturnValue",8)!=8||
       *api.parms(mode)!=1||*api.parms(overlap)!=1||*api.parms(profile)!=9||*api.parms(response)!=2||
       factory_parameter(profile,"InCollisionProfileName",8)!=0||factory_parameter(profile,"bUpdateOverlaps",1)!=8||
       factory_parameter(response,"Channel",1)!=0||factory_parameter(response,"NewResponse",1)!=1)return false;
    combat_proxy_stage=5;
    std::array<void*,2> state_args{actor,nullptr};
    api.event(gameplay,get_state,state_args.data());
    const auto gs=state_args[1];
    const bool damageable=gs&&api.isa(gs,gs_type);
    if(damageable&&!makeweak(state.damageable_world,gs))return false;
    combat_proxy_stage=6;
    alignas(16) FactoryParameters args{};
    factory_put(args,factory_offsets.audio_class,primitive);
    api.event(actor,components,args.data());
    const auto found=AnimU::field<NativeActorArray>(args.data(),factory_offsets.audio_result);
    std::array<void*,64> shapes{};
    const bool valid=found.data&&found.count>0&&found.count<=static_cast<int>(shapes.size())&&found.capacity>=found.count;
    if(valid)std::copy_n(found.data,found.count,shapes.begin());
    if(found.data)native_free(found.data);
    if(!valid||std::find(shapes.begin(),shapes.begin()+found.count,capsule)==shapes.begin()+found.count)return false;
    for(int i=0;i<found.count;++i){
        if(!shapes[i]||!api.isa(shapes[i],primitive))return false;
        void* parent{};
        api.event(shapes[i],owner,&parent);
        if(parent!=actor)return false;
    }
    const auto name=medic_name(L"NoCollision");
    if(!name.index)return false;
    args={};
    factory_put(args,0,name);
    for(int i=0;i<found.count;++i){
        const auto shape=shapes[i];
        std::uint8_t disabled{};
        api.event(shape,overlap,&disabled);
        if(shape!=capsule)api.event(shape,profile,args.data());
    }
    // UE4 query-only capsule and the original preview3 target responses.
    std::uint8_t query=1;
    api.event(capsule,mode,&query);
    std::uint8_t pawn[]{2,1},visibility[]{3,0},camera[]{4,0};
    api.event(capsule,response,pawn);
    api.event(capsule,response,visibility);
    api.event(capsule,response,camera);
    if(damageable){
        api.event(gs,add,&actor);
        ++combat_proxies_registered;
    }
    api.setbool(dead,actor,true,0);
    state.dead_property=dead;
    state.proxy_health=*api.offset(health);
    combat_proxy_stage=7;
    ++combat_proxies_armed;
    return true;
}
static void update_threat_proxy(NativeDamagePeer& state,const LifePeer& remote,double time){
    const auto actor=api.weakget(&state.proxy),collision=factory_object(factory_collision);
    if(!actor||!collision)return;
    if(!state.armed){
        if(time<state.next_arm)return;
        state.next_arm=time+.5;
        if(!world_input(world_threat_register))scan_world_inputs(4096);
        state.armed=arm_threat_proxy(state,actor);
        if(!state.armed)return;
    }
    const bool standing=fresh_life(remote,LifePhase::standing,time)&&remote.health>0;
    if(standing==state.standing&&(!standing||state.health_received==remote.received))return;
    const auto value=standing?remote.health:0.f;
    std::memcpy(static_cast<unsigned char*>(actor)+state.proxy_health,&value,sizeof(value));
    state.health_received=remote.received;
    if(state.standing!=standing){
        api.setbool(state.dead_property,actor,!standing,0);
        own_set(actor,collision,standing);
        state.standing=standing;
    }
}
