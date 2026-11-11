template<class T> static T pause_world_query(Address pawn,Address cdo,unsigned method,unsigned at){
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&pawn,sizeof(pawn));
    event(cdo,resolve(pause_probe_methods[method]),args.data());
    return U::field<T>(args.data(),at);
}
static Address pause_test_widget(Address pawn){
    const auto fn=resolve(pause_probe_methods[pause_find_widgets]),type=resolve(pause_probe_methods[pause_menu_class]);
    alignas(16) Parameters args{};
    const auto found=parameter_offset(fn,"FoundWidgets",16);
    if(found<0||!put(args,fn,"WorldContextObject",pawn)||!put(args,fn,"WidgetClass",type)||
       !put(args,fn,"TopLevelOnly",std::uint8_t{1}))return 0;
    event(resolve(pause_probe_methods[pause_widget_lib]),fn,args.data());
    struct WidgetArray {Address* data;int count,capacity;};
    const auto widgets=U::field<WidgetArray>(args.data(),found);
    Address visible{};
    if(widgets.data&&widgets.count>0&&widgets.count<=64&&widgets.capacity>=widgets.count){
        for(int i=0;i<widgets.count;++i){
            std::uint8_t in_viewport{},shown{};
            if(!widgets.data[i])continue;
            event(widgets.data[i],resolve(pause_probe_methods[pause_viewport]),&in_viewport);
            event(widgets.data[i],resolve(pause_probe_methods[pause_visible]),&shown);
            if(in_viewport&&shown){visible=widgets.data[i];break;}
        }
    }
    if(widgets.data)reinterpret_cast<void(*)(void*)>(image+0xeae0b0)(widgets.data);
    return visible;
}
static std::string pause_test_state(Address pawn,Address gm){
    auto menu=object_field(pawn,"CurrentGameMenu");
    if(!menu)menu=pause_test_widget(pawn);
    const auto cdo=resolve(gameplay);
    const auto clock=pause_world_query<float>(pawn,resolve(library),pause_clock,8);
    const auto dilation=pause_world_query<float>(pawn,cdo,pause_dilation,8);
    std::uint8_t menu_time{},enemy_time{},in_viewport{},visible{};
    event(gm,resolve(pause_probe_methods[pause_menu_time]),&menu_time);
    event(gm,resolve(pause_probe_methods[pause_enemy_time]),&enemy_time);
    std::string menu_path;
    if(menu){
        U::object_path(at<Address>(menu,16),image+0x4a27f00,read_local,menu_path);
        event(menu,resolve(pause_probe_methods[pause_viewport]),&in_viewport);
        event(menu,resolve(pause_probe_methods[pause_visible]),&visible);
    }
    return "paused="+std::to_string(pause_world_query<std::uint8_t>(pawn,cdo,pause_paused,8))+
           " game_time="+std::to_string(clock)+" dilation="+std::to_string(dilation)+
           " gm_menu_pause="+std::to_string(menu_time)+" gm_enemy_pause="+std::to_string(enemy_time)+
           " menu="+menu_path+" in_viewport="+std::to_string(in_viewport)+" visible="+std::to_string(visible)+"\n";
}
static bool probe_stock_pause(){
    static unsigned stage;
    static ULONGLONG opened_at;
    static std::string report;
    if(stage==2)return true;
    const auto pawn=local_test_pawn();
    if(!pawn)return false;
    const auto gm=pause_world_query<Address>(pawn,resolve(gameplay),pause_gm,8);
    if(!gm)return false;
    if(stage==0){
        for(unsigned i=0;i<pause_probe_methods.size();++i){
            const auto fn=resolve(pause_probe_methods[i]);
            report+=pause_probe_paths[i];
            if(i>=pause_widget_lib){report+='\n';continue;}
            char exec[32]{};
            std::snprintf(exec,sizeof(exec),"0x%llx",static_cast<unsigned long long>(at<Address>(fn,216)-image));
            report+=" bytes="+std::to_string(at<int>(fn,104))+" parms="+std::to_string(at<std::uint16_t>(fn,182))+" exec="+exec+"\n";
            for(auto field=at<Address>(fn,80);field;field=at<Address>(field,32)){
                if(!(at<std::uint64_t>(field,64)&0x80u))continue;
                std::string name;
                if(U::name_string(image+0x4a27f00,at<U::NameKey>(field,40),read_local,name))
                    report+=" "+name+" at="+std::to_string(at<int>(field,76))+" size="+std::to_string(at<int>(field,60))+"\n";
            }
        }
        report+="before "+pause_test_state(pawn,gm);
        std::uint8_t no{};
        event(pawn,resolve(pause_probe_methods[pause_open]),&no);
        report+="open "+pause_test_state(pawn,gm);
        write_file(marker(role_index,L"stock-pause-layout.txt").c_str(),report.data(),static_cast<unsigned>(report.size()));
        opened_at=GetTickCount64();
        ++stage;
        return false;
    }
    if(GetTickCount64()-opened_at<650)return false;
    report+="after650ms "+pause_test_state(pawn,gm);
    std::uint8_t no{};
    event(gm,resolve(pause_probe_methods[pause_set_menu]),&no);
    event(gm,resolve(pause_probe_methods[pause_set_enemy]),&no);
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&pawn,sizeof(pawn));
    event(resolve(gameplay),resolve(pause_probe_methods[pause_unpause]),args.data());
    if(const auto menu=pause_test_widget(pawn))event(menu,resolve(pause_probe_methods[pause_remove]),nullptr);
    report+="restored "+pause_test_state(pawn,gm);
    write_file(marker(role_index,L"stock-pause-layout.txt").c_str(),report.data(),static_cast<unsigned>(report.size()));
    ++stage;
    return true;
}
static bool pause_test_overlay(Address pawn){
    const auto property=field_of(at<Address>(pawn,16),"ForceNoOverlayPause",1);
    return property&&U::get_bool(reinterpret_cast<void*>(property),reinterpret_cast<void*>(pawn),0);
}
static bool pause_test_global(Address pawn,float value){
    const auto fn=resolve(pause_probe_methods[pause_global_set]);
    alignas(16) Parameters args{};
    if(!fn||at<std::uint16_t>(fn,182)!=12||!put(args,fn,"WorldContextObject",pawn)||
       !put(args,fn,"TimeDilation",value))return false;
    event(resolve(gameplay),fn,args.data());
    return true;
}
static bool run_world_pause_stage(){
    static unsigned stage;
    static ULONGLONG at_time;
    static float initial,progress;
    if(stage==4)return all_player_markers(L"world-pause-ok.txt");
    const auto pawn=local_test_pawn(),gs=resolve(gameplay),kismet=resolve(library);
    if(!pawn||!gs||!kismet)return false;
    const auto gm=pause_world_query<Address>(pawn,gs,pause_gm,8);
    if(!gm)return false;
    const auto now=GetTickCount64();
    if(stage==0){
        const auto fn=resolve(pause_probe_methods[pause_unpause]);
        if(!pause_test_overlay(pawn)||!fn||at<Address>(fn,216)==image+0x2dc3580)return false;
        initial=pause_world_query<float>(pawn,kismet,pause_clock,8);
        std::uint8_t no{};
        event(pawn,resolve(pause_probe_methods[pause_open]),&no);
        at_time=now;
        ++stage;
        return false;
    }
    if(stage==1){
        if(now-at_time<650)return false;
        progress=pause_world_query<float>(pawn,kismet,pause_clock,8)-initial;
        if(!pause_test_widget(pawn)||pause_world_query<std::uint8_t>(pawn,gs,pause_paused,8)||progress<.25f)return false;
        std::uint8_t yes=1;
        event(gm,resolve(pause_probe_methods[pause_set_menu]),&yes);
        event(gm,resolve(pause_probe_methods[pause_set_enemy]),&yes);
        at_time=now;
        ++stage;
        return false;
    }
    if(stage==2){
        if(now-at_time<150)return false;
        std::uint8_t menu{},enemy{};
        event(gm,resolve(pause_probe_methods[pause_menu_time]),&menu);
        event(gm,resolve(pause_probe_methods[pause_enemy_time]),&enemy);
        if(menu||enemy||!pause_test_widget(pawn)||!pause_test_global(pawn,.25f))return false;
        at_time=now;
        ++stage;
        return false;
    }
    if(now-at_time<150)return false;
    const auto scale=pause_world_query<float>(pawn,gs,pause_dilation,8);
    const auto widget=pause_test_widget(pawn);
    if(!widget||std::abs(scale-.25f)>.001f||!pause_test_global(pawn,1.f))return false;
    event(widget,resolve(pause_probe_methods[pause_remove]),nullptr);
    char report[150]{};
    const auto size=std::snprintf(report,sizeof(report),"visible=1 world_time_delta=%.4f menu_pause=0 enemy_pause=0 independent_scale=%.2f\n",
                                  progress,scale);
    if(size>0)write_file(marker(role_index,L"world-pause-ok.txt").c_str(),report,static_cast<unsigned>(size));
    ++stage;
    return false;
}
static bool verify_pause_cleanup(){
    const auto pawn=local_test_pawn(),setter=resolve(pause_probe_methods[pause_unpause]);
    const bool restored=pawn&&setter&&at<Address>(setter,216)==image+0x2dc3580&&!pause_test_overlay(pawn);
    if(restored)write_file(marker(role_index,L"world-pause-clean.txt").c_str(),"ok",2);
    return restored;
}
static bool photo_test_visible(Address pawn){
    const auto hud=object_field(pawn,"PhotomodeHUD");
    if(!hud||!exact_path(at<Address>(hud,16),"/Game/UI/PhotoMode/PhotoMode_HUD.PhotoMode_HUD_C"))return false;
    std::uint8_t viewport{},visible{};
    event(hud,resolve(pause_probe_methods[pause_viewport]),&viewport);
    event(hud,resolve(pause_probe_methods[pause_visible]),&visible);
    return viewport&&visible;
}
static bool probe_stock_photo(){
    static unsigned stage;
    static ULONGLONG at_time;
    static float initial;
    static U::WeakKey photo_pawn;
    if(stage==2)return true;
    const auto pawn=stage?resolve(photo_pawn):local_test_pawn();
    const auto gs=resolve(gameplay),kismet=resolve(library);
    const auto global=resolve(pause_probe_methods[pause_global_set]);
    const auto enter=resolve(photo_probe_methods[0]),toggle=resolve(photo_probe_methods[1]);
    if(!pawn||!gs||!kismet||!global||!enter||!toggle||at<std::uint16_t>(enter,182)||at<std::uint16_t>(toggle,182))return false;
    const auto now=GetTickCount64();
    if(stage==0){
        remember(photo_pawn,pawn);
        initial=pause_world_query<float>(pawn,kismet,pause_clock,8);
        event(pawn,enter,nullptr);
        at_time=now;
        ++stage;
        return false;
    }
    if(now-at_time<650)return false;
    const auto visible=photo_test_visible(pawn);
    const auto elapsed=pause_world_query<float>(pawn,kismet,pause_clock,8)-initial;
    const auto scale=pause_world_query<float>(pawn,gs,pause_dilation,8);
    const auto local=local_test_pawn();
    std::string path;
    if(local)U::object_path(at<Address>(local,16),image+0x4a27f00,read_local,path);
    event(pawn,toggle,nullptr);
    const auto restored=pause_world_query<float>(pawn,gs,pause_dilation,8);
    char report[400]{};
    const auto size=std::snprintf(report,sizeof(report),"global_exec=0x%llx parms=%u visible=%u scale=%.8f game_delta=%.6f same_pawn=%u pawn=%s restored_scale=%.4f\n",
        static_cast<unsigned long long>(at<Address>(global,216)-image),at<std::uint16_t>(global,182),
        static_cast<unsigned>(visible),scale,elapsed,static_cast<unsigned>(local==pawn),path.c_str(),restored);
    if(size>0&&size<static_cast<int>(sizeof(report)))write_file(marker(role_index,L"stock-photo.txt").c_str(),report,static_cast<unsigned>(size));
    ++stage;
    return true;
}
static bool run_world_photo_stage(){
    static unsigned stage;
    static ULONGLONG at_time;
    static float initial,unmasked;
    if(stage==3)return all_player_markers(L"world-photo-ok.txt");
    const auto pawn=local_test_pawn(),gs=resolve(gameplay),kismet=resolve(library);
    const auto setter=resolve(pause_probe_methods[pause_global_set]);
    const auto enter=resolve(photo_probe_methods[0]),toggle=resolve(photo_probe_methods[1]);
    if(!pawn||!gs||!kismet||!setter||!enter||!toggle)return false;
    if(stage==0){
        if(at<Address>(setter,216)==image+0x2dc39f0||photo_test_visible(pawn)||pause_test_widget(pawn)||
           !pause_test_global(pawn,.005f))return false;
        unmasked=pause_world_query<float>(pawn,gs,pause_dilation,8);
        if(std::abs(unmasked-.005f)>.0001f||!pause_test_global(pawn,1.f))return false;
        event(pawn,enter,nullptr);
        ++stage;
        return false;
    }
    if(!photo_test_visible(pawn))return false;
    const auto now=GetTickCount64();
    if(stage==1){
        if(!pause_test_global(pawn,.005f))return false;
        if(std::abs(pause_world_query<float>(pawn,gs,pause_dilation,8)-1.f)>.001f)return false;
        initial=pause_world_query<float>(pawn,kismet,pause_clock,8);
        at_time=now;
        ++stage;
        return false;
    }
    if(now-at_time<650)return false;
    const auto elapsed=pause_world_query<float>(pawn,kismet,pause_clock,8)-initial;
    const auto scale=pause_world_query<float>(pawn,gs,pause_dilation,8);
    if(elapsed<.25f||std::abs(scale-1.f)>.001f)return false;
    event(pawn,toggle,nullptr);
    char report[150]{};
    const auto size=std::snprintf(report,sizeof(report),"photo_visible=1 modal_scale=%.4f game_delta=%.4f outside_scale=%.5f\n",
                                  scale,elapsed,unmasked);
    if(size>0)write_file(marker(role_index,L"world-photo-ok.txt").c_str(),report,static_cast<unsigned>(size));
    ++stage;
    return false;
}
static bool verify_photo_cleanup(){
    const auto setter=resolve(pause_probe_methods[pause_global_set]);
    const bool restored=setter&&at<Address>(setter,216)==image+0x2dc39f0&&verify_pause_cleanup();
    if(restored)write_file(marker(role_index,L"world-photo-clean.txt").c_str(),"ok",2);
    return restored;
}
