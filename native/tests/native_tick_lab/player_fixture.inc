static std::wstring marker(unsigned role,const wchar_t* file);
static bool exists(const std::wstring& path);

struct TestPlayerPose {
    std::array<float,3> position{};
    float yaw{};
};
static std::array<TestPlayerPose,P::slots> first_poses{},moved_poses{};
static TestPlayerPose held_pose{};
static U::WeakKey held_pawn{};
static std::array<U::WeakKey,P::slots> avatar_keys{};
static constexpr const wchar_t* pose_markers[]{L"player-first.bin",L"player-moved.bin"};
static constexpr const wchar_t* verified_markers[]{L"player-first-ok.txt",L"player-moved-ok.txt"};
static constexpr auto silent_role=P::slots-1;
static unsigned player_test_stage,player_check,player_check_slot;
static ULONGLONG player_next_check;
static bool player_silence,player_hidden,player_retired;
static double player_pump_max_us;
#include "player_body_fixture.inc"
static bool all_player_markers(const wchar_t* file);
#include "player_gear_fixture.inc"

static Address local_test_pawn(){
    const auto fn=resolve(local_pawn_method);
    alignas(16) Parameters args{};
    if(!fn||parameter_offset(fn,"ReturnValue",8)!=16||
       !put(args,fn,"WorldContextObject",resolve(player))||
       !put(args,fn,"PlayerIndex",std::int32_t{0}))return 0;
    event(resolve(gameplay),fn,args.data());
    return U::field<Address>(args.data(),16);
}

static TestPlayerPose read_player_pose(Address actor){
    alignas(16) Parameters args{};
    event(actor,metadata.arguments[10],args.data());
    return {location(actor),U::field<float>(args.data(),at<int>(metadata.arguments[11],76)+4)};
}

static bool apply_test_pose(Address pawn,const TestPlayerPose& pose){
    const auto setter=metadata.arguments[23];
    alignas(16) Parameters args{};
    if(!put(args,setter,"NewLocation",pose.position)||
       !put(args,setter,"NewRotation",std::array<float,3>{0,pose.yaw,0})||
       !put(args,setter,"bTeleport",std::uint8_t{1}))return false;
    event(pawn,setter,args.data());
    return true;
}

static bool write_local_pose(unsigned step){
    const auto pawn=local_test_pawn();
    if(!pawn||(step==0&&!quiet_natural_enemy(pawn)))return false;
    TestPlayerPose target{location(pawn),step==0?170.f-60.f*role_index:-170.f+55.f*role_index};
    target.position[0]+=step==0?5.f*role_index:12.f+role_index;
    target.position[1]+=3.f*role_index;
    if(!apply_test_pose(pawn,target))return false;
    const auto actual=read_player_pose(pawn);
    if(std::abs(actual.position[0]-target.position[0])>.02f||std::abs(actual.yaw-target.yaw)>.02f)return false;
    remember(held_pawn,pawn);
    if(resolve(held_pawn)!=pawn)return false;
    held_pose=actual;
    if(player_body_lab){
        if(step==0&&!bind_test_movement(pawn)){phase=92;return false;}
        if(step==1&&host&&!start_test_actions(pawn)){phase=93;return false;}
        test_motion_stage=step;
    }
    write_file(marker(role_index,pose_markers[step]).c_str(),reinterpret_cast<const char*>(&actual),sizeof(actual));
    return true;
}

static bool read_expected_pose(unsigned role,unsigned step,TestPlayerPose& pose){
    const auto path=marker(role,pose_markers[step]);
    const auto file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                                nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD bytes{};
    const bool read=ReadFile(file,&pose,sizeof(pose),&bytes,nullptr)&&bytes==sizeof(pose);
    CloseHandle(file);
    return read;
}

static bool player_actor_hidden(Address actor){
    const auto property=field_of(at<Address>(actor,16),"bHidden",1);
    return property&&U::get_bool(reinterpret_cast<void*>(property),reinterpret_cast<void*>(actor),0);
}

static std::optional<std::array<unsigned,P::slots>> fixture_roles_by_slot(){
    std::array<unsigned,P::slots> roles{0,1,2,3};
    if(!world_lan_lab)return roles;
    unsigned seen{};
    for(unsigned role=0;role<roles.size();++role){
        const auto path=marker(role,L"lan-slot.bin");
        const auto file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
        if(file==INVALID_HANDLE_VALUE)return std::nullopt;
        std::uint8_t slot=255;
        DWORD read{};
        const bool valid=ReadFile(file,&slot,1,&read,nullptr)&&read==1&&slot<roles.size()&&!(seen&(1u<<slot));
        CloseHandle(file);
        if(!valid)return std::nullopt;
        seen|=1u<<slot;
        roles[slot]=role;
    }
    return roles;
}

static bool verify_player_poses(unsigned step){
    const auto mapped=fixture_roles_by_slot();
    if(!mapped)return false;
    const auto local_slot=static_cast<unsigned>(std::find(mapped->begin(),mapped->end(),role_index)-mapped->begin());
    P::Binding own{};
    if(core.inspect_player(local_slot,&own)){phase=86;return false;}
    auto& poses=step==0?first_poses:moved_poses;
    auto keys=avatar_keys;
    const auto local=local_test_pawn();
    const auto same=[](U::WeakKey left,U::WeakKey right){
        return left.index==right.index&&left.serial==right.serial;
    };
    for(unsigned slot=0;slot<P::slots;++slot){
        player_check_slot=slot;
        player_check=1;
        TestPlayerPose expected{};
        if(!read_expected_pose((*mapped)[slot],step,expected))return false;
        if(slot==local_slot){poses[slot]=expected;continue;}
        P::Binding binding{};
        player_check=2;
        if(!core.inspect_player(slot,&binding)||!binding.visible||binding.generation!=1||!binding.sequence)return false;
        const U::WeakKey key{binding.object_index,binding.object_serial};
        const auto actor=resolve(key);
        player_check=3;
        if(!actor||actor==local||!exact_path(at<Address>(actor,16),"/Script/Engine.SkeletalMeshActor"))
            return false;
        const auto component=object_field(actor,"SkeletalMeshComponent");
        const auto mesh=component?object_field(component,"SkeletalMesh"):0;
        const auto actual=read_player_pose(actor);
        poses[slot]=actual;
        player_check=4;
        if(!mesh||!exact_path(mesh,"/Game/Player/Assets/FPModel/Player2026/Player_FullChar_2026.Player_FullChar_2026"))
            return false;
        player_check=5;
        if(query_byte(actor,resolve(ownership_keys[1]))||player_actor_hidden(actor))return false;
        player_check=6;
        expected.position[2]-=85.f;
        if(player_body_lab||player_gear_lab||player_rescue_lab)expected.yaw=std::remainder(expected.yaw-90.f,360.f);
        for(unsigned axis=0;axis<3;++axis)
            if(std::abs(actual.position[axis]-expected.position[axis])>.05f)return false;
        if(std::abs(std::remainder(actual.yaw-expected.yaw,360.f))>.05f)return false;
        player_check=7;
        if(player_body_lab&&!verify_avatar_graph(component,slot,step))return false;
        if(step&&!same(key,keys[slot])){phase=87;return false;}
        for(unsigned other=0;other<slot;++other)if(same(key,keys[other])){phase=88;return false;}
        keys[slot]=key;
    }
    avatar_keys=keys;
    return true;
}

static bool all_player_markers(const wchar_t* file){
    for(unsigned slot=0;slot<P::slots;++slot)if(!exists(marker(slot,file)))return false;
    return true;
}

static bool finish_body_actions(){
    constexpr auto marker_name=L"player-actions-stopped.txt";
    if(test_body_verified)return all_player_markers(marker_name);
    if(host){
        if(!test_actions_started||!stop_test_actions(resolve(held_pawn))){phase=94;return false;}
    }else{
        const auto actor=resolve(avatar_keys[0]);
        const auto instance=actor?test_anim_instance(object_field(actor,"SkeletalMeshComponent")):0;
        if(!instance)return false;
        for(auto key:rendered_actions){
            const auto asset=resolve(key);
            if(!asset||!test_montage_stopped(instance,asset))return false;
        }
        test_actions_stopped=true;
    }
    test_motion_stage=2;
    test_body_verified=true;
    write_file(marker(role_index,marker_name).c_str(),"ok",2);
    return all_player_markers(marker_name);
}

#include "player_rescue_fixture.inc"
#include "checkpoint_probe_fixture.inc"
#include "world_map_probe_fixture.inc"
#include "player_world_progress_fixture.inc"
#include "world_actor_probe_fixture.inc"
#include "world_mechanism_probe_fixture.inc"
#include "world_shrine_gate_fixture.inc"
#include "world_pause_probe_fixture.inc"
#include "enemy_damage_probe_fixture.inc"
#include "enemy_damage_inbound_fixture.inc"
#include "enemy_threat_fixture.inc"
#include "enemy_weapon_fixture.inc"
#include "world_travel_guard_fixture.inc"
#include "world_stream_probe_fixture.inc"
#include "persistent_save_probe_fixture.inc"
#include "world_music_fixture.inc"
#include "world_story_fixture.inc"
#include "world_library_fixture.inc"
#include "world_castle_gate_fixture.inc"
#include "world_barrier_fixture.inc"
#include "world_arena_fixture.inc"
#include "world_cutscene_fixture.inc"
#include "world_boss_fixture.inc"
#include "world_elevator_fixture.inc"
#include "world_elevator_guest_fixture.inc"
#include "player_checkpoint_fixture.inc"
#include "guest_safe_save_fixture.inc"

static bool fixture_lan_active(){
    static unsigned ready_epoch{};
    if(ready_epoch==fixture_lan_epoch)return true;
    Lan::Status status;
    if(!core.lan_room(&status,sizeof(status)))return false;
    if(status.phase==Lan::Phase::failed){phase=89;return false;}
    if(status.phase!=Lan::Phase::active||status.epoch!=fixture_lan_epoch||status.seats!=P::slots||status.connected!=P::slots-1||!status.room||
       status.peer_mask!=(host?7u:1u)||(!host&&status.slot>=P::slots-1))return false;
    if(fixture_lan_epoch==2&&(!resolve(held_pawn)||local_test_pawn()!=resolve(held_pawn)))return false;
    const auto slot=static_cast<char>(host?0:status.slot+1);
    write_file(marker(role_index,L"lan-slot.bin").c_str(),&slot,1);
    char text[190]{};
    const auto size=std::snprintf(text,sizeof(text),"slot=%u seats=%u connected=%u mask=%u epoch=%u scope=%llx.%llx\n",
        static_cast<unsigned>(slot),status.seats,status.connected,status.peer_mask,status.epoch,
        static_cast<unsigned long long>(status.room.high),static_cast<unsigned long long>(status.room.low));
    if(size>0)write_file(marker(role_index,L"lan-active.txt").c_str(),text,static_cast<unsigned>(size));
    ready_epoch=fixture_lan_epoch;
    return true;
}

static bool fixture_lan_next_epoch(){
    static bool left{};
    Lan::Status status;
    if(!core.lan_room(&status,sizeof(status)))return false;
    if(role_index==3){
        if(!left){
            if(!core.stop()||!core.lan_room(&status,sizeof(status))||status.phase!=Lan::Phase::closed)return false;
            left=true;
            write_file(marker(role_index,L"lan-left.txt").c_str(),"closed epoch 1",14);
        }
        if(!exists(marker(0,L"lan-recovered.txt"))||!core.start_lan(0,lan_ipv4,first_port,0))return false;
        write_file(marker(role_index,L"lan-rejoined.txt").c_str(),"new worker",10);
    }else{
        if(status.phase!=Lan::Phase::recovering||status.epoch!=1||core.recover()!=1)return false;
        write_file(marker(role_index,L"lan-recovered.txt").c_str(),"clean epoch 1",13);
    }
    avatar_keys={};
    fixture_lan_epoch=2;
    player_test_stage=0;
    return true;
}

static bool fixture_lan_departure(){
    Lan::Status status;
    if(!core.lan_room(&status,sizeof(status)))return false;
    if(!host&&(status.phase!=Lan::Phase::failed||status.failure!=Lan::Failure::departure))return false;
    if(!core.stop())return false;
    if(!core.lan_room(&status,sizeof(status)))return false;
    const bool closed=host?status.phase==Lan::Phase::closed&&status.failure==Lan::Failure::none:
        status.phase==Lan::Phase::failed&&status.failure==Lan::Failure::departure;
    if(!closed)return false;
    constexpr auto proof=L"lan-departure.txt";
    const auto report="phase="+std::to_string(static_cast<unsigned>(status.phase))+" failure="+
        std::to_string(static_cast<unsigned>(status.failure))+"\n";
    write_file(marker(role_index,proof).c_str(),report.data(),static_cast<unsigned>(report.size()));
    return true;
}

static void player_tick(void* engine,float dt){
    const auto now=GetTickCount64();
    if(world_lan_lab){
        if(!fixture_lan_active())return;
        if(player_test_stage==4&&all_player_markers(verified_markers[1])){
            if(fixture_lan_epoch==1){fixture_lan_next_epoch();return;}
            if(fixture_lan_departure()){
                ++player_test_stage;
                passed=true;
                phase=3;
            }
            return;
        }
    }
    if(!player_silence){
        // The game controller can rotate the Pawn on each tick. Hold the
        // fixture's yaw just before production capture on the game thread.
        if(const auto pawn=resolve(held_pawn)){
            if(!arrival_fixture_free_pose)apply_test_pose(pawn,held_pose);
            if(player_body_lab)hold_test_movement(pawn,held_pose.yaw);
            if(player_rescue_lab)rescue_test_input(pawn,now);
        }
        if(player_bone_lab&&host)hold_test_bone_charge();
        LARGE_INTEGER begin{},end{};
        QueryPerformanceCounter(&begin);
        const auto limit=world_threat_lab?threat_lab_slots:world_boss_lab?4u:1u;
        const auto world=core.pump_world(engine,dt,limit);
        QueryPerformanceCounter(&end);
        if(world==WorldPhase::world_changed||world==WorldPhase::failed){phase=82;return;}
        if(world!=WorldPhase::active){
            if(world_photo_probe&&host&&player_test_stage==4)probe_stock_photo();
            return;
        }
        ++active_ticks;
        const auto elapsed=static_cast<double>(end.QuadPart-begin.QuadPart)*counter_us;
        player_pump_max_us=std::max(player_pump_max_us,elapsed);
        if(player_test_stage>=2&&core_sample_count<core_us.size())core_us[core_sample_count++]=elapsed;
    }
    if(world_arrival_lab&&player_test_stage==4&&all_player_markers(verified_markers[1])){
        if(!run_world_arrival_stage(reinterpret_cast<Address>(engine),now))return;
        ++player_test_stage;
        passed=true;
        phase=3;
        return;
    }
    if(player_body_lab&&!host&&player_test_stage>=2&&!test_body_verified)find_test_actions();
    if(now<player_next_check)return;
    player_next_check=now+50;
    switch(player_test_stage){
        case 0:
            if(write_local_pose(0))++player_test_stage;
            break;
        case 1:
        case 3:{
            const auto step=player_test_stage==1?0u:1u;
            if(!verify_player_poses(step))break;
            write_file(marker(role_index,verified_markers[step]).c_str(),"ok",2);
            ++player_test_stage;
            break;
        }
        case 2:
            if(all_player_markers(verified_markers[0])&&write_local_pose(1))++player_test_stage;
            break;
        case 4:{
            if(!all_player_markers(verified_markers[1]))break;
            if(world_lan_lab)break;
            if(checkpoint_probe&&host&&!probe_stock_checkpoint(reinterpret_cast<Address>(engine)))break;
            if(world_map_probe&&host&&!probe_world_maps(reinterpret_cast<Address>(engine)))break;
            if(world_actor_probe&&host&&!probe_world_actors(reinterpret_cast<Address>(engine)))break;
            if(world_chest_probe&&host&&!probe_stock_chest(reinterpret_cast<Address>(engine)))break;
            if(world_chest_lab&&!run_world_chest_stage(reinterpret_cast<Address>(engine)))break;
            if(world_mechanism_probe&&host&&!probe_stock_mechanisms())break;
            if(world_mechanism_lab&&!run_world_mechanism_stage())break;
            if(world_shrine_gate_lab&&!run_world_shrine_gate_stage())break;
            if(world_castle_gate_lab&&!run_world_castle_gate_stage(reinterpret_cast<Address>(engine)))break;
            if(world_barrier_lab&&!run_world_barrier_stage())break;
            if(world_arena_lab&&!run_world_arena_stage(reinterpret_cast<Address>(engine)))break;
            if(world_cutscene_lab&&!run_world_cutscene_stage())break;
            if(world_boss_lab&&!run_world_boss_stage())break;
            if(world_boss_interact_lab&&!run_world_boss_interact_stage())break;
            if(world_pause_probe&&host&&!probe_stock_pause())break;
            if(world_pause_lab&&!run_world_pause_stage())break;
            if(world_photo_probe&&host&&!probe_stock_photo())break;
            if(world_photo_lab&&!run_world_photo_stage())break;
            if(world_travel_lab&&!run_travel_guard_stage())break;
            if(world_stream_probe&&host&&!probe_native_streaming(reinterpret_cast<Address>(engine)))break;
            if(world_stream_replay&&host&&!replay_stock_streams(reinterpret_cast<Address>(engine)))break;
            if(world_stream_lab&&!run_world_stream_stage(reinterpret_cast<Address>(engine),now))break;
            if(persistent_save_probe&&!probe_persistent_save(reinterpret_cast<Address>(engine)))break;
            if(persistent_reload_lab&&!run_persistent_reload_stage(reinterpret_cast<Address>(engine)))break;
            if(world_music_lab&&!run_world_music_stage(reinterpret_cast<Address>(engine),now))break;
            if(world_story_lab&&!run_world_story_stage(reinterpret_cast<Address>(engine),now))break;
            if(world_library_lab&&!run_world_library_stage(reinterpret_cast<Address>(engine)))break;
            if(world_elevator_lab&&!run_world_elevator_stage(reinterpret_cast<Address>(engine)))break;
            if(world_elevator_guest_lab&&!run_world_elevator_guest_stage(reinterpret_cast<Address>(engine),now))break;
            if(guest_safe_save_lab&&!run_guest_safe_save_stage(reinterpret_cast<Address>(engine)))break;
            if(world_damage_probe&&host&&!probe_stock_damage())break;
            if(world_damage_stock&&host&&!apply_stock_damage_probe())break;
            if(world_damage_lab&&!run_native_damage_stage())break;
            if(world_enemy_damage_lab&&!run_enemy_damage_inbound())break;
            if(world_threat_lab&&!run_native_threat_stage())break;
            if(world_hitbox_lab&&!run_enemy_weapon_stage(now))break;
            if(world_progress_lab&&!run_world_progress_stage(reinterpret_cast<Address>(engine)))break;
            if(checkpoint_lab&&!run_checkpoint_stage(reinterpret_cast<Address>(engine)))break;
            if(player_body_lab&&!finish_body_actions())break;
            if(player_gear_lab&&!run_gear_stage())break;
            if((player_rescue_lab||player_medic_replay)&&!rescue_instance)
                rescue_instance=object_field(reinterpret_cast<Address>(engine),"GameInstance");
            if(player_rescue_lab&&!run_rescue_stage(now))break;
            if(player_medic_replay&&(!rescue_instance||!rescue_replay_check()))break;
            if(player_medic_interrupt||checkpoint_wipe_missing||world_stream_lab||persistent_save_probe||
               persistent_reload_lab||world_music_lab||world_story_lab||guest_safe_save_lab||world_library_lab||
               world_elevator_lab||world_elevator_guest_lab||world_castle_gate_lab||world_arena_lab||world_cutscene_lab||world_boss_lab){
                ++player_test_stage;
                passed=true;
                phase=3;
                break;
            }
            if(role_index==silent_role){
                player_silence=true;
                for(unsigned i=0;i<silent_role;++i)if(!exists(marker(i,L"player-timeout-ok.txt")))return;
            }else{
                P::Binding binding{};
                if(core.inspect_player(silent_role,&binding)&&!binding.visible){
                    const auto actor=resolve(avatar_keys[silent_role]);
                    if(actor&&player_actor_hidden(actor))
                        player_hidden=true;
                }
                if(!player_hidden||!fixture_actor_gone(avatar_keys[silent_role]))break;
                if(world_enemy_damage_lab&&host&&!verify_stale_enemy_proxy())break;
                player_retired=true;
                write_file(marker(role_index,L"player-timeout-ok.txt").c_str(),"ok",2);
                if(host)for(unsigned i=1;i<silent_role;++i)if(!exists(marker(i,L"player-timeout-ok.txt")))return;
            }
            ++player_test_stage;
            passed=true;
            phase=3;
            break;
        }
    }
}

static void write_player_report(){
    bool clean=true;
    for(unsigned slot=0;slot<P::slots;++slot){
        P::Binding binding{};
        clean=clean&&!core.inspect_player(slot,&binding)&&
            (avatar_keys[slot].serial==0||fixture_actor_gone(avatar_keys[slot]));
    }
    if(presentation_lab){
        constexpr std::pair<unsigned,Address> originals[]{
            {0x07,0x11b6560},{0x14,0x11b6810},{0x4d,0x11b7860},{0x4f,0x11b78b0},{0x63,0x11b4b20}
        };
        const bool restored=std::ranges::all_of(originals,[](const auto& entry){
            return at<Address>(image+0x4a62e10,entry.first*8)==image+entry.second;
        });
        if(restored)write_file(marker(role_index,L"presentation-clean.txt").c_str(),"ok",2);
        clean=clean&&restored;
    }
    if(checkpoint_lab){
        const auto save=resolve(cp_probe_methods[cp_save]);
        const bool restored=save&&at<Address>(save,216)==image+0x2dc30f0;
        if(restored)write_file(marker(role_index,L"checkpoint-clean.txt").c_str(),"ok",2);
        clean=clean&&restored;
    }
    if(world_pause_lab)clean=clean&&verify_pause_cleanup();
    if(world_photo_lab)clean=clean&&verify_photo_cleanup();
    if(world_damage_lab||world_hitbox_lab||world_enemy_damage_lab||world_threat_lab)clean=clean&&verify_damage_cleanup();
    if(world_threat_lab)clean=clean&&verify_threat_cleanup();
    if(world_travel_lab)clean=clean&&verify_travel_guard_cleanup();
    if(world_music_lab)clean=clean&&verify_world_music_cleanup();
    if(world_library_lab)clean=clean&&verify_world_library_cleanup();
    passed=passed&&clean;
    char head[1152]{};
    std::snprintf(head,sizeof(head),"{\"stage\":%u,\"check\":%u,\"check_slot\":%u,\"clean\":%s,\"hidden_on_timeout\":%s,\"retired_on_timeout\":%s,\"pump_max_us\":%.3f,\"body_verified\":%s,\"action_start\":[%.4f,%.4f],\"action_end\":[%.4f,%.4f],\"gear_verified\":%s,\"gear_stage\":%u,\"gear_check\":%u,\"gear_slot\":%u,\"gear_weapon\":%u,\"gear_mesh\":%u,\"bone_verified\":%s,\"bone_stage\":%u,\"bone_check\":%u,\"bone_sequences\":[%llu,%llu],\"bone_errors\":[%.4f,%.4f],\"bone_motion\":%.4f,\"poses\":[",
        player_test_stage,player_check,player_check_slot,clean?"true":"false",player_hidden?"true":"false",
        player_retired?"true":"false",player_pump_max_us,(test_body_verified&&test_actions_stopped)?"true":"false",
        first_action_positions[0],first_action_positions[1],last_action_positions[0],last_action_positions[1],
        gear_verified?"true":"false",gear_test_stage,gear_test_check,gear_test_slot,gear_observed_weapon,gear_observed_mesh,
        bone_verified?"true":"false",bone_test_stage,bone_test_check,
        static_cast<unsigned long long>(bone_first_sequence),static_cast<unsigned long long>(bone_second_sequence),
        bone_first_error,bone_second_error,bone_motion);
    std::string json=head;
    for(unsigned slot=0;slot<P::slots;++slot){
        if(slot)json+=',';
        const auto& first=first_poses[slot];
        const auto& moved=moved_poses[slot];
        char sample[384]{};
        std::snprintf(sample,sizeof(sample),"{\"slot\":%u,\"index\":%d,\"serial\":%d,\"first\":[%.3f,%.3f,%.3f,%.3f],\"moved\":[%.3f,%.3f,%.3f,%.3f],\"gear_meshes\":[%u,%u,%u]}",
            slot,avatar_keys[slot].index,avatar_keys[slot].serial,
            first.position[0],first.position[1],first.position[2],first.yaw,
            moved.position[0],moved.position[1],moved.position[2],moved.yaw,
            gear_mesh_evidence[0][slot],gear_mesh_evidence[1][slot],gear_mesh_evidence[2][slot]);
        json+=sample;
    }
    json+="]}";
    write_file(marker(role_index,L"native-players.json").c_str(),json.data(),static_cast<unsigned>(json.size()));
}
