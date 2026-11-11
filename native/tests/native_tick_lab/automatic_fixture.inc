static constexpr unsigned automatic_slots=4;
using WorldPhase=qdcoop::native_world::Phase;
static WorldPhase automatic_phase;
static unsigned automatic_prepare_steps;
struct AutomaticSample {
    E::NativeBindingView binding{};
    std::string path;
    float hp{};
    std::array<float,3> position{},mesh_position{};
};
static std::array<AutomaticSample,automatic_slots> automatic_initial{};
static AutomaticSample automatic_replacement;
static bool automatic_enabled,automatic_initial_verified,automatic_death_requested,
            automatic_replacement_verified,automatic_old_gone,automatic_stop_clean;

static float automatic_marker(unsigned slot,unsigned generation){
    return 70.f+static_cast<float>(slot)*2.25f+static_cast<float>(generation)*10.f;
}
static bool automatic_query_only(Address actor){
    const auto mesh=object_field(actor,"Mesh"),capsule=object_field(actor,"CapsuleComponent");
    const auto mode=resolve(hitbox_test_methods[0]),response=resolve(hitbox_test_methods[1]);
    if(!mesh||!capsule||!mode||!response||at<std::uint16_t>(mode,182)!=1||at<std::uint16_t>(response,182)!=2||
       query_byte(mesh,mode)!=1||query_byte(capsule,mode)!=0)return false;
    std::uint8_t pawn[]{2,0};
    event(mesh,response,pawn);
    return pawn[1]==0;
}

static bool inspect_automatic(unsigned slot,unsigned generation,AutomaticSample& output,
                              bool authority,float expected){
    if(output.binding.generation)return true;
    E::NativeBindingView binding{};
    if(!core.inspect_binding(slot,&binding)||!binding.confirmed||binding.generation!=generation)return false;
    const auto actor=resolve(binding.actor),visual=resolve(binding.display);
    if(!actor||(!authority&&(!visual||visual==actor)))return false;
    const auto mesh=authority?object_field(actor,"Mesh"):object_field(visual,"SkeletalMeshComponent");
    if(!mesh)return false;
    const auto hp_offset=static_cast<unsigned>(at<int>(metadata.arguments[2],76));
    if(authority){
        if(!quiet_natural_enemy(actor)){phase=63;return false;}
        std::memcpy(reinterpret_cast<void*>(actor+hp_offset),&expected,4);
    }
    const auto hp=at<float>(actor,hp_offset);
    if(hp!=expected)return false;
    if(!authority){
        const auto hidden=U::get_bool(reinterpret_cast<void*>(metadata.arguments[7]),reinterpret_cast<void*>(actor),0);
        const auto tick_fn=resolve(ownership_keys[0]),collision_fn=resolve(ownership_keys[1]);
        if(!hidden||!tick_fn||!collision_fn||query_byte(actor,tick_fn)||
           (query_byte(actor,collision_fn)&&!automatic_query_only(actor))){
            phase=64;
            return false;
        }
    }
    if(!U::object_path(actor,image+0x4A27F00,read_local,output.path)){phase=65;return false;}
    output.binding=binding;
    output.hp=hp;
    output.position=location(actor);
    output.mesh_position=component_location(mesh);
    return true;
}

static bool sample_clean(const AutomaticSample& sample,bool authority){
    if(!sample.binding.generation)return false;
    const auto& binding=sample.binding;
    if(authority)return resolve(binding.actor)!=0&&!fixture_actor_gone(binding.actor);
    return fixture_actor_gone(binding.actor)&&fixture_actor_gone(binding.display);
}

static bool first_room_clean(){
    if(!automatic_replacement_verified||!sample_clean(automatic_replacement,host))return false;
    for(unsigned i=1;i<automatic_slots;++i)if(!sample_clean(automatic_initial[i],host))return false;
    return true;
}

#include "reopen_fixture.inc"

static void automatic_tick(void* engine,float dt){
    if(reopen_stage!=ReopenStage::first){
        reopen_tick(engine,dt);
        return;
    }
    if(!automatic_enabled){
        const auto status=read_native_status(core.status);
        if(status.find("\"build\":-1,")!=std::string::npos){phase=62;return;}
        if(status.find("\"build\":1,")==std::string::npos)return;
    }
    LARGE_INTEGER begin{},end{};
    QueryPerformanceCounter(&begin);
    automatic_phase=core.pump_world(engine,dt,automatic_slots);
    QueryPerformanceCounter(&end);
    if(automatic_phase!=WorldPhase::active){
        if(automatic_phase==WorldPhase::failed||automatic_phase==WorldPhase::world_changed){
            phase=62;
            return;
        }
        ++automatic_prepare_steps;
        return;
    }
    if(!automatic_enabled){
        automatic_enabled=true;
        if(!host)write_file(ready_path,"ready",5);
    }
    if(core_sample_count<core_us.size())
        core_us[core_sample_count++]=static_cast<double>(end.QuadPart-begin.QuadPart)*counter_us;
    ++active_ticks;
    if(!automatic_initial_verified){
        bool complete=true;
        for(unsigned i=0;i<automatic_slots;++i)
            if(!inspect_automatic(i+1,1,automatic_initial[i],host,automatic_marker(i+1,1)))complete=false;
        if(!complete||phase>=10)return;
        automatic_initial_verified=true;
        if(!host)write_file(verified_path,"verified",8);
    }
    if(host&&!automatic_death_requested){
        if(!peers_alive_verified)return;
        const auto actor=resolve(automatic_initial[0].binding.actor);
        const auto kill=resolve(methods[kill_actor]);
        if(!actor||!kill||at<std::uint16_t>(kill,182)!=2){phase=66;return;}
        const auto hp_offset=at<int>(metadata.arguments[2],76);
        const float zero=0;
        std::memcpy(reinterpret_cast<void*>(actor+hp_offset),&zero,sizeof(zero));
        unsigned char arguments[2]{1,1};
        event(actor,kill,arguments);
        automatic_death_requested=true;
        return;
    }
    if(!automatic_replacement_verified){
        if(!inspect_automatic(1,2,automatic_replacement,host,automatic_marker(1,2)))return;
        const auto& old=automatic_initial[0].binding;
        automatic_old_gone=fixture_actor_gone(old.actor)&&(host||fixture_actor_gone(old.display));
        if(!automatic_old_gone||automatic_initial[0].path==automatic_replacement.path){phase=67;return;}
        automatic_replacement_verified=true;
        if(!host)write_file(reuse_verified_marker.c_str(),"verified",8);
    }
    if(!host||peers_reuse_verified){
        if(reopen_lab){
            reopen_live_rejected=!start_reopened_room(reopen_nonce);
            if(!reopen_live_rejected){phase=75;return;}
            reopen_stage=ReopenStage::stop_first;
            return;
        }
        passed=true;
        phase=3;
    }
}

static void write_automatic_report(){
    automatic_stop_clean=first_room_clean();

    std::string json="{\"enabled\":";
    json+=automatic_enabled?"true":"false";
    json+=",\"world_phase\":"+std::to_string(static_cast<unsigned>(automatic_phase));
    json+=",\"prepare_steps\":"+std::to_string(automatic_prepare_steps);
    json+=",\"initial_verified\":";
    json+=automatic_initial_verified?"true":"false";
    json+=",\"replacement_verified\":";
    json+=automatic_replacement_verified?"true":"false";
    json+=",\"old_gone\":";
    json+=automatic_old_gone?"true":"false";
    json+=",\"stop_clean\":";
    json+=automatic_stop_clean?"true":"false";
    if(reopen_lab)append_reopen_report(json);
    json+=",\"initial\":[";
    auto append=[&](const AutomaticSample& sample){
        char row[512]{};
        const auto bytes=std::snprintf(row,sizeof(row),
            "{\"generation\":%u,\"actor\":\"%s\",\"hp\":%.2f,\"position\":[%.3f,%.3f,%.3f],\"mesh\":[%.3f,%.3f,%.3f]}",
            sample.binding.generation,sample.path.c_str(),sample.hp,
            sample.position[0],sample.position[1],sample.position[2],
            sample.mesh_position[0],sample.mesh_position[1],sample.mesh_position[2]);
        if(bytes>0&&bytes<static_cast<int>(sizeof(row)))json.append(row,static_cast<unsigned>(bytes));
    };
    for(unsigned i=0;i<automatic_slots;++i){
        if(i)json+=",";
        append(automatic_initial[i]);
    }
    json+="],\"replacement\":";
    append(automatic_replacement);
    json+="}";
    const auto path=std::wstring(run_root)+L"/"+role_dirs[role_index]+L"/native-auto.json";
    write_file(path.c_str(),json.data(),static_cast<unsigned>(json.size()));
}
