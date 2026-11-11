using FxVector=std::array<float,3>;
struct NiagaraLocationInput {
    void* world{};
    void* system{};
    FxVector position{},rotation{},scale{};
    unsigned char auto_destroy{},auto_activate{},pooling{},pre_cull{};
};
static_assert(sizeof(NiagaraLocationInput)==56&&offsetof(NiagaraLocationInput,auto_activate)==53);
using StockNiagaraLocation=void*(*)(void*,void*,FxVector,FxVector,FxVector,bool,bool,unsigned char,bool);
static StockNiagaraLocation projectile_stock_niagara_location;

static void projectile_point_fallback(const NiagaraLocationInput& args,void* effect){
    if(!args.auto_activate||!args.system||
       !Projectile::bounded(args.position,10000000.f)||!Projectile::bounded(args.rotation,10000000.f)||
       !Projectile::bounded(args.scale,1000.f))return;
    const auto caller=native_cosmetic_actor(args.world);
    const auto profile=projectile_effect_profile(caller,false,false);
    const auto system=world_input(world_projectile_system),math=world_input(world_projectile_math);
    const auto component=world_input(world_projectile_niagara);
    const auto transform=world_input(world_projectile_make_transform);
    if(profile==Projectile::Profile::none||!system||!api.isa(args.system,system)||!math||!transform||
       (effect&&(!component||!api.isa(effect,component)))||
       !medic_params(world_projectile_make_transform,96,{{"Location",12,0},{"Rotation",12,12},
        {"Scale",12,24},{"ReturnValue",48,48}}))return;
    ProjectilePart asset;
    if(!projectile_known_asset(asset,args.system,Projectile::assets::trails,Projectile::assets::player_trails))return;
    alignas(16) FactoryParameters pose{};
    factory_put(pose,0,args.position);
    factory_put(pose,12,args.rotation);
    factory_put(pose,24,args.scale);
    api.event(math,transform,pose.data());
    if(projectile_record_effect(caller,effect,profile,false,asset,projectile_read_transform(pose.data(),48)))
        ++projectile_fallback_births;
}
static void observe_projectile_effect(void* context,void* frame,void* result){
    const auto original=projectile_original_effect.load(std::memory_order_acquire);
    if(GetCurrentThreadId()!=game_thread.load()||!active_peer_mask||!frame||!result){
        original(context,frame,result);
        return;
    }
    NiagaraLocationInput args;
    auto& code=*reinterpret_cast<const unsigned char**>(static_cast<unsigned char*>(frame)+32);
    if(code){
        unsigned auto_destroy{},auto_activate{},pre_cull{};
        for(auto field:{static_cast<void*>(&args.world),static_cast<void*>(&args.system),static_cast<void*>(&args.position),
                        static_cast<void*>(&args.rotation),static_cast<void*>(&args.scale),static_cast<void*>(&auto_destroy),
                        static_cast<void*>(&auto_activate),static_cast<void*>(&args.pooling),static_cast<void*>(&pre_cull)})
            native_vm_argument(frame,field);
        if(code)++code;
        args.auto_activate=auto_activate!=0;
        const auto component=projectile_stock_niagara_location(args.world,args.system,args.position,args.rotation,args.scale,
            auto_destroy!=0,args.auto_activate!=0,args.pooling,pre_cull!=0);
        std::memcpy(result,&component,sizeof(component));
    }else{
        if(AnimU::field<void*>(frame,16)==api.weakget(&projectile_effect_function))
            if(const auto params=AnimU::field<const void*>(frame,40))std::memcpy(&args,params,sizeof(args));
        original(context,frame,result);
    }
    const auto effect=AnimU::field<void*>(result,0);
    if(!projectile_observe_effect(args.world,effect,false))projectile_point_fallback(args,effect);
}
