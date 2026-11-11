#pragma once
#include "qdcoop/player_pose.hpp"

namespace qdcoop::player {
inline constexpr std::uint32_t gear_pose_kind=13;
inline constexpr unsigned gear_pose_header=64,gear_history=12;
static_assert(gear_pose_header+max_equipment_bones*40<=1184);

struct GearPose {
    std::uint32_t slot{},generation{};
    std::uint64_t sequence{};
    std::uint32_t serial{};
    std::uint16_t mesh{};
    std::uint8_t part{},count{};
    std::array<BoneTransform,max_equipment_bones> bones{};
    bool operator==(const GearPose&) const = default;
};
struct GearPoseBinding {
    std::uint64_t sequence{};
    std::uint32_t serial{};
    std::uint16_t mesh{};
    std::uint8_t hand{},visible{};
};

inline bool valid_gear_pose(const GearPose& pose){
    if((pose.slot>=slots&&pose.slot!=unassigned)||!pose.generation||!pose.sequence||!pose.serial||
       !pose.mesh||pose.mesh>=std::size(equipment_meshes)||pose.part>=equipment_slots||
       !pose.count||pose.count>max_equipment_bones||pose.count!=equipment_meshes[pose.mesh].bone_count)
        return false;
    for(unsigned i=0;i<pose.count;++i)if(!valid_gear_transform(pose.bones[i]))return false;
    return true;
}

inline bool encode_gear_pose(const enemy::Envelope& room,const GearPose& pose,enemy::Packet& packet){
    if(!valid_gear_pose(pose))return false;
    enemy::enrollment::write_header(room,gear_pose_kind,gear_pose_header+40*pose.count,packet);
    using enemy::animation::wire::put;
    put(packet,40,pose.slot);
    put(packet,44,pose.generation);
    put(packet,48,pose.sequence);
    put(packet,56,pose.serial);
    put(packet,60,pose.mesh);
    put(packet,62,pose.part);
    put(packet,63,pose.count);
    for(unsigned i=0;i<pose.count;++i)
        for(unsigned axis=0;axis<10;++axis)put(packet,gear_pose_header+i*40+axis*4,pose.bones[i][axis]);
    return true;
}

inline bool decode_gear_pose(const enemy::Envelope& room,const enemy::Packet& packet,GearPose& out){
    using enemy::animation::wire::get;
    if(packet.size<gear_pose_header)return false;
    const auto count=get<std::uint8_t>(packet,63);
    if(count>max_equipment_bones||
       !enemy::enrollment::check_header(room,packet,gear_pose_kind,gear_pose_header+40*count))return false;
    GearPose pose{get<std::uint32_t>(packet,40),get<std::uint32_t>(packet,44),get<std::uint64_t>(packet,48),
        get<std::uint32_t>(packet,56),get<std::uint16_t>(packet,60),get<std::uint8_t>(packet,62),count,{}};
    for(unsigned i=0;i<count;++i)
        for(unsigned axis=0;axis<10;++axis)pose.bones[i][axis]=get<float>(packet,gear_pose_header+i*40+axis*4);
    if(!valid_gear_pose(pose))return false;
    out=pose;
    return true;
}

inline bool same_part(const GearPose& pose,const EquipmentPart& part){
    return pose.serial==part.serial&&pose.mesh==part.mesh;
}

class GearHistory {
    std::array<GearPose,gear_history> poses_{};
public:
    bool push(const GearPose& pose,std::uint64_t latest){
        // Keep one forward slot for a companion that overtakes its base pose.
        if((pose.sequence>latest&&pose.sequence-latest>1)||
           (pose.sequence<=latest&&latest-pose.sequence>=gear_history-1))return false;
        auto& entry=poses_[pose.sequence%poses_.size()];
        if(entry.sequence>=pose.sequence)return false;
        entry=pose;
        return true;
    }
    const GearPose* sample(std::uint64_t sequence,const EquipmentPart& part) const {
        const GearPose* selected=nullptr;
        for(const auto& pose:poses_)
            if(pose.sequence&&pose.sequence<=sequence&&sequence-pose.sequence<gear_history&&
               same_part(pose,part)&&(!selected||pose.sequence>selected->sequence))
                selected=&pose;
        return selected;
    }
};
}
