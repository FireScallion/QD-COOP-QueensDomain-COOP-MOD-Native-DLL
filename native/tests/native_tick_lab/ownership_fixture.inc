// Read back the actual replica state independently of the core's saved state.
static constexpr std::array<const char*,24> ownership_paths{
    "/Script/Engine.Actor.IsActorTickEnabled",
    "/Script/Engine.Actor.GetActorEnableCollision",
    "/Script/Engine.Actor.SetActorEnableCollision",
    "/Script/Engine.ActorComponent.IsComponentTickEnabled",
    "/Script/Engine.Pawn.GetController",
    "/Script/Engine.MovementComponent.StopMovementImmediately",
    "/Script/Engine.Controller.StopMovement",
    "/Script/AIModule.BrainComponent.StopLogic",
    "/Script/AIModule.BrainComponent.RestartLogic",
    "/Script/AIModule.BrainComponent.IsRunning",
    "/Script/Engine.ActorComponent",
    "/Script/Engine.MovementComponent",
    "/Script/AIModule.AIController",
    "/Script/AIModule.BrainComponent",
    nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,
    "/Script/Engine.AudioComponent",
    "/Script/Engine.AudioComponent.SetPaused",
    "/Script/Engine.AudioComponent.GetPlayState"
};
static std::array<U::WeakKey,ownership_paths.size()> ownership_keys{};
static unsigned ownership_fixture_step,ownership_before,ownership_held,ownership_after,ownership_audio;
static unsigned ownership_audio_active_before,ownership_audio_paused_held;
static float dilation_before,dilation_held,dilation_after;
static bool ownership_restored,stale_release_rejected;
static bool fixture_controller,fixture_brain,fixture_uro,fixture_los;

static void discover_ownership(Address object,std::string_view leaf){
    for(unsigned i=0;i<ownership_paths.size();++i){
        if(!ownership_paths[i])continue;
        const std::string_view path=ownership_paths[i];
        if(leaf==path.substr(path.find_last_of('.')+1)&&exact_path(object,ownership_paths[i]))
            remember(ownership_keys[i],object);
    }
}
static bool ownership_inputs_ready(){
    for(unsigned i=0;i<ownership_paths.size();++i)
        if(ownership_paths[i]&&!resolve(ownership_keys[i]))return false;
    return true;
}
static unsigned char query_byte(Address object,Address function){
    unsigned char result{};
    if(object&&function)event(object,function,&result);
    return result;
}
static void set_flag(Address object,Address function,bool enabled){
    unsigned char value=enabled?1:0;
    if(object&&function)event(object,function,&value);
}
static bool get_flag(Address object,Address field){
    return object&&field&&U::get_bool(reinterpret_cast<void*>(field),reinterpret_cast<void*>(object),0);
}
static void set_field_flag(Address object,Address field,bool value){
    if(object&&field)U::set_bool(reinterpret_cast<void*>(field),reinterpret_cast<void*>(object),value,0);
}

struct OwnershipView {
    unsigned flags{};
    float dilation{};
    std::array<unsigned char,16> audio{};
    bool operator==(const OwnershipView&)const=default;
};

static bool fixture_ownership(Address actor,Address mesh){
    std::array<std::uint64_t,24> args{};
    for(unsigned i=0;i<args.size();++i)if(ownership_paths[i])args[i]=resolve(ownership_keys[i]);
    const auto actor_type=at<Address>(actor,16);
    const auto los=object_field(actor,"EnemyLineOfSight");
    const auto movement=object_field(actor,"CharacterMovement");
    const auto uro=object_field(actor,"ActorUROManager");
    Address controller{};
    event(actor,args[4],&controller);
    const auto brain=controller?object_field(controller,"BrainComponent"):0;
    fixture_controller=controller!=0;
    fixture_brain=brain!=0;
    fixture_uro=uro!=0;
    fixture_los=los!=0;
    args[14]=los?at<Address>(los,16):0;
    args[15]=field_of(actor_type,"CharacterMovement",8);
    args[16]=field_of(actor_type,"ActorUROManager",8);
    args[17]=field_of(actor_type,"EnemyLineOfSight",8);
    args[18]=field_of(actor_type,"bIsAIDisabled",1);
    args[19]=field_of(args[12],"BrainComponent",8);
    args[20]=field_of(args[14],"bEnabled",1);
    const auto dilation=field_of(actor_type,"CustomTimeDilation",4);
    ownership_fixture_step=1;
    if(!dilation||!std::all_of(args.begin(),args.end(),[](auto value){return value!=0;})||
       !core.configure_ownership(args.data(),static_cast<unsigned>(args.size())))return false;

    // This one-time lab scan provides the actual owned audio components.
    // The gameplay caller can supply the list from its component discovery.
    std::array<Address,16> audio{};
    const auto array=at<U::ObjectArray>(image+0x4A64260,0);
    const auto owner_fn=resolve(methods[get_owner]);
    ownership_fixture_step=2;
    if(!U::object_array_shape(array)||!owner_fn)return false;
    for(int index=0;index<array.num_elements;++index){
        const auto chunk=at<Address>(array.chunks,(index/U::objects_per_chunk)*8);
        if(!chunk)continue;
        const auto item=at<U::ObjectItem>(chunk,(index%U::objects_per_chunk)*sizeof(U::ObjectItem));
        if(!item.object||(item.flags&0x30000000u)||(at<unsigned>(item.object,8)&0x30u)||
           !U::is_a(reinterpret_cast<void*>(item.object),reinterpret_cast<void*>(args[21])))continue;
        Address owner{};
        event(item.object,owner_fn,&owner);
        if(owner!=actor)continue;
        if(ownership_audio==audio.size())return false;
        audio[ownership_audio++]=item.object;
    }

    const auto dilation_address=actor+static_cast<unsigned>(at<int>(dilation,76));
    const auto set_actor_tick=resolve(methods[actor_tick]);
    const auto set_component_tick=resolve(methods[component_tick]);
    const auto hide=metadata.arguments[40];
    const auto visibility=metadata.arguments[41];
    set_field_flag(actor,args[18],false);
    set_field_flag(los,args[20],true);
    set_flag(actor,set_actor_tick,true);
    set_flag(actor,args[2],true);
    set_flag(actor,hide,false);
    set_flag(mesh,set_component_tick,true);
    set_flag(movement,set_component_tick,true);
    set_flag(controller,set_actor_tick,true);
    set_flag(uro,set_component_tick,false);
    set_flag(los,set_component_tick,true);
    unsigned char shown[2]{1,0};
    event(mesh,visibility,shown);
    const float original_dilation=.75f;
    std::memcpy(reinterpret_cast<void*>(dilation_address),&original_dilation,sizeof(original_dilation));

    auto snapshot=[&](){
        OwnershipView result{};
        alignas(16) Parameters visible{};
        event(mesh,metadata.arguments[18],visible.data());
        const bool flags[]{
            get_flag(actor,args[18]),get_flag(actor,metadata.arguments[7]),
            query_byte(actor,args[1])!=0,query_byte(actor,args[0])!=0,
            query_byte(mesh,args[3])!=0,
            U::get_bool(reinterpret_cast<void*>(metadata.arguments[19]),visible.data(),0),
            query_byte(movement,args[3])!=0,query_byte(controller,args[0])!=0,
            query_byte(brain,args[9])!=0,query_byte(uro,args[3])!=0,
            query_byte(los,args[3])!=0,get_flag(los,args[20])
        };
        for(unsigned i=0;i<std::size(flags);++i)if(flags[i])result.flags|=1u<<i;
        result.dilation=at<float>(dilation_address,0);
        for(unsigned i=0;i<ownership_audio;++i)result.audio[i]=query_byte(audio[i],args[23]);
        return result;
    };
    const auto before=snapshot();
    ownership_before=before.flags;
    dilation_before=before.dilation;
    for(unsigned i=0;i<ownership_audio;++i)
        ownership_audio_active_before+=before.audio[i]!=1&&before.audio[i]!=2;
    ownership_fixture_step=3;
    if(!core.take_ownership(1,1,audio.data(),ownership_audio))return false;
    const auto held=snapshot();
    ownership_held=held.flags;
    dilation_held=held.dilation;
    if(held.flags!=3||held.dilation!=0.f)return false;
    for(unsigned i=0;i<ownership_audio;++i){
        if(held.audio[i]!=1&&held.audio[i]!=2)return false;
        ownership_audio_paused_held+=held.audio[i]==2;
    }

    ownership_fixture_step=4;
    if(!core.take_ownership(1,1,audio.data(),ownership_audio)||snapshot()!=held)return false;
    stale_release_rejected=!core.release_ownership(1,2)&&snapshot()==held;
    if(!stale_release_rejected||!core.release_ownership(1,1))return false;
    const auto after=snapshot();
    ownership_after=after.flags;
    dilation_after=after.dilation;
    ownership_restored=after==before;
    if(!ownership_restored)return false;

    ownership_fixture_step=5;
    const auto visual=resolve(display);
    const auto pose=visual?object_field(visual,"SkeletalMeshComponent"):0;
    if(!pose||!core.register_replica(&offer,reinterpret_cast<void*>(actor),reinterpret_cast<void*>(mesh))||
       !core.bind_display(1,1,reinterpret_cast<void*>(visual),reinterpret_cast<void*>(pose),reinterpret_cast<void*>(owner_fn))||
       !core.take_ownership(1,1,audio.data(),ownership_audio)||snapshot()!=held)return false;
    ownership_fixture_step=6;
    return true;
}
