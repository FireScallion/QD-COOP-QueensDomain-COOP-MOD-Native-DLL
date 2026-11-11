static bool story_test_change(Address pawn,Address item,int delta){
    static constexpr std::array paths{
        "/Game/Blueprints/SaveSystem/SaveFunctions.Default__SaveFunctions_C",
        "/Game/Blueprints/SaveSystem/SaveFunctions.SaveFunctions_C.AddItemToInventory",
        "/Game/Blueprints/SaveSystem/SaveFunctions.SaveFunctions_C.TryConsumingItemFromInventory",
        "/Script/Engine.KismetSystemLibrary.Conv_ObjectToSoftObjectReference"
    };
    static std::array<U::WeakKey,paths.size()> methods{};
    if(std::ranges::any_of(methods,[](auto key){return !resolve(key);})){
        scan([](Address object){
            const auto leaf=name(object);
            for(unsigned i=0;i<paths.size();++i){
                const std::string_view path=paths[i];
                if(leaf==path.substr(path.rfind('.')+1)&&exact_path(object,paths[i]))remember(methods[i],object);
            }
        });
    }
    const auto cdo=resolve(methods[0]),fn=resolve(methods[delta>0?1:2]),convert=resolve(methods[3]);
    const auto kismet=resolve(body_methods[body_kismet]);
    if(!cdo||!fn||!convert||!kismet||!parameter(convert,"Object",0,8)||!parameter(convert,"ReturnValue",8,40)||
       !parameter(fn,"Item",0,40)||!parameter(fn,"Count",40,4)||!parameter(fn,"__WorldContext",48,8)||
       (delta<0&&!parameter(fn,"Success",56,1)))return false;
    alignas(16) Parameters soft{},args{};
    std::memcpy(soft.data(),&item,sizeof(item));
    event(kismet,convert,soft.data());
    std::memcpy(args.data(),soft.data()+8,40);
    const int count=std::abs(delta);
    std::memcpy(args.data()+40,&count,sizeof(count));
    std::memcpy(args.data()+48,&pawn,sizeof(pawn));
    event(cdo,fn,args.data());
    return delta>0||args[56];
}

static bool run_world_story_stage(Address engine,ULONGLONG time){
    static U::WeakKey duck_key;
    static std::array<U::NameKey,3> keys{};
    static bool prepared{},claimed{},unlocked{},earlier{};
    static ULONGLONG ready_at{},consumed_at{};
    const auto gi=object_field(engine,"GameInstance"),save=gi?object_field(gi,"ActiveSaveSlot"):0;
    const auto pawn=resolve(held_pawn);
    if(!save||!pawn)return false;
    const auto names=test_world_map(save,"NamedSaveData"),inventory=test_world_map(save,"InventoryContents");
    if(!names.field||!inventory.field)return false;
    if(!resolve(duck_key)){
        const auto item=load_test_asset("/Game/Items/KeyItems/ShopProgressionKey/StoneDuckShopUnlock.StoneDuckShopUnlock");
        if(!item)return false;
        remember(duck_key,item);
    }
    const auto duck=resolve(duck_key);
    const auto count=[&]{return test_world_find(inventory,duck).value_or(0);};
    if(!prepared){
        if(!resolve(world_map_methods[5])||!resolve(world_map_methods[6]))return false;
        keys={test_world_name(L"ShrineBossRewardCount"),test_world_name(L"ShopUnlockFinished"),
              test_world_name(L"QDCoopPersonal~story~stoneDuck")};
        if(std::ranges::any_of(keys,[](auto key){return !key.index;}))return false;
        for(auto key:keys)test_world_put(names,key,std::nullopt);
        if(const auto quantity=count();quantity&&!story_test_change(pawn,duck,-quantity))return false;
        if(role_index==2&&(!story_test_change(pawn,duck,1)||count()!=1))return false;
        if(role_index==3)test_world_put(names,keys[2],1);
        prepared=true;
        ready_at=time;
    }
    if(!exists(marker(role_index,L"story-duck-unclaimed.txt"))){
        if(time-ready_at<1500||count()!=static_cast<int>(role_index==2))return false;
        write_file(marker(role_index,L"story-duck-unclaimed.txt").c_str(),"ok",2);
    }
    if(!all_player_markers(L"story-duck-unclaimed.txt"))return false;
    if(host&&!claimed){test_world_put(names,keys[0],1);claimed=true;}
    if(!exists(marker(role_index,L"story-duck-claimed.txt"))){
        if(test_world_find(names,keys[0])!=1||test_world_find(names,keys[2])!=1||count()!=1)return false;
        constexpr char proof[]="stock inventory count=1";
        write_file(marker(role_index,L"story-duck-claimed.txt").c_str(),proof,sizeof(proof)-1);
    }
    if(!all_player_markers(L"story-duck-claimed.txt"))return false;
    if(host&&!unlocked){test_world_put(names,keys[1],1);unlocked=true;}
    if(!exists(marker(role_index,L"story-duck-consumed.txt"))){
        if(test_world_find(names,keys[1])!=1)return false;
        if(!consumed_at){
            if(!story_test_change(pawn,duck,-1)||count()!=0)return false;
            consumed_at=time;
        }
        if(time-consumed_at<1500||count()!=0)return false;
        constexpr char proof[]="stock consume, no regrant after unlock";
        write_file(marker(role_index,L"story-duck-consumed.txt").c_str(),proof,sizeof(proof)-1);
    }
    if(!all_player_markers(L"story-duck-consumed.txt"))return false;
    if(host&&!earlier){test_world_put(names,keys[1],std::nullopt);earlier=true;}
    if(test_world_find(names,keys[1])||test_world_find(names,keys[2])!=1||count()!=1)return false;
    constexpr char proof[]="earlier locked world repaired existing receipt";
    write_file(marker(role_index,L"story-duck-repaired.txt").c_str(),proof,sizeof(proof)-1);
    return all_player_markers(L"story-duck-repaired.txt");
}
