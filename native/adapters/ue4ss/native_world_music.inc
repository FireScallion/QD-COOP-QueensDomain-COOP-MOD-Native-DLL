struct WorldMusic {
    Weak instance,controller,previous,owned;
    int controller_at{},playing_at{},override_at{};
    double next_bind{},next_apply{};
};
static WorldMusic world_music;

static bool world_music_cleanup(){
    auto& state=world_music;
    const auto controller=api.weakget(&state.controller),owned=api.weakget(&state.owned);
    if(controller&&owned&&AnimU::field<void*>(controller,state.override_at)==owned){
        const auto previous=api.weakget(&state.previous);
        const auto fn=world_input(previous?world_music_override:world_music_clear);
        if(!fn)return false;
        void* cue=previous;
        api.event(controller,fn,previous?&cue:nullptr);
        if(AnimU::field<void*>(controller,state.override_at)!=previous)return false;
        ++world_music_restored;
    }
    state.owned=state.previous={};
    return true;
}
static void world_music_reset(){world_music_cleanup();world_music={};}

static bool bind_world_music(void* instance,double time){
    auto& state=world_music;
    if(!instance)return false;
    if(api.weakget(&state.instance)==instance){
        const auto controller=api.weakget(&state.controller);
        if(controller&&AnimU::field<void*>(instance,state.controller_at)==controller)return true;
    }
    if(time<state.next_bind)return false;
    state.next_bind=time+.5;
    for(unsigned i=world_music_type;i<=world_music_clear;++i)if(!world_input(i)){
        scan_world_inputs(4096);
        break;
    }
    const auto type=world_input(world_music_type),cue=world_input(world_music_cue);
    const auto override=world_input(world_music_override),clear=world_input(world_music_clear);
    if(!type||!cue||!override||!clear||AnimU::field<int>(override,104)!=36||AnimU::field<int>(clear,104)!=18||
       !medic_params(world_music_override,8,{{"Cue",8,0}})||!medic_params(world_music_clear,0,{}))return false;
    const auto parameter=catalog_field(override,"Cue",8);
    const auto source=catalog_field(AnimU::field<void*>(instance,16),"MusicController",8);
    const auto playing=catalog_field(type,"CurrentPlayingTrack",8),current=catalog_field(type,"CurrentTrackOverride",8);
    for(const auto field:{parameter,source,playing,current})if(!medic_field_class(field,"ObjectProperty"))return false;
    if(AnimU::field<void*>(parameter,0x78)!=cue||!world_music_cleanup())return false;
    WorldMusic binding;
    binding.next_apply=state.next_apply;
    binding.controller_at=*api.offset(source);
    binding.playing_at=*api.offset(playing);
    binding.override_at=*api.offset(current);
    const auto controller=AnimU::field<void*>(instance,binding.controller_at);
    if(!controller||!api.isa(controller,type)||!makeweak(binding.controller,controller)||!makeweak(binding.instance,instance))
        return false;
    state=binding;
    return true;
}

static void world_music_capture(void* save,double time,std::string& out){
    if(world_stream.scripted_world)return;
    const auto instance=api.weakget(&medic_runtime.save.gi);
    void* cue{};
    if(bind_world_music(instance,time))
        cue=AnimU::field<void*>(api.weakget(&world_music.controller),world_music.playing_at);
    if(!cue)cue=catalog_object_field(save,"PlayingMusicTrack");
    const auto type=world_input(world_music_cue);
    std::string path;
    if(cue&&type&&api.isa(cue,type)&&catalog_path(cue,path)&&WP::music_path(path))out=std::move(path);
}

static void world_music_step(double time){
    if(host_role||world_stream.scripted_world||!active_peer_mask||!world_progress_ready())return;
    auto& state=world_music;
    const auto& desired=world_progress.authoritative.music;
    if(desired.empty()||time<state.next_apply)return;
    state.next_apply=time+.5;
    if(!bind_world_music(api.weakget(&medic_runtime.save.gi),time))return;
    const auto controller=api.weakget(&state.controller),type=world_input(world_music_cue);
    const auto override=world_input(world_music_override);
    if(!controller||!type||!override)return;
    const auto current=AnimU::field<void*>(controller,state.override_at);
    std::string path;
    if(current&&catalog_path(current,path)&&path==desired)return;
    auto cue=load_world_asset(desired,UINT32_MAX,false,nullptr);
    Weak owned{},previous=state.previous;
    if(!cue||!api.isa(cue,type)||!makeweak(owned,cue))return;
    if(current!=api.weakget(&state.owned)){
        previous={};
        if(current&&(!api.isa(current,type)||!makeweak(previous,current)))return;
    }
    api.event(controller,override,&cue);
    if(AnimU::field<void*>(controller,state.override_at)!=cue)return;
    state.owned=owned;
    state.previous=previous;
    ++world_music_changes;
}
