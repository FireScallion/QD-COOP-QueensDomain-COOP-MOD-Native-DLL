struct PlacedItemModel {
    void* component{};
    void* mesh{};
    bool skeletal{};
};

static void* placed_object_field(void* object,std::string_view name,std::string_view kind="ObjectProperty"){
    if(!object)return nullptr;
    const auto property=catalog_field(AnimU::field<void*>(object,16),name,8);
    return medic_field_class(property,kind)?AnimU::field<void*>(object,*api.offset(property)):nullptr;
}

static bool placed_array(void* object,std::string_view name,WorldArray& values,void*& element){
    if(!object)return false;
    const auto field=catalog_field(AnimU::field<void*>(object,16),name,16);
    if(!medic_field_class(field,"ArrayProperty"))return false;
    element=AnimU::field<void*>(field,0x78);
    values=AnimU::field<WorldArray>(object,*api.offset(field));
    return element&&values.valid(128);
}

static PlacedItemModel placed_model_candidate(void* component){
    if(!component)return {};
    const auto static_type=world_input(world_static_component),skeletal_type=world_input(world_pose_component);
    if(static_type&&api.isa(component,static_type)){
        const auto mesh=placed_object_field(component,"StaticMesh"),type=world_input(world_static_asset);
        if(mesh&&type&&api.isa(mesh,type))return {component,mesh,false};
    }else if(skeletal_type&&api.isa(component,skeletal_type)){
        const auto mesh=placed_object_field(component,"SkeletalMesh");
        if(mesh&&catalog_class_matches(catalog_skeletal_mesh,AnimU::field<void*>(mesh,16)))return {component,mesh,true};
    }
    return {};
}

static bool placed_preferred_model(void* component,PlacedItemModel& model){
    const auto candidate=placed_model_candidate(component);
    if(!candidate.component)return false;
    std::string name;
    const bool preferred=AnimU::name_string(catalog_name_pool,AnimU::field<AnimU::NameKey>(component,24),catalog_read,name)&&
        name.find("PickupGraphics")!=std::string::npos;
    if(preferred||!model.component)model=candidate;
    return preferred;
}

static PlacedItemModel placed_class_model(void* type){
    if(!type||!catalog_class_matches(catalog_blueprint,AnimU::field<void*>(type,16))||
       !catalog_subclass(type,actor_class))return {};
    PlacedItemModel model;
    WorldArray values;
    void* inner{};
    const auto handler=placed_object_field(type,"InheritableComponentHandler");
    if(placed_array(handler,"Records",values,inner)&&medic_field_class(inner,"StructProperty")){
        const auto entry=AnimU::field<void*>(inner,0x78);
        const auto field=catalog_field(entry,"ComponentTemplate",8);
        const auto stride=*api.size(inner);
        std::string path;
        if(stride>=8&&stride<=512&&medic_field_class(field,"ObjectProperty")&&
           *api.offset(field)<=stride-8&&catalog_path(entry,path)&&path=="/Script/Engine.ComponentOverrideRecord"){
            for(int i=0;i<values.count;++i){
                const auto component=AnimU::field<void*>(values.data,i*stride+*api.offset(field));
                if(placed_preferred_model(component,model))return model;
            }
        }
    }
    const auto scs=placed_object_field(type,"SimpleConstructionScript");
    if(placed_array(scs,"AllNodes",values,inner)&&medic_field_class(inner,"ObjectProperty")&&*api.size(inner)==8){
        const auto nodes=static_cast<void* const*>(values.data);
        for(int i=0;i<values.count;++i)
            if(placed_preferred_model(placed_object_field(nodes[i],"ComponentTemplate"),model))return model;
    }
    return model;
}

static PlacedItemModel placed_item_model(void* item){
    for(auto name:{"Drop BP / Fallback Visual BP","Pickup BP"})
        if(const auto model=placed_class_model(placed_object_field(item,name,"ClassProperty"));model.component)return model;
    const auto property=catalog_field(AnimU::field<void*>(item,16),"Default Visual BP",40);
    const auto kismet=world_input(world_kismet),load=world_input(world_load_class);
    if(!medic_field_class(property,"SoftClassProperty")||!kismet||
       !medic_params(world_load_class,48,{{"AssetClass",40,0},{"ReturnValue",8,40}}))return {};
    alignas(16) FactoryParameters args{};
    std::memcpy(args.data(),static_cast<unsigned char*>(item)+*api.offset(property),40);
    api.event(kismet,load,args.data());
    return placed_class_model(AnimU::field<void*>(args.data(),40));
}
