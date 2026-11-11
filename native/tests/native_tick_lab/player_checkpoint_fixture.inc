namespace TestCP=qdcoop::checkpoint;
static std::uint64_t checkpoint_test_base;
static bool checkpoint_test_written,checkpoint_test_health;
static unsigned checkpoint_test_check;
static float checkpoint_test_hp;
static std::array<float,3> checkpoint_test_location;
static TestCP::Manifest checkpoint_test_manifest;
static U::WeakKey checkpoint_test_pawn;
static std::wstring checkpoint_test_archive;

struct CheckpointTestString {const wchar_t* text;int count,capacity;};
static void checkpoint_test_string(Parameters& args,const std::wstring& value){
    const auto size=static_cast<int>(value.size()+1);
    const CheckpointTestString string{value.c_str(),size,size};
    std::memcpy(args.data(),&string,sizeof(string));
}
static std::wstring checkpoint_test_primary(Address gi){
    int index=-1;
    event(gi,resolve(cp_probe_methods[cp_get_index]),&index);
    if(index!=1)return {};
    auto path=marker(role_index,L"profile/Saved/SaveGames/World1.sav");
    std::replace(path.begin(),path.end(),L'\\',L'/');
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&index,4);
    event(resolve(cp_probe_methods[cp_utils]),resolve(cp_probe_methods[cp_get_slot]),args.data());
    const auto string=U::field<CheckpointTestString>(args.data(),16);
    std::wstring actual;
    if(string.text&&string.count>0&&string.count<2048&&string.text[string.count-1]==0)
        actual.assign(string.text,static_cast<std::size_t>(string.count-1));
    if(string.text)reinterpret_cast<void(*)(const void*)>(image+0xeae0b0)(string.text);
    std::replace(actual.begin(),actual.end(),L'\\',L'/');
    return _wcsicmp((L"FULLPATH:"+path).c_str(),actual.c_str())==0?path:std::wstring{};
}
static bool checkpoint_test_file(const TestCP::BindingView& view,const std::wstring& primary,
                                 Address gi,const wchar_t* name,bool refreshed){
    const auto& m=view.latest;
    const auto prefix=primary+L".qdc-"+qdcoop::rescue::rescue_suffix({view.room.room_high,view.room.room_low})+
                      L"-"+qdcoop::rescue::rescue_suffix({view.room.world,m.token});
    TestCP::SaveProof archive{},stock{};
    checkpoint_test_check=1;
    if(view.slot>=m.saves.size()||m.roster.mask!=15||
       !E::reward::journal::fingerprint_save((prefix+L".sav").c_str(),archive)||
       archive!=m.saves[view.slot]||
       !E::reward::journal::fingerprint_save(primary.c_str(),stock)||archive!=stock)return false;
    checkpoint_test_check=2;
    qdcoop::rescue::FileHandle file(CreateFileW((prefix+L".commit").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,
                                               OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
    E::Packet packet{};
    DWORD bytes{};
    LARGE_INTEGER length{};
    TestCP::Message decoded{};
    if(!file||!GetFileSizeEx(file.value,&length)||length.QuadPart!=TestCP::wire_size||
       !ReadFile(file.value,packet.data.data(),TestCP::wire_size,&bytes,nullptr)||bytes!=TestCP::wire_size)return false;
    packet.size=bytes;
    if(!TestCP::decode(view.room,packet,decoded)||decoded.op!=TestCP::Op::commit||decoded.manifest!=m)return false;
    checkpoint_test_check=3;
    alignas(16) Parameters args{};
    const auto slot_name=L"FULLPATH:"+prefix+L".sav";
    checkpoint_test_string(args,slot_name);
    event(resolve(gameplay),resolve(cp_probe_methods[cp_load]),args.data());
    const auto saved=U::field<Address>(args.data(),24);
    const auto active=object_field(gi,"ActiveSaveSlot");
    if(!saved||!active||saved==active||at<Address>(saved,16)!=at<Address>(active,16))return false;
    checkpoint_test_check=4;
    const auto health=field_of(at<Address>(saved,16),"CurrentPlayerHealth",4);
    const auto level=field_of(at<Address>(saved,16),"SaveLevel",8);
    std::string level_name;
    if(!health||!level||!U::name_string(image+0x4a27f00,at<U::NameKey>(saved,at<int>(level,76)),read_local,level_name))
        return false;
    const auto hp=at<float>(saved,at<int>(health,76));
    if(level_name!="Ruins_Level_00"||(refreshed&&std::abs(hp-(92.f-role_index*7.f))>.1f))return false;
    const auto position=field_of(at<Address>(saved,16),"SaveLocation",48);
    if(!position)return false;
    checkpoint_test_hp=hp;
    checkpoint_test_location=at<std::array<float,3>>(saved,at<int>(position,76)+16);
    checkpoint_test_manifest=m;
    checkpoint_test_archive=prefix+L".sav";
    checkpoint_test_check=0;
    char report[220]{};
    const auto count=std::snprintf(report,sizeof(report),"token=%016llx slot=%u serial=%llu archives=%llu commits=%llu stock=%llu hp=%.2f bytes=%llu level=%s\n",
        static_cast<unsigned long long>(m.token),view.slot,static_cast<unsigned long long>(m.serial),
        static_cast<unsigned long long>(view.archives),static_cast<unsigned long long>(view.commits),
        static_cast<unsigned long long>(view.stock),hp,static_cast<unsigned long long>(archive.bytes),level_name.c_str());
    if(count>0)write_file(marker(role_index,name).c_str(),report,static_cast<unsigned>(count));
    return count>0;
}
static bool checkpoint_test_wipe(const TestCP::BindingView& view,Address gi){
    static bool fired,verified;
    static ULONGLONG host_last_at;
    if(verified)return all_player_markers(L"checkpoint-wipe.txt");
    if(!fired){
        if(checkpoint_wipe_missing){
            const auto missing=marker(1,L"checkpoint-wipe-missing.txt");
            if(role_index==1&&!exists(missing)){
                const auto held=checkpoint_test_archive+L".held";
                if(!MoveFileExW(checkpoint_test_archive.c_str(),held.c_str(),MOVEFILE_WRITE_THROUGH))return false;
                write_file(missing.c_str(),"ok",2);
            }
            if(!exists(missing))return false;
        }
        if(host){
            for(unsigned slot=1;slot<P::slots;++slot)if(!exists(marker(slot,L"checkpoint-wipe-down.txt")))return false;
            if(!host_last_at)host_last_at=GetTickCount64()+800;
            if(GetTickCount64()<host_last_at)return false;
        }
        const auto pawn=local_test_pawn();
        const auto pit=resolve(cp_probe_methods[cp_pit]);
        P::RescueView life{};
        if(!pawn||!pit||(view.flags&5)!=5||!core.inspect_rescue(&life)||!life.armed)return false;
        remember(checkpoint_test_pawn,pawn);
        if(resolve(checkpoint_test_pawn)!=pawn)return false;
        held_pawn={};
        float one=1.f;
        std::uint8_t crushing=1;
        event(pawn,resolve(cp_probe_methods[cp_health]),&one);
        event(pawn,pit,&crushing);
        fired=true;
        write_file(marker(role_index,L"checkpoint-wipe-fired.txt").c_str(),"ok",2);
        return false;
    }
    checkpoint_test_check=6;
    if(view.flags&2)write_file(marker(role_index,L"checkpoint-wipe-down.txt").c_str(),"ok",2);
    if(checkpoint_wipe_missing){
        if(view.rollbacks||view.world_nonce||view.latest!=checkpoint_test_manifest||!(view.flags&2)||
           view.rollback_stage!=TestCP::RollbackStage::failed||!resolve(checkpoint_test_pawn))return false;
        write_file(marker(role_index,L"checkpoint-wipe-refused.txt").c_str(),"ok",2);
        if(!all_player_markers(L"checkpoint-wipe-refused.txt"))return false;
        const auto restored=marker(1,L"checkpoint-wipe-restored.txt");
        if(role_index==1&&!exists(restored)){
            const auto held=checkpoint_test_archive+L".held";
            if(!MoveFileExW(held.c_str(),checkpoint_test_archive.c_str(),MOVEFILE_WRITE_THROUGH))return false;
            write_file(restored.c_str(),"ok",2);
        }
        return exists(restored);
    }
    if(!view.rollbacks||!view.world_nonce||!view.latest.token||view.latest==checkpoint_test_manifest||
       view.slot>=P::slots||view.latest.roster.mask!=15||
       view.latest.roster.members[view.slot].character==checkpoint_test_manifest.roster.members[view.slot].character)return false;
    alignas(16) Parameters args{};
    const auto fn=resolve(local_pawn_method);
    if(!fn||!put(args,fn,"WorldContextObject",gi)||!put(args,fn,"PlayerIndex",std::int32_t{0}))return false;
    event(resolve(gameplay),fn,args.data());
    const auto pawn=U::field<Address>(args.data(),16);
    const auto active=object_field(gi,"ActiveSaveSlot");
    const auto hp=pawn?field_of(at<Address>(pawn,16),"Health",4):0;
    if(!hp||!active||active!=object_field(gi,"BaseActiveSave")||pawn==resolve(checkpoint_test_pawn)||
       std::abs(at<float>(pawn,at<int>(hp,76))-checkpoint_test_hp)>.1f)return false;
    const auto position=location(pawn);
    for(unsigned i=0;i<position.size();++i)if(std::abs(position[i]-checkpoint_test_location[i])>25.f)return false;
    checkpoint_test_check=7;
    auto keys=avatar_keys;
    for(unsigned slot=0;slot<P::slots;++slot){
        if(slot==role_index)continue;
        P::Binding binding{};
        if(!fixture_actor_gone(avatar_keys[slot])||!core.inspect_player(slot,&binding)||!binding.visible)return false;
        keys[slot]={binding.object_index,binding.object_serial};
    }
    avatar_keys=keys;
    remember(player,pawn);
    char report[220]{};
    const auto size=std::snprintf(report,sizeof(report),"nonce=%016llx slot=%u hp=%.2f pos=%.2f,%.2f,%.2f\n",
        static_cast<unsigned long long>(view.world_nonce),view.slot,checkpoint_test_hp,position[0],position[1],position[2]);
    if(size>0)write_file(marker(role_index,L"checkpoint-wipe.txt").c_str(),report,static_cast<unsigned>(size));
    verified=size>0;
    return verified&&all_player_markers(L"checkpoint-wipe.txt");
}
static bool run_checkpoint_stage(Address engine){
    TestCP::BindingView view{};
    if(!core.inspect_checkpoint(&view))return false;
    char report[220]{};
    const auto size=std::snprintf(report,sizeof(report),"phase=%u flags=%u slot=%u check=%u serial=%llu archives=%llu commits=%llu errors=%llu stock=%llu failure=%u wipe=%u rollbacks=%llu\n",
        view.stage,view.flags,view.slot,checkpoint_test_check,static_cast<unsigned long long>(view.latest.serial),static_cast<unsigned long long>(view.archives),
        static_cast<unsigned long long>(view.commits),static_cast<unsigned long long>(view.errors),static_cast<unsigned long long>(view.stock),
        view.failure,static_cast<unsigned>(view.rollback_stage),static_cast<unsigned long long>(view.rollbacks));
    if(size>0)write_file(marker(role_index,L"checkpoint-progress.txt").c_str(),report,static_cast<unsigned>(size));
    if(!view.latest.token)return false;
    const auto gi=object_field(engine,"GameInstance");
    if(!gi)return false;
    if(checkpoint_wipe&&view.rollbacks)return checkpoint_test_wipe(view,gi);
    static std::wstring primary;
    if(primary.empty()){
        checkpoint_test_check=5;
        primary=checkpoint_test_primary(gi);
        if(primary.empty())return false;
    }
    if(!checkpoint_test_base){
        if(view.latest.refresh||!checkpoint_test_file(view,primary,gi,L"checkpoint-base.txt",false))return false;
        checkpoint_test_base=view.latest.serial;
    }
    if(!all_player_markers(L"checkpoint-base.txt"))return false;
    if(!checkpoint_refresh)return !checkpoint_wipe||checkpoint_test_wipe(view,gi);
    const auto pawn=local_test_pawn();
    if(!pawn)return false;
    if(!checkpoint_test_health){
        float hp=92.f-role_index*7.f;
        event(pawn,resolve(cp_probe_methods[cp_health]),&hp);
        checkpoint_test_health=true;
        write_file(marker(role_index,L"checkpoint-health.txt").c_str(),"ok",2);
    }
    if(!all_player_markers(L"checkpoint-health.txt"))return false;
    if(role_index==1&&!checkpoint_test_written){
        event(pawn,resolve(cp_probe_methods[cp_status_save]),nullptr);
        event(gi,resolve(cp_probe_methods[cp_stock_save]),nullptr);
        checkpoint_test_written=true;
    }
    if(view.latest.serial<=checkpoint_test_base||!view.latest.refresh)return false;
    if(!exists(marker(role_index,L"checkpoint-refresh.txt"))&&
       !checkpoint_test_file(view,primary,gi,L"checkpoint-refresh.txt",true))return false;
    return all_player_markers(L"checkpoint-refresh.txt")&&(!checkpoint_wipe||checkpoint_test_wipe(view,gi));
}
