// Pinned Win64 ABI. The generated GetAllActorsOfClass thunk calls the
// implementation with (WorldContextObject, ActorClass, OutActors).
namespace C=qdcoop::enemy::catalog;
struct NativeActorArray {
    void** data{};
    std::int32_t count{},capacity{};
};
static_assert(sizeof(NativeActorArray)==16);
using NativeGetActors=void(*)(void*,void*,NativeActorArray*);
using NativeFree=void(*)(void*);
static NativeGetActors native_get_actors;
static NativeFree native_free;
static Weak native_enum_method{};
static Weak native_enum_context{};
static std::array<AnimU::WeakKey,E::capacity> native_enum_candidates{};
static unsigned native_enum_count;
static std::uint64_t native_enum_world;
static std::uint64_t enum_calls,enum_seen,enum_in_range,enum_peer_range,enum_frees;
static unsigned native_enrollment_origins(void* context,std::array<std::array<float,3>,qdcoop::player::slots>& origins);
static bool native_encounter_managed(unsigned class_id,void* actor);

static bool configure_native_enumerator(void* function){
    std::string path;
    if(!catalog_path(function,path)||path!="/Script/Engine.GameplayStatics.GetAllActorsOfClass"||
       *api.parms(function)!=32)return false;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto thunk=image+0x2DBE520,implementation=image+0x28DDA50,free_entry=image+0xEAE0B0;
    constexpr unsigned char thunk_entry[]{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x30};
    constexpr unsigned char implementation_entry[]{0x40,0x55,0x53,0x56,0x57,0x48,0x8d,0x6c,0x24,0xc1};
    constexpr unsigned char free_prologue[]{0x48,0x85,0xc9,0x74,0x2e,0x53,0x48,0x83,0xec,0x20};
    if(AnimU::field<std::uintptr_t>(function,216)!=thunk||
       std::memcmp(reinterpret_cast<const void*>(thunk),thunk_entry,sizeof(thunk_entry))||
       std::memcmp(reinterpret_cast<const void*>(implementation),implementation_entry,sizeof(implementation_entry))||
       std::memcmp(reinterpret_cast<const void*>(free_entry),free_prologue,sizeof(free_prologue)))return false;
    constexpr std::string_view names[]{"WorldContextObject","ActorClass","OutActors"};
    constexpr int widths[]{8,8,16};
    for(unsigned i=0;i<std::size(names);++i){
        const auto field=reinterpret_cast<void*>(AnimU::find_property(
            reinterpret_cast<std::uintptr_t>(function),catalog_name_pool,names[i],catalog_read));
        if(!property(field,widths[i],32)||*api.offset(field)!=static_cast<int>(8*i)||
           !(AnimU::field<std::uint64_t>(field,64)&0x80u))return false;
    }
    if(!makeweak(native_enum_method,function))return false;
    native_get_actors=reinterpret_cast<NativeGetActors>(implementation);
    native_free=reinterpret_cast<NativeFree>(free_entry);
    return true;
}

template<std::size_t N>
struct EnemyAssetCache {
    std::array<Weak,N> keys{};
    std::array<void*,N> addresses{};
    std::unordered_map<void*,std::uint16_t> ids;
};
template<std::size_t N>
static std::uint16_t enemy_catalog_id(void* object,const std::string_view (&paths)[N],
                                      EnemyAssetCache<N>& cache){
    if(!object)return 0;
    if(const auto found=cache.ids.find(object);found!=cache.ids.end()){
        if(api.weakget(&cache.keys[found->second])==object)return found->second;
        cache.ids.erase(found);
    }
    std::string path;
    if(!catalog_path(object,path))return 0;
    const auto match=std::find(std::begin(paths)+1,std::end(paths),path);
    if(match==std::end(paths))return 0;
    const auto id=static_cast<std::uint16_t>(match-std::begin(paths));
    if(!makeweak(cache.keys[id],object))return 0;
    if(auto previous=cache.addresses[id];previous&&previous!=object){
        const auto old=cache.ids.find(previous);
        if(old!=cache.ids.end()&&old->second==id)cache.ids.erase(old);
    }
    cache.addresses[id]=object;
    cache.ids[object]=id;
    return id;
}
static EnemyAssetCache<std::size(C::classes)> enemy_classes;
static EnemyAssetCache<std::size(C::meshes)> enemy_meshes;
static void cache_native_enemy_asset(void* object,bool mesh){
    if(mesh)enemy_catalog_id(object,C::meshes,enemy_meshes);
    else enemy_catalog_id(object,C::classes,enemy_classes);
}
static Weak enemy_mesh_layout_type;
static unsigned enemy_mesh_asset_offset;

static void* native_skeletal_asset(void* mesh){
    if(api.weakget(&enemy_mesh_layout_type)!=mesh_class){
        const auto field=catalog_field(mesh_class,"SkeletalMesh",8);
        if(!field||!makeweak(enemy_mesh_layout_type,mesh_class))return nullptr;
        enemy_mesh_asset_offset=static_cast<unsigned>(*api.offset(field));
    }
    return AnimU::field<void*>(mesh,enemy_mesh_asset_offset);
}

static bool identify_native_enemy(void* actor,void*& mesh,std::uint16_t& class_id,std::uint16_t& mesh_id){
    if(!actor||!api.isa(actor,base_class)||(AnimU::field<unsigned>(actor,8)&0x10u))return false;
    mesh=AnimU::field<void*>(actor,mesh_at);
    if(!mesh||!api.isa(mesh,mesh_class))return false;
    class_id=enemy_catalog_id(AnimU::field<void*>(actor,16),C::classes,enemy_classes);
    mesh_id=enemy_catalog_id(native_skeletal_asset(mesh),C::meshes,enemy_meshes);
    return class_id&&mesh_id;
}

static std::array<float,3> native_actor_location(void* actor){
    alignas(16) unsigned char parameters[256]{};
    api.event(actor,getters[0].fn,parameters);
    std::array<float,3> result{};
    std::memcpy(result.data(),parameters+getters[0].at,sizeof(result));
    return result;
}

static bool native_enemy_discoverable(void* actor,unsigned class_id){
    const auto hp=AnimU::field<float>(actor,hp_at);
    if(!std::isfinite(hp)||(hp<=0&&!native_encounter_managed(class_id,actor)))return false;
    alignas(16) unsigned char parameters[256]{};
    api.event(actor,getters[3].fn,parameters);
    return !api.getbool(getters[3].ret,parameters,0);
}

static double native_enemy_distance(const std::array<float,3>& position,const std::array<float,3>& origin){
    double distance{};
    for(unsigned i=0;i<position.size();++i){
        const auto delta=static_cast<double>(position[i])-origin[i];
        distance+=delta*delta;
    }
    return distance;
}

static int enumerate_native_enemies(void* context,E::NativeCandidate* out,unsigned count){
    if(!native_get_actors||!api.weakget(&native_enum_method)||!context||!api.isa(context,actor_class)||
       !out||!count||count>E::capacity)return -1;
    native_enum_count=0;
    for(unsigned i=0;i<metadata_count;++i)if(!api.weakget(&metadata_weak[i]))return -1;
    if(!makeweak(native_enum_context,context))return -1;
    struct OwnedArray {
        NativeActorArray actors{};
        ~OwnedArray(){if(actors.data){native_free(actors.data);++enum_frees;}}
    } batch;
    native_get_actors(context,base_class,&batch.actors);
    ++enum_calls;
    const auto& actors=batch.actors;
    if(actors.count<0||actors.capacity<actors.count||actors.capacity>65536||
       (actors.count&&!actors.data))return -1;
    enum_seen+=static_cast<unsigned>(actors.count);
    std::array<std::array<float,3>,qdcoop::player::slots> origins{};
    const auto origin_count=native_enrollment_origins(context,origins);
    constexpr double range=7000.0*7000.0;
    unsigned written=0;
    std::array<double,E::capacity> distances{};
    for(int i=0;i<actors.count;++i){
        const auto actor=actors.data[i];
        if(!actor||!api.isa(actor,base_class))continue;
        const auto position=native_actor_location(actor);
        const auto local_distance=native_enemy_distance(position,origins[0]);
        auto distance=local_distance;
        for(unsigned j=1;j<origin_count;++j)distance=std::min(distance,native_enemy_distance(position,origins[j]));
        if(!std::isfinite(distance)||distance>=range)continue;
        if(written==count&&distance>=distances[written-1])continue;
        void* mesh{};
        E::NativeCandidate candidate{};
        if(!identify_native_enemy(actor,mesh,candidate.class_id,candidate.mesh_id)||
           !native_enemy_discoverable(actor,candidate.class_id))continue;
        engine_weak_assign(&candidate.actor,actor);
        if(AnimU::resolve_weak(native_object_array,candidate.actor,catalog_read)!=reinterpret_cast<std::uintptr_t>(actor))
            continue;
        candidate.location=position;
        candidate.hp=AnimU::field<float>(actor,hp_at);
        unsigned place=std::min(written,count-1);
        while(place&&distance<distances[place-1]){
            out[place]=out[place-1];
            distances[place]=distances[place-1];
            --place;
        }
        out[place]=candidate;
        distances[place]=distance;
        written=std::min(written+1,count);
        if(local_distance>=range)++enum_peer_range;
    }
    for(unsigned i=0;i<written;++i)native_enum_candidates[i]=out[i].actor;
    native_enum_count=written;
    native_enum_world=envelope.world;
    enum_in_range+=written;
    return static_cast<int>(written);
}

static bool native_enumerated_identity(AnimU::WeakKey key){
    return native_enum_world==envelope.world&&api.weakget(&native_enum_context)&&
        std::any_of(native_enum_candidates.begin(),native_enum_candidates.begin()+native_enum_count,
            [&](AnimU::WeakKey candidate){return candidate.index==key.index&&candidate.serial==key.serial;});
}
