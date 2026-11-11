struct SuppressedEffect {
    Weak component;
    bool visible{},hidden{},ticking{};
};
static std::array<SuppressedEffect,128> suppressed_effects;

static bool projectile_effect_display(void* component,bool visible,bool hidden,bool ticking){
    const auto hide=world_input(world_barrier_hidden),tick=world_input(world_component_tick);
    if(!visible_fn||*api.parms(visible_fn)!=2||
       !medic_params(world_barrier_hidden,2,{{"NewHidden",1,0},{"bPropagateToChildren",1,1}})||
       !tick||*api.parms(tick)!=1)return false;
    unsigned char visibility[]{static_cast<unsigned char>(visible),0};
    unsigned char visibility_hidden[]{static_cast<unsigned char>(hidden),0};
    api.event(component,visible_fn,visibility);
    api.event(component,hide,visibility_hidden);
    own_set(component,tick,ticking);
    return true;
}
static bool projectile_restore_effect(SuppressedEffect& entry){
    if(const auto component=api.weakget(&entry.component))
        if(!projectile_effect_display(component,entry.visible,entry.hidden,entry.ticking))return false;
    entry={};
    return true;
}
static bool projectile_suppress_replica(void* source,void* component){
    if(host_role||!component)return false;
    const bool suppress=native_replica_author(source);
    SuppressedEffect* slot{};
    for(auto& entry:suppressed_effects){
        const auto current=api.weakget(&entry.component);
        if(current==component){
            if(!suppress)return !projectile_restore_effect(entry);
            if(projectile_effect_display(component,false,true,false))++projectile_replica_suppressed;
            return true;
        }
        if(!current&&!slot)slot=&entry;
    }
    if(!suppress)return false;
    const auto type=AnimU::field<void*>(component,16);
    const auto hidden=catalog_field(type,"bHiddenInGame",1),tick=own_meta(3);
    if(!slot||!getters[5].fn||!getters[5].ret||!tick||*api.parms(tick)!=1||
       !medic_field_class(hidden,"BoolProperty"))return true;
    SuppressedEffect saved;
    if(!makeweak(saved.component,component))return true;
    alignas(16) FactoryParameters args{};
    api.event(component,getters[5].fn,args.data());
    saved.visible=api.getbool(getters[5].ret,args.data(),0);
    saved.hidden=api.getbool(hidden,component,0);
    saved.ticking=own_bool(component,3);
    if(projectile_effect_display(component,false,true,false)){
        *slot=saved;
        ++projectile_replica_suppressed;
    }
    return true;
}
