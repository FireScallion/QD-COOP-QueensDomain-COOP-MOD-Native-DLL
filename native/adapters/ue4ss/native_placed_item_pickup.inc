static Weak placed_pickup_request;
static std::string placed_pickup_pending;
static AnimU::NameKey placed_pickup_drop_key;

static bool native_placed_item_save_pending(){return placed_save.stage!=PlacedSaveStage::idle;}

static bool native_placed_item_custody_busy(){
    return native_placed_item_save_pending()||placed_escrow_busy||std::ranges::any_of(placed_item_transfers,[](const auto& transfer){
        return transfer.state==Placed::Custody::pending;
    });
}

static void placed_pickup_reset(){
    placed_pickup_request={};
    placed_pickup_pending.clear();
    placed_pickup_drop_key={};
}

static bool placed_stackable(void* item){
    if(!item)return false;
    const auto type=AnimU::field<void*>(item,16);
    const auto consumable=placed_item_bool(type,"Consumable"),equippable=placed_item_bool(type,"Equippable");
    const auto unstackable=placed_item_bool(type,"Unstackable");
    return consumable&&equippable&&unstackable&&api.getbool(consumable,item,0)&&
        !api.getbool(equippable,item,0)&&!api.getbool(unstackable,item,0);
}

static bool placed_local_owner(const Placed::Drop& drop){
    return Placed::owner_matches(drop.owner,placed_claim_identity(medic_identity()),placed_legacy_identity);
}

static AnimU::NameKey placed_pickup_name(std::string_view text){
    const std::wstring wide(text.begin(),text.end());
    const auto key=medic_name(wide);
    return medic_name_is(key,wide)?key:AnimU::NameKey{};
}

static bool placed_claim_visible(void* pawn,void* actor,const std::array<float,3>& point,const Placed::Drop& drop);

static bool placed_pickup_near(void* pawn,void* actor,const Placed::Drop& drop){
    placed_pickup_stage=10;
    if(!pawn||!actor||local_life.actor!=pawn||local_life.life.phase!=LifePhase::standing)return false;
    alignas(16) FactoryParameters args{};
    api.event(pawn,getters[0].fn,args.data());
    const auto point=AnimU::field<std::array<float,3>>(args.data(),getters[0].at);
    return placed_claim_visible(pawn,actor,point,drop);
}

static bool placed_credit(void* pawn,const Placed::Drop& drop,bool recovery,PlacedSaveProof& proof){
    const auto& binding=medic_runtime.save;
    const auto save=medic_active(binding,pawn);
    if(!save||!world_progress.remove)return false;
    const auto item=placed_item_data(drop.item);
    placed_pickup_stage=21;
    const bool stackable=placed_stackable(item);
    if(!item||(!stackable&&!placed_local_owner(drop)))return false;
    const auto receipt=placed_pickup_name("QDCoopPersonal~got~"+drop.id);
    placed_pickup_stage=22;
    if(!receipt.index)return false;
    int previous{};
    const bool had_receipt=binding.find(binding.markers.address(save),binding.markers.field,&receipt,&previous);
    const bool received=had_receipt&&previous==1;
    placed_pickup_stage=23;
    if((had_receipt||recovery)&&!received)return false;
    const auto before=medic_count(binding,item);
    placed_pickup_stage=24;
    if(before<0||(!received&&(before==INT_MAX||(!stackable&&before!=0))))return false;
    const auto expected=before+(!received?1:0);
    const std::array markers{PlacedSaveMarker{receipt,1}};
    if(!placed_save_prepare(pawn,markers,drop.item,expected,proof))return false;
    if(received)return true;
    // A reentrant stock callback must never make an incomplete credit
    // look settled in a save. Only the exact +1 transition closes it.
    medic_set_marker(binding,save,receipt,-1);
    placed_pickup_stage=54;
    if(!medic_inventory_delta(pawn,item,1)||medic_active(binding,pawn)!=save||
       medic_count(binding,item)!=expected){
        if(medic_active(binding,pawn)==save&&medic_count(binding,item)==before)
            world_progress.remove(binding.markers.address(save),binding.markers.field,&receipt);
        return false;
    }
    medic_set_marker(binding,save,receipt,1);
    api.event(pawn,world_input(world_medic_refresh),nullptr);
    ++placed_pickup_credits;
    return true;
}

static bool placed_claim_visible(void* pawn,void* actor,const std::array<float,3>& point,const Placed::Drop& drop){
    const auto trace=world_input(world_life_trace),kismet=world_input(world_kismet);
    if(!trace||!kismet||!pawn||!actor||local_life.actor!=pawn)return false;
    std::array<float,3> target{};
    float squared{};
    for(unsigned i=0;i<point.size();++i){
        target[i]=static_cast<float>(drop.position[i]);
        const auto distance=target[i]-point[i];
        squared+=distance*distance;
    }
    placed_pickup_stage=11;
    if(!std::isfinite(squared)||squared>250.f*250.f)return false;
    target[2]+=18.f;
    const auto& fields=local_life.layout.trace;
    if(std::ranges::min(fields)<0)return false;
    void* ignored[]{pawn,actor};
    const NativeActorArray ignored_actors{ignored,2,2};
    alignas(16) FactoryParameters args{};
    factory_put(args,fields[0],pawn);
    factory_put(args,fields[1],point);
    factory_put(args,fields[2],target);
    factory_put(args,fields[3],std::uint8_t{1});
    factory_put(args,fields[6],ignored_actors);
    api.event(kismet,trace,args.data());
    placed_pickup_stage=12;
    return !args[fields[4]];
}

static bool placed_pickup_start(void* pawn,PlacedItemVisual& visual,bool recovery,double time){
    const auto& binding=medic_runtime.save;
    const auto save=medic_active(binding,pawn);
    placed_pickup_stage=20;
    if(!save||!world_progress.remove||!checkpoint_medic_idle()||native_checkpoint.stage!=CheckpointStage::idle||
       rollback_busy()||world_scene_busy()||native_placed_item_save_pending())return false;
    const auto drop=placed_pickup_name(visual.save_key);
    if(!drop.index)return false;
    int current{};
    if(!binding.find(binding.markers.address(save),binding.markers.field,&drop,&current)||
       current!=static_cast<int>(Placed::Custody::ground))return false;
    PlacedSaveProof proof;
    if(!placed_credit(pawn,visual.drop,recovery,proof))return false;
    placed_operation=PlacedOperation::local_pickup;
    placed_pickup_pending=visual.save_key;
    placed_pickup_drop_key=drop;
    visual.pickable=false;
    if(const auto actor=api.weakget(&visual.actor))own_set(actor,factory_object(factory_collision),false);
    placed_save_begin(std::move(proof),time);
    return true;
}

static void placed_pickup_enable(void* pawn,void* actor,PlacedItemVisual& visual,double time){
    const auto& binding=medic_runtime.save;
    const auto save=medic_active(binding,pawn);
    if(visual.pickable||native_placed_item_save_pending()||!save)return;
    visual.retry=time+.75;
    if(host_role){
        const auto receipt=placed_pickup_name("QDCoopPersonal~got~"+visual.drop.id);
        int received{};
        if(receipt.index&&binding.find(binding.markers.address(save),binding.markers.field,&receipt,&received)){
            placed_pickup_start(pawn,visual,true,time);
            return;
        }
    }
    const auto item=placed_item_data(visual.drop.item);
    const auto sphere=placed_object_field(actor,"InteractCollision");
    if(!item||(!placed_stackable(item)&&!placed_local_owner(visual.drop))||!sphere||
       !medic_params(world_placed_sphere_radius,5,{{"InSphereRadius",4,0},{"bUpdateOverlaps",1,4}})||
       !world_input(world_placed_suspend)||!native_placed_item_bind_interact(AnimU::field<void*>(actor,16)))return;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,32.f);
    args[4]=1;
    api.event(sphere,world_input(world_placed_sphere_radius),args.data());
    unsigned char query_only=1;
    api.event(sphere,world_input(world_component_collision),&query_only);
    own_set(actor,factory_object(factory_collision),true);
    visual.pickable=true;
    ++placed_pickup_enables;
}

static void placed_item_request(void* actor,void* pawn){
    if(pawn!=api.weakget(&automatic_context)||native_placed_item_save_pending())return;
    const auto found=std::ranges::find_if(placed_item_visuals,[&](const auto& visual){
        return visual.wanted&&visual.ready&&visual.pickable&&api.weakget(&visual.actor)==actor;
    });
    if(found==placed_item_visuals.end())return;
    const bool queued=host_role?makeweak(placed_pickup_request,actor):placed_claim_queue(found->drop.id,now());
    if(queued)++placed_pickup_requests;
}

static void placed_pickup_step(void* pawn,double time){
    if(native_placed_item_save_pending()){
        placed_save_poll(pawn,time);
        if(placed_save.stage!=PlacedSaveStage::proved)return;
        const auto& binding=medic_runtime.save;
        const auto save=medic_active(binding,pawn);
        if(!save)return;
        if(placed_operation==PlacedOperation::local_pickup){
            medic_set_marker(binding,save,placed_pickup_drop_key,static_cast<int>(Placed::Custody::claimed));
            const auto found=std::ranges::find(placed_item_visuals,placed_pickup_pending,&PlacedItemVisual::save_key);
            if(found!=placed_item_visuals.end())found->wanted=false;
            ++placed_pickup_confirmed;
            placed_pickup_reset();
        }else if(placed_operation==PlacedOperation::place)++placed_source_confirmed;
        else if(placed_operation==PlacedOperation::register_drop)++placed_deposit_registered;
        else if(placed_operation==PlacedOperation::settle_escrow)++placed_deposit_settled;
        else placed_claim_finished(time);
        placed_save={};
        placed_operation=PlacedOperation::none;
        return;
    }
    placed_claims_step(pawn,time);
    if(!host_role||native_placed_item_save_pending())return;
    const auto actor=api.weakget(&placed_pickup_request);
    placed_pickup_request={};
    if(!actor)return;
    const auto found=std::ranges::find_if(placed_item_visuals,[&](const auto& visual){
        return visual.wanted&&visual.ready&&visual.pickable&&api.weakget(&visual.actor)==actor;
    });
    if(found!=placed_item_visuals.end()&&placed_pickup_near(pawn,actor,found->drop))
        placed_pickup_start(pawn,*found,false,time);
}
