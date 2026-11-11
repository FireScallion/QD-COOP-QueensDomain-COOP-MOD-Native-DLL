static bool blocked_threat_point(Address enemy,Address proxy,TestPlayerPose& result){
    const auto trace=resolve(threat_memory_test_methods[2]),kismet=resolve(threat_memory_test_methods[3]);
    const auto capsule=object_field(enemy,"CapsuleComponent");
    const auto viewpoint=field_of(at<Address>(enemy,16),"LocalViewpoint",48);
    if(!trace||!kismet||!capsule||!viewpoint)return false;
    alignas(16) Parameters transform_args{};
    event(capsule,metadata.arguments[16],transform_args.data());
    const auto transform=U::field<std::array<float,12>>(transform_args.data(),at<int>(metadata.arguments[17],76));
    const auto offset=at<std::array<float,3>>(enemy,at<int>(viewpoint,76)+16);
    const float x=offset[0]*transform[8],y=offset[1]*transform[9],z=offset[2]*transform[10];
    const float qx=transform[0],qy=transform[1],qz=transform[2],w=transform[3];
    const float dot=2.f*(qx*x+qy*y+qz*z),factor=w*w-(qx*qx+qy*qy+qz*qz);
    const std::array<float,3> start{transform[4]+dot*qx+factor*x+2.f*w*(qy*z-qz*y),
                                   transform[5]+dot*qy+factor*y+2.f*w*(qz*x-qx*z),
                                   transform[6]+dot*qz+factor*z+2.f*w*(qx*y-qy*x)};
    const auto center=location(enemy);
    const auto returned=parameter_offset(trace,"ReturnValue",1);
    std::array<Address,3> ignored{enemy,proxy,local_test_pawn()};
    struct Actors {const Address* data;int count,capacity;};
    if(returned<0||returned>=256)return false;
    for(float distance:{350.f,550.f,750.f})for(unsigned bearing=0;bearing<8;++bearing){
        const auto yaw=bearing*45.f;
        const auto angle=yaw*.01745329252f;
        TestPlayerPose test{{center[0]+distance*std::cos(angle),center[1]+distance*std::sin(angle),center[2]+3.f},
                             std::remainder(yaw+180.f,360.f)};
        alignas(16) Parameters args{};
        if(!put(args,trace,"WorldContextObject",enemy)||!put(args,trace,"Start",start)||!put(args,trace,"End",test.position)||
           !put(args,trace,"bTraceComplex",std::uint8_t{1})||!put(args,trace,"bIgnoreSelf",std::uint8_t{1})||
           !put(args,trace,"ActorsToIgnore",Actors{ignored.data(),3,3}))return false;
        event(kismet,trace,args.data());
        if(args[returned]){result=test;return true;}
    }
    return false;
}
static void probe_threat_memory_layout(Address actor){
    const auto target=field_of(at<Address>(actor,16),"TargetPlayer",8);
    std::string report;
    for(unsigned i=0;i<2;++i){
        const auto fn=resolve(threat_memory_test_methods[i]);
        const auto code=fn?at<Address>(fn,96):0;
        const auto size=fn?at<int>(fn,104):0;
        if(!code||size<1||size>256)continue;
        report+=std::string(threat_memory_test_paths[i])+" size="+std::to_string(size)+" property sites=";
        for(int index=0;index+9<=size;++index)if(at<unsigned char>(code,index)==0x01&&at<Address>(code,index+1)==target)
            report+=std::to_string(index)+",";
        report+="\n";
    }
    if(!report.empty())write_file(marker(0,L"threat-memory-layout.txt").c_str(),report.data(),static_cast<unsigned>(report.size()));
}
static bool run_threat_memory_stage(Address pawn,Address target){
    static bool occluded{},read_memory{};
    static U::WeakKey marker_key;
    if(role_index==enemy_damage_recipient&&!occluded){
        TestPlayerPose point{};
        if(!read_threat_pose(0,L"threat-occluded.bin",point))return false;
        held_pose=point;
        if(!apply_test_pose(pawn,held_pose))return false;
        write_file(marker(role_index,L"threat-occluded-applied.txt").c_str(),"ok",2);
        occluded=true;
    }
    if(host&&!occluded){
        probe_threat_memory_layout(target);
        TestPlayerPose point{};
        if(!blocked_threat_point(target,object_field(target,"TargetPlayer"),point))return false;
        write_file(marker(0,L"threat-occluded.bin").c_str(),reinterpret_cast<const char*>(&point),sizeof(point));
        occluded=true;
    }
    if(host&&!read_memory){
        const auto chase=resolve(threat_memory_test_methods[0]),seen=resolve(threat_memory_test_methods[1]);
        if(!chase||!seen||at<std::uint16_t>(chase,182)!=8||at<std::uint16_t>(seen,182)!=12)return false;
        Address actor{};
        event(target,chase,&actor);
        if(!actor||!exact_path(at<Address>(actor,16),"/Script/Engine.SkeletalMeshActor"))return false;
        std::array<float,3> point{};
        event(target,seen,point.data());
        const auto where=location(actor);
        TestPlayerPose previous{};
        if(!read_threat_pose(enemy_damage_recipient,L"threat-approach.bin",previous))return false;
        for(unsigned axis=0;axis<3;++axis)
            if(std::abs(where[axis]-previous.position[axis])>.2f||std::abs(point[axis]-where[axis])>.02f)return false;
        remember(marker_key,actor);
        write_file(marker(0,L"threat-memory-used.txt").c_str(),"chase and last seen",19);
        read_memory=true;
    }
    if(host&&read_memory&&fixture_actor_gone(marker_key)&&at<Address>(image+0x4a62e10,0x01*8)==image+0x11b5e60)
        write_file(marker(0,L"threat-memory-expired.txt").c_str(),"retired",7);
    return exists(marker(0,L"threat-memory-expired.txt"));
}
