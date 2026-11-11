namespace AttachedFxFixture {
enum Symbol {library,spawn,active,location,move,destroy,component_type,activate,deactivate,set_active_fn,
             float_name,float_string,float_generic,make_name,strings,set_asset_fn,color_name,color_string,
             point_spawn,math_library,make_transform,read_transform,is_visible,set_visibility,set_hidden,set_actor_hidden,count};
constexpr std::array<std::string_view,count> paths{
    "/Script/Niagara.Default__NiagaraFunctionLibrary","/Script/Niagara.NiagaraFunctionLibrary.SpawnSystemAttached",
    "/Script/Engine.ActorComponent.IsActive","/Script/Engine.SceneComponent.K2_GetComponentLocation",
    "/Script/Engine.SceneComponent.K2_SetWorldLocation","/Script/Engine.ActorComponent.K2_DestroyComponent",
    "/Script/Niagara.NiagaraComponent","/Script/Engine.ActorComponent.Activate","/Script/Engine.ActorComponent.Deactivate",
    "/Script/Engine.ActorComponent.SetActive","/Script/Niagara.NiagaraComponent.SetVariableFloat",
    "/Script/Niagara.NiagaraComponent.SetNiagaraVariableFloat","/Script/Engine.FXSystemComponent.SetFloatParameter",
    "/Script/Engine.KismetStringLibrary.Conv_StringToName","/Script/Engine.Default__KismetStringLibrary",
    "/Script/Niagara.NiagaraComponent.SetAsset","/Script/Niagara.NiagaraComponent.SetVariableLinearColor",
    "/Script/Niagara.NiagaraComponent.SetNiagaraVariableLinearColor",
    "/Script/Niagara.NiagaraFunctionLibrary.SpawnSystemAtLocation","/Script/Engine.Default__KismetMathLibrary",
    "/Script/Engine.KismetMathLibrary.MakeTransform","/Script/Engine.SceneComponent.K2_GetComponentToWorld",
    "/Script/Engine.SceneComponent.IsVisible","/Script/Engine.SceneComponent.SetVisibility",
    "/Script/Engine.SceneComponent.SetHiddenInGame","/Script/Engine.Actor.SetActorHiddenInGame"
};
constexpr std::string_view template_path="/Game/Items/Weapons/Daggers/Default/Dagger_Projectile_Trail.Dagger_Projectile_Trail";
constexpr std::string_view scalar_template="/Game/Items/Weapons/Slashgun/SlashGun_Trail.SlashGun_Trail";
constexpr std::string_view color_template="/Game/Items/Weapons/CrushingLance/Lance_CharFX.Lance_CharFX";
constexpr wchar_t scalar_name[]=L"User.Opacity";
constexpr wchar_t color_parameter[]=L"User.WindColor";
constexpr std::array scalar_values{.24f,.63f,.88f};
constexpr std::array<std::array<float,4>,2> color_values{{{.19f,.31f,.73f,1.f},{.70f,.30f,.10f,.80f}}};
std::array<UE::WeakKey,count> symbols{};
UE::WeakKey asset{},effect{},replacement{},effect_owner{};
std::wstring root;
ULONGLONG start{},next_scan{},next_attempt{},next_status{};
bool created{},moved{},stopped{},rendered{},remote_moved{},reported{},activity{},set_active{},scalar_mode{};
bool replacement_mode{},swapped{},replacement_rendered{};
bool typed_mode{};
bool point_mode{},point_attempted{},point_source_inactive{},point_pose_verified{};
bool visibility_mode{},owner_initial_hidden{};
unsigned source_visibility{},remote_visibility{};
constexpr unsigned visibility_steps=6;
unsigned remote_deactivations{},source_scalars{},remote_scalars{},invalid_scalars{};
using Exec=void(*)(void*,void*,void*);
Exec original_deactivate{},original_scalar{};
void* volatile* deactivate_cell{};
void* volatile* scalar_cell{};
DWORD observer_thread{};
unsigned (*read_core)(char*,unsigned){};

bool source(){return lan.role==L"client";}
std::string_view system_path(){return typed_mode||point_mode?color_template:scalar_mode?scalar_template:template_path;}
unsigned parameter_steps(){return typed_mode?static_cast<unsigned>(color_values.size()):static_cast<unsigned>(scalar_values.size());}
Address fn(Symbol symbol){return ui.get(symbols[symbol]);}
std::wstring file(std::wstring_view name){return root+L"/"+std::wstring(name);}
bool seen(std::wstring_view name){return marker_exists(file(name).c_str());}
bool waiting_replacement(){
    return replacement_mode&&rendered&&!replacement_rendered&&seen(L"fx-source-asset.txt");
}
bool members_ready(){
    for(const auto role:{L"host",L"client",L"client2",L"client3"})
        if(!seen(std::wstring(role)+L"/ready.txt"))return false;
    return true;
}
void scan(){
    const auto array=at<UE::ObjectArray>(image+TitleUi::object_array,0);
    if(!UE::object_array_shape(array))return;
    const bool binding=std::any_of(symbols.begin(),symbols.end(),[](UE::WeakKey key){return !ui.get(key);});
    const auto type=fn(component_type);
    const bool replacing=lan.host&&waiting_replacement()&&!ui.get(replacement);
    for(unsigned i=static_cast<unsigned>(array.num_elements);i;){
        --i;
        const auto chunk=at<Address>(array.chunks,(i/UE::objects_per_chunk)*8);
        if(!chunk)continue;
        const auto item=at<UE::ObjectItem>(chunk,(i%UE::objects_per_chunk)*sizeof(UE::ObjectItem));
        if(!item.object||(item.flags&0x30000000u))continue;
        if(lan.host&&((!rendered&&!ui.get(effect))||replacing)&&type&&at<Address>(item.object,16)==type&&
           !(at<unsigned>(item.object,8)&0x10u)){
            const auto system=ui.object(item.object,"Asset"),parent=ui.object(item.object,"AttachParent");
            const auto expected=replacing?scalar_template:system_path();
            if(system&&path_is(system,expected)&&parent&&leaf(at<Address>(parent,16))=="StaticMeshComponent"&&
               !ui.object(parent,"StaticMesh"))ui.remember(replacing?replacement:effect,item.object);
        }
        if(!binding)continue;
        const auto name=leaf(item.object);
        for(unsigned j=0;j<paths.size();++j)
            if(!ui.get(symbols[j])&&name==paths[j].substr(paths[j].rfind('.')+1)&&path_is(item.object,paths[j]))
                ui.remember(symbols[j],item.object);
    }
}
bool running(Address component){
    if(!component||!ui.method(fn(active),1,{{"ReturnValue","BoolProperty",0,1}}))return false;
    unsigned char result{};
    TitleUi::event(component,fn(active),&result);
    return result!=0;
}
bool set_effect_active(Address component,bool enabled){
    if(!ui.method(fn(set_active_fn),2,{{"bNewActive","BoolProperty",0,1},{"bReset","BoolProperty",1,1}}))return false;
    unsigned char args[]{static_cast<unsigned char>(enabled),1};
    TitleUi::event(component,fn(set_active_fn),args);
    return true;
}
void observe_deactivate(void* context,void* frame,void* result){
    const bool watched=GetCurrentThreadId()==observer_thread&&reinterpret_cast<Address>(context)==ui.get(effect);
    original_deactivate(context,frame,result);
    if(watched)++remote_deactivations;
}
void observe_scalar(void* context,void* frame,void* result){
    const bool watched=GetCurrentThreadId()==observer_thread&&reinterpret_cast<Address>(context)==ui.get(effect);
    original_scalar(context,frame,result);
    if(!watched||!frame)return;
    const auto params=UE::field<Address>(frame,40);
    std::string name;
    if(!params||!UE::name_string(image+TitleUi::name_pool,at<UE::NameKey>(params,0),read_local,name)||
       name!=(typed_mode?"User.WindColor":"User.Opacity"))return;
    bool valid=remote_scalars<parameter_steps();
    if(typed_mode){
        const auto value=at<std::array<float,4>>(params,8);
        for(unsigned i=0;valid&&i<value.size();++i)valid=std::abs(value[i]-color_values[remote_scalars][i])<.0001f;
        if(auto stream=_wfopen(file(L"fx-host-color.txt").c_str(),L"ab")){
            std::fprintf(stream,"%s rgba=%.4f,%.4f,%.4f,%.4f matched=%u\n",name.c_str(),static_cast<double>(value[0]),
                static_cast<double>(value[1]),static_cast<double>(value[2]),static_cast<double>(value[3]),valid);
            std::fclose(stream);
        }
    }else if(auto stream=_wfopen(file(L"fx-host-scalar.txt").c_str(),L"ab")){
        const auto value=at<float>(params,8);
        valid=valid&&std::abs(value-scalar_values[remote_scalars])<.0001f;
        std::fprintf(stream,"%s value=%.4f matched=%u\n",name.c_str(),static_cast<double>(value),valid);
        std::fclose(stream);
    }else valid=false;
    if(!valid){
        ++invalid_scalars;
        return;
    }
    ++remote_scalars;
    marker(file(L"fx-scalar-"+std::to_wstring(remote_scalars)+L".txt").c_str());
}
bool watch(Address function,Exec observer,Exec& original,void* volatile*& hook_cell){
    if(hook_cell)return true;
    auto cell=reinterpret_cast<void* volatile*>(function+216);
    const auto current=*cell;
    MEMORY_BASIC_INFORMATION region{};
    if(!current||!VirtualQuery(current,&region,sizeof(region))||!(region.Protect&0xf0u)||
       region.AllocationBase!=GetModuleHandleW(L"QDCoopNativeM1.dll"))return false;
    original=reinterpret_cast<Exec>(current);
    observer_thread=GetCurrentThreadId();
    if(InterlockedCompareExchangePointer(cell,reinterpret_cast<void*>(observer),current)!=current)return false;
    hook_cell=cell;
    return true;
}
bool restore(Exec observer,Exec original,void* volatile*& cell){
    if(!cell)return true;
    const auto hook=reinterpret_cast<void*>(observer),stock=reinterpret_cast<void*>(original);
    const auto previous=InterlockedCompareExchangePointer(cell,stock,hook);
    if(previous!=hook&&previous!=stock)return false;
    cell=nullptr;
    return true;
}
bool cleanup(){
    const bool scalar=restore(&observe_scalar,original_scalar,scalar_cell);
    return restore(&observe_deactivate,original_deactivate,deactivate_cell)&&scalar;
}
bool watch_effect(){
    if(activity&&(!ui.method(fn(deactivate),0,{})||
       !watch(fn(deactivate),&observe_deactivate,original_deactivate,deactivate_cell)))return false;
    if(typed_mode)return ui.method(fn(color_name),24,{{"InVariableName","NameProperty",0,8},{"InValue","StructProperty",8,16}})&&
       watch(fn(color_name),&observe_scalar,original_scalar,scalar_cell);
    return !scalar_mode||(ui.method(fn(float_name),12,{{"InVariableName","NameProperty",0,8},{"InValue","FloatProperty",8,4}})&&
       watch(fn(float_name),&observe_scalar,original_scalar,scalar_cell));
}
struct FixtureString {const wchar_t* data;int count,capacity;};
UE::NameKey fixture_name(FixtureString text){
    if(!fn(strings)||!ui.method(fn(make_name),24,{{"inString","StrProperty",0,16},{"ReturnValue","NameProperty",16,8}}))
        return {};
    struct Convert {FixtureString text;UE::NameKey name;} convert{text,{}};
    TitleUi::event(fn(strings),fn(make_name),&convert);
    return convert.name;
}
bool send_scalar(Address component,unsigned index){
    if(!component||index>=scalar_values.size())return false;
    constexpr auto length=static_cast<int>(std::size(scalar_name));
    constexpr FixtureString key{scalar_name,length,length};
    if(index==1){
        if(!ui.method(fn(float_string),20,{{"InVariableName","StrProperty",0,16},{"InValue","FloatProperty",16,4}}))return false;
        struct Args {FixtureString name;float value;} args{key,scalar_values[index]};
        TitleUi::event(component,fn(float_string),&args);
        return true;
    }
    const auto name=fixture_name(key);
    if(!name.index)return false;
    const auto function=index==0?fn(float_name):fn(float_generic);
    if(!ui.method(function,12,{{index==0?"InVariableName":"ParameterName","NameProperty",0,8},
                              {index==0?"InValue":"Param","FloatProperty",8,4}}))return false;
    struct Args {UE::NameKey name;float value;} args{name,scalar_values[index]};
    TitleUi::event(component,function,&args);
    return true;
}
bool send_color(Address component,unsigned index){
    if(!component||index>=color_values.size())return false;
    constexpr auto length=static_cast<int>(std::size(color_parameter));
    constexpr FixtureString key{color_parameter,length,length};
    if(index){
        if(!ui.method(fn(color_string),32,{{"InVariableName","StrProperty",0,16},{"InValue","StructProperty",16,16}}))
            return false;
        struct Args {FixtureString name;std::array<float,4> value;} args{key,color_values[index]};
        TitleUi::event(component,fn(color_string),&args);
        return true;
    }
    const auto name=fixture_name(key);
    if(!name.index||!ui.method(fn(color_name),24,{{"InVariableName","NameProperty",0,8},{"InValue","StructProperty",8,16}}))
        return false;
    struct Args {UE::NameKey name;std::array<float,4> value;} args{name,color_values[index]};
    TitleUi::event(component,fn(color_name),&args);
    return true;
}
bool locate(Address component,std::array<float,3>& spot){
    if(!component||!ui.method(fn(location),12,{{"ReturnValue","StructProperty",0,12}}))return false;
    TitleUi::event(component,fn(location),spot.data());
    return true;
}
bool coords(std::wstring_view name,std::array<float,3>& spot,bool write){
    const auto stream=_wfopen(file(name).c_str(),write?L"wb":L"rb");
    if(!stream)return false;
    const auto count=write?std::fwrite(spot.data(),sizeof(float),spot.size(),stream):
        std::fread(spot.data(),sizeof(float),spot.size(),stream);
    std::fclose(stream);
    return count==spot.size();
}
bool at_coords(Address component,std::wstring_view name){
    std::array<float,3> actual{},expected{};
    if(!running(component)||!locate(component,actual)||!coords(name,expected,false))return false;
    for(unsigned axis=0;axis<actual.size();++axis)if(std::abs(actual[axis]-expected[axis])>1.f)return false;
    return true;
}
bool change_owner_hidden(Address owner,bool hidden){
    const auto property=ui.member(owner,"bHidden","BoolProperty",1);
    if(!property||!ui.method(fn(set_actor_hidden),1,{{"bNewHidden","BoolProperty",0,1}}))return false;
    unsigned char args=hidden;
    TitleUi::event(owner,fn(set_actor_hidden),&args);
    return UE::get_bool(reinterpret_cast<void*>(property),reinterpret_cast<void*>(owner),0)==hidden;
}
bool start_effect(Address instance){
    if(!ui.get(asset))if(const auto system=ui.load_asset_path(system_path()))ui.remember(asset,system);
    const auto system=ui.get(asset);
    if(!system||!fn(library)||!ui.method(fn(spawn),64,{{"SystemTemplate","ObjectProperty",0,8},
        {"AttachToComponent","ObjectProperty",8,8},{"AttachPointName","NameProperty",16,8},
        {"Location","StructProperty",24,12},{"Rotation","StructProperty",36,12},{"LocationType","ByteProperty",48,1},
        {"bAutoDestroy","BoolProperty",49,1},{"bAutoActivate","BoolProperty",50,1},{"PoolingMethod","EnumProperty",51,1},
        {"bPreCullCheck","BoolProperty",52,1},{"ReturnValue","ObjectProperty",56,8}}))return false;
    Address pawn[]{instance,0,0};
    TitleUi::event(ui.get(TitleUi::gameplay),ui.get(TitleUi::local_pawn),pawn);
    const auto parent=ui.object(pawn[2],"RootComponent");
    if(!parent)return false;
    if(visibility_mode&&!ui.get(effect_owner)){
        const auto hidden=ui.member(pawn[2],"bHidden","BoolProperty",1);
        if(!hidden)return false;
        owner_initial_hidden=UE::get_bool(reinterpret_cast<void*>(hidden),reinterpret_cast<void*>(pawn[2]),0);
        if(!change_owner_hidden(pawn[2],false))return false;
        ui.remember(effect_owner,pawn[2]);
    }
    alignas(16) std::array<unsigned char,64> args{};
    std::memcpy(args.data(),&system,8);
    std::memcpy(args.data()+8,&parent,8);
    const std::array<float,3> offset{100.f,0.f,100.f};
    std::memcpy(args.data()+24,offset.data(),sizeof(offset));
    args[50]=!activity;
    TitleUi::event(fn(library),fn(spawn),args.data());
    const auto component=UE::field<Address>(args.data(),56);
    if(!ui.is(component,fn(component_type))||ui.object(component,"AttachParent")!=parent)return false;
    if(activity){
        if(running(component))return false;
        if(set_active){
            if(!set_effect_active(component,true))return false;
        }else{
            if(!ui.method(fn(activate),1,{{"bReset","BoolProperty",0,1}}))return false;
            unsigned char reset=1;
            TitleUi::event(component,fn(activate),&reset);
        }
    }
    if(!running(component))return false;
    ui.remember(effect,component);
    std::array<float,3> spot{};
    return locate(component,spot)&&coords(L"fx-origin.bin",spot,true);
}
bool move_effect(Address component){
    if(!ui.method(fn(move),153,{{"NewLocation","StructProperty",0,12},{"bSweep","BoolProperty",12,1},
       {"SweepHitResult","StructProperty",16,136},{"bTeleport","BoolProperty",152,1}}))return false;
    std::array<float,3> spot{};
    if(!locate(component,spot))return false;
    spot[0]+=240.f;
    alignas(16) std::array<unsigned char,160> args{};
    std::memcpy(args.data(),spot.data(),sizeof(spot));
    args[152]=1;
    TitleUi::event(component,fn(move),args.data());
    return locate(component,spot)&&coords(L"fx-moved.bin",spot,true);
}
bool effect_visible(Address component,bool expected){
    if(!component||!ui.method(fn(is_visible),1,{{"ReturnValue","BoolProperty",0,1}}))return false;
    unsigned char visible{};
    TitleUi::event(component,fn(is_visible),&visible);
    return visible==static_cast<unsigned char>(expected);
}
bool change_visibility(Address component,unsigned stage){
    if(!component||stage>=visibility_steps)return false;
    const bool visible=stage%2!=0;
    if(stage>=4)return change_owner_hidden(ui.get(effect_owner),!visible)&&running(component);
    const bool hidden_fn=stage>=2;
    const auto function=fn(hidden_fn?set_hidden:set_visibility);
    if(!ui.method(function,2,{{hidden_fn?"NewHidden":"bNewVisibility","BoolProperty",0,1},
                              {"bPropagateToChildren","BoolProperty",1,1}}))return false;
    unsigned char args[]{static_cast<unsigned char>(hidden_fn?!visible:visible),0};
    TitleUi::event(component,function,args);
    return effect_visible(component,visible)&&running(component);
}
std::wstring visibility_marker(const wchar_t* prefix,unsigned stage){
    return std::wstring(prefix)+std::to_wstring(stage)+L".txt";
}
void receive_visibility(Address component){
    if(remote_visibility>=visibility_steps||!seen(visibility_marker(L"fx-source-visible-",remote_visibility+1)))return;
    const bool visible=remote_visibility%2!=0;
    if(!effect_visible(component,visible)||!running(component))return;
    if(remote_visibility==0&&!at_coords(component,L"fx-moved.bin"))return;
    if(auto stream=_wfopen(file(L"fx-host-visibility.txt").c_str(),L"ab")){
        std::fprintf(stream,"stage=%u visible=%u active=1\n",remote_visibility+1,visible);
        std::fclose(stream);
        ++remote_visibility;
        marker(file(visibility_marker(L"fx-host-visible-",remote_visibility)).c_str());
    }
}
bool stop_effect(Address component){
    if(!component)return false;
    if(set_active)return set_effect_active(component,false);
    if(activity){
        if(!ui.method(fn(deactivate),0,{}))return false;
        TitleUi::event(component,fn(deactivate),nullptr);
        return true;
    }
    if(!ui.method(fn(destroy),8,{{"Object","ObjectProperty",0,8}}))return false;
    TitleUi::event(component,fn(destroy),&component);
    return true;
}
bool swap_effect(Address component){
    if(!component||!ui.method(fn(set_asset_fn),9,{{"InAsset","ObjectProperty",0,8},
        {"bResetExistingOverrideParameters","BoolProperty",8,1}}))return false;
    const auto target=ui.load_asset_path(scalar_template);
    if(!target||ui.object(component,"Asset")==target)return false;
    struct Args {Address asset;unsigned char reset;} args{target,1};
    TitleUi::event(component,fn(set_asset_fn),&args);
    return ui.get(effect)==component&&ui.object(component,"Asset")==target&&running(component);
}
bool status(std::string& result){
    if(!read_core){
        const auto dll=GetModuleHandleW(L"QDCoopNativeM1.dll");
        if(!dll||!load_export(dll,"QDEnemyNativeStatus",read_core))return false;
    }
    const auto size=read_core(nullptr,0);
    if(!size||size>16384)return false;
    result.resize(size);
    if(read_core(result.data(),size)!=size)return false;
    if(auto stream=_wfopen(file(L"fx-core-"+lan.role+L".json").c_str(),L"wb")){
        std::fputs(result.c_str(),stream);
        std::fclose(stream);
    }
    return true;
}
bool counter(const std::string& value,std::string_view name,unsigned amount){
    return value.find("\""+std::string(name)+"\":"+std::to_string(amount)+",")!=value.npos;
}
bool point_file(std::wstring_view name,std::array<float,12>& pose,bool write){
    const auto stream=_wfopen(file(name).c_str(),write?L"wb":L"rb");
    if(!stream)return false;
    const auto result=write?std::fwrite(pose.data(),sizeof(float),pose.size(),stream):
        std::fread(pose.data(),sizeof(float),pose.size(),stream);
    std::fclose(stream);
    return result==pose.size();
}
bool start_point(Address instance){
    const auto system=ui.get(asset);
    if(!system||!fn(library)||!fn(math_library)||
       !ui.method(fn(make_transform),96,{{"Location","StructProperty",0,12},{"Rotation","StructProperty",12,12},
          {"Scale","StructProperty",24,12},{"ReturnValue","StructProperty",48,48}})||
       !ui.method(fn(point_spawn),64,{{"WorldContextObject","ObjectProperty",0,8},{"SystemTemplate","ObjectProperty",8,8},
          {"Location","StructProperty",16,12},{"Rotation","StructProperty",28,12},{"Scale","StructProperty",40,12},
          {"bAutoDestroy","BoolProperty",52,1},{"bAutoActivate","BoolProperty",53,1},{"PoolingMethod","EnumProperty",54,1},
          {"bPreCullCheck","BoolProperty",55,1},{"ReturnValue","ObjectProperty",56,8}}))return false;
    Address pawn[]{instance,0,0};
    TitleUi::event(ui.get(TitleUi::gameplay),ui.get(TitleUi::local_pawn),pawn);
    std::array<float,3> point{};
    if(!locate(ui.object(pawn[2],"RootComponent"),point))return false;
    point[0]+=140.f;
    point[2]+=100.f;
    constexpr std::array rotation{13.f,27.f,4.f},scale{1.2f,.9f,1.1f};
    alignas(16) std::array<unsigned char,96> args{};
    std::memcpy(args.data(),point.data(),12);
    std::memcpy(args.data()+12,rotation.data(),12);
    std::memcpy(args.data()+24,scale.data(),12);
    TitleUi::event(fn(math_library),fn(make_transform),args.data());
    auto pose=UE::field<std::array<float,12>>(args.data(),48);
    if(!point_file(L"fx-point-expected.bin",pose,true)||!coords(L"fx-origin.bin",point,true))return false;
    args={};
    std::memcpy(args.data(),&pawn[2],8);
    std::memcpy(args.data()+8,&system,8);
    std::memcpy(args.data()+16,point.data(),12);
    std::memcpy(args.data()+28,rotation.data(),12);
    std::memcpy(args.data()+40,scale.data(),12);
    args[52]=args[53]=args[55]=1;
    TitleUi::event(fn(library),fn(point_spawn),args.data());
    point_attempted=true;
    const auto original=UE::field<Address>(args.data(),56);
    point_source_inactive=!original||!running(original);
    if(auto stream=_wfopen(file(L"fx-point-source.txt").c_str(),L"wb")){
        std::fprintf(stream,"returned=%u inactive=%u\n",original!=0,point_source_inactive);
        std::fclose(stream);
    }
    return point_source_inactive;
}
bool point_matches(Address component){
    if(!running(component)||!ui.method(fn(read_transform),48,{{"ReturnValue","StructProperty",0,48}}))return false;
    alignas(16) std::array<float,12> actual{},wanted{};
    if(!point_file(L"fx-point-expected.bin",wanted,false))return false;
    TitleUi::event(component,fn(read_transform),actual.data());
    float dot{};
    for(unsigned i=0;i<4;++i)dot+=actual[i]*wanted[i];
    if(std::abs(std::abs(dot)-1.f)>.002f)return false;
    for(unsigned i=0;i<3;++i)
        if(std::abs(actual[i+4]-wanted[i+4])>1.f||std::abs(actual[i+8]-wanted[i+8])>.01f)return false;
    return point_file(L"fx-point-actual.bin",actual,true);
}
void report(){
    if(auto stream=_wfopen(file(L"fx-"+lan.role+L".json").c_str(),L"wb")){
        std::fprintf(stream,"{\"passed\":1,\"source\":%u,\"created\":%u,\"moved\":%u,\"stopped\":%u,\"rendered\":%u,"
            "\"remote_moved\":%u,\"activity\":%u,\"set_active\":%u,\"remote_deactivations\":%u,"
            "\"scalar_mode\":%u,\"source_scalars\":%u,\"remote_scalars\":%u,\"invalid_scalars\":%u,"
            "\"replacement_mode\":%u,\"swapped\":%u,\"replacement_rendered\":%u,\"typed_mode\":%u,"
            "\"point_mode\":%u,\"point_source_inactive\":%u,\"point_pose_verified\":%u,"
            "\"visibility_mode\":%u,\"source_visibility\":%u,\"remote_visibility\":%u}\n",
            source(),created,moved,stopped,rendered,remote_moved,activity,set_active,remote_deactivations,
            scalar_mode,source_scalars,remote_scalars,invalid_scalars,replacement_mode,swapped,replacement_rendered,typed_mode,
            point_mode,point_source_inactive,point_pose_verified,visibility_mode,source_visibility,remote_visibility);
        std::fclose(stream);
        reported=true;
    }
}
bool point_step(Address instance,ULONGLONG time){
    if((source()||lan.host)&&!ui.get(asset))
        if(const auto loaded=ui.load_asset_path(system_path()))ui.remember(asset,loaded);
    if(lan.host&&ui.get(asset)&&fn(read_transform)&&fn(component_type)&&!seen(L"fx-point-ready.txt"))
        marker(file(L"fx-point-ready.txt").c_str());
    std::string sample;
    if(time>=next_status){
        next_status=time+500;
        status(sample);
    }
    if(source()){
        if(!point_attempted&&members_ready()&&seen(L"fx-point-ready.txt")&&time>=start+1800)
            created=start_point(instance);
        if(created&&seen(L"fx-host.json")&&seen(L"fx-client2.json")&&seen(L"fx-client3.json")&&status(sample)&&
           counter(sample,"projectile_fallback_births",1)&&counter(sample,"projectile_observed",1)&&
           counter(sample,"projectile_retired",1)&&counter(sample,"projectile_received",0)){
            stopped=true;
            report();
        }
    }else{
        const auto component=ui.get(effect);
        if(lan.host&&!point_pose_verified&&point_matches(component))rendered=point_pose_verified=true;
        if((!lan.host||(point_pose_verified&&!component))&&status(sample)&&counter(sample,"projectile_spawned",1)&&
           counter(sample,"projectile_retired",1)&&counter(sample,"projectile_removed",1))report();
    }
    return reported&&(!lan.host||seen(L"fx-client.json"));
}
bool step(Address instance,ULONGLONG time){
    if(root.empty()){
        wchar_t directory[32768]{};
        if(!environment_title(L"QD_PROJECTILE_TITLE_ROOT",directory,32768))return false;
        root=directory;
        start=time;
        wchar_t mode[2]{};
        const bool selected=environment_title(L"QD_FX_ACTIVITY",mode,2);
        scalar_mode=selected&&mode[0]==L'3';
        replacement_mode=selected&&mode[0]==L'4';
        typed_mode=selected&&mode[0]==L'5';
        point_mode=selected&&mode[0]==L'6';
        visibility_mode=selected&&mode[0]==L'7';
        set_active=scalar_mode||replacement_mode||typed_mode||visibility_mode||(selected&&mode[0]==L'2');
        activity=set_active||(selected&&mode[0]==L'1');
    }
    if(reported)return !lan.host||seen(L"fx-client.json");
    if((source()||lan.host)&&time>=next_scan&&(!fn(component_type)||!fn(move)||!fn(destroy)||!fn(spawn)||
       !fn(activate)||!fn(deactivate)||!fn(set_active_fn)||
       (scalar_mode&&(!fn(float_name)||!fn(float_string)||!fn(float_generic)||!fn(strings)||!fn(make_name)))||
       (typed_mode&&(!fn(color_name)||!fn(color_string)||!fn(strings)||!fn(make_name)))||
       (replacement_mode&&(!fn(set_asset_fn)||waiting_replacement()))||
       (point_mode&&(!fn(point_spawn)||!fn(math_library)||!fn(make_transform)||!fn(read_transform)))||
       (visibility_mode&&(!fn(is_visible)||!fn(set_visibility)||!fn(set_hidden)||!fn(set_actor_hidden)))||
       (lan.host&&!rendered&&!ui.get(effect)))){
        next_scan=time+250;
        scan();
    }
    if(point_mode)return point_step(instance,time);
    std::string sample;
    if(time>=next_status){
        next_status=time+500;
        status(sample);
    }
    const auto component=ui.get(effect);
    const auto expected=replacement_mode?2u:1u;
    const bool parameter_mode=scalar_mode||typed_mode;
    if(source()){
        if(!created&&members_ready()&&time>=start+1800&&time>=next_attempt){
            next_attempt=time+1000;
            created=start_effect(instance);
        }
        if(created&&!moved&&seen(L"fx-host-playing.txt")){
            if(visibility_mode){
                if(source_visibility||change_visibility(component,0)){
                    source_visibility=1;
                    moved=move_effect(component);
                    if(moved)marker(file(visibility_marker(L"fx-source-visible-",source_visibility)).c_str());
                }
            }else if(replacement_mode&&!swapped){
                if(swap_effect(component)){
                    swapped=true;
                    marker(file(L"fx-source-asset.txt").c_str());
                }
            }else if((!replacement_mode||seen(L"fx-host-asset.txt"))&&
                     (!parameter_mode||seen(L"fx-scalar-"+std::to_wstring(parameter_steps())+L".txt"))){
                moved=move_effect(component);
            }
            else if(parameter_mode&&(!source_scalars||seen(L"fx-scalar-"+std::to_wstring(source_scalars)+L".txt"))&&
                    (typed_mode?send_color(component,source_scalars):send_scalar(component,source_scalars))){
                ++source_scalars;
                marker(file(L"fx-source-scalar-"+std::to_wstring(source_scalars)+L".txt").c_str());
            }
        }
        if(visibility_mode&&moved&&source_visibility<visibility_steps&&
           seen(visibility_marker(L"fx-host-visible-",source_visibility))&&change_visibility(component,source_visibility)){
            ++source_visibility;
            marker(file(visibility_marker(L"fx-source-visible-",source_visibility)).c_str());
        }
        const bool visibility_done=!visibility_mode||seen(visibility_marker(L"fx-host-visible-",visibility_steps));
        if(moved&&!stopped&&visibility_done&&seen(L"fx-host-moved.txt"))stopped=stop_effect(component);
        const bool finished=activity?component&&!running(component):!component;
        if(stopped&&finished&&seen(L"fx-host.json")&&seen(L"fx-client2.json")&&seen(L"fx-client3.json")&&
           status(sample)&&counter(sample,"projectile_observed",expected)&&counter(sample,"projectile_retired",expected)&&
           counter(sample,"projectile_received",0)&&(!visibility_mode||change_owner_hidden(ui.get(effect_owner),owner_initial_hidden)))report();
    }else{
        if(lan.host&&!rendered&&at_coords(component,L"fx-origin.bin")&&
           (!visibility_mode||effect_visible(component,true))&&watch_effect()){
            rendered=true;
            marker(file(L"fx-host-playing.txt").c_str());
        }
        if(lan.host&&rendered&&visibility_mode)receive_visibility(component);
        if(lan.host&&waiting_replacement()&&!component&&at_coords(ui.get(replacement),L"fx-origin.bin")){
            effect=replacement;
            replacement_rendered=true;
            marker(file(L"fx-host-asset.txt").c_str());
        }
        if(rendered&&(!replacement_mode||replacement_rendered)&&!remote_moved&&at_coords(component,L"fx-moved.bin")){
            remote_moved=true;
            marker(file(L"fx-host-moved.txt").c_str());
        }
        if((!lan.host||(remote_moved&&!component&&(!activity||remote_deactivations==1)&&
           (!visibility_mode||remote_visibility==visibility_steps)&&
           (!parameter_mode||(remote_scalars==parameter_steps()&&!invalid_scalars))))&&status(sample)&&
           counter(sample,"projectile_spawned",expected)&&counter(sample,"projectile_retired",expected)&&
           counter(sample,"projectile_removed",expected)&&cleanup())report();
    }
    return reported&&(!lan.host||seen(L"fx-client.json"));
}
}
