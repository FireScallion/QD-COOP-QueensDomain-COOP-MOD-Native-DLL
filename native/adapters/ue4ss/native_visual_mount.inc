namespace Mount=qdcoop::visual_mount;
struct MountSource {
    Weak component;
    Mount::Frame frame;
    double next_send{},next_material{},terminal_until{};
    bool world_reference{};
};
struct MountDisplay {
    Mount::Frame frame;
    Weak component,owner,parent;
    SuppressedEffect hidden;
    std::array<std::uint16_t,Mount::material_slots> materials{};
    GearTransform transform{};
    std::uint64_t applied{};
    std::uint16_t asset{};
    double expires{},retry{};
    bool skeletal{},visible{},ticking{},placed{};
};
static std::array<MountSource,Mount::capacity> mount_sources;
static std::array<MountDisplay,Mount::capacity> mount_displays;
static std::array<std::array<MountDisplay,Mount::capacity>,Player::slots> mount_players;
static GearAsset mount_static_assets[std::size(Mount::assets::mesh_paths)];
static GearAsset mount_skeletal_assets[std::size(Mount::assets::skeletal_paths)];
static GearAsset mount_material_assets[std::size(Mount::assets::material_paths)];
static unsigned mount_scan_cursor,mount_capture_cursor;
static double mount_next_scan,mount_next_capture,mount_next_hands;

static unsigned mount_copy_components(const NativeActorArray& array,std::span<void*> out){
    unsigned count{};
    if(array.data&&array.count>0&&array.count<=256&&array.capacity>=array.count){
        count=std::min(static_cast<unsigned>(array.count),static_cast<unsigned>(out.size()));
        std::copy_n(array.data,count,out.data());
    }
    if(array.data)native_free(const_cast<void*>(static_cast<const void*>(array.data)));
    return count;
}

static unsigned mount_components(void* actor,std::span<void*> out){
    const auto scene=world_input(world_projectile_scene),get=factory_object(factory_audio);
    if(!actor||!scene||!get||!native_free)return 0;
    alignas(16) FactoryParameters args{};
    factory_put(args,factory_offsets.audio_class,scene);
    api.event(actor,get,args.data());
    return mount_copy_components(AnimU::field<NativeActorArray>(args.data(),factory_offsets.audio_result),out);
}

static unsigned mount_children(void* parent,std::span<void*> out){
    if(!parent||!native_free||!medic_params(world_mount_children,24,{{"bIncludeAllDescendants",1,0},{"Children",16,8}}))return 0;
    alignas(16) FactoryParameters args{};
    args[0]=1;
    api.event(parent,world_input(world_mount_children),args.data());
    return mount_copy_components(AnimU::field<NativeActorArray>(args.data(),8),out);
}

static void* mount_component_owner(void* component){
    void* owner{};
    if(const auto get=factory_object(factory_owner))api.event(component,get,&owner);
    return owner;
}

template<std::size_t N>
static std::uint16_t mount_asset_id(void* asset,const std::string_view (&paths)[N]){
    std::string path;
    if(!asset||!catalog_path(asset,path))return 0;
    const auto it=std::ranges::lower_bound(paths,path);
    return it!=std::end(paths)&&*it==path?static_cast<std::uint16_t>(it-std::begin(paths)):0;
}

static void* mount_mesh_asset(void* component,bool skeletal){
    return catalog_object_field(component,skeletal?"SkeletalMesh":"StaticMesh");
}
static std::uint16_t mount_mesh_id(void* component,bool skeletal){
    const auto asset=mount_mesh_asset(component,skeletal);
    return skeletal?mount_asset_id(asset,Mount::assets::skeletal_paths):mount_asset_id(asset,Mount::assets::mesh_paths);
}
static std::string mount_name(void* object){
    std::string text;
    if(object)AnimU::name_string(catalog_name_pool,AnimU::field<AnimU::NameKey>(object,24),catalog_read,text);
    return text;
}
static GearTransform mount_socket(void* mesh,AnimU::NameKey socket){
    alignas(16) FactoryParameters args{};
    factory_put(args,gear_offsets.socket_name,socket);
    api.event(mesh,world_input(world_socket_transform),args.data());
    return AnimU::field<GearTransform>(args.data(),gear_offsets.socket_result);
}
static bool mount_bone(void* mesh,AnimU::NameKey socket){
    const auto get=world_input(world_get_bone_index);
    if(!get||!medic_params(world_get_bone_index,12,{{"BoneName",8,0},{"ReturnValue",4,8}}))return false;
    alignas(8) FactoryParameters args{};
    factory_put(args,0,socket);
    api.event(mesh,get,args.data());
    return AnimU::field<int>(args.data(),8)>=0;
}

static std::optional<GearTransform> mount_basis(void* component,void* actor,void* mesh,bool cross_actor,Mount::Frame& frame){
    frame.anchor=Mount::Anchor::root;
    frame.socket.clear();
    const auto root=catalog_object_field(actor,"RootComponent");
    auto child=component;
    for(unsigned depth=0;child&&depth<12;++depth){
        void* parent{};
        api.event(child,world_input(world_scene_parent),&parent);
        if(parent==mesh){
            AnimU::NameKey socket{};
            api.event(child,world_input(world_scene_socket),&socket);
            std::string name;
            if(socket.index&&AnimU::name_string(catalog_name_pool,socket,catalog_read,name)&&Mount::label(name)&&mount_bone(mesh,socket)){
                frame.anchor=Mount::Anchor::bone;
                frame.socket=std::move(name);
                return mount_socket(mesh,socket);
            }
            frame.anchor=Mount::Anchor::mesh;
            return gear_world_transform(mesh);
        }
        if(root&&parent==root)return gear_world_transform(root);
        child=parent;
    }
    if(cross_actor)return std::nullopt;
    if(root)return gear_world_transform(root);
    frame.anchor=Mount::Anchor::mesh;
    return gear_world_transform(mesh);
}

static void mount_register(unsigned owner,std::uint32_t generation,bool player,void* component,void* actor,void* mesh,
                           bool world_reference=false){
    if(!component||component==mesh)return;
    if(std::ranges::any_of(projectile_sources,[&](auto& source){return api.weakget(&source.mesh.component)==component;}))return;
    const bool skeletal=api.isa(component,world_input(world_pose_component));
    if(!skeletal&&!api.isa(component,world_input(world_static_component)))return;
    const auto component_owner=mount_component_owner(component);
    if(!projectile_visible(component,component_owner?component_owner:actor))return;
    const auto previous=std::ranges::find_if(mount_sources,[&](auto& source){return api.weakget(&source.component)==component;});
    if(previous!=mount_sources.end()){
        previous->world_reference|=world_reference;
        return;
    }
    const auto asset=mount_mesh_id(component,skeletal);
    auto name=mount_name(component);
    if(!asset||!Mount::label(name))return;
    const auto slot=std::ranges::find_if(mount_sources,[](const auto& source){return !source.frame.owner;});
    if(slot==mount_sources.end()||slot->frame.generation==UINT32_MAX||!makeweak(slot->component,component))return;
    auto& frame=slot->frame;
    const auto next=frame.generation+1;
    frame={};
    frame.stream=static_cast<unsigned>(slot-mount_sources.begin())+1;
    frame.generation=next;
    frame.owner=owner;
    frame.owner_generation=generation;
    frame.player=player;
    frame.asset=asset;
    frame.skeletal=skeletal;
    frame.name=std::move(name);
    slot->next_send=slot->next_material=slot->terminal_until=0;
    slot->world_reference=world_reference;
}

static void mount_discover_references(unsigned index,void* actor,void* mesh){
    constexpr auto& refs=qdcoop::enemy_visual_catalog::references;
    const auto niagara=world_input(world_projectile_niagara),cascade=world_input(world_projectile_cascade_component);
    auto particle_budget=static_cast<int>(Projectile::capacity)-8-static_cast<int>(std::ranges::count_if(
        projectile_sources,[](const auto& source){return source.frame.id!=0;}));
    auto type=AnimU::field<void*>(actor,16);
    for(unsigned depth=0;type&&depth<24;++depth,type=AnimU::field<void*>(type,64)){
        std::string path;
        if(!catalog_path(type,path))continue;
        auto ref=std::ranges::lower_bound(refs,path,std::ranges::less{},&qdcoop::enemy_visual_catalog::Reference::owner);
        for(;ref!=std::end(refs)&&ref->owner==path;++ref){
            const auto component=catalog_object_field(actor,ref->field);
            if(!component||component==mesh)continue;
            const bool is_cascade=cascade&&api.isa(component,cascade);
            if(is_cascade||(niagara&&api.isa(component,niagara))){
                if(particle_budget>0&&!projectile_tracked_effect(component)&&
                   projectile_observe_effect(actor,component,true,is_cascade))--particle_budget;
            }else mount_register(index+1,entities[index].generation,false,component,actor,mesh,true);
        }
        if(type==base_class)break;
    }
}

static void mount_discover(){
    unsigned budget=3;
    for(unsigned scanned=0;scanned<E::capacity&&budget;++scanned){
        const auto index=mount_scan_cursor++%E::capacity;
        const auto& enemy=entities[index];
        if(!enemy.used||!enrollment_peer_mask(index))continue;
        const auto actor=api.weakget(&enemy.actor),mesh=api.weakget(&enemy.mesh);
        if(!actor||!mesh)continue;
        --budget;
        std::array<void*,96> components{};
        const auto count=mount_components(actor,components);
        for(unsigned i=0;i<count;++i)mount_register(index+1,enemy.generation,false,components[i],actor,mesh);
        const auto root=catalog_object_field(actor,"RootComponent");
        const auto children=mount_children(root?root:mesh,components);
        for(unsigned i=0;i<children;++i)mount_register(index+1,enemy.generation,false,components[i],actor,mesh);
        mount_discover_references(index,actor,mesh);
    }
}

static bool mount_known_gear(void* component){
    return std::ranges::any_of(gear_source.parts,[&](auto& part){return part.id&&api.weakget(&part.component)==component;});
}

static void mount_discover_hands(){
    const auto pawn=api.weakget(&player_source);
    const auto arms=pawn?catalog_object_field(pawn,"Arms"):nullptr;
    if(!arms||!player_generation)return;
    std::array<void*,96> children{};
    const auto count=mount_children(arms,children);
    const auto owner=host_role?1u:Player::slots+1;
    for(unsigned i=0;i<count;++i)
        if(children[i]&&!mount_known_gear(children[i]))mount_register(owner,player_generation,true,children[i],pawn,arms);
}

static void mount_materials(void* component,Mount::Frame& frame){
    frame.materials={};
    const auto count=std::clamp(gear_material_count(component),0,static_cast<int>(Mount::material_slots));
    for(int i=0;i<count;++i)frame.materials[i]=stock_visual_material_id(body_material_at(component,i));
}

static void mount_capture(MountSource& source,double time){
    auto& frame=source.frame;
    const auto index=frame.owner-1;
    const auto& enemy=entities[index];
    const bool current=frame.player?player_generation==frame.owner_generation:enemy.used&&enemy.generation==frame.owner_generation;
    const auto actor=current?api.weakget(frame.player?&player_source:&enemy.actor):nullptr;
    const auto mesh=actor?(frame.player?catalog_object_field(actor,"Arms"):api.weakget(&enemy.mesh)):nullptr;
    const auto component=api.weakget(&source.component);
    const auto hand=frame.player&&component&&mesh?gear_hand(component,mesh,catalog_object_field(actor,"WeaponAttachPoint_L")):0;
    const auto asset=component&&actor&&mesh&&(!frame.player||(hand&&!mount_known_gear(component)))?
        mount_mesh_id(component,frame.skeletal):0;
    const auto component_owner=asset?mount_component_owner(component):nullptr;
    auto candidate=frame;
    const auto basis=!asset?std::optional<GearTransform>{}:frame.player?gear_world_transform(mesh):
        mount_basis(component,actor,mesh,component_owner&&component_owner!=actor,candidate);
    const bool world=asset&&source.world_reference&&!basis;
    if(!basis&&!world){
        if(!source.terminal_until){
            const auto stream=frame.stream,generation=frame.generation,owner=frame.owner,owner_generation=frame.owner_generation;
            const auto sequence=frame.sequence;
            const auto player=frame.player;
            frame={};
            frame.stream=stream;frame.generation=generation;frame.owner=owner;frame.owner_generation=owner_generation;
            frame.sequence=sequence+1;
            frame.player=player;
            source.component={};
            source.terminal_until=time+1.5;
            source.next_send=time;
        }else if(time>=source.terminal_until){frame.owner=0;return;}
    }else{
        candidate.asset=asset;
        candidate.active=true;
        candidate.visible=projectile_visible(component,component_owner?component_owner:actor);
        auto transform=world?gear_world_transform(component):gear_relative(component,*basis);
        if(world){candidate.anchor=Mount::Anchor::world;candidate.socket.clear();}
        if(frame.player){
            candidate.anchor=hand==2?Mount::Anchor::hand_left:Mount::Anchor::hand_right;
            candidate.socket.clear();
            const auto offset=gear_socket(mesh,gear_hands[hand-1]);
            for(unsigned i=4;i<7;++i)transform[i]-=offset[i];
        }
        std::copy_n(transform.begin(),7,candidate.relative.begin());
        std::copy_n(transform.begin()+8,3,candidate.relative.begin()+7);
        for(unsigned i=0;i<candidate.relative.size();++i)
            if(std::abs(candidate.relative[i]-frame.relative[i])<.0002f)candidate.relative[i]=frame.relative[i];
        if(time>=source.next_material||candidate.asset!=frame.asset){
            mount_materials(component,candidate);
            source.next_material=time+.35;
        }
        const auto dissolve=body_material_override(body_material_at(component,0));
        candidate.has_dissolve=dissolve.has_value();
        candidate.dissolve=dissolve.value_or(0.f);
        if(candidate!=frame){
            ++candidate.sequence;
            if(!Mount::valid(candidate))return;
            frame=std::move(candidate);
            source.next_send=time;
        }
    }
    const auto mask=frame.player?(host_role?active_peer_mask:1u):current?enrollment_peer_mask(index):active_peer_mask;
    E::Packet packet{};
    if(mask&&time>=source.next_send&&Mount::encode(envelope,frame,packet)&&send(packet,mask)){
        source.next_send=time+(frame.active?.65:.25);
        ++visual_mount_sent;
    }
}

static bool mount_destroy(MountDisplay& display){
    if(!projectile_restore_effect(display.hidden))return false;
    if(const auto component=api.weakget(&display.component)){
        auto owner=api.weakget(&display.owner);
        const auto destroy=world_input(world_destroy_component);
        if(!destroy)return false;
        projectile_effect_display(component,false,true,false);
        api.event(component,destroy,&owner);
        if(api.weakget(&display.component)==component)return false;
        ++visual_mount_removed;
    }
    display.component=display.owner=display.parent={};
    display.materials={};
    display.asset=0;
    display.applied=0;
    display.placed=display.ticking=false;
    return true;
}

static void* mount_stock_component(void* actor,const Mount::Frame& frame){
    std::array<void*,96> components{};
    const auto count=mount_components(actor,components);
    const auto type=world_input(frame.skeletal?world_pose_component:world_static_component);
    for(unsigned i=0;i<count;++i){
        const auto component=components[i];
        if(component&&api.isa(component,type)&&mount_name(component)==frame.name&&mount_mesh_id(component,frame.skeletal)==frame.asset)
            return component;
    }
    return nullptr;
}

static void mount_hide_stock(MountDisplay& display,void* component){
    if(!component)return;
    const auto hidden=catalog_field(AnimU::field<void*>(component,16),"bHiddenInGame",1);
    if(!medic_field_class(hidden,"BoolProperty")||!own_meta(3)||!makeweak(display.hidden.component,component))return;
    alignas(8) FactoryParameters args{};
    api.event(component,getters[5].fn,args.data());
    display.hidden.visible=api.getbool(getters[5].ret,args.data(),0);
    display.hidden.hidden=api.getbool(hidden,component,0);
    display.hidden.ticking=own_bool(component,3);
    projectile_effect_display(component,false,true,false);
}

static void mount_skeletal_follow(MountDisplay& display,void* component,void* stock,void* actor,void* pose){
    if(!display.frame.skeletal||!stock||!pose)return;
    const auto asset=mount_mesh_asset(stock,true),parent=mount_mesh_asset(pose,true);
    const auto skeleton=asset?catalog_object_field(asset,"Skeleton"):nullptr;
    if(!skeleton||!parent||catalog_object_field(parent,"Skeleton")!=skeleton)return;
    const auto mode=catalog_field(AnimU::field<void*>(stock,16),"Attachment Is ChildPose",1);
    const auto master=catalog_field(AnimU::field<void*>(stock,16),"MasterPoseComponent",8);
    const auto follower=medic_field_class(master,"WeakObjectProperty")?
        AnimU::resolve_weak(native_object_array,AnimU::field<AnimU::WeakKey>(stock,*api.offset(master)),catalog_read):0;
    if(medic_field_class(mode,"BoolProperty")&&api.getbool(mode,stock,0)&&
       follower&&reinterpret_cast<void*>(follower)==catalog_object_field(actor,"Mesh")&&
       medic_params(world_mount_master,9,{{"NewMasterBoneComponent",8,0},{"bForceUpdate",1,8}})){
        alignas(8) FactoryParameters args{};
        factory_put(args,0,pose);
        args[8]=1;
        api.event(component,world_input(world_mount_master),args.data());
        return;
    }
    const auto anim=catalog_field(AnimU::field<void*>(stock,16),"AnimClass",8);
    auto type=medic_field_class(anim,"ClassProperty")?AnimU::field<void*>(stock,*api.offset(anim)):nullptr;
    std::string path;
    if(!type||!catalog_path(type,path)||!medic_params(world_mount_anim,8,{{"NewClass",8,0}}))return;
    constexpr std::array cloth{
        "/Game/Enemies/Boner_00/Skeletons24/ShrineBoss/Boner_ShrineBoss_PPABP.Boner_ShrineBoss_PPABP_C",
        "/Game/Enemies/JosamiKnight_00/Josami24/JosamiKnight_Cloth_A_ABP.JosamiKnight_Cloth_A_ABP_C",
        "/Game/Enemies/JosamiKnight_00/Josami24/JosamiKnight_Cloth_B_ABP.JosamiKnight_Cloth_B_ABP_C"};
    if(std::ranges::find(cloth,path)==cloth.end())return;
    api.event(component,world_input(world_mount_anim),&type);
    const auto tick_option=catalog_field(AnimU::field<void*>(component,16),"VisibilityBasedAnimTickOption",1);
    if(medic_field_class(tick_option,"ByteProperty")||medic_field_class(tick_option,"EnumProperty"))
        static_cast<unsigned char*>(component)[*api.offset(tick_option)]=0;
    display.ticking=true;
}

static bool mount_create(MountDisplay& display,void* owner,void* actor,void* parent,void* pose,double time){
    const auto& frame=display.frame;
    const auto id=frame.asset;
    const auto type=world_input(frame.skeletal?world_pose_component:world_static_component);
    const auto asset=frame.skeletal?
        gear_load(mount_skeletal_assets[id],Mount::assets::skeletal_paths[id],time,catalog_skeletal_mesh,nullptr):
        gear_load(mount_static_assets[id],Mount::assets::mesh_paths[id],time,UINT32_MAX,world_input(world_static_asset));
    const auto attach=world_input(world_attach_component);
    const auto socket_at=factory_parameter(attach,"SocketName",8);
    const auto socket=frame.socket.empty()?AnimU::NameKey{}:placed_pickup_name(frame.socket);
    if(!asset||!type||socket_at<0||(frame.anchor==Mount::Anchor::bone&&(!socket.index||!mount_bone(parent,socket)))||
       !makeweak(display.owner,owner)||!makeweak(display.parent,parent))return false;
    constexpr GearTransform identity{0,0,0,1,0,0,0,0,1,1,1,0};
    alignas(16) FactoryParameters args{};
    factory_put(args,gear_offsets.add_type,type);
    factory_put(args,gear_offsets.add_manual,std::uint8_t{1});
    factory_put(args,gear_offsets.add_transform,identity);
    api.event(owner,world_input(world_add_component),args.data());
    const auto component=AnimU::field<void*>(args.data(),gear_offsets.add_result);
    if(!component||!api.isa(component,type))return false;
    if(!makeweak(display.component,component)){
        api.event(component,world_input(world_destroy_component),&owner);
        return false;
    }
    unsigned char disabled{};
    api.event(component,world_input(world_component_collision),&disabled);
    api.event(component,world_input(world_component_overlap),&disabled);
    args={};
    factory_put(args,gear_offsets.attach_parent,parent);
    factory_put(args,socket_at,socket);
    const bool world=frame.anchor==Mount::Anchor::world;
    if(!world)api.event(component,attach,args.data());
    const bool attached=world||AnimU::field<std::uint8_t>(args.data(),gear_offsets.attach_result)!=0;
    bool bound{};
    if(frame.skeletal)bound=factory_apply_skinned_mesh(component,asset,type);
    else{
        args={};
        factory_put(args,gear_offsets.static_asset,asset);
        api.event(component,world_input(world_set_static),args.data());
        bound=AnimU::field<std::uint8_t>(args.data(),gear_offsets.static_result)!=0;
    }
    if(!attached||!bound){mount_destroy(display);return false;}
    const auto stock=frame.player?nullptr:mount_stock_component(actor,frame);
    mount_hide_stock(display,stock);
    mount_skeletal_follow(display,component,stock,actor,pose);
    display.asset=id;
    display.skeletal=frame.skeletal;
    ++visual_mount_spawned;
    return true;
}

static bool receive_visual_mount(const ReceivedPacket& received,unsigned kind,double time){
    if(kind!=Mount::kind)return false;
    Mount::Frame frame;
    if(!active_peer_mask||!Mount::decode(envelope,received.packet,frame))return true;
    unsigned relay{};
    if(frame.player){
        auto slot=frame.owner==Player::slots+1?Player::unassigned:frame.owner-1;
        if(!Player::assign_slot(slot,host_role,received.peer,active_peer_mask,relay)||
           remote_players[slot].generation!=frame.owner_generation)return true;
        frame.owner=slot+1;
    }else if(host_role||received.peer)return true;
    auto& display=frame.player?mount_players[frame.owner-1][frame.stream-1]:mount_displays[frame.stream-1];
    const auto& old=display.frame;
    if(frame.generation<old.generation||(frame.generation==old.generation&&frame.sequence<old.sequence)||
       (frame.generation==old.generation&&frame.sequence==old.sequence&&frame!=old))return true;
    if((frame.generation!=old.generation||frame.anchor!=old.anchor||frame.socket!=old.socket||
        (old.has_dissolve&&!frame.has_dissolve))&&!mount_destroy(display))return true;
    display.frame=std::move(frame);
    display.expires=time+3.;
    ++visual_mount_received;
    E::Packet packet{};
    if(relay&&Mount::encode(envelope,display.frame,packet))send(packet,relay);
    return true;
}

static void mount_render(MountDisplay& display,double time){
    const auto& frame=display.frame;
    if(!frame.sequence)return;
    visual_mount_stage=1;
    const auto& entity=entities[frame.owner-1];
    const auto player=frame.player?&remote_players[frame.owner-1]:nullptr;
    const bool dead=!player&&deaths[frame.owner-1].terminal;
    const auto actor=api.weakget(player?&player->actor:&entity.actor);
    const auto pose=api.weakget(player?&player->equipment.body:dead?&entity.mesh:&entity.visual_mesh);
    const auto owner=player||dead?actor:api.weakget(&entity.visual);
    const auto parent=frame.anchor==Mount::Anchor::root&&actor?catalog_object_field(actor,"RootComponent"):pose;
    const bool current=player?player->generation==frame.owner_generation:entity.used&&entity.generation==frame.owner_generation;
    if(!frame.active||time>=display.expires||!current||!owner||!parent){
        if(display.asset||api.weakget(&display.component)||api.weakget(&display.hidden.component))mount_destroy(display);
        return;
    }
    visual_mount_stage=2;
    if(display.asset!=frame.asset||display.skeletal!=frame.skeletal||api.weakget(&display.parent)!=parent){
        if(!mount_destroy(display)||time<display.retry)return;
    }
    auto component=api.weakget(&display.component);
    if(!component){
        if(time<display.retry)return;
        display.retry=time+.65;
        if(!mount_create(display,owner,actor,parent,pose,time))return;
        component=api.weakget(&display.component);
        if(!component)return;
    }
    visual_mount_stage=3;
    const bool fresh=display.applied!=frame.sequence;
    if(!fresh&&!frame.player)return;
    const auto& t=frame.relative;
    GearTransform relative{t[0],t[1],t[2],t[3],t[4],t[5],t[6],0,t[7],t[8],t[9],0};
    if(player){
        const auto offset=gear_socket(pose,gear_hands[frame.anchor==Mount::Anchor::hand_left]);
        for(unsigned i=4;i<7;++i)relative[i]+=offset[i];
    }
    if(!display.placed||relative!=display.transform){
        alignas(16) FactoryParameters args{};
        factory_put(args,gear_offsets.new_transform,relative);
        factory_put(args,gear_offsets.teleport,std::uint8_t{1});
        api.event(component,frame.anchor==Mount::Anchor::world?mesh_fn:world_input(world_relative_set),args.data());
        display.transform=relative;
    }
    if(!fresh)return;
    for(unsigned i=0;i<display.materials.size();++i){
        const auto id=frame.materials[i];
        if(id==display.materials[i])continue;
        const auto material=id?gear_load(mount_material_assets[id],Mount::assets::material_paths[id],time,
                                        UINT32_MAX,world_input(world_material_type)):nullptr;
        if(id&&!material)return;
        gear_set_material(component,static_cast<int>(i),material);
        display.materials[i]=id;
    }
    if(frame.has_dissolve)body_material_write(component,world_body_scalar,frame.dissolve);
    if(!display.placed||display.visible!=frame.visible){
        projectile_effect_display(component,frame.visible,!frame.visible,display.ticking);
        display.visible=frame.visible;
    }
    display.placed=true;
    display.applied=frame.sequence;
}

static void native_visual_mount_step(double time){
    if(!active_peer_mask||time<mount_next_capture||!prepare_gear(time)||!body_material_ready(time))return;
    mount_next_capture=time+.055;
    if(!host_role)for(auto& display:mount_displays)mount_render(display,time);
    for(auto& displays:mount_players)for(auto& display:displays)mount_render(display,time);
    if(time>=mount_next_hands){
        mount_next_hands=time+.28;
        mount_discover_hands();
    }
    if(host_role&&time>=mount_next_scan){mount_next_scan=time+.20;mount_discover();}
    unsigned budget=12;
    for(unsigned checked=0;checked<Mount::capacity&&budget;++checked){
        auto& source=mount_sources[mount_capture_cursor++%Mount::capacity];
        if(!source.frame.owner)continue;
        mount_capture(source,time);
        --budget;
    }
}

static bool native_visual_mount_cleanup(){
    bool clean=true;
    for(auto& display:mount_displays){
        if(mount_destroy(display))display={};
        else clean=false;
    }
    for(auto& displays:mount_players)for(auto& display:displays){
        if(mount_destroy(display))display={};
        else clean=false;
    }
    mount_sources={};
    mount_scan_cursor=mount_capture_cursor=0;
    mount_next_scan=mount_next_capture=mount_next_hands=0;
    return clean;
}
