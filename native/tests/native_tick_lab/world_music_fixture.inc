static constexpr std::array music_test_paths{
    "/Game/Music/0-System/0-2_Rest_MusicCue.0-2_Rest_MusicCue",
    "/Game/Music/3-Boss/3-1_ShrineGaurdian-FallenLord_MusicCue.3-1_ShrineGaurdian-FallenLord_MusicCue",
    "/Game/Music/1-Stage/1-1_GreenRuin-GreenRuins_MusicCue.1-1_GreenRuin-GreenRuins_MusicCue"
};
static U::WeakKey music_test_controller;
static std::array<U::WeakKey,music_test_paths.size()> music_test_cues{};
static int music_test_override=-1,music_test_playing=-1,music_test_started=-1;

static bool run_world_music_stage(Address engine,ULONGLONG time){
    static bool ready{},boss_requested{},area_requested{},boss_stable{};
    static ULONGLONG boss_since{};
    static float boss_start{};
    const auto gi=object_field(engine,"GameInstance");
    const auto controller=gi?object_field(gi,"MusicController"):0;
    if(!controller)return false;
    const auto type=at<Address>(controller,16),override=test_world_method(type,"OverrideTrack");
    if(!override||at<std::uint16_t>(override,182)!=8||!parameter(override,"Cue",0,8))return false;
    const auto playing=field_of(type,"CurrentPlayingTrack",8),current=field_of(type,"CurrentTrackOverride",8);
    const auto started_at=field_of(type,"LastSongStartTime",4);
    if(!playing||!current||!started_at)return false;
    if(!ready){
        for(unsigned i=0;i<music_test_cues.size();++i)if(!resolve(music_test_cues[i])){
            const auto cue=load_test_asset(music_test_paths[i]);
            if(!cue)return false;
            remember(music_test_cues[i],cue);
        }
        remember(music_test_controller,controller);
        music_test_override=at<int>(current,76);
        music_test_playing=at<int>(playing,76);
        music_test_started=at<int>(started_at,76);
        if(!host){
            auto rest=resolve(music_test_cues[0]);
            if(!rest)return false;
            event(controller,override,&rest);
            if(at<Address>(controller,music_test_override)!=rest)return false;
        }
        ready=true;
        write_file(marker(role_index,L"music-ready.txt").c_str(),"ok",2);
    }
    if(!all_player_markers(L"music-ready.txt"))return false;
    auto boss=resolve(music_test_cues[1]),area=resolve(music_test_cues[2]);
    if(!boss||!area||resolve(music_test_controller)!=controller)return false;
    if(host&&!boss_requested){
        event(controller,override,&boss);
        if(at<Address>(controller,music_test_override)!=boss)return false;
        boss_requested=true;
    }
    if(!boss_stable){
        if(at<Address>(controller,music_test_override)!=boss||at<Address>(controller,music_test_playing)!=boss)
            return false;
        const auto began=at<float>(controller,music_test_started);
        if(!std::isfinite(began)||began<=0.f)return false;
        if(!boss_since){boss_since=time;boss_start=began;}
        if(began!=boss_start){phase=85;return false;}
        if(time-boss_since<1500)return false;
        boss_stable=true;
        constexpr char proof[]="stock boss cue playing, start time held across repeated frames";
        write_file(marker(role_index,L"music-boss.txt").c_str(),proof,sizeof(proof)-1);
    }
    if(!all_player_markers(L"music-boss.txt"))return false;
    if(host&&!area_requested){
        event(controller,override,&area);
        if(at<Address>(controller,music_test_override)!=area)return false;
        area_requested=true;
    }
    const auto current_override=at<Address>(controller,music_test_override);
    const auto current_playing=at<Address>(controller,music_test_playing);
    static Address previous_override{},previous_playing{};
    if(current_override!=previous_override||current_playing!=previous_playing){
        previous_override=current_override;
        previous_playing=current_playing;
        char report[150]{};
        std::snprintf(report,sizeof(report),"area_override=%u area_playing=%u start=%.4f boss_start=%.4f",
                      static_cast<unsigned>(current_override==area),static_cast<unsigned>(current_playing==area),
                      at<float>(controller,music_test_started),boss_start);
        write_file(marker(role_index,L"music-area-state.txt").c_str(),report,static_cast<unsigned>(std::strlen(report)));
    }
    if(current_override!=area||current_playing!=area)return false;
    constexpr char proof[]="stock GreenRuins area cue replaced the boss cue";
    write_file(marker(role_index,L"music-area.txt").c_str(),proof,sizeof(proof)-1);
    return all_player_markers(L"music-area.txt");
}

static bool verify_world_music_cleanup(){
    const auto controller=resolve(music_test_controller);
    const auto cue=resolve(music_test_cues[host?2:0]);
    if(!controller||!cue||music_test_override<0||at<Address>(controller,music_test_override)!=cue)return false;
    constexpr char proof[]="stock override retained or restored on room exit";
    write_file(marker(role_index,L"music-clean.txt").c_str(),proof,sizeof(proof)-1);
    return true;
}
