struct ArenaFixtureActor {
    bool hidden{},collision{};
    bool operator==(const ArenaFixtureActor&) const=default;
};
static bool arena_fixture_read(Address actor,ArenaFixtureActor& value){
    const auto get=test_world_method(at<Address>(actor,16),"GetActorEnableCollision");
    const auto hidden=library_fixture_bool(actor,"bHidden");
    if(!get||!hidden||!parameter(get,"ReturnValue",0,1))return false;
    std::uint8_t collision=255;
    event(actor,get,&collision);
    value={*hidden,collision!=0};
    return collision<=1;
}
static bool arena_fixture_write(Address actor,const ArenaFixtureActor& value){
    const auto type=at<Address>(actor,16),hide=test_world_method(type,"SetActorHiddenInGame"),
               set=test_world_method(type,"SetActorEnableCollision");
    if(!hide||!set||!parameter(hide,"bNewHidden",0,1)||!parameter(set,"bNewActorEnableCollision",0,1))return false;
    std::uint8_t hidden=value.hidden,collision=value.collision;
    event(actor,hide,&hidden);
    event(actor,set,&collision);
    return true;
}
static bool arena_fixture_material(Address actor,bool hidden,float& radius,std::array<float,3>& center){
    const auto mesh=object_field(actor,"StaticMeshComponent");
    if(!mesh)return false;
    const auto type=at<Address>(mesh,16),bounds=test_world_method(type,"GetLocalBounds"),
               scale=test_world_method(type,"K2_GetComponentScale"),position=test_world_method(type,"K2_GetComponentLocation");
    const auto data=field_of(type,"CustomPrimitiveDataInternal",16);
    if(!bounds||!scale||!position||!data||!parameter(bounds,"Min",0,12)||!parameter(bounds,"Max",12,12)||
       !parameter(scale,"ReturnValue",0,12)||!parameter(position,"ReturnValue",0,12))return false;
    std::array<float,6> range{};
    std::array<float,3> factor{};
    event(mesh,bounds,range.data());
    event(mesh,scale,factor.data());
    event(mesh,position,center.data());
    float squared{};
    for(unsigned i=0;i<center.size();++i){
        const auto delta=(range[i+3]-range[i])*factor[i];
        squared+=delta*delta;
    }
    const auto expected=hidden?0.f:(std::sqrt(squared)*.5f+2000.f)*1.2f;
    struct Floats {const float* data;int count,capacity;};
    const auto actual=at<Floats>(mesh,at<int>(data,76));
    if(!actual.data||actual.count<35||actual.capacity<actual.count)return false;
    radius=actual.data[7];
    return std::abs(radius-expected)<.01f&&std::equal(center.begin(),center.end(),actual.data+32);
}
static bool run_world_arena_stage(Address engine){
    static constexpr std::array paths{
        "/Game/RuinsLevel/Maps/300_LIB.300_LIB.PersistentLevel.NecroBossDoor_SM",
        "/Game/RuinsLevel/Maps/300_LIB.300_LIB.PersistentLevel.Lib_BossDoor_BP"
    };
    constexpr std::array proofs{L"arena-ready.txt",L"arena-changed.txt",L"arena-masked.txt",L"arena-restored.txt"};
    static std::array<U::WeakKey,2> keys{};
    static std::array<ArenaFixtureActor,2> initial{},changed{};
    static BarrierFixtureState initial_part,changed_part,masked_part;
    static bool streaming{},positioned{},show_requested{},commanded{};
    static unsigned stage{};
    if(stage==proofs.size())return all_player_markers(proofs.back());
    const auto gi=object_field(engine,"GameInstance"),pawn=resolve(held_pawn);
    if(!gi||!pawn)return false;
    if(host&&!streaming){streaming=library_fixture_stream(engine,gi);return false;}
    if(!resolve(fixture_stream_lookup)||std::ranges::any_of(keys,[](auto key){return !resolve(key);})){
        scan([](Address object){
            const auto leaf=name(object);
            if(leaf=="GetStreamingLevel"&&exact_path(object,"/Script/Engine.GameplayStatics.GetStreamingLevel"))
                remember(fixture_stream_lookup,object);
            for(unsigned i=0;i<paths.size();++i){
                const std::string_view path=paths[i];
                if(leaf==path.substr(path.rfind('.')+1)&&exact_path(object,path.data()))remember(keys[i],object);
            }
        });
        return false;
    }
    if(!positioned){
        held_pose.position={23595.43f+3.f*role_index,1674.266f,10941.553f};
        positioned=apply_test_pose(pawn,held_pose);
        return false;
    }
    if(!fixture_visible_stream(gi,L"/Game/RuinsLevel/Maps/300_LIB",show_requested)||
       (stage&&!all_player_markers(proofs[stage-1])))return false;
    const std::array actors{resolve(keys[0]),resolve(keys[1])};
    const auto component=object_field(actors[1],"InteractBox");
    std::array<ArenaFixtureActor,2> actual{};
    BarrierFixtureState actual_part;
    if(!arena_fixture_read(actors[0],actual[0])||!arena_fixture_read(actors[1],actual[1])||
       !read_fixture_barrier(component,actual_part))return false;
    const bool restore=stage==proofs.size()-1;
    const auto& wanted=restore?initial:changed;
    const auto& wanted_part=restore?initial_part:stage==2?masked_part:changed_part;
    if(stage&&host&&!commanded){
        if(stage==2){
            if(!arena_fixture_write(actors[1],{wanted[1].hidden,true})||!write_fixture_barrier(component,wanted_part))return false;
        }
        for(unsigned i=0;i<actors.size();++i)if(!arena_fixture_write(actors[i],wanted[i]))return false;
        if(stage!=2&&!write_fixture_barrier(component,wanted_part))return false;
        commanded=true;
        return false;
    }
    float radius{};
    std::array<float,3> center{};
    const bool material=host||arena_fixture_material(actors[0],actual[0].hidden,radius,center);
    char report[320]{};
    const auto get_effective=test_world_method(at<Address>(component,16),"GetCollisionEnabled");
    std::uint8_t effective=255;
    if(!get_effective||!parameter(get_effective,"ReturnValue",0,1))return false;
    event(component,get_effective,&effective);
    if(effective!=(actual[1].collision?actual_part.collision:0))return false;
    const auto size=std::snprintf(report,sizeof(report),
        "stage=%u necro_hidden=%u necro_collision=%u library_hidden=%u library_collision=%u box_mode=%u effective=%u box_hidden=%u box_visible=%u box_x=%.3f material=%u radius=%.3f center=%.3f,%.3f,%.3f\n",
        stage,actual[0].hidden,actual[0].collision,actual[1].hidden,actual[1].collision,actual_part.collision,
        effective,actual_part.hidden,actual_part.visible,
        actual_part.transform[4],material,radius,center[0],center[1],center[2]);
    if(size>0)write_file(marker(role_index,L"arena-progress.txt").c_str(),report,static_cast<unsigned>(size));
    if(!material)return false;
    if(stage==0){
        initial=actual;
        for(unsigned i=0;i<actual.size();++i)changed[i]={!actual[i].hidden,!actual[i].collision};
        initial_part=changed_part=actual_part;
        changed_part.transform[4]+=32.f;
        changed_part.collision=3;
        changed_part.visible=!actual_part.visible;
        masked_part=changed_part;
        masked_part.collision=0;
    }else{
        if(actual!=wanted||!same_fixture_barrier(actual_part,wanted_part)){
            char difference[480]{};
            const auto size=std::snprintf(difference,sizeof(difference),
                "actor_match=%u actual flags=%u,%u,%u transform=%.5f,%.5f,%.5f,%.5f/%.5f,%.5f,%.5f/%.5f,%.5f,%.5f expected flags=%u,%u,%u transform=%.5f,%.5f,%.5f,%.5f/%.5f,%.5f,%.5f/%.5f,%.5f,%.5f\n",
                actual==wanted,actual_part.hidden,actual_part.visible,actual_part.collision,
                actual_part.transform[0],actual_part.transform[1],actual_part.transform[2],actual_part.transform[3],
                actual_part.transform[4],actual_part.transform[5],actual_part.transform[6],
                actual_part.transform[8],actual_part.transform[9],actual_part.transform[10],
                wanted_part.hidden,wanted_part.visible,wanted_part.collision,
                wanted_part.transform[0],wanted_part.transform[1],wanted_part.transform[2],wanted_part.transform[3],
                wanted_part.transform[4],wanted_part.transform[5],wanted_part.transform[6],
                wanted_part.transform[8],wanted_part.transform[9],wanted_part.transform[10]);
            if(size>0)write_file(marker(role_index,L"arena-difference.txt").c_str(),difference,static_cast<unsigned>(size));
            return false;
        }
    }
    if(size>0)write_file(marker(role_index,proofs[stage]).c_str(),report,static_cast<unsigned>(size));
    ++stage;
    commanded=false;
    return false;
}
