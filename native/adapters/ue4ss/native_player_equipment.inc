// Player cosmetics are value-only on the wire. Only our display components
// are created, attached, recolored or removed.
static std::atomic<unsigned> local_player_skin{1};
extern "C" __declspec(dllexport) int QDEnemySetNativeSkin(unsigned skin){
    if(!Player::valid_skin(skin))return 0;
    local_player_skin.store(skin,std::memory_order_relaxed);
    return 1;
}

using GearTransform=std::array<float,12>;
struct GearOffsets {
    int socket_name{},socket_space{},socket_result{};
    int rel_child{},rel_parent{},rel_result{};
    int add_type{},add_manual{},add_transform{},add_result{};
    int attach_parent{},attach_result{};
    int static_asset{},static_result{},new_transform{},teleport{};
};
static GearOffsets gear_offsets;
static std::array<AnimU::NameKey,2> gear_hands{};
static AnimU::NameKey gear_face{};
static bool gear_ready;
static double gear_next_prepare;

static AnimU::NameKey gear_name(const wchar_t* value){
    const auto fn=world_input(world_string_name);
    if(!fn)return {};
    const auto input=AnimU::field<void*>(fn,80);
    const auto output=factory_parameter(fn,"ReturnValue",8);
    if(*api.parms(fn)!=24||!property(input,16,24)||*api.offset(input)!=0||
       !(AnimU::field<std::uint64_t>(input,64)&0x80u)||output!=16){
        player_equipment_stage=260;
        return {};
    }
    struct StringArgument {const wchar_t* data;int count,capacity;};
    const auto size=static_cast<int>(std::wcslen(value)+1);
    alignas(16) FactoryParameters args{};
    factory_put(args,0,StringArgument{value,size,size});
    api.event(world_input(world_strings),fn,args.data());
    return AnimU::field<AnimU::NameKey>(args.data(),output);
}

static bool prepare_gear(double time){
    if(gear_ready)return true;
    if(time<gear_next_prepare)return false;
    gear_next_prepare=time+.25;
    player_equipment_stage=1;
    for(unsigned id=world_scene_parent;id<world_bone_first;++id){
        if(!world_input(id)){
            player_equipment_stage=100+id-world_scene_parent;
            scan_world_inputs(2048);
            return false;
        }
    }
    const auto socket=world_input(world_socket_transform),relative=world_input(world_make_relative);
    const auto add=world_input(world_add_component),attach=world_input(world_attach_component);
    const auto mesh=world_input(world_set_static),set=world_input(world_relative_set);
    GearOffsets offsets{
        factory_parameter(socket,"InSocketName",8),factory_parameter(socket,"TransformSpace",1),
        factory_parameter(socket,"ReturnValue",48),factory_parameter(relative,"A",48),
        factory_parameter(relative,"RelativeTo",48),factory_parameter(relative,"ReturnValue",48),
        factory_parameter(add,"Class",8),factory_parameter(add,"bManualAttachment",1),
        factory_parameter(add,"RelativeTransform",48),factory_parameter(add,"ReturnValue",8),
        factory_parameter(attach,"Parent",8),factory_parameter(attach,"ReturnValue",1),
        factory_parameter(mesh,"NewMesh",8),factory_parameter(mesh,"ReturnValue",1),
        factory_parameter(set,"NewTransform",48),factory_parameter(set,"bTeleport",1)
    };
    const int fields[]{offsets.socket_name,offsets.socket_space,offsets.socket_result,
        offsets.rel_child,offsets.rel_parent,offsets.rel_result,offsets.add_type,offsets.add_manual,
        offsets.add_transform,offsets.add_result,offsets.attach_parent,offsets.attach_result,
        offsets.static_asset,offsets.static_result,offsets.new_transform,offsets.teleport};
    player_equipment_stage=2;
    for(unsigned i=0;i<std::size(fields);++i)
        if(fields[i]<0){player_equipment_stage=200+i;return false;}
    const std::pair<WorldObject,int> sizes[]{
        {world_scene_parent,8},{world_scene_socket,8},{world_destroy_component,8},
        {world_component_collision,1},{world_component_overlap,1},{world_material_count,4},
        {world_get_material,16},{world_set_material,16},{world_material_slot,12}
    };
    for(unsigned i=0;i<std::size(sizes);++i){
        const auto [id,size]=sizes[i];
        if(*api.parms(world_input(id))!=size){player_equipment_stage=230+i;return false;}
    }
    if(factory_parameter(world_input(world_get_material),"ElementIndex",4)!=0||
       factory_parameter(world_input(world_get_material),"ReturnValue",8)!=8||
       factory_parameter(world_input(world_set_material),"ElementIndex",4)!=0||
       factory_parameter(world_input(world_set_material),"Material",8)!=8||
       factory_parameter(world_input(world_material_slot),"MaterialSlotName",8)!=0||
       factory_parameter(world_input(world_material_slot),"ReturnValue",4)!=8){
        player_equipment_stage=250;
        return false;
    }
    gear_hands={gear_name(L"Hand_R"),gear_name(L"Hand_L")};
    gear_face=gear_name(L"QD_Face");
    if(!gear_hands[0].index||!gear_hands[1].index||!gear_face.index){
        if(player_equipment_stage==2)player_equipment_stage=270+(!gear_hands[0].index)+
            2*(!gear_hands[1].index)+4*(!gear_face.index);
        return false;
    }
    gear_offsets=offsets;
    gear_ready=true;
    player_equipment_stage=3;
    return true;
}

static GearTransform gear_socket(void* component,AnimU::NameKey name){
    alignas(16) FactoryParameters args{};
    factory_put(args,gear_offsets.socket_name,name);
    factory_put(args,gear_offsets.socket_space,std::uint8_t{2}); // Component space.
    api.event(component,world_input(world_socket_transform),args.data());
    return AnimU::field<GearTransform>(args.data(),gear_offsets.socket_result);
}

static GearTransform gear_world_transform(void* component){
    alignas(16) FactoryParameters args{};
    api.event(component,getters[4].fn,args.data());
    return AnimU::field<GearTransform>(args.data(),getters[4].at);
}

static GearTransform gear_relative(void* component,const GearTransform& parent){
    alignas(16) FactoryParameters args{};
    factory_put(args,gear_offsets.rel_child,gear_world_transform(component));
    factory_put(args,gear_offsets.rel_parent,parent);
    api.event(world_input(world_math),world_input(world_make_relative),args.data());
    return AnimU::field<GearTransform>(args.data(),gear_offsets.rel_result);
}

template<std::size_t N,class Path>
static std::uint16_t gear_catalog_id(void* asset,const unsigned (&order)[N],Path path_at){
    std::string path;
    if(!catalog_path(asset,path))return 0;
    const auto end=std::end(order);
    const auto item=std::lower_bound(std::begin(order),end,path,
        [&](unsigned id,std::string_view wanted){return path_at(id)<wanted;});
    return item!=end&&path_at(*item)==path?static_cast<std::uint16_t>(*item):0;
}

struct SourceGearPart {
    Weak component,mesh;
    std::uint16_t id{};
    std::uint8_t hand{};
    int mesh_at{-1};
    void* hidden{};
    std::array<std::uint16_t,Player::material_slots> materials{};
    std::uint32_t serial{};
    std::uint64_t bone_sequence{};
    bool bone_checked{},bone_valid{};
    std::array<Player::BoneTransform,Player::max_equipment_bones> sent_bones{};
};
struct SourceEquipment {
    Weak pawn_type,arms,right,left,quick_dash,weapon_class;
    std::array<Weak,2> visual_types{};
    std::array<int,2> visual_fields{-1,-1};
    std::array<SourceGearPart,Player::equipment_slots> parts{};
    int arms_at{-1},right_at{-1},left_at{-1},quick_at{-1},left_attach_at{-1};
    std::uint16_t weapon{};
    double next_scan{},next_materials{};
};
static SourceEquipment gear_source;
static std::uint32_t gear_serial;

static std::uint32_t next_gear_serial(){
    if(++gear_serial==0)++gear_serial;
    return gear_serial;
}

static unsigned gear_hand(void* component,void* arms,void* left_attach){
    unsigned hand=1;
    for(unsigned level=0;component&&level<10;++level){
        if(component==arms)return hand;
        if(component==left_attach)return 2;
        AnimU::NameKey socket{};
        api.event(component,world_input(world_scene_socket),&socket);
        if(socket.index){
            std::string name;
            if(AnimU::name_string(catalog_name_pool,socket,catalog_read,name)&&
               (name=="Weapon_L"||name=="Hand_L"||name=="hand_l"))hand=2;
        }
        void* parent{};
        api.event(component,world_input(world_scene_parent),&parent);
        component=parent;
    }
    return 0;
}

static void gear_discover(void* arms,void* left_attach,const std::array<void*,3>& visuals){
    struct Candidate { void* component{};std::string path;unsigned hand{}; };
    std::array<Candidate,64> candidates{};
    unsigned count=0;
    const auto static_type=world_input(world_static_component);
    const auto add=[&](void* component){
        if(!component||count==candidates.size()||
           (!api.isa(component,static_type)&&!api.isa(component,mesh_class)))return;
        if(std::any_of(candidates.begin(),candidates.begin()+count,
                       [&](const Candidate& entry){return entry.component==component;}))return;
        const auto hand=gear_hand(component,arms,left_attach);
        std::string path;
        if(hand&&catalog_path(component,path))candidates[count++]={component,std::move(path),hand};
    };
    for(auto visual:visuals){
        if(!visual||!api.isa(visual,actor_class))continue;
        const auto type=AnimU::field<void*>(visual,16);
        if(const auto field=catalog_field(type,"VisualComps",16)){
            const auto array=AnimU::field<NativeActorArray>(visual,*api.offset(field));
            if(array.data&&array.count>0&&array.count<=64&&array.capacity>=array.count)
                for(int i=0;i<array.count;++i)add(array.data[i]);
        }
        alignas(16) FactoryParameters args{};
        factory_put(args,factory_offsets.audio_class,world_input(world_mesh_component));
        api.event(visual,factory_object(factory_audio),args.data());
        const auto array=AnimU::field<NativeActorArray>(args.data(),factory_offsets.audio_result);
        if(array.data&&array.count>0&&array.count<=64&&array.capacity>=array.count)
            for(int i=0;i<array.count;++i)add(array.data[i]);
        if(array.data)native_free(array.data);
    }
    std::sort(candidates.begin(),candidates.begin()+count,[](const Candidate& a,const Candidate& b){return a.path<b.path;});
    const auto previous=gear_source.parts;
    gear_source.parts={};
    const auto set_hand=[](SourceGearPart& part,unsigned hand){
        if(part.hand==hand)return;
        part.hand=static_cast<std::uint8_t>(hand);
        part.serial=next_gear_serial();
        part.bone_sequence=0;
    };
    // Preserve the slot of a still-attached component when the other hand
    // gains or loses a part, even when its object path sorts before this one.
    for(unsigned i=0;i<count;++i){
        auto& candidate=candidates[i];
        const auto old=std::find_if(previous.begin(),previous.end(),[&](const SourceGearPart& entry){
            return api.weakget(&entry.component)==candidate.component;
        });
        if(old==previous.end())continue;
        auto& part=gear_source.parts[old-previous.begin()];
        part=*old;
        set_hand(part,candidate.hand);
        candidate.component=nullptr;
    }
    auto slot=gear_source.parts.begin();
    for(unsigned i=0;i<count;++i){
        const auto component=candidates[i].component;
        if(!component)continue;
        while(slot!=gear_source.parts.end()&&slot->serial)++slot;
        if(slot==gear_source.parts.end())break;
        const auto type=AnimU::field<void*>(component,16);
        slot->mesh_at=player_offset(type,api.isa(component,mesh_class)?"SkeletalMesh":"StaticMesh",8);
        slot->hidden=catalog_field(type,"bHiddenInGame",1);
        if(slot->mesh_at<0||!makeweak(slot->component,component)){*slot={};continue;}
        set_hand(*slot,candidates[i].hand);
    }
}

static void* gear_visual(void* weapon,unsigned side){
    if(!weapon)return nullptr;
    auto& source=gear_source;
    const auto type=AnimU::field<void*>(weapon,16);
    if(api.weakget(&source.visual_types[side])!=type){
        if(!makeweak(source.visual_types[side],type))return nullptr;
        source.visual_fields[side]=player_offset(type,"CurrentVisuals",8);
    }
    const auto offset=source.visual_fields[side];
    return offset>=0?AnimU::field<void*>(weapon,offset):nullptr;
}

static int gear_material_count(void* component){
    int count{};
    api.event(component,world_input(world_material_count),&count);
    return std::clamp(count,0,static_cast<int>(Player::material_slots));
}

static std::uint16_t source_material(void* component,int index){
    alignas(16) FactoryParameters args{};
    factory_put(args,0,index);
    api.event(component,world_input(world_get_material),args.data());
    auto material=AnimU::field<void*>(args.data(),8);
    const auto type=world_input(world_material_type);
    for(unsigned depth=0;material&&depth<5&&api.isa(material,type);++depth){
        const auto id=gear_catalog_id(material,Player::equipment_materials_by_path,
            [](unsigned i){return Player::equipment_materials[i];});
        if(id)return id;
        material=catalog_object_field(material,"Parent");
    }
    return 0;
}

static Player::Equipment capture_equipment(void* pawn,double time){
    Player::Equipment result;
    result.skin=static_cast<std::uint16_t>(local_player_skin.load(std::memory_order_relaxed));
    if(!prepare_gear(time))return result;
    auto& source=gear_source;
    const auto type=AnimU::field<void*>(pawn,16);
    if(api.weakget(&source.pawn_type)!=type){
        source={};
        if(!makeweak(source.pawn_type,type))return result;
        source.arms_at=player_offset(type,"Arms",8);
        source.right_at=player_offset(type,"CurrentInHandWeapon",8);
        source.left_at=player_offset(type,"Offhand_FlyingSword_WeaponScript",8);
        source.quick_at=player_offset(type,"QuickDashSwordVisuals",8);
        source.left_attach_at=player_offset(type,"WeaponAttachPoint_L",8);
    }
    const auto arms=source.arms_at>=0?AnimU::field<void*>(pawn,source.arms_at):nullptr;
    if(!arms||!api.isa(arms,mesh_class))return result;
    const auto weapon=source.right_at>=0?AnimU::field<void*>(pawn,source.right_at):nullptr;
    const auto offhand=source.left_at>=0?AnimU::field<void*>(pawn,source.left_at):nullptr;
    if(weapon){
        const auto weapon_type=AnimU::field<void*>(weapon,16);
        if(api.weakget(&source.weapon_class)!=weapon_type){
            source.weapon=gear_catalog_id(weapon_type,Player::weapons_by_path,[](unsigned i){return Player::weapons[i].path;});
            makeweak(source.weapon_class,weapon_type);
        }
        result.weapon=source.weapon;
    }
    const auto right=gear_visual(weapon,0),left=gear_visual(offhand,1);
    const auto quick=source.quick_at>=0?AnimU::field<void*>(pawn,source.quick_at):nullptr;
    const auto left_attach=source.left_attach_at>=0?AnimU::field<void*>(pawn,source.left_attach_at):nullptr;
    if(!right&&!left&&!quick){
        source.right=source.left=source.quick_dash={};
        source.parts={};
        return result;
    }
    const bool changed=api.weakget(&source.arms)!=arms||api.weakget(&source.right)!=right||
        api.weakget(&source.left)!=left||api.weakget(&source.quick_dash)!=quick;
    if(changed||time>=source.next_scan){
        makeweak(source.arms,arms);
        if(right)makeweak(source.right,right);else source.right={};
        if(left)makeweak(source.left,left);else source.left={};
        if(quick)makeweak(source.quick_dash,quick);else source.quick_dash={};
        gear_discover(arms,left_attach,{right,left,quick});
        source.next_scan=time+.5;
    }
    const bool materials_due=changed||time>=source.next_materials;
    if(materials_due)source.next_materials=time+.2;
    const auto world=gear_world_transform(arms);
    std::array<GearTransform,2> hands{};
    std::array<bool,2> have_hand{};
    for(unsigned i=0;i<source.parts.size();++i){
        auto& part=source.parts[i];
        const auto component=api.weakget(&part.component);
        if(!component)continue;
        const auto asset=AnimU::field<void*>(component,part.mesh_at);
        if(!asset)continue;
        const bool mesh_changed=api.weakget(&part.mesh)!=asset;
        if(mesh_changed){
            part.id=gear_catalog_id(asset,Player::equipment_meshes_by_path,
                [](unsigned id){return Player::equipment_meshes[id].path;});
            makeweak(part.mesh,asset);
            part.serial=next_gear_serial();
            part.bone_checked=part.bone_valid=false;
            part.bone_sequence=0;
        }
        if(!part.id)continue;
        if(materials_due||mesh_changed){
            part.materials={};
            const auto count=gear_material_count(component);
            for(int index=0;index<count;++index)part.materials[index]=source_material(component,index);
        }
        auto relative=gear_relative(component,world);
        const auto hand=part.hand-1;
        if(!have_hand[hand]){hands[hand]=gear_socket(arms,gear_hands[hand]);have_hand[hand]=true;}
        for(unsigned j=4;j<7;++j)relative[j]-=hands[hand][j];
        alignas(16) FactoryParameters args{};
        api.event(component,getters[5].fn,args.data());
        const bool visible=api.getbool(getters[5].ret,args.data(),0)&&(!part.hidden||!api.getbool(part.hidden,component,0));
        const Player::EquipmentPart captured{part.id,part.hand,static_cast<std::uint8_t>(visible),
            {relative[0],relative[1],relative[2],relative[3],relative[4],relative[5],relative[6],
             relative[8],relative[9],relative[10]},part.materials,part.serial};
        if(Player::valid_part(captured))result.parts[i]=captured;
    }
    return result;
}

struct GearAsset {Weak object;double retry{};};
static std::array<GearAsset,std::size(Player::equipment_meshes)> gear_meshes{};
static std::array<GearAsset,std::size(Player::equipment_materials)> gear_materials{};
static std::array<GearAsset,Player::skin_names.size()> skin_meshes{},skin_faces{};
static double gear_next_load;

struct DisplayGearPart {
    Weak component;
    std::uint16_t mesh{};
    std::uint8_t hand{};
    bool visible{},placed{};
    GearTransform transform{};
    std::array<std::uint16_t,Player::material_slots> materials{};
    std::uint32_t serial{};
    std::uint64_t bone_sequence{};
    bool bone_checked{},bone_valid{};
};
struct DisplayEquipment {
    Weak body;
    unsigned skin{1},weapon{};
    int face_slot{-1};
    std::array<DisplayGearPart,Player::equipment_slots> parts{};
};

static void reset_player_equipment(){
    gear_source={};
    gear_serial=0;
    gear_meshes={};
    gear_materials={};
    skin_meshes=skin_faces={};
    gear_ready=false;
    gear_next_prepare=gear_next_load=0;
}

static void* gear_load(GearAsset& cache,std::string_view path,double time,unsigned kind,void* base=nullptr){
    if(const auto asset=api.weakget(&cache.object))return asset;
    if(time<cache.retry||time<gear_next_load)return nullptr;
    gear_next_load=time+.025;
    const auto asset=load_player_asset(path,kind);
    if(!asset||(base&&!api.isa(asset,base))||!makeweak(cache.object,asset)){
        cache.retry=time+5;
        return nullptr;
    }
    return asset;
}

static void gear_set_material(void* component,int slot,void* material){
    alignas(16) FactoryParameters args{};
    factory_put(args,0,slot);
    factory_put(args,8,material);
    api.event(component,world_input(world_set_material),args.data());
}

static int skin_face_index(void* component){
    alignas(16) FactoryParameters args{};
    factory_put(args,0,gear_face);
    api.event(component,world_input(world_material_slot),args.data());
    return AnimU::field<int>(args.data(),8);
}

static void* apply_player_skin(DisplayEquipment& display,void* component,void* prince,unsigned skin,double time){
    if(skin==display.skin)return nullptr;
    static constexpr std::array<std::string_view,Player::skin_names.size()> faces{
        "","",
        "/Game/NPCs/Knight/Knight_head_02_MI.Knight_head_02_MI",
        "/Game/NPCs/Knight/Knight_head_04_MI.Knight_head_04_MI",
        "/Game/NPCs/Knight/Knight_head_06_MI.Knight_head_06_MI",
        "/Game/NPCs/Knight/Knight_head_05_MI.Knight_head_05_MI"
    };
    constexpr auto knight="/Game/QDCoopSkins/SK_Knight00.SK_Knight00";
    constexpr auto helmet="/Game/QDCoopSkins/SK_Knight01.SK_Knight01";
    auto mesh=prince;
    void* face{};
    if(skin>1){
        mesh=gear_load(skin_meshes[skin==5?5:2],skin==5?helmet:knight,time,catalog_skeletal_mesh);
        if(!mesh||catalog_object_field(mesh,"Skeleton")!=catalog_object_field(prince,"Skeleton"))return nullptr;
        face=gear_load(skin_faces[skin],faces[skin],time,UINT32_MAX,world_input(world_material_type));
        if(!face)return nullptr;
    }
    const auto previous=native_skeletal_asset(component);
    if(previous!=mesh&&!factory_apply_mesh(component,mesh))return nullptr;
    const auto face_slot=face?skin_face_index(component):-1;
    if(face&&(face_slot<0||face_slot>=gear_material_count(component))){
        if(previous&&previous!=mesh)factory_apply_mesh(component,previous);
        return nullptr;
    }
    if(display.face_slot>=0&&display.face_slot!=face_slot)
        gear_set_material(component,display.face_slot,nullptr);
    if(face)gear_set_material(component,face_slot,face);
    display.skin=skin;
    display.face_slot=face_slot;
    ++player_skin_changed;
    return previous!=mesh?mesh:nullptr;
}

static void show_gear(DisplayGearPart& part,void* component,bool visible){
    if(part.visible==visible)return;
    unsigned char args[]{static_cast<unsigned char>(visible),0};
    api.event(component,visible_fn,args);
    part.visible=visible;
}

static bool remove_gear(void* actor,DisplayGearPart& part){
    const auto component=api.weakget(&part.component);
    if(component){
        const auto destroy=world_input(world_destroy_component);
        if(!destroy)return false;
        show_gear(part,component,false);
        api.event(component,destroy,&actor);
        if(api.weakget(&part.component)==component)return false;
    }
    if(part.mesh)++player_parts_removed;
    part={};
    return true;
}

static void* create_gear(void* actor,void* body,DisplayGearPart& part,const Player::EquipmentPart& wanted,double time){
    const auto id=wanted.mesh;
    const auto& mesh=Player::equipment_meshes[id];
    const auto asset=gear_load(gear_meshes[id],mesh.path,time,mesh.skeletal?static_cast<unsigned>(catalog_skeletal_mesh):UINT32_MAX,
                               mesh.skeletal?nullptr:world_input(world_static_asset));
    if(!asset)return nullptr;
    const auto type=world_input(mesh.skeletal?world_pose_component:world_static_component);
    constexpr GearTransform identity{0,0,0,1,0,0,0,0,1,1,1,0};
    alignas(16) FactoryParameters args{};
    factory_put(args,gear_offsets.add_type,type);
    factory_put(args,gear_offsets.add_manual,std::uint8_t{1});
    factory_put(args,gear_offsets.add_transform,identity);
    api.event(actor,world_input(world_add_component),args.data());
    const auto component=AnimU::field<void*>(args.data(),gear_offsets.add_result);
    if(!component||!api.isa(component,type))return nullptr;
    if(!makeweak(part.component,component)){
        api.event(component,world_input(world_destroy_component),&actor);
        return nullptr;
    }
    part.visible=true;
    show_gear(part,component,false);
    unsigned char disabled{};
    api.event(component,world_input(world_component_collision),&disabled);
    api.event(component,world_input(world_component_overlap),&disabled);
    args={};
    factory_put(args,gear_offsets.attach_parent,body);
    api.event(component,world_input(world_attach_component),args.data());
    const bool attached=AnimU::field<std::uint8_t>(args.data(),gear_offsets.attach_result)!=0;
    bool applied=false;
    if(mesh.skeletal)applied=factory_apply_skinned_mesh(component,asset,type);
    else {
        args={};
        factory_put(args,gear_offsets.static_asset,asset);
        api.event(component,world_input(world_set_static),args.data());
        applied=AnimU::field<std::uint8_t>(args.data(),gear_offsets.static_result)!=0;
    }
    if(!attached||!applied){
        remove_gear(actor,part);
        return nullptr;
    }
    part.mesh=static_cast<std::uint16_t>(id);
    part.hand=wanted.hand;
    part.serial=wanted.serial;
    ++player_parts_created;
    return component;
}

static void apply_equipment(void* actor,void* body,DisplayEquipment& display,const Player::Equipment& equipment,double time){
    display.weapon=equipment.weapon;
    std::array<GearTransform,2> hands{};
    std::array<bool,2> have_hand{};
    for(unsigned i=0;i<display.parts.size();++i){
        auto& part=display.parts[i];
        const auto& wanted=equipment.parts[i];
        if((part.mesh!=wanted.mesh||part.serial!=wanted.serial)&&!remove_gear(actor,part))continue;
        if(!wanted.mesh)continue;
        auto component=api.weakget(&part.component);
        if(!component){
            part={};
            component=create_gear(actor,body,part,wanted,time);
            if(!component)continue;
        }
        const auto hand=wanted.hand-1;
        if(!have_hand[hand]){hands[hand]=gear_socket(body,gear_hands[hand]);have_hand[hand]=true;}
        const auto& t=wanted.transform;
        const GearTransform transform{t[0],t[1],t[2],t[3],
            t[4]+hands[hand][4],t[5]+hands[hand][5],t[6]+hands[hand][6],0,t[7],t[8],t[9],0};
        if(!part.placed||part.transform!=transform){
            alignas(16) FactoryParameters args{};
            factory_put(args,gear_offsets.new_transform,transform);
            factory_put(args,gear_offsets.teleport,std::uint8_t{1});
            api.event(component,world_input(world_relative_set),args.data());
            part.transform=transform;
            part.placed=true;
        }
        for(unsigned slot=0;slot<part.materials.size();++slot){
            const auto id=wanted.materials[slot];
            if(part.materials[slot]==id)continue;
            if(static_cast<int>(slot)>=gear_material_count(component))break;
            const auto material=id?gear_load(gear_materials[id],Player::equipment_materials[id],time,
                                             UINT32_MAX,world_input(world_material_type)):nullptr;
            if(id&&!material)continue;
            gear_set_material(component,static_cast<int>(slot),material);
            part.materials[slot]=id;
        }
        show_gear(part,component,wanted.visible!=0);
    }
}
