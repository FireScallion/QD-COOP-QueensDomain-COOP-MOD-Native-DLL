struct BarrierFixtureState {
    std::array<float,12> transform{};
    bool hidden{},visible{};
    std::uint8_t collision{};
};
static bool read_fixture_barrier(Address component,BarrierFixtureState& value){
    if(!component)return false;
    const auto type=at<Address>(component,16);
    const auto get=test_world_method(type,"GetRelativeTransform");
    const auto hidden=library_fixture_bool(component,"bHiddenInGame");
    const auto visible=library_fixture_bool(component,"bVisible");
    const auto body=U::find_inherited_property(type,image+0x4a27f00,"BodyInstance",read_local);
    if(!get||!hidden||!visible||!body||world_map_property_type(body)!="StructProperty"||!parameter(get,"ReturnValue",0,48))return false;
    const auto body_type=at<Address>(body,0x78);
    const auto collision=body_type?field_of(body_type,"CollisionEnabled",1):0;
    if(!collision||world_map_property_type(collision)!="ByteProperty")return false;
    alignas(16) Parameters args{};
    event(component,get,args.data());
    value.transform=U::field<std::array<float,12>>(args.data(),0);
    value.hidden=*hidden;
    value.visible=*visible;
    value.collision=at<std::uint8_t>(component,at<int>(body,76)+at<int>(collision,76));
    return value.collision<=3;
}
static bool write_fixture_barrier(Address component,const BarrierFixtureState& value){
    const auto type=at<Address>(component,16);
    const auto set=test_world_method(type,"K2_SetRelativeTransform"),hide=test_world_method(type,"SetHiddenInGame"),
               show=test_world_method(type,"SetVisibility"),collision=test_world_method(type,"SetCollisionEnabled");
    if(!set||!hide||!show||!collision||!parameter(hide,"NewHidden",0,1)||
       !parameter(show,"bNewVisibility",0,1)||!parameter(collision,"NewType",0,1))return false;
    const auto transform=parameter_offset(set,"NewTransform",48),teleport=parameter_offset(set,"bTeleport",1);
    alignas(16) Parameters args{};
    if(transform<0||teleport<0||at<std::uint16_t>(set,182)>args.size())return false;
    std::memcpy(args.data()+transform,value.transform.data(),48);
    args[teleport]=1;
    event(component,set,args.data());
    std::uint8_t hidden[]{static_cast<std::uint8_t>(value.hidden),0},visible[]{static_cast<std::uint8_t>(value.visible),0};
    auto mode=value.collision;
    event(component,hide,hidden);
    event(component,show,visible);
    event(component,collision,&mode);
    return true;
}
static bool same_fixture_barrier(const BarrierFixtureState& actual,const BarrierFixtureState& expected){
    if(actual.hidden!=expected.hidden||actual.visible!=expected.visible||actual.collision!=expected.collision)return false;
    float dot{};
    for(unsigned i=0;i<4;++i)dot+=actual.transform[i]*expected.transform[i];
    const auto sign=dot<0?-1.f:1.f;
    for(unsigned i=0;i<4;++i)
        if(std::abs(actual.transform[i]-sign*expected.transform[i])>.005f)return false;
    for(const auto i:{4u,5u,6u,8u,9u,10u})
        if(std::abs(actual.transform[i]-expected.transform[i])>.005f)return false;
    return true;
}
static bool run_world_barrier_stage(){
    static unsigned stage{};
    static bool changed{};
    static std::array<BarrierFixtureState,2> initial{},moved{};
    constexpr std::array proofs{L"barrier-ready.txt",L"barrier-changed.txt",L"barrier-restored.txt"};
    if(stage==proofs.size())return all_player_markers(proofs.back());
    if(stage&&!all_player_markers(proofs[stage-1]))return false;
    std::array<Address,2> components{};
    std::array<BarrierFixtureState,2> actual{};
    for(unsigned i=0;i<components.size();++i){
        const auto actor=resolve(castle_fixture_doors[i]);
        if(!actor)return false;
        components[i]=object_field(actor,i?"InteractBox":"BlockerBox");
        if(!read_fixture_barrier(components[i],actual[i]))return false;
        CastleFixtureDoor door{};
        if(!castle_fixture_door(i,door)||door.playing||std::abs(door.relative)>.01f||std::abs(door.position)>.001f)return false;
    }
    if(stage==0){
        initial=actual;
        moved=initial;
        for(unsigned i=0;i<moved.size();++i){
            moved[i].collision=initial[i].collision==3?0:3;
            moved[i].hidden=!initial[i].hidden;
            moved[i].visible=!initial[i].visible;
            moved[i].transform[4]+=42.f+5.f*i;
            moved[i].transform[8]*=1.2f;
        }
    }else{
        const auto& wanted=stage==1?moved:initial;
        if(host&&!changed){
            for(unsigned i=0;i<components.size();++i)if(!write_fixture_barrier(components[i],wanted[i]))return false;
            changed=true;
            return false;
        }
        for(unsigned i=0;i<actual.size();++i)if(!same_fixture_barrier(actual[i],wanted[i]))return false;
    }
    char report[240]{};
    const auto size=std::snprintf(report,sizeof(report),
        "stage=%u bridge mode=%u hidden=%u visible=%u x=%.3f scale=%.3f front mode=%u hidden=%u visible=%u x=%.3f scale=%.3f meshes_closed=1\n",
        stage,actual[0].collision,actual[0].hidden,actual[0].visible,actual[0].transform[4],actual[0].transform[8],
        actual[1].collision,actual[1].hidden,actual[1].visible,actual[1].transform[4],actual[1].transform[8]);
    if(size>0)write_file(marker(role_index,proofs[stage]).c_str(),report,static_cast<unsigned>(size));
    ++stage;
    changed=false;
    return false;
}
