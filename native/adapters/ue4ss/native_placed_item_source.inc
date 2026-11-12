struct PlacedEscrow {
    Placed::Drop drop;
    std::string raw;
    AnimU::NameKey local,ground;
};
static Weak placed_source_save;
static std::vector<PlacedEscrow> placed_source_escrows;
static double placed_escrow_next;
static double placed_source_deadline;

static void placed_source_reset(){
    placed_source_save={};
    placed_source_escrows.clear();
    placed_escrow_busy=false;
    placed_escrow_next=0;
    placed_legacy_identity.clear();
}

static std::string placed_source_room(){
    if(placed_item_room_ambiguous)return {};
    if(!placed_item_room.empty())return placed_item_room;
    return placed_claim_identity(host_role?medic_identity():life_peers[0].rescue_identity);
}

struct PlacedSourceMarker {AnimU::NameKey key;int value;};
static bool placed_source_markers(void* save,std::vector<PlacedSourceMarker>& markers){
    static_assert(sizeof(PlacedSourceMarker)==12);
    return WP::read_sparse_map<PlacedSourceMarker,20>(reinterpret_cast<std::uintptr_t>(
        medic_runtime.save.markers.address(save)),catalog_read,markers);
}

static bool placed_source_bind(void* save){
    if(api.weakget(&placed_source_save)==save)return true;
    placed_source_reset();
    std::vector<PlacedSourceMarker> markers;
    if(!placed_source_markers(save,markers)||!makeweak(placed_source_save,save))return false;
    bool ambiguous{};
    for(const auto& marker:markers){
        std::string name;
        if(marker.value!=1||!AnimU::name_string(catalog_name_pool,marker.key,catalog_read,name))continue;
        if(Placed::prefix(name,Placed::identity_prefix)){
            const auto id=std::string_view(name).substr(Placed::identity_prefix.size());
            if(!Placed::valid_id(id)||(!placed_legacy_identity.empty()&&!WP::name_equal(placed_legacy_identity,id)))ambiguous=true;
            else placed_legacy_identity=id;
            continue;
        }
        if(!Placed::prefix(name,Placed::escrow_prefix))continue;
        const auto raw=std::string_view(name).substr(Placed::escrow_prefix.size());
        Placed::Drop drop;
        if(!Placed::decode(raw,drop))continue;
        const auto key=placed_pickup_name(std::string(Placed::drop_prefix)+std::string(raw));
        if(key.index)placed_source_escrows.push_back({std::move(drop),std::string(raw),marker.key,key});
    }
    if(ambiguous)placed_legacy_identity.clear();
    return true;
}

static bool placed_source_point(void* pawn,Placed::Drop& drop){
    const auto trace=world_input(world_life_trace),kismet=world_input(world_kismet);
    const auto control=world_input(world_control_rotation);
    const auto& fields=local_life.layout.trace;
    placed_source_stage=3;
    if(!trace||!kismet||!control||std::ranges::min(fields)<0||*api.parms(trace)>sizeof(FactoryParameters))return false;
    const auto hit=catalog_field(trace,"OutHit",136);
    if(!medic_field_class(hit,"StructProperty")||!damage_type_path(hit,"/Script/Engine.HitResult")||
       static_cast<unsigned>(*api.offset(hit))+136u>*api.parms(trace))return false;
    const auto impact=catalog_field(AnimU::field<void*>(hit,0x78),"ImpactPoint",12);
    placed_source_stage=4;
    if(!medic_field_class(impact,"StructProperty")||!damage_type_path(impact,"/Script/Engine.Vector_NetQuantize")||
       static_cast<unsigned>(*api.offset(impact))+12u>136u)return false;

    alignas(16) FactoryParameters location{};
    api.event(pawn,getters[0].fn,location.data());
    const auto origin=AnimU::field<std::array<float,3>>(location.data(),getters[0].at);
    std::array<float,3> rotation{};
    api.event(pawn,control,rotation.data());
    const auto valid=[](float value){return std::isfinite(value)&&std::abs(value)<=10000000.f;};
    if(!valid(rotation[1])||!std::ranges::all_of(origin,valid))return false;
    constexpr double radians=.017453292519943295;
    const auto yaw=rotation[1]*radians;
    const auto x=static_cast<float>(std::cos(yaw)),y=static_cast<float>(std::sin(yaw));
    void* ignored[]{pawn};
    const NativeActorArray ignored_actors{ignored,1,1};
    const auto cast=[&](const auto& start,const auto& end,FactoryParameters& args){
        args={};
        factory_put(args,fields[0],pawn);
        factory_put(args,fields[1],start);
        factory_put(args,fields[2],end);
        factory_put(args,fields[3],std::uint8_t{1});
        factory_put(args,fields[6],ignored_actors);
        api.event(kismet,trace,args.data());
        return args[fields[4]]!=0;
    };
    alignas(16) FactoryParameters args{};
    for(const float distance:{95.f,72.f,50.f}){
        const std::array<float,3> forward{origin[0]+distance*x,origin[1]+distance*y,origin[2]};
        placed_source_stage=5;
        if(cast(origin,forward,args))continue;
        auto above=forward,below=forward;
        above[2]+=30.f;
        below[2]-=240.f;
        placed_source_stage=6;
        if(!cast(above,below,args))continue;
        auto position=AnimU::field<std::array<float,3>>(args.data(),*api.offset(hit)+*api.offset(impact));
        position[2]+=12.f;
        if(!std::ranges::all_of(position,valid))continue;
        std::ranges::transform(position,drop.position.begin(),[](float value){return static_cast<std::int32_t>(std::floor(value));});
        drop.yaw=static_cast<std::int32_t>(std::floor(rotation[1]));
        return true;
    }
    return false;
}

static bool placed_source_ready(void* pawn){
    return active_peer_mask&&world_progress_ready()&&factory_configured&&!placed_source_room().empty()&&
        pawn&&local_life.actor==pawn&&local_life.life.phase==LifePhase::standing&&
        native_checkpoint.stage==CheckpointStage::idle&&!rollback_busy()&&!world_scene_busy()&&
        !native_placed_item_custody_busy()&&placed_item_transfers.size()<Placed::max_ground_items;
}

static bool placed_source_eligible(void* item){
    const auto type=world_input(world_placed_item_type);
    if(!item||!type||!api.isa(item,type)||!placed_stackable(item))return false;
    const auto key=placed_item_bool(type,"ShowInKeyMenu");
    return key&&!api.getbool(key,item,0);
}

static bool placed_source_control(){
    return native_bootstrap&&GetCurrentThreadId()==game_thread.load()&&native_epoch_open&&!native_stopping;
}

extern "C" __declspec(dllexport) int QDEnemyNativePlaceAvailability(void* item){
    if(!placed_source_control()||!active_peer_mask||!placed_source_eligible(item))return 0;
    const auto pawn=api.weakget(&automatic_context);
    return !api.weakget(&placed_source_request)&&placed_source_ready(pawn)&&medic_active(medic_runtime.save,pawn)&&
        medic_count(medic_runtime.save,item)>0?1:2;
}

static bool placed_source_selected(void* widget,void* item){
    std::string type;
    if(!widget||!item||!catalog_path(AnimU::field<void*>(widget,16),type)||
       type!="/Game/UI/Widgets/FS_InspectItem.FS_InspectItem_C")return false;
    const auto open=placed_item_bool(AnimU::field<void*>(widget,16),"IsOpen");
    return open&&api.getbool(open,widget,0)&&placed_object_field(widget,"CurrentItem")==item;
}

extern "C" __declspec(dllexport) std::uint64_t QDEnemyQueueNativePlaceFromInspect(void* widget,void* item){
    if(QDEnemyNativePlaceAvailability(item)!=1||!placed_source_selected(widget,item)||
       placed_source_sequence==UINT64_MAX||!makeweak(placed_source_inspect,widget)||!makeweak(placed_source_request,item))return 0;
    placed_source_token=++placed_source_sequence;
    placed_source_deadline=now()+1.5;
    return placed_source_token;
}

extern "C" __declspec(dllexport) int QDEnemyReadNativePlaceResult(std::uint64_t token){
    if(!placed_source_control()||!token)return -1;
    if(token==placed_source_token)return 0;
    return token==placed_source_completed?placed_source_result:-1;
}

extern "C" __declspec(dllexport) int QDEnemyQueueNativePlacedItem(void* item){
    if(!placed_source_control()||
       api.weakget(&placed_source_request)||!placed_source_ready(api.weakget(&automatic_context)))return 0;
    return item&&makeweak(placed_source_request,item)?1:0;
}

static void placed_source_step(void* pawn,double time){
    const auto item=api.weakget(&placed_source_request);
    if(!item&&!placed_source_token)return;
    const bool ready=placed_source_ready(pawn);
    if(placed_source_token&&item&&!ready&&time<placed_source_deadline&&
       placed_source_selected(api.weakget(&placed_source_inspect),item))return;
    placed_source_request={};
    if(placed_source_token){
        placed_source_completed=placed_source_token;
        placed_source_token=0;
        placed_source_result=-1;
        const auto widget=api.weakget(&placed_source_inspect);
        placed_source_inspect={};
        if(!placed_source_selected(widget,item))return;
    }
    if(!item||!ready)return;
    placed_source_stage=1;
    const auto& binding=medic_runtime.save;
    const auto save=medic_active(binding,pawn);
    if(!save||!placed_source_bind(save))return;
    Placed::Drop drop;
    drop.owner=placed_claim_identity(medic_identity());
    drop.room=placed_source_room();
    placed_source_stage=2;
    if(drop.owner.empty()||!catalog_path(item,drop.item)||placed_item_data(drop.item)!=item||!placed_stackable(item)||
       !catalog_path(api.weakget(&attached_world),drop.map))return;
    if(!placed_source_point(pawn,drop)){placed_source_result=-2;return;}
    const int before=medic_count(binding,item);
    placed_source_stage=7;
    if(before<1)return;
    qdcoop::Id identity{};
    if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&identity),sizeof(identity),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0||
       !identity.valid())return;
    drop.id=placed_claim_identity(identity);
    const auto raw=Placed::encode(drop);
    if(raw.empty()||placed_find_transfer(drop.id))return;
    const auto world_key=std::string(Placed::drop_prefix)+raw;
    const auto ground=placed_pickup_name(world_key);
    const auto escrow=placed_pickup_name(std::string(Placed::escrow_prefix)+raw);
    const auto room=placed_pickup_name(std::string(Placed::room_prefix)+drop.room);
    if(!ground.index||!escrow.index||!room.index)return;
    int present{};
    if(binding.find(binding.markers.address(save),binding.markers.field,&ground,&present)||
       binding.find(binding.markers.address(save),binding.markers.field,&escrow,&present))return;
    const std::array all_markers{PlacedSaveMarker{escrow,1},PlacedSaveMarker{ground,static_cast<int>(Placed::Custody::ground)},
        PlacedSaveMarker{room,1}};
    const auto markers=std::span(all_markers).first(host_role?all_markers.size():1);
    PlacedSaveProof proof;
    placed_source_stage=8;
    if(!placed_save_prepare(pawn,markers,drop.item,before-1,proof)||!placed_item_bind(time))return;
    PlacedItemVisual visual{drop,world_key,{}};
    if(host_role)visual.ready=placed_item_create(pawn,visual);
    const bool prepared=host_role?visual.ready:placed_item_model(item).component!=nullptr;
    placed_source_stage=9;
    if(prepared){
        // Validate the model before debiting. The guest creates its actor from
        // the host's durable ground record after the deposit is accepted.
        medic_inventory_delta(pawn,item,-1);
        if(medic_active(binding,pawn)==save&&medic_count(binding,item)==before-1){
            for(const auto& marker:markers)medic_set_marker(binding,save,marker.key,marker.value);
            placed_source_escrows.push_back({drop,raw,escrow,ground});
            if(host_role)placed_item_visuals.push_back(std::move(visual));
            else {
                placed_escrow_busy=true;
                placed_escrow_next=0;
            }
            placed_operation=PlacedOperation::place;
            placed_save_begin(std::move(proof),time);
            api.event(pawn,world_input(world_medic_refresh),nullptr);
            ++placed_source_debits;
            placed_source_stage=10;
            placed_source_result=1;
            return;
        }
    }
    if(host_role){
        visual.wanted=false;
        if(!destroy_owned_actor(visual.actor,false))placed_item_visuals.push_back(std::move(visual));
    }
}

static bool placed_source_register(void* pawn,const Placed::Drop& drop,std::string_view raw,bool legacy,double time){
    std::string map;
    if(!catalog_path(api.weakget(&attached_world),map)||!WP::name_equal(drop.map,map)||
       native_checkpoint.stage!=CheckpointStage::idle||rollback_busy()||world_scene_busy())return false;
    const auto& binding=medic_runtime.save;
    const auto save=medic_active(binding,pawn);
    if(!save)return false;
    const auto ground=placed_pickup_name(std::string(Placed::drop_prefix)+std::string(raw));
    const auto room=placed_pickup_name(std::string(Placed::room_prefix)+drop.room);
    int existing{};
    if(!ground.index||!room.index||binding.find(binding.markers.address(save),binding.markers.field,&ground,&existing)||
       placed_item_transfers.size()>=Placed::max_ground_items||placed_find_transfer(drop.id))return false;
    std::vector<PlacedSourceMarker> names;
    if(!placed_source_markers(save,names))return false;
    const auto stem=std::string(Placed::drop_prefix)+drop.id+"~";
    for(const auto& name:names){
        std::string key;
        if(!AnimU::name_string(catalog_name_pool,name.key,catalog_read,key)||Placed::prefix(key,stem))return false;
    }
    const auto item=placed_item_data(drop.item);
    if(!item||(!placed_stackable(item)&&!legacy)||!placed_item_model(item).component)return false;
    const std::array markers{PlacedSaveMarker{ground,0},PlacedSaveMarker{room,1}};
    PlacedSaveProof proof;
    if(!placed_save_prepare(pawn,markers,{},-1,proof))return false;
    for(const auto& marker:markers)medic_set_marker(binding,save,marker.key,marker.value);
    placed_operation=PlacedOperation::register_drop;
    placed_save_begin(std::move(proof),time);
    return true;
}

static void placed_source_network(void* pawn,double time){
    if(!active_peer_mask||!world_progress_ready()||native_placed_item_save_pending())return;
    const auto& binding=medic_runtime.save;
    const auto save=medic_active(binding,pawn);
    if(!save||!placed_source_bind(save)||placed_source_escrows.empty()||time<placed_escrow_next)return;
    placed_escrow_next=time+.45;
    const auto identity=placed_claim_identity(medic_identity());
    const auto room=placed_source_room();
    if(identity.empty()||room.empty())return;
    placed_escrow_busy=false;
    for(auto it=placed_source_escrows.begin();it!=placed_source_escrows.end();){
        const auto& entry=*it;
        if(!Placed::owner_matches(entry.drop.owner,identity,placed_legacy_identity)||!WP::name_equal(entry.drop.room,room)){
            ++it;
            continue;
        }
        int state{};
        if(!binding.find(binding.markers.address(save),binding.markers.field,&entry.ground,&state)){
            placed_escrow_busy=true;
            if(host_role){
                const bool legacy=WP::name_equal(entry.drop.owner,placed_legacy_identity);
                if(placed_source_register(pawn,entry.drop,entry.raw,legacy,time))return;
            }else if(local_life_pose.generation){
                E::Packet packet{};
                if(Placed::encode_deposit(envelope,local_life_pose.generation,entry.raw,packet)&&send(packet,1))
                    ++placed_deposit_sent;
            }
        }else if(state==static_cast<int>(Placed::Custody::claimed)&&native_checkpoint.stage==CheckpointStage::idle&&
                  !rollback_busy()&&!world_scene_busy()){
            const std::array markers{PlacedSaveMarker{entry.local,2}};
            PlacedSaveProof proof;
            if(placed_save_prepare(pawn,markers,{},-1,proof)){
                medic_set_marker(binding,save,entry.local,2);
                placed_source_escrows.erase(it);
                placed_operation=PlacedOperation::settle_escrow;
                placed_save_begin(std::move(proof),time);
                return;
            }
        }
        ++it;
    }
}

static bool receive_placed_deposit(const ReceivedPacket& received,unsigned kind,double time){
    if(kind!=Placed::deposit_kind)return false;
    if(!host_role||!world_progress_ready()||native_placed_item_save_pending()||!placed_claim_peer(received.peer,time)||
       native_checkpoint.stage!=CheckpointStage::idle||rollback_busy()||world_scene_busy())return true;
    std::uint32_t generation{};
    std::string_view raw;
    Placed::Drop drop;
    const auto& peer=life_peers[received.peer+1];
    const auto legacy=placed_claim_peer_alias(received.peer,time);
    if(!Placed::decode_deposit(envelope,received.packet,generation,raw,drop)||generation!=peer.generation||
       !Placed::owner_matches(drop.owner,placed_claim_identity(peer.rescue_identity),legacy)||
       !WP::name_equal(drop.room,placed_source_room()))return true;
    const auto pawn=api.weakget(&automatic_context);
    placed_source_register(pawn,drop,raw,WP::name_equal(drop.owner,legacy),time);
    return true;
}
