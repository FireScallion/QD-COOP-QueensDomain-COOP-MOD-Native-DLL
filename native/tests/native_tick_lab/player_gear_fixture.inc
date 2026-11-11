static constexpr std::array<unsigned,P::slots> gear_first_weapons{3,1,6,2};
static constexpr std::array<unsigned,P::slots> gear_next_weapons{2,6,1,3};
static constexpr std::array<unsigned,P::slots> gear_next_skins{1,5,2,3};
static constexpr const wchar_t* gear_markers[]{
    L"equipment-first.txt",L"equipment-swapped.txt",L"equipment-cleared.txt"
};
static std::array<std::array<U::WeakKey,P::equipment_slots>,P::slots> gear_old_parts{};
static std::array<std::array<unsigned,P::slots>,3> gear_mesh_evidence{};
static std::array<U::WeakKey,2> gear_test_items{};
static unsigned gear_test_stage,gear_test_check,gear_test_slot;
static unsigned gear_observed_weapon,gear_observed_mesh;
static bool gear_stage_sent,gear_verified;

struct GearTestArg {const void* data;unsigned size;};
static bool gear_test_call(Address object,Address method,std::initializer_list<GearTestArg> values){
    const auto fn=resolve(gear_lab_methods[method]);
    if(!object||!fn)return false;
    const auto bytes=at<std::uint16_t>(fn,182);
    if(bytes>256||(!bytes&&values.size()))return false;
    alignas(16) Parameters args{};
    auto input=values.begin();
    auto field=at<Address>(fn,80);
    for(unsigned i=0;field&&i<32;++i,field=at<Address>(field,32)){
        const auto flags=at<std::uint64_t>(field,64);
        if(!(flags&0x80)||(flags&0x400))continue;
        const auto size=at<unsigned>(field,60),offset=at<unsigned>(field,76);
        if(offset>bytes||size>bytes-offset)return false;
        if(input!=values.end()&&(input->size==size||(!input->size&&(size==1||size==4)))){
            if(input->data)std::memcpy(args.data()+offset,input->data,size);
            ++input;
        }else if(!(flags&0x100))return false;
    }
    if(input!=values.end())return false;
    event(object,fn,args.data());
    return true;
}

static bool grant_test_item(Address pawn,Address item){
    const auto convert=resolve(gear_lab_methods[gear_soft_ref]);
    const auto kismet=resolve(body_methods[body_kismet]);
    gear_test_check=10;
    if(!pawn||!item||!convert||!kismet)return false;
    alignas(16) Parameters soft{};
    const auto output=parameter_offset(convert,"ReturnValue",40);
    if(output<0||!put(soft,convert,"Object",item))return false;
    event(kismet,convert,soft.data());
    const std::int32_t one=1;
    const auto library=resolve(gear_lab_methods[gear_save_functions]);
    gear_test_check=11;
    return gear_test_call(library,gear_inventory,{{soft.data()+output,40},{&one,4},{&pawn,8}});
}

static bool equip_test_item(unsigned weapon){
    const auto pawn=resolve(held_pawn);
    if(!pawn)return false;
    const auto& entry=P::weapons[weapon];
    const auto item=load_test_asset(entry.item),type=load_test_asset(entry.path);
    const auto selection=object_field(pawn,"WeaponSelection");
    if(!item||!type||!selection||!grant_test_item(pawn,item))return false;
    gear_test_check=12;
    if(!gear_test_call(pawn,gear_equip,{{&item,8},{nullptr,0}}))return false;
    gear_test_check=13;
    if(!gear_test_call(pawn,gear_equip_weapon,{{&item,8}}))return false;
    gear_test_check=14;
    if(!gear_test_call(selection,gear_register,{{&type,8}}))return false;
    gear_test_check=15;
    if(!gear_test_call(selection,gear_select,{{&item,8}}))return false;
    remember(gear_test_items[gear_test_stage],item);
    return true;
}

static Address gear_test_material(Address component,int slot){
    const auto fn=resolve(gear_lab_methods[gear_get_material]);
    if(!fn||at<std::uint16_t>(fn,182)!=16)return 0;
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&slot,4);
    event(component,fn,args.data());
    return U::field<Address>(args.data(),8);
}

static bool verify_gear_skin(Address body,unsigned skin){
    const auto mesh=object_field(body,"SkeletalMesh");
    const auto instance=test_anim_instance(body);
    constexpr const char* prince="/Game/Player/Assets/FPModel/Player2026/Player_FullChar_2026.Player_FullChar_2026";
    constexpr const char* knight="/Game/QDCoopSkins/SK_Knight00.SK_Knight00";
    constexpr const char* helmet="/Game/QDCoopSkins/SK_Knight01.SK_Knight01";
    if(!mesh||!instance||!exact_path(at<Address>(instance,16),test_graph_path)||
       !exact_path(mesh,skin==1?prince:skin==5?helmet:knight))return false;
    if(skin==1)return true;
    constexpr const char* faces[]{
        "","",
        "/Game/NPCs/Knight/Knight_head_02_MI.Knight_head_02_MI",
        "/Game/NPCs/Knight/Knight_head_04_MI.Knight_head_04_MI",
        "/Game/NPCs/Knight/Knight_head_06_MI.Knight_head_06_MI",
        "/Game/NPCs/Knight/Knight_head_05_MI.Knight_head_05_MI"
    };
    for(int slot=0;slot<8;++slot){
        const auto material=gear_test_material(body,slot);
        if(material&&exact_path(material,faces[skin]))return true;
    }
    return false;
}

static bool expected_gear_mesh(unsigned weapon,unsigned mesh){
    const auto path=P::equipment_meshes[mesh].path;
    switch(weapon){
    case 1:return path.ends_with("/Axe_00_SM.Axe_00_SM");
    case 2:return path.ends_with("/B33Sword_SK.B33Sword_SK");
    case 3:return path.ends_with("/BasicSpear_00_SM.BasicSpear_00_SM");
    case 6:return path.ends_with("/BirdFlail.BirdFlail");
    default:return false;
    }
}

static unsigned initial_gear_weapon(unsigned slot){
    return player_bone_lab&&slot==0?6:gear_first_weapons[slot];
}

#include "player_bone_fixture.inc"

static bool inspect_gear_stage(unsigned stage){
    for(unsigned slot=0;slot<P::slots;++slot){
        if(slot==role_index)continue;
        gear_test_slot=slot;
        gear_test_check=20;
        const auto skin=stage==0?slot+2:gear_next_skins[slot];
        const auto weapon=stage==0?initial_gear_weapon(slot):stage==1?gear_next_weapons[slot]:0;
        P::EquipmentBinding view{};
        if(!core.inspect_equipment(slot,&view))return false;
        gear_observed_weapon=view.weapon;
        gear_observed_mesh=view.parts[0].mesh;
        const bool unarmed=!view.weapon||P::weapons[view.weapon].path.ends_with(".Null_WeaponScript_C");
        if(view.skin!=skin||(weapon?view.weapon!=weapon:!unarmed))return false;
        const auto actor=resolve(avatar_keys[slot]);
        const auto body=actor?object_field(actor,"SkeletalMeshComponent"):0;
        gear_test_check=21;
        if(!body||!verify_gear_skin(body,skin))return false;
        if(stage>0){
            gear_test_check=22;
            for(const auto& key:gear_old_parts[slot])if(resolve(key))return false;
        }
        bool found=false;
        std::array<U::WeakKey,P::equipment_slots> keys{};
        for(unsigned i=0;i<view.parts.size();++i){
            const auto& part=view.parts[i];
            if(!part.mesh)continue;
            gear_test_check=23;
            if(!weapon||part.mesh>=std::size(P::equipment_meshes))return false;
            const U::WeakKey key{part.object_index,part.object_serial};
            const auto component=resolve(key);
            if(!component)return false;
            const auto& definition=P::equipment_meshes[part.mesh];
            const auto asset=object_field(component,definition.skeletal?"SkeletalMesh":"StaticMesh");
            if(!asset||!exact_path(asset,definition.path.data()))return false;
            const auto parent=resolve(gear_lab_methods[gear_get_parent]);
            const auto collision=resolve(gear_lab_methods[gear_get_collision]);
            Address attached{};
            event(component,parent,&attached);
            if(attached!=body||query_byte(component,collision))return false;
            keys[i]=key;
            if(expected_gear_mesh(weapon,part.mesh)&&part.visible){
                const auto material=gear_test_material(component,0);
                if(!material)return false;
                found=true;
                gear_mesh_evidence[stage][slot]=part.mesh;
            }
        }
        if(weapon&&!found){gear_test_check=24;return false;}
        if(!weapon)gear_mesh_evidence[stage][slot]=0;
    }
    if(stage<2)for(unsigned slot=0;slot<P::slots;++slot){
        if(slot==role_index)continue;
        P::EquipmentBinding view{};
        if(!core.inspect_equipment(slot,&view))return false;
        for(unsigned i=0;i<view.parts.size();++i)
            gear_old_parts[slot][i]={view.parts[i].object_index,view.parts[i].object_serial};
    }
    return true;
}

static bool run_gear_stage(){
    if(gear_verified)return all_player_markers(gear_markers[2]);
    if(gear_test_stage&&!all_player_markers(gear_markers[gear_test_stage-1]))return false;
    if(!gear_stage_sent){
        if(gear_test_stage==2){
            const auto pawn=resolve(held_pawn);
            for(unsigned i=gear_test_items.size();i-->0;){
                const auto item=resolve(gear_test_items[i]);
                if(!item||!gear_test_call(pawn,gear_unequip,{{&item,8},{nullptr,0}})){phase=95;return false;}
            }
            if(!gear_test_call(pawn,gear_refresh,{})){phase=95;return false;}
        }else{
            const auto skin=gear_test_stage==0?role_index+2:gear_next_skins[role_index];
            const auto weapon=gear_test_stage==0?initial_gear_weapon(role_index):gear_next_weapons[role_index];
            if(!core.set_skin(skin)||!equip_test_item(weapon)){phase=96;return false;}
        }
        gear_stage_sent=true;
    }
    if(!inspect_gear_stage(gear_test_stage))return false;
    if(player_bone_lab&&gear_test_stage==0&&!run_weapon_bone_fixture())return false;
    write_file(marker(role_index,gear_markers[gear_test_stage]).c_str(),"ok",2);
    ++gear_test_stage;
    gear_stage_sent=false;
    if(gear_test_stage==std::size(gear_markers))gear_verified=true;
    return gear_verified&&all_player_markers(gear_markers[2]);
}
