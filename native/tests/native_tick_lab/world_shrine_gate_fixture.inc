static bool run_world_shrine_gate_stage(){
    static unsigned stage{};
    static bool commanded{};
    static std::array<std::array<float,6>,2> closed{};
    constexpr std::array markers{L"shrine-gate-ready.txt",L"shrine-gate-open.txt",L"shrine-gate-closed.txt"};
    if(stage==markers.size())return all_player_markers(markers.back());
    const auto gate=resolve(shrine_test_gate);
    if(!gate)return false;
    const auto type=at<Address>(gate,16),timeline=object_field(gate,"Timeline_0"),box=object_field(gate,"ClosedBox");
    if(!timeline||!box)return false;
    const auto at_fn=test_world_method(at<Address>(timeline,16),"GetPlaybackPosition"),
               end_fn=test_world_method(at<Address>(timeline,16),"GetTimelineLength"),
               collision_fn=test_world_method(at<Address>(box,16),"GetCollisionEnabled");
    const auto opened=field_of(type,"Opened",1),activation=field_of(type,"CurrentActivationCount",4);
    if(!at_fn||!end_fn||!collision_fn||!opened||!activation||!parameter(at_fn,"ReturnValue",0,4)||
       !parameter(end_fn,"ReturnValue",0,4)||!parameter(collision_fn,"ReturnValue",0,1))return false;
    const auto wanted=stage==1;
    if(stage&&!all_player_markers(markers[stage-1]))return false;
    const bool stock_open=U::get_bool(reinterpret_cast<void*>(opened),reinterpret_cast<void*>(gate),0);
    if(stage&&host&&!commanded){
        const auto fn=test_world_method(type,wanted?"OpenDoor":"CloseDoor");
        if(!fn||at<std::uint16_t>(fn,182))return false;
        const int count=wanted?2:1;
        std::memcpy(reinterpret_cast<void*>(gate+at<int>(activation,76)),&count,sizeof(count));
        event(gate,fn,nullptr);
        commanded=true;
        return false;
    }
    float at_time{},end_time{};
    std::uint8_t collision{};
    event(timeline,at_fn,&at_time);
    event(timeline,end_fn,&end_time);
    event(box,collision_fn,&collision);
    const auto count=at<int>(gate,at<int>(activation,76));
    std::array<std::array<float,6>,2> shape{};
    constexpr std::array mesh_names{"DoorMesh","OppositeDoorMesh"};
    for(unsigned i=0;i<mesh_names.size();++i){
        const auto mesh=object_field(gate,mesh_names[i]);
        if(!mesh)return false;
        const auto mesh_type=at<Address>(mesh,16);
        const auto location=field_of(mesh_type,"RelativeLocation",12),rotation=field_of(mesh_type,"RelativeRotation",12);
        if(!location||!rotation)return false;
        std::memcpy(shape[i].data(),reinterpret_cast<const void*>(mesh+at<int>(location,76)),12);
        std::memcpy(shape[i].data()+3,reinterpret_cast<const void*>(mesh+at<int>(rotation,76)),12);
    }
    if(end_time<=0.f)return false;
    if(!stage)closed=shape;
    float motion{};
    for(unsigned mesh=0;mesh<shape.size();++mesh)
        for(unsigned axis=0;axis<shape[mesh].size();++axis){
            const auto delta=shape[mesh][axis]-closed[mesh][axis];
            motion=std::max(motion,std::abs(axis<3?delta:std::remainder(delta,360.f)));
        }
    char report[256]{};
    const auto length=std::snprintf(report,sizeof(report),
        "stage=%u timeline=%.4f end=%.4f box_collision=%u motion=%.4f opened_flag=%u count=%d\n",
        stage,at_time,end_time,static_cast<unsigned>(collision),motion,stock_open,count);
    if(length>0)write_file(marker(role_index,L"shrine-gate-progress.txt").c_str(),report,static_cast<unsigned>(length));
    if(wanted?(at_time<end_time-.001f||collision||motion<10.f||count!=2):
              (at_time>.001f||collision!=3||motion>.2f||count!=static_cast<int>(stage==2)))return false;
    if(length>0)write_file(marker(role_index,markers[stage]).c_str(),report,static_cast<unsigned>(length));
    ++stage;
    commanded=false;
    return false;
}
