namespace Projectile=qdcoop::projectile;
struct ProjectilePart {
    Weak component,asset;
    std::uint16_t id{};
};
struct ProjectileSource {
    Weak actor,author,display_owner;
    ProjectilePart mesh,trail;
    Projectile::Frame frame;
    std::uint32_t generation{};
    double next{},retire_until{},first_seen{},next_bind{},deactivate_until{};
    bool published{},following{},detached{};
};
struct ProjectileVisual {
    Projectile::Track track;
    Weak actor,component,trail;
    std::uint32_t generation{};
    std::uint16_t mesh_id{},trail_id{};
    double next_spawn{},next_trail{};
    std::array<float,12> pose{},trail_pose{};
    std::array<Projectile::Parameter,Projectile::parameter_capacity> parameters{};
    bool positioned{},trail_positioned{},trail_started{},trail_hidden{};
};
static std::array<ProjectileSource,Projectile::capacity> projectile_sources;
static std::array<std::array<ProjectileVisual,Projectile::capacity>,Player::slots> projectile_visuals;
struct NativeVisualAsset {Weak asset;double retry{};};
static std::array<NativeVisualAsset,Projectile::assets::meshes.size()> projectile_meshes;
static std::array<NativeVisualAsset,Projectile::assets::trails.size()> projectile_trails;
static std::array<NativeVisualAsset,Projectile::assets::particles.size()> projectile_particles;
static Weak player_projectile_class,enemy_projectile_base,projectile_hook_function,projectile_effect_function,projectile_attach_function;
static Weak projectile_cascade_location,projectile_cascade_attached;
static Weak projectile_activate_function,projectile_deactivate_function,projectile_set_active_function;
static Weak projectile_set_asset_function;
static std::atomic<KillExec> projectile_original_finish{},projectile_original_effect{},projectile_original_attach{};
static std::atomic<KillExec> projectile_original_cascade_location{},projectile_original_cascade_attached{};
static std::atomic<KillExec> projectile_original_activate{},projectile_original_deactivate{},projectile_original_set_active{};
static std::atomic<KillExec> projectile_original_set_asset{};

static bool projectile_local_owner(void* actor,void* pawn){
    for(auto source:{catalog_object_field(actor,"Owner"),catalog_object_field(actor,"Instigator")}){
        for(unsigned depth=0;source&&depth<4&&api.isa(source,actor_class);++depth){
            if(source==pawn)return true;
            const auto parent=catalog_object_field(source,"Owner");
            if(parent==source)break;
            source=parent;
        }
    }
    return false;
}

static bool enemy_projectile_class(void* type){
    if(const auto base=api.weakget(&enemy_projectile_base))return catalog_subclass(type,base);
    constexpr std::string_view expected="/Game/Enemies/Shared/Enemy_Projectile_Base.Enemy_Projectile_Base_C";
    std::string name;
    for(unsigned depth=0;type&&depth<32;++depth,type=AnimU::field<void*>(type,64)){
        const auto key=AnimU::field<AnimU::NameKey>(type,24);
        if(AnimU::name_string(catalog_name_pool,key,catalog_read,name)&&name==catalog_leaf(expected)&&
           catalog_path(type,name)&&name==expected)return makeweak(enemy_projectile_base,type);
    }
    return false;
}
static bool player_projectile_type(void* type){
    if(!type)return false;
    if(api.weakget(&player_projectile_class)==type)return true;
    std::string path;
    return catalog_path(type,path)&&Projectile::assets::id(Projectile::assets::projectile_classes,path)&&
        makeweak(player_projectile_class,type);
}

static ProjectileSource* projectile_reserve(void* object,Projectile::Profile profile,double time){
    for(unsigned i=0;i<projectile_sources.size();++i){
        auto& slot=projectile_sources[i];
        if(slot.frame.id||!makeweak(slot.actor,object))continue;
        if(!++slot.generation)++slot.generation;
        slot.frame={i+1,slot.generation,profile,Projectile::Phase::flying,0};
        slot.frame.slot=host_role?0:Player::unassigned;
        slot.author=slot.display_owner={};
        slot.mesh=slot.trail={};
        slot.next=slot.retire_until=slot.next_bind=slot.deactivate_until=0;
        slot.first_seen=time;
        slot.published=false;
        slot.following=slot.detached=false;
        ++projectile_observed;
        return &slot;
    }
    ++projectile_rejected;
    return nullptr;
}

static void observe_projectile_spawn(void* context,void* frame,void* result){
    projectile_original_finish.load(std::memory_order_acquire)(context,frame,result);
    if(GetCurrentThreadId()!=game_thread.load()||!result||!active_peer_mask)return;
    const auto actor=AnimU::field<void*>(result,0);
    const auto owner=api.weakget(&automatic_context);
    if(!actor||!owner)return;
    const auto type=AnimU::field<void*>(actor,16);
    if(!type)return;
    auto profile=Projectile::Profile::stock_player;
    if(host_role&&enemy_projectile_class(type)){
        const auto world=api.weakget(&attached_world);
        if(!world||native_actor_world(actor)!=world)return;
        profile=Projectile::Profile::stock_enemy;
    }else if(!projectile_local_owner(actor,owner)||!player_projectile_type(type))return;
    projectile_reserve(actor,profile,now());
}
static bool native_install_exec(void* function,std::uintptr_t rva,const unsigned char* prefix,std::size_t length,
                                    Weak& key,std::atomic<KillExec>& original,KillExec observer){
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto stock=reinterpret_cast<void*>(image+rva);
    auto cell=reinterpret_cast<void* volatile*>(static_cast<unsigned char*>(function)+216);
    MEMORY_BASIC_INFORMATION memory{};
    if(*cell!=stock||std::memcmp(stock,prefix,length)||
       !VirtualQuery(const_cast<void**>(cell),&memory,sizeof(memory))||memory.State!=MEM_COMMIT||
       !(memory.Protect&PAGE_READWRITE)||(memory.Protect&PAGE_GUARD)||!makeweak(key,function))return false;
    original.store(reinterpret_cast<KillExec>(stock),std::memory_order_release);
    if(InterlockedCompareExchangePointer(cell,reinterpret_cast<void*>(observer),stock)!=stock){
        key={};
        return false;
    }
    return true;
}
static bool install_projectile_observer(){
    if(api.weakget(&projectile_hook_function))return true;
    const auto finish=factory_object(factory_finish);
    if(!finish||factory_offsets.finish_actor!=0||
       factory_offsets.finish_transform!=16||factory_offsets.finish_result!=64||*api.parms(finish)!=72)return false;
    constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x20,0x57,0x48,0x83,0xec,0x70};
    return native_install_exec(finish,0x2dbdf80,prefix,sizeof(prefix),projectile_hook_function,
                                   projectile_original_finish,&observe_projectile_spawn);
}
static Projectile::Transform projectile_read_transform(const void* data,unsigned offset){
    return {AnimU::field<std::array<float,3>>(data,offset+16),
            AnimU::field<std::array<float,3>>(data,offset+32),
            AnimU::field<std::array<float,4>>(data,offset)};
}
static Projectile::Transform projectile_transform(void* component){
    alignas(16) FactoryParameters args{};
    api.event(component,getters[4].fn,args.data());
    return projectile_read_transform(args.data(),getters[4].at);
}
template<std::size_t Count>
static std::uint16_t projectile_known_asset(ProjectilePart& part,void* asset,
                                           const std::array<std::string_view,Count>& catalog,std::size_t player_count){
    if(!asset){
        part.asset={};
        part.id=0;
        return 0;
    }
    if(api.weakget(&part.asset)==asset)return part.id;
    std::string path;
    part.id=catalog_path(asset,path)?Projectile::assets::id(catalog,path,player_count):0;
    if(!makeweak(part.asset,asset))part.id=0;
    return part.id;
}
template<std::size_t Count>
static std::uint16_t projectile_asset_id(ProjectilePart& part,void* component,const char* property,
                                         const std::array<std::string_view,Count>& catalog,std::size_t player_count){
    return projectile_known_asset(part,catalog_object_field(component,property),catalog,player_count);
}
static std::uint16_t projectile_mesh_id(ProjectilePart& part,void* component){
    return projectile_asset_id(part,component,"StaticMesh",Projectile::assets::meshes,Projectile::assets::player_meshes);
}
static std::uint16_t projectile_trail_id(ProjectilePart& part,void* component){
    return projectile_asset_id(part,component,"Asset",Projectile::assets::trails,Projectile::assets::player_trails);
}
static void projectile_bind_parts(void* actor,ProjectileSource& source,double time){
    if(time<source.next_bind)return;
    source.next_bind=time+.20;
    const auto scene=world_input(world_projectile_scene),mesh=world_input(world_static_component);
    const auto trail=world_input(world_projectile_niagara),fn=factory_object(factory_audio);
    if(!scene||!mesh||!trail||!fn||!native_free)return;
    alignas(16) FactoryParameters args{};
    factory_put(args,factory_offsets.audio_class,scene);
    api.event(actor,fn,args.data());
    const auto components=AnimU::field<NativeActorArray>(args.data(),factory_offsets.audio_result);
    auto mesh_bound=api.weakget(&source.mesh.component),trail_bound=api.weakget(&source.trail.component);
    if(components.data&&components.count>0&&components.count<=64&&components.capacity>=components.count){
        for(int i=0;i<components.count&&(!mesh_bound||!trail_bound);++i){
            const auto component=components.data[i];
            if(!component)continue;
            if(!mesh_bound&&api.isa(component,mesh)&&
               projectile_mesh_id(source.mesh,component)&&makeweak(source.mesh.component,component))
                mesh_bound=component;
            if(!trail_bound&&api.isa(component,trail)&&
               projectile_trail_id(source.trail,component)&&makeweak(source.trail.component,component))
                trail_bound=component;
        }
    }
    if(components.data)native_free(components.data);
}
static bool projectile_trail_live(void* component){
    const auto active=world_input(world_projectile_is_active);
    if(!component||!active||!medic_params(world_projectile_is_active,1,{{"ReturnValue",1,0}}))return false;
    unsigned char running{};
    api.event(component,active,&running);
    return running!=0;
}
static bool projectile_visible(void* component,void* owner){
    if(owner&&api.getbool(bool_props[3],owner,0))return false;
    alignas(16) FactoryParameters args{};
    api.event(component,getters[5].fn,args.data());
    return api.getbool(getters[5].ret,args.data(),0);
}
#include "native_projectile_suppression.inc"
static Projectile::Profile projectile_effect_profile(void* caller,bool following,bool cascade){
    if(cascade&&!host_role)return {};
    const auto world=api.weakget(&attached_world);
    if(!caller||!world||!api.isa(caller,actor_class)||native_actor_world(caller)!=world)return {};
    const auto type=AnimU::field<void*>(caller,16),pawn=api.weakget(&automatic_context);
    // The projectile profile already owns its stock component trail.
    if(following&&!cascade&&(player_projectile_type(type)||enemy_projectile_class(type)))return {};
    if(host_role&&(api.isa(caller,base_class)||enemy_projectile_class(type)))
        return cascade?Projectile::Profile::stock_enemy_particle:Projectile::Profile::stock_enemy_effect;
    if(!cascade&&pawn&&(caller==pawn||(projectile_local_owner(caller,pawn)&&player_projectile_type(type))))
        return Projectile::Profile::stock_player_effect;
    return {};
}
static void projectile_seed_effect(void* owner,void* component,Projectile::Frame& frame);
static bool projectile_record_effect(void* caller,void* effect,Projectile::Profile profile,bool following,
                                     const ProjectilePart& asset,const Projectile::Transform& pose){
    Projectile::Frame captured{1,1,profile,Projectile::Phase::flying,1};
    captured.trail_asset=asset.id;
    captured.mesh=captured.trail=pose;
    captured.trail_active=true;
    if(effect)projectile_seed_effect(caller,effect,captured);
    if(!Projectile::valid(captured))return false;
    // A Niagara pool can reuse a component for a new burst in the same tick.
    if(effect)for(auto& source:projectile_sources)
        if(Projectile::effect(source.frame.profile)&&api.weakget(&source.actor)==effect)source.actor={};
    if(const auto source=projectile_reserve(effect?effect:caller,profile,now())){
        makeweak(source->author,caller);
        if(effect)if(const auto owner=native_cosmetic_actor(effect);owner&&api.isa(owner,actor_class))
            makeweak(source->display_owner,owner);
        source->trail=asset;
        source->frame.mesh=source->frame.trail=captured.trail;
        source->frame.trail_asset=captured.trail_asset;
        source->frame.trail_active=true;
        source->frame.parameters=captured.parameters;
        source->following=following;
        source->detached=!effect;
        return true;
    }
    return false;
}
static bool projectile_observe_effect(void* caller,void* effect,bool following,bool cascade=false){
    const auto component=world_input(cascade?world_projectile_cascade_component:world_projectile_niagara);
    if(!effect||!component||!api.isa(effect,component))return false;
    if(projectile_suppress_replica(caller,effect))return true;
    caller=native_cosmetic_actor(caller);
    const auto profile=projectile_effect_profile(caller,following,cascade);
    if(profile==Projectile::Profile::none||!projectile_trail_live(effect))return false;
    ProjectilePart asset;
    if(cascade)projectile_asset_id(asset,effect,"Template",Projectile::assets::particles,Projectile::assets::player_particles);
    else projectile_trail_id(asset,effect);
    if(asset.id)projectile_record_effect(caller,effect,profile,following,asset,projectile_transform(effect));
    return true;
}
static void* projectile_location_caller(void* frame,const Weak& function){
    if(!AnimU::field<const void*>(frame,32)&&AnimU::field<void*>(frame,16)==api.weakget(&function))
        if(const auto params=AnimU::field<const void*>(frame,40))return AnimU::field<void*>(params,0);
    return AnimU::field<void*>(frame,24);
}
#include "native_projectile_niagara_location.inc"
static void observe_projectile_cascade_location(void* context,void* frame,void* result){
    projectile_original_cascade_location.load(std::memory_order_acquire)(context,frame,result);
    if(GetCurrentThreadId()!=game_thread.load()||!active_peer_mask||!frame||!result)return;
    projectile_observe_effect(projectile_location_caller(frame,projectile_cascade_location),AnimU::field<void*>(result,0),false,true);
}
static void observe_projectile_cascade_attached(void* context,void* frame,void* result){
    projectile_original_cascade_attached.load(std::memory_order_acquire)(context,frame,result);
    if(GetCurrentThreadId()!=game_thread.load()||!active_peer_mask||!result)return;
    const auto effect=AnimU::field<void*>(result,0),type=world_input(world_projectile_cascade_component);
    if(!effect||!type||!api.isa(effect,type))return;
    projectile_observe_effect(catalog_object_field(effect,"AttachParent"),effect,true,true);
}
static ProjectileSource* projectile_tracked_effect(void* component){
    for(auto& source:projectile_sources)
        if(Projectile::effect(source.frame.profile)&&source.frame.phase==Projectile::Phase::flying&&
           api.weakget(&source.actor)==component)return &source;
    return nullptr;
}
#include "native_projectile_parameters.inc"
static void projectile_seed_effect(void* owner,void* component,Projectile::Frame& frame){
    if(!host_role||!owner||!component||(frame.profile!=Projectile::Profile::stock_enemy_effect&&
       frame.profile!=Projectile::Profile::stock_enemy_particle))return;
    std::string leaf;
    if(!AnimU::name_string(catalog_name_pool,AnimU::field<AnimU::NameKey>(component,24),catalog_read,leaf))return;
    std::array<void*,24> chain{};
    unsigned count{};
    for(auto type=AnimU::field<void*>(owner,16);type&&count<chain.size();type=AnimU::field<void*>(type,64)){
        chain[count++]=type;
        if(type==base_class)break;
    }
    constexpr auto& defaults=qdcoop::enemy_visual_catalog::parameters;
    while(count){
        std::string path;
        if(!catalog_path(chain[--count],path))continue;
        auto entry=std::ranges::lower_bound(defaults,path,std::ranges::less{},&qdcoop::enemy_visual_catalog::Parameter::owner);
        for(;entry!=std::end(defaults)&&entry->owner==path;++entry){
            if(entry->component!=leaf)continue;
            const auto id=projectile_parameter_id(entry->name);
            if(id)Projectile::set_parameter(frame,{id,static_cast<ProjectileParameterType>(entry->type),entry->words});
        }
    }
}
static void observe_projectile_set_asset(void* context,void* frame,void* result){
    projectile_original_set_asset.load(std::memory_order_acquire)(context,frame,result);
    if(GetCurrentThreadId()!=game_thread.load()||!active_peer_mask||!context)return;
    const auto source=projectile_tracked_effect(context);
    if(!source)return;
    const auto replacement=catalog_object_field(context,"Asset");
    if(replacement==api.weakget(&source->trail.asset))return;
    const auto author=api.weakget(&source->author);
    const bool following=source->following;
    source->actor={};
    source->deactivate_until=now();
    source->next=0;
    if(replacement&&author)projectile_observe_effect(author,context,following);
}
static void install_projectile_asset_observer(){
    if(api.weakget(&projectile_set_asset_function)||
       !medic_params(world_projectile_niagara_set_asset,9,{{"InAsset",8,0},{"bResetExistingOverrideParameters",1,8}}))return;
    constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x18,0x57,0x48,
        0x83,0xec,0x20,0x33,0xff,0x48,0x8b,0xda,0x48,0x89,0x7c,0x24};
    native_install_exec(world_input(world_projectile_niagara_set_asset),0x994b80,prefix,sizeof(prefix),
        projectile_set_asset_function,projectile_original_set_asset,&observe_projectile_set_asset);
}
static void projectile_component_activated(void* context){
    const auto particle=world_input(world_projectile_cascade_component),niagara=world_input(world_projectile_niagara);
    const bool cascade=particle&&api.isa(context,particle);
    if(!cascade&&(!niagara||!api.isa(context,niagara)))return;
    const auto parent=catalog_object_field(context,"AttachParent");
    const auto author=parent?parent:context;
    if(const auto source=projectile_tracked_effect(context)){
        if(projectile_suppress_replica(author,context)){
            source->actor={};
            source->deactivate_until=now();
            source->next=0;
        }else if(source->deactivate_until&&projectile_trail_live(context))source->deactivate_until=source->next=0;
        return;
    }
    projectile_observe_effect(author,context,true,cascade);
}
static void projectile_component_deactivated(void* context){
    if(const auto source=projectile_tracked_effect(context);source&&!source->deactivate_until){
        source->deactivate_until=now()+1.;
        source->next=0;
    }
}
static void observe_projectile_activation(void* context,void* frame,void* result){
    projectile_original_activate.load(std::memory_order_acquire)(context,frame,result);
    if(GetCurrentThreadId()==game_thread.load()&&active_peer_mask&&context)projectile_component_activated(context);
}
static void observe_projectile_deactivation(void* context,void* frame,void* result){
    projectile_original_deactivate.load(std::memory_order_acquire)(context,frame,result);
    if(GetCurrentThreadId()==game_thread.load()&&active_peer_mask&&context)projectile_component_deactivated(context);
}
static void observe_projectile_set_active(void* context,void* frame,void* result){
    const auto original=projectile_original_set_active.load(std::memory_order_acquire);
    if(GetCurrentThreadId()!=game_thread.load()||!active_peer_mask||!context||!frame){
        original(context,frame,result);
        return;
    }
    const auto niagara=world_input(world_projectile_niagara),particle=world_input(world_projectile_cascade_component);
    if(!(niagara&&api.isa(context,niagara))&&!(host_role&&particle&&api.isa(context,particle))){
        original(context,frame,result);
        return;
    }
    auto& code=*reinterpret_cast<const unsigned char**>(static_cast<unsigned char*>(frame)+32);
    bool active{};
    if(!code){
        const auto function=api.weakget(&projectile_set_active_function);
        const auto params=AnimU::field<const void*>(frame,40);
        const auto field=catalog_field(function,"bNewActive",1);
        if(!params||AnimU::field<void*>(frame,16)!=function||!medic_field_class(field,"BoolProperty")){
            original(context,frame,result);
            return;
        }
        active=api.getbool(field,params,0);
        original(context,frame,result);
    }else{
        using SetActive=void(*)(void*,bool,bool);
        const auto implementation=AnimU::field<SetActive>(AnimU::field<const void*>(context,0),0x288);
        unsigned requested{},reset{};
        native_vm_argument(frame,&requested);
        native_vm_argument(frame,&reset);
        if(code)++code;
        active=requested!=0;
        implementation(context,active,reset!=0);
    }
    if(active)projectile_component_activated(context);
    else projectile_component_deactivated(context);
}
static void observe_projectile_attachment(void* context,void* frame,void* result){
    projectile_original_attach.load(std::memory_order_acquire)(context,frame,result);
    if(GetCurrentThreadId()!=game_thread.load()||!active_peer_mask||!frame||!result)return;
    const auto effect=AnimU::field<void*>(result,0);
    if(!effect||!api.isa(effect,world_input(world_projectile_niagara)))return;
    projectile_observe_effect(catalog_object_field(effect,"AttachParent"),effect,true);
}
static bool install_projectile_effect_observer(){
    if(api.weakget(&projectile_effect_function))return true;
    const auto spawn=world_input(world_projectile_spawn_location);
    if(!spawn||!native_bind_vm()||!medic_params(world_projectile_spawn_location,64,{{"WorldContextObject",8,0},{"SystemTemplate",8,8},
        {"Location",12,16},{"Rotation",12,28},{"Scale",12,40},{"bAutoDestroy",1,52},{"bAutoActivate",1,53},
        {"PoolingMethod",1,54},{"bPreCullCheck",1,55},{"ReturnValue",8,56}}))return false;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto instruction=reinterpret_cast<const unsigned char*>(image+0x9a7352);
    if(*instruction!=0xe8||image+0x9a7357+AnimU::field<std::int32_t>(instruction,1)!=image+0x91f220)return false;
    constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x08,0x55,0x56,0x57,0x41,0x56,0x41,0x57,
        0x48,0x8d,0x6c,0x24,0xc9,0x48,0x81,0xec,0xd0,0x00,0x00,0x00};
    projectile_stock_niagara_location=reinterpret_cast<StockNiagaraLocation>(image+0x91f220);
    return native_install_exec(spawn,0x9a70a0,prefix,sizeof(prefix),projectile_effect_function,
                                   projectile_original_effect,&observe_projectile_effect);
}
static bool projectile_attachment_layout(){
    return medic_params(world_projectile_spawn_fx,64,{{"SystemTemplate",8,0},{"AttachToComponent",8,8},
        {"AttachPointName",8,16},{"Location",12,24},{"Rotation",12,36},{"LocationType",1,48},
        {"bAutoDestroy",1,49},{"bAutoActivate",1,50},{"PoolingMethod",1,51},{"bPreCullCheck",1,52},{"ReturnValue",8,56}});
}
static bool install_projectile_attachment_observer(){
    if(api.weakget(&projectile_attach_function))return true;
    const auto spawn=world_input(world_projectile_spawn_fx);
    if(!spawn||!projectile_attachment_layout())return false;
    constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x08,0x55,0x56,0x57,0x41,0x56,0x41,0x57,
        0x48,0x8d,0x6c,0x24,0xc9,0x48,0x81,0xec,0xb0,0x00,0x00,0x00};
    return native_install_exec(spawn,0x9a7380,prefix,sizeof(prefix),projectile_attach_function,
                               projectile_original_attach,&observe_projectile_attachment);
}
static bool projectile_cascade_layout(){
    return medic_params(world_projectile_cascade_attached,72,{{"EmitterTemplate",8,0},{"AttachToComponent",8,8},
        {"AttachPointName",8,16},{"Location",12,24},{"Rotation",12,36},{"Scale",12,48},
        {"LocationType",1,60},{"bAutoDestroy",1,61},{"PoolingMethod",1,62},{"bAutoActivate",1,63},{"ReturnValue",8,64}});
}
static void install_projectile_cascade_observers(){
    if(!api.weakget(&projectile_cascade_location)&&medic_params(world_projectile_cascade_location,64,
       {{"WorldContextObject",8,0},{"EmitterTemplate",8,8},{"Location",12,16},{"Rotation",12,28},
        {"Scale",12,40},{"bAutoDestroy",1,52},{"PoolingMethod",1,53},{"bAutoActivateSystem",1,54},{"ReturnValue",8,56}})){
        constexpr unsigned char prefix[]{0x40,0x55,0x53,0x56,0x57,0x41,0x56,0x48,0x8d,0x6c,0x24,0xc9,
            0x48,0x81,0xec,0xb0,0x00,0x00,0x00,0x33,0xff,0x4d,0x8b,0xf0};
        native_install_exec(world_input(world_projectile_cascade_location),0x2dc5220,prefix,sizeof(prefix),
            projectile_cascade_location,projectile_original_cascade_location,&observe_projectile_cascade_location);
    }
    if(!api.weakget(&projectile_cascade_attached)&&projectile_cascade_layout()){
        constexpr unsigned char prefix[]{0x40,0x55,0x53,0x56,0x57,0x41,0x56,0x48,0x8d,0x6c,0x24,0xc9,
            0x48,0x81,0xec,0xd0,0x00,0x00,0x00,0x33,0xff,0x4d,0x8b,0xf0};
        native_install_exec(world_input(world_projectile_cascade_attached),0x2dc54a0,prefix,sizeof(prefix),
            projectile_cascade_attached,projectile_original_cascade_attached,&observe_projectile_cascade_attached);
    }
}
static void install_projectile_activity_observers(){
    if(!api.weakget(&projectile_activate_function)&&medic_params(world_projectile_activate,1,{{"bReset",1,0}})){
        constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,
            0x83,0xec,0x20,0x33,0xff,0x48,0x8b,0xda,0x89,0x7c,0x24,0x30};
        native_install_exec(world_input(world_projectile_activate),0x2484f80,prefix,sizeof(prefix),
            projectile_activate_function,projectile_original_activate,&observe_projectile_activation);
    }
    if(!api.weakget(&projectile_deactivate_function)&&medic_params(world_projectile_deactivate,0,{})){
        constexpr unsigned char prefix[]{0x48,0x8b,0x42,0x20,0x45,0x33,0xc0,0x48,0x85,0xc0,0x41,0x0f,
            0x95,0xc0,0x4c,0x03,0xc0,0x4c,0x89,0x42,0x20,0x48,0x8b,0x01};
        native_install_exec(world_input(world_projectile_deactivate),0x1f31040,prefix,sizeof(prefix),
            projectile_deactivate_function,projectile_original_deactivate,&observe_projectile_deactivation);
    }
    if(!api.weakget(&projectile_set_active_function)&&native_bind_vm()&&
       medic_params(world_projectile_set_active,2,{{"bNewActive",1,0},{"bReset",1,1}})){
        constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x18,0x56,0x57,0x41,0x56,0x48,0x83,0xec,
            0x20,0x33,0xff,0x48,0x8b,0xda,0x89,0x7c,0x24,0x40,0x4c,0x8b};
        constexpr unsigned char dispatch[]{0xff,0x90,0x88,0x02,0x00,0x00};
        const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if(!std::memcmp(reinterpret_cast<void*>(image+0x2d5c7d0+0xbe),dispatch,sizeof(dispatch)))
            native_install_exec(world_input(world_projectile_set_active),0x2d5c7d0,prefix,sizeof(prefix),
                projectile_set_active_function,projectile_original_set_active,&observe_projectile_set_active);
    }
}
static bool projectile_actor_live(void* actor){
    if(!actor)return false;
    alignas(16) FactoryParameters args{};
    api.event(actor,getters[3].fn,args.data());
    if(api.getbool(getters[3].ret,args.data(),0))return false;
    const auto arc=world_input(world_projectile_arc);
    if(arc&&api.isa(actor,arc)){
        const auto field=catalog_field(AnimU::field<void*>(actor,16),"Destroying",1);
        if(medic_field_class(field,"BoolProperty")&&api.getbool(field,actor,0))return false;
    }
    return true;
}
static bool projectile_capture(void* actor,ProjectileSource& source,double time){
    auto& frame=source.frame;
    auto mesh=api.weakget(&source.mesh.component),trail=api.weakget(&source.trail.component);
    if(!mesh||!trail){
        projectile_bind_parts(actor,source,time);
        mesh=api.weakget(&source.mesh.component);
        trail=api.weakget(&source.trail.component);
    }
    frame.mesh_asset=0;
    if(mesh&&projectile_visible(mesh,actor))frame.mesh_asset=projectile_mesh_id(source.mesh,mesh);
    const auto trail_asset=projectile_trail_id(source.trail,trail);
    if(frame.trail_asset!=trail_asset)frame.parameters={};
    frame.trail_asset=trail_asset;
    frame.trail_active=frame.trail_asset&&projectile_trail_live(trail);
    frame.trail_hidden=frame.trail_active&&!projectile_visible(trail,actor);
    frame.trail=frame.trail_active?projectile_transform(trail):Projectile::Transform{};
    frame.mesh=frame.mesh_asset?projectile_transform(mesh):frame.trail;
    alignas(16) FactoryParameters args{};
    api.event(actor,getters[2].fn,args.data());
    frame.velocity=AnimU::field<std::array<float,3>>(args.data(),getters[2].at);
    return Projectile::valid(frame);
}
static void projectile_send(double time){
    install_projectile_observer();
    install_projectile_effect_observer();
    install_projectile_attachment_observer();
    install_projectile_cascade_observers();
    install_projectile_activity_observers();
    install_projectile_parameter_observers();
    install_projectile_asset_observer();
    for(auto& source:projectile_sources){
        auto& frame=source.frame;
        if(!frame.id||time<source.next)continue;
        if(source.retire_until&&time>=source.retire_until){
            source.actor={};
            frame={};
            continue;
        }
        source.next=time+.05;
        ++frame.sequence;
        if(frame.phase==Projectile::Phase::flying){
            const auto object=api.weakget(&source.actor);
            const bool effect=Projectile::effect(frame.profile);
            const auto age=time-source.first_seen;
            bool live{};
            if(effect){
                const bool draining=source.deactivate_until!=0.;
                if(draining)live=time<source.deactivate_until;
                // Retain a one-shot birth long enough for multiple UDP samples.
                else if(!source.following&&age<.35)live=true;
                else if(!source.detached&&(source.following||age<30))live=projectile_trail_live(object);
                if(live){
                    frame.trail_active=!draining;
                    if(object&&!source.detached)
                        frame.trail_hidden=!projectile_visible(object,api.weakget(&source.display_owner));
                    if(source.following&&object)frame.mesh=frame.trail=projectile_transform(object);
                }
            }else live=projectile_actor_live(object);
            if(!live||(!effect&&!projectile_capture(object,source,time))){
                if(!source.published){
                    if(live&&time-source.first_seen<.20)continue;
                    source.actor={};
                    frame={};
                    continue;
                }
                frame.phase=Projectile::Phase::retired;
                frame.mesh=frame.trail={};
                frame.velocity={};
                frame.trail_active=frame.trail_hidden=false;
                frame.mesh_asset=frame.trail_asset=0;
                frame.parameters={};
                source.retire_until=time+.55;
                ++projectile_retired;
            }
        }
        E::Packet packet{};
        if(Projectile::encode(envelope,frame,packet)&&send(packet,active_peer_mask)){
            source.published=true;
            ++projectile_sent;
            if(frame.trail_active)++projectile_trail_sent;
        }
    }
}
static bool receive_projectile(const ReceivedPacket& received,unsigned kind,double time){
    if(kind!=Projectile::kind)return false;
    Projectile::Frame frame;
    unsigned relay{};
    if(!automatic_limit||!active_peer_mask||!Projectile::decode(envelope,received.packet,frame)||
       !Projectile::accept_origin(frame,host_role,received.peer,active_peer_mask,relay)){
        ++projectile_rejected;
        return true;
    }
    auto& track=projectile_visuals[frame.slot][frame.id-1].track;
    const bool first_retirement=frame.phase==Projectile::Phase::retired&&
        (track.last.generation!=frame.generation||track.last.phase!=Projectile::Phase::retired);
    if(!track.push(frame,time))return true;
    ++projectile_received;
    if(frame.trail_active)++projectile_trail_received;
    if(first_retirement)++projectile_retired;
    E::Packet packet{};
    if(relay&&Projectile::encode(envelope,frame,packet)&&send(packet,relay))++projectile_relayed;
    return true;
}
static bool projectile_remove_trail(ProjectileVisual& visual){
    const bool had_trail=visual.trail_id!=0;
    if(auto trail=api.weakget(&visual.trail)){
        const auto destroy=world_input(world_destroy_component);
        if(!destroy||!medic_params(world_destroy_component,8,{{"Object",8,0}}))return false;
        api.event(trail,destroy,&trail);
        if(api.weakget(&visual.trail))return false;
    }
    visual.trail={};
    visual.trail_id=0;
    visual.parameters={};
    visual.trail_positioned=visual.trail_started=visual.trail_hidden=false;
    if(had_trail)++projectile_trail_removed;
    return true;
}
static bool projectile_remove(ProjectileVisual& visual){
    if(!visual.generation&&!api.weakget(&visual.actor))return true;
    if(!projectile_remove_trail(visual))return false;
    if(!destroy_owned_actor(visual.actor,false))return false;
    visual.actor={};
    visual.component={};
    visual.generation=0;
    visual.mesh_id=0;
    visual.positioned=false;
    ++projectile_removed;
    return true;
}
static void* native_visual_asset(NativeVisualAsset& entry,std::string_view path,void* type,double time){
    if(!type)return nullptr;
    if(const auto asset=api.weakget(&entry.asset))return asset;
    if(time<entry.retry)return nullptr;
    entry.retry=time+.5;
    const auto asset=load_player_asset(path,UINT32_MAX);
    return asset&&api.isa(asset,type)&&makeweak(entry.asset,asset)?asset:nullptr;
}
template<std::size_t Count>
static void* projectile_asset(std::array<NativeVisualAsset,Count>& cache,const std::array<std::string_view,Count>& paths,
                              std::uint16_t id,void* type,double time){
    return id&&id<=Count?native_visual_asset(cache[id-1],paths[id-1],type,time):nullptr;
}
static bool projectile_spawn(void* context,ProjectileVisual& visual,double time){
    if(time<visual.next_spawn)return false;
    visual.next_spawn=time+.15;
    const auto& frame=visual.track.last;
    const auto type=world_input(world_projectile_visual),mobility=world_input(world_projectile_mobility);
    const auto mesh=projectile_asset(projectile_meshes,Projectile::assets::meshes,
                                     frame.mesh_asset,world_input(world_static_asset),time);
    const auto mesh_set=world_input(world_set_static),collision=world_input(world_component_collision);
    const auto overlap=world_input(world_component_overlap),actor_collision=factory_object(factory_collision);
    if(!type||(frame.mesh_asset&&!mesh)||!actor_collision||!medic_params(world_projectile_mobility,1,{{"NewMobility",1,0}})||
       !medic_params(world_set_static,9,{{"NewMesh",8,0},{"ReturnValue",1,8}})||
       !medic_params(world_component_collision,1,{{"NewType",1,0}})||
       !medic_params(world_component_overlap,1,{{"bInGenerateOverlapEvents",1,0}}))return false;
    const auto actor=factory_spawn(context,type,frame.mesh.position,visual.actor,true);
    const auto component=actor?catalog_object_field(actor,"StaticMeshComponent"):nullptr;
    if(!component||!api.isa(component,world_input(world_static_component))||!makeweak(visual.component,component)){
        projectile_remove(visual);
        return false;
    }
    unsigned char none{},movable=2;
    api.event(actor,actor_collision,&none);
    api.event(component,collision,&none);
    api.event(component,overlap,&none);
    api.event(component,mobility,&movable);
    alignas(16) FactoryParameters args{};
    factory_put(args,0,mesh);
    api.event(component,mesh_set,args.data());
    if(catalog_object_field(component,"StaticMesh")!=mesh){
        projectile_remove(visual);
        return false;
    }
    visual.generation=frame.generation;
    visual.mesh_id=frame.mesh_asset;
    ++projectile_spawned;
    return true;
}
static void* projectile_trail_component(void* component,ProjectileVisual& visual,std::uint16_t id,double time){
    if(const auto trail=api.weakget(&visual.trail))return trail;
    if(time<visual.next_trail)return nullptr;
    visual.next_trail=time+.15;
    const bool cascade=Projectile::particle(visual.track.last.profile);
    const auto library=cascade?factory_object(factory_gameplay):world_input(world_projectile_fx_library);
    const auto spawn=world_input(cascade?world_projectile_cascade_attached:world_projectile_spawn_fx);
    const auto component_type=world_input(cascade?world_projectile_cascade_component:world_projectile_niagara);
    const auto system_type=world_input(cascade?world_projectile_cascade_asset:world_projectile_system);
    if(!library||!spawn||!component_type||!system_type||
       !(cascade?projectile_cascade_layout():projectile_attachment_layout()))return nullptr;
    const auto asset=cascade?projectile_asset(projectile_particles,Projectile::assets::particles,id,system_type,time):
        projectile_asset(projectile_trails,Projectile::assets::trails,id,system_type,time);
    if(!asset)return nullptr;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,asset);
    factory_put(args,8,component);
    // Unpooled, auto-destroy, activate after applying the world transform.
    if(cascade){
        factory_put(args,48,std::array{1.f,1.f,1.f});
        args[61]=1;
    }else args[49]=1;
    api.event(library,spawn,args.data());
    auto trail=AnimU::field<void*>(args.data(),cascade?64:56);
    if(!trail||!api.isa(trail,component_type))return nullptr;
    if(!makeweak(visual.trail,trail)){
        if(const auto destroy=world_input(world_destroy_component))api.event(trail,destroy,&trail);
        return nullptr;
    }
    visual.trail_positioned=visual.trail_started=visual.trail_hidden=false;
    visual.parameters={};
    visual.trail_id=id;
    ++projectile_trail_spawned;
    return trail;
}
static bool projectile_place(void* component,const Projectile::Transform& transform,const std::array<float,3>& velocity,
                             float elapsed,std::array<float,12>& old,bool& positioned){
    std::array<float,12> pose{};
    std::copy(transform.rotation.begin(),transform.rotation.end(),pose.begin());
    for(unsigned axis=0;axis<3;++axis){
        pose[axis+4]=transform.position[axis]+velocity[axis]*elapsed;
        pose[axis+8]=transform.scale[axis];
    }
    if(positioned&&old==pose)return false;
    alignas(16) FactoryParameters args{};
    std::memcpy(args.data(),pose.data(),sizeof(pose));
    args[188]=1;
    api.event(component,mesh_fn,args.data());
    old=pose;
    positioned=true;
    return true;
}
static void projectile_display_trail(void* component,ProjectileVisual& visual,const Projectile::Frame& frame,
                                     double time,float elapsed){
    if(visual.trail_id&&visual.trail_id!=frame.trail_asset&&!projectile_remove_trail(visual))return;
    const bool effect=Projectile::effect(frame.profile);
    auto trail=api.weakget(&visual.trail);
    if(!trail){
        if(!frame.trail_active){visual.trail_started=false;return;}
        if(effect&&visual.trail_started)return;
        trail=projectile_trail_component(component,visual,frame.trail_asset,time);
    }
    if(!trail)return;
    if(frame.trail_active||effect){
        projectile_place(trail,frame.trail,frame.velocity,elapsed,visual.trail_pose,visual.trail_positioned);
        if(visual.trail_hidden!=frame.trail_hidden){
            if(!medic_params(world_life_visibility,2,{{"bNewVisibility",1,0},{"bPropagateToChildren",1,1}}))return;
            unsigned char visibility[]{static_cast<unsigned char>(!frame.trail_hidden),0};
            api.event(trail,world_input(world_life_visibility),visibility);
            visual.trail_hidden=frame.trail_hidden;
        }
    }
    projectile_apply_parameters(trail,visual,frame);
    if(visual.trail_started==frame.trail_active)return;
    if(!frame.trail_active){
        const auto deactivate=world_input(world_projectile_deactivate);
        if(!deactivate||*api.parms(deactivate)!=0)return;
        api.event(trail,deactivate,nullptr);
        visual.trail_started=false;
        return;
    }
    if(medic_params(world_projectile_activate,1,{{"bReset",1,0}})){
        unsigned char reset=1;
        api.event(trail,world_input(world_projectile_activate),&reset);
        visual.trail_started=true;
    }
}
static void projectile_display(void* context,double time){
    for(auto& owner:projectile_visuals)for(auto& visual:owner){
        const auto& frame=visual.track.last;
        if((visual.generation!=frame.generation||visual.mesh_id!=frame.mesh_asset||!visual.track.visible(time))&&
           !projectile_remove(visual))continue;
        if(!visual.track.visible(time))continue;
        if(!api.weakget(&visual.actor)&&!projectile_spawn(context,visual,time))continue;
        const auto actor=api.weakget(&visual.actor),component=api.weakget(&visual.component);
        if(!actor||!component)continue;
        const auto elapsed=static_cast<float>(std::clamp(time-visual.track.received,0.,.055));
        const bool first=!visual.positioned;
        if(projectile_place(component,frame.mesh,frame.velocity,elapsed,visual.pose,visual.positioned))++projectile_moves;
        if(first)own_set(actor,hide_fn,false);
        projectile_display_trail(component,visual,frame,time,elapsed);
    }
}
static void native_projectile_step(void* context,double time){
    if(!factory_configured||!active_peer_mask||!envelope.world)return;
    projectile_send(time);
    projectile_display(context,time);
}
static bool native_restore_exec(Weak& key,const std::atomic<KillExec>& original,KillExec observer){
    if(const auto fn=api.weakget(&key)){
        auto cell=reinterpret_cast<void* volatile*>(static_cast<unsigned char*>(fn)+216);
        const auto hook=reinterpret_cast<void*>(observer);
        const auto stock=reinterpret_cast<void*>(original.load(std::memory_order_acquire));
        const auto previous=InterlockedCompareExchangePointer(cell,stock,hook);
        if(previous!=hook&&previous!=stock)return false;
    }
    key={};
    return true;
}
static bool native_projectile_cleanup(){
    bool clean=native_restore_exec(projectile_hook_function,projectile_original_finish,&observe_projectile_spawn);
    if(!native_restore_exec(projectile_effect_function,projectile_original_effect,&observe_projectile_effect))clean=false;
    if(!native_restore_exec(projectile_attach_function,projectile_original_attach,&observe_projectile_attachment))clean=false;
    if(!native_restore_exec(projectile_cascade_location,projectile_original_cascade_location,&observe_projectile_cascade_location))clean=false;
    if(!native_restore_exec(projectile_cascade_attached,projectile_original_cascade_attached,&observe_projectile_cascade_attached))clean=false;
    if(!native_restore_exec(projectile_activate_function,projectile_original_activate,&observe_projectile_activation))clean=false;
    if(!native_restore_exec(projectile_deactivate_function,projectile_original_deactivate,&observe_projectile_deactivation))clean=false;
    if(!native_restore_exec(projectile_set_active_function,projectile_original_set_active,&observe_projectile_set_active))clean=false;
    for(auto& hook:projectile_parameter_hooks)
        if(!native_restore_exec(hook.function,hook.original,hook.observer))clean=false;
    if(!native_restore_exec(projectile_set_asset_function,projectile_original_set_asset,&observe_projectile_set_asset))clean=false;
    for(auto& owner:projectile_visuals)for(auto& visual:owner)if(!projectile_remove(visual))clean=false;
    for(auto& effect:suppressed_effects)if(!projectile_restore_effect(effect))clean=false;
    return clean;
}
static void native_projectile_reset(){
    projectile_sources={};
    projectile_visuals={};
    projectile_meshes={};
    projectile_trails={};
    projectile_particles={};
    projectile_parameter_names={};
    projectile_parameter_ids={};
    suppressed_effects={};
    player_projectile_class=enemy_projectile_base={};
}
