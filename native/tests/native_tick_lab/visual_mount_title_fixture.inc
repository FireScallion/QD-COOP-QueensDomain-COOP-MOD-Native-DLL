namespace MountFixture {
namespace E=qdcoop::enemy;
enum Symbol {strings,make_name,location,static_type,add,attach,set_mesh,relative,collision,material,make_mid,
             set_scalar,read_scalar,set_material,destroy,count};
constexpr std::array<std::string_view,count> paths{
    "/Script/Engine.Default__KismetStringLibrary","/Script/Engine.KismetStringLibrary.Conv_StringToName",
    "/Script/Engine.Actor.K2_GetActorLocation","/Script/Engine.StaticMeshComponent","/Script/Engine.Actor.AddComponentByClass",
    "/Script/Engine.SceneComponent.K2_AttachToComponent","/Script/Engine.StaticMeshComponent.SetStaticMesh",
    "/Script/Engine.SceneComponent.K2_SetRelativeTransform","/Script/Engine.PrimitiveComponent.GetCollisionEnabled",
    "/Script/Engine.PrimitiveComponent.GetMaterial","/Script/Engine.PrimitiveComponent.CreateDynamicMaterialInstance",
    "/Script/Engine.MaterialInstanceDynamic.SetScalarParameterValue","/Script/Engine.MaterialInstanceDynamic.K2_GetScalarParameterValue",
    "/Script/Engine.PrimitiveComponent.SetMaterial","/Script/Engine.ActorComponent.K2_DestroyComponent"};
constexpr std::string_view sphere_path="/Engine/BasicShapes/Sphere.Sphere";
constexpr std::array<std::array<float,3>,2> positions{{{117.f,9.f,48.f},{157.f,-11.f,63.f}}};
constexpr std::array scalar{.314159f,.728124f};
constexpr std::array roles{L"host",L"client",L"client2",L"client3"};
std::array<UE::WeakKey,count> symbols;
UE::WeakKey sphere{},source{},source_owner{},source_mesh{},stock_material{},dynamic_material{},remote{};
UE::NameKey dissolve{};
float stock_value{};
std::wstring root;
unsigned stage{};
unsigned build_stage{};
std::uint32_t source_id{};
ULONGLONG next{};
unsigned (*read_core)(char*,unsigned){};
int (*inspect_binding)(std::uint32_t,E::NativeBindingView*){};

Address fn(Symbol index){return ui.get(symbols[index]);}
std::wstring file(std::wstring_view leaf){return root+L"/"+std::wstring(leaf);}
bool seen(std::wstring_view leaf){return marker_exists(file(leaf).c_str());}
void mark(std::wstring_view leaf){marker(file(lan.role+L"-"+std::wstring(leaf)+L".txt").c_str());}
bool all(std::wstring_view leaf){return std::ranges::all_of(roles,[&](const auto role){return seen(std::wstring(role)+L"-"+std::wstring(leaf)+L".txt");});}
bool point(const std::array<float,3>& actual,const std::array<float,3>& expected){
    for(unsigned i=0;i<actual.size();++i)if(std::abs(actual[i]-expected[i])>.5f)return false;
    return true;
}

void scan(){
    const auto array=at<UE::ObjectArray>(image+TitleUi::object_array,0);
    if(!UE::object_array_shape(array))return;
    const bool binding=std::ranges::any_of(symbols,[](const auto key){return !ui.get(key);});
    const auto static_class=fn(static_type);
    remote={};
    for(int i=0;i<array.num_elements;++i){
        const auto chunk=at<Address>(array.chunks,(i/UE::objects_per_chunk)*8);
        if(!chunk)continue;
        const auto entry=at<UE::ObjectItem>(chunk,(i%UE::objects_per_chunk)*sizeof(UE::ObjectItem));
        if(!entry.object||(entry.flags&0x30000000u))continue;
        if(binding){
            const auto name=leaf(entry.object);
            for(unsigned j=0;j<paths.size();++j)
                if(!ui.get(symbols[j])&&name==paths[j].substr(paths[j].rfind('.')+1)&&path_is(entry.object,paths[j]))
                    ui.remember(symbols[j],entry.object);
        }
        if(at<unsigned>(entry.object,8)&0x10u)continue;
        if(!lan.host&&static_class&&at<Address>(entry.object,16)==static_class){
            const auto asset=ui.object(entry.object,"StaticMesh");
            const auto field=ui.member(entry.object,"RelativeLocation","StructProperty",12);
            if(asset&&field&&path_is(asset,sphere_path)&&ui.object(entry.object,"AttachParent")){
                const auto position=at<std::array<float,3>>(entry.object,at<unsigned>(field,76));
                if(point(position,positions[0])||point(position,positions[1]))ui.remember(remote,entry.object);
            }
        }
    }
    if(!inspect_binding)if(const auto core=GetModuleHandleW(L"QDCoopNativeM1.dll"))
        load_export(core,"QDEnemyInspectNativeBinding",inspect_binding);
    if(lan.host&&!ui.get(source_owner)){
        if(!inspect_binding)return;
        for(std::uint32_t id=1;id<=E::capacity;++id){
            E::NativeBindingView view{};
            if(!inspect_binding(id,&view)||!view.confirmed)continue;
            const auto actor=ui.get(view.actor);
            if(!actor||!ui.object(actor,"Mesh"))continue;
            const auto hidden=ui.member(actor,"bHidden","BoolProperty",1);
            if(!hidden||UE::get_bool(reinterpret_cast<void*>(hidden),reinterpret_cast<const void*>(actor),0))continue;
            source_owner=view.actor;
            source_id=id;
            break;
        }
    }
}

void report_binding(){
    if(!inspect_binding||!seen(L"host-mount-source.txt"))return;
    if(!source_id)if(const auto in=_wfopen(file(L"host-mount-source.txt").c_str(),L"rb")){
        unsigned id{};
        if(std::fscanf(in,"enrolled_enemy=%u",&id)==1&&id&&id<=E::capacity)source_id=id;
        std::fclose(in);
    }
    E::NativeBindingView view{};
    const bool bound=source_id&&inspect_binding(source_id,&view);
    if(auto out=_wfopen(file(lan.role+L"-mount-owner.txt").c_str(),L"wb")){
        std::fprintf(out,"id=%u bound=%u confirmed=%u actor=%u display=%u\n",source_id,bound,view.confirmed,
            ui.get(view.actor)!=0,ui.get(view.display)!=0);
        std::fclose(out);
    }
}

bool scalar_args(){
    if(!ui.method(fn(set_scalar),12,{{"ParameterName","NameProperty",0,8},{"Value","FloatProperty",8,4}})||
       !ui.method(fn(read_scalar),12,{{"ParameterName","NameProperty",0,8},{"ReturnValue","FloatProperty",8,4}}))return false;
    if(dissolve.index)return true;
    constexpr wchar_t text[]=L"Dissolve";
    struct Args {TitleUi::String input;UE::NameKey name{};};
    Args args{{text,9,9}};
    if(!ui.method(fn(make_name),24,{{"inString","StrProperty",0,16},{"ReturnValue","NameProperty",16,8}}))return false;
    TitleUi::event(fn(strings),fn(make_name),&args);
    dissolve=args.name;
    return dissolve.index!=0;
}
float read(Address mid){
    struct Args {UE::NameKey key;float value{};} args{dissolve};
    TitleUi::event(mid,fn(read_scalar),&args);
    return args.value;
}
void write(Address mid,float value){
    struct Args {UE::NameKey key;float value;} args{dissolve,value};
    TitleUi::event(mid,fn(set_scalar),&args);
}

bool move_source(Address component,unsigned phase){
    if(!ui.method(fn(relative),189,{{"NewTransform","StructProperty",0,48},{"bSweep","BoolProperty",48,1},
       {"SweepHitResult","StructProperty",52,136},{"bTeleport","BoolProperty",188,1}}))return false;
    alignas(16) std::array<unsigned char,192> args{};
    const auto& p=positions[phase];
    const std::array<float,12> pose{0,0,0,1,p[0],p[1],p[2],0,.2f,.2f,.2f,0};
    std::memcpy(args.data(),pose.data(),48);
    args[188]=1;
    TitleUi::event(component,fn(relative),args.data());
    return true;
}

bool create_source(){
    build_stage=1;
    const auto actor=ui.get(source_owner),mesh=ui.object(actor,"Mesh"),asset=ui.get(sphere);
    if(!actor||!mesh||!asset||!scalar_args())return false;
    const auto add_method=fn(add),attach_method=fn(attach);
    build_stage=2;
    const auto add_bytes=add_method?at<std::uint16_t>(add_method,182):0;
    if(add_bytes!=80||!ui.method(add_method,80,{{"Class","ClassProperty",0,8},{"bManualAttachment","BoolProperty",8,1},
       {"RelativeTransform","StructProperty",16,48},{"bDeferredFinish","BoolProperty",64,1},{"ReturnValue","ObjectProperty",72,8}})||
       !attach_method||at<std::uint16_t>(attach_method,182)!=21||!parameter(attach_method,"Parent",0,8)||
       !parameter(attach_method,"ReturnValue",20,1)||!ui.method(fn(set_mesh),9,{{"NewMesh","ObjectProperty",0,8},
       {"ReturnValue","BoolProperty",8,1}}))return false;
    alignas(16) std::array<unsigned char,80> args{};
    const auto component_type=fn(static_type);
    const std::array<float,12> identity{0,0,0,1,0,0,0,0,1,1,1,0};
    std::memcpy(args.data(),&component_type,8);
    args[8]=1;
    std::memcpy(args.data()+16,identity.data(),48);
    TitleUi::event(actor,add_method,args.data());
    const auto component=at<Address>(reinterpret_cast<Address>(args.data()),72);
    build_stage=3;
    if(!component)return false;
    ui.remember(source,component);
    ui.remember(source_mesh,mesh);
    args={};
    std::memcpy(args.data(),&mesh,8);
    TitleUi::event(component,attach_method,args.data());
    if(!args[20])return false;
    args={};
    std::memcpy(args.data(),&asset,8);
    TitleUi::event(component,fn(set_mesh),args.data());
    if(!args[8]||!move_source(component,0))return false;
    build_stage=4;
    if(!ui.method(fn(material),16,{{"ElementIndex","IntProperty",0,4},{"ReturnValue","ObjectProperty",8,8}})||
       !ui.method(fn(make_mid),32,{{"ElementIndex","IntProperty",0,4},{"SourceMaterial","ObjectProperty",8,8},
          {"OptionalName","NameProperty",16,8},{"ReturnValue","ObjectProperty",24,8}}))return false;
    Address slots[4]{};
    TitleUi::event(mesh,fn(material),slots);
    if(!slots[1])return false;
    ui.remember(stock_material,slots[1]);
    Address mid[4]{};
    TitleUi::event(mesh,fn(make_mid),mid);
    if(!mid[3])return false;
    build_stage=5;
    ui.remember(dynamic_material,mid[3]);
    stock_value=read(mid[3]);
    write(mid[3],scalar[0]);
    return std::abs(read(mid[3])-scalar[0])<.0001f;
}

bool cleanup_source(){
    const auto actor=ui.get(source_owner),component=ui.get(source),mesh=ui.get(source_mesh);
    const auto stock=ui.get(stock_material),mid=ui.get(dynamic_material);
    if(!actor||!component||!mesh||!stock||!mid||!fn(destroy)||at<std::uint16_t>(fn(destroy),182)!=8||
       !ui.method(fn(set_material),16,{{"ElementIndex","IntProperty",0,4},{"Material","ObjectProperty",8,8}}))return false;
    if(stock==mid)write(mid,stock_value);
    else {Address args[]{0,stock};TitleUi::event(mesh,fn(set_material),args);}
    auto caller=actor;
    TitleUi::event(component,fn(destroy),&caller);
    return true;
}

bool report(){
    if(!read_core)if(const auto core=GetModuleHandleW(L"QDCoopNativeM1.dll"))load_export(core,"QDEnemyNativeStatus",read_core);
    if(!read_core)return false;
    const auto size=read_core(nullptr,0);
    if(!size||size>32768)return false;
    std::string result(size,'\0');
    if(read_core(result.data(),size)!=size)return false;
    if(auto out=_wfopen(file(lan.role+L"-mount-core.json").c_str(),L"wb")){
        std::fputs(result.c_str(),out);
        std::fclose(out);
        return true;
    }
    return false;
}

bool step(Address,ULONGLONG now){
    if(root.empty()){
        wchar_t directory[32768]{};
        if(!environment_title(L"QD_PLACED_ITEM_TITLE_ROOT",directory,32768))return false;
        root=directory;
    }
    if(stage==3)return all(L"mount-clean");
    if(now<next)return false;
    next=now+250;
    scan();
    report_binding();
    if(!ui.get(sphere))if(const auto asset=ui.load_asset_path(sphere_path))ui.remember(sphere,asset);
    if(!lan.host&&stage<2&&scalar_args()){
        const auto parent=ui.object(ui.get(remote),"AttachParent");
        if(parent&&fn(material)&&ui.is(parent,at<Address>(fn(material),32))&&
           ui.method(fn(material),16,{{"ElementIndex","IntProperty",0,4},{"ReturnValue","ObjectProperty",8,8}})){
            Address slots[2]{};
            TitleUi::event(parent,fn(material),slots);
            const auto dynamic_type=at<Address>(fn(read_scalar),32);
            if(ui.is(slots[1],dynamic_type)&&std::abs(read(slots[1])-scalar[stage])<.0001f)
                mark(L"body-material-"+std::to_wstring(stage));
        }
    }
    if(lan.host){
        if(!stage){
            if(!create_source()){
                if(auto out=_wfopen(file(L"host-mount-binding.txt").c_str(),L"wb")){
                    std::fprintf(out,"stage=%u owner=%u asset=%u\n",build_stage,ui.get(source_owner)!=0,ui.get(sphere)!=0);
                    for(unsigned i=0;i<count;++i){
                        const auto method=ui.get(symbols[i]);
                        std::fprintf(out,"%u %u %u\n",i,method!=0,method?at<std::uint16_t>(method,182):0);
                    }
                    std::fclose(out);
                }
                report();
                return false;
            }
            if(auto out=_wfopen(file(L"host-mount-source.txt").c_str(),L"wb")){
                std::fprintf(out,"enrolled_enemy=%u\n",source_id);
                std::fclose(out);
            }
            stage=1;mark(L"mount-started");
        }else if(stage==1&&seen(L"client-mount-0.txt")&&seen(L"client2-mount-0.txt")&&seen(L"client3-mount-0.txt")){
            if(!move_source(ui.get(source),1))return false;
            write(ui.get(dynamic_material),scalar[1]);
            stage=2;mark(L"mount-moved");
        }else if(stage==2&&seen(L"client-mount-1.txt")&&seen(L"client2-mount-1.txt")&&seen(L"client3-mount-1.txt")){
            if(!cleanup_source()||!report())return false;
            stage=3;mark(L"mount-clean");
        }
    }else if(stage<2){
        const auto component=ui.get(remote),property=ui.member(component,"RelativeLocation","StructProperty",12);
        const auto phase=stage;
        if(property&&point(at<std::array<float,3>>(component,at<unsigned>(property,76)),positions[phase])&&
           fn(collision)&&at<std::uint16_t>(fn(collision),182)==1){
            unsigned char collision_mode=255;
            TitleUi::event(component,fn(collision),&collision_mode);
            if(!collision_mode){++stage;mark(L"mount-"+std::to_wstring(phase));}
        }
    }else if(seen(L"host-mount-clean.txt")&&!ui.get(remote)&&report()){
        stage=3;mark(L"mount-clean");
    }
    report();
    return stage==3&&all(L"mount-clean");
}
}
