static Address stream_probe_property(Address owner,std::string_view field){
    return U::find_inherited_property(owner,image+0x4a27f00,field,read_local);
}
static U::WeakKey stream_probe_manager;
static void stream_probe_layout(std::string& out,Address owner,std::string_view name){
    const auto property=stream_probe_property(owner,name);
    if(!property){out+=std::string(name)+" unavailable\n";return;}
    const auto kind=world_map_property_type(property);
    out+=std::string(name)+" "+kind+" offset="+std::to_string(at<int>(property,76))+" width="+std::to_string(at<int>(property,60))+"\n";
    auto structure=property;
    if(kind=="ArrayProperty")structure=at<Address>(property,0x78);
    if(!structure||world_map_property_type(structure)!="StructProperty")return;
    const auto type=at<Address>(structure,0x78);
    std::string path;
    U::object_path(type,image+0x4a27f00,read_local,path);
    out+=path+" bytes="+std::to_string(at<int>(structure,60))+"\n";
    damage_layout_fields(out,type,false);
}
static void stream_probe_entries(std::string& out,Address instance,Address type,std::string_view name){
    const auto property=stream_probe_property(type,name);
    if(!property||world_map_property_type(property)!="ArrayProperty")return;
    const auto item=at<Address>(property,0x78);
    const auto fields=item?at<Address>(item,0x78):0;
    const auto list=stream_probe_property(fields,"List"),priority=stream_probe_property(fields,"Priority");
    if(!list||!priority||at<int>(list,60)!=8||at<int>(priority,60)!=4)return;
    struct Array {Address data;int count,capacity;};
    const auto entries=at<Array>(instance,at<int>(property,76));
    if(entries.count<0||entries.count>128||entries.capacity<entries.count||(entries.count&&!entries.data))return;
    const auto stride=at<int>(item,60);
    if(stride<12||stride>128)return;
    out+=std::string(name)+" live count="+std::to_string(entries.count)+"\n";
    for(int i=0;i<entries.count;++i){
        const auto row=entries.data+i*stride;
        const auto asset=at<Address>(row,at<int>(list,76));
        std::string path;
        U::object_path(asset,image+0x4a27f00,read_local,path);
        out+=path+" priority="+std::to_string(at<int>(row,at<int>(priority,76)))+"\n";
    }
}
static bool probe_native_streaming(Address engine){
    static bool tested{};
    if(tested)return true;
    Address instance{};
    scan([&](Address object){
        if(instance||(at<unsigned>(object,8)&0x10u))return;
        const auto type=at<Address>(object,16);
        if(name(type)=="FS_LevelManager"&&exact_path(type,"/Script/QueensDomain.FS_LevelManager"))instance=object;
    });
    const auto gi=object_field(engine,"GameInstance"),save=gi?object_field(gi,"ActiveSaveSlot"):0;
    if(!instance||!save)return false;
    const auto type=at<Address>(instance,16),save_type=at<Address>(save,16);
    remember(stream_probe_manager,instance);
    std::string report;
    U::object_path(instance,image+0x4a27f00,read_local,report);
    report+="\n";
    for(const auto name:{"LevelListStack","TriggerLevelListStack"}){
        stream_probe_layout(report,type,name);
        stream_probe_entries(report,instance,type,name);
    }
    stream_probe_layout(report,save_type,"LevelLoadingSuspendState");
    const auto state=stream_probe_property(save_type,"LevelLoadingSuspendState");
    const auto state_type=state?at<Address>(state,0x78):0;
    if(state_type)for(const auto name:{"SafeLocLevelListStack","SafeLocTriggerLevelListStack"})
        stream_probe_layout(report,state_type,name);
    for(const auto name:{"RestoreFromSuspendState","FlushLevelManagerActions","IsProcessingLevelChanges"}){
        const auto fn=test_world_method(type,name);
        if(!fn)return false;
        report+=std::string(name)+" params="+std::to_string(at<std::uint16_t>(fn,182))+"\n";
        damage_layout_fields(report,fn,true);
    }
    tested=true;
    write_file(marker(role_index,L"streaming-layout.txt").c_str(),report.data(),static_cast<unsigned>(report.size()));
    return true;
}
struct alignas(8) StreamProbeRow {Address list{};std::int32_t priority{};std::uint8_t trigger{};};
static_assert(sizeof(StreamProbeRow)==16);
struct StreamProbeArray {const StreamProbeRow* data{};int count{},capacity{};};
static_assert(sizeof(StreamProbeArray)==16);
static bool replay_stock_streams(Address engine){
    static U::WeakKey list_key;
    static bool fired{},finished{};
    if(finished)return true;
    if(!resolve(list_key))scan([](Address object){
        if(name(object)=="GreenRuins"&&exact_path(object,"/Game/RuinsLevel/Maps/LevelLists/GreenRuins.GreenRuins"))remember(list_key,object);
    });
    const auto manager=resolve(stream_probe_manager),list=resolve(list_key),gi=object_field(engine,"GameInstance");
    if(!manager||!list||!gi)return false;
    const auto type=at<Address>(manager,16);
    const auto restore=test_world_method(type,"RestoreFromSuspendState"),flush=test_world_method(type,"FlushLevelManagerActions");
    const auto busy=test_world_method(type,"IsProcessingLevelChanges");
    const auto prop=field_of(restore,"StateToRestore",32);
    if(!restore||!flush||!busy||!prop||!damage_probe_struct(prop)||
       !parameter(restore,"StateToRestore",0,32)||!parameter(flush,"WorldContextObject",0,8)||
       !parameter(flush,"bBlocking",8,1)||!parameter(busy,"WorldContextObject",0,8)||!parameter(busy,"ReturnValue",8,1))return false;
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&gi,sizeof(gi));
    event(manager,busy,args.data());
    if(args[8])return false;
    if(!fired){
        const StreamProbeRow row{list,7,0};
        const std::array<StreamProbeArray,2> input{{{&row,1,1},{}}};
        const auto init=reinterpret_cast<void(*)(Address,void*)>(image+0x11914f0);
        const auto destroy=reinterpret_cast<void(*)(Address,void*)>(image+0x118e4c0);
        const auto copy=reinterpret_cast<void(*)(Address,void*,const void*,int)>(image+0x118e4b0);
        alignas(16) Parameters owned{};
        init(prop,owned.data());
        copy(prop,owned.data(),input.data(),1);
        event(manager,restore,owned.data());
        destroy(prop,owned.data());
        args[8]=0;
        event(manager,flush,args.data());
        fired=true;
        write_file(marker(role_index,L"streaming-replay-fired.txt").c_str(),"ok",2);
        return false;
    }
    const auto stack=field_of(type,"LevelListStack",16);
    const auto values=stack?at<StreamProbeArray>(manager,at<int>(stack,76)):StreamProbeArray{};
    if(values.count!=1||!values.data||values.data[0].list!=list||values.data[0].priority!=7||values.data[0].trigger)return false;
    constexpr char report[]="GreenRuins priority=7 LevelListStack=1 stock streaming idle\n";
    write_file(marker(role_index,L"streaming-replay.txt").c_str(),report,sizeof(report)-1);
    finished=true;
    return true;
}
static bool run_world_stream_stage(Address engine,ULONGLONG time){
    static bool ready{};
    static ULONGLONG observed{};
    if(!probe_native_streaming(engine))return false;
    if(!ready){
        ready=true;
        write_file(marker(role_index,L"streaming-ready.txt").c_str(),"ok",2);
    }
    if(!all_player_markers(L"streaming-ready.txt"))return false;
    if(host&&!replay_stock_streams(engine))return false;
    const auto manager=resolve(stream_probe_manager),gi=object_field(engine,"GameInstance");
    if(!manager||!gi)return false;
    const auto type=at<Address>(manager,16),stack=field_of(type,"LevelListStack",16);
    const auto busy=test_world_method(type,"IsProcessingLevelChanges");
    if(!stack||!busy||!parameter(busy,"ReturnValue",8,1)||!parameter(busy,"WorldContextObject",0,8))return false;
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&gi,sizeof(gi));
    event(manager,busy,args.data());
    const auto items=at<StreamProbeArray>(manager,at<int>(stack,76));
    if(args[8]||items.count!=1||!items.data||items.data[0].priority!=7||items.data[0].trigger||
       !exact_path(items.data[0].list,"/Game/RuinsLevel/Maps/LevelLists/GreenRuins.GreenRuins"))return false;
    if(!observed)observed=time;
    if(time-observed<750)return false;
    write_file(marker(role_index,L"streaming-synchronized.txt").c_str(),"GreenRuins priority=7",21);
    return all_player_markers(L"streaming-synchronized.txt");
}

static bool arrival_fixture_free_pose;
using ArrivalFixtureStacks=std::array<std::vector<std::pair<Address,int>>,2>;
static bool arrival_fixture_stacks(Address engine,ArrivalFixtureStacks& out){
    const auto manager=resolve(stream_probe_manager),gi=object_field(engine,"GameInstance");
    if(!manager||!gi)return false;
    const auto type=at<Address>(manager,16);
    const auto busy=test_world_method(type,"IsProcessingLevelChanges");
    if(!busy||!parameter(busy,"ReturnValue",8,1)||!parameter(busy,"WorldContextObject",0,8))return false;
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&gi,sizeof(gi));
    event(manager,busy,args.data());
    if(args[8])return false;
    ArrivalFixtureStacks stacks;
    constexpr std::array names{"LevelListStack","TriggerLevelListStack"};
    for(unsigned i=0;i<stacks.size();++i){
        const auto field=field_of(type,names[i],16);
        if(!field)return false;
        const auto list=at<StreamProbeArray>(manager,at<int>(field,76));
        if(list.count<0||list.count>64||(list.count&&!list.data))return false;
        for(int row=0;row<list.count;++row)
            stacks[i].emplace_back(list.data[row].list,list.data[row].priority);
    }
    out=std::move(stacks);
    return true;
}
static bool run_world_arrival_stage(Address engine,ULONGLONG time){
    constexpr float yaw=68.f;
    static ULONGLONG departure{};
    static TestPlayerPose destination;
    static U::WeakKey set_control,get_control;
    static bool ready{},placed{},held{},verified{},released{};
    static std::uint8_t original_mode{};
    static ArrivalFixtureStacks original_stream;
    const auto pawn=resolve(held_pawn);
    const auto movement=pawn?object_field(pawn,"CharacterMovement"):0;
    const auto controller=pawn?object_field(pawn,"Controller"):0;
    const auto mode=movement?field_of(at<Address>(movement,16),"MovementMode",1):0;
    const auto flag=pawn?field_of(at<Address>(pawn,16),"QuickTeleporting",1):0;
    if(!mode||!flag||!controller||!probe_native_streaming(engine))return false;
    if(!resolve(set_control)||!resolve(get_control))scan([](Address object){
        if(name(object)=="SetControlRotation"&&exact_path(object,"/Script/Engine.Controller.SetControlRotation"))
            remember(set_control,object);
        if(name(object)=="GetControlRotation"&&exact_path(object,"/Script/Engine.Pawn.GetControlRotation"))
            remember(get_control,object);
    });
    const auto control=resolve(set_control);
    const auto view_fn=resolve(get_control);
    if(!control||!view_fn||!parameter(control,"NewRotation",0,12)||!parameter(view_fn,"ReturnValue",0,12))return false;
    const auto current_mode=at<std::uint8_t>(movement,at<int>(mode,76));
    const bool cancelling=world_arrival_cancel_lab&&role_index==3;
    if(!ready){
        if(!current_mode||(cancelling&&!arrival_fixture_stacks(engine,original_stream)))return false;
        ready=true;
        original_mode=current_mode;
        write_file(marker(role_index,L"arrival-ready.txt").c_str(),"ok",2);
    }
    if(!all_player_markers(L"arrival-ready.txt"))return false;
    if(cancelling&&held&&!verified){
        if(!core.stop())return false;
        const auto restored_mode=at<std::uint8_t>(movement,at<int>(mode,76));
        if(!released){
            if(restored_mode!=original_mode){phase=84;return false;}
            released=true;
            arrival_fixture_free_pose=false;
        }
        ArrivalFixtureStacks current;
        if(!arrival_fixture_stacks(engine,current)||current!=original_stream)return false;
        const auto pose=read_player_pose(pawn);
        if(std::abs(pose.position[0]-held_pose.position[0])>3.f)return false;
        constexpr char report[]="room left during streaming hold, stock movement and both original streaming stacks restored";
        write_file(marker(role_index,L"arrival-verified.txt").c_str(),report,sizeof(report)-1);
        verified=true;
    }
    if(host){
        if(!departure){
            U::set_bool(reinterpret_cast<void*>(flag),reinterpret_cast<void*>(pawn),true,0);
            departure=time;
            destination=held_pose;
            destination.position[0]+=600.f;
            destination.position[2]+=80.f;
            destination.yaw=yaw;
        }
        if(!placed){
            if(time-departure<250||!replay_stock_streams(engine))return false;
            if(!apply_test_pose(pawn,destination))return false;
            alignas(16) std::array<float,3> rotation{0,yaw,0};
            event(controller,control,rotation.data());
            held_pose=destination;
            U::set_bool(reinterpret_cast<void*>(flag),reinterpret_cast<void*>(pawn),false,0);
            placed=true;
        }
    }else if(!verified){
        if(!held&&current_mode)original_mode=current_mode;
        if(!current_mode){
            held=true;
            arrival_fixture_free_pose=true;
            if(cancelling)player_silence=true;
            write_file(marker(role_index,L"arrival-movement-held.txt").c_str(),"ok",2);
            return false;
        }
        if(!held)return false;
    }
    if(!verified){
        const auto pose=read_player_pose(pawn);
        TestPlayerPose expected=destination;
        if(!host){
            if(!read_expected_pose(0,1,expected))return false;
            expected.position[0]+=600.f;
            expected.position[2]+=80.f;
        }
        alignas(16) std::array<float,3> view{};
        event(pawn,view_fn,view.data());
        if(!host&&!released){
            released=true;
            char proof[256]{};
            std::snprintf(proof,sizeof(proof),"position=%.3f,%.3f,%.3f expected=%.3f,%.3f,%.3f yaw=%.2f movement_before=%u movement_after=%u",
                          pose.position[0],pose.position[1],pose.position[2],expected.position[0],expected.position[1],
                          expected.position[2],view[1],original_mode,current_mode);
            write_file(marker(role_index,L"arrival-movement-released.txt").c_str(),proof,static_cast<unsigned>(std::strlen(proof)));
        }
        if(!host&&current_mode!=original_mode)return false;
        for(unsigned i=0;i<3;++i)if(std::abs(pose.position[i]-expected.position[i])>3.f)return false;
        if(std::abs(std::remainder(view[1]-yaw,360.f))>.1f)return false;
        const auto manager=resolve(stream_probe_manager),gi=object_field(engine,"GameInstance");
        const auto type=manager?at<Address>(manager,16):0;
        const auto stack=type?field_of(type,"LevelListStack",16):0;
        const auto busy=type?test_world_method(type,"IsProcessingLevelChanges"):0;
        if(!gi||!stack||!busy)return false;
        alignas(16) Parameters args{};
        std::memcpy(args.data(),&gi,sizeof(gi));
        event(manager,busy,args.data());
        const auto items=at<StreamProbeArray>(manager,at<int>(stack,76));
        if(args[8]||items.count!=1||!items.data||items.data[0].priority!=7||
           !exact_path(items.data[0].list,"/Game/RuinsLevel/Maps/LevelLists/GreenRuins.GreenRuins"))return false;
        char proof[192]{};
        std::snprintf(proof,sizeof(proof),"position=%.3f,%.3f,%.3f yaw=%.2f movement_before=%u movement_after=%u held=%u stream=GreenRuins:7",
                      pose.position[0],pose.position[1],pose.position[2],view[1],original_mode,current_mode,held);
        write_file(marker(role_index,L"arrival-verified.txt").c_str(),proof,static_cast<unsigned>(std::strlen(proof)));
        verified=true;
        held_pose=pose;
        arrival_fixture_free_pose=false;
    }
    return all_player_markers(L"arrival-verified.txt");
}
