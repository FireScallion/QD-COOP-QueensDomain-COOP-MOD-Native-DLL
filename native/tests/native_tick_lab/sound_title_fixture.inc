namespace SoundFixture {
enum Symbol {at_location,attached,component_location,is_playing,dagger_class,fire_dagger,exit_cutscene,
             get_controller,set_listener,clear_listener,play_2d,spawn_2d,create_2d,component_play,
             component_stop,component_fade,component_state,count};
constexpr std::array<std::string_view,count> paths{
    "/Script/Engine.GameplayStatics.SpawnSoundAtLocation",
    "/Script/Engine.GameplayStatics.SpawnSoundAttached",
    "/Script/Engine.SceneComponent.K2_GetComponentLocation",
    "/Script/Engine.AudioComponent.IsPlaying",
    "/Game/Items/Weapons/Daggers/Default/DefaultDagger_Projectile_BP.DefaultDagger_Projectile_BP_C",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.FireDagger",
    "/Game/Blueprints/FS_GameInstance.FS_GameInstance_C.ExitCutsceneMode",
    "/Script/Engine.GameplayStatics.GetPlayerController",
    "/Script/Engine.PlayerController.SetAudioListenerOverride",
    "/Script/Engine.PlayerController.ClearAudioListenerOverride",
    "/Script/Engine.GameplayStatics.PlaySound2D","/Script/Engine.GameplayStatics.SpawnSound2D",
    "/Script/Engine.GameplayStatics.CreateSound2D","/Script/Engine.AudioComponent.Play",
    "/Script/Engine.AudioComponent.Stop","/Script/Engine.AudioComponent.FadeOut","/Script/Engine.AudioComponent.GetPlayState"
};
constexpr std::string_view sound_path="/Game/Items/Weapons/Daggers/HomingDagger/HomingDaggerThrowSound.HomingDaggerThrowSound";
constexpr std::string_view attenuation_path="/Game/Player/Assets/Audio/CharacterAudioAtten.CharacterAudioAtten";
constexpr std::string_view action_attenuation="/Game/Player/Player_Action_Atten.Player_Action_Atten";
constexpr std::string_view dagger_sound="/Game/Player/Assets/Audio/DaggerThrowSound.DaggerThrowSound";
constexpr std::string_view control_sound="/Game/Items/Weapons/CrushingLance/CrushingLanceLoop.CrushingLanceLoop";
constexpr std::array volumes{.075f,.09f,.065f},pitches{.88f,1.08f,.96f};
std::array<UE::WeakKey,count> objects{};
UE::WeakKey sound{},attenuation{},listener_controller{},prepared_voice{};
UE::WeakKey control_source_voice{};
std::array<UE::WeakKey,2> control_voices{};
std::array<bool,2> remote_controls{};
std::array<std::array<float,3>,3> expected{};
std::array<std::array<float,3>,3> actual{};
std::array<bool,3> played{},positions{},active_voices{};
std::wstring directory;
ULONGLONG start{},last_scan{},last_report{},next_play{},listener_ready{},prepared_at{};
unsigned observed_source{},attempted{},source_controls{};
bool reported{},dagger{},two_dimensional{},prepared_silent{},controlled{};
unsigned (*read_core)(char*,unsigned){};
using SoundExec=void(*)(void*,void*,void*);
struct FixtureHook {SoundExec next{};void* volatile* cell{};};
FixtureHook playback_hook,stop_hook,fade_hook;
DWORD observer_thread{};

Address get(Symbol id){return ui.get(objects[id]);}
std::wstring path(std::wstring_view name){return directory+L"/"+std::wstring(name);}
bool source(){return lan.role==L"client";}
unsigned sample_count(){return dagger?1:two_dimensional?3:2;}
std::string_view cue_path(){return dagger?dagger_sound:controlled?control_sound:sound_path;}
std::wstring sound_marker(std::wstring_view label,unsigned index){return path(std::wstring(label)+std::to_wstring(index)+L".txt");}
bool ready(){
    for(const auto role:{L"host",L"client",L"client2",L"client3"})
        if(!marker_exists(path(std::wstring(role)+L"/ready.txt").c_str()))return false;
    return true;
}
bool sound_at(Address voice,std::array<float,3>& position){
    if(!ui.method(get(component_location),12,{{"ReturnValue","StructProperty",0,12}}))return false;
    TitleUi::event(voice,get(component_location),position.data());
    return true;
}
bool voice_playing(Address voice){
    if(!voice||!ui.method(get(is_playing),1,{{"ReturnValue","BoolProperty",0,1}}))return false;
    unsigned char playing{};
    TitleUi::event(voice,get(is_playing),&playing);
    return playing!=0;
}
void scan(){
    const auto array=at<UE::ObjectArray>(image+TitleUi::object_array,0);
    if(!UE::object_array_shape(array))return;
    for(unsigned i=static_cast<unsigned>(array.num_elements);i;){
        --i;
        const auto chunk=at<Address>(array.chunks,(i/UE::objects_per_chunk)*8);
        if(!chunk)continue;
        const auto item=at<UE::ObjectItem>(chunk,(i%UE::objects_per_chunk)*sizeof(UE::ObjectItem));
        if(!item.object||(item.flags&0x30000000u))continue;
        const auto name=leaf(item.object);
        for(unsigned j=0;j<paths.size();++j)
            if(!ui.get(objects[j])&&name==paths[j].substr(paths[j].rfind('.')+1)&&path_is(item.object,paths[j]))
                ui.remember(objects[j],item.object);
    }
}
void observe_voice(void* context,void* frame,void* result){
    playback_hook.next(context,frame,result);
    if(GetCurrentThreadId()!=observer_thread||!result)return;
    const auto voice=UE::field<Address>(result,0);
    if((dagger||two_dimensional||controlled)&&frame&&!UE::field<Address>(frame,32)){
        const auto parameters=UE::field<Address>(frame,40);
        const auto cue=parameters?at<Address>(parameters,8):0;
        std::string name;
        if(cue&&UE::object_path(cue,image+TitleUi::name_pool,read_local,name)){
            const auto location=at<std::array<float,3>>(parameters,16);
            if(auto file=_wfopen(path(L"sound-host-request.txt").c_str(),L"ab")){
                std::fprintf(file,"%s v=%.3f p=%.3f xyz=%.3f %.3f %.3f returned=%u\n",name.c_str(),
                    at<float>(parameters,40),at<float>(parameters,44),location[0],location[1],location[2],voice!=0);
                std::fclose(file);
            }
        }
    }
    const auto cue=ui.object(voice,"Sound"),atten=ui.object(voice,"AttenuationSettings");
    if(!cue||!path_is(cue,cue_path()))return;
    const auto volume=ui.member(voice,"VolumeMultiplier","FloatProperty",4);
    const auto pitch=ui.member(voice,"PitchMultiplier","FloatProperty",4);
    if(!volume||!pitch)return;
    const auto v=at<float>(voice,at<unsigned>(volume,76)),p=at<float>(voice,at<unsigned>(pitch,76));
    const bool attenuation_matches=atten&&(path_is(atten,attenuation_path)||((two_dimensional||controlled)&&path_is(atten,action_attenuation)));
    const bool active=voice_playing(voice);
    for(unsigned index=0;index<sample_count();++index)if(std::abs(v-(dagger?1.f:volumes[index]))<.001f&&
       std::abs(p-(dagger?1.f:pitches[index]))<.001f){
        const bool located=sound_at(voice,actual[index]);
        active_voices[index]=located&&active&&attenuation_matches;
        if(controlled&&active_voices[index]&&index<control_voices.size())ui.remember(control_voices[index],voice);
        if(auto file=_wfopen(path(L"sound-host-voice.txt").c_str(),L"ab")){
            const auto& point=actual[index];
            std::fprintf(file,"voice=%u v=%.4f p=%.4f position=%.3f %.3f %.3f located=%u playing=%u attenuation=%u\n",
                index,v,p,point[0],point[1],point[2],located,active,attenuation_matches);
            std::fclose(file);
        }
    }
}
bool watch_sound(Address function,SoundExec observer,FixtureHook& hook){
    if(hook.cell)return true;
    if(!function)return false;
    auto cell=reinterpret_cast<void* volatile*>(function+216);
    const auto next=*cell;
    MEMORY_BASIC_INFORMATION memory{};
    if(!next||!VirtualQuery(next,&memory,sizeof(memory))||
       memory.AllocationBase!=GetModuleHandleW(L"QDCoopNativeM1.dll")||!(memory.Protect&0xf0u))return false;
    hook.next=reinterpret_cast<SoundExec>(next);
    observer_thread=GetCurrentThreadId();
    if(InterlockedCompareExchangePointer(cell,reinterpret_cast<void*>(observer),next)!=next)return false;
    hook.cell=cell;
    return true;
}
unsigned voice_state(Address voice){
    if(!voice||!ui.method(get(component_state),1,{{"ReturnValue","EnumProperty",0,1}}))return 255;
    unsigned char state{};
    TitleUi::event(voice,get(component_state),&state);
    return state;
}
void observe_remote_stop(void* context,void* frame,void* result){
    const auto voice=GetCurrentThreadId()==observer_thread?ui.get(control_voices[0]):0;
    const bool watched=voice&&reinterpret_cast<Address>(context)==voice&&voice_playing(voice);
    stop_hook.next(context,frame,result);
    if(watched&&!voice_playing(ui.get(control_voices[0]))){
        remote_controls[0]=true;
        if(auto file=_wfopen(path(L"sound-host-stop.txt").c_str(),L"ab")){
            std::fputs("was_playing=1 stopped=1\n",file);
            std::fclose(file);
        }
        marker(sound_marker(L"sound-host-control-",0).c_str());
    }
}
void observe_remote_fade(void* context,void* frame,void* result){
    const bool watched=GetCurrentThreadId()==observer_thread&&reinterpret_cast<Address>(context)==ui.get(control_voices[1]);
    fade_hook.next(context,frame,result);
    if(!watched||!frame)return;
    const auto params=UE::field<Address>(frame,40);
    if(!params)return;
    const auto duration=at<float>(params,0),target=at<float>(params,4);
    const auto curve=at<unsigned char>(params,8);
    const auto state=voice_state(ui.get(control_voices[1]));
    const bool match=std::abs(duration-.7f)<.0001f&&target==0.f&&curve==2&&state==4;
    if(auto file=_wfopen(path(L"sound-host-fade.txt").c_str(),L"ab")){
        std::fprintf(file,"duration=%.4f target=%.4f curve=%u state=%u matched=%u\n",duration,target,curve,state,match);
        std::fclose(file);
    }
    if(match){
        remote_controls[1]=true;
        marker(sound_marker(L"sound-host-control-",1).c_str());
    }
}
bool observe_playback(){
    if(!get(component_location)||!get(is_playing)||!watch_sound(get(at_location),&observe_voice,playback_hook))return false;
    return !controlled||(get(component_state)&&watch_sound(get(component_stop),&observe_remote_stop,stop_hook)&&
       watch_sound(get(component_fade),&observe_remote_fade,fade_hook));
}
bool place_listener(Address instance,ULONGLONG time){
    if(!dagger&&!two_dimensional&&!controlled)return true;
    if(listener_ready)return time>=listener_ready;
    const auto controller_fn=get(get_controller),set=get(set_listener),clear=get(clear_listener);
    if(!ui.method(controller_fn,24,{{"WorldContextObject","ObjectProperty",0,8},{"PlayerIndex","IntProperty",8,4},
        {"ReturnValue","ObjectProperty",16,8}})||!ui.method(set,32,{{"AttachToComponent","ObjectProperty",0,8},
        {"Location","StructProperty",8,12},{"Rotation","StructProperty",20,12}})||!ui.method(clear,0,{}))return false;
    const auto gameplay=ui.get(TitleUi::gameplay);
    Address pawn[]{instance,0,0},controller[]{instance,0,0};
    TitleUi::event(gameplay,ui.get(TitleUi::local_pawn),pawn);
    TitleUi::event(gameplay,controller_fn,controller);
    const auto parent=ui.object(pawn[2],"RootComponent");
    if(!parent||!controller[2])return false;
    struct Listener {Address root;std::array<float,3> location,rotation;};
    Listener args{parent,{0,0,60},{}};
    TitleUi::event(controller[2],set,&args);
    ui.remember(listener_controller,controller[2]);
    listener_ready=time+450;
    return false;
}
bool restore_sound(FixtureHook& hook,SoundExec observer){
    if(!hook.cell)return true;
    const auto watch=reinterpret_cast<void*>(observer),next=reinterpret_cast<void*>(hook.next);
    const auto found=InterlockedCompareExchangePointer(hook.cell,next,watch);
    if(found!=watch&&found!=next)return false;
    hook.cell=nullptr;
    return true;
}
bool cleanup(){
    if(const auto controller=ui.get(listener_controller)){
        TitleUi::event(controller,get(clear_listener),nullptr);
        listener_controller={};
    }
    const bool playback=restore_sound(playback_hook,&observe_voice);
    const bool stop=restore_sound(stop_hook,&observe_remote_stop);
    return restore_sound(fade_hook,&observe_remote_fade)&&playback&&stop;
}
bool core(std::string& status){
    if(!read_core){
        const auto module=GetModuleHandleW(L"QDCoopNativeM1.dll");
        if(!module||!load_export(module,"QDEnemyNativeStatus",read_core))return false;
    }
    const auto size=read_core(nullptr,0);
    if(!size||size>16384)return false;
    status.resize(size);
    if(read_core(status.data(),size)!=size)return false;
    if(auto file=_wfopen(path(L"sound-core-"+lan.role+L".json").c_str(),L"wb")){
        std::fputs(status.c_str(),file);
        std::fclose(file);
    }
    return true;
}
unsigned number(std::string_view status,std::string_view name){
    const auto key="\""+std::string(name)+"\":";
    const auto at=status.find(key);
    if(at==status.npos)return UINT_MAX;
    unsigned value{};
    const auto start=at+key.size();
    for(auto i=start;i<status.size()&&status[i]>='0'&&status[i]<='9';++i)value=value*10+static_cast<unsigned>(status[i]-'0');
    return value;
}
bool fire_2d(Address pawn,const std::array<float,3>& position,unsigned index,ULONGLONG time){
    const auto cue=ui.get(sound);
    if(!cue)return false;
    float start_time=.035f;
    if(index==2&&ui.get(prepared_voice)){
        const auto voice=ui.get(prepared_voice);
        std::string status;
        if(time<prepared_at||!core(status)||number(status,"sound_observed")!=2||voice_playing(voice)||
           !ui.method(get(component_play),4,{{"StartTime","FloatProperty",0,4}}))return false;
        prepared_silent=true;
        expected[index]=position;
        ++attempted;
        TitleUi::event(voice,get(component_play),&start_time);
        return voice_playing(voice);
    }
    alignas(16) std::array<unsigned char,56> args{};
    std::memcpy(args.data(),&pawn,8);
    std::memcpy(args.data()+8,&cue,8);
    std::memcpy(args.data()+16,&volumes[index],4);
    std::memcpy(args.data()+20,&pitches[index],4);
    std::memcpy(args.data()+24,&start_time,4);
    const auto gameplay=ui.get(TitleUi::gameplay);
    if(index==0&&!controlled){
        const auto function=get(play_2d);
        if(!ui.method(function,49,{{"WorldContextObject","ObjectProperty",0,8},{"Sound","ObjectProperty",8,8},
           {"VolumeMultiplier","FloatProperty",16,4},{"PitchMultiplier","FloatProperty",20,4},{"StartTime","FloatProperty",24,4},
           {"ConcurrencySettings","ObjectProperty",32,8},{"OwningActor","ObjectProperty",40,8},{"bIsUISound","BoolProperty",48,1}}))
            return false;
        std::memcpy(args.data()+40,&pawn,8);
        args[48]=1;
        ++attempted;
        expected[index]=position;
        TitleUi::event(gameplay,function,args.data());
        return true;
    }
    const auto function=get(index==1||controlled?spawn_2d:create_2d);
    if(!ui.method(function,56,{{"WorldContextObject","ObjectProperty",0,8},{"Sound","ObjectProperty",8,8},
       {"VolumeMultiplier","FloatProperty",16,4},{"PitchMultiplier","FloatProperty",20,4},{"StartTime","FloatProperty",24,4},
       {"ConcurrencySettings","ObjectProperty",32,8},{"bPersistAcrossLevelTransition","BoolProperty",40,1},
       {"bAutoDestroy","BoolProperty",41,1},{"ReturnValue","ObjectProperty",48,8}}))return false;
    args[41]=index==1||controlled;
    TitleUi::event(gameplay,function,args.data());
    const auto voice=UE::field<Address>(args.data(),48);
    if(!voice)return false;
    if(index==2){
        if(!voice_playing(voice)){
            ui.remember(prepared_voice,voice);
            prepared_at=time+450;
        }
        return false;
    }
    if(!voice_playing(voice))return false;
    if(controlled)ui.remember(control_source_voice,voice);
    ++attempted;
    expected[index]=position;
    return true;
}
bool fire(Address instance,unsigned index,ULONGLONG time){
    const auto gameplay=ui.get(TitleUi::gameplay);
    Address pawn[]{instance,0,0};
    TitleUi::event(gameplay,ui.get(TitleUi::local_pawn),pawn);
    if(!pawn[2])return false;
    const auto parent=ui.object(pawn[2],"RootComponent");
    std::array<float,3> position{};
    if(!parent||!sound_at(parent,position))return false;
    if(two_dimensional||controlled)return fire_2d(pawn[2],position,index,time);
    if(dagger){
        if(const auto cutscene=ui.member(instance,"InCutscene","BoolProperty",1);
           cutscene&&UE::get_bool(reinterpret_cast<void*>(cutscene),reinterpret_cast<void*>(instance),0)){
            if(ui.method(get(exit_cutscene),0,{}))TitleUi::event(instance,get(exit_cutscene),nullptr);
            return false;
        }
        const auto fn=get(fire_dagger);
        if(!get(dagger_class)||!ui.method(fn,9,{{"Class","ClassProperty",0,8},{"Success","BoolProperty",8,1}}))return false;
        struct Args {Address type;bool success;} args{get(dagger_class),false};
        expected[0]=position;
        ++attempted;
        TitleUi::event(pawn[2],fn,&args);
        return args.success;
    }
    if(!ui.get(sound)||!ui.get(attenuation))return false;
    alignas(16) std::array<unsigned char,96> args{};
    const auto put=[&](unsigned offset,const auto& value){std::memcpy(args.data()+offset,&value,sizeof(value));};
    const auto cue=ui.get(sound),atten=ui.get(attenuation);
    const float start_time=.035f;
    Address output{};
    if(index==0){
        const auto spawn=get(at_location);
        if(!ui.method(spawn,88,{{"WorldContextObject","ObjectProperty",0,8},{"Sound","ObjectProperty",8,8},
            {"Location","StructProperty",16,12},{"Rotation","StructProperty",28,12},{"VolumeMultiplier","FloatProperty",40,4},
            {"PitchMultiplier","FloatProperty",44,4},{"StartTime","FloatProperty",48,4},{"AttenuationSettings","ObjectProperty",56,8},
            {"ConcurrencySettings","ObjectProperty",64,8},{"bAutoDestroy","BoolProperty",72,1},{"ReturnValue","ObjectProperty",80,8}}))
            return false;
        position[0]+=40.f;
        put(0,pawn[2]);put(8,cue);put(16,position);put(40,volumes[index]);put(44,pitches[index]);put(48,start_time);put(56,atten);
        args[72]=1;
        ++attempted;
        TitleUi::event(gameplay,spawn,args.data());
        output=UE::field<Address>(args.data(),80);
    }else{
        const auto spawn=get(attached);
        if(!ui.method(spawn,96,{{"Sound","ObjectProperty",0,8},{"AttachToComponent","ObjectProperty",8,8},
            {"AttachPointName","NameProperty",16,8},{"Location","StructProperty",24,12},{"Rotation","StructProperty",36,12},
            {"LocationType","ByteProperty",48,1},{"bStopWhenAttachedToDestroyed","BoolProperty",49,1},
            {"VolumeMultiplier","FloatProperty",52,4},{"PitchMultiplier","FloatProperty",56,4},{"StartTime","FloatProperty",60,4},
            {"AttenuationSettings","ObjectProperty",64,8},{"ConcurrencySettings","ObjectProperty",72,8},
            {"bAutoDestroy","BoolProperty",80,1},{"ReturnValue","ObjectProperty",88,8}}))return false;
        const std::array offset{30.f,10.f,40.f};
        put(0,cue);put(8,parent);put(24,offset);put(52,volumes[index]);put(56,pitches[index]);put(60,start_time);put(64,atten);
        args[80]=1;
        ++attempted;
        TitleUi::event(gameplay,spawn,args.data());
        output=UE::field<Address>(args.data(),88);
    }
    return output&&voice_playing(output)&&sound_at(output,expected[index]);
}
bool control_voice(unsigned index){
    const auto voice=ui.get(control_source_voice);
    if(!voice_playing(voice))return false;
    if(index==0){
        if(!ui.method(get(component_stop),0,{}))return false;
        TitleUi::event(voice,get(component_stop),nullptr);
        return !voice_playing(ui.get(control_source_voice));
    }
    if(!ui.method(get(component_fade),9,{{"FadeOutDuration","FloatProperty",0,4},
       {"FadeVolumeLevel","FloatProperty",4,4},{"FadeCurve","EnumProperty",8,1}}))return false;
    struct Fade {float duration,target;unsigned char curve;} args{.7f,0.f,2};
    TitleUi::event(voice,get(component_fade),&args);
    return voice_state(ui.get(control_source_voice))==4;
}
void origin_file(unsigned index){
    if(auto file=_wfopen(path(L"sound-origin-"+std::to_wstring(index)+L".bin").c_str(),L"wb")){
        positions[index]=std::fwrite(expected[index].data(),sizeof(expected[index]),1,file)==1;
        std::fclose(file);
    }
}
void read_origins(){
    for(unsigned index=0;index<sample_count();++index)if(!positions[index])
        if(auto file=_wfopen(path(L"sound-origin-"+std::to_wstring(index)+L".bin").c_str(),L"rb")){
            positions[index]=std::fread(expected[index].data(),sizeof(expected[index]),1,file)==1;
            std::fclose(file);
        }
}
void report(){
    if(auto file=_wfopen(path(L"sound-"+lan.role+L".json").c_str(),L"wb")){
        std::fprintf(file,"{\"passed\":1,\"source\":%u,\"host\":%u,\"stock_played\":%u,\"at_location\":%u,\"attached\":%u,\"dagger\":%u,"
            "\"two_dimensional\":%u,\"prepared_silent\":%u,\"prepared_played\":%u,\"controlled\":%u,"
            "\"source_controls\":%u,\"remote_stop\":%u,\"remote_fade\":%u}\n",
            source(),lan.host,observed_source,played[0],played[1],dagger,two_dimensional,prepared_silent,played[2],
            controlled,source_controls,remote_controls[0],remote_controls[1]);
        std::fclose(file);
    }
    reported=true;
}
bool step(Address instance,ULONGLONG time){
    if(directory.empty()){
        wchar_t root[32768]{};
        if(!environment_title(L"QD_PROJECTILE_TITLE_ROOT",root,32768))return false;
        directory=root;
        wchar_t variant[12]{};
        if(environment_title(L"QD_SOUND_TITLE_VARIANT",variant,12)){
            dagger=std::wcscmp(variant,L"dagger")==0;
            two_dimensional=std::wcscmp(variant,L"2d")==0;
            controlled=std::wcscmp(variant,L"control")==0;
        }
    }
    if(reported)return !lan.host||marker_exists(path(L"sound-client.json").c_str());
    if(!start)start=time;
    if(time-last_scan>=150&&(source()||lan.host)&&(!get(at_location)||!get(attached)||!get(component_location)||!get(is_playing)||
       (dagger&&source()&&(!get(dagger_class)||!get(fire_dagger)||!get(exit_cutscene)))||
       (two_dimensional&&source()&&(!get(play_2d)||!get(spawn_2d)||!get(create_2d)||!get(component_play)))||
       (controlled&&(!get(spawn_2d)||!get(component_stop)||!get(component_fade)||!get(component_state)))||
       ((dagger||two_dimensional||controlled)&&lan.host&&(!get(get_controller)||!get(set_listener)||!get(clear_listener))))){
        last_scan=time;
        scan();
    }
    if(lan.host){
        if(observe_playback()&&place_listener(instance,time)&&!marker_exists(path(L"sound-host-observing.txt").c_str()))
            marker(path(L"sound-host-observing.txt").c_str());
        read_origins();
        for(unsigned i=0;i<sample_count();++i)if(positions[i]&&active_voices[i]){
            played[i]=true;
            for(unsigned axis=0;axis<3;++axis)if(std::abs(actual[i][axis]-expected[i][axis])>2.f)played[i]=false;
            if(dagger){
                float distance_squared{};
                for(unsigned axis=0;axis<3;++axis){
                    const auto difference=actual[i][axis]-expected[i][axis];
                    distance_squared+=difference*difference;
                }
                played[i]=distance_squared<300.f*300.f;
            }
            if(controlled&&played[i]&&!marker_exists(sound_marker(L"sound-host-playing-",i).c_str()))
                marker(sound_marker(L"sound-host-playing-",i).c_str());
        }
    }
    std::string status;
    if(time-last_report>=500){last_report=time;core(status);}
    if(source()){
        if(controlled&&source_controls<observed_source&&marker_exists(sound_marker(L"sound-host-playing-",source_controls).c_str())&&
           control_voice(source_controls))++source_controls;
        const bool next_ready=!controlled||(source_controls==observed_source&&
            (!observed_source||marker_exists(sound_marker(L"sound-host-control-",observed_source-1).c_str())));
        if(next_ready&&observed_source<sample_count()&&time>=next_play&&time-start>=1800&&ready()&&
           marker_exists(path(L"sound-host-observing.txt").c_str())){
            next_play=time+300;
            if(!dagger){
                if(!ui.get(sound))if(const auto asset=ui.load_asset_path(cue_path()))ui.remember(sound,asset);
                if(!two_dimensional&&!controlled&&!ui.get(attenuation))
                    if(const auto asset=ui.load_asset_path(attenuation_path))ui.remember(attenuation,asset);
            }
            if(fire(instance,observed_source,time)){
                origin_file(observed_source++);
            }
            if(auto file=_wfopen(path(L"sound-source-attempts.txt").c_str(),L"wb")){
                std::fprintf(file,"attempted=%u actual_playing=%u\n",attempted,observed_source);
                std::fclose(file);
            }
        }
        if(std::all_of(positions.begin(),positions.begin()+sample_count(),[](bool seen){return seen;})&&
           marker_exists(path(L"sound-host.json").c_str())&&marker_exists(path(L"sound-client2.json").c_str())&&
           marker_exists(path(L"sound-client3.json").c_str())&&core(status)&&
           (!controlled||(source_controls==2&&number(status,"sound_control_observed")==2&&
             number(status,"sound_control_received")==0))&&
           (dagger?number(status,"sound_direct_blueprint")>=1:
            number(status,"sound_observed")==sample_count()&&number(status,"sound_sent")>=2*sample_count()&&
            number(status,"sound_received")==0))report();
    }else if((!lan.host||std::all_of(played.begin(),played.begin()+sample_count(),[](bool heard){return heard;}))&&core(status)&&
             (!controlled||(number(status,"sound_control_received")==2&&
              (!lan.host||(remote_controls[0]&&remote_controls[1]&&!voice_playing(ui.get(control_voices[1]))&&
                           number(status,"sound_control_applied")==2))))&&
             (dagger?number(status,"sound_received")>=1:number(status,"sound_received")==sample_count())&&
             (!lan.host||(dagger?number(status,"sound_played")>=1:number(status,"sound_played")==sample_count()))&&cleanup())report();
    return reported&&(!lan.host||marker_exists(path(L"sound-client.json").c_str()));
}
}
