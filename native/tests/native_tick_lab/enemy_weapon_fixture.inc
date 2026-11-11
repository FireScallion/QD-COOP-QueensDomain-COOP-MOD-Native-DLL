static bool read_test_hitbox(Address actor){
    const auto mesh=object_field(actor,"Mesh"),capsule=object_field(actor,"CapsuleComponent");
    const auto mode=resolve(hitbox_test_methods[0]),response=resolve(hitbox_test_methods[1]),tick=resolve(hitbox_test_methods[2]);
    if(!mesh||!capsule||!mode||!response||!tick||at<std::uint16_t>(mode,182)!=1||
       at<std::uint16_t>(response,182)!=2||at<std::uint16_t>(tick,182)!=1)return false;
    std::uint8_t mesh_mode{},capsule_mode{},actor_tick{},type{};
    std::uint8_t trace[]{17,0},pawn[]{2,0};
    event(mesh,mode,&mesh_mode);
    event(capsule,mode,&capsule_mode);
    event(actor,tick,&actor_tick);
    event(mesh,response,trace);
    event(mesh,response,pawn);
    event(mesh,resolve(hitbox_test_methods[8]),&type);
    static std::array<std::uint8_t,6> last{255};
    const std::array state{type,mesh_mode,capsule_mode,actor_tick,trace[1],pawn[1]};
    if(state!=last){
        last=state;
        char report[160]{};
        const auto count=std::snprintf(report,sizeof(report),"object=%u mesh=%u capsule=%u tick=%u trace=%u pawn=%u\n",
                                       type,mesh_mode,capsule_mode,actor_tick,trace[1],pawn[1]);
        if(count>0)write_file(marker(role_index,L"weapon-collision.txt").c_str(),report,static_cast<unsigned>(count));
    }
    return mesh_mode==1&&capsule_mode==0&&!actor_tick&&trace[1]>0&&trace[1]<=2&&!pawn[1];
}
static bool send_stock_attack_key(Address pawn,unsigned index){
    const auto fn=resolve(hitbox_test_methods[index]),string_name=resolve(hitbox_test_methods[5]),strings=resolve(hitbox_test_methods[6]);
    if(!fn||!string_name||!strings||at<std::uint16_t>(string_name,182)!=24)return false;
    const auto size=at<std::uint16_t>(fn,182);
    const auto key=field_of(fn,"Key",size);
    if(size<8||size>64||!damage_probe_struct(key)||at<int>(key,76)!=0)return false;
    const auto name=field_of(at<Address>(key,0x78),"KeyName",8);
    if(!name||at<int>(name,76)!=0)return false;
    constexpr wchar_t button[]=L"LeftMouseButton";
    struct String {const wchar_t* value;int count,capacity;};
    alignas(16) Parameters convert{};
    const String text{button,std::size(button),std::size(button)};
    std::memcpy(convert.data(),&text,sizeof(text));
    event(strings,string_name,convert.data());
    const auto name_key=U::field<U::NameKey>(convert.data(),16);
    if(!name_key.index)return false;
    const auto init=reinterpret_cast<void(*)(Address,void*)>(image+0x11914f0);
    const auto destroy=reinterpret_cast<void(*)(Address,void*)>(image+0x118e4c0);
    alignas(16) Parameters args{};
    init(key,args.data());
    std::memcpy(args.data(),&name_key,sizeof(name_key));
    event(pawn,fn,args.data());
    destroy(key,args.data());
    return true;
}
static void probe_weapon_trace(Address pawn,Address enemy,unsigned action){
    const auto fn=resolve(hitbox_test_methods[9]),loc=resolve(hitbox_test_methods[10]);
    const auto shape=object_field(pawn,"CurrentActiveHitbox");
    const auto hits=field_of(fn,"OutHits",16);
    const auto inner=hits?at<Address>(hits,0x78):0;
    const auto actor=inner?field_of(at<Address>(inner,0x78),"Actor",8):0;
    if(!shape||!actor||at<int>(inner,60)!=136||at<std::uint16_t>(loc,182)!=12)return;
    alignas(16) Parameters args{};
    if(!put(args,fn,"TraceChannel",std::uint8_t{2})||!put(args,fn,"Target",shape))return;
    event(pawn,fn,args.data());
    struct Hits {Address base;int count,capacity;};
    const auto found=U::field<Hits>(args.data(),at<int>(hits,76));
    unsigned matching{};
    if(found.base&&found.count>0&&found.count<=1024&&found.capacity>=found.count){
        for(int i=0;i<found.count;++i){
            const auto key=at<U::WeakKey>(found.base+i*136,at<int>(actor,76));
            if(resolve(key)==enemy)++matching;
        }
    }
    if(found.base)reinterpret_cast<void(*)(void*)>(image+0xeae0b0)(reinterpret_cast<void*>(found.base));
    const auto from=location(pawn),to=location(enemy);
    std::array<float,3> hitbox{};
    event(shape,loc,hitbox.data());
    char report[300]{};
    const auto size=std::snprintf(report,sizeof(report),"action=%u hits=%d enemy=%u pawn=%.2f,%.2f,%.2f target=%.2f,%.2f,%.2f hitbox=%.2f,%.2f,%.2f\n",
        action,found.count,matching,from[0],from[1],from[2],to[0],to[1],to[2],hitbox[0],hitbox[1],hitbox[2]);
    if(size>0)write_file(marker(role_index,(L"weapon-trace-"+std::to_wstring(action)+L".txt").c_str()).c_str(),report,static_cast<unsigned>(size));
}
static bool run_enemy_weapon_stage(ULONGLONG time){
    static float initial{};
    static unsigned actions{};
    static ULONGLONG next{};
    static TestPlayerPose aim{};
    E::NativeBindingView binding{};
    const auto pawn=local_test_pawn();
    if(!pawn||!core.inspect_binding(1,&binding)||!binding.confirmed)return false;
    const auto target=resolve(binding.actor);
    const auto health=target?field_of(at<Address>(target,16),"HP",4):0;
    if(!health)return false;
    const float hp=at<float>(target,at<int>(health,76));
    if(!initial){
        if(!std::isfinite(hp)||hp<=1||(!host&&!read_test_hitbox(target)))return false;
        initial=hp;
        write_file(marker(role_index,L"weapon-ready.txt").c_str(),"ok",2);
    }
    if(!all_player_markers(L"weapon-ready.txt"))return false;
    if(hp<initial){
        char report[104]{};
        const auto count=std::snprintf(report,sizeof(report),"generation=%u hp_before=%.3f hp_after=%.3f actions=%u\n",
                                       binding.generation,initial,hp,actions);
        if(count>0)write_file(marker(role_index,L"weapon-hit.txt").c_str(),report,static_cast<unsigned>(count));
        return all_player_markers(L"weapon-hit.txt");
    }
    if(role_index!=(world_host_melee_lab?0u:1u)||actions>=8||time<next)return false;
    if(!actions){
        const auto enable_actor=resolve(methods[actor_tick]),enable_collision=resolve(methods[actor_collision]);
        const auto enable_component=resolve(methods[component_tick]);
        const auto movement=object_field(pawn,"CharacterMovement"),mesh=object_field(pawn,"Mesh");
        const auto visibility=mesh?field_of(at<Address>(mesh,16),"VisibilityBasedAnimTickOption",1):0;
        if(!enable_actor||!enable_collision||!enable_component||!movement||!mesh||!visibility)return false;
        // The pose replication fixture freezes its local pawn before entering
        // this stage. Restore stock input, movement and offscreen bone updates.
        std::uint8_t enabled=1;
        event(pawn,enable_actor,&enabled);
        event(pawn,enable_collision,&enabled);
        event(movement,enable_component,&enabled);
        event(mesh,enable_component,&enabled);
        *reinterpret_cast<std::uint8_t*>(mesh+at<int>(visibility,76))=0;
    }
    if(actions%2==0){
        const auto from=location(pawn),to=location(target);
        const auto dx=to[0]-from[0],dy=to[1]-from[1];
        const auto distance=std::hypot(dx,dy);
        if(distance<1.f)return false;
        constexpr float reach[]{75.f,105.f,45.f,145.f};
        const auto range=reach[actions/2];
        aim={{to[0]-dx/distance*range,to[1]-dy/distance*range,to[2]+3.f},std::atan2(dy,dx)*57.2957795f};
    }
    const auto controller=object_field(pawn,"Controller"),rotate=resolve(hitbox_test_methods[7]);
    auto direction=std::array<float,3>{0,aim.yaw,0};
    if(!controller||!rotate||at<std::uint16_t>(rotate,182)!=12||!apply_test_pose(pawn,aim))return false;
    held_pose=aim;
    event(controller,rotate,direction.data());
    if(!send_stock_attack_key(pawn,3+actions%2))return false;
    ++actions;
    probe_weapon_trace(pawn,target,actions);
    next=time+(actions%2?160:850);
    const auto pressed=field_of(at<Address>(pawn,16),"AttackPressed",1);
    const bool is_pressed=pressed&&U::get_bool(reinterpret_cast<void*>(pressed),reinterpret_cast<void*>(pawn),0);
    const auto active=object_field(pawn,"CurrentActiveHitbox");
    char report[150]{};
    const auto count=std::snprintf(report,sizeof(report),"actions=%u yaw=%.3f hp=%.3f key=%u hitbox=%u\n",
                                   actions,aim.yaw,hp,static_cast<unsigned>(is_pressed),static_cast<unsigned>(active!=0));
    if(count>0)write_file(marker(role_index,L"weapon-input.txt").c_str(),report,static_cast<unsigned>(count));
    return false;
}
