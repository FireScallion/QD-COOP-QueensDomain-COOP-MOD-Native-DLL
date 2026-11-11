static constexpr unsigned bird_mesh=5,bird_bones=12;
static_assert(P::equipment_meshes[bird_mesh].bone_count==bird_bones);
using ObservedBones=std::array<P::BoneTransform,bird_bones>;
struct BoneObservations {
    std::uint32_t count{};
    std::array<ObservedBones,8> poses{};
};
static BoneObservations bone_observations;
static ObservedBones first_bones{};
static std::array<U::NameKey,bird_bones> bone_test_names{};
static U::WeakKey bone_host_weapon,bone_host_anim,bone_host_offhand,bone_right_key,bone_left_key;
static Address bone_charge_field;
static std::uint64_t bone_first_sequence,bone_second_sequence;
static unsigned bone_test_stage,bone_test_check,bone_records;
static ULONGLONG bone_stage_since;
static bool bone_host_shown,bone_host_hidden,bone_test_marked,bone_verified;
static float bone_first_error,bone_second_error,bone_motion;
static constexpr const wchar_t* bone_frames[]{L"bone-observed-first.bin",L"bone-observed-second.bin"};
static constexpr const wchar_t* bone_markers[]{L"bone-first.txt",L"bone-second.txt",L"bone-offhand-hidden.txt"};

static U::NameKey bone_test_name(std::string_view name){
    const auto function=resolve(bone_lab_methods[bone_name]),cdo=resolve(bone_lab_methods[bone_strings]);
    if(!function||!cdo||at<std::uint16_t>(function,182)!=24||parameter_offset(function,"ReturnValue",8)!=16)return {};
    const std::wstring wide(name.begin(),name.end());
    const auto size=static_cast<int>(wide.size()+1);
    struct StringArg {const wchar_t* data;int count,capacity;};
    const StringArg string{wide.c_str(),size,size};
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&string,sizeof(string));
    event(cdo,function,args.data());
    return U::field<U::NameKey>(args.data(),16);
}

static bool observe_weapon_bones(Address component,ObservedBones& output){
    const auto fn=resolve(bone_lab_methods[bone_socket]);
    if(!component||!fn)return false;
    const auto ret=parameter_offset(fn,"ReturnValue",48);
    if(ret<0)return false;
    for(unsigned i=0;i<output.size();++i){
        auto& key=bone_test_names[i];
        if(!key.index){
            key=bone_test_name(P::equipment_bones[P::equipment_meshes[bird_mesh].first_bone+i]);
            if(!key.index)return false;
        }
        alignas(16) Parameters args{};
        if(!put(args,fn,"InSocketName",key)||!put(args,fn,"TransformSpace",std::uint8_t{2}))return false;
        event(component,fn,args.data());
        const auto transform=U::field<std::array<float,12>>(args.data(),ret);
        output[i]={transform[0],transform[1],transform[2],transform[3],
            transform[4],transform[5],transform[6],transform[8],transform[9],transform[10]};
        if(!P::valid_gear_transform(output[i]))return false;
    }
    return true;
}

static float pose_error(const ObservedBones& expected,const ObservedBones& actual){
    float error=0;
    for(unsigned i=0;i<expected.size();++i){
        float dot=0;
        for(unsigned axis=0;axis<4;++axis)dot+=expected[i][axis]*actual[i][axis];
        error=std::max(error,100.f*std::max(0.f,1.f-std::abs(dot)));
        for(unsigned axis=4;axis<10;++axis)
            error=std::max(error,std::abs(expected[i][axis]-actual[i][axis])*(axis>=7?20.f:1.f));
    }
    return error;
}

static Address local_bird_component(){
    const auto pawn=resolve(held_pawn);
    const auto weapon=pawn?object_field(pawn,"CurrentInHandWeapon"):0;
    const auto visual=weapon?object_field(weapon,"CurrentVisuals"):0;
    const auto property=visual?field_of(at<Address>(visual,16),"VisualComps",16):0;
    if(!property)return 0;
    struct Components {const Address* data;int count,capacity;};
    const auto array=at<Components>(visual,at<int>(property,76));
    if(!array.data||array.count<1||array.count>64||array.capacity<array.count)return 0;
    for(int i=0;i<array.count;++i){
        const auto component=array.data[i];
        if(!component)continue;
        const auto mesh=object_field(component,"SkeletalMesh");
        if(mesh&&exact_path(mesh,P::equipment_meshes[bird_mesh].path.data()))return component;
    }
    return 0;
}

static void hold_test_bone_charge(){
    const auto anim=resolve(bone_host_anim);
    if(!anim||!bone_charge_field||bone_test_stage>1)return;
    const float charge=bone_test_stage==0?0.f:.1f;
    std::memcpy(reinterpret_cast<void*>(anim+at<int>(bone_charge_field,76)),&charge,sizeof(charge));
}

static bool start_offhand_fixture(){
    const auto pawn=resolve(held_pawn),component=local_bird_component();
    const auto anim=test_anim_instance(component);
    const auto show=resolve(bone_lab_methods[bone_show]);
    bone_test_check=1;
    if(!pawn||!component||!anim||!show||
       !exact_path(at<Address>(anim,16),"/Game/Items/Weapons/BirdFlail/BirdFlail_AnimBP.BirdFlail_AnimBP_C"))return false;
    bone_charge_field=field_of(at<Address>(anim,16),"Charge",4);
    if(!bone_charge_field)return false;
    remember(bone_host_weapon,component);
    remember(bone_host_anim,anim);
    bone_test_check=2;
    const auto item=load_test_asset("/Game/Items/Weapons/FlyingSword/FlyingSword.FlyingSword");
    alignas(16) Parameters args{};
    if(!grant_test_item(pawn,item)||!put(args,show,"ForceRightHand",std::uint8_t{0})){
        phase=97;
        return false;
    }
    event(pawn,show,args.data());
    const auto visual=object_field(pawn,"QuickDashSwordVisuals");
    if(!visual){phase=98;return false;}
    remember(bone_host_offhand,visual);
    bone_host_shown=true;
    bone_stage_since=GetTickCount64();
    return true;
}

static bool read_bone_observations(unsigned stage,BoneObservations& observations){
    const auto path=marker(0,bone_frames[stage]);
    const auto file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                                nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD bytes{};
    const bool ok=ReadFile(file,&observations,sizeof(observations),&bytes,nullptr)&&bytes==sizeof(observations);
    CloseHandle(file);
    return ok&&observations.count&&observations.count<=observations.poses.size();
}

static bool guest_weapon_bones(){
    P::EquipmentBinding view{};
    if(!core.inspect_equipment(0,&view)||view.weapon!=6)return false;
    bone_test_check=5;
    Address right{};
    U::WeakKey right_key{},left_key{};
    std::uint64_t sequence{};
    for(unsigned i=0;i<view.parts.size();++i){
        const auto& part=view.parts[i];
        if(!part.mesh||!part.visible)continue;
        P::GearPoseBinding pose{};
        if(!core.inspect_gear_pose(0,i,&pose)||pose.mesh!=part.mesh||!pose.visible)return false;
        const U::WeakKey key{part.object_index,part.object_serial};
        if(pose.hand==1&&part.mesh==bird_mesh){
            right=resolve(key);
            right_key=key;
            sequence=pose.sequence;
        }
        if(pose.hand==2&&P::equipment_meshes[part.mesh].path.ends_with("/WingedSword.WingedSword"))
            left_key=key;
    }
    if(!right||!sequence)return false;
    const auto same=[](U::WeakKey a,U::WeakKey b){return a.index==b.index&&a.serial==b.serial;};
    if(bone_test_stage==2)
        return same(right_key,bone_right_key)&&!left_key.serial&&!resolve(bone_left_key);
    if(!resolve(left_key))return false;
    const auto body=object_field(resolve(avatar_keys[0]),"SkeletalMeshComponent");
    const auto parent=resolve(gear_lab_methods[gear_get_parent]);
    Address attached{};
    event(resolve(left_key),parent,&attached);
    if(attached!=body||query_byte(resolve(left_key),resolve(gear_lab_methods[gear_get_collision])))return false;
    bone_test_check=6;
    BoneObservations source{};
    ObservedBones actual{};
    if(!read_bone_observations(bone_test_stage,source)||!observe_weapon_bones(right,actual))return false;
    float error=100000.f;
    for(unsigned i=0;i<source.count;++i)error=std::min(error,pose_error(source.poses[i],actual));
    if(bone_test_stage==0)bone_first_error=error;
    else bone_second_error=error;
    if(error>2.f)return false;
    if(bone_test_stage==0){
        bone_right_key=right_key;
        bone_left_key=left_key;
        bone_first_sequence=sequence;
        first_bones=actual;
    }else{
        bone_test_check=7;
        bone_motion=pose_error(first_bones,actual);
        if(sequence<=bone_first_sequence||bone_motion<1.f||!same(right_key,bone_right_key)||
           !same(left_key,bone_left_key))return false;
        bone_second_sequence=sequence;
    }
    return true;
}

static bool host_weapon_bones(){
    if(!bone_host_shown&&!start_offhand_fixture())return false;
    if(bone_test_stage==2){
        if(!bone_host_hidden){
            const auto hide=resolve(bone_lab_methods[bone_hide]),pawn=resolve(held_pawn);
            if(!hide||!pawn||at<std::uint16_t>(hide,182)!=0)return false;
            event(pawn,hide,nullptr);
            bone_host_hidden=true;
        }
        return !resolve(bone_host_offhand)&&resolve(bone_host_weapon)!=0;
    }
    bone_test_check=3;
    if(GetTickCount64()-bone_stage_since<500)return false;
    ObservedBones observation{};
    if(!observe_weapon_bones(resolve(bone_host_weapon),observation))return false;
    if(bone_test_stage==1){
        bone_motion=pose_error(first_bones,observation);
        if(bone_motion<1.f)return false;
    }
    if(!bone_records&&bone_test_stage==0)first_bones=observation;
    bone_observations.poses[bone_records++%bone_observations.poses.size()]=observation;
    bone_observations.count=std::min(bone_records,static_cast<unsigned>(bone_observations.poses.size()));
    const auto path=marker(0,bone_frames[bone_test_stage]);
    write_file(path.c_str(),reinterpret_cast<const char*>(&bone_observations),sizeof(bone_observations));
    return bone_observations.count>=3;
}

static bool run_weapon_bone_fixture(){
    if(bone_verified)return true;
    // Keep publishing the host observations while peers wait for their sample.
    const bool ready=host?host_weapon_bones():bone_test_marked||guest_weapon_bones();
    if(ready&&!bone_test_marked){
        write_file(marker(role_index,bone_markers[bone_test_stage]).c_str(),"ok",2);
        bone_test_marked=true;
    }
    if(!bone_test_marked||!all_player_markers(bone_markers[bone_test_stage]))return false;
    ++bone_test_stage;
    bone_test_marked=false;
    bone_records=0;
    bone_observations={};
    bone_stage_since=GetTickCount64();
    bone_verified=bone_test_stage==std::size(bone_markers);
    return bone_verified;
}
