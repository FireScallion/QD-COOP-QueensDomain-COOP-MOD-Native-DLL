static constexpr std::array<unsigned,2> test_actions{13,166};
static std::array<U::WeakKey,test_actions.size()> source_actions{},rendered_actions{};
static std::array<float,test_actions.size()> first_action_positions{},last_action_positions{};
static U::WeakKey test_movement;
static Address test_velocity_field,test_mode_field,test_running_field;
static unsigned test_motion_stage;
static bool test_actions_started,test_actions_stopped,test_body_verified;
static constexpr const char* test_graph_path="/Game/QDCoopBody/ABP_QDBody.ABP_QDBody_C";

static Address test_anim_instance(Address component){
    const auto fn=resolve(body_methods[body_get_instance]);
    Address instance{};
    if(component&&fn&&at<std::uint16_t>(fn,182)==8)event(component,fn,&instance);
    return instance;
}

static bool bind_test_movement(Address pawn){
    const auto movement=object_field(pawn,"CharacterMovement");
    if(!movement)return false;
    const auto type=at<Address>(movement,16);
    test_velocity_field=field_of(type,"Velocity",12);
    test_mode_field=field_of(type,"MovementMode",1);
    test_running_field=field_of(at<Address>(pawn,16),"Running",1);
    remember(test_movement,movement);
    return test_velocity_field&&test_mode_field&&test_running_field&&resolve(test_movement)==movement;
}

static void hold_test_movement(Address pawn,float yaw){
    const auto movement=resolve(test_movement);
    if(!movement)return;
    const auto moving=test_motion_stage==1;
    const auto air=moving&&role_index==2;
    const auto run=moving&&role_index<2;
    const float forward=moving?(role_index==0?550.f:role_index==1?-500.f:200.f):0.f;
    const auto radians=yaw*(3.14159265f/180.f);
    const std::array<float,3> velocity{forward*std::cos(radians),forward*std::sin(radians),air?320.f:0.f};
    const std::uint8_t mode=air?3:1;
    std::memcpy(reinterpret_cast<void*>(movement+at<int>(test_velocity_field,76)),velocity.data(),sizeof(velocity));
    std::memcpy(reinterpret_cast<void*>(movement+at<int>(test_mode_field,76)),&mode,1);
    U::set_bool(reinterpret_cast<void*>(test_running_field),reinterpret_cast<void*>(pawn),run,0);
}

static Address load_test_asset(std::string_view path){
    const auto kismet=resolve(body_methods[body_kismet]),make=resolve(body_methods[body_make_path]);
    const auto ref=resolve(body_methods[body_ref]),load=resolve(body_methods[body_load]);
    const auto make_out=parameter_offset(make,"ReturnValue",24),ref_in=parameter_offset(ref,"SoftObjectPath",24);
    const auto ref_out=parameter_offset(ref,"ReturnValue",40),load_in=parameter_offset(load,"Asset",40);
    const auto load_out=parameter_offset(load,"ReturnValue",8);
    if(make_out<0||ref_in<0||ref_out<0||load_in<0||load_out<0)return 0;
    const std::wstring wide(path.begin(),path.end());
    const auto count=static_cast<int>(wide.size()+1);
    struct StringArgument {const wchar_t* data;int count,capacity;};
    alignas(16) Parameters path_args{},ref_args{},load_args{};
    if(!put(path_args,make,"PathString",StringArgument{wide.c_str(),count,count}))return 0;
    event(kismet,make,path_args.data());
    std::memcpy(ref_args.data()+ref_in,path_args.data()+make_out,24);
    event(kismet,ref,ref_args.data());
    std::memcpy(load_args.data()+load_in,ref_args.data()+ref_out,40);
    event(kismet,load,load_args.data());
    const auto asset=U::field<Address>(load_args.data(),load_out);
    return asset&&exact_path(asset,path.data())?asset:0;
}

static bool start_test_actions(Address pawn){
    const auto instance=test_anim_instance(object_field(pawn,"Arms"));
    const auto fn=resolve(body_methods[body_play]);
    if(!instance||!fn||at<std::uint16_t>(fn,182)!=28)return false;
    for(unsigned i=0;i<test_actions.size();++i){
        const auto asset=load_test_asset(P::clips[test_actions[i]].source);
        alignas(16) Parameters args{};
        if(!asset||!put(args,fn,"MontageToPlay",asset)||!put(args,fn,"InPlayRate",.15f)||
           !put(args,fn,"InTimeToStartMontageAt",i==0?1.1f:.15f))return false;
        event(instance,fn,args.data());
        if(U::field<float>(args.data(),24)<=0)return false;
        remember(source_actions[i],asset);
    }
    test_actions_started=true;
    return true;
}

static bool stop_test_actions(Address pawn){
    const auto instance=test_anim_instance(object_field(pawn,"Arms"));
    const auto fn=resolve(body_methods[body_stop]);
    if(!instance||!fn||at<std::uint16_t>(fn,182)!=16)return false;
    for(auto key:source_actions){
        const auto asset=resolve(key);
        alignas(16) Parameters args{};
        if(!asset||!put(args,fn,"InBlendOutTime",.05f)||!put(args,fn,"Montage",asset))return false;
        event(instance,fn,args.data());
    }
    test_actions_stopped=true;
    return true;
}

static std::string test_montage_path(unsigned index){
    const auto name=P::clips[test_actions[index]].name;
    return std::string("/Game/QDCoopBody/Montages/").append(name).append(".").append(name);
}

static void find_test_actions(){
    if(std::all_of(rendered_actions.begin(),rendered_actions.end(),[](auto key){return resolve(key)!=0;}))return;
    scan([](Address object){
        const auto leaf=name(object);
        for(unsigned i=0;i<test_actions.size();++i)
            if(leaf==P::clips[test_actions[i]].name&&exact_path(object,test_montage_path(i).c_str()))
                remember(rendered_actions[i],object);
    });
}

static bool test_montage_stopped(Address instance,Address asset){
    const auto fn=resolve(body_methods[body_stopped]);
    if(!fn||at<std::uint16_t>(fn,182)!=9)return false;
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&asset,8);
    event(instance,fn,args.data());
    return args[8]!=0;
}

static bool verify_action_positions(Address instance){
    const auto fn=resolve(body_methods[body_position]);
    if(!fn||at<std::uint16_t>(fn,182)!=12)return false;
    bool advanced=true;
    for(unsigned i=0;i<test_actions.size();++i){
        const auto asset=resolve(rendered_actions[i]);
        if(!asset||test_montage_stopped(instance,asset))return false;
        alignas(16) Parameters args{};
        std::memcpy(args.data(),&asset,8);
        event(instance,fn,args.data());
        const auto position=U::field<float>(args.data(),8);
        if(!std::isfinite(position)||position<(i==0?1.09f:.14f)||position>=P::clips[test_actions[i]].cooked)
            return false;
        if(!first_action_positions[i])first_action_positions[i]=position;
        last_action_positions[i]=position;
        advanced=advanced&&position-first_action_positions[i]>.025f;
    }
    return advanced;
}

static bool verify_avatar_graph(Address component,unsigned slot,unsigned step){
    const auto instance=test_anim_instance(component);
    if(!instance||!exact_path(at<Address>(instance,16),test_graph_path))return false;
    const auto type=at<Address>(instance,16);
    const auto number=[&](const char* field){
        const auto property=field_of(type,field,4);
        return property?at<float>(instance,at<int>(property,76)):-100.f;
    };
    if(step==0)return number("MoveWeight")>=0&&number("MoveWeight")<.05f&&number("JumpBlend")==0.f;
    const auto root=object_field(instance,"RootSequence"),left=object_field(instance,"LeftSequence");
    std::string root_path,left_path;
    if(!root||!left||!U::object_path(root,image+0x4A27F00,read_local,root_path)||
       !U::object_path(left,image+0x4A27F00,read_local,left_path)||
       !root_path.starts_with("/Game/QDCoopBody/Animations/")||!left_path.starts_with("/Game/QDCoopBody/Animations/"))
        return false;
    if(number("MoveWeight")<.7f||(slot==2?number("JumpBlend")!=1.f:number("JumpBlend")!=0.f))return false;
    if(slot<2&&number("RunBlend")<.7f)return false;
    if(slot>=2&&number("RunBlend")>.05f)return false;
    if((slot==1&&number("WalkRate")>=-.24f)||(slot!=1&&number("WalkRate")<=.24f))return false;
    return slot!=0||verify_action_positions(instance);
}
