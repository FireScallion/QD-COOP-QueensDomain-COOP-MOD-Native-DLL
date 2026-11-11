enum FactoryObject { factory_gameplay,factory_visual_type,factory_begin,factory_finish,
                     factory_set_mesh,factory_owner,factory_audio,factory_collision,factory_object_count };
static constexpr std::string_view factory_paths[]{
    "/Script/Engine.Default__GameplayStatics",
    "/Script/Engine.SkeletalMeshActor",
    "/Script/Engine.GameplayStatics.BeginDeferredActorSpawnFromClass",
    "/Script/Engine.GameplayStatics.FinishSpawningActor",
    "/Script/Engine.SkinnedMeshComponent.SetSkeletalMesh",
    "/Script/Engine.ActorComponent.GetOwner",
    "/Script/Engine.Actor.K2_GetComponentsByClass",
    "/Script/Engine.Actor.SetActorEnableCollision"
};
static constexpr std::string_view factory_ownership_paths[]{
    "/Script/Engine.Actor.IsActorTickEnabled",
    "/Script/Engine.Actor.GetActorEnableCollision",
    "/Script/Engine.Actor.SetActorEnableCollision",
    "/Script/Engine.ActorComponent.IsComponentTickEnabled",
    "/Script/Engine.Pawn.GetController",
    "/Script/Engine.MovementComponent.StopMovementImmediately",
    "/Script/Engine.Controller.StopMovement",
    "/Script/AIModule.BrainComponent.StopLogic",
    "/Script/AIModule.BrainComponent.RestartLogic",
    "/Script/AIModule.BrainComponent.IsRunning",
    "/Script/Engine.ActorComponent",
    "/Script/Engine.MovementComponent",
    "/Script/AIModule.AIController",
    "/Script/AIModule.BrainComponent",
    "/Script/Engine.AudioComponent",
    "/Script/Engine.AudioComponent.SetPaused",
    "/Script/Engine.AudioComponent.GetPlayState"
};
static_assert(std::size(factory_paths)==factory_object_count);
static constexpr unsigned factory_metadata_count=factory_object_count+std::size(factory_ownership_paths);

struct FactoryOffsets {
    int context{},actor_class{},begin_transform{},collision{},owner{},begin_result{};
    int finish_actor{},finish_transform{},finish_result{};
    int mesh{},reinitialize{},audio_class{},audio_result{};
};
static FactoryOffsets factory_offsets;
static std::array<Weak,factory_metadata_count> factory_metadata{};
static Weak automatic_context;
static unsigned automatic_limit,automatic_guest_cursor;
static double automatic_next_scan;
static bool factory_configured;

static void* factory_object(unsigned index){return api.weakget(&factory_metadata[index]);}

static int factory_parameter(void* function,std::string_view name,int width){
    const auto bytes=*api.parms(function);
    if(!bytes||bytes>256)return -1;
    const auto field=reinterpret_cast<void*>(AnimU::find_property(
        reinterpret_cast<std::uintptr_t>(function),catalog_name_pool,name,catalog_read));
    if(!property(field,width,bytes)||!(AnimU::field<std::uint64_t>(field,64)&0x80u))return -1;
    return *api.offset(field);
}

static bool configure_native_factory(const std::uint64_t* values,unsigned count){
    if(!native_animation_ready()||automatic_limit||!values||
       (count!=factory_metadata_count&&count!=factory_object_count))return false;
    std::array<Weak,factory_metadata_count> keys{};
    std::array<void*,factory_metadata_count> objects{};
    for(unsigned i=0;i<count;++i){
        objects[i]=reinterpret_cast<void*>(static_cast<std::uintptr_t>(values[i]));
        const auto expected=i<factory_object_count?factory_paths[i]:factory_ownership_paths[i-factory_object_count];
        std::string path;
        if(!catalog_path(objects[i],path)||path!=expected||!makeweak(keys[i],objects[i]))return 0;
    }
    const auto begin=objects[factory_begin],finish=objects[factory_finish];
    const auto mesh=objects[factory_set_mesh],audio=objects[factory_audio];
    const FactoryOffsets offsets{
        factory_parameter(begin,"WorldContextObject",8),factory_parameter(begin,"ActorClass",8),
        factory_parameter(begin,"SpawnTransform",48),factory_parameter(begin,"CollisionHandlingOverride",1),
        factory_parameter(begin,"Owner",8),factory_parameter(begin,"ReturnValue",8),
        factory_parameter(finish,"Actor",8),factory_parameter(finish,"SpawnTransform",48),
        factory_parameter(finish,"ReturnValue",8),factory_parameter(mesh,"NewMesh",8),
        factory_parameter(mesh,"bReinitPose",1),factory_parameter(audio,"ComponentClass",8),
        factory_parameter(audio,"ReturnValue",16)
    };
    const int positions[]{offsets.context,offsets.actor_class,offsets.begin_transform,offsets.collision,
        offsets.owner,offsets.begin_result,offsets.finish_actor,offsets.finish_transform,offsets.finish_result,
        offsets.mesh,offsets.reinitialize,offsets.audio_class,offsets.audio_result};
    if(std::any_of(std::begin(positions),std::end(positions),[](int offset){return offset<0;})||
       *api.parms(objects[factory_owner])!=8||factory_parameter(objects[factory_owner],"ReturnValue",8)!=0||
       *api.parms(objects[factory_collision])!=1||!catalog_subclass(objects[factory_visual_type],actor_class))
        return 0;
    factory_metadata=keys;
    factory_offsets=offsets;
    factory_configured=true;
    return 1;
}

extern "C" __declspec(dllexport) int QDEnemyConfigureNativeFactory(const std::uint64_t* values,unsigned count){
    return !host_role&&count==factory_metadata_count&&configure_native_factory(values,count);
}

extern "C" __declspec(dllexport) int QDEnemyEnableNativeWorld(void* context,unsigned limit){
    if(!native_enrollment_ready()||automatic_limit||!context||!api.isa(context,actor_class)||
       !limit||limit>E::capacity||!native_get_actors||!api.weakget(&native_enum_method)||!catalog_ready||
       !death_metadata_alive()||(!host_role&&(!factory_configured||!api.weakget(&replica_destroy_method))))
        return 0;
    if(host_role&&!install_kill_observer())return 0;
    if(!makeweak(automatic_context,context))return 0;
    automatic_limit=limit;
    return 1;
}

extern "C" __declspec(dllexport) int QDEnemyInspectNativeBinding(std::uint32_t id,E::NativeBindingView* out){
    if(!native_animation_ready()||!id||id>E::capacity||!out)return 0;
    const auto& entry=enrollments[id-1];
    const auto& entity=entities[id-1];
    if(!entry.offer.id||entry.retiring||!entity.used||!api.weakget(&entity.actor))return 0;
    E::NativeBindingView value{};
    value.generation=entity.generation;
    value.confirmed=host_role?entry.waiting==0:entry.confirmed;
    std::memcpy(&value.actor,entity.actor.bytes,sizeof(value.actor));
    if(entity.split)std::memcpy(&value.display,entity.visual.bytes,sizeof(value.display));
    *out=value;
    return 1;
}

using FactoryParameters=std::array<unsigned char,256>;
template<class T>static void factory_put(FactoryParameters& args,int offset,const T& value){
    std::memcpy(args.data()+offset,&value,sizeof(value));
}

static void* factory_spawn(void* context,void* type,const std::array<float,3>& position,Weak& owned,bool hidden=false,
                           bool(*prepare)(void*)=nullptr){
    const auto gameplay=factory_object(factory_gameplay);
    const auto begin=factory_object(factory_begin),finish=factory_object(factory_finish);
    if(!gameplay||!begin||!finish||!type||!catalog_subclass(type,actor_class))return nullptr;
    const auto& offsets=factory_offsets;
    const std::array<float,12> transform{0,0,0,1,position[0],position[1],position[2],0,1,1,1,0};
    alignas(16) FactoryParameters args{},completed{};
    factory_put(args,offsets.context,context);
    factory_put(args,offsets.actor_class,type);
    factory_put(args,offsets.begin_transform,transform);
    factory_put(args,offsets.collision,std::uint8_t{1}); // AlwaysSpawn.
    factory_put(args,offsets.owner,static_cast<void*>(nullptr));
    api.event(gameplay,begin,args.data());
    auto actor=AnimU::field<void*>(args.data(),offsets.begin_result);
    if(!actor||!api.isa(actor,actor_class))return nullptr;
    if(!makeweak(owned,actor)){
        if(auto destroy=api.weakget(&replica_destroy_method))api.event(actor,destroy,nullptr);
        return nullptr;
    }
    if(!api.isa(actor,type))return nullptr;
    if(hidden)own_set(actor,hide_fn,true);
    if(prepare&&!prepare(actor))return nullptr;
    factory_put(completed,offsets.finish_actor,actor);
    factory_put(completed,offsets.finish_transform,transform);
    api.event(gameplay,finish,completed.data());
    return AnimU::field<void*>(completed.data(),offsets.finish_result)==actor&&api.weakget(&owned)==actor?
        actor:nullptr;
}

static bool factory_apply_skinned_mesh(void* component,void* asset,void* type){
    const auto setter=factory_object(factory_set_mesh);
    if(!component||!setter||!type||!api.isa(component,type))return false;
    alignas(16) FactoryParameters args{};
    factory_put(args,factory_offsets.mesh,asset);
    factory_put(args,factory_offsets.reinitialize,std::uint8_t{1});
    api.event(component,setter,args.data());
    return native_skeletal_asset(component)==asset;
}

static bool factory_apply_mesh(void* component,void* asset){
    return factory_apply_skinned_mesh(component,asset,mesh_class);
}

static bool factory_configure_ownership(void* actor){
    const auto los=catalog_object_field(actor,"EnemyLineOfSight");
    if(ownership_alive()&&(!los||(own_meta(14)&&api.isa(los,own_meta(14)))))return true;
    if(ownership_ready&&own_meta(14))return false;
    std::array<std::uint64_t,24> args{};
    for(unsigned i=0;i<14;++i)args[i]=reinterpret_cast<std::uintptr_t>(factory_object(factory_object_count+i));
    for(unsigned i=0;i<3;++i)args[21+i]=reinterpret_cast<std::uintptr_t>(factory_object(factory_object_count+14+i));
    const auto los_type=los?AnimU::field<void*>(los,16):nullptr;
    args[14]=reinterpret_cast<std::uintptr_t>(los_type);
    args[15]=reinterpret_cast<std::uintptr_t>(catalog_field(base_class,"CharacterMovement",8));
    args[16]=reinterpret_cast<std::uintptr_t>(catalog_field(base_class,"ActorUROManager",8));
    args[17]=reinterpret_cast<std::uintptr_t>(catalog_field(base_class,"EnemyLineOfSight",8));
    args[18]=reinterpret_cast<std::uintptr_t>(catalog_field(base_class,"bIsAIDisabled",1));
    args[19]=reinterpret_cast<std::uintptr_t>(catalog_field(reinterpret_cast<void*>(args[12]),"BrainComponent",8));
    args[20]=reinterpret_cast<std::uintptr_t>(catalog_field(los_type,"bEnabled",1));
    return configure_ownership(args,true);
}

static bool factory_take_ownership(const Roster::Offer& offer,void* actor){
    if(!factory_configure_ownership(actor)||!native_free)return false;
    const auto function=factory_object(factory_audio),audio_class=own_meta(15);
    if(!function||!audio_class)return false;
    alignas(16) FactoryParameters args{};
    factory_put(args,factory_offsets.audio_class,audio_class);
    api.event(actor,function,args.data());
    const auto result=AnimU::field<NativeActorArray>(args.data(),factory_offsets.audio_result);
    std::array<void*,16> audio{};
    const bool valid=result.count>=0&&result.capacity>=result.count&&
        result.count<=static_cast<int>(audio.size())&&(!result.count||result.data);
    if(valid&&result.count)std::copy_n(result.data,result.count,audio.begin());
    if(result.data)native_free(result.data);
    return valid&&ownership_take(offer.id,offer.generation,audio,static_cast<unsigned>(result.count));
}

static bool factory_create_replica(void* context,Enrollment& entry,void* type,void* asset){
    const auto& offer=entry.offer;
    auto actor=factory_spawn(context,type,offer.location,entry.replica);
    if(!actor)return false;
    auto mesh=AnimU::field<void*>(actor,mesh_at);
    if(!mesh||!api.isa(mesh,mesh_class))return false;
    if(native_skeletal_asset(mesh)!=asset&&!factory_apply_mesh(mesh,asset))return false;
    if(!QDEnemyRegisterNativeReplica(&offer,actor,mesh))return false;
    auto visual=factory_spawn(context,factory_object(factory_visual_type),offer.location,entry.display);
    auto pose=catalog_object_field(visual,"SkeletalMeshComponent");
    if(!visual||!factory_apply_mesh(pose,asset))return false;
    const auto collision=factory_object(factory_collision);
    if(!collision)return false;
    unsigned char disabled{};
    api.event(visual,collision,&disabled);
    return QDEnemyBindNativeDisplay(offer.id,offer.generation,visual,pose,factory_object(factory_owner))&&
        factory_take_ownership(offer,actor)&&QDEnemyBindCatalogNativeAnimation(offer.id,offer.generation);
}

static bool factory_rollback(unsigned index){
    entities[index].used=false;
    if(!destroy_owned_replica(enrollments[index]))return false;
    enrollments[index].hit_queries=false;
    entities[index]={};
    anim_bindings[index]={};
    ownership[index]={};
    return true;
}

static void automatic_guest_step(void* context,double time){
    if(!unconfirmed_offers)return;
    constexpr unsigned attempts_per_tick=2;
    unsigned attempted=0;
    for(unsigned visited=0;visited<E::capacity&&attempted<attempts_per_tick;++visited){
        const auto index=automatic_guest_cursor;
        automatic_guest_cursor=(automatic_guest_cursor+1)%E::capacity;
        auto& entry=enrollments[index];
        if(!entry.offer.id||entry.retiring||entry.confirmed||time<entry.factory_retry)continue;
        ++attempted;
        if(entry.factory_ready&&(!entities[index].used||!api.weakget(&entry.replica)||!api.weakget(&entry.display)))
            entry.factory_ready=false;
        if(!entry.factory_ready){
            if(!factory_rollback(index)){entry.factory_retry=time+.2;continue;}
            const auto type=api.weakget(&enemy_classes.keys[entry.offer.class_id]);
            const auto asset=api.weakget(&enemy_meshes.keys[entry.offer.mesh_id]);
            if(!type||!asset){++factory_unloaded;entry.factory_retry=time+1.;continue;}
            if(!factory_create_replica(context,entry,type,asset)){
                ++factory_failed;
                factory_rollback(index);
                entry.factory_retry=time+.5;
                continue;
            }
            entry.factory_ready=true;
        }
        if(QDEnemyConfirmNativeReplica(entry.offer.id,entry.offer.generation))++factory_created;
        else entry.factory_retry=time+.2;
    }
}

static unsigned automatic_free_slot(){
    for(unsigned slot=0;slot<automatic_limit;++slot){
        const auto& entry=enrollments[slot];
        if(!entry.offer.id&&!entry.retiring&&!entities[slot].used&&entry.tombstone!=UINT32_MAX)
            return slot;
    }
    return automatic_limit;
}

static void automatic_host_step(void* context,double time){
    if(time<automatic_next_scan)return;
    automatic_next_scan=time+.5;
    auto slot=automatic_free_slot();
    if(slot==automatic_limit)return;
    std::array<E::NativeCandidate,E::capacity> candidates{};
    const auto count=enumerate_native_enemies(context,candidates.data(),E::capacity);
    unsigned added=0;
    for(int i=0;i<count&&added<2&&slot<automatic_limit;++i){
        const auto& identity=candidates[i].actor;
        auto actor=reinterpret_cast<void*>(AnimU::resolve_weak(native_object_array,identity,catalog_read));
        if(!actor||std::any_of(entities.begin(),entities.end(),[&](const Entity& entity){
            return entity.used&&api.weakget(&entity.actor)==actor;
        }))continue;
        void* type{};
        void* skeleton{};
        if(!catalog_mesh_profile(AnimU::field<void*>(actor,mesh_at),type,skeleton)){
            ++automatic_profile_unready;
            continue;
        }
        const auto generation=enrollments[slot].tombstone+1;
        if(!QDEnemyEnrollNativeEnemy(slot+1,generation,&identity))continue;
        if(QDEnemyBindCatalogNativeAnimation(slot+1,generation)){
            ++automatic_enrolled;
            ++added;
        }else{
            ++factory_failed;
            begin_host_retirement(slot,generation);
        }
        slot=automatic_free_slot();
    }
}

static void player_step(void* context,double time);
static void native_life_step(void* pawn,double time);
static bool checkpoint_step(void* pawn,double time);
static void world_progress_step(void* pawn,double time);
static void world_actor_step(void* pawn,double time);
static void world_mechanisms_step(void* pawn,double time);
static void world_barriers_step(void* pawn,double time);
static void world_progress_notify(void* pawn,double time);
static void native_pause_step(void* pawn,double time);
static void native_damage_step(void* pawn,double time);
static void native_boss_interact_step(double time);
static void native_projectile_step(void* context,double time);
static void native_placed_items_step(void* pawn,double time);
static void native_body_material_step(double time);
static void native_visual_mount_step(double time);
static void native_healthbar_step(void* pawn,double time);
static void native_sound_step(void* context,double time);
static void native_travel_guard_step(double time);
static void world_stream_step(void* pawn,double time);
static void world_music_step(double time);
static void world_scene_step(void* pawn,double time);
static bool world_scene_busy();
static void world_story_step(void* pawn,double time);
static void world_library_step(void* pawn,double time);
static void world_elevator_step(void* pawn,double time);
static void native_reward_step(void* pawn,double time);

static void native_world_step(double time){
    if(!automatic_limit)return;
    const auto context=api.weakget(&automatic_context);
    if(!context)return;
    native_pause_step(context,time);
    native_travel_guard_step(time);
    native_life_step(context,time);
    native_reward_step(context,time);
    native_damage_step(context,time);
    native_boss_interact_step(time);
    native_projectile_step(context,time);
    native_sound_step(context,time);
    world_progress_step(context,time);
    native_placed_items_step(context,time);
    world_stream_step(context,time);
    world_scene_step(context,time);
    if(!world_scene_busy())world_music_step(time);
    world_story_step(context,time);
    world_library_step(context,time);
    world_elevator_step(context,time);
    world_actor_step(context,time);
    world_mechanisms_step(context,time);
    world_barriers_step(context,time);
    world_progress_notify(context,time);
    if(!world_scene_busy()&&checkpoint_step(context,time))return;
    player_step(context,time);
    if(host_role)automatic_host_step(context,time);
    else automatic_guest_step(context,time);
    native_body_material_step(time);
    native_visual_mount_step(time);
    native_healthbar_step(context,time);
}
