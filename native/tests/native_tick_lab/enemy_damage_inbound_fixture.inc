static constexpr unsigned enemy_damage_recipient=2;
static bool probe_damageable_registration(){
    static unsigned found{};
    if(found==7)return true;
    scan([&](Address object){
        const auto leaf=name(object);
        const unsigned bit=leaf=="RegisterDamageableActor"?1u:leaf=="UnregisterDamageableActor"?2u:leaf=="GetGameState"?4u:0u;
        if(!bit||(found&bit))return;
        std::string path;
        if(!U::object_path(object,image+0x4a27f00,read_local,path)||!path.starts_with("/Script/"))return;
        if(bit==4){
            if(path!="/Script/Engine.GameplayStatics.GetGameState"||at<std::uint16_t>(object,182)!=16)return;
            std::array<Address,2> args{local_test_pawn(),0};
            if(!args[0])return;
            event(resolve(gameplay),object,args.data());
            std::string state="none";
            if(args[1])U::object_path(at<Address>(args[1],16),image+0x4a27f00,read_local,state);
            write_file(marker(role_index,L"enemy-proxy-game-state.txt").c_str(),state.data(),static_cast<unsigned>(state.size()));
            found|=bit;
            return;
        }
        found|=bit;
        path+=" parms="+std::to_string(at<std::uint16_t>(object,182))+"\n";
        damage_layout_fields(path,object,true);
        write_file(marker(role_index,bit==1?L"enemy-proxy-register.txt":L"enemy-proxy-unregister.txt").c_str(),
                   path.data(),static_cast<unsigned>(path.size()));
    });
    return found==7;
}
static Address enemy_damage_proxy_at(const TestPlayerPose& pose){
    Address found{};
    scan([&](Address object){
        if(found||(at<unsigned>(object,8)&0x10u))return;
        const auto type=at<Address>(object,16);
        if(name(type)!="QDCoopGuestTarget_C"||!exact_path(type,"/Game/QDCoop/QDCoopGuestTarget.QDCoopGuestTarget_C"))return;
        const auto point=location(object);
        if(std::abs(point[0]-pose.position[0])<.02f&&std::abs(point[1]-pose.position[1])<.02f&&
           std::abs(point[2]-pose.position[2])<.02f)found=object;
    });
    return found;
}
static bool run_enemy_damage_inbound(){
    static float initial{};
    static bool fired{};
    E::NativeBindingView binding{};
    const auto pawn=local_test_pawn();
    if(!pawn||!core.inspect_binding(1,&binding)||!binding.confirmed)return false;
    const auto source=resolve(binding.actor),health=field_of(at<Address>(pawn,16),"Health",4);
    if(!source||!health)return false;
    const auto hp=at<float>(pawn,at<int>(health,76));
    if(!initial){
        if(!std::isfinite(hp)||hp<12)return false;
        initial=hp;
        write_file(marker(role_index,L"enemy-damage-ready.txt").c_str(),"ok",2);
    }
    if(!all_player_markers(L"enemy-damage-ready.txt"))return false;
    if(host&&!fired){
        if(!probe_damageable_registration())return false;
        if(at<Address>(image+0x4a62e10,0x27*8)==image+0x11b9d20)return false;
        TestPlayerPose peer_pose{};
        if(!read_expected_pose(enemy_damage_recipient,1,peer_pose))return false;
        const auto proxy=enemy_damage_proxy_at(peer_pose);
        if(!proxy)return false;
        const auto mesh=object_field(proxy,"Mesh"),capsule=object_field(proxy,"CapsuleComponent");
        const auto mode=resolve(hitbox_test_methods[0]),response=resolve(hitbox_test_methods[1]);
        if(!mesh||!capsule||!mode||!response||at<std::uint16_t>(mode,182)!=1||at<std::uint16_t>(response,182)!=2||
           query_byte(mesh,mode)!=0||query_byte(capsule,mode)!=1)return false;
        std::uint8_t pawn_channel[]{2,0},camera[]{4,0};
        event(capsule,response,pawn_channel);
        event(capsule,response,camera);
        if(pawn_channel[1]!=1||camera[1]!=0)return false;
        write_file(marker(role_index,L"enemy-proxy-query.txt").c_str(),"query-only",10);
        const auto fn=test_world_method(at<Address>(proxy,16),"TryApplyDamage");
        if(!fn||at<int>(fn,104)!=3)return false;
        float effective{};
        std::uint8_t accepted{};
        if(!damage_test_event(source,proxy,fn,4.f,effective,accepted))return false;
        fired=true;
        if(!accepted){phase=95;return false;}
        write_file(marker(role_index,L"enemy-damage-fired.txt").c_str(),"accepted",8);
    }
    if(!exists(marker(enemy_damage_recipient,L"enemy-damage-hit.txt"))&&role_index!=enemy_damage_recipient)return false;
    if(role_index==enemy_damage_recipient?hp>=initial:hp!=initial)return false;
    char report[150]{};
    const auto size=std::snprintf(report,sizeof(report),"generation=%u receiver=%u before=%.3f after=%.3f\n",
                                  binding.generation,enemy_damage_recipient,initial,hp);
    if(size>0)write_file(marker(role_index,L"enemy-damage-hit.txt").c_str(),report,static_cast<unsigned>(size));
    return all_player_markers(L"enemy-damage-hit.txt");
}
static bool verify_stale_enemy_proxy(){
    TestPlayerPose peer_pose{};
    if(!read_expected_pose(silent_role,1,peer_pose))return false;
    const auto proxy=enemy_damage_proxy_at(peer_pose);
    const auto type=proxy?at<Address>(proxy,16):0;
    const auto health=field_of(type,"Health",4),dead=field_of(type,"IsDead",1);
    const auto collision=resolve(ownership_keys[1]);
    if(!proxy||!health||!dead||!collision||at<float>(proxy,at<int>(health,76))!=0.f||
       !U::get_bool(reinterpret_cast<void*>(dead),reinterpret_cast<void*>(proxy),0)||query_byte(proxy,collision))return false;
    write_file(marker(role_index,L"enemy-proxy-stale.txt").c_str(),"inert",5);
    return true;
}
