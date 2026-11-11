static constexpr std::array library_fixture_paths{
    "/Game/RuinsLevel/Maps/300_LIB.300_LIB.PersistentLevel.LIB_BookRecepticle_BP9",
    "/Game/RuinsLevel/Maps/300_LIB.300_LIB.PersistentLevel.LIB_BookRecepticle_BP8",
    "/Game/RuinsLevel/Maps/300_LIB.300_LIB.PersistentLevel.Lib_TreasureGate_BP"
};
static std::array<U::WeakKey,library_fixture_paths.size()> library_fixture_actors{};
static U::WeakKey library_fixture_key;
static int library_inventory_before{};

struct FixtureStreamList {const char* path;int priority;};
static bool fixture_stock_streams(Address engine,Address gi,std::span<const FixtureStreamList> lists){
    if(!probe_native_streaming(engine))return false;
    const auto manager=resolve(stream_probe_manager);
    if(!manager)return false;
    const auto type=at<Address>(manager,16);
    const auto restore=test_world_method(type,"RestoreFromSuspendState"),flush=test_world_method(type,"FlushLevelManagerActions");
    if(!restore||!flush||!parameter(restore,"StateToRestore",0,32)||
       !parameter(flush,"WorldContextObject",0,8)||!parameter(flush,"bBlocking",8,1))return false;
    const auto field=field_of(restore,"StateToRestore",32);
    if(!damage_probe_struct(field))return false;
    std::array<StreamProbeRow,4> entries{};
    if(lists.empty()||lists.size()>entries.size())return false;
    for(unsigned i=0;i<lists.size();++i){
        const auto list=load_test_asset(lists[i].path);
        if(!list)return false;
        entries[i]={list,lists[i].priority,0};
    }
    const auto count=static_cast<int>(lists.size());
    const std::array<StreamProbeArray,2> input{{{entries.data(),count,count},{}}};
    const auto init=reinterpret_cast<void(*)(Address,void*)>(image+0x11914f0);
    const auto destroy=reinterpret_cast<void(*)(Address,void*)>(image+0x118e4c0);
    const auto copy=reinterpret_cast<void(*)(Address,void*,const void*,int)>(image+0x118e4b0);
    alignas(16) Parameters args{},owned{};
    init(field,owned.data());
    copy(field,owned.data(),input.data(),1);
    event(manager,restore,owned.data());
    destroy(field,owned.data());
    std::memcpy(args.data(),&gi,8);
    args[8]=1;
    event(manager,flush,args.data());
    return true;
}
static bool library_fixture_stream(Address engine,Address gi){
    constexpr std::array lists{FixtureStreamList{"/Game/RuinsLevel/Maps/LevelLists/Library.Library",7}};
    return fixture_stock_streams(engine,gi,lists);
}
static std::optional<bool> library_fixture_bool(Address object,const char* name){
    const auto field=object?field_of(at<Address>(object,16),name,1):0;
    if(!field||world_map_property_type(field)!="BoolProperty")return std::nullopt;
    return U::get_bool(reinterpret_cast<void*>(field),reinterpret_cast<void*>(object),0);
}
static unsigned library_fixture_collision(Address book){
    const auto box=book?object_field(book,"InteractBox"):0;
    const auto fn=box?test_world_method(at<Address>(box,16),"GetCollisionEnabled"):0;
    if(!fn||!parameter(fn,"ReturnValue",0,1))return 255;
    std::uint8_t mode=255;
    event(box,fn,&mode);
    return mode;
}
static bool library_fixture_insert(Address book,Address pawn,Address key){
    const auto use=test_world_method(at<Address>(book,16),"TryUseKey");
    const auto fail=use?field_of(use,"Fail Message",24):0;
    if(!use||!fail||at<std::uint16_t>(use,182)!=48||at<int>(use,104)!=463||
       !parameter(use,"Key",0,8)||!parameter(use,"Player",8,8)||!parameter(use,"Success",16,1)||
       !parameter(use,"OverrideFailMessage",17,1)||!parameter(use,"Fail Message",24,24)||
       world_map_property_type(fail)!="TextProperty")return false;
    const auto vtable=at<Address>(fail,0);
    if(at<Address>(vtable,31*8)!=image+0x11a4420||at<Address>(vtable,30*8)!=image+0x119e940)return false;
    const auto init=reinterpret_cast<void(*)(Address,void*)>(image+0x11a4420);
    const auto destroy=reinterpret_cast<void(*)(Address,void*)>(image+0x119e940);
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&key,8);
    std::memcpy(args.data()+8,&pawn,8);
    init(fail,args.data()+24);
    event(book,use,args.data());
    destroy(fail,args.data()+24);
    return args[16]==1;
}
static bool run_world_library_stage(Address engine){
    constexpr auto ready=L"library-book-ready.txt",done=L"library-book-restored.txt";
    if(exists(marker(role_index,done)))return all_player_markers(done);
    static bool streaming{},inserted{};
    const auto gi=object_field(engine,"GameInstance"),save=gi?object_field(gi,"ActiveSaveSlot"):0;
    const auto pawn=resolve(held_pawn);
    if(!save||!pawn)return false;
    if(host&&!streaming){
        streaming=library_fixture_stream(engine,gi);
        return false;
    }
    if(std::ranges::any_of(library_fixture_actors,[](auto key){return !resolve(key);})){
        scan([](Address object){
            const auto leaf=name(object);
            for(unsigned i=0;i<library_fixture_paths.size();++i){
                const std::string_view path=library_fixture_paths[i];
                if(leaf==path.substr(path.rfind('.')+1)&&exact_path(object,path.data()))
                    remember(library_fixture_actors[i],object);
            }
        });
        return false;
    }
    const auto book=resolve(library_fixture_actors[0]),idle=resolve(library_fixture_actors[1]),
               gate=resolve(library_fixture_actors[2]);
    if(!resolve(library_fixture_key)){
        const auto key=load_test_asset("/Game/Items/KeyItems/BookKeys/QueerRomance/BookKey_00_QueerRomance_Item.BookKey_00_QueerRomance_Item");
        if(!key)return false;
        remember(library_fixture_key,key);
    }
    const auto key=resolve(library_fixture_key);
    const auto inventory=test_world_map(save,"InventoryContents"),items=test_world_map(save,"SavableActorItems");
    const auto keys=field_of(at<Address>(book,16),"GUIDs",16);
    if(!inventory.field||!items.field||!keys)return false;
    const auto mode=library_fixture_collision(idle);
    const auto disabled=library_fixture_bool(idle,"InteractIsDisabled"),flag=library_fixture_bool(gate,"SetPuzzleNSD");
    if(!disabled||!flag||mode>3||*flag!=host||*disabled==host||(host?mode==0:mode!=0))return false;
    const auto quantity=[&]{return test_world_find(inventory,key).value_or(0);};
    if(!exists(marker(role_index,ready))){
        if(object_field(book,"ContainedKey"))return false;
        library_inventory_before=quantity();
        if(!story_test_change(pawn,key,1)||quantity()!=library_inventory_before+1)return false;
        const std::string proof="untouched book disabled="+std::to_string(*disabled)+
            " collision="+std::to_string(mode)+" puzzle flag="+std::to_string(*flag)+"\n";
        write_file(marker(role_index,ready).c_str(),proof.data(),static_cast<unsigned>(proof.size()));
        return false;
    }
    if(!all_player_markers(ready))return false;
    if(host&&!inserted){
        inserted=library_fixture_insert(book,pawn,key);
        if(!inserted)phase=96;
        return false;
    }
    const auto contained=object_field(book,"ContainedKey");
    const auto mesh=object_field(book,"KeyMesh");
    const auto visible_fn=mesh?test_world_method(at<Address>(mesh,16),"IsVisible"):0;
    if(!visible_fn||!parameter(visible_fn,"ReturnValue",0,1))return false;
    std::uint8_t visible{};
    event(mesh,visible_fn,&visible);
    struct GuidArray { const TestWP::Guid* data;int count,capacity; };
    const auto guids=at<GuidArray>(book,at<int>(keys,76));
    int recorded{};
    if(guids.data&&guids.count>0&&guids.count<=16&&guids.capacity>=guids.count)
        for(int i=0;i<guids.count;++i)recorded+=test_world_find<Address>(items,guids.data[i])==key;
    const auto expected=library_inventory_before+static_cast<int>(!host);
    char report[160]{};
    const auto size=std::snprintf(report,sizeof(report),"contained=%u visible=%u map=%d inventory=%d expected=%d\n",
                                  contained==key,static_cast<unsigned>(visible),recorded,quantity(),expected);
    if(size>0)write_file(marker(role_index,L"library-book-progress.txt").c_str(),report,static_cast<unsigned>(size));
    if(contained!=key||!visible||recorded!=1||quantity()!=expected)return false;
    if(size>0)write_file(marker(role_index,done).c_str(),report,static_cast<unsigned>(size));
    return size>0&&all_player_markers(done);
}
static bool verify_world_library_cleanup(){
    const auto idle=resolve(library_fixture_actors[1]),gate=resolve(library_fixture_actors[2]);
    const auto collision=library_fixture_collision(idle);
    if(!idle||!gate||library_fixture_bool(idle,"InteractIsDisabled")!=false||
       library_fixture_bool(gate,"SetPuzzleNSD")!=true||!collision||collision>3)return false;
    constexpr char proof[]="untouched book interaction and configured puzzle flag restored";
    write_file(marker(role_index,L"library-book-clean.txt").c_str(),proof,sizeof(proof)-1);
    return true;
}
