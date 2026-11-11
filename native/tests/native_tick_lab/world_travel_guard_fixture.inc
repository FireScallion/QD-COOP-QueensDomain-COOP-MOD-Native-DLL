static constexpr const char* travel_uber[]{"ExecuteUbergraph_FS_PlayerChar","ExecuteUbergraph_TeleporterUI","ExecuteUbergraph_SaveMenu"};
static constexpr const char* travel_persistent[]{"K2Node_CustomEvent_Consumable","K2Node_CustomEvent_Teleporter","K2Node_ComponentBoundEvent_Button_3"};
static constexpr unsigned travel_width[]{1,8,8};
static void scan_travel_methods(){
    scan([](Address object){
        const auto leaf=name(object);
        for(unsigned i=0;i<travel_test_methods.size();++i){
            if(resolve(travel_test_methods[i]))continue;
            const auto path=travel_test_paths[i];
            if(leaf==path.substr(path.find_last_of('.')+1)&&exact_path(object,path.data()))remember(travel_test_methods[i],object);
        }
    });
}
static std::uint16_t travel_original_word(Address function,unsigned index){
    const auto type=at<Address>(function,32),uber=test_world_method(type,travel_uber[index]);
    const auto property=uber?field_of(uber,travel_persistent[index],travel_width[index]):0;
    return property?static_cast<std::uint16_t>(0x64u|((property&255u)<<8)):0;
}
static Address make_travel_widget(Address pawn,Address type){
    const auto create=resolve(travel_test_methods[3]),library=resolve(travel_test_methods[4]);
    const auto controller=object_field(pawn,"Controller");
    if(!create||!library||!controller||!type||at<std::uint16_t>(create,182)!=32||parameter_offset(create,"ReturnValue",8)!=24)return 0;
    alignas(16) Parameters args{};
    if(!put(args,create,"WorldContextObject",pawn)||!put(args,create,"WidgetType",type)||!put(args,create,"OwningPlayer",controller))return 0;
    event(library,create,args.data());
    const auto result=U::field<Address>(args.data(),24);
    return result&&at<Address>(result,16)==type?result:0;
}
static bool travel_test_event(Address object,Address function,unsigned index){
    if(!object||!function||at<std::uint16_t>(function,182)!=travel_width[index])return false;
    const auto type=at<Address>(object,16),uber=test_world_method(type,travel_uber[index]);
    const auto storage=field_of(type,"UberGraphFrame",8);
    const auto member=uber?field_of(uber,travel_persistent[index],travel_width[index]):0;
    const auto frame=storage?at<Address>(object,at<int>(storage,76)):0;
    if(!member||!frame)return false;
    const auto offset=at<int>(member,76);
    alignas(16) Parameters args{};
    std::uint64_t before{};
    std::memcpy(&before,reinterpret_cast<const void*>(frame+offset),travel_width[index]);
    if(!index)args[0]=static_cast<std::uint8_t>(!before);
    event(object,function,args.data());
    std::uint64_t after{};
    std::memcpy(&after,reinterpret_cast<const void*>(frame+offset),travel_width[index]);
    return before==after;
}
static bool run_travel_guard_stage(){
    static bool tested{};
    if(tested)return all_player_markers(L"travel-guard.txt");
    if(std::ranges::any_of(travel_test_methods,[](auto key){return !resolve(key);})){scan_travel_methods();return false;}
    const auto pawn=local_test_pawn();
    if(!pawn)return false;
    for(unsigned i=0;i<3;++i){
        const auto fn=resolve(travel_test_methods[i]),code=at<Address>(fn,96);
        const auto word=host?travel_original_word(fn,i):std::uint16_t{0x0b04};
        if(!code||!word||at<int>(fn,104)!=36||at<std::uint16_t>(code,0)!=word)return false;
    }
    if(!host){
        const auto before=location(pawn);
        if(!travel_test_event(pawn,resolve(travel_test_methods[0]),0))return false;
        for(unsigned i=1;i<3;++i){
            const auto fn=resolve(travel_test_methods[i]);
            if(!travel_test_event(make_travel_widget(pawn,at<Address>(fn,32)),fn,i))return false;
        }
        if(location(pawn)!=before){phase=98;return false;}
    }
    tested=true;
    write_file(marker(role_index,L"travel-guard.txt").c_str(),host?"stock":"guarded",host?5:7);
    return all_player_markers(L"travel-guard.txt");
}
static bool verify_travel_guard_cleanup(){
    for(unsigned i=0;i<3;++i){
        const auto fn=resolve(travel_test_methods[i]);
        const auto code=fn?at<Address>(fn,96):0;
        if(!code||at<std::uint16_t>(code,0)!=travel_original_word(fn,i))return false;
    }
    write_file(marker(role_index,L"travel-guard-clean.txt").c_str(),"restored",8);
    return true;
}
