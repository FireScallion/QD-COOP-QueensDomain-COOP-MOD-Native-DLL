struct WorldSpecialSpec {
    unsigned type;
    std::string_view flag,action;
};
static constexpr std::array world_special_specs{
    WorldSpecialSpec{world_special_secret,"Opened","ForceOpen"},
    WorldSpecialSpec{world_special_dagger,"AlreadyHit","Hit"},
    WorldSpecialSpec{world_special_gargoyle,"Lit","Activate"}
};
struct WorldSpecialActor {
    WP::Guid key;
    Weak actor;
    bool synced{},presented_active{};
    double animated_at{};
};
struct WorldSpecialBinding {
    Weak type,action,finish;
    MedicMap guids;
    void *active{},*running{};
    int started{-1},button_end{-1};
    std::vector<WorldSpecialActor> actors;
};
static std::array<WorldSpecialBinding,world_special_specs.size()> world_specials;
static double world_special_next_scan{};
static NativeVisualAsset world_dagger_sparks,world_gargoyle_sound;
static std::array<AnimU::NameKey,3> world_dagger_scalars;
static void world_special_reset(){
    world_specials={};
    world_special_next_scan=0.;
    world_dagger_sparks={};
    world_gargoyle_sound={};
    world_dagger_scalars={};
}
static bool world_special_bind(unsigned i,void* type){
    auto& binding=world_specials[i];
    if(type&&api.weakget(&binding.type)==type&&binding.active)return true;
    if(!type)return false;
    const auto& spec=world_special_specs[i];
    WorldSpecialBinding found{};
    found.guids=world_actor_array(type,"GUIDs",16,"/Script/CoreUObject.Guid");
    found.active=catalog_field(type,spec.flag,1);
    if(!found.guids.field||!medic_field_class(found.active,"BoolProperty")||
       !world_mechanism_method(type,spec.action,found.action)||!makeweak(found.type,type))return false;
    if(i==0){
        found.running=catalog_field(type,"AnimRunning",1);
        const auto start=catalog_field(type,"AnimStartTime",4);
        if(!medic_field_class(found.running,"BoolProperty")||!medic_field_class(start,"FloatProperty")||
           !world_mechanism_method(type,"SetToFinalState",found.finish))return false;
        found.started=*api.offset(start);
    }else if(i==1){
        const auto endpoint=catalog_field(type,"ButtonPushinPos",4);
        if(!medic_field_class(endpoint,"FloatProperty"))return false;
        found.button_end=*api.offset(endpoint);
    }else if(!world_mechanism_method(type,"Timeline_0__FinishedFunc",found.finish))return false;
    binding=std::move(found);
    return true;
}
static void world_special_scan(void* pawn,double time){
    if(time<world_special_next_scan||!native_get_actors||!native_free)return;
    world_special_next_scan=time+1.;
    for(unsigned i=0;i<world_specials.size();++i){
        const auto type=world_input(world_special_specs[i].type);
        if(!world_special_bind(i,type))continue;
        auto& binding=world_specials[i];
        auto previous=std::move(binding.actors);
        binding.actors.clear();
        NativeActorArray entries{};
        native_get_actors(pawn,type,&entries);
        if(entries.data&&entries.count>=0&&entries.count<=8192&&entries.capacity>=entries.count)
            for(int j=0;j<entries.count;++j){
                const auto actor=entries.data[j];
                if(!actor||!api.isa(actor,type)||actor_is_destroying(actor))continue;
                const auto keys=AnimU::field<WorldArray>(actor,binding.guids.offset);
                if(!keys.valid(32)||!keys.count)continue;
                const auto key=*static_cast<const WP::Guid*>(keys.data);
                if(key==WP::Guid{}||WP::personal(key))continue;
                const auto old=std::ranges::lower_bound(previous,key,{},&WorldSpecialActor::key);
                if(old!=previous.end()&&old->key==key&&api.weakget(&old->actor)==actor)binding.actors.push_back(*old);
                else {
                    WorldSpecialActor found{key,{}};
                    if(makeweak(found.actor,actor))binding.actors.push_back(found);
                }
            }
        if(entries.data)native_free(entries.data);
        std::ranges::sort(binding.actors,{},&WorldSpecialActor::key);
        for(auto it=binding.actors.begin();it!=binding.actors.end();){
            const auto end=std::upper_bound(it,binding.actors.end(),it->key,
                [](const auto& key,const auto& entry){return key<entry.key;});
            it=end==it+1?end:binding.actors.erase(it,end);
        }
    }
}
static void world_special_capture(void* pawn,double time,std::vector<WP::MechanismState>& out){
    world_special_scan(pawn,time);
    for(unsigned i=0;i<world_specials.size();++i)
        for(const auto& entry:world_specials[i].actors)
            if(const auto actor=api.weakget(&entry.actor);actor&&out.size()<WP::max_mechanisms)
                out.push_back({entry.key,static_cast<WP::MechanismKind>(static_cast<unsigned>(WP::MechanismKind::secret)+i),
                    0,api.getbool(world_specials[i].active,actor,0)});
}
static void world_special_observe(void* save,double time){
    for(unsigned i=0;i<world_specials.size();++i)
        for(const auto& entry:world_specials[i].actors)
            if(const auto actor=api.weakget(&entry.actor);actor&&api.getbool(world_specials[i].active,actor,0))
                world_interaction_queue(save,entry.key,
                    static_cast<WorldIntent::Action>(static_cast<unsigned>(WorldIntent::Action::secret)+i),time);
}
static bool world_special_accept_guest(const WorldIntent::Message& message,const LifePeer& peer){
    const auto i=static_cast<unsigned>(message.action)-static_cast<unsigned>(WorldIntent::Action::secret);
    if(i>=world_specials.size())return false;
    const auto& binding=world_specials[i];
    const auto it=std::ranges::lower_bound(binding.actors,message.key,{},&WorldSpecialActor::key);
    if(it==binding.actors.end()||it->key!=message.key)return false;
    const auto actor=api.weakget(&it->actor),action=api.weakget(&binding.action);
    const double range=i==0?1200.:4500.;
    if(!actor||!action||actor_is_destroying(actor)||api.getbool(binding.active,actor,0)||
       !(native_enemy_distance(native_actor_location(actor),peer.position)<=range*range))return false;
    api.event(actor,action,nullptr);
    return api.getbool(binding.active,actor,0);
}
static bool world_dagger_pose(void* actor,const WorldSpecialBinding& binding,float progress){
    const auto mesh=catalog_object_field(actor,"StaticMesh");
    const auto mesh_type=world_input(world_static_component),get=world_input(world_barrier_relative);
    if(!mesh||!mesh_type||!api.isa(mesh,mesh_type)||!get||
       !medic_params(world_barrier_relative,48,{{"ReturnValue",48,0}})||
       !medic_params(world_relative_set,189,{{"NewTransform",48,0},{"bSweep",1,48},
           {"SweepHitResult",136,52},{"bTeleport",1,188}})||
       !medic_params(world_body_scalar,12,{{"ParameterName",8,0},{"ParameterValue",4,8}}))return false;
    const auto endpoint=AnimU::field<float>(actor,binding.button_end);
    if(!std::isfinite(endpoint))return false;
    constexpr std::array names{L"Color_Brightness",L"Cubemap_Intensity",L"Emissive_Brightness"};
    for(unsigned i=0;i<names.size();++i)if(!world_dagger_scalars[i].index){
        const auto key=medic_name(names[i]);
        if(!medic_name_is(key,names[i]))return false;
        world_dagger_scalars[i]=key;
    }
    alignas(16) FactoryParameters args{},result{};
    api.event(mesh,get,result.data());
    auto transform=AnimU::field<std::array<float,12>>(result.data(),0);
    transform[4]=endpoint*progress;
    transform[5]=transform[6]=0.f;
    factory_put(args,0,transform);
    api.event(mesh,world_input(world_relative_set),args.data());
    const std::array values{1.f-.8f*progress,1.f-.9f*progress,.3f-.25f*progress};
    for(unsigned i=0;i<values.size();++i){
        factory_put(args,0,world_dagger_scalars[i]);
        factory_put(args,8,values[i]);
        api.event(mesh,world_input(world_body_scalar),args.data());
    }
    return true;
}
static void world_dagger_flash(void* actor,double time){
    if(world_actor_can_sound())world_actor_sound(actor,"SoundOnHit");
    constexpr auto path="/Game/RuinsLevel/Models/DaggerButton/DaggerButton_Hitspakrs.DaggerButton_Hitspakrs";
    const auto system=native_visual_asset(world_dagger_sparks,path,world_input(world_projectile_system),time);
    if(!system||!projectile_stock_niagara_location)return;
    alignas(16) FactoryParameters rotation{};
    api.event(actor,getters[1].fn,rotation.data());
    projectile_stock_niagara_location(actor,system,native_actor_location(actor),
        AnimU::field<std::array<float,3>>(rotation.data(),getters[1].at),{1,1,1},true,true,1,true);
}
static bool world_dagger_display(void* actor,const WorldSpecialBinding& binding,WorldSpecialActor& entry,
                                 bool active,bool announce,double time){
    if(!entry.synced||entry.presented_active!=active){
        const bool animate=entry.synced&&active;
        if((active||entry.presented_active)&&!world_dagger_pose(actor,binding,active&&!animate?1.f:0.f))return false;
        entry.animated_at=animate?time:0.;
        entry.presented_active=active;
        entry.synced=true;
        if(animate&&announce)world_dagger_flash(actor,time);
    }
    if(entry.animated_at){
        const float t=static_cast<float>(std::clamp((time-entry.animated_at)/.5,0.,1.));
        if(world_dagger_pose(actor,binding,t*t*(3.f-2.f*t))&&t>=1.f)entry.animated_at=0.;
    }
    return true;
}
static void* world_special_attach_sound(void* sound,void* parent){
    const auto scene=world_input(world_projectile_scene),sound_type=world_input(world_actor_sound_type);
    if(!sound_stock_attachment||!sound||!sound_type||!api.isa(sound,sound_type)||
       !parent||!scene||!api.isa(parent,scene))return nullptr;
    return sound_stock_attachment(sound,parent,{}, {},{},0,true,1.f,1.f,0.f,nullptr,nullptr,true);
}
static void world_secret_sound(void* actor){
    const auto field=catalog_field(AnimU::field<void*>(actor,16),"OpeningSoundComp",8);
    const auto component_type=world_input(world_sound_component);
    if(!medic_field_class(field,"ObjectProperty")||!component_type||AnimU::field<void*>(field,0x78)!=component_type)return;
    const auto offset=*api.offset(field);
    const auto previous=AnimU::field<void*>(actor,offset);
    if(previous&&api.isa(previous,component_type)&&medic_params(world_sound_stop,0,{}))
        sound_gameplay_event(previous,world_input(world_sound_stop),nullptr);
    auto sound=catalog_object_field(actor,"OpenSoundOverride");
    if(!sound)sound=catalog_object_field(actor,"OpenSound");
    const auto component=world_special_attach_sound(sound,catalog_object_field(actor,"DefaultSceneRoot"));
    if(!component||!api.isa(component,component_type))return;
    std::memcpy(static_cast<unsigned char*>(actor)+offset,&component,sizeof(component));
    if(medic_params(world_sound_fade_in,13,{{"FadeInDuration",4,0},{"FadeVolumeLevel",4,4},
        {"StartTime",4,8},{"FadeCurve",1,12}})){
        std::array<float,4> args{.2f,1.f,0.f,0.f};
        sound_gameplay_event(component,world_input(world_sound_fade_in),args.data());
    }
}
static bool world_secret_display(void* actor,const WorldSpecialBinding& binding,bool open,bool live){
    const auto finish=api.weakget(&binding.finish);
    const auto system=world_input(world_kismet);
    if(live&&system&&medic_params(world_special_time,12,{{"WorldContextObject",8,0},{"ReturnValue",4,8}})){
        alignas(16) FactoryParameters args{};
        factory_put(args,0,actor);
        api.event(system,world_input(world_special_time),args.data());
        const auto start=AnimU::field<float>(args.data(),8);
        if(std::isfinite(start)){
            api.setbool(binding.active,actor,open,0);
            std::memcpy(static_cast<unsigned char*>(actor)+binding.started,&start,sizeof(start));
            api.setbool(binding.running,actor,true,0);
            own_set(actor,death_actor_tick,true);
            world_secret_sound(actor);
            return true;
        }
    }
    if(!finish)return false;
    api.setbool(binding.active,actor,open,0);
    api.event(actor,finish,nullptr);
    return true;
}
static bool world_gargoyle_display(void* actor,const WorldSpecialBinding& binding,bool live,double time){
    const auto timeline=catalog_object_field(actor,"Timeline_0");
    const auto play=world_input(world_actor_play),finish=api.weakget(&binding.finish);
    if(live&&timeline&&play&&!*api.parms(play)&&api.isa(timeline,AnimU::field<void*>(play,32))){
        api.event(timeline,play,nullptr);
        constexpr auto path="/Game/Enemies/Gargler_00/Audio/Gargoyle_Switch_Light.Gargoyle_Switch_Light";
        const auto sound=native_visual_asset(world_gargoyle_sound,path,world_input(world_actor_sound_type),time);
        world_special_attach_sound(sound,catalog_object_field(actor,"SkeletalMesh"));
    }else if(finish)api.event(actor,finish,nullptr);
    else return false;
    api.setbool(binding.active,actor,true,0);
    return true;
}
static void world_special_step(void* pawn,double time){
    if(!active_peer_mask||!world_progress_ready())return;
    world_special_scan(pawn,time);
    if(host_role){
        // The stock Hit wrapper can save AlreadyHit before its timeline branch.
        // Keep the host mesh in step with that flag without firing Hit again.
        const auto& binding=world_specials[1];
        for(auto& entry:world_specials[1].actors)
            if(const auto actor=api.weakget(&entry.actor))
                world_dagger_display(actor,binding,entry,api.getbool(binding.active,actor,0),false,time);
        return;
    }
    const auto& desired=world_progress.authoritative.mechanisms;
    for(unsigned i=0;i<world_specials.size();++i){
        const auto& binding=world_specials[i];
        const auto kind=static_cast<WP::MechanismKind>(static_cast<unsigned>(WP::MechanismKind::secret)+i);
        for(auto& entry:world_specials[i].actors){
            const auto actor=api.weakget(&entry.actor);
            if(!actor)continue;
            const WP::MechanismState key{entry.key,kind};
            const auto it=std::ranges::lower_bound(desired,key,WP::mechanism_less);
            const bool has_state=it!=desired.end()&&it->key==entry.key&&it->kind==kind;
            const bool saved=world_actor_saved_positive(world_progress.authoritative.actors,entry.key);
            if(!has_state&&!saved)continue;
            const bool open=has_state?it->active:saved;
            const bool live=entry.synced;
            const bool local=api.getbool(binding.active,actor,0);
            if(live&&local&&!open){
                const auto save=api.weakget(&world_progress.save);
                const auto action=static_cast<WorldIntent::Action>(static_cast<unsigned>(WorldIntent::Action::secret)+i);
                if((save&&world_interaction_queue(save,entry.key,action,time))||
                   std::ranges::any_of(world_actor.pending,[&](const auto& request){
                       return request.key==entry.key&&request.action==action;
                   })){
                    if(i==1)world_dagger_display(actor,binding,entry,true,false,time);
                    continue;
                }
            }
            if(i==1){
                if(world_dagger_display(actor,binding,entry,open,!local,time)&&local!=open)
                    api.setbool(binding.active,actor,open,0);
                continue;
            }
            // The stock save restore can set the flag and seek to the final
            // pose before this step. Preserve the last authoritative transition.
            const bool changed=live&&entry.presented_active!=open;
            bool displayed=local==open&&!changed;
            if(!displayed&&i==0)
                displayed=(local==open&&api.getbool(binding.running,actor,0))||
                    world_secret_display(actor,binding,open,changed);
            else if(!displayed&&open)
                displayed=world_gargoyle_display(actor,binding,changed,time);
            if(displayed){
                entry.synced=true;
                entry.presented_active=open;
            }
        }
    }
}
