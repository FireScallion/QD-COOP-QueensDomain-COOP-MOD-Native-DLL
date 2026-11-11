struct SafeSaveTestArray {Address data;int count,capacity;};
static bool safe_save_test_array(SafeSaveTestArray expected,SafeSaveTestArray actual,unsigned width){
    if(expected.count!=actual.count||actual.count<0||actual.count>256||actual.capacity<actual.count||
       expected.capacity<expected.count||(actual.count&&(!actual.data||!expected.data)))return false;
    for(int i=0;i<actual.count;++i)
        if(std::memcmp(reinterpret_cast<const void*>(expected.data+i*16),
                       reinterpret_cast<const void*>(actual.data+i*16),width))return false;
    return true;
}
static bool run_guest_safe_save_stage(Address engine){
    constexpr auto proof=L"guest-safe-save.txt";
    constexpr auto warmup=L"guest-safe-save-regular.txt";
    if(exists(marker(role_index,proof)))return all_player_markers(proof);
    TestCP::BindingView view{};
    if(!core.inspect_checkpoint(&view)||!(view.flags&1))return false;
    const auto gi=object_field(engine,"GameInstance"),active=object_field(gi,"ActiveSaveSlot"),
               base=object_field(gi,"BaseActiveSave"),pawn=resolve(held_pawn);
    if(!gi||!active||!base||!pawn)return false;
    const auto primary=checkpoint_test_primary(gi);
    if(primary.empty())return false;
    const auto type=at<Address>(active,16),pawn_type=at<Address>(pawn,16);
    const auto suspended=field_of(type,"IsSuspendSave",1),xp=field_of(type,"XPAmount",4),
               safe=field_of(pawn_type,"LastSafeLocation",48),midair=field_of(pawn_type,"SafeLocationIsForcedMidair",1);
    if(!suspended||!xp||!safe||!midair)return false;
    const auto get_bool=[](Address prop,Address object){return U::get_bool(reinterpret_cast<void*>(prop),reinterpret_cast<void*>(object),0);};
    const auto set_bool=[](Address prop,Address object,bool value){U::set_bool(reinterpret_cast<void*>(prop),reinterpret_cast<void*>(object),value,0);};
    const auto read_disk=[&](const std::wstring& path){
        alignas(16) Parameters args{};
        const auto slot=L"FULLPATH:"+path;
        checkpoint_test_string(args,slot);
        event(resolve(gameplay),resolve(cp_probe_methods[cp_load]),args.data());
        return U::field<Address>(args.data(),24);
    };
    const auto save=[&]{
        event(pawn,resolve(cp_probe_methods[cp_status_save]),nullptr);
        event(gi,resolve(cp_probe_methods[cp_stock_save]),nullptr);
        return read_disk(primary);
    };
    if(get_bool(midair,pawn))return false;
    if(!exists(marker(role_index,warmup))){
        if(!host)set_bool(midair,pawn,true);
        const auto disk=save();
        if(!host)set_bool(midair,pawn,false);
        if(!disk||at<Address>(disk,16)!=type||get_bool(suspended,disk)){
            phase=95;
            return false;
        }
        char report[175]{};
        const auto count=std::snprintf(report,sizeof(report),
            "regular=1 forced_midair=%u stock_active_changed=%u stock_base_changed=%u\n",
            !host,object_field(gi,"ActiveSaveSlot")!=active,object_field(gi,"BaseActiveSave")!=base);
        if(count>0)write_file(marker(role_index,warmup).c_str(),report,static_cast<unsigned>(count));
        return false;
    }
    if(!all_player_markers(warmup))return false;
    const auto before=get_bool(suspended,active);

    if(!resolve(stream_probe_manager)){
        const auto level=at<Address>(pawn,32);
        scan([&](Address object){
            if(resolve(stream_probe_manager)||(at<unsigned>(object,8)&0x10u)||at<Address>(object,32)!=level)return;
            const auto klass=at<Address>(object,16);
            if(name(klass)=="FS_LevelManager"&&exact_path(klass,"/Script/QueensDomain.FS_LevelManager"))
                remember(stream_probe_manager,object);
        });
    }
    const auto manager=resolve(stream_probe_manager),music=object_field(gi,"MusicController");
    if(!manager||!music)return false;
    const auto level_get=test_world_method(at<Address>(manager,16),"GetSafeLocationSuspendState"),
               music_get=test_world_method(at<Address>(music,16),"GetSafeLocationSuspendState");
    if(!level_get||!music_get||!parameter(level_get,"ReturnValue",0,32)||!parameter(music_get,"ReturnValue",0,16))return false;
    const auto level_return=field_of(level_get,"ReturnValue",32),music_return=field_of(music_get,"ReturnValue",16);
    const auto save_pos=field_of(type,"SaveLocation",48),suspend_pos=field_of(type,"SuspendLocation",48),
               forced=field_of(type,"SuspendPositionIsForcedMidair",1),levels=field_of(type,"LevelLoadingSuspendState",32),
               areas=field_of(type,"MusicAreaSuspendState",16);
    if(!level_return||!music_return||!save_pos||!suspend_pos||!forced||!levels||!areas)return false;

    const auto safe_at=pawn+at<int>(safe,76);
    const auto original_safe=at<std::array<float,12>>(safe_at,0);
    auto target=original_safe;
    target[4]+=25.f*role_index;
    std::memcpy(reinterpret_cast<void*>(safe_at),target.data(),sizeof(target));
    const int personal_xp=700+static_cast<int>(role_index);
    std::memcpy(reinterpret_cast<void*>(active+at<int>(xp,76)),&personal_xp,sizeof(personal_xp));
    alignas(16) std::array<SafeSaveTestArray,2> expected_levels{};
    alignas(16) SafeSaveTestArray expected_music{};
    event(manager,level_get,expected_levels.data());
    event(music,music_get,&expected_music);
    const auto disk=save();
    std::memcpy(reinterpret_cast<void*>(safe_at),original_safe.data(),sizeof(original_safe));
    const auto backup=read_disk(primary.substr(0,primary.size()-4)+L"_backup.sav");
    const auto verify=[&](Address object){
        if(!object||at<Address>(object,16)!=type||get_bool(suspended,object)!=!host||
           at<int>(object,at<int>(xp,76))!=personal_xp)return false;
        if(host)return true;
        for(const auto prop:{save_pos,suspend_pos}){
            const auto transform=at<std::array<float,12>>(object,at<int>(prop,76));
            for(unsigned i:{0u,1u,2u,3u,4u,5u,6u,8u,9u,10u})if(target[i]!=transform[i])return false;
        }
        const auto saved_levels=at<std::array<SafeSaveTestArray,2>>(object,at<int>(levels,76));
        return !get_bool(forced,object)&&safe_save_test_array(expected_levels[0],saved_levels[0],13)&&
            safe_save_test_array(expected_levels[1],saved_levels[1],13)&&
            safe_save_test_array(expected_music,at<SafeSaveTestArray>(object,at<int>(areas,76)),16);
    };
    const bool primary_ok=verify(disk),backup_ok=verify(backup),live_ok=object_field(gi,"ActiveSaveSlot")==active&&
        object_field(gi,"BaseActiveSave")==base&&get_bool(suspended,active)==before;
    char report[256]{};
    const auto count=std::snprintf(report,sizeof(report),
        "primary=%u backup=%u live=%u suspend=%u xp=%d feet=%.3f,%.3f,%.3f levels=%d,%d music=%d\n",
        primary_ok,backup_ok,live_ok,disk&&get_bool(suspended,disk),personal_xp,target[4],target[5],target[6],
        expected_levels[0].count,expected_levels[1].count,expected_music.count);
    const auto destroy=reinterpret_cast<void(*)(Address,void*)>(image+0x118e4c0);
    const auto destroy_array=reinterpret_cast<void(*)(Address,void*)>(image+0x11703e0);
    destroy(level_return,expected_levels.data());
    destroy_array(music_return,&expected_music);
    if(!primary_ok||!backup_ok||!live_ok||
       (!host&&!CopyFileW(primary.c_str(),marker(role_index,L"guest-safe-reload.sav").c_str(),TRUE))){
        if(count>0)write_file(marker(role_index,L"guest-safe-save-failed.txt").c_str(),report,static_cast<unsigned>(count));
        phase=95;
        return false;
    }
    if(count>0)write_file(marker(role_index,proof).c_str(),report,static_cast<unsigned>(count));
    return count>0&&all_player_markers(proof);
}
