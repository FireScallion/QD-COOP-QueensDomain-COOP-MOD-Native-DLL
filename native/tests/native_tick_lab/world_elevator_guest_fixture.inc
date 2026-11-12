static bool run_world_elevator_guest_stage(Address engine,ULONGLONG now){
    constexpr std::array proofs{L"elevator-intent-ready.txt",L"elevator-intent-refused.txt",
                                L"elevator-intent-up.txt",L"elevator-intent-down.txt"};
    constexpr float lower=7197.713f,upper=8367.713f;
    static unsigned stage{};
    static bool streaming{},commanded{},moved{};
    static ULONGLONG moved_at{},delay_for_index{},rediscovered_at{};
    static bool key_hidden{},key_restored{},caught_motion{};
    static std::array<std::int32_t,4> elevator_key{};
    wchar_t late_flag[2]{};
    const bool late=GetEnvironmentVariableW(L"QD_WORLD_ELEVATOR_LATE_KEY",late_flag,2)==1&&late_flag[0]==L'1';
    if(stage==proofs.size())return all_player_markers(proofs.back());
    const auto gi=object_field(engine,"GameInstance"),pawn=resolve(held_pawn);
    if(!gi||!pawn)return false;
    if(host&&!streaming){streaming=library_fixture_stream(engine,gi);return false;}
    if(!resolve(elevator_fixture_actor)){
        scan([](Address object){
            if(name(object)=="Lib_ElevatorSmall_00BP"&&exact_path(object,elevator_fixture_path))
                remember(elevator_fixture_actor,object);
        });
        return false;
    }
    const auto actor=resolve(elevator_fixture_actor),root=object_field(actor,"ElevatorPlatformRoot");
    const auto type=at<Address>(actor,16),start=test_world_method(type,"StartElevatorTowardsEnd"),
               reverse=field_of(type,"PlayingInReverse",1);
    const auto position=root?test_world_method(at<Address>(root,16),"K2_GetComponentLocation"):0;
    const auto running=library_fixture_bool(actor,"Running"),end=library_fixture_bool(actor,"AtEnd");
    if(!root||!start||!reverse||!position||!parameter(position,"ReturnValue",0,12)||!running||!end||
       at<std::uint16_t>(start,182)||at<int>(start,104)!=18)return false;
    std::array<float,3> platform{};
    event(root,position,platform.data());
    const auto at_z=[&](float target){return std::abs(platform[2]-target)<=.1f;};
    const auto command=[&](bool direction){
        U::set_bool(reinterpret_cast<void*>(reverse),reinterpret_cast<void*>(actor),direction,0);
        event(actor,start,nullptr);
    };
    if(stage&&!all_player_markers(proofs[stage-1]))return false;
    if((stage==0||stage==3)&&role_index==1&&!moved){
        if(!at_z(stage==0?lower:upper))return false;
        held_pose.position={platform[0]+30.f,platform[1],platform[2]+100.f};
        if(!apply_test_pose(pawn,held_pose))return false;
        moved_at=now;
        moved=true;
        return false;
    }
    if(role_index==1&&moved&&now-moved_at<700)return false;
    const auto status=read_native_status(core.status);
    if(status.empty())return false;
    const auto sent=counter(status.c_str(),"\"elevator_intents\":");
    const auto accepted=counter(status.c_str(),host?"\"elevator_host_accepts\":":"\"elevator_guest_accepts\":");
    const auto refused=counter(status.c_str(),host?"\"elevator_host_refusals\":":"\"elevator_guest_refusals\":");
    const auto replace_key=[&](bool hide){
        const auto property=field_of(type,"GUIDs",16);
        if(!property)return false;
        struct GuidArray {Address data;int count,capacity;};
        const auto row=at<GuidArray>(actor,at<unsigned>(property,76));
        if(!row.data||row.count<1||row.capacity<row.count||row.capacity>32)return false;
        if(hide)elevator_key=at<decltype(elevator_key)>(row.data,0);
        if(elevator_key==decltype(elevator_key){})return false;
        const auto value=hide?decltype(elevator_key){}:elevator_key;
        std::memcpy(reinterpret_cast<void*>(row.data),value.data(),sizeof(value));
        return true;
    };
    static ULONGLONG next_sample{};
    if(now>=next_sample){
        next_sample=now+100;
        if(auto file=_wfopen(marker(role_index,L"elevator-motion-samples.txt").c_str(),L"ab")){
            std::fprintf(file,"%llu stage=%u z=%.3f running=%u end=%u\n",
                static_cast<unsigned long long>(now),stage,platform[2],*running,*end);
            std::fclose(file);
        }
    }
    if(stage==0){
        if(*running||*end||!at_z(lower)||sent||accepted||refused)return false;
    }else if(stage==1){
        if(role_index==2&&!commanded){command(false);commanded=true;return false;}
        if(*running||*end!=(role_index==2)||!at_z(role_index==2?upper:lower)||accepted||
           sent!=static_cast<unsigned>(role_index==2)||refused!=static_cast<unsigned>(host||role_index==2))return false;
        if(late&&role_index==3&&!key_hidden){
            if(!replace_key(true))return false;
            key_hidden=true;
            write_file(marker(role_index,L"elevator-index-withheld.txt").c_str(),"ok",2);
        }
    }else{
        const bool down=stage==3;
        if(late&&!down){
            if(role_index==1&&!commanded){
                if(!exists(marker(3,L"elevator-index-withheld.txt")))return false;
                if(!delay_for_index)delay_for_index=now+800;
                if(now<delay_for_index)return false;
            }
            if(host&&*running&&platform[2]>=lower+300.f&&!exists(marker(0,L"elevator-mid-motion.txt")))
                write_file(marker(0,L"elevator-mid-motion.txt").c_str(),"ok",2);
            if(role_index==3&&!key_restored){
                if(!exists(marker(0,L"elevator-mid-motion.txt")))return false;
                if(!at_z(lower)||!counter(status.c_str(),"\"elevator_motion_received\":")||!replace_key(false))return false;
                key_restored=true;
                rediscovered_at=now;
                return false;
            }
            if(role_index==3&&!caught_motion){
                if(!*running||platform[2]<lower+285.f||platform[2]>=upper||now-rediscovered_at>1300)return false;
                char evidence[128]{};
                const auto size=std::snprintf(evidence,sizeof(evidence),"z=%.3f delay_ms=%llu\n",platform[2],now-rediscovered_at);
                if(size<=0)return false;
                write_file(marker(role_index,L"elevator-mid-motion-restored.txt").c_str(),evidence,static_cast<unsigned>(size));
                caught_motion=true;
            }
        }
        if(role_index==1&&!commanded){command(down);commanded=true;return false;}
        const auto expected=stage-1;
        if(*running||*end==down||!at_z(down?lower:upper)||
           sent!=(role_index==1?expected:static_cast<unsigned>(role_index==2))||
           accepted!=((host||role_index==1)?expected:0)||
           refused!=static_cast<unsigned>(host||role_index==2))return false;
    }
    char proof[200]{};
    const auto size=std::snprintf(proof,sizeof(proof),"z=%.3f end=%u intents=%llu accepted=%llu refused=%llu\n",
        platform[2],*end,static_cast<unsigned long long>(sent),static_cast<unsigned long long>(accepted),
        static_cast<unsigned long long>(refused));
    if(size>0)write_file(marker(role_index,proofs[stage]).c_str(),proof,static_cast<unsigned>(size));
    ++stage;
    commanded=false;
    if(stage==3)moved=false;
    return false;
}
