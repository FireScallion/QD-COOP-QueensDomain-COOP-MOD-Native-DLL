static unsigned select_threat_enemy();
static U::WeakKey threat_test_enemy,threat_test_previous;
static bool read_threat_pose(unsigned role,const wchar_t* file_name,TestPlayerPose& pose){
    const auto path=marker(role,file_name);
    const auto file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD bytes{};
    const auto read=ReadFile(file,&pose,sizeof(pose),&bytes,nullptr)&&bytes==sizeof(pose);
    CloseHandle(file);
    return read;
}
#include "enemy_threat_memory_fixture.inc"
static void report_threat_behavior(Address target){
    constexpr const char* flags[]{"bAggrod","bIsAttacking","bIsChasing","bIsAIDisabled","bPlayerIsSeen","HasPlayerlos","bInRecoveryTime"};
    const auto type=at<Address>(target,16);
    static unsigned seen{},sampled{};
    unsigned current{};
    for(unsigned i=0;i<std::size(flags);++i){
        const auto field=field_of(type,flags[i],1);
        if(field&&U::get_bool(reinterpret_cast<void*>(field),reinterpret_cast<void*>(target),0))current|=1u<<i;
    }
    if(sampled&&!(current&~seen))return;
    ++sampled;
    seen|=current;
    std::string report;
    for(unsigned i=0;i<std::size(flags);++i)report+=std::string(flags[i])+"="+((seen&(1u<<i))?"1\n":"0\n");
    write_file(marker(role_index,L"threat-behavior.txt").c_str(),report.data(),static_cast<unsigned>(report.size()));
}
static bool run_native_threat_stage(){
    static float initial{};
    static unsigned slot{};
    static bool approached{},selected{};
    static ULONGLONG approach_retry{};
    const auto pawn=local_test_pawn();
    E::NativeBindingView binding{};
    if(!slot)slot=select_threat_enemy();
    if(!pawn||!slot||!core.inspect_binding(slot,&binding)||!binding.confirmed)return false;
    const auto target=resolve(binding.actor),health=field_of(at<Address>(pawn,16),"Health",4);
    if(!target||!health)return false;
    const auto hp=at<float>(pawn,at<int>(health,76));
    if(!initial){
        if(!std::isfinite(hp)||hp<12)return false;
        if(host){
            remember(threat_test_enemy,target);
            if(const auto previous=object_field(target,"TargetPlayer"))remember(threat_test_previous,previous);
        }
        initial=hp;
        write_file(marker(role_index,L"threat-ready.txt").c_str(),"ok",2);
    }
    if(!all_player_markers(L"threat-ready.txt"))return false;
    if(host)report_threat_behavior(target);
    const auto time=GetTickCount64();
    const bool retry=!world_threat_memory_lab&&approached&&hp>=initial&&time>=approach_retry&&exists(marker(0,L"threat-selected.txt"));
    if(role_index==enemy_damage_recipient&&(!approached||retry)){
        const auto fn=resolve(threat_forward_method);
        if(!fn||at<std::uint16_t>(fn,182)!=12)return false;
        std::array<float,3> forward{};
        event(target,fn,forward.data());
        const auto direction=std::hypot(forward[0],forward[1]);
        if(direction<.1f||!std::isfinite(direction))return false;
        auto point=location(target);
        const float reach=retry?70.f:150.f;
        point[0]+=reach*forward[0]/direction;
        point[1]+=reach*forward[1]/direction;
        point[2]+=3.f;
        held_pose={point,std::atan2(-forward[1],-forward[0])*57.2957795f};
        if(!apply_test_pose(pawn,held_pose))return false;
        if(!approached)write_file(marker(role_index,L"threat-approach.bin").c_str(),reinterpret_cast<const char*>(&held_pose),sizeof(held_pose));
        approached=true;
        approach_retry=time+2500;
    }
    if(host&&!selected){
        TestPlayerPose point{};
        if(!read_threat_pose(enemy_damage_recipient,L"threat-approach.bin",point))return false;
        const auto proxy=enemy_damage_proxy_at(point);
        const auto target_pawn=object_field(target,"TargetPlayer");
        if(!proxy||target_pawn!=proxy)return false;
        selected=true;
        write_file(marker(role_index,L"threat-selected.txt").c_str(),"guest",5);
    }
    if(!exists(marker(0,L"threat-selected.txt")))return false;
    if(world_threat_memory_lab)return run_threat_memory_stage(pawn,target);
    if(role_index==enemy_damage_recipient?hp>=initial:hp!=initial)return false;
    char report[128]{};
    const auto size=std::snprintf(report,sizeof(report),"generation=%u receiver=%u before=%.3f after=%.3f\n",
                                  binding.generation,enemy_damage_recipient,initial,hp);
    if(size>0)write_file(marker(role_index,L"threat-hit.txt").c_str(),report,static_cast<unsigned>(size));
    return all_player_markers(L"threat-hit.txt");
}
static bool verify_threat_cleanup(){
    if(!host)return true;
    const auto actor=resolve(threat_test_enemy);
    if(!actor||object_field(actor,"TargetPlayer")!=resolve(threat_test_previous))return false;
    write_file(marker(role_index,L"threat-clean.txt").c_str(),"restored",8);
    return true;
}
static constexpr unsigned threat_lab_slots=8;
static unsigned select_threat_enemy(){
    if(!host){
        const auto path=marker(0,L"threat-enemy-slot.bin");
        const auto file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)return 0;
        DWORD bytes{};
        unsigned slot{};
        const auto valid=ReadFile(file,&slot,sizeof(slot),&bytes,nullptr)&&bytes==sizeof(slot)&&slot&&slot<=threat_lab_slots;
        CloseHandle(file);
        return valid?slot:0;
    }
    for(unsigned slot=1;slot<=threat_lab_slots;++slot){
        E::NativeBindingView binding{};
        if(!core.inspect_binding(slot,&binding)||!binding.confirmed)continue;
        const auto actor=resolve(binding.actor),type=actor?at<Address>(actor,16):0;
        const auto disabled=field_of(type,"bIsAIDisabled",1);
        if(!actor||!disabled||U::get_bool(reinterpret_cast<void*>(disabled),reinterpret_cast<void*>(actor),0))continue;
        const auto name_of_type=name(type);
        if(name_of_type!="Mossy_Fighter_BP_C"&&name_of_type!="RattyBugger_Base_BP_C")continue;
        write_file(marker(0,L"threat-enemy-slot.bin").c_str(),reinterpret_cast<const char*>(&slot),sizeof(slot));
        std::string path;
        if(U::object_path(actor,image+0x4a27f00,read_local,path))
            write_file(marker(0,L"threat-enemy.txt").c_str(),path.data(),static_cast<unsigned>(path.size()));
        return slot;
    }
    return 0;
}
