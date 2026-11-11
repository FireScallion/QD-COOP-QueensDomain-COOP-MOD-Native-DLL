static constexpr auto elevator_fixture_path="/Game/RuinsLevel/Maps/300_LIB.300_LIB.PersistentLevel.Lib_ElevatorSmall_00BP";
static U::WeakKey elevator_fixture_actor;
struct TestElevatorLever {U::WeakKey lever;bool initial{};};
static std::vector<TestElevatorLever> elevator_fixture_levers;

static bool elevator_fixture_capture_levers(Address actor){
    const auto add=[](Address lever){
        if(!lever||std::ranges::any_of(elevator_fixture_levers,[&](const auto& entry){return resolve(entry.lever)==lever;}))return;
        const auto value=library_fixture_bool(lever,"DisableLockedMessage");
        if(!value)return;
        TestElevatorLever entry{};
        remember(entry.lever,lever);
        entry.initial=*value;
        elevator_fixture_levers.push_back(entry);
    };
    add(object_field(actor,"OnboardLever"));
    for(const auto key:{"StartLevers","EndLevers"}){
        const auto field=field_of(at<Address>(actor,16),key,16);
        if(!field)return false;
        struct Levers {const Address* data;int count,capacity;};
        const auto levers=at<Levers>(actor,at<int>(field,76));
        if(levers.count<0||levers.count>32||levers.capacity<levers.count||(levers.count&&!levers.data))return false;
        for(int i=0;i<levers.count;++i)add(levers.data[i]);
    }
    return !elevator_fixture_levers.empty()&&
        std::ranges::any_of(elevator_fixture_levers,[](const auto& entry){return !entry.initial;});
}
static bool elevator_fixture_lever_state(bool moving){
    return std::ranges::all_of(elevator_fixture_levers,[&](const auto& entry){
        return library_fixture_bool(resolve(entry.lever),"DisableLockedMessage")==std::optional<bool>{moving||entry.initial};
    });
}
static bool run_world_elevator_stage(Address engine){
    static bool streaming{},commanded{},stale_written{},restored{};
    static unsigned stage{};
    static float from{},to{};
    constexpr std::array proofs{L"elevator-ready.txt",L"elevator-up-running.txt",L"elevator-stale-held.txt",
                                L"elevator-up-arrived.txt",L"elevator-down-running.txt",L"elevator-down-arrived.txt"};
    if(stage==proofs.size())return all_player_markers(proofs.back());
    const auto gi=object_field(engine,"GameInstance"),save=gi?object_field(gi,"ActiveSaveSlot"):0;
    if(!save)return false;
    if(host&&!streaming){streaming=library_fixture_stream(engine,gi);return false;}
    if(!resolve(elevator_fixture_actor)){
        scan([](Address object){
            if(name(object)=="Lib_ElevatorSmall_00BP"&&exact_path(object,elevator_fixture_path))
                remember(elevator_fixture_actor,object);
        });
        return false;
    }
    const auto actor=resolve(elevator_fixture_actor),root=object_field(actor,"ElevatorPlatformRoot");
    const auto type=at<Address>(actor,16),guids=field_of(type,"GUIDs",16),mode=field_of(type,"LoopMode",1);
    const auto start=test_world_method(type,"StartElevatorTowardsEnd"),reverse=field_of(type,"PlayingInReverse",1);
    const auto position=root?test_world_method(at<Address>(root,16),"K2_GetComponentLocation"):0;
    if(!root||!guids||!mode||!start||!reverse||!position||!parameter(position,"ReturnValue",0,12)||
       at<std::uint16_t>(start,182)||at<int>(start,104)!=18||at<std::uint8_t>(actor,at<int>(mode,76))!=0)return false;
    const auto running=library_fixture_bool(actor,"Running"),at_end=library_fixture_bool(actor,"AtEnd"),
               is_reverse=library_fixture_bool(actor,"PlayingInReverse");
    if(!running||!at_end||!is_reverse)return false;
    const auto states=test_world_map(save,"SavableActorStates");
    struct Keys {const TestWP::Guid* data;int count,capacity;};
    const auto keys=at<Keys>(actor,at<int>(guids,76));
    if(!states.field||!keys.data||keys.count<1||keys.count>32||keys.capacity<keys.count)return false;
    std::array<float,3> root_position{};
    event(root,position,root_position.data());
    const auto z=root_position[2];
    const auto goal=test_world_find(states,keys.data[0]).value_or(-1);
    if(stage&&!all_player_markers(proofs[stage-1]))return false;
    const auto command=[&](bool reverse_direction){
        U::set_bool(reinterpret_cast<void*>(reverse),reinterpret_cast<void*>(actor),reverse_direction,0);
        event(actor,start,nullptr);
    };
    if(stage==0){
        if(*running||*at_end||std::abs(z-7197.713f)>.1f||!elevator_fixture_capture_levers(actor))return false;
        from=z;
    }else if(stage==1||stage==4){
        const bool down=stage==4;
        if(host&&!commanded){command(down);commanded=true;return false;}
        if(!*running||*is_reverse!=down||goal!=static_cast<int>(!down)||!elevator_fixture_lever_state(true)||
           (down?to-z:z-from)<40.f)return false;
    }else if(stage==2){
        if(host&&!stale_written){test_world_put(states,keys.data[0],0);stale_written=true;return false;}
        if(!*running||*is_reverse||goal!=0||!elevator_fixture_lever_state(true))return false;
    }else if(stage==3){
        if(host&&!restored){test_world_put(states,keys.data[0],1);restored=true;return false;}
        if(*running||!*at_end||goal!=1||!elevator_fixture_lever_state(false)||std::abs((z-from)-1170.f)>.1f)return false;
        to=z;
        commanded=false;
    }else if(*running||*at_end||goal!=0||!elevator_fixture_lever_state(false)||std::abs(z-from)>.1f)return false;
    char proof[150]{};
    const auto size=std::snprintf(proof,sizeof(proof),"z=%.3f running=%u reverse=%u end=%u goal=%d levers=%zu\n",
                                  z,*running,*is_reverse,*at_end,goal,elevator_fixture_levers.size());
    if(size>0)write_file(marker(role_index,proofs[stage]).c_str(),proof,static_cast<unsigned>(size));
    ++stage;
    return false;
}
