namespace StoryReward=qdcoop::story_reward_intent;
enum StoryDuckKey : unsigned { duck_killed,duck_claimed,duck_unlocked,duck_unlocks,duck_receipt,duck_key_count };
enum class ShopTrip { idle,awaiting_teleport,outbound,visiting,returning };
struct WorldStoryReward {
    Weak duck;
    std::array<AnimU::NameKey,duck_key_count> keys;
    StoryReward::Stage pending{},requested{};
    ShopTrip shop_trip{};
    std::uint64_t arrival_at_unlock{};
    double next{},next_send{},next_pickup_scan{};
    bool bound{};
};
static WorldStoryReward world_story;
static constexpr std::array story_duck_keys{
    L"ShrineBossKilled",L"ShrineBossRewardCount",L"ShopUnlockFinished",L"ShopUnlocks",L"QDCoopPersonal~story~stoneDuck"
};
static constexpr std::array<std::string_view,duck_key_count> story_duck_names{
    "ShrineBossKilled","ShrineBossRewardCount","ShopUnlockFinished","ShopUnlocks","QDCoopPersonal~story~stoneDuck"
};
static void world_story_reset(){world_story={};}

static bool world_story_shop_active(){return world_story.shop_trip!=ShopTrip::idle;}
static bool world_story_shop_local(void* pawn){
    auto& state=world_story;
    if(state.shop_trip==ShopTrip::idle)return false;
    if(world_progress.authoritative.arrival.serial!=state.arrival_at_unlock){
        state.shop_trip=ShopTrip::idle;
        return false;
    }
    const bool teleporting=world_stream_teleporting(pawn);
    switch(state.shop_trip){
    case ShopTrip::awaiting_teleport:
        if(teleporting)state.shop_trip=ShopTrip::outbound;
        break;
    case ShopTrip::outbound:
        if(!teleporting)state.shop_trip=ShopTrip::visiting;
        break;
    case ShopTrip::visiting:
        if(teleporting)state.shop_trip=ShopTrip::returning;
        break;
    case ShopTrip::returning:
        if(!teleporting)state.shop_trip=ShopTrip::idle;
        break;
    case ShopTrip::idle:
        break;
    }
    return state.shop_trip!=ShopTrip::idle;
}

static bool world_story_bind(){
    auto& state=world_story;
    if(state.bound)return true;
    for(unsigned i=0;i<state.keys.size();++i){
        state.keys[i]=medic_name(story_duck_keys[i]);
        if(!medic_name_is(state.keys[i],story_duck_keys[i]))return false;
    }
    state.bound=true;
    return true;
}
static int world_story_saved(const std::vector<WP::NamedState>& names,StoryDuckKey key){
    const auto name=story_duck_names[key];
    const auto found=std::ranges::lower_bound(names,name,WP::name_less,&WP::NamedState::key);
    return found!=names.end()&&WP::name_equal(found->key,name)?found->value:0;
}
static bool world_story_preserve(const std::vector<WP::NamedState>& names,const AnimU::NameKey& key){
    const auto& state=world_story;
    if(!state.bound||state.pending==StoryReward::Stage{})return false;
    const auto matches=[&](StoryDuckKey target){
        return key.index==state.keys[target].index&&key.number==state.keys[target].number;
    };
    if(matches(duck_claimed))return world_story_saved(names,duck_killed)>0&&world_story_saved(names,duck_claimed)<1;
    return state.pending==StoryReward::Stage::unlocked&&(matches(duck_unlocked)||matches(duck_unlocks))&&
        (world_story_saved(names,duck_claimed)>0||world_story_saved(names,duck_killed)>0)&&
        world_story_saved(names,duck_unlocked)<1;
}

static bool receive_story_reward(const ReceivedPacket& received,unsigned kind,double time){
    if(kind!=StoryReward::kind)return false;
    StoryReward::Message message;
    if(!host_role||received.peer>=Player::slots-1||!(active_peer_mask&(1u<<received.peer))||
       !StoryReward::decode(envelope,received.packet,message))return true;
    const auto& peer=life_peers[received.peer+1];
    if(message.generation==peer.generation&&fresh_life(peer,LifePhase::standing,time))
        world_story.requested=std::max(world_story.requested,message.stage);
    return true;
}
static void world_story_observe(void* save,double time){
    auto& state=world_story;
    if(!checkpoint_medic_idle()||rollback_busy())return;
    const auto& binding=medic_runtime.save;
    if(host_role){
        if(state.requested==StoryReward::Stage{}||!world_story_bind())return;
        const auto requested=state.requested;
        state.requested={};
        const auto& keys=state.keys;
        if(medic_marker(binding,save,keys[duck_claimed])<1){
            if(medic_marker(binding,save,keys[duck_killed])<1)return;
            medic_set_marker(binding,save,keys[duck_claimed],1);
            if(medic_marker(binding,save,keys[duck_claimed])!=1)return;
            world_progress.quests.push_back(keys[duck_claimed]);
            world_progress.notify=true;
            world_progress.next_capture=0.;
        }
        if(requested==StoryReward::Stage::unlocked&&medic_marker(binding,save,keys[duck_unlocked])<1){
            // Mirror the two checkpoint save writes without invoking its travel.
            const auto prior=medic_marker(binding,save,keys[duck_unlocks]);
            const auto count=prior==INT_MAX?prior:std::max(1,prior+1);
            medic_set_marker(binding,save,keys[duck_unlocked],1);
            if(medic_marker(binding,save,keys[duck_unlocked])!=1)return;
            medic_set_marker(binding,save,keys[duck_unlocks],count);
            if(medic_marker(binding,save,keys[duck_unlocks])!=count){
                medic_set_marker(binding,save,keys[duck_unlocked],0);
                return;
            }
            world_progress.quests.push_back(keys[duck_unlocked]);
            world_progress.quests.push_back(keys[duck_unlocks]);
            world_progress.notify=true;
            world_progress.next_capture=0.;
        }
        return;
    }
    if(world_guest_waiting()||!world_story_bind())return;
    const auto& names=world_progress.authoritative.names;
    const bool killed=world_story_saved(names,duck_killed)>0,claimed=world_story_saved(names,duck_claimed)>0;
    if((!killed&&!claimed)||world_story_saved(names,duck_unlocked)>0){state.pending={};return;}
    if(state.pending==StoryReward::Stage::claimed&&claimed)state.pending={};
    if(medic_marker(binding,save,state.keys[duck_unlocked])>0&&
       medic_marker(binding,save,state.keys[duck_claimed])>0){
        if(state.pending!=StoryReward::Stage::unlocked){
            state.next_send=0.;
            state.shop_trip=ShopTrip::awaiting_teleport;
            state.arrival_at_unlock=world_progress.authoritative.arrival.serial;
        }
        state.pending=StoryReward::Stage::unlocked;
    }else if(!claimed&&killed&&state.pending==StoryReward::Stage{}&&
             medic_marker(binding,save,state.keys[duck_claimed])>0){
        state.pending=StoryReward::Stage::claimed;
        state.next_send=0.;
    }
    if(state.pending==StoryReward::Stage{}||time<state.next_send||
       local_life.life.phase!=LifePhase::standing)return;
    E::Packet packet{};
    if(StoryReward::encode(envelope,{local_life_pose.generation,state.pending},packet)&&send(packet,1))
        state.next_send=time+.25;
}

static void* world_story_duck(){
    if(const auto found=api.weakget(&world_story.duck))return found;
    constexpr auto path="/Game/Items/KeyItems/ShopProgressionKey/StoneDuckShopUnlock.StoneDuckShopUnlock";
    const auto method=world_input(world_medic_credit);
    const auto property=method?catalog_field(method,"Item",40):nullptr;
    if(!medic_field_class(property,"SoftObjectProperty"))return nullptr;
    const auto type=AnimU::field<void*>(property,0x78);
    const auto duck=type?load_world_asset(path,UINT32_MAX,false,nullptr):nullptr;
    return duck&&api.isa(duck,type)&&makeweak(world_story.duck,duck)?duck:nullptr;
}

static void world_story_refresh_pickup(void* pawn,double time){
    auto& state=world_story;
    if(time<state.next_pickup_scan||!native_get_actors||!native_free)return;
    state.next_pickup_scan=time+2.;
    const auto npc_type=world_input(world_story_shrine_npc),pickup_type=world_input(world_story_duck_pickup),
        set=world_input(world_story_pickup_state),refresh=world_input(world_story_pickup_refresh);
    if(!npc_type||!pickup_type||!set||!refresh){
        scan_world_inputs(2048);
        return;
    }
    const auto pickup_base=AnimU::field<void*>(set,32);
    const auto acquired=catalog_field(pickup_type,"Acquired",1),transient=catalog_field(pickup_type,"TransientPickup",1);
    if(!catalog_subclass(pickup_type,pickup_base)||AnimU::field<void*>(refresh,32)!=pickup_base||
       !medic_params(world_story_pickup_state,1,{{"AlreadyAcquired",1,0}})||*api.parms(refresh)||
       !medic_field_class(acquired,"BoolProperty")||!medic_field_class(transient,"BoolProperty"))return;
    NativeActorArray entries{};
    native_get_actors(pawn,npc_type,&entries);
    if(entries.data&&entries.count>=0&&entries.count<=32&&entries.capacity>=entries.count)
        for(int i=0;i<entries.count;++i){
            const auto npc=entries.data[i];
            if(!npc||!api.isa(npc,npc_type)||actor_is_destroying(npc))continue;
            const auto reward=catalog_object_field(npc,"Reward Actor");
            if(!reward||!api.isa(reward,pickup_type)||actor_is_destroying(reward)||
               !api.getbool(transient,reward,0)||api.getbool(acquired,reward,0))continue;
            unsigned char collected=1;
            api.event(reward,set,&collected);
            api.event(reward,refresh,nullptr);
            ++story_duck_pickups_retired;
        }
    if(entries.data)native_free(entries.data);
}

static void world_story_step(void* pawn,double time){
    auto& state=world_story;
    if(!active_peer_mask||time<state.next)return;
    state.next=time+.5;
    if(world_stream.scripted_world||!world_progress_ready()||!checkpoint_medic_idle()||rollback_busy()||
       local_life.actor!=pawn||local_life.locked||local_life.pending||!local_life.layout.dead||
       api.getbool(local_life.layout.dead,pawn,0))return;
    const auto& binding=medic_runtime.save;
    const auto save=medic_active(binding,pawn);
    if(!save||!world_story_bind())return;
    const auto& keys=state.keys;
    if(medic_marker(binding,save,keys[duck_claimed])<1)return;
    if(host_role||world_story_saved(world_progress.authoritative.names,duck_claimed)>0)
        world_story_refresh_pickup(pawn,time);
    const bool received=medic_marker(binding,save,keys[duck_receipt])>0;
    if(medic_marker(binding,save,keys[duck_unlocked])<1&&state.pending!=StoryReward::Stage::unlocked){
        const auto duck=world_story_duck();
        const auto before=medic_count(binding,duck);
        if(before<0)return;
        if(!before){
            // An older personal receipt can coexist with an earlier shared
            // world where the shop still requires the original item.
            if(!medic_inventory_delta(pawn,duck,1)||medic_active(binding,pawn)!=save||
               medic_count(binding,duck)!=1)return;
            ++story_duck_grants;
        }
    }else if(received){
        const auto duck=world_story_duck();
        if(medic_count(binding,duck)>0&&!medic_inventory_delta(pawn,duck,-1))return;
    }
    if(!received)medic_set_marker(binding,save,keys[duck_receipt],1);
}
