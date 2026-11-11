static bool fixture_actor_gone(U::WeakKey key){
    const auto actor=resolve(key);
    if(!actor)return true;
    alignas(16) Parameters args{};
    event(actor,metadata.arguments[14],args.data());
    return U::get_bool(reinterpret_cast<void*>(metadata.arguments[15]),args.data(),0);
}

static bool enroll_replacement_host(){
    std::array<E::NativeCandidate,E::capacity> candidates{};
    const auto context=resolve(player);
    if(!context)return false;
    const auto count=enumerate_fixture_candidates(context,candidates);
    if(count<0)return false;
    for(int i=0;i<count;++i){
        const auto& candidate=candidates[i];
        if(candidate.class_id!=first_offer.class_id||candidate.mesh_id!=first_offer.mesh_id||
           candidate.hp!=host_hp||
           (candidate.actor.index==first_world_enemy.index&&candidate.actor.serial==first_world_enemy.serial))
            continue;
        const auto actor=resolve(candidate.actor);
        if(!actor||!U::object_path(actor,image+0x4A27F00,read_local,reuse_path)||reuse_path==enemy_path)continue;
        old_generation_rejected=!core.enroll(1,1,&candidate.actor);
        if(!old_generation_rejected||!core.enroll(1,2,&candidate.actor)||
           core.bind_catalog_animation(1,2)!=fixture_profile)return false;
        offer={1,2,candidate.class_id,candidate.mesh_id,candidate.location};
        enemy=candidate.actor;
        reuse_hp=candidate.hp;
        return true;
    }
    return false;
}

static void host_retirement_step(const char* status){
    const auto acks=counter(status,"\"retire_acked\":");
    if(!retirement_notice_written&&counter(status,"\"retire_sent\":")){
        write_file(retirement_notice_marker.c_str(),"published",9);
        retirement_notice_written=true;
    }
    if(!first_clean){
        if(acks<client_count)return;
        if(counter(status,"\"death_pending\":")||!enroll_replacement_host()){phase=54;return;}
        first_clean=reuse_enrolled=true;
        return;
    }
    if(!reuse_retire_requested){
        if(!peers_reuse_verified)return;
        if(counter(status,"\"enroll_acked\":")!=2*client_count||!core.retire(1,2)){phase=55;return;}
        reuse_verified=reuse_retire_requested=true;
        return;
    }
    if(acks!=2*client_count||!peer_finished)return;
    const auto actor=resolve(enemy);
    const auto hp_offset=at<int>(metadata.arguments[2],76);
    natural_reuse_alive=actor&&!fixture_actor_gone(enemy)&&
        at<float>(actor,static_cast<unsigned>(hp_offset))>0.f;
    if(!natural_reuse_alive){phase=56;return;}
    second_clean=passed=true;
    phase=3;
}

static bool spawn_replacement_guest(){
    if(!core.read_offer(1,&offer))return false;
    if(offer.generation!=2){phase=57;return false;}
    const auto context=resolve(player);
    Address actor{},mesh{};
    if(!context||!spawn_offered_replica(context,actor,mesh))return false;
    reuse_start_x=location(actor)[0];
    if(!U::object_path(actor,image+0x4A27F00,read_local,reuse_path)||reuse_path==enemy_path){
        phase=57;
        return false;
    }
    const auto hp_offset=at<int>(metadata.arguments[2],76);
    const float marker=client_hp+20.f;
    std::memcpy(reinterpret_cast<void*>(actor+hp_offset),&marker,sizeof(marker));
    if(!core.take_ownership(1,2,nullptr,0)||core.bind_catalog_animation(1,2)!=fixture_profile||
       !core.confirm_replica(1,2)){
        phase=57;
        return false;
    }
    return true;
}

static void guest_retirement_step(const char* status){
    const auto applied=counter(status,"\"retire_applied\":");
    if(!first_clean){
        if(!applied)return;
        if(applied!=1||!fixture_actor_gone(enemy)||!fixture_actor_gone(display)){phase=58;return;}
        first_clean=true;
    }
    if(!reuse_enrolled){
        reuse_enrolled=spawn_replacement_guest();
        return;
    }
    if(!reuse_verified){
        const auto actor=resolve(enemy);
        if(!actor){phase=59;return;}
        const auto hp_offset=at<int>(metadata.arguments[2],76);
        reuse_hp=at<float>(actor,static_cast<unsigned>(hp_offset));
        if(reuse_hp!=host_hp||std::abs(location(actor)[0]-reuse_start_x)<1.f)return;
        reuse_verified=true;
        write_file(reuse_verified_marker.c_str(),"verified",8);
        return;
    }
    if(applied!=2)return;
    if(!fixture_actor_gone(enemy)||!fixture_actor_gone(display)){phase=60;return;}
    write_file(final_clean_marker.c_str(),"verified",8);
    second_clean=passed=true;
    phase=3;
}

static void retirement_fixture_step(){
    if(active_ticks%8)return;
    const auto status=read_native_status(core.status);
    if(host)host_retirement_step(status.c_str());
    else guest_retirement_step(status.c_str());
}
