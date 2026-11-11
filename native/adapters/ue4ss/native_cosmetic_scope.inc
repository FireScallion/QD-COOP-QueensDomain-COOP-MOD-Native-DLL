static void* native_cosmetic_actor(void* source){
    const auto scene=world_input(world_projectile_scene);
    if(source&&scene&&api.isa(source,scene)){
        const auto owner=factory_object(factory_owner);
        if(!owner||*api.parms(owner)!=8)return nullptr;
        void* actor{};
        api.event(source,owner,&actor);
        return actor;
    }
    return source;
}
static bool native_replica_author(void* source){
    if(host_role)return false;
    const auto root=native_cosmetic_actor(source),world=api.weakget(&attached_world);
    if(!root||!world||!base_class||!api.isa(root,actor_class)||native_actor_world(root)!=world)return false;
    for(auto actor:{root,catalog_object_field(root,"Instigator")}){
        for(unsigned depth=0;actor&&depth<4&&api.isa(actor,actor_class);++depth){
            if(api.isa(actor,base_class))
                for(const auto& owned:ownership)
                    if(owned.active&&api.weakget(&owned.objects[0])==actor)return true;
            const auto parent=catalog_object_field(actor,"Owner");
            if(parent==actor)break;
            actor=parent;
        }
    }
    return false;
}
