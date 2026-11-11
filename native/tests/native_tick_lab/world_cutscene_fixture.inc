static U::WeakKey fixture_cine_manager;
static constexpr std::string_view fixture_cine_path=
    "/Game/RuinsLevel/Maps/998_QueenCutscenes.998_QueenCutscenes.PersistentLevel.Ending1CutsceneManager_2";

static Address fixture_cine_stream(Address pawn){
    const auto get=resolve(fixture_stream_lookup),gs=resolve(gameplay);
    if(!get||!gs||!parameter(get,"WorldContextObject",0,8)||!parameter(get,"PackageName",8,8)||
       !parameter(get,"ReturnValue",16,8))return 0;
    const auto name=test_world_name(L"/Game/RuinsLevel/Maps/998_QueenCutscenes");
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&pawn,8);
    std::memcpy(args.data()+8,&name,8);
    event(gs,get,args.data());
    return U::field<Address>(args.data(),16);
}

static bool fixture_cine_load(Address pawn){
    const auto stream=fixture_cine_stream(pawn);
    if(!stream)return false;
    const auto type=at<Address>(stream,16),load=test_world_method(type,"SetShouldBeLoaded"),
               show=test_world_method(type,"SetShouldBeVisible"),visible=test_world_method(type,"IsLevelVisible");
    if(!load||!show||!visible||!parameter(load,"bInShouldBeLoaded",0,1)||!parameter(show,"bInShouldBeVisible",0,1)||
       !parameter(visible,"ReturnValue",0,1))return false;
    std::uint8_t on=1,active{};
    event(stream,visible,&active);
    if(!active){event(stream,load,&on);event(stream,show,&on);}
    return active!=0;
}

static bool run_world_cutscene_stage(){
    static bool commanded{},saw_play{},skipped{},baseline_recorded{},sampled{};
    static bool load_before{},show_before{},damage_before{};
    static ULONGLONG guest_skip_at{};
    constexpr auto playing_proof=L"cine-playing.txt",done_proof=L"cine-restored.txt";
    const auto pawn=resolve(held_pawn);
    if(!pawn)return false;
    if(!resolve(fixture_stream_lookup)||(!resolve(fixture_cine_manager)&&!saw_play))scan([](Address object){
        const auto leaf=name(object);
        if(leaf=="GetStreamingLevel"&&exact_path(object,"/Script/Engine.GameplayStatics.GetStreamingLevel"))remember(fixture_stream_lookup,object);
        if(leaf=="Ending1CutsceneManager_2"&&exact_path(object,fixture_cine_path.data()))remember(fixture_cine_manager,object);
    });
    const auto stream=fixture_cine_stream(pawn);
    const auto load=stream?library_fixture_bool(stream,"bShouldBeLoaded"):std::nullopt;
    const auto show=stream?library_fixture_bool(stream,"bShouldBeVisible"):std::nullopt;
    const auto damage=library_fixture_bool(pawn,"bCanBeDamaged");
    if(!load||!show||!damage)return false;
    if(!baseline_recorded){
        load_before=*load;
        show_before=*show;
        damage_before=*damage;
        baseline_recorded=true;
        write_file(marker(role_index,L"cine-ready.txt").c_str(),"ready",5);
    }
    const auto active=object_field(pawn,"CurrentCutsceneManager"),manager=resolve(fixture_cine_manager);
    if(host&&!commanded&&all_player_markers(L"cine-ready.txt")){
        if(active||!fixture_cine_load(pawn)||!manager)return false;
        const auto begin=test_world_method(at<Address>(manager,16),"BeginCutscene");
        if(!begin||!parameter(begin,"Ended",0,16))return false;
        struct Delegate {U::WeakKey object;U::NameKey function;};
        static_assert(sizeof(Delegate)==16);
        Delegate callback{held_pawn,test_world_name(L"UnhideHUD")};
        if(!resolve(callback.object)||!test_world_method(at<Address>(pawn,16),"UnhideHUD"))return false;
        event(manager,begin,&callback);
        commanded=true;
        return false;
    }
    std::uint8_t playing{};
    double seconds{};
    if(active&&active==manager){
        const auto get=test_world_method(at<Address>(manager,16),"GetSequencePlayer");
        if(!get||!parameter(get,"SequencePlayer",0,8))return false;
        Address sequence{};
        event(manager,get,&sequence);
        const auto play=sequence?test_world_method(at<Address>(sequence,16),"IsPlaying"):0;
        const auto clock=sequence?test_world_method(at<Address>(sequence,16),"GetCurrentTime"):0;
        if(!play||!clock||!parameter(play,"ReturnValue",0,1)||!parameter(clock,"ReturnValue",0,16))return false;
        struct QualifiedTime {int frame;float fraction;int numerator,denominator;} value{};
        event(sequence,play,&playing);
        event(sequence,clock,&value);
        if(value.numerator>0&&value.denominator>0)seconds=(value.frame+value.fraction)*value.denominator/value.numerator;
    }
    char report[220]{};
    const auto now=GetTickCount64();
    const auto length=std::snprintf(report,sizeof(report),"stock_manager=%u playing=%u seconds=%.3f tick_ms=%llu damage=%u load=%u show=%u baseline=%u,%u,%u\n",
        active&&active==manager,playing,seconds,now,*damage,*load,*show,damage_before,load_before,show_before);
    if(length>0)write_file(marker(role_index,L"cine-progress.txt").c_str(),report,static_cast<unsigned>(length));
    if(!saw_play&&active&&active==manager&&playing&&seconds>=.2){
        if(!host&&!guest_skip_at){
            const auto skip=test_world_method(at<Address>(manager,16),"SkipCutscene");
            if(!skip||at<std::uint16_t>(skip,182))return false;
            guest_skip_at=now;
            event(manager,skip,nullptr);
            return false;
        }
        if(!host&&now-guest_skip_at<300)return false;
        saw_play=true;
        write_file(marker(role_index,playing_proof).c_str(),report,static_cast<unsigned>(length));
        if(!host)write_file(marker(role_index,L"cine-local-skip.txt").c_str(),report,static_cast<unsigned>(length));
    }
    if(!sampled&&saw_play&&active==manager&&playing&&all_player_markers(playing_proof)){
        sampled=true;
        write_file(marker(role_index,L"cine-sample.txt").c_str(),report,static_cast<unsigned>(length));
    }
    if(host&&saw_play&&!skipped&&active==manager&&all_player_markers(L"cine-sample.txt")){
        const auto skip=test_world_method(at<Address>(manager,16),"SkipCutscene");
        if(!skip||at<std::uint16_t>(skip,182))return false;
        event(manager,skip,nullptr);
        skipped=true;
        return false;
    }
    if(saw_play&&!active&&*damage==damage_before&&(host||(*load==load_before&&*show==show_before))){
        const auto status=read_native_status(core.status);
        if(status.find("\"cine_commits\":1")!=std::string::npos&&
           status.find(host?"\"cine_callbacks\":1":"\"cine_callbacks\":0")!=std::string::npos)
            write_file(marker(role_index,done_proof).c_str(),report,static_cast<unsigned>(length));
    }
    return all_player_markers(done_proof);
}
