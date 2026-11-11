static bool describe_fixture_actor(Address actor){
    if(!actor||!U::object_path(actor,image+0x4A27F00,read_local,enemy_path))return false;
    position_before=location(actor);
    const auto hp_offset=at<int>(metadata.arguments[2],76);
    hp_before=at<float>(actor,static_cast<unsigned>(hp_offset));
    return true;
}

static int enumerate_fixture_candidates(Address context,std::array<E::NativeCandidate,E::capacity>& candidates){
    LARGE_INTEGER begin{},end{};
    QueryPerformanceCounter(&begin);
    const auto count=core.enumerate(reinterpret_cast<void*>(context),candidates.data(),
                                    static_cast<unsigned>(candidates.size()));
    QueryPerformanceCounter(&end);
    ++enum_probe_count;
    enum_max_us=std::max(enum_max_us,static_cast<double>(end.QuadPart-begin.QuadPart)*counter_us);
    if(count<0){phase=44;return -1;}
    native_candidates=static_cast<unsigned>(count);
    return count;
}

static bool prepare_natural_host(Address context){
    static ULONGLONG next_enum;
    const auto now=GetTickCount64();
    if(now<next_enum)return false;
    next_enum=now+500;
    std::array<E::NativeCandidate,E::capacity> candidates{};
    const auto count=enumerate_fixture_candidates(context,candidates);
    if(count<0)return false;
    if(enum_probe_count<3)return false;
    for(int i=0;i<count;++i){
        const auto& candidate=candidates[i];
        if(E::catalog::classes[candidate.class_id]!=fixture_path||candidate.hp!=host_hp)continue;
        const auto actor=resolve(candidate.actor);
        const auto mesh=actor?object_field(actor,"Mesh"):0;
        if(!mesh||!describe_fixture_actor(actor))continue;
        auto stale=candidate.actor;
        stale.serial=0;
        if(core.enroll(1,1,&stale)){phase=45;return false;}
        if(!core.enroll(1,1,&candidate.actor))continue;
        remember(enemy,actor);
        if(!quiet_natural_enemy(actor)||!bind_fixture_animation(mesh)){phase=38;return false;}
        offer={1,1,candidate.class_id,candidate.mesh_id,candidate.location};
        first_offer=offer;
        first_world_enemy=enemy;
        for(int j=0;j<count;++j){
            const auto& other=candidates[j];
            if(j!=i&&other.class_id==candidate.class_id&&other.mesh_id==candidate.mesh_id){
                reuse_candidate=other.actor;
                break;
            }
        }
        if(!resolve(reuse_candidate)){phase=51;return false;}
        native_natural=native_enrolled=true;
        return true;
    }
    phase=45;
    return false;
}

static bool spawn_offered_replica(Address context,Address& actor,Address& mesh){
    if(!resolve(offer_class)||!resolve(offer_mesh)){
        scan([](Address object){
            const auto leaf=name(object);
            const auto class_path=E::catalog::classes[offer.class_id];
            const auto mesh_path=E::catalog::meshes[offer.mesh_id];
            if(leaf==class_path.substr(class_path.find_last_of('.')+1)&&name(at<Address>(object,16))=="BlueprintGeneratedClass"&&
               exact_path(object,class_path.data()))remember(offer_class,object);
            if(leaf==mesh_path.substr(mesh_path.find_last_of('.')+1)&&name(at<Address>(object,16))=="SkeletalMesh"&&
               exact_path(object,mesh_path.data()))remember(offer_mesh,object);
        });
    }
    const auto type=resolve(offer_class),asset=resolve(offer_mesh);
    if(!type||!asset)return false;
    auto position=offer.location;
    position[0]-=40.f+20.f*static_cast<float>(role_index);
    const auto default_mode=field_of(type,"DefaultEnemyMode",1);
    actor=spawn_actor(context,type,transform_at(position),default_mode);
    if(!actor){phase=46;return false;}
    mesh=object_field(actor,"Mesh");
    if(!mesh){phase=46;return false;}
    if(object_field(mesh,"SkeletalMesh")!=asset){
        alignas(16) Parameters args{};
        const auto fn=resolve(methods[set_mesh]);
        if(!put(args,fn,"NewMesh",asset)||!put(args,fn,"bReinitPose",std::uint8_t{1})){
            phase=46;
            return false;
        }
        event(mesh,fn,args.data());
    }
    const auto actor_ptr=reinterpret_cast<void*>(actor),mesh_ptr=reinterpret_cast<void*>(mesh);
    if(offer.generation==1){
        auto stale=offer;
        ++stale.generation;
        stale_offer_rejected=!core.register_replica(&stale,actor_ptr,mesh_ptr);
        if(!stale_offer_rejected){phase=47;return false;}
    }else{
        old_generation_rejected=!core.register_replica(&first_offer,actor_ptr,mesh_ptr);
        if(!old_generation_rejected){phase=47;return false;}
    }
    if(!core.register_replica(&offer,actor_ptr,mesh_ptr)||
       !bind_fixture_display(context,actor,mesh)){
        phase=47;
        return false;
    }
    remember(enemy,actor);
    return true;
}

static bool prepare_offered_replica(Address context){
    if(!core.read_offer(1,&offer))return false;
    if(!offer_seen_at)offer_seen_at=GetTickCount64();
    Address actor{},mesh{};
    if(offer.generation!=1||!spawn_offered_replica(context,actor,mesh))return false;
    first_offer=offer;
    const auto hp_offset=at<int>(metadata.arguments[2],76);
    const auto marker=client_hp+7.25f*static_cast<float>(role_index-1);
    std::memcpy(reinterpret_cast<void*>(actor+hp_offset),&marker,sizeof(marker));
    if(!describe_fixture_actor(actor)||!fixture_ownership(actor,mesh)||!bind_fixture_animation(mesh)){
        phase=48;
        return false;
    }
    native_enrolled=true;
    return true;
}
