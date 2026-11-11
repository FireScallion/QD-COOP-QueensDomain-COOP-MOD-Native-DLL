static constexpr std::string_view fixture_shrine_boss_class=
    "/Game/Enemies/Boner_00/Skeletons24/ShrineBoss/Boner_Shrineboss_BP.Boner_Shrineboss_BP_C";
static U::WeakKey fixture_boss_spawner,fixture_boss_actor;

static bool run_world_boss_stage(){
    static unsigned stage{};
    static bool positioned{},spawned{},commanded{};
    constexpr std::array proofs{L"boss-phase-ready.txt",L"boss-phase-defeated.txt"};
    if(stage==proofs.size())return all_player_markers(proofs.back());
    const auto pawn=resolve(held_pawn);
    if(!pawn)return false;
    if(!positioned){
        held_pose.position={18265.f+3.f*role_index,-7750.f,80.f};
        positioned=apply_test_pose(pawn,held_pose);
        return false;
    }
    if((host&&!resolve(fixture_boss_spawner))||(!host&&!resolve(fixture_boss_actor))){
        scan([](Address object){
            if(host){
                if(name(object)=="ShrinebossSpawner"&&exact_path(object,
                    "/Game/RuinsLevel/Maps/100_GR.100_GR.PersistentLevel.ShrinebossSpawner"))remember(fixture_boss_spawner,object);
            }else if(!(at<unsigned>(object,8)&0x10u)){
                const auto type=at<Address>(object,16);
                if(name(type)=="Boner_Shrineboss_BP_C"&&exact_path(type,fixture_shrine_boss_class.data())){
                    const auto dilation=field_of(type,"CustomTimeDilation",4);
                    if(dilation&&at<float>(object,at<int>(dilation,76))==0)remember(fixture_boss_actor,object);
                }
            }
        });
    }
    if(host&&!spawned){
        const auto spawner=resolve(fixture_boss_spawner);
        if(!spawner)return false;
        const auto type=at<Address>(spawner,16),spawn=test_world_method(type,"SpawnEnemyWithConfigApplied"),
                   activate=test_world_method(type,"ActivateEnemy");
        if(!spawn||!activate||!parameter(spawn,"ReturnValue",0,8)||at<std::uint16_t>(activate,182))return false;
        auto actor=object_field(spawner,"ChildActor");
        if(!actor)event(spawner,spawn,&actor);
        if(!actor||!exact_path(at<Address>(actor,16),fixture_shrine_boss_class.data()))return false;
        remember(fixture_boss_actor,actor);
        event(spawner,activate,nullptr);
        spawned=true;
        return false;
    }
    const auto actor=resolve(fixture_boss_actor);
    if(!actor)return false;
    const auto type=at<Address>(actor,16),box=object_field(actor,"Box");
    const auto dead=library_fixture_bool(actor,"bDead"),suspended=library_fixture_bool(actor,"DeathIsSuspended"),
               attack=library_fixture_bool(actor,"bIsAttacking");
    const auto tick=test_world_method(type,"IsActorTickEnabled"),dilation=field_of(type,"CustomTimeDilation",4);
    const auto collision=box?test_world_method(at<Address>(box,16),"GetCollisionEnabled"):0;
    if(!dead||!suspended||!attack||!tick||!dilation||!collision||!parameter(tick,"ReturnValue",0,1)||
       !parameter(collision,"ReturnValue",0,1))return false;
    std::uint8_t ticks{},mode=255;
    event(actor,tick,&ticks);
    event(box,collision,&mode);
    const auto speed=at<float>(actor,at<int>(dilation,76));
    const auto report="stage="+std::to_string(stage)+" dead="+std::to_string(*dead)+" suspended="+std::to_string(*suspended)+
        " attacking="+std::to_string(*attack)+" box="+std::to_string(mode)+" tick="+std::to_string(ticks)+" dilation="+std::to_string(speed)+"\n";
    write_file(marker(role_index,L"boss-phase-progress.txt").c_str(),report.data(),static_cast<unsigned>(report.size()));
    if(stage==0){
        if(*dead||*suspended||mode||(!host&&(ticks||speed!=0)))return false;
        const auto status=read_native_status(core.status);
        if(status.find(host?"\"boss_phase_sent\":0":"\"boss_phase_applied\":0")!=std::string::npos)return false;
    }else{
        if(!all_player_markers(proofs[0]))return false;
        if(host&&!commanded){
            const auto kill=test_world_method(type,"Kill");
            if(!kill||!parameter(kill,"NoRewards",0,1)||!parameter(kill,"SupressDeathSound",1,1))return false;
            std::uint8_t args[2]{1,1};
            event(actor,kill,args);
            commanded=true;
            return false;
        }
        if(!*dead||!*suspended||(host?mode!=3:mode!=1||ticks||speed!=0))return false;
        if(!host){
            const auto status=read_native_status(core.status);
            if(status.find("\"boss_phase_interactable\":1")==std::string::npos)return false;
        }
    }
    write_file(marker(role_index,proofs[stage]).c_str(),report.data(),static_cast<unsigned>(report.size()));
    ++stage;
    return false;
}

static U::WeakKey fixture_boss_dummy;
static bool run_world_boss_interact_stage(){
    static ULONGLONG far_moved_at{},near_called_at{};
    static bool far_called{},far_refused{},far_safe{},reported{};
    constexpr auto far_proof=L"boss-interact-far-refused.txt",safe_proof=L"boss-interact-far-safe.txt",
                   done_proof=L"boss-interact-result.txt";
    if(reported)return all_player_markers(done_proof);
    const auto actor=resolve(fixture_boss_actor),pawn=resolve(held_pawn);
    if(!actor||!pawn)return false;
    if(!resolve(fixture_boss_dummy))scan([](Address object){
        const auto type=at<Address>(object,16);
        if(!(at<unsigned>(object,8)&0x10u)&&name(type)=="Shrineboss-DummyNPC_BP_C"&&exact_path(type,
           "/Game/Enemies/Boner_00/Skeletons24/ShrineBoss/Shrineboss-DummyNPC_BP.Shrineboss-DummyNPC_BP_C"))
            remember(fixture_boss_dummy,object);
    });
    const auto type=at<Address>(actor,16),box=object_field(actor,"Box");
    const auto collision=box?test_world_method(at<Address>(box,16),"GetCollisionEnabled"):0;
    if(!collision||!parameter(collision,"ReturnValue",0,1))return false;
    std::uint8_t mode=255;
    event(box,collision,&mode);
    const auto status=read_native_status(core.status);
    const auto dummy=resolve(fixture_boss_dummy);
    const auto now=GetTickCount64();
    if(role_index==2&&!far_called){
        if(!far_moved_at){
            held_pose.position[0]=13900.f;
            if(apply_test_pose(pawn,held_pose))far_moved_at=now;
            return false;
        }
        if(now-far_moved_at<900||mode!=1)return false;
        const auto interact=test_world_method(type,"Interact");
        if(!interact||!parameter(interact,"PlayerInteracting",0,8))return false;
        auto argument=pawn;
        event(actor,interact,&argument);
        far_called=true;
    }
    if(role_index==2&&far_called&&!far_refused&&mode==1&&!dummy&&
       status.find("\"boss_interact_guest_refusals\":1")!=std::string::npos){
        write_file(marker(role_index,far_proof).c_str(),"original replica still active",29);
        far_refused=true;
    }
    if(host&&!far_safe&&GetFileAttributesW(marker(2,far_proof).c_str())!=INVALID_FILE_ATTRIBUTES&&mode==3&&!dummy&&
       status.find("\"boss_interact_host_refusals\":1")!=std::string::npos){
        write_file(marker(0,safe_proof).c_str(),"no story and original box open",30);
        far_safe=true;
    }
    if(role_index==1&&!near_called_at&&mode==1&&GetFileAttributesW(marker(0,safe_proof).c_str())!=INVALID_FILE_ATTRIBUTES){
        const auto info=test_world_method(type,"GetInteractInfo"),interact=test_world_method(type,"Interact");
        if(!info||!interact||!parameter(info,"InteractingPlayer",0,8)||!parameter(info,"IsInteractible",8,1)||
           !parameter(interact,"PlayerInteracting",0,8))return false;
        alignas(16) Parameters args{};
        std::memcpy(args.data(),&pawn,8);
        event(actor,info,args.data());
        if(args[8]!=1)return false;
        auto argument=pawn;
        event(actor,interact,&argument);
        event(actor,interact,&argument);
        near_called_at=now;
    }
    const bool owner=host?dummy!=0:dummy==0;
    const bool response=role_index!=1||(near_called_at&&status.find("\"boss_interact_guest_accepts\":1")!=std::string::npos);
    if(!mode&&owner&&response&&(!host||status.find("\"boss_interact_host_accepts\":1")!=std::string::npos)&&
       GetFileAttributesW(marker(0,safe_proof).c_str())!=INVALID_FILE_ATTRIBUTES){
        const auto result="box=0 original_dialogue_actor="+std::to_string(dummy!=0)+"\n";
        write_file(marker(role_index,done_proof).c_str(),result.data(),static_cast<unsigned>(result.size()));
        reported=true;
    }
    return all_player_markers(done_proof);
}
