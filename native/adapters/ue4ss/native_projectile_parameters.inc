using ProjectileParameterType=Projectile::ParameterType;
struct ProjectileString {wchar_t* data{};int size{},capacity{};};
static_assert(sizeof(ProjectileString)==16);
struct ProjectileParameterHook {
    Weak function;
    std::atomic<KillExec> original{};
    KillExec observer{};
    std::uintptr_t implementation{};
};
constexpr unsigned projectile_generic_float=10;
static std::array<ProjectileParameterHook,projectile_generic_float+1> projectile_parameter_hooks;
static std::array<AnimU::NameKey,Projectile::assets::variables.size()> projectile_parameter_names;
struct ProjectileParameterName {AnimU::NameKey name;std::uint16_t id{};};
static std::array<ProjectileParameterName,64> projectile_parameter_ids;

static constexpr unsigned projectile_parameter_index(ProjectileParameterType type,bool text=false){
    return 2*(static_cast<unsigned>(type)-1)+static_cast<unsigned>(text);
}
static char projectile_lower(char value){
    return value>='A'&&value<='Z'?static_cast<char>(value+('a'-'A')):value;
}
static std::uint16_t projectile_parameter_id(std::string_view name){
    if(name.empty()||name.size()>96)return 0;
    const auto equal=[](char a,char b){return projectile_lower(a)==projectile_lower(b);};
    for(unsigned i=0;i<Projectile::assets::variables.size();++i){
        const auto full=Projectile::assets::variables[i];
        const auto short_name=full.substr(5);
        if(std::equal(name.begin(),name.end(),full.begin(),full.end(),equal)||
           std::equal(name.begin(),name.end(),short_name.begin(),short_name.end(),equal))return static_cast<std::uint16_t>(i+1);
    }
    return 0;
}
static std::uint16_t projectile_parameter_id(AnimU::NameKey key){
    if(!key.index)return 0;
    auto& cached=projectile_parameter_ids[(key.index+key.number)%projectile_parameter_ids.size()];
    if(cached.name.index==key.index&&cached.name.number==key.number)return cached.id;
    std::string name;
    cached={key,AnimU::name_string(catalog_name_pool,key,catalog_read,name)?projectile_parameter_id(name):std::uint16_t{}};
    return cached.id;
}
static std::uint16_t projectile_parameter_id(const ProjectileString& key){
    if(!key.data||key.size<2||key.size>97||key.capacity<key.size||key.data[key.size-1])return 0;
    std::string name;
    name.reserve(static_cast<unsigned>(key.size-1));
    for(int i=0;i<key.size-1;++i){
        if(key.data[i]<1||key.data[i]>127)return 0;
        name.push_back(static_cast<char>(key.data[i]));
    }
    return projectile_parameter_id(name);
}
static ProjectileSource* projectile_parameter_source(void* component){
    if(const auto effect=projectile_tracked_effect(component))return effect;
    for(auto& source:projectile_sources)
        if(source.frame.id&&source.frame.phase==Projectile::Phase::flying&&
           api.weakget(&source.trail.component)==component)return &source;
    return nullptr;
}

template<ProjectileParameterType type,class Key>
static void projectile_dispatch_parameter(std::uintptr_t address,void* context,Key name,
                                          const Projectile::Parameter& parameter,const void* reference){
    if constexpr(type==ProjectileParameterType::floating)
        reinterpret_cast<void(*)(void*,Key,float)>(address)(context,name,std::bit_cast<float>(parameter.value[0]));
    else if constexpr(type==ProjectileParameterType::boolean)
        reinterpret_cast<void(*)(void*,Key,bool)>(address)(context,name,parameter.value[0]!=0);
    else if constexpr(type==ProjectileParameterType::integer)
        reinterpret_cast<void(*)(void*,Key,std::int32_t)>(address)(context,name,std::bit_cast<std::int32_t>(parameter.value[0]));
    else
        reinterpret_cast<void(*)(void*,Key,const void*)>(address)(context,name,reference);
}
template<ProjectileParameterType type,bool text,bool generic=false>
static void observe_projectile_parameter(void* context,void* frame,void* result){
    using Key=std::conditional_t<text,ProjectileString,AnimU::NameKey>;
    constexpr auto index=generic?projectile_generic_float:projectile_parameter_index(type,text);
    auto& hook=projectile_parameter_hooks[index];
    const auto original=hook.original.load(std::memory_order_acquire);
    if(GetCurrentThreadId()!=game_thread.load()||!active_peer_mask||!context||!frame||!projectile_parameter_source(context)){
        original(context,frame,result);
        return;
    }
    Key key{};
    alignas(16) std::array<std::uint32_t,4> words{};
    auto& code=*reinterpret_cast<const unsigned char**>(static_cast<unsigned char*>(frame)+32);
    std::uint16_t id{};
    if(!code){
        const auto function=api.weakget(&hook.function);
        const auto params=AnimU::field<const unsigned char*>(frame,40);
        if(params&&AnimU::field<void*>(frame,16)==function){
            key=AnimU::field<Key>(params,0);
            id=projectile_parameter_id(key);
            if constexpr(type==ProjectileParameterType::boolean){
                const auto field=catalog_field(function,"InValue",1);
                if(medic_field_class(field,"BoolProperty"))words[0]=api.getbool(field,params,0);
                else id=0;
            }else std::memcpy(words.data(),params+sizeof(Key),4*Projectile::parameter_words(type));
        }
        original(context,frame,result);
    }else{
        native_vm_argument(frame,&key);
        if constexpr(type==ProjectileParameterType::color)
            *reinterpret_cast<void**>(static_cast<unsigned char*>(frame)+56)=nullptr;
        native_vm_argument(frame,words.data());
        const void* reference=words.data();
        if constexpr(type==ProjectileParameterType::color){
            if(const auto evaluated=AnimU::field<const void*>(frame,56);evaluated&&evaluated!=reference){
                reference=evaluated;
                std::memcpy(words.data(),reference,16);
            }
        }
        if(code)++code;
        id=projectile_parameter_id(key);
        const auto target=generic?
            AnimU::field<std::uintptr_t>(AnimU::field<const void*>(context,0),0x890):hook.implementation;
        const Projectile::Parameter parameter{id,type,words};
        if constexpr(text){
            projectile_dispatch_parameter<type>(target,context,&key,parameter,reference);
            if(key.data)native_free(key.data);
        }else projectile_dispatch_parameter<type>(target,context,key,parameter,reference);
    }
    if(!id)return;
    if constexpr(type==ProjectileParameterType::boolean)words[0]=words[0]!=0;
    if(const auto source=projectile_parameter_source(context))Projectile::set_parameter(source->frame,{id,type,words});
}

static constexpr std::array<unsigned char,24> projectile_parameter_prefix(unsigned char saved,unsigned char stack){
    return {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,saved,0x57,0x48,
            0x83,0xec,stack,0x33,0xff,0x48,0x8b,0xda,0x48,0x89,0x7c,0x24};
}
template<ProjectileParameterType type,bool text=false>
static void install_projectile_parameter(std::uintptr_t image,unsigned world,std::uintptr_t exec,std::uintptr_t call,
                                         std::uintptr_t stock,unsigned char saved,unsigned char stack){
    constexpr auto index=projectile_parameter_index(type,text);
    constexpr auto name_size=text?16:8;
    constexpr auto value_size=type==ProjectileParameterType::boolean?1:4*Projectile::parameter_words(type);
    auto& hook=projectile_parameter_hooks[index];
    if(api.weakget(&hook.function)||(text&&!native_free)||
       !medic_params(world,name_size+value_size,{{"InVariableName",name_size,0},{"InValue",value_size,name_size}}))return;
    const auto instruction=reinterpret_cast<const unsigned char*>(image+call);
    if(*instruction!=0xe8||image+call+5+AnimU::field<std::int32_t>(instruction,1)!=image+stock)return;
    const auto prefix=projectile_parameter_prefix(saved,stack);
    hook.implementation=image+stock;
    hook.observer=&observe_projectile_parameter<type,text>;
    native_install_exec(world_input(world),exec,prefix.data(),prefix.size(),hook.function,hook.original,hook.observer);
}
static void install_projectile_parameter_observers(){
    if(!native_bind_vm())return;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    using Type=ProjectileParameterType;
    install_projectile_parameter<Type::floating>(image,world_projectile_niagara_float,0x995d70,0x995e29,0x831300,0x18,0x20);
    install_projectile_parameter<Type::floating,true>(image,world_projectile_niagara_string_float,0x9951e0,0x99529e,0x82fc00,0x18,0x30);
    install_projectile_parameter<Type::boolean>(image,world_projectile_niagara_bool,0x995ca0,0x995d57,0x831230,0x18,0x20);
    install_projectile_parameter<Type::boolean,true>(image,world_projectile_niagara_string_bool,0x995100,0x9951bc,0x82fbb0,0x18,0x30);
    install_projectile_parameter<Type::integer>(image,world_projectile_niagara_int,0x995e40,0x995ef3,0x8313d0,0x18,0x20);
    install_projectile_parameter<Type::integer,true>(image,world_projectile_niagara_string_int,0x9952d0,0x995388,0x82fc50,0x18,0x30);
    install_projectile_parameter<Type::vector>(image,world_projectile_niagara_vec3,0x9963f0,0x9964b3,0x831910,0x18,0x40);
    install_projectile_parameter<Type::vector,true>(image,world_projectile_niagara_string_vec3,0x995670,0x995738,0x82fed0,0x10,0x50);
    install_projectile_parameter<Type::color>(image,world_projectile_niagara_color,0x995f10,0x995fc9,0x8314a0,0x18,0x30);
    install_projectile_parameter<Type::color,true>(image,world_projectile_niagara_string_color,0x9953b0,0x99546e,0x82fca0,0x10,0x40);
    auto& hook=projectile_parameter_hooks[projectile_generic_float];
    if(!api.weakget(&hook.function)&&medic_params(world_projectile_fx_float,12,{{"ParameterName",8,0},{"Param",4,8}})){
        constexpr unsigned char exec[]{0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57,0x48,
            0x83,0xec,0x20,0x33,0xff,0x48,0x8b,0xda,0x48,0x89,0x7c,0x24};
        constexpr unsigned char dispatch[]{0xff,0x90,0x90,0x08,0x00,0x00};
        if(!std::memcmp(reinterpret_cast<void*>(image+0x2e6ddcc),dispatch,sizeof(dispatch))){
            hook.observer=&observe_projectile_parameter<Type::floating,false,true>;
            native_install_exec(world_input(world_projectile_fx_float),0x2e6dd10,exec,sizeof(exec),
                                hook.function,hook.original,hook.observer);
        }
    }
}
static void projectile_apply_parameters(void* component,ProjectileVisual& visual,const Projectile::Frame& frame){
    if(visual.parameters==frame.parameters)return;
    for(unsigned i=0;i<frame.parameters.size()&&frame.parameters[i].id;++i){
        const auto& item=frame.parameters[i];
        if(visual.parameters[i]==item)continue;
        const auto index=Projectile::particle(frame.profile)?projectile_generic_float:projectile_parameter_index(item.type);
        const auto setter=api.weakget(&projectile_parameter_hooks[index].function);
        if(!setter)continue;
        auto& key=projectile_parameter_names[item.id-1];
        if(!key.index){
            const auto path=Projectile::assets::variables[item.id-1];
            key=gear_name(std::wstring(path.begin(),path.end()).c_str());
        }
        if(!key.index)continue;
        alignas(16) std::array<unsigned char,24> args{};
        std::memcpy(args.data(),&key,8);
        if(item.type==ProjectileParameterType::boolean)args[8]=static_cast<unsigned char>(item.value[0]);
        else std::memcpy(args.data()+8,item.value.data(),4*Projectile::parameter_words(item.type));
        api.event(component,setter,args.data());
        visual.parameters[i]=item;
    }
}
