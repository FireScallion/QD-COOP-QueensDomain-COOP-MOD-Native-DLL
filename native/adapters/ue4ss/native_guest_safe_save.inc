struct GuestSafeSaveState {
    using Destroy=void(*)(void*,void*);
    void *levels_property{},*music_property{};
    Destroy destroy_levels{},destroy_music{};
    alignas(16) std::array<WorldArray,2> levels{};
    alignas(16) WorldArray music{};
    ~GuestSafeSaveState(){
        destroy_music(music_property,&music);
        destroy_levels(levels_property,levels.data());
    }
};

static void* guest_safe_save(void* pawn,void* active,void* outgoing){
    if(!world_progress.applied.number||world_progress.fault||api.weakget(&world_progress.save)!=active||
       world_stream.scripted_world||world_stream.hold.serial||
       world_progress.authoritative.arrival.serial>world_stream.arrived||persistent_travel.load.target.serial||
       rollback_busy()||local_life.actor!=pawn||local_life.locked||local_life.pending||
       local_life.life.phase!=LifePhase::standing||!local_life.layout.dead||api.getbool(local_life.layout.dead,pawn,0))return nullptr;
    const auto gi=api.weakget(&medic_runtime.save.gi);
    const auto time=now();
    if(!bind_world_stream(pawn,time)||!bind_world_music(gi,time)||world_stream_teleporting(pawn))return nullptr;
    const auto manager=api.weakget(&world_stream.binding.manager),music=api.weakget(&world_music.controller);
    if(!music||!world_stream_idle(manager,gi))return nullptr;
    if(!world_input(world_guest_save_levels)||!world_input(world_guest_save_music)||!world_input(world_travel_create_save))
        scan_world_inputs(4096);
    const auto levels_fn=world_input(world_guest_save_levels),music_fn=world_input(world_guest_save_music),
               make=world_input(world_travel_create_save),gameplay=factory_object(factory_gameplay);
    if(!gameplay||!levels_fn||!music_fn||!make||AnimU::field<int>(levels_fn,104)||AnimU::field<int>(music_fn,104)||
       !medic_params(world_guest_save_levels,32,{{"ReturnValue",32,0}})||
       !medic_params(world_guest_save_music,16,{{"ReturnValue",16,0}})||
       !medic_params(world_travel_create_save,16,{{"SaveGameClass",8,0},{"ReturnValue",8,8}}))return nullptr;

    const auto pawn_type=AnimU::field<void*>(pawn,16),save_type=AnimU::field<void*>(outgoing,16);
    const auto midair=damage_field(pawn_type,"SafeLocationIsForcedMidair",1,5214,"BoolProperty");
    const auto suspend=damage_field(save_type,"IsSuspendSave",1,2728,"BoolProperty");
    const auto forced=damage_field(save_type,"SuspendPositionIsForcedMidair",1,2672,"BoolProperty");
    if(!midair||!suspend||!forced||api.getbool(midair,pawn,0)||api.getbool(suspend,outgoing,0))return nullptr;
    const auto safe=damage_field(pawn_type,"LastSafeLocation",48,3312,"StructProperty");
    const auto saved=damage_field(save_type,"SaveLocation",48,864,"StructProperty");
    const auto suspended=damage_field(save_type,"SuspendLocation",48,2624,"StructProperty");
    for(const auto property:{safe,saved,suspended})
        if(!damage_struct_property(property)||!damage_type_path(property,"/Script/CoreUObject.Transform"))return nullptr;
    alignas(16) const auto safe_transform=AnimU::field<std::array<float,12>>(pawn,3312);
    for(unsigned i:{0u,1u,2u,3u,4u,5u,6u,8u,9u,10u})
        if(!std::isfinite(safe_transform[i]))return nullptr;

    const auto level_return=catalog_field(levels_fn,"ReturnValue",32);
    const auto level_save=damage_field(save_type,"LevelLoadingSuspendState",32,2680,"StructProperty");
    if(!damage_struct_property(level_return)||!damage_struct_property(level_save))return nullptr;
    const auto level_type=AnimU::field<void*>(world_stream.binding.payload,0x78);
    if(AnimU::field<void*>(level_return,0x78)!=level_type||AnimU::field<void*>(level_save,0x78)!=level_type)return nullptr;
    constexpr auto music_data="/Script/QueensDomain.FS_MusicAreaInstance_SuspendData";
    const auto music_return=world_actor_array(music_fn,"ReturnValue",16,music_data);
    const auto music_save=world_actor_array(save_type,"MusicAreaSuspendState",16,music_data);
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(!music_return.field||!music_save.field||music_return.offset!=0||music_save.offset!=2712)return nullptr;
    for(const auto field:{music_return.field,music_save.field}){
        const auto vtable=AnimU::field<const std::uintptr_t*>(field,0);
        if(!vtable||vtable[23]!=image+0x116fa40||vtable[30]!=image+0x11703e0)return nullptr;
    }

    SC::Layout layout;
    if(!SC::bind(image,catalog_name_pool,reinterpret_cast<std::uintptr_t>(save_type),catalog_read,layout))return nullptr;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,save_type);
    api.event(gameplay,make,args.data());
    const auto clone=AnimU::field<void*>(args.data(),8);
    if(!SC::copy(layout,clone,outgoing))return nullptr;

    GuestSafeSaveState state{level_return,music_return.field,
        reinterpret_cast<GuestSafeSaveState::Destroy>(image+0x118e4c0),
        reinterpret_cast<GuestSafeSaveState::Destroy>(image+0x11703e0)};
    api.event(manager,levels_fn,state.levels.data());
    api.event(music,music_fn,&state.music);
    if(!std::ranges::all_of(state.levels,[](auto array){return array.valid(WP::max_stream_entries);})||
       !state.music.valid(4096))return nullptr;
    const auto set=[&](void* property,const void* value){
        const auto copy=AnimU::field<const SC::CopyValue*>(property,0)[23];
        copy(property,static_cast<unsigned char*>(clone)+*api.offset(property),value,1);
    };
    set(saved,&safe_transform);
    set(suspended,&safe_transform);
    set(level_save,state.levels.data());
    set(music_save.field,&state.music);
    api.setbool(forced,clone,false,0);
    api.setbool(suspend,clone,true,0);
    return clone;
}
