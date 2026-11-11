namespace SoundLoopFixture {
enum Symbol {spawn,playing,position,move,volume,pitch,fade_out,fade_in,play_state,begin_actor,finish_actor,disable_move,
             spawn_2d,spawn_at_location,stop_voice,fx_library,fx_location,fx_attached,fx_visible,fx_tick,fx_active,fx_destroy,count};
constexpr std::array<std::string_view,count> paths{
    "/Script/Engine.GameplayStatics.SpawnSoundAttached","/Script/Engine.AudioComponent.IsPlaying",
    "/Script/Engine.SceneComponent.K2_GetComponentLocation","/Script/Engine.SceneComponent.K2_SetWorldLocation",
    "/Script/Engine.AudioComponent.SetVolumeMultiplier","/Script/Engine.AudioComponent.SetPitchMultiplier",
    "/Script/Engine.AudioComponent.FadeOut","/Script/Engine.AudioComponent.FadeIn","/Script/Engine.AudioComponent.GetPlayState",
    "/Script/Engine.GameplayStatics.BeginDeferredActorSpawnFromClass","/Script/Engine.GameplayStatics.FinishSpawningActor",
    "/Script/Engine.CharacterMovementComponent.DisableMovement","/Script/Engine.GameplayStatics.SpawnSound2D",
    "/Script/Engine.GameplayStatics.SpawnSoundAtLocation","/Script/Engine.AudioComponent.Stop",
    "/Script/Niagara.Default__NiagaraFunctionLibrary","/Script/Niagara.NiagaraFunctionLibrary.SpawnSystemAtLocation",
    "/Script/Niagara.NiagaraFunctionLibrary.SpawnSystemAttached","/Script/Engine.SceneComponent.IsVisible",
    "/Script/Engine.ActorComponent.IsComponentTickEnabled","/Script/Engine.ActorComponent.IsActive",
    "/Script/Engine.ActorComponent.K2_DestroyComponent"
};
constexpr std::string_view sound_path="/Game/Items/Weapons/CrushingLance/CrushingLanceLoop_Cue.CrushingLanceLoop_Cue";
constexpr std::string_view attenuation_path="/Game/Player/Assets/Audio/CharacterAudioAtten.CharacterAudioAtten";
constexpr std::string_view enemy_path="/Game/Enemies/Boner_00/Skeletons24/Gray/Boner_Gray_DualSword_BP.Boner_Gray_DualSword_BP_C";
constexpr std::string_view mute_cue="/Game/Items/Weapons/Daggers/HomingDagger/HomingDaggerThrowSound.HomingDaggerThrowSound";
constexpr std::string_view ambient_trail="/Game/Enemies/Boner_00/Skeletons24/Black/Boner_Gun_ProjectileTrail.Boner_Gun_ProjectileTrail";
constexpr std::string_view replica_burst="/Game/Enemies/Boner_00/Skeletons24/Black/Boner_SlashGun_ImpactFX.Boner_SlashGun_ImpactFX";
constexpr float initial_volume=.060f,initial_pitch=1.f,moved_volume=.095f,moved_pitch=.87f;
std::array<UE::WeakKey,count> symbols{};
UE::WeakKey voice{},cue{},attenuation{},enemy{};
UE::WeakKey published_fx{};
std::wstring root;
ULONGLONG started{},next_scan{},next_status{},next_spawn{};
unsigned phase{};
bool reported{},source_played{},source_fading_in{},source_fading{},source_stopped{};
bool remote_fading_in{},remote_played{},remote_moved{},remote_fading{},remote_stopped{};
bool ambient{},stock_spawn_bypassed{},mute_test{};
bool fx_mute_test{},host_fx_visible{},host_fx_stopped{};
unsigned muted_voices{};
unsigned hidden_effects{};
unsigned (*read_core)(char*,unsigned){};
using FadeExec=void(*)(void*,void*,void*);
struct TestFadeHook {FadeExec next{};void* volatile* cell{};};
TestFadeHook fade_out_hook,fade_in_hook;
DWORD fade_thread{};

bool source(){return lan.role==(ambient?L"host":L"client");}
bool receiver(){return lan.role==(ambient?L"client":L"host");}
Address fn(Symbol id){return ui.get(symbols[id]);}
std::wstring file(std::wstring_view name){return root+L"/"+std::wstring(name);}
bool seen(std::wstring_view name){return marker_exists(file(name).c_str());}
bool members_ready(){
    for(const auto role:{L"host",L"client",L"client2",L"client3"})if(!seen(std::wstring(role)+L"/ready.txt"))return false;
    return true;
}
bool receivers_done(){
    for(const auto role:{L"host",L"client",L"client2",L"client3"})
        if(lan.role!=role&&!seen(L"loop-"+std::wstring(role)+L".json"))return false;
    return true;
}
bool is_playing(Address component){
    if(!component||!ui.method(fn(playing),1,{{"ReturnValue","BoolProperty",0,1}}))return false;
    unsigned char active{};
    TitleUi::event(component,fn(playing),&active);
    return active!=0;
}
unsigned voice_state(Address component){
    if(!component||!ui.method(fn(play_state),1,{{"ReturnValue","EnumProperty",0,1}}))return 255;
    unsigned char state{};
    TitleUi::event(component,fn(play_state),&state);
    return state;
}
bool fade_voice(Address component){
    if(!component||!ui.method(fn(fade_out),9,{{"FadeOutDuration","FloatProperty",0,4},
       {"FadeVolumeLevel","FloatProperty",4,4},{"FadeCurve","EnumProperty",8,1}}))return false;
    struct Params {float duration,target;unsigned char curve;};
    Params args{.95f,0.f,2};
    TitleUi::event(component,fn(fade_out),&args);
    return voice_state(component)==4;
}
void observe_fade(void* context,void* frame,void* result){
    const auto component=reinterpret_cast<Address>(context);
    const bool watched=GetCurrentThreadId()==fade_thread&&frame&&ui.get(voice)==component;
    fade_out_hook.next(context,frame,result);
    if(!watched)return;
    const auto parameters=UE::field<Address>(frame,40);
    const auto duration=parameters?at<float>(parameters,0):0.f;
    const auto target=parameters?at<float>(parameters,4):0.f;
    const auto curve=parameters?at<unsigned char>(parameters,8):255;
    const auto actual=ui.get(voice);
    const auto fading=voice_state(actual)==4;
    remote_fading=remote_fading||(fading&&duration>0.f&&duration<=.951f&&target==0.f&&curve==2);
    if(auto stream=_wfopen(file(L"loop-host-fade.txt").c_str(),L"ab")){
        std::fprintf(stream,"duration=%.4f target=%.3f curve=%u active=%u fading=%u\n",
                     duration,target,curve,is_playing(actual),fading);
        std::fclose(stream);
    }
}
void observe_fade_in(void* context,void* frame,void* result){
    fade_in_hook.next(context,frame,result);
    if(GetCurrentThreadId()!=fade_thread||!frame)return;
    const auto component=reinterpret_cast<Address>(context);
    const auto asset=ui.object(component,"Sound");
    if(!asset||!path_is(asset,sound_path))return;
    const auto parameters=UE::field<Address>(frame,40);
    const auto duration=parameters?at<float>(parameters,0):0.f;
    const auto target=parameters?at<float>(parameters,4):0.f;
    const auto start=parameters?at<float>(parameters,8):0.f;
    const auto curve=parameters?at<unsigned char>(parameters,12):255;
    const auto state=voice_state(component);
    remote_fading_in=remote_fading_in||(state==3&&duration>0.f&&duration<=.851f&&target==1.f&&
        std::abs(start-.035f)<.0001f&&curve==1);
    if(remote_fading_in)ui.remember(voice,component);
    if(auto stream=_wfopen(file(L"loop-host-fade-in.txt").c_str(),L"ab")){
        std::fprintf(stream,"duration=%.4f target=%.3f start=%.3f curve=%u state=%u\n",duration,target,start,curve,state);
        std::fclose(stream);
    }
}
bool watch_fade(TestFadeHook& hook,Address function,FadeExec observer){
    if(hook.cell)return true;
    if(!function)return false;
    auto cell=reinterpret_cast<void* volatile*>(function+216);
    const auto next=*cell;
    MEMORY_BASIC_INFORMATION memory{};
    if(!next||!VirtualQuery(next,&memory,sizeof(memory))||
       memory.AllocationBase!=GetModuleHandleW(L"QDCoopNativeM1.dll")||!(memory.Protect&0xf0u))return false;
    hook.next=reinterpret_cast<FadeExec>(next);
    fade_thread=GetCurrentThreadId();
    if(InterlockedCompareExchangePointer(cell,reinterpret_cast<void*>(observer),next)!=next)return false;
    hook.cell=cell;
    return true;
}
bool restore_fade(TestFadeHook& hook,FadeExec observer){
    if(!hook.cell)return true;
    const auto address=reinterpret_cast<void*>(observer),next=reinterpret_cast<void*>(hook.next);
    const auto found=InterlockedCompareExchangePointer(hook.cell,next,address);
    if(found!=address&&found!=next)return false;
    hook.cell=nullptr;
    return true;
}
bool cleanup(){
    const bool in=restore_fade(fade_in_hook,&observe_fade_in);
    const bool out=restore_fade(fade_out_hook,&observe_fade);
    return in&&out;
}
bool locate(Address component,std::array<float,3>& location){
    if(!component||!ui.method(fn(position),12,{{"ReturnValue","StructProperty",0,12}}))return false;
    TitleUi::event(component,fn(position),location.data());
    return true;
}
bool spot(std::wstring_view name,std::array<float,3>& position,bool write){
    const auto stream=_wfopen(file(name).c_str(),write?L"wb":L"rb");
    if(!stream)return false;
    const auto count=write?std::fwrite(position.data(),sizeof(float),position.size(),stream):
        std::fread(position.data(),sizeof(float),position.size(),stream);
    std::fclose(stream);
    return count==position.size();
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
            if(!ui.get(symbols[j])&&name==paths[j].substr(paths[j].rfind('.')+1)&&path_is(item.object,paths[j]))
                ui.remember(symbols[j],item.object);
        if(mute_test&&receiver()&&!ui.get(enemy)&&!(at<unsigned>(item.object,8)&0x10u)){
            const auto type=at<Address>(item.object,16);
            if(leaf(type)=="Boner_Gray_DualSword_BP_C"&&path_is(type,enemy_path))ui.remember(enemy,item.object);
        }
        if(fx_mute_test&&receiver()&&!ui.get(published_fx)&&!(at<unsigned>(item.object,8)&0x10u)&&
           leaf(at<Address>(item.object,16))=="NiagaraComponent"){
            const auto asset=ui.object(item.object,"Asset"),parent=ui.object(item.object,"AttachParent");
            if(asset&&path_is(asset,ambient_trail)&&parent&&leaf(at<Address>(parent,16))=="StaticMeshComponent"&&
               !ui.object(parent,"StaticMesh"))ui.remember(published_fx,item.object);
        }
        if(!receiver()||remote_played||ui.get(voice)||(at<unsigned>(item.object,8)&0x10u)||
           leaf(at<Address>(item.object,16))!="AudioComponent")continue;
        const auto asset=ui.object(item.object,"Sound");
        if(asset&&path_is(asset,sound_path))ui.remember(voice,item.object);
    }
}
bool get_status(std::string& status){
    if(!read_core){
        const auto dll=GetModuleHandleW(L"QDCoopNativeM1.dll");
        if(!dll||!load_export(dll,"QDEnemyNativeStatus",read_core))return false;
    }
    const auto size=read_core(nullptr,0);
    if(!size||size>16384)return false;
    status.resize(size);
    if(read_core(status.data(),size)!=size)return false;
    if(auto stream=_wfopen(file(L"loop-core-"+lan.role+L".json").c_str(),L"wb")){
        std::fputs(status.c_str(),stream);
        std::fclose(stream);
    }
    return true;
}
bool status_is(const std::string& status,std::string_view field,unsigned value){
    return status.find("\""+std::string(field)+"\":"+std::to_string(value)+",")!=std::string::npos;
}
Address ambient_parent(Address pawn){
    if(const auto actor=ui.get(enemy))return ui.object(actor,"RootComponent");
    if(!ui.method(fn(begin_actor),88,{{"WorldContextObject","ObjectProperty",0,8},{"ActorClass","ClassProperty",8,8},
        {"SpawnTransform","StructProperty",16,48},{"CollisionHandlingOverride","EnumProperty",64,1},
        {"Owner","ObjectProperty",72,8},{"ReturnValue","ObjectProperty",80,8}})||
       !ui.method(fn(finish_actor),72,{{"Actor","ObjectProperty",0,8},{"SpawnTransform","StructProperty",16,48},
        {"ReturnValue","ObjectProperty",64,8}})||!ui.method(fn(disable_move),0,{}))return 0;
    const auto type=ui.load_asset_path(enemy_path);
    std::array<float,3> point{};
    if(!type||!locate(ui.object(pawn,"RootComponent"),point))return 0;
    const std::array<float,12> transform{0,0,0,1,point[0]+220.f,point[1],point[2]+30.f,0,1,1,1,0};
    alignas(16) std::array<unsigned char,96> begin{},finish{};
    std::memcpy(begin.data(),&pawn,8);
    std::memcpy(begin.data()+8,&type,8);
    std::memcpy(begin.data()+16,transform.data(),48);
    begin[64]=1;
    const auto gameplay=ui.get(TitleUi::gameplay);
    TitleUi::event(gameplay,fn(begin_actor),begin.data());
    const auto actor=UE::field<Address>(begin.data(),80);
    if(!actor)return 0;
    std::memcpy(finish.data(),&actor,8);
    std::memcpy(finish.data()+16,transform.data(),48);
    TitleUi::event(gameplay,fn(finish_actor),finish.data());
    if(UE::field<Address>(finish.data(),64)!=actor)return 0;
    if(const auto movement=ui.object(actor,"CharacterMovement"))TitleUi::event(movement,fn(disable_move),nullptr);
    ui.remember(enemy,actor);
    return ui.object(actor,"RootComponent");
}
bool spawn_without_observer(Address gameplay,Address function,void* parameters){
    auto cell=reinterpret_cast<void* volatile*>(function+216);
    const auto observer=*cell;
    const auto address=image+0x2dc6450;
    const auto stock=reinterpret_cast<void*>(address);
    MEMORY_BASIC_INFORMATION region{};
    if(!ui.shipping_code(address)||!observer||!VirtualQuery(observer,&region,sizeof(region))||
       region.AllocationBase!=GetModuleHandleW(L"QDCoopNativeM1.dll")||
       InterlockedCompareExchangePointer(cell,stock,observer)!=observer)return false;
    TitleUi::event(gameplay,function,parameters);
    return InterlockedCompareExchangePointer(cell,observer,stock)==stock;
}
bool fx_bool(Address effect,Symbol method){
    if(!effect||!ui.method(fn(method),1,{{"ReturnValue","BoolProperty",0,1}}))return false;
    unsigned char result{};
    TitleUi::event(effect,fn(method),&result);
    return result!=0;
}
bool destroy_effect(Address effect){
    if(!effect)return true;
    if(!ui.method(fn(fx_destroy),8,{{"Object","ObjectProperty",0,8}}))return false;
    TitleUi::event(effect,fn(fx_destroy),&effect);
    return true;
}
Address stock_fx(Address actor,Address parent,bool attached){
    const auto asset=ui.load_asset_path(attached?ambient_trail:replica_burst),library=fn(fx_library);
    if(!asset||!library||!parent)return 0;
    alignas(16) std::array<unsigned char,64> args{};
    const auto put=[&](unsigned offset,const auto& value){std::memcpy(args.data()+offset,&value,sizeof(value));};
    if(attached){
        if(!ui.method(fn(fx_attached),64,{{"SystemTemplate","ObjectProperty",0,8},{"AttachToComponent","ObjectProperty",8,8},
           {"AttachPointName","NameProperty",16,8},{"Location","StructProperty",24,12},{"Rotation","StructProperty",36,12},
           {"LocationType","ByteProperty",48,1},{"bAutoDestroy","BoolProperty",49,1},{"bAutoActivate","BoolProperty",50,1},
           {"PoolingMethod","EnumProperty",51,1},{"bPreCullCheck","BoolProperty",52,1},{"ReturnValue","ObjectProperty",56,8}}))
            return 0;
        put(0,asset);put(8,parent);put(24,std::array{30.f,0.f,80.f});args[49]=args[50]=1;
        TitleUi::event(library,fn(fx_attached),args.data());
    }else{
        if(!ui.method(fn(fx_location),64,{{"WorldContextObject","ObjectProperty",0,8},{"SystemTemplate","ObjectProperty",8,8},
           {"Location","StructProperty",16,12},{"Rotation","StructProperty",28,12},{"Scale","StructProperty",40,12},
           {"bAutoDestroy","BoolProperty",52,1},{"bAutoActivate","BoolProperty",53,1},{"PoolingMethod","EnumProperty",54,1},
           {"bPreCullCheck","BoolProperty",55,1},{"ReturnValue","ObjectProperty",56,8}}))return 0;
        std::array<float,3> point{};
        if(!locate(parent,point))return 0;
        point[2]+=80.f;
        put(0,actor);put(8,asset);put(16,point);put(40,std::array{1.f,1.f,1.f});args[52]=args[53]=1;
        TitleUi::event(library,fn(fx_location),args.data());
    }
    return UE::field<Address>(args.data(),56);
}
bool replica_fx_proof(){
    if(hidden_effects==2)return host_fx_visible;
    const auto replicated=ui.get(published_fx);
    if(!fx_bool(replicated,fx_visible)||!fx_bool(replicated,fx_active))return false;
    const auto actor=ui.get(enemy),parent=ui.object(actor,"RootComponent");
    if(!actor||!parent||!ui.method(fn(fx_visible),1,{{"ReturnValue","BoolProperty",0,1}})||
       !ui.method(fn(fx_tick),1,{{"ReturnValue","BoolProperty",0,1}}))return false;
    for(;hidden_effects<2;++hidden_effects){
        const auto effect=stock_fx(actor,parent,hidden_effects!=0);
        if(!effect)return false;
        const auto field=ui.member(effect,"bHiddenInGame","BoolProperty",1);
        const bool hidden=field&&UE::get_bool(reinterpret_cast<void*>(field),reinterpret_cast<void*>(effect),0);
        const bool visible=fx_bool(effect,fx_visible),ticking=fx_bool(effect,fx_tick);
        if(auto stream=_wfopen(file(L"loop-replica-fx.txt").c_str(),L"ab")){
            std::fprintf(stream,"%s visible=%u hidden=%u ticking=%u\n",
                         hidden_effects?"SpawnSystemAttached":"SpawnSystemAtLocation",visible,hidden,ticking);
            std::fclose(stream);
        }
        if(!destroy_effect(effect)||visible||!hidden||ticking)return false;
    }
    host_fx_visible=fx_bool(replicated,fx_visible)&&fx_bool(replicated,fx_active);
    return host_fx_visible;
}
bool start_voice(Address instance){
    if(!ui.get(cue))if(const auto asset=ui.load_asset_path(sound_path))ui.remember(cue,asset);
    if(!ui.get(attenuation))if(const auto asset=ui.load_asset_path(attenuation_path))ui.remember(attenuation,asset);
    const auto asset=ui.get(cue),atten=ui.get(attenuation),function=fn(spawn);
    if(!asset||!atten||!ui.method(function,96,{{"Sound","ObjectProperty",0,8},{"AttachToComponent","ObjectProperty",8,8},
        {"AttachPointName","NameProperty",16,8},{"Location","StructProperty",24,12},{"Rotation","StructProperty",36,12},
        {"LocationType","ByteProperty",48,1},{"bStopWhenAttachedToDestroyed","BoolProperty",49,1},
        {"VolumeMultiplier","FloatProperty",52,4},{"PitchMultiplier","FloatProperty",56,4},{"StartTime","FloatProperty",60,4},
        {"AttenuationSettings","ObjectProperty",64,8},{"ConcurrencySettings","ObjectProperty",72,8},
        {"bAutoDestroy","BoolProperty",80,1},{"ReturnValue","ObjectProperty",88,8}})||
       !ui.method(fn(volume),4,{{"NewVolumeMultiplier","FloatProperty",0,4}})||
       !ui.method(fn(fade_in),13,{{"FadeInDuration","FloatProperty",0,4},{"FadeVolumeLevel","FloatProperty",4,4},
           {"StartTime","FloatProperty",8,4},{"FadeCurve","EnumProperty",12,1}}))return false;
    const auto gs=ui.get(TitleUi::gameplay);
    Address pawn[]{instance,0,0};
    TitleUi::event(gs,ui.get(TitleUi::local_pawn),pawn);
    const auto parent=ambient?ambient_parent(pawn[2]):ui.object(pawn[2],"RootComponent");
    if(!parent)return false;
    if(fx_mute_test&&!ui.get(published_fx)){
        const auto visual=stock_fx(ui.get(enemy),parent,true);
        if(!fx_bool(visual,fx_active)||!fx_bool(visual,fx_visible))return false;
        ui.remember(published_fx,visual);
    }
    alignas(16) std::array<unsigned char,96> parameters{};
    const auto put=[&](unsigned offset,const auto& value){std::memcpy(parameters.data()+offset,&value,sizeof(value));};
    const std::array offset{30.f,20.f,70.f};
    put(0,asset);put(8,parent);put(24,offset);put(56,initial_pitch);put(64,atten);
    parameters[80]=1;
    if(ambient){
        put(52,initial_volume);
        stock_spawn_bypassed=spawn_without_observer(gs,function,parameters.data());
        if(!stock_spawn_bypassed)return false;
    }else TitleUi::event(gs,function,parameters.data());
    const auto component=UE::field<Address>(parameters.data(),88);
    if(!component)return false;
    if(!ambient){
        float level=initial_volume;
        TitleUi::event(component,fn(volume),&level);
        struct Ramp {float duration,target,start;unsigned char curve;};
        Ramp ramp{.85f,1.f,.035f,1};
        TitleUi::event(component,fn(fade_in),&ramp);
        source_fading_in=voice_state(component)==3;
    }
    std::array<float,3> location{};
    if(!(ambient?is_playing(component):source_fading_in)||!locate(component,location))return false;
    ui.remember(voice,component);
    return spot(L"loop-origin.bin",location,true);
}
bool move_voice(Address component){
    if(!component||!ui.method(fn(move),153,{{"NewLocation","StructProperty",0,12},{"bSweep","BoolProperty",12,1},
       {"SweepHitResult","StructProperty",16,136},{"bTeleport","BoolProperty",152,1}})||
       !ui.method(fn(volume),4,{{"NewVolumeMultiplier","FloatProperty",0,4}})||
       !ui.method(fn(pitch),4,{{"NewPitchMultiplier","FloatProperty",0,4}}))return false;
    std::array<float,3> location{};
    if(!locate(component,location))return false;
    location[0]+=240.f;
    alignas(16) std::array<unsigned char,160> args{};
    std::memcpy(args.data(),location.data(),sizeof(location));
    args[152]=1;
    TitleUi::event(component,fn(move),args.data());
    float level=moved_volume,rate=moved_pitch;
    TitleUi::event(component,fn(volume),&level);
    TitleUi::event(component,fn(pitch),&rate);
    return locate(component,location)&&spot(L"loop-moved.bin",location,true);
}
bool voice_matches(Address component,std::wstring_view name,float level,float rate){
    const auto vol=ui.member(component,"VolumeMultiplier","FloatProperty",4);
    const auto speed=ui.member(component,"PitchMultiplier","FloatProperty",4);
    if(!vol||!speed||!is_playing(component)||std::abs(at<float>(component,at<unsigned>(vol,76))-level)>.001f||
       std::abs(at<float>(component,at<unsigned>(speed,76))-rate)>.001f)return false;
    std::array<float,3> expected{},actual{};
    if(!spot(name,expected,false)||!locate(component,actual))return false;
    for(unsigned axis=0;axis<3;++axis)if(std::abs(actual[axis]-expected[axis])>2.f)return false;
    return true;
}
bool check_muted_voice(Address voice,std::string_view route){
    const auto field=ui.member(voice,"VolumeMultiplier","FloatProperty",4);
    if(!field)return false;
    const auto value=at<float>(voice,at<unsigned>(field,76));
    if(auto stream=_wfopen(file(L"loop-replica-mute.txt").c_str(),L"ab")){
        std::fprintf(stream,"%.*s volume=%.4f\n",static_cast<int>(route.size()),route.data(),static_cast<double>(value));
        std::fclose(stream);
    }
    TitleUi::event(voice,fn(stop_voice),nullptr);
    return value==0.f;
}
bool replica_mute_proof(){
    if(muted_voices==3)return true;
    const auto actor=ui.get(enemy);
    const auto parent=ui.object(actor,"RootComponent");
    if(!actor||!parent||!ui.method(fn(stop_voice),0,{})||
       !ui.method(fn(spawn_2d),56,{{"WorldContextObject","ObjectProperty",0,8},{"Sound","ObjectProperty",8,8},
        {"VolumeMultiplier","FloatProperty",16,4},{"PitchMultiplier","FloatProperty",20,4},{"StartTime","FloatProperty",24,4},
        {"ConcurrencySettings","ObjectProperty",32,8},{"bPersistAcrossLevelTransition","BoolProperty",40,1},
        {"bAutoDestroy","BoolProperty",41,1},{"ReturnValue","ObjectProperty",48,8}})||
       !ui.method(fn(spawn_at_location),88,{{"WorldContextObject","ObjectProperty",0,8},{"Sound","ObjectProperty",8,8},
        {"Location","StructProperty",16,12},{"Rotation","StructProperty",28,12},{"VolumeMultiplier","FloatProperty",40,4},
        {"PitchMultiplier","FloatProperty",44,4},{"StartTime","FloatProperty",48,4},{"AttenuationSettings","ObjectProperty",56,8},
        {"ConcurrencySettings","ObjectProperty",64,8},{"bAutoDestroy","BoolProperty",72,1},{"ReturnValue","ObjectProperty",80,8}})||
       !ui.method(fn(spawn),96,{{"Sound","ObjectProperty",0,8},{"AttachToComponent","ObjectProperty",8,8},
        {"AttachPointName","NameProperty",16,8},{"Location","StructProperty",24,12},{"Rotation","StructProperty",36,12},
        {"LocationType","ByteProperty",48,1},{"bStopWhenAttachedToDestroyed","BoolProperty",49,1},
        {"VolumeMultiplier","FloatProperty",52,4},{"PitchMultiplier","FloatProperty",56,4},{"StartTime","FloatProperty",60,4},
        {"AttenuationSettings","ObjectProperty",64,8},{"ConcurrencySettings","ObjectProperty",72,8},
        {"bAutoDestroy","BoolProperty",80,1},{"ReturnValue","ObjectProperty",88,8}}))return false;
    const auto asset=ui.load_asset_path(mute_cue),atten=ui.load_asset_path(attenuation_path);
    std::array<float,3> point{};
    if(!asset||!atten||!locate(parent,point))return false;
    const auto gameplay=ui.get(TitleUi::gameplay);
    constexpr float audible=.25f,rate=1.f;
    alignas(16) std::array<unsigned char,96> args{};
    const auto put=[&](unsigned offset,const auto& value){std::memcpy(args.data()+offset,&value,sizeof(value));};
    if(muted_voices==0){
        put(0,actor);put(8,asset);put(16,audible);put(20,rate);args[41]=1;
        TitleUi::event(gameplay,fn(spawn_2d),args.data());
        if(!check_muted_voice(UE::field<Address>(args.data(),48),"SpawnSound2D"))return false;
        ++muted_voices;
    }
    args={};
    if(muted_voices==1){
        put(0,actor);put(8,asset);put(16,point);put(40,audible);put(44,rate);put(56,atten);args[72]=1;
        TitleUi::event(gameplay,fn(spawn_at_location),args.data());
        if(!check_muted_voice(UE::field<Address>(args.data(),80),"SpawnSoundAtLocation"))return false;
        ++muted_voices;
    }
    args={};
    put(0,asset);put(8,parent);put(52,audible);put(56,rate);put(64,atten);args[80]=1;
    TitleUi::event(gameplay,fn(spawn),args.data());
    if(!check_muted_voice(UE::field<Address>(args.data(),88),"SpawnSoundAttached"))return false;
    ++muted_voices;
    return true;
}
void report(){
    if(auto stream=_wfopen(file(L"loop-"+lan.role+L".json").c_str(),L"wb")){
        std::fprintf(stream,"{\"passed\":1,\"host\":%u,\"source\":%u,\"source_played\":%u,\"source_fading_in\":%u,\"source_fading\":%u,"
            "\"source_stopped\":%u,\"remote_fading_in\":%u,\"remote_played\":%u,\"remote_moved\":%u,\"remote_fading\":%u,\"remote_stopped\":%u,"
            "\"ambient\":%u,\"stock_spawn_bypassed\":%u,\"mute_test\":%u,\"muted_voices\":%u,"
            "\"fx_mute_test\":%u,\"hidden_effects\":%u,\"host_fx_visible\":%u,\"host_fx_stopped\":%u}\n",
            lan.host,source(),source_played,source_fading_in,source_fading,source_stopped,remote_fading_in,
            remote_played,remote_moved,remote_fading,remote_stopped,ambient,stock_spawn_bypassed,mute_test,muted_voices,
            fx_mute_test,hidden_effects,host_fx_visible,host_fx_stopped);
        std::fclose(stream);
    }
    reported=true;
}
bool step(Address instance,ULONGLONG time){
    if(root.empty()){
        wchar_t directory[32768]{};
        if(!environment_title(L"QD_PROJECTILE_TITLE_ROOT",directory,32768))return false;
        root=directory;
        wchar_t variant[12]{};
        if(environment_title(L"QD_SOUND_LOOP_VARIANT",variant,12)){
            fx_mute_test=std::wcscmp(variant,L"vfx-mute")==0;
            mute_test=fx_mute_test||std::wcscmp(variant,L"mute")==0;
            ambient=mute_test||std::wcscmp(variant,L"ambient")==0;
        }
    }
    if(reported)return !lan.host||seen(L"loop-client.json");
    if(!started)started=time;
    if((source()||receiver())&&time>=next_scan&&(!fn(fade_out)||!fn(fade_in)||!fn(play_state)||!fn(move)||
       (ambient&&source()&&(!fn(begin_actor)||!fn(finish_actor)||!fn(disable_move)))||
       (mute_test&&receiver()&&(!ui.get(enemy)||!fn(spawn_2d)||!fn(spawn_at_location)||!fn(stop_voice)))||
       (fx_mute_test&&(!fn(fx_library)||!fn(fx_attached)||!fn(fx_location)||!fn(fx_visible)||!fn(fx_tick)||
                      !fn(fx_active)||!fn(fx_destroy)||(receiver()&&!remote_played&&!ui.get(published_fx))))||
       (receiver()&&!remote_played&&!ui.get(voice)))){
        next_scan=time+200;
        scan();
    }
    std::string status;
    if(time>=next_status){next_status=time+500;get_status(status);}
    const auto component=ui.get(voice);
    if(source()){
        if(!phase&&members_ready()&&seen(L"loop-host-observing.txt")&&time>=started+1800&&time>=next_spawn){
            next_spawn=time+1000;
            if(start_voice(instance)){source_played=true;phase=1;}
        }else if(phase==1&&seen(L"loop-host-playing.txt")&&move_voice(component)){
            phase=2;
        }else if(phase==2){
            if(seen(L"loop-host-moved.txt")){
                if(fade_voice(component)){
                    if(fx_mute_test)host_fx_stopped=destroy_effect(ui.get(published_fx));
                    source_fading=true;
                    phase=3;
                }
            }else if(ambient){
                std::array<float,3> location{};
                if(locate(component,location))spot(L"loop-moved.bin",location,true);
            }
        }else if(phase==3&&!is_playing(component)&&receivers_done()&&get_status(status)&&status_is(status,"sound_loop_observed",1)&&
                 status_is(status,"sound_loop_retired",1)&&status_is(status,"sound_loop_received",0)&&
                 status_is(status,"sound_loop_fade_observed",1)&&status_is(status,"sound_loop_fade_in_observed",ambient?0:1)&&
                 status_is(status,"sound_loop_ambient_discovered",ambient?1:0)&&
                 (!fx_mute_test||(host_fx_stopped&&status_is(status,"projectile_observed",1)&&status_is(status,"projectile_retired",1)))){
            source_stopped=true;
            report();
        }
    }else if(receiver()){
        if(!seen(L"loop-host-observing.txt")&&(ambient||watch_fade(fade_in_hook,fn(fade_in),&observe_fade_in)))
            marker(file(L"loop-host-observing.txt").c_str());
        if(!remote_played&&(ambient||remote_fading_in)&&voice_state(component)==0&&
           voice_matches(component,L"loop-origin.bin",initial_volume,initial_pitch)&&
           (!mute_test||(get_status(status)&&status_is(status,"ownership_taken",1)&&replica_mute_proof()))&&
           (!fx_mute_test||replica_fx_proof())){
            remote_played=true;
            marker(file(L"loop-host-playing.txt").c_str());
        }
        if(remote_played&&!remote_moved&&voice_matches(component,L"loop-moved.bin",moved_volume,moved_pitch)&&
           watch_fade(fade_out_hook,fn(fade_out),&observe_fade)){
            remote_moved=true;
            marker(file(L"loop-host-moved.txt").c_str());
        }
        if(remote_fading&&!is_playing(component)&&get_status(status)&&status_is(status,"sound_loop_played",1)&&
           status_is(status,"sound_loop_retired",1)&&status_is(status,"sound_loop_stopped",1)&&
           status_is(status,"sound_loop_fade_applied",1)&&status_is(status,"sound_loop_fade_in_applied",ambient?0:1)&&
           (!fx_mute_test||(!ui.get(published_fx)&&status_is(status,"projectile_removed",1)))&&cleanup()){
            remote_stopped=true;
            report();
        }
    }else if(get_status(status)&&status_is(status,"sound_loop_retired",1)&&status_is(status,"sound_loop_fade_received",1)&&
             status_is(status,"sound_loop_fade_in_received",ambient?0:1)&&
             (!fx_mute_test||status_is(status,"projectile_retired",1)))report();
    return reported&&(!lan.host||seen(L"loop-client.json"));
}
}
