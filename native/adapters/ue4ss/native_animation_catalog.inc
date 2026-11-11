// Native discovery is game-thread owned and uses only the pinned layout.
namespace AnimU=qdcoop::ue427;
static constexpr std::string_view catalog_method_paths[]{
    "/Script/Engine.SkeletalMeshComponent.GetAnimInstance",
    "/Script/Engine.AnimInstance.GetCurrentActiveMontage",
    "/Script/Engine.AnimInstance.Montage_Play",
    "/Script/Engine.AnimInstance.Montage_Stop",
    "/Script/Engine.AnimInstance.Montage_GetPosition",
    "/Script/Engine.AnimInstance.Montage_GetPlayRate",
    "/Script/Engine.AnimInstance.Montage_SetPlayRate",
    "/Script/Engine.AnimInstance.Montage_SetPosition",
    "/Script/Engine.AnimInstance.Montage_IsPlaying",
    "/Script/Engine.SkeletalMeshComponent.SetAnimClass",
    "/Script/Engine.SkeletalMeshComponent.SetAnimInstanceClass"
};
static_assert(std::size(catalog_method_paths)==anim_functions.size()+2);
static_assert(std::size(A::catalog_profiles)==std::size(A::layouts));
static_assert(std::size(A::catalog_montages)==std::size(A::montages));
static_assert(std::size(A::catalog_sequences)==std::size(A::sequences));
enum CatalogType { catalog_function,catalog_montage,catalog_sequence,catalog_blueprint,catalog_skeletal_mesh };
static constexpr std::string_view catalog_types[]{
    "/Script/CoreUObject.Function", "/Script/Engine.AnimMontage", "/Script/Engine.AnimSequence",
    "/Script/Engine.BlueprintGeneratedClass", "/Script/Engine.SkeletalMesh"
};
static void cache_native_enemy_asset(void* object,bool mesh);
static bool catalog_subclass(void* type,void* base){
    for(unsigned depth=0;type&&depth<256;++depth){
        if(type==base)return true;
        type=AnimU::field<void*>(type,64);
    }
    return false;
}
static std::array<Weak,std::size(catalog_method_paths)> catalog_methods{};
static std::array<Weak,std::size(catalog_types)> catalog_classes{};
static std::array<Weak,std::size(A::catalog_skeletons)> catalog_skeletons{};
static std::array<Weak,std::size(A::catalog_profiles)> catalog_profile_types{};
static std::array<std::unordered_map<void*,unsigned>,2> catalog_asset_ids;
static unsigned catalog_scan_cursor;
static std::uint64_t catalog_slots,catalog_passes,catalog_binds;
static bool catalog_ready;
static std::uintptr_t catalog_name_pool;

static bool catalog_read(std::uintptr_t address,void* out,std::size_t size){
    std::memcpy(out,reinterpret_cast<const void*>(address),size);
    return true;
}

static bool catalog_path(const void* object,std::string& path){
    return object&&AnimU::object_path(reinterpret_cast<std::uintptr_t>(object),catalog_name_pool,
                                      catalog_read,path);
}

static std::string_view catalog_leaf(std::string_view path){
    return path.substr(path.find_last_of('.')+1);
}

static void* catalog_field(void* type,std::string_view name,int bytes){
    if(!type)return nullptr;
    const auto owner_size=*api.propertiesSize(type);
    if(owner_size<=0||owner_size>1048576)return nullptr;
    auto field=reinterpret_cast<void*>(AnimU::find_inherited_property(
        reinterpret_cast<std::uintptr_t>(type),catalog_name_pool,name,catalog_read));
    return property(field,bytes,owner_size)?field:nullptr;
}

static void* catalog_object_field(void* object,std::string_view name){
    if(!object)return nullptr;
    auto field=catalog_field(AnimU::field<void*>(object,16),name,8);
    return field?AnimU::field<void*>(object,*api.offset(field)):nullptr;
}

static bool catalog_skeleton_matches(void* skeleton,unsigned id){
    if(!skeleton||!id||id>=catalog_skeletons.size())return false;
    auto& cached=catalog_skeletons[id];
    if(api.weakget(&cached)==skeleton)return true;
    std::string path;
    return catalog_path(skeleton,path)&&path==A::catalog_skeletons[id]&&makeweak(cached,skeleton);
}

template<std::size_t N,std::size_t M>
static unsigned catalog_find_asset(std::string_view path,const std::string_view (&paths)[N],
                                   const std::uint16_t (&ordered)[M]){
    static_assert(N==M+1);
    const auto match=std::lower_bound(std::begin(ordered),std::end(ordered),path,
        [&](unsigned id,std::string_view wanted){return paths[id]<wanted;});
    return match!=std::end(ordered)&&paths[*match]==path?*match:0;
}

struct CatalogAssetLayout { Weak type{}; unsigned skeleton{},length{}; };
static std::array<CatalogAssetLayout,2> catalog_asset_layouts{};

static bool catalog_class_matches(unsigned kind,void* type){
    auto& key=catalog_classes[kind];
    if(auto cached=api.weakget(&key))return cached==type;
    std::string path;
    return catalog_path(type,path)&&path==catalog_types[kind]&&makeweak(key,type);
}

static unsigned catalog_register_asset(unsigned kind,void* asset){
    auto type=AnimU::field<void*>(asset,16);
    if(!catalog_class_matches(kind+1,type))return UINT32_MAX;
    std::string path;
    if(!catalog_path(asset,path))return UINT32_MAX;
    const auto id=kind?catalog_find_asset(path,A::catalog_sequences,A::catalog_sequences_by_path):
                       catalog_find_asset(path,A::catalog_montages,A::catalog_montages_by_path);
    if(!id)return UINT32_MAX;
    auto& layout=catalog_asset_layouts[kind];
    if(api.weakget(&layout.type)!=type){
        const auto skeleton=catalog_field(type,"Skeleton",8),length=catalog_field(type,"SequenceLength",4);
        if(!skeleton||!length||!makeweak(layout.type,type))return UINT32_MAX;
        layout.skeleton=static_cast<unsigned>(*api.offset(skeleton));
        layout.length=static_cast<unsigned>(*api.offset(length));
    }
    const auto expected=kind?A::sequences[id]:A::montages[id];
    if(!catalog_skeleton_matches(AnimU::field<void*>(asset,layout.skeleton),expected))return UINT32_MAX;
    auto& entry=kind?anim_sequences[id]:anim_montages[id];
    auto& index=catalog_asset_ids[kind];
    const auto previous=entry.address;
    if(!register_anim_asset(kind,id,asset,AnimU::field<float>(asset,layout.length)))return UINT32_MAX;
    entry.catalog_checked=true;
    if(previous&&previous!=asset)index.erase(previous);
    index[asset]=id;
    return id;
}

static unsigned native_catalog_asset_id(unsigned kind,void* asset){
    if(!asset)return 0;
    auto& index=catalog_asset_ids[kind];
    const auto match=index.find(asset);
    if(match!=index.end()){
        const auto id=match->second;
        const auto& entry=kind?anim_sequences[id]:anim_montages[id];
        if(entry.catalog_checked&&api.weakget(&entry.weak)==asset)return id;
        index.erase(match);
    }
    return catalog_register_asset(kind,asset);
}

static void catalog_find_method(void* object){
    std::string leaf;
    if(!AnimU::name_string(catalog_name_pool,AnimU::field<AnimU::NameKey>(object,24),catalog_read,leaf))
        return;
    for(unsigned i=0;i<catalog_methods.size();++i){
        if(leaf!=catalog_leaf(catalog_method_paths[i]))continue;
        std::string path;
        if(catalog_path(object,path)&&path==catalog_method_paths[i])makeweak(catalog_methods[i],object);
    }
}

static int scan_native_animation(unsigned budget){
    AnimU::ObjectArray array{};
    catalog_read(native_object_array,&array,sizeof(array));
    if(!AnimU::object_array_shape(array))return -1;

    std::array<void*,std::size(catalog_types)> classes{};
    for(unsigned i=0;i<classes.size();++i)classes[i]=api.weakget(&catalog_classes[i]);
    const auto total=static_cast<unsigned>(array.num_elements);
    if(catalog_scan_cursor>total)catalog_scan_cursor=0;
    const auto end=std::min(total,catalog_scan_cursor+budget);
    catalog_slots+=end-catalog_scan_cursor;
    for(;catalog_scan_cursor<end;++catalog_scan_cursor){
        const auto chunk=reinterpret_cast<const std::uintptr_t*>(array.chunks)
            [catalog_scan_cursor/AnimU::objects_per_chunk];
        if(!chunk)continue;
        const auto item=reinterpret_cast<const AnimU::ObjectItem*>(chunk)
            [catalog_scan_cursor%AnimU::objects_per_chunk];
        if(!item.object||(item.flags&0x30000000u))continue;
        auto object=reinterpret_cast<void*>(item.object);
        auto type=AnimU::field<void*>(object,16);
        if(!type||(AnimU::field<unsigned>(object,8)&0x10u))continue;
        if(std::find(classes.begin(),classes.end(),nullptr)!=classes.end()){
            std::string leaf;
            if(AnimU::name_string(catalog_name_pool,AnimU::field<AnimU::NameKey>(type,24),catalog_read,leaf)){
                for(unsigned i=0;i<classes.size();++i){
                    if(classes[i]||leaf!=catalog_leaf(catalog_types[i]))continue;
                    if(catalog_class_matches(i,type))classes[i]=type;
                }
            }
        }
        if(type==classes[catalog_function])catalog_find_method(object);
        else if(type==classes[catalog_montage])native_catalog_asset_id(0,object);
        else if(type==classes[catalog_sequence])native_catalog_asset_id(1,object);
        else if(type==classes[catalog_blueprint]&&catalog_subclass(object,base_class))
            cache_native_enemy_asset(object,false);
        else if(type==classes[catalog_skeletal_mesh])cache_native_enemy_asset(object,true);
    }
    if(catalog_scan_cursor<total)return 0;
    catalog_scan_cursor=0;
    ++catalog_passes;

    std::array<std::uint64_t,anim_functions.size()> functions{};
    for(unsigned i=0;i<functions.size();++i)
        functions[i]=reinterpret_cast<std::uintptr_t>(api.weakget(&catalog_methods[i]));
    const auto set_class=api.weakget(&catalog_methods[anim_functions.size()]);
    const auto set_instance=api.weakget(&catalog_methods[anim_functions.size()+1]);
    catalog_ready=((set_class&&*api.parms(set_class)==8)||(set_instance&&*api.parms(set_instance)==8))&&
        configure_anim_functions(functions.data(),static_cast<unsigned>(functions.size()));
    return catalog_ready?1:-1;
}

static unsigned catalog_configure_profile(void* type){
    for(unsigned i=1;i<catalog_profile_types.size();++i)
        if(api.weakget(&catalog_profile_types[i])==type&&api.weakget(&anim_profiles[i].type)==type)
            return i;
    std::string path;
    if(!catalog_path(type,path))return 0;
    unsigned profile=1;
    for(;profile<std::size(A::catalog_profiles);++profile)
        if(A::catalog_profiles[profile].path==path)break;
    if(profile==std::size(A::catalog_profiles))return 0;
    constexpr std::string_view common[]{
        "bIsMoving","bIsChasing","bIsFalling","bIsAggro","bIsWandering","CurrentAimAngle"
    };
    std::array<std::uint64_t,14> fields{};
    unsigned count=0;
    for(const auto name:common)
        fields[count++]=reinterpret_cast<std::uintptr_t>(catalog_field(type,name,name=="CurrentAimAngle"?4:1));
    const auto& layout=A::layouts[profile];
    for(unsigned i=0;i<layout.count;){
        const auto kind=layout.kinds[i];
        const auto width=anim_field_width(kind);
        const auto name=A::catalog_profiles[profile].fields[count-std::size(common)];
        const auto bytes=width==3?12:kind=='b'?1:kind=='a'?8:4;
        fields[count++]=reinterpret_cast<std::uintptr_t>(catalog_field(type,name,bytes));
        i+=width;
    }
    return configure_anim_profile(profile,type,fields.data(),count)&&makeweak(catalog_profile_types[profile],type)?
        profile:0;
}

static unsigned catalog_mesh_profile(void* source,void*& type,void*& skeleton){
    const auto get_instance=anim_function(0);
    if(!get_instance||!source||!api.isa(source,mesh_class))return 0;
    void* instance{};
    api.event(source,get_instance,&instance);
    if(!instance)return 0;
    type=AnimU::field<void*>(instance,16);
    if(!type)return 0;
    const auto profile=catalog_configure_profile(type);
    if(!profile)return 0;
    skeleton=catalog_object_field(catalog_object_field(source,"SkeletalMesh"),"Skeleton");
    return catalog_skeleton_matches(skeleton,A::layouts[profile].skeleton)?profile:0;
}

static unsigned prepare_catalog_animation(Entity& entity,void* actor){
    const auto source=api.weakget(&entity.mesh);
    if(!source||AnimU::field<void*>(actor,mesh_at)!=source)return 0;
    void* type{};
    void* source_skeleton{};
    const auto profile=catalog_mesh_profile(source,type,source_skeleton);
    if(!profile)return 0;
    if(!entity.split)return profile;

    const auto pose=api.weakget(&entity.visual_mesh);
    const auto visual=api.weakget(&entity.visual);
    const auto owner_fn=api.weakget(&entity.owner_fn);
    if(!pose||!visual||!owner_fn||pose==source||
       catalog_object_field(catalog_object_field(pose,"SkeletalMesh"),"Skeleton")!=source_skeleton)
        return 0;
    void* owner{};
    api.event(pose,owner_fn,&owner);
    if(owner!=visual)return 0;
    const auto tick_option=catalog_field(AnimU::field<void*>(pose,16),"VisibilityBasedAnimTickOption",1);
    auto set_class=api.weakget(&catalog_methods[anim_functions.size()]);
    if(!set_class||*api.parms(set_class)!=8)
        set_class=api.weakget(&catalog_methods[anim_functions.size()+1]);
    if(!tick_option||!set_class||*api.parms(set_class)!=8)return 0;
    const auto get_instance=anim_function(0);
    if(!get_instance)return 0;
    void* target{};
    api.event(pose,get_instance,&target);
    if(!target||AnimU::field<void*>(target,16)!=type)api.event(pose,set_class,&type);
    static_cast<unsigned char*>(pose)[*api.offset(tick_option)]=0; // AlwaysTickPoseAndRefreshBones.
    return profile;
}
