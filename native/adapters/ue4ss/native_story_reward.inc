struct WorldStoryReward {
    Weak duck;
    std::array<AnimU::NameKey,3> keys;
    double next{};
    bool bound{};
};
static WorldStoryReward world_story;
static constexpr std::array story_duck_keys{
    L"ShrineBossRewardCount",L"ShopUnlockFinished",L"QDCoopPersonal~story~stoneDuck"
};
static void world_story_reset(){world_story={};}

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

static void world_story_step(void* pawn,double time){
    auto& state=world_story;
    if(!active_peer_mask||time<state.next)return;
    state.next=time+.5;
    if(world_stream.scripted_world||!world_progress_ready()||!checkpoint_medic_idle()||rollback_busy()||
       local_life.actor!=pawn||local_life.locked||local_life.pending||!local_life.layout.dead||
       api.getbool(local_life.layout.dead,pawn,0))return;
    const auto& binding=medic_runtime.save;
    const auto save=medic_active(binding,pawn);
    if(!save)return;
    if(!state.bound){
        for(unsigned i=0;i<state.keys.size();++i){
            state.keys[i]=medic_name(story_duck_keys[i]);
            if(!medic_name_is(state.keys[i],story_duck_keys[i]))return;
        }
        state.bound=true;
    }
    const auto& [claimed,unlocked,receipt]=state.keys;
    if(medic_marker(binding,save,claimed)<1)return;
    if(medic_marker(binding,save,unlocked)<1){
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
    }
    if(medic_marker(binding,save,receipt)<1)medic_set_marker(binding,save,receipt,1);
}
