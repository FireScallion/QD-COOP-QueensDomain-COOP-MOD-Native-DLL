static bool probe_persistent_save(Address engine){
    constexpr auto proof=L"persistent-save-cloned.txt";
    if(exists(marker(0,proof)))return true;
    if(!host)return false;
    static U::WeakKey constructor;
    if(!resolve(constructor))scan([](Address object){
        if(name(object)=="CreateSaveGameObject"&&exact_path(object,"/Script/Engine.GameplayStatics.CreateSaveGameObject"))
            remember(constructor,object);
    });
    const auto fn=resolve(constructor);
    const auto gi=object_field(engine,"GameInstance"),active=gi?object_field(gi,"ActiveSaveSlot"):0;
    if(!fn||!active)return false;
    const auto base=object_field(gi,"BaseActiveSave"),gs=resolve(gameplay);
    if(!gs||active!=base||at<std::uint16_t>(fn,182)!=16||
       !parameter(fn,"SaveGameClass",0,8)||!parameter(fn,"ReturnValue",8,8))return false;
    std::string report="CreateSaveGameObject parameters="+std::to_string(at<std::uint16_t>(fn,182))+
        " class_at="+std::to_string(parameter_offset(fn,"SaveGameClass",8))+
        " return_at="+std::to_string(parameter_offset(fn,"ReturnValue",8))+
        " same_base="+std::to_string(active==base)+"\n";
    unsigned fields{};
    for(auto type=at<Address>(active,16);type&&fields<256;type=at<Address>(type,64)){
        std::string path;
        if(!U::object_path(type,image+0x4A27F00,read_local,path))return false;
        report+=path+" size="+std::to_string(at<int>(type,88))+"\n";
        if(path=="/Script/Engine.SaveGame")break;
        for(auto field=at<Address>(type,80);field&&fields<256;field=at<Address>(field,32)){
            ++fields;
            std::string key,kind;
            const auto field_type=at<Address>(field,8),vtable=at<Address>(field,0);
            if(!field_type||!vtable||
               !U::name_string(image+0x4A27F00,at<U::NameKey>(field,40),read_local,key)||
               !U::name_string(image+0x4A27F00,at<U::NameKey>(field_type,0),read_local,kind))return false;
            char line[300]{};
            const auto flags=at<std::uint64_t>(field,64),copy=at<Address>(vtable,23*8);
            std::snprintf(line,sizeof(line),"%s %s offset=%d dim=%d size=%d flags=%llx copy=%llx\n",
                          key.c_str(),kind.c_str(),at<int>(field,76),at<int>(field,56),at<int>(field,60),
                          static_cast<unsigned long long>(flags),static_cast<unsigned long long>(copy-image));
            report+=line;
        }
    }
    if(!fields||fields>=256)return false;
    write_file(marker(0,L"persistent-save-layout.txt").c_str(),report.data(),static_cast<unsigned>(report.size()));
    const auto type=at<Address>(active,16);
    qdcoop::native_save_clone::Layout binding;
    if(!qdcoop::native_save_clone::bind(image,image+0x4a27f00,type,read_local,binding))return false;
    alignas(16) std::array<Address,2> args{type,0};
    event(gs,fn,args.data());
    const auto clone=args[1];
    if(!clone||!qdcoop::native_save_clone::copy(binding,reinterpret_cast<void*>(clone),reinterpret_cast<void*>(active)))
        return false;
    unsigned maps{};
    constexpr std::array containers{"SavableActorStates","Inventory","InventoryContents","Equipment","NamedSaveData"};
    for(const auto key:containers){
        const auto field=field_of(type,key,80);
        if(!field||world_map_property_type(field)!="MapProperty")return false;
        const auto offset=at<int>(field,76);
        const auto size=[&](Address object){return at<int>(object+offset,8)-at<int>(object+offset,52);};
        const auto count=size(active);
        if(count!=size(clone)||count<0||(count&&at<Address>(active,offset)==at<Address>(clone,offset)))return false;
        if(count)++maps;
    }
    const auto xp=field_of(type,"XPAmount",4),intro=field_of(type,"DoIntro",1);
    const auto read_intro=[&](Address object){return U::get_bool(reinterpret_cast<void*>(intro),reinterpret_cast<void*>(object),0);};
    if(!xp||!intro||!maps||at<int>(active,at<int>(xp,76))!=at<int>(clone,at<int>(xp,76))||
       read_intro(active)!=read_intro(clone))return false;
    const auto before=read_intro(active);
    U::set_bool(reinterpret_cast<void*>(intro),reinterpret_cast<void*>(clone),!before,0);
    if(read_intro(clone)==before||read_intro(active)!=before||
       object_field(gi,"ActiveSaveSlot")!=active||object_field(gi,"BaseActiveSave")!=active)return false;
    const auto cloned="copied="+std::to_string(binding.members.size())+" independent_nonempty_maps="+std::to_string(maps)+
        " independent_intro=1 stock_save_pointers_unchanged=1\n";
    write_file(marker(0,proof).c_str(),cloned.data(),static_cast<unsigned>(cloned.size()));
    return true;
}

static bool run_persistent_reload_stage(Address engine){
    constexpr std::array ready_markers{L"persistent-ready.txt",L"persistent-ready-return.txt"};
    constexpr std::array settled_markers{L"persistent-settled.txt",L"persistent-settled-return.txt"};
    static U::WeakKey open_level,convert_name,string_library,original_pawn;
    static bool fired{};
    static unsigned leg{};
    const unsigned legs=persistent_cross_lab?2u:1u;
    if(leg==legs)return true;
    const auto ready=ready_markers[leg],settled=settled_markers[leg];
    const bool intro=persistent_cross_lab&&leg==0;
    const auto destination=intro?"/Game/RuinsLevel/Maps/Intro_Level_00.Intro_Level_00":
                                 "/Game/RuinsLevel/Maps/Ruins_Level_00.Ruins_Level_00";
    const auto gi=object_field(engine,"GameInstance"),gs=resolve(gameplay),get_pawn=resolve(local_pawn_method);
    if(!gi||!gs||!get_pawn)return false;
    alignas(16) Parameters args{};
    if(!put(args,get_pawn,"WorldContextObject",gi)||!put(args,get_pawn,"PlayerIndex",std::int32_t{0}))return false;
    event(gs,get_pawn,args.data());
    const auto pawn=U::field<Address>(args.data(),16);
    if(!pawn)return false;
    if(original_pawn.serial==0){
        remember(original_pawn,pawn);
        write_file(marker(role_index,ready).c_str(),"ok",2);
    }else if(pawn!=resolve(original_pawn))remember(player,pawn);
    if(!all_player_markers(ready))return false;
    const auto status=read_native_status(core.status);
    if(status.empty())return false;
    if(host&&!fired){
        if(counter(status.c_str(),"\"cp_acks\":")<3)return false;
        if(!resolve(open_level)||!resolve(convert_name)||!resolve(string_library))scan([](Address object){
            const auto leaf=name(object);
            if(leaf=="OpenLevel"&&exact_path(object,"/Script/Engine.GameplayStatics.OpenLevel"))remember(open_level,object);
            if(leaf=="Conv_StringToName"&&exact_path(object,"/Script/Engine.KismetStringLibrary.Conv_StringToName"))
                remember(convert_name,object);
            if(leaf=="Default__KismetStringLibrary"&&exact_path(object,"/Script/Engine.Default__KismetStringLibrary"))
                remember(string_library,object);
        });
        const auto open=resolve(open_level),convert=resolve(convert_name),strings=resolve(string_library);
        if(!open||!convert||!strings||!parameter(convert,"inString",0,16)||!parameter(convert,"ReturnValue",16,8)||
           !parameter(open,"WorldContextObject",0,8)||!parameter(open,"LevelName",8,8)||
           !parameter(open,"bAbsolute",16,1)||!parameter(open,"Options",24,16))return false;
        const std::wstring path=intro?L"/Game/RuinsLevel/Maps/Intro_Level_00":L"/Game/RuinsLevel/Maps/Ruins_Level_00";
        struct String {const wchar_t* data;int count,capacity;};
        const auto size=static_cast<int>(path.size()+1);
        const String string{path.c_str(),size,size};
        args={};
        std::memcpy(args.data(),&string,sizeof(string));
        event(strings,convert,args.data());
        const auto level=U::field<U::NameKey>(args.data(),16);
        if(!level.index)return false;
        args={};
        if(!put(args,open,"WorldContextObject",gi)||!put(args,open,"LevelName",level)||
           !put(args,open,"bAbsolute",std::uint8_t{1}))return false;
        fired=true;
        const auto opened=marker(0,leg==0?L"persistent-opened.txt":L"persistent-opened-return.txt");
        write_file(opened.c_str(),destination,static_cast<unsigned>(std::strlen(destination)));
        event(gs,open,args.data());
        return false;
    }
    const auto level=at<Address>(pawn,32);
    const auto owner=level?object_field(level,"OwningWorld"):0;
    const auto world=owner?owner:level?at<Address>(level,32):0;
    if(pawn==resolve(original_pawn)||!world||!exact_path(world,destination)||
       counter(status.c_str(),host?"\"persistent_host_changes\":" : "\"persistent_guest_changes\":")!=leg+1||
       (!intro&&counter(status.c_str(),host?"\"world_arrival_published\":" : "\"world_arrival_applied\":")==0))return false;
    for(unsigned slot=0;slot<P::slots;++slot){
        P::Binding remote{};
        if(slot==role_index)continue;
        if(!core.inspect_player(slot,&remote)||!remote.visible||!remote.sequence)return false;
    }
    write_file(marker(role_index,settled).c_str(),"new stock pawn and same transport room",38);
    if(!all_player_markers(settled))return false;
    ++leg;
    fired=false;
    original_pawn={};
    return leg==legs;
}
