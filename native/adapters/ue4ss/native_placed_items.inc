namespace Placed=qdcoop::placed_item;
#include "native_placed_item_template.inc"

struct PlacedItemVisual {
    Placed::Drop drop;
    std::string save_key;
    Weak actor;
    double retry{};
    bool wanted{true},ready{},pickable{};
};
using PlacedMapClear=void(*)(const void*,const void*);
struct PlacedItemBinding {
    Weak type;
    void *transient{},*drop{},*activation{};
    MedicMap grants,guids,actors;
    PlacedMapClear clear_map{};
    WorldArrayClear clear_array{};
};
static PlacedItemBinding placed_item_binding;
static std::vector<PlacedItemVisual> placed_item_visuals;
static std::vector<Placed::Transfer> placed_item_transfers;
static std::string placed_item_room;
static std::string placed_legacy_identity;
static bool placed_item_room_ambiguous;
static Weak placed_source_request;
static Weak placed_source_inspect;
static std::uint64_t placed_source_sequence,placed_source_token,placed_source_completed;
static int placed_source_result;
static bool placed_escrow_busy;
enum class PlacedOperation { none,local_pickup,reserve,credit,settle,reopen,decline,place,register_drop,settle_escrow };
static PlacedOperation placed_operation;
static bool placed_claim_queue(std::string_view id,double time);
static std::string placed_claim_identity(qdcoop::Id identity);
static void placed_claims_step(void* pawn,double time);
static void placed_claim_finished(double time);
static void placed_claims_reset();
static void placed_source_step(void* pawn,double time);
static void placed_source_network(void* pawn,double time);
static void placed_source_reset();
static double placed_item_next_step,placed_item_next_bind;
static constexpr std::string_view placed_item_type_path="/Game/QDCoop/QDCoopPlacedItem.QDCoopPlacedItem_C";

static PlacedMapClear placed_item_map_clear(){
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto method=world_input(world_placed_map_clear);
    const auto entry=image+0x2756140;
    constexpr unsigned char jump[]{0xe9,0xf8,0x67,0x9d,0xff};
    constexpr unsigned char prefix[]{0x48,0x85,0xc9,0x0f,0x84,0xad,0x00,0x00,0x00,0x48,0x83,0xec,0x78};
    if(!method||*api.parms(method)!=80||AnimU::field<std::uintptr_t>(method,216)!=image+0x2d7f8c0||
       std::memcmp(reinterpret_cast<const void*>(image+0x2d7f943),jump,sizeof(jump))||
       std::memcmp(reinterpret_cast<const void*>(entry),prefix,sizeof(prefix)))return nullptr;
    return reinterpret_cast<PlacedMapClear>(entry);
}

static void* placed_item_bool(void* type,std::string_view name){
    const auto field=catalog_field(type,name,1);
    return medic_field_class(field,"BoolProperty")?field:nullptr;
}

static bool placed_item_bind(double time){
    if(api.weakget(&placed_item_binding.type))return true;
    if(time<placed_item_next_bind)return false;
    placed_item_next_bind=time+.7;
    placed_items_stage=1;
    if(!world_input(world_placed_map_clear)||!world_input(world_actor_clear)||!world_input(world_placed_item_type)||
       !world_input(world_placed_sphere)||!world_input(world_barrier_relative)||!world_input(world_relative_set)){
        scan_world_inputs(2048);
        return false;
    }
    if(!prepare_gear(time))return false;
    placed_items_stage=2;
    const auto type=load_world_asset(placed_item_type_path,UINT32_MAX,true,actor_class);
    if(!type)return false;
    placed_items_stage=3;
    PlacedItemBinding candidate;
    candidate.transient=placed_item_bool(type,"TransientPickup");
    candidate.drop=placed_item_bool(type,"IsADrop");
    candidate.activation=placed_item_bool(type,"RequireActivation");
    candidate.clear_array=world_actor_array_clear();
    candidate.clear_map=placed_item_map_clear();
    candidate.guids=world_actor_array(type,"GUIDs",16,"/Script/CoreUObject.Guid");
    const auto grants=catalog_field(type,"ItemsToGrant",80),actors=catalog_field(type,"ActorsToActivateOnPickup",16);
    if(!candidate.transient||!candidate.drop||!candidate.activation||!candidate.clear_array||!candidate.clear_map||
       !candidate.guids.field||!medic_field_class(grants,"MapProperty")||!medic_field_class(actors,"ArrayProperty")||
       !medic_params(world_barrier_relative,48,{{"ReturnValue",48,0}})||
       !medic_params(world_relative_set,189,{{"NewTransform",48,0},{"bSweep",1,48},
                                            {"SweepHitResult",136,52},{"bTeleport",1,188}}))return false;
    const auto key=AnimU::field<void*>(grants,0x78),value=AnimU::field<void*>(grants,0x80);
    const auto element=AnimU::field<void*>(actors,0x78);
    placed_items_stage=4;
    if(!medic_field_class(key,"SoftObjectProperty")||!medic_field_class(value,"IntProperty")||
       *api.size(key)!=40||*api.offset(key)!=0||*api.size(value)!=4||*api.offset(value)!=40||
       AnimU::field<unsigned>(grants,0xa0)!=0||
       !medic_field_class(element,"InterfaceProperty")||*api.size(element)!=16||
       !makeweak(candidate.type,type))return false;
    candidate.grants={grants,*api.offset(grants)};
    candidate.actors={actors,*api.offset(actors)};
    placed_item_binding=candidate;
    placed_items_stage=5;
    return true;
}

static bool placed_item_prepare_actor(void* actor){
    const auto& binding=placed_item_binding;
    if(AnimU::field<void*>(actor,16)!=api.weakget(&binding.type))return false;
    api.setbool(binding.transient,actor,true,0);
    api.setbool(binding.drop,actor,false,0);
    api.setbool(binding.activation,actor,false,0);
    binding.clear_map(binding.grants.address(actor),binding.grants.field);
    binding.clear_array(binding.guids.address(actor),binding.guids.field);
    binding.clear_array(binding.actors.address(actor),binding.actors.field);
    return !AnimU::field<WorldArray>(binding.grants.address(actor),0).count&&
           !AnimU::field<WorldArray>(binding.guids.address(actor),0).count&&
           !AnimU::field<WorldArray>(binding.actors.address(actor),0).count;
}

static void* placed_item_data(std::string_view path){
    if(!Placed::path(path,"/Game/Items/",250)||Placed::prefix(path,"/Game/Items/KeyItems/")||
       Placed::prefix(path,"/Game/Items/Upgrades/"))return nullptr;
    const auto type=world_input(world_placed_item_type);
    const auto item=type?load_world_asset(path,UINT32_MAX,false,nullptr):nullptr;
    if(!item||!api.isa(item,type))return nullptr;
    const auto key=placed_item_bool(type,"ShowInKeyMenu"),consumable=placed_item_bool(type,"Consumable");
    const auto equipment=placed_item_bool(type,"Equippable");
    return key&&consumable&&equipment&&!api.getbool(key,item,0)&&
        (api.getbool(consumable,item,0)||api.getbool(equipment,item,0))?item:nullptr;
}

#include "native_placed_item_save.inc"
#include "native_placed_item_interact.inc"
#include "native_placed_item_pickup.inc"
#include "native_placed_item_claims.inc"

static void* placed_item_skeletal(void* actor,void* graphics,void* mesh){
    const auto type=world_input(world_pose_component);
    constexpr GearTransform identity{0,0,0,1,0,0,0,0,1,1,1,0};
    alignas(16) FactoryParameters args{};
    factory_put(args,gear_offsets.add_type,type);
    factory_put(args,gear_offsets.add_manual,std::uint8_t{1});
    factory_put(args,gear_offsets.add_transform,identity);
    api.event(actor,world_input(world_add_component),args.data());
    const auto component=AnimU::field<void*>(args.data(),gear_offsets.add_result);
    if(!component||!api.isa(component,type)||!factory_apply_skinned_mesh(component,mesh,type))return nullptr;
    args={};
    factory_put(args,gear_offsets.attach_parent,graphics);
    api.event(component,world_input(world_attach_component),args.data());
    return args[gear_offsets.attach_result]?component:nullptr;
}

static bool placed_item_geometry(void* actor,void* graphics,const PlacedItemModel& model){
    alignas(16) FactoryParameters args{};
    auto component=graphics;
    if(model.skeletal){
        component=placed_item_skeletal(actor,graphics,model.mesh);
        if(!component)return false;
    }else{
        factory_put(args,gear_offsets.static_asset,model.mesh);
        api.event(graphics,world_input(world_set_static),args.data());
        if(placed_object_field(graphics,"StaticMesh")!=model.mesh)return false;
    }
    const auto get=world_input(world_barrier_relative);
    alignas(16) FactoryParameters original{},target{};
    api.event(model.component,get,original.data());
    api.event(component,get,target.data());
    const auto source=AnimU::field<GearTransform>(original.data(),0);
    auto transform=AnimU::field<GearTransform>(target.data(),0);
    std::copy_n(source.begin(),4,transform.begin());
    std::copy_n(source.begin()+8,3,transform.begin()+8);
    args={};
    factory_put(args,0,transform);
    args[188]=1;
    api.event(component,world_input(world_relative_set),args.data());
    int materials{};
    api.event(model.component,world_input(world_material_count),&materials);
    if(materials<0||materials>64)return false;
    for(int i=0;i<materials;++i){
        args={};
        factory_put(args,0,i);
        api.event(model.component,world_input(world_get_material),args.data());
        const auto material=AnimU::field<void*>(args.data(),8);
        if(material)gear_set_material(component,i,material);
    }
    unsigned char disabled{};
    api.event(component,world_input(world_component_collision),&disabled);
    api.event(component,world_input(world_component_overlap),&disabled);
    return true;
}

static bool placed_item_create(void* pawn,PlacedItemVisual& visual){
    placed_items_stage=6;
    const auto item=placed_item_data(visual.drop.item);
    if(!item)return false;
    const auto model=placed_item_model(item);
    if(!model.component)return false;
    placed_items_stage=7;
    std::array<float,3> position{};
    std::ranges::transform(visual.drop.position,position.begin(),[](std::int32_t value){return static_cast<float>(value);});
    const auto actor=factory_spawn(pawn,api.weakget(&placed_item_binding.type),position,visual.actor,true,
                                   &placed_item_prepare_actor);
    const auto graphics=placed_object_field(actor,"PickupGraphics"),interact=placed_object_field(actor,"InteractCollision");
    if(!actor||!graphics||!interact||!api.isa(graphics,world_input(world_static_component))||
       !api.isa(interact,world_input(world_placed_sphere))||!placed_item_prepare_actor(actor))return false;
    placed_items_stage=8;
    if(!placed_item_geometry(actor,graphics,model))return false;
    // Custody is handled separately from this read-only presentation. Keep the
    // Blueprint's pickup collision disabled so it cannot grant or remove stock data.
    unsigned char disabled{};
    api.event(interact,world_input(world_component_collision),&disabled);
    api.event(actor,factory_object(factory_collision),&disabled);
    own_set(actor,hide_fn,false);
    placed_items_stage=9;
    ++placed_items_spawned;
    return true;
}

static void native_placed_items_snapshot(const std::vector<WP::NamedState>& names){
    for(auto& visual:placed_item_visuals)visual.wanted=false;
    placed_item_room.clear();
    placed_item_room_ambiguous=false;
    for(const auto& state:names){
        if(state.value!=1||!Placed::prefix(state.key,Placed::room_prefix))continue;
        const auto id=std::string_view(state.key).substr(Placed::room_prefix.size());
        if(!Placed::valid_id(id)||!placed_item_room.empty())placed_item_room_ambiguous=true;
        else placed_item_room=id;
    }
    placed_item_transfers=Placed::transfers(names);
    if(placed_item_transfers.empty())return;
    std::string map;
    if(!catalog_path(api.weakget(&attached_world),map)){
        placed_item_transfers.clear();
        return;
    }
    for(const auto& transfer:placed_item_transfers){
        if(transfer.state!=Placed::Custody::ground||!WP::name_equal(map,transfer.drop.map))continue;
        const auto& drop=transfer.drop;
        const auto found=std::ranges::find(placed_item_visuals,drop,&PlacedItemVisual::drop);
        if(found!=placed_item_visuals.end())found->wanted=true;
        else if(placed_item_visuals.size()<2*Placed::max_ground_items)
            placed_item_visuals.push_back({drop,transfer.save_key,{}});
    }
}

static void native_placed_items_step(void* pawn,double time){
    placed_pickup_step(pawn,time);
    placed_source_network(pawn,time);
    placed_source_step(pawn,time);
    if(placed_item_visuals.empty()||time<placed_item_next_step)return;
    placed_item_next_step=time+.10;
    std::erase_if(placed_item_visuals,[](auto& visual){
        if(visual.wanted||!destroy_owned_actor(visual.actor,false))return false;
        if(visual.ready)++placed_items_removed;
        return true;
    });
    if(!active_peer_mask||!world_progress_ready()||!factory_configured)return;
    for(auto& visual:placed_item_visuals){
        if(!visual.wanted||time<visual.retry)continue;
        if(const auto actor=api.weakget(&visual.actor)){
            if(visual.ready){
                placed_pickup_enable(pawn,actor,visual,time);
                continue;
            }
            if(!destroy_owned_actor(visual.actor,false))continue;
        }
        if(!placed_item_bind(time))return;
        visual.retry=time+1.;
        visual.ready=placed_item_create(pawn,visual);
        if(!visual.ready)destroy_owned_actor(visual.actor,false);
        break;
    }
}

static bool native_placed_items_cleanup(){
    bool clean=placed_save_cleanup();
    if(!native_placed_item_unbind_interact())clean=false;
    placed_pickup_reset();
    placed_claims_reset();
    placed_source_request={};
    placed_source_inspect={};
    placed_source_token=placed_source_completed=0;
    placed_source_result=0;
    placed_source_reset();
    for(auto& visual:placed_item_visuals)
        if(!destroy_owned_actor(visual.actor,false))clean=false;
    return clean;
}

static void native_placed_items_reset(){
    placed_item_visuals.clear();
    placed_item_transfers.clear();
    placed_item_room.clear();
    placed_item_room_ambiguous=false;
    placed_operation=PlacedOperation::none;
    placed_item_binding={};
    placed_item_next_step=placed_item_next_bind=0;
    placed_items_stage=0;
}

#include "native_placed_item_source.inc"
