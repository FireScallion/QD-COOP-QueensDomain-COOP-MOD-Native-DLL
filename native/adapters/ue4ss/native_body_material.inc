namespace BodyMaterial=qdcoop::body_material;
struct BodyScalarLayout {
    int values{-1},size{},info{},name{},association{},index{},value{};
};
struct NativeBodySource {
    BodyMaterial::Frame frame;
    double next_send{},reset_until{},next_materials{};
};
struct BodyMaterialSlot {
    Weak original,current;
    float scalar{};
    bool was_dynamic{};
};
struct NativeBodyDisplay {
    BodyMaterial::Frame frame;
    Weak mesh;
    std::vector<BodyMaterialSlot> slots;
    std::uint64_t applied{};
    double expires{};
};
static BodyScalarLayout body_scalar_layout;
static std::array<NativeBodySource,E::capacity> body_sources;
static std::array<NativeBodyDisplay,E::capacity> body_displays;
static GearAsset body_assets[std::size(qdcoop::visual_mount::assets::material_paths)];
static AnimU::NameKey body_dissolve;
static double body_material_next,body_material_bind_next;
static unsigned body_material_cursor;

static bool body_material_ready(double time){
    if(body_dissolve.index)return true;
    if(time<body_material_bind_next)return false;
    body_material_bind_next=time+.5;
    body_material_stage=1;
    if(!world_input(world_body_scalar))scan_world_inputs(2048);
    if(!medic_params(world_body_scalar,12,{{"ParameterName",8,0},{"ParameterValue",4,8}})||
       !medic_params(world_get_material,16,{{"ElementIndex",4,0},{"ReturnValue",8,8}})||
       !medic_params(world_set_material,16,{{"ElementIndex",4,0},{"Material",8,8}})||
       !medic_params(world_material_count,4,{{"ReturnValue",4,0}})||
       !medic_params(world_view_scalar,12,{{"ParameterName",8,0},{"Value",4,8}})||
       !medic_params(world_view_read_scalar,12,{{"ParameterName",8,0},{"ReturnValue",4,8}}))return false;
    const auto key=medic_name(L"Dissolve");
    if(!medic_name_is(key,L"Dissolve"))return false;
    body_dissolve=key;
    body_material_stage=2;
    return true;
}

static void* body_material_at(void* mesh,int slot){
    struct Args {int slot,padding{};void* material{};} args{slot};
    api.event(mesh,world_input(world_get_material),&args);
    return args.material;
}
static bool body_material_dynamic(void* material){
    const auto scalar=world_input(world_view_scalar);
    return material&&scalar&&api.isa(material,AnimU::field<void*>(scalar,32));
}
static std::uint16_t stock_visual_material_id(void* material){
    const auto& paths=qdcoop::visual_mount::assets::material_paths;
    for(unsigned depth=0;material&&depth<5;++depth){
        std::string path;
        if(catalog_path(material,path)){
            const auto found=std::ranges::lower_bound(paths,path);
            if(found!=std::end(paths)&&*found==path)return static_cast<std::uint16_t>(found-std::begin(paths));
        }
        material=catalog_object_field(material,"Parent");
    }
    return 0;
}
static float body_material_scalar(void* material){
    alignas(8) FactoryParameters args{};
    factory_put(args,0,body_dissolve);
    api.event(material,world_input(world_view_read_scalar),args.data());
    return AnimU::field<float>(args.data(),8);
}
static void body_material_write(void* target,unsigned symbol,float value){
    alignas(8) FactoryParameters args{};
    factory_put(args,0,body_dissolve);
    factory_put(args,8,value);
    api.event(target,world_input(symbol),args.data());
}

static bool body_material_layout(void* material){
    if(body_scalar_layout.values>=0)return true;
    const auto values=catalog_field(AnimU::field<void*>(material,16),"ScalarParameterValues",16);
    if(!medic_field_class(values,"ArrayProperty"))return false;
    const auto element=AnimU::field<void*>(values,0x78);
    if(!medic_field_class(element,"StructProperty")||!damage_type_path(element,"/Script/Engine.ScalarParameterValue"))return false;
    const auto type=AnimU::field<void*>(element,0x78);
    const auto info=catalog_field(type,"ParameterInfo",16),value=catalog_field(type,"ParameterValue",4);
    if(!medic_field_class(info,"StructProperty")||!damage_type_path(info,"/Script/Engine.MaterialParameterInfo")||
       !medic_field_class(value,"FloatProperty"))return false;
    const auto info_type=AnimU::field<void*>(info,0x78);
    const auto name=catalog_field(info_type,"Name",8),association=catalog_field(info_type,"Association",1);
    const auto index=catalog_field(info_type,"Index",4);
    if(!medic_field_class(name,"NameProperty")||!medic_field_class(index,"IntProperty")||
       (!medic_field_class(association,"ByteProperty")&&!medic_field_class(association,"EnumProperty")))return false;
    body_scalar_layout={*api.offset(values),*api.size(element),*api.offset(info),*api.offset(name),
        *api.offset(association),*api.offset(index),*api.offset(value)};
    return true;
}

static std::optional<float> body_material_override(void* material){
    if(!body_material_dynamic(material)||!body_material_layout(material))return std::nullopt;
    const auto& layout=body_scalar_layout;
    const auto values=AnimU::field<WorldArray>(material,layout.values);
    if(!values.valid(128))return std::nullopt;
    const auto data=static_cast<const unsigned char*>(values.data);
    for(int i=0;i<values.count;++i){
        const auto entry=data+i*layout.size,info=entry+layout.info;
        const auto key=AnimU::field<AnimU::NameKey>(info,layout.name);
        if(key.index==body_dissolve.index&&key.number==body_dissolve.number&&
           AnimU::field<unsigned char>(info,layout.association)==2&&AnimU::field<int>(info,layout.index)==-1){
            const auto value=AnimU::field<float>(entry,layout.value);
            if(std::isfinite(value)&&std::abs(value)<=10000.f)return value;
        }
    }
    return std::nullopt;
}

static void body_material_restore(NativeBodyDisplay& display){
    const auto mesh=api.weakget(&display.mesh);
    if(mesh&&world_input(world_get_material)&&world_input(world_set_material)&&world_input(world_view_scalar)){
        for(unsigned i=0;i<display.slots.size();++i){
            const auto& slot=display.slots[i];
            const auto current=api.weakget(&slot.current),original=api.weakget(&slot.original);
            if(!current||body_material_at(mesh,static_cast<int>(i))!=current)continue;
            if(current!=original)gear_set_material(mesh,static_cast<int>(i),original);
            else if(slot.was_dynamic&&std::isfinite(slot.scalar))body_material_write(current,world_view_scalar,slot.scalar);
        }
    }
    display.mesh={};
    display.slots.clear();
    display.applied=0;
}

static bool body_material_apply(NativeBodyDisplay& display,void* mesh,double time){
    if(api.weakget(&display.mesh)!=mesh){
        body_material_restore(display);
        int count{};
        api.event(mesh,world_input(world_material_count),&count);
        if(count<1||count>32||!makeweak(display.mesh,mesh))return false;
        display.slots.resize(static_cast<unsigned>(count));
    }
    std::array<void*,BodyMaterial::material_slots> desired{};
    for(unsigned i=0;i<desired.size()&&i<display.slots.size();++i){
        const auto id=display.frame.materials[i];
        if(!id)continue;
        const auto path=qdcoop::visual_mount::assets::material_paths[id];
        desired[i]=gear_load(body_assets[id],path,time,UINT32_MAX,world_input(world_material_type));
        if(!desired[i])return false;
    }
    for(unsigned i=0;i<display.slots.size();++i){
        auto& slot=display.slots[i];
        const auto original=body_material_at(mesh,static_cast<int>(i));
        if(original&&api.weakget(&slot.current)!=original){
            slot={};
            if(!makeweak(slot.original,original))return false;
            slot.was_dynamic=body_material_dynamic(original);
            if(slot.was_dynamic)slot.scalar=body_material_scalar(original);
        }
        if(i<desired.size()&&desired[i]&&!slot.was_dynamic&&desired[i]!=original&&
           stock_visual_material_id(original)!=display.frame.materials[i])
            gear_set_material(mesh,static_cast<int>(i),desired[i]);
    }
    body_material_write(mesh,world_body_scalar,display.frame.dissolve);
    for(unsigned i=0;i<display.slots.size();++i)
        if(const auto current=body_material_at(mesh,static_cast<int>(i)))makeweak(display.slots[i].current,current);
    display.applied=display.frame.sequence;
    ++body_material_applied;
    return true;
}

static bool receive_body_material(const ReceivedPacket& received,unsigned kind,double time){
    if(kind!=BodyMaterial::kind)return false;
    BodyMaterial::Frame frame;
    if(host_role||received.peer||!active_peer_mask||!BodyMaterial::decode(envelope,received.packet,frame))return true;
    auto& display=body_displays[frame.id-1];
    const auto& current=display.frame;
    if(frame.generation<current.generation||(frame.generation==current.generation&&frame.sequence<current.sequence)||
       (frame.generation==current.generation&&frame.sequence==current.sequence&&frame!=current))return true;
    if(frame.generation!=current.generation)body_material_restore(display);
    display.frame=frame;
    display.expires=time+3.;
    ++body_material_received;
    return true;
}

static void body_material_publish(unsigned index,double time){
    const auto& entity=entities[index];
    auto& source=body_sources[index];
    if(entity.generation!=source.frame.generation)source={};
    const auto mesh=entity.used?api.weakget(&entity.mesh):nullptr;
    const auto value=mesh?body_material_override(body_material_at(mesh,0)):std::optional<float>{};
    auto materials=source.frame.materials;
    if(!value)materials={};
    else if(!source.frame.active||time>=source.next_materials){
        source.next_materials=time+.35;
        materials={};
        const auto count=std::clamp(gear_material_count(mesh),0,static_cast<int>(materials.size()));
        for(int i=0;i<count;++i)materials[i]=stock_visual_material_id(body_material_at(mesh,i));
    }
    if(value.has_value()!=source.frame.active||(value&&*value!=source.frame.dissolve)||materials!=source.frame.materials){
        source.frame={index+1,entity.generation,source.frame.sequence+1,value.value_or(0.f),value.has_value(),materials};
        source.next_send=time;
        source.reset_until=time+2.;
    }
    const auto mask=enrollment_peer_mask(index);
    if(!source.frame.sequence||!mask||time<source.next_send||(!source.frame.active&&time>=source.reset_until))return;
    E::Packet packet{};
    if(BodyMaterial::encode(envelope,source.frame,packet)&&send(packet,mask)){
        source.next_send=time+(source.frame.active?.55:.25);
        ++body_material_sent;
    }
}

static void native_body_material_step(double time){
    if(!active_peer_mask||time<body_material_next||!body_material_ready(time))return;
    body_material_next=time+.045;
    if(host_role){
        unsigned budget=8;
        for(unsigned checked=0;checked<E::capacity&&budget;++checked){
            const auto index=body_material_cursor++%E::capacity;
            if(!entities[index].used&&!body_sources[index].frame.sequence)continue;
            body_material_publish(index,time);
            --budget;
        }
        return;
    }
    for(unsigned index=0;index<E::capacity;++index){
        auto& display=body_displays[index];
        if(!display.frame.sequence)continue;
        body_material_stage=3;
        const auto& entity=entities[index];
        if(!display.frame.active||time>=display.expires||!entity.used||entity.generation!=display.frame.generation){
            if(display.applied)body_material_restore(display);
            continue;
        }
        const auto mesh=api.weakget(deaths[index].terminal?&entity.mesh:&entity.visual_mesh);
        if(mesh){
            body_material_stage=4;
            if(display.applied!=display.frame.sequence||api.weakget(&display.mesh)!=mesh)body_material_apply(display,mesh,time);
        }
    }
}

static void native_body_material_cleanup(){
    for(auto& display:body_displays){
        if(display.applied)body_material_restore(display);
        display={};
    }
    body_sources={};
    body_scalar_layout={};
    body_dissolve={};
    body_material_next=body_material_bind_next=0;
    body_material_cursor=0;
}
