static constexpr std::array castle_fixture_paths{
    "/Game/RuinsLevel/Maps/500_AUT.500_AUT.PersistentLevel.CastleBridge_Gate_Rear",
    "/Game/RuinsLevel/Maps/500_AUT.500_AUT.PersistentLevel.OW_CastleDoor_00_SM"
};
static std::array<U::WeakKey,2> castle_fixture_doors{};
static U::WeakKey fixture_stream_lookup;
static bool fixture_visible_stream(Address gi,const wchar_t* path,bool& requested){
    const auto get=resolve(fixture_stream_lookup),gs=resolve(gameplay);
    if(!get||!gs||!parameter(get,"WorldContextObject",0,8)||!parameter(get,"PackageName",8,8)||
       !parameter(get,"ReturnValue",16,8))return false;
    const auto package=test_world_name(path);
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&gi,8);
    std::memcpy(args.data()+8,&package,8);
    event(gs,get,args.data());
    const auto stream=U::field<Address>(args.data(),16);
    if(!stream)return false;
    const auto type=at<Address>(stream,16);
    const auto visible=test_world_method(type,"IsLevelVisible"),loaded=test_world_method(type,"IsLevelLoaded"),
               show=test_world_method(type,"SetShouldBeVisible");
    if(!visible||!loaded||!show||!parameter(visible,"ReturnValue",0,1)||!parameter(loaded,"ReturnValue",0,1))return false;
    std::uint8_t active{},ready{};
    event(stream,visible,&active);
    event(stream,loaded,&ready);
    if(!active&&ready&&!requested&&parameter(show,"bInShouldBeVisible",0,1)){
        std::uint8_t enabled=1;
        event(stream,show,&enabled);
        requested=true;
    }
    return active;
}
struct CastleFixtureDoor {
    Address actor{},timeline{},open{},close{},stop{},seek{};
    float relative{},target{},position{},length{};
    bool playing{};
};
static bool castle_fixture_door(unsigned index,CastleFixtureDoor& out){
    const auto actor=resolve(castle_fixture_doors[index]);
    if(!actor)return false;
    const auto type=at<Address>(actor,16);
    const auto mesh=object_field(actor,index?"AquaDoor_00_SM":"LeftDoor");
    const auto timeline=object_field(actor,index?"DoorOpen":"OpenDOors");
    if(!mesh||!timeline)return false;
    const auto mesh_type=at<Address>(mesh,16),timeline_type=at<Address>(timeline,16);
    const auto relative=field_of(mesh_type,"RelativeLocation",12),target=field_of(type,index?"doorLFinalState":"OpenWidth",index?48:4);
    const auto at_fn=test_world_method(timeline_type,"GetPlaybackPosition"),length=test_world_method(timeline_type,"GetTimelineLength"),
               playing=test_world_method(timeline_type,"IsPlaying");
    out.open=test_world_method(type,"SetToOpenState");
    out.close=test_world_method(type,"CloseDoor");
    out.stop=test_world_method(timeline_type,"Stop");
    out.seek=test_world_method(timeline_type,"SetPlaybackPosition");
    if(!relative||!target||!at_fn||!length||!playing||!out.open||!out.close||!out.stop||!out.seek||
       !parameter(at_fn,"ReturnValue",0,4)||!parameter(length,"ReturnValue",0,4)||!parameter(playing,"ReturnValue",0,1)||
       !parameter(out.seek,"NewPosition",0,4)||!parameter(out.seek,"bFireEvents",4,1)||!parameter(out.seek,"bFireUpdate",5,1))return false;
    out.actor=actor;
    out.timeline=timeline;
    out.relative=at<float>(mesh,at<int>(relative,76)+index*4);
    out.target=index?at<float>(actor,at<int>(target,76)+20):-at<float>(actor,at<int>(target,76));
    event(timeline,at_fn,&out.position);
    event(timeline,length,&out.length);
    std::uint8_t active{};
    event(timeline,playing,&active);
    out.playing=active!=0;
    return out.length>0.f;
}
static void castle_fixture_close(const CastleFixtureDoor& door,bool bridge){
    event(door.timeline,door.stop,nullptr);
    const auto position=bridge?door.length:0.f;
    alignas(4) unsigned char args[6]{};
    std::memcpy(args,&position,4);
    args[5]=1;
    event(door.timeline,door.seek,args);
    if(bridge)event(door.actor,door.close,nullptr);
}
static bool run_world_castle_gate_stage(Address engine){
    static bool streaming{},positioned{},commanded{},show_requested{};
    static unsigned stage{};
    constexpr std::array proofs{L"castle-gate-ready.txt",L"castle-gate-open.txt",L"castle-gate-closed.txt"};
    if(stage==proofs.size())return all_player_markers(proofs.back());
    const auto gi=object_field(engine,"GameInstance"),pawn=resolve(held_pawn);
    if(!gi||!pawn)return false;
    if(host&&!streaming){
        constexpr std::array lists{FixtureStreamList{"/Game/RuinsLevel/Maps/LevelLists/GreenRuins.GreenRuins",7},
                                   FixtureStreamList{"/Game/RuinsLevel/Maps/LevelLists/AutumnIsland.AutumnIsland",9}};
        streaming=fixture_stock_streams(engine,gi,lists);
        return false;
    }
    if(!resolve(fixture_stream_lookup)||std::ranges::any_of(castle_fixture_doors,[](auto key){return !resolve(key);})){
        scan([](Address object){
            const auto leaf=name(object);
            if(leaf=="GetStreamingLevel"&&exact_path(object,"/Script/Engine.GameplayStatics.GetStreamingLevel"))
                remember(fixture_stream_lookup,object);
            for(unsigned i=0;i<castle_fixture_paths.size();++i){
                const std::string_view path=castle_fixture_paths[i];
                if(leaf==path.substr(path.rfind('.')+1)&&exact_path(object,path.data()))remember(castle_fixture_doors[i],object);
            }
        });
        return false;
    }
    if(!positioned){
        held_pose.position={-51709.285f+3.f*role_index,-14280.334f,-39308.215f};
        positioned=apply_test_pose(pawn,held_pose);
        return false;
    }
    if(!fixture_visible_stream(gi,L"/Game/RuinsLevel/Maps/500_AUT",show_requested)||
       (stage&&!all_player_markers(proofs[stage-1])))return false;
    std::array<CastleFixtureDoor,2> doors{};
    for(unsigned i=0;i<doors.size();++i)if(!castle_fixture_door(i,doors[i]))return false;
    if(stage&&host&&!commanded){
        if(stage==1)for(const auto& door:doors)event(door.actor,door.open,nullptr);
        else for(unsigned i=0;i<doors.size();++i)castle_fixture_close(doors[i],!i);
        commanded=true;
        return false;
    }
    char proof[240]{};
    const auto size=std::snprintf(proof,sizeof(proof),
        "stage=%u bridge=%.3f timeline=%.4f playing=%u front=%.3f timeline=%.4f playing=%u\n",
        stage,doors[0].relative,doors[0].position,doors[0].playing,doors[1].relative,doors[1].position,doors[1].playing);
    if(size>0)write_file(marker(role_index,L"castle-gate-progress.txt").c_str(),proof,static_cast<unsigned>(size));
    for(const auto& door:doors){
        const bool opened=stage==1;
        const float wanted_position=opened&&!host?door.length:0.f;
        if(door.playing||std::abs(door.relative-(opened?door.target:0.f))>.01f||
           std::abs(door.position-wanted_position)>.001f)return false;
    }
    if(size>0)write_file(marker(role_index,proofs[stage]).c_str(),proof,static_cast<unsigned>(size));
    ++stage;
    commanded=false;
    return false;
}
