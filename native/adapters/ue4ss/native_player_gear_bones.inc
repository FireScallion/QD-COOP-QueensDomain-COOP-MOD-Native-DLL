// Pinned against the Shipping CopyPoseFromSkeletalComponent implementation.
static constexpr unsigned gear_source_bones_at=0x6d8,gear_parallel_at=0xb60;
static constexpr unsigned gear_display_bones_at=0x698,gear_display_dirty_at=0x7f8;
struct GearBoneArray {GearTransform* data{};std::int32_t count{},capacity{};};
static_assert(sizeof(GearBoneArray)==16&&sizeof(GearTransform)==48);
using CopyGearBones=GearBoneArray*(*)(void*,GearBoneArray*);
static CopyGearBones copy_gear_bones;
static double gear_bone_prepare_at;
static std::array<AnimU::NameKey,std::size(Player::equipment_bones)> gear_bone_names{};

static bool prepare_gear_bones(double time){
    if(copy_gear_bones)return true;
    if(time<gear_bone_prepare_at)return false;
    gear_bone_prepare_at=time+.25;
    const auto copy=world_input(world_copy_pose),index=world_input(world_get_bone_index);
    const auto count=world_input(world_get_bone_count),pose_type=world_input(world_pose_component);
    gear_pose_stage=1;
    if(!copy||!index||!count||!pose_type){scan_world_inputs(2048);return false;}
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    constexpr unsigned char copy_entry[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20};
    constexpr unsigned char source_reads[]{0x48,0x63,0xb7,0xe0,0x06,0,0,0x48,0x8b,0xbf,0xd8,0x06,0,0};
    constexpr unsigned char parallel_check[]{0x48,0x83,0xb9,0x60,0x0b,0,0,0};
    constexpr unsigned char destination[]{0x48,0x8d,0x87,0x98,0x06,0,0};
    constexpr unsigned char dirty[]{0xc6,0x87,0xf8,0x07,0,0,0x01};
    gear_pose_stage=2;
    if(*api.parms(copy)!=8||AnimU::field<std::uintptr_t>(copy,216)!=image+0x2e872b0||
       *api.propertiesSize(mesh_class)<static_cast<int>(gear_parallel_at+8)||
       *api.propertiesSize(pose_type)<=static_cast<int>(gear_display_dirty_at)||
       std::memcmp(reinterpret_cast<const void*>(image+0x27eed20),copy_entry,sizeof(copy_entry))||
       std::memcmp(reinterpret_cast<const void*>(image+0x27eed2f),parallel_check,sizeof(parallel_check))||
       std::memcmp(reinterpret_cast<const void*>(image+0x27eed6c),source_reads,sizeof(source_reads))||
       std::memcmp(reinterpret_cast<const void*>(image+0x27bd306),destination,sizeof(destination))||
       std::memcmp(reinterpret_cast<const void*>(image+0x27bd3f9),dirty,sizeof(dirty)))return false;
    gear_pose_stage=3;
    if(*api.parms(index)!=12||factory_parameter(index,"BoneName",8)!=0||
       factory_parameter(index,"ReturnValue",4)!=8||*api.parms(count)!=4||
       factory_parameter(count,"ReturnValue",4)!=0)return false;
    copy_gear_bones=reinterpret_cast<CopyGearBones>(image+0x27eed20);
    gear_pose_stage=4;
    return true;
}

static bool gear_bone_layout(void* component,unsigned mesh){
    const auto& definition=Player::equipment_meshes[mesh];
    if(!definition.bone_count)return false;
    int count{};
    api.event(component,world_input(world_get_bone_count),&count);
    if(count!=static_cast<int>(definition.bone_count))return false;
    for(unsigned i=0;i<definition.bone_count;++i){
        auto& bone=gear_bone_names[definition.first_bone+i];
        if(!bone.index){
            const auto name=Player::equipment_bones[definition.first_bone+i];
            bone=gear_name(std::wstring(name.begin(),name.end()).c_str());
            if(!bone.index)return false;
        }
        alignas(16) FactoryParameters args{};
        factory_put(args,0,bone);
        api.event(component,world_input(world_get_bone_index),args.data());
        if(AnimU::field<int>(args.data(),8)!=static_cast<int>(i))return false;
    }
    return true;
}

static bool gear_bone_array(const GearBoneArray& array,unsigned count){
    return array.data&&reinterpret_cast<std::uintptr_t>(array.data)%16==0&&
        array.count==static_cast<int>(count)&&array.capacity>=array.count&&array.capacity<=512;
}

static Player::BoneTransform pack_gear_bone(const GearTransform& transform){
    return {transform[0],transform[1],transform[2],transform[3],
            transform[4],transform[5],transform[6],transform[8],transform[9],transform[10]};
}

static GearTransform unpack_gear_bone(const Player::BoneTransform& transform){
    return {transform[0],transform[1],transform[2],transform[3],
            transform[4],transform[5],transform[6],0,transform[7],transform[8],transform[9],0};
}

static bool capture_gear_bones(void* component,Player::GearPose& pose){
    // The native accessor waits for parallel evaluation and copies if that
    // task owns the array. With no task, the same game-thread buffer is stable.
    const auto parallel=AnimU::field<void*>(component,gear_parallel_at);
    GearBoneArray array{};
    if(parallel)copy_gear_bones(component,&array);
    else array=AnimU::field<GearBoneArray>(component,gear_source_bones_at);
    const bool valid=gear_bone_array(array,pose.count);
    if(valid)for(unsigned i=0;i<pose.count;++i)pose.bones[i]=pack_gear_bone(array.data[i]);
    if(parallel&&array.data)native_free(array.data);
    return valid;
}

static void send_gear_bones(const Player::Pose& frame,double time){
    for(unsigned i=0;i<frame.equipment.parts.size();++i){
        const auto& part=frame.equipment.parts[i];
        if(!part.mesh||!part.visible||!Player::equipment_meshes[part.mesh].bone_count)continue;
        if(!prepare_gear_bones(time))return;
        auto& source=gear_source.parts[i];
        const auto component=api.weakget(&source.component);
        if(!component||!api.isa(component,mesh_class)||source.serial!=part.serial||
           native_skeletal_asset(component)!=api.weakget(&source.mesh))continue;
        if(!source.bone_checked){
            source.bone_checked=true;
            source.bone_valid=gear_bone_layout(component,part.mesh);
        }
        if(!source.bone_valid)continue;
        Player::GearPose pose{frame.slot,frame.generation,frame.sequence,part.serial,part.mesh,
            static_cast<std::uint8_t>(i),static_cast<std::uint8_t>(Player::equipment_meshes[part.mesh].bone_count),{}};
        if(!capture_gear_bones(component,pose))continue;
        if(source.bone_sequence&&frame.sequence-source.bone_sequence<4&&source.sent_bones==pose.bones)continue;
        E::Packet packet{};
        if(Player::encode_gear_pose(envelope,pose,packet)&&send(packet,active_peer_mask)){
            ++gear_pose_sent;
            source.bone_sequence=frame.sequence;
            source.sent_bones=pose.bones;
        }
    }
}

static void apply_gear_bones(DisplayGearPart& display,const Player::GearPose& pose,double time){
    if(display.mesh!=pose.mesh||display.serial!=pose.serial||display.bone_sequence>=pose.sequence||
       !prepare_gear_bones(time))return;
    const auto component=api.weakget(&display.component);
    const auto type=world_input(world_pose_component);
    if(!component||!type||!api.isa(component,type)||native_skeletal_asset(component)!=api.weakget(&gear_meshes[pose.mesh].object))
        return;
    if(!display.bone_checked){
        display.bone_checked=true;
        display.bone_valid=gear_bone_layout(component,pose.mesh);
    }
    if(!display.bone_valid)return;
    const auto array=AnimU::field<GearBoneArray>(component,gear_display_bones_at);
    if(!gear_bone_array(array,pose.count)||AnimU::field<unsigned char>(component,gear_display_dirty_at)>1)return;
    bool changed=false;
    for(unsigned i=0;i<pose.count;++i){
        const auto transform=unpack_gear_bone(pose.bones[i]);
        if(pack_gear_bone(array.data[i])==pose.bones[i])continue;
        array.data[i]=transform;
        changed=true;
        ++gear_pose_writes;
    }
    if(changed)static_cast<unsigned char*>(component)[gear_display_dirty_at]=1;
    display.bone_sequence=pose.sequence;
    ++gear_pose_applied;
}
