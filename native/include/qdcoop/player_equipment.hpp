#pragma once
#include "qdcoop/equipment_catalog.hpp"
#include "qdcoop/player_skin.hpp"
#include <array>
#include <cmath>
#include <cstdint>

namespace qdcoop::player {
inline constexpr unsigned equipment_slots=4,material_slots=8,equipment_part_size=64,equipment_size=4+equipment_slots*equipment_part_size;
using BoneTransform=std::array<float,10>;
struct EquipmentPart {
    std::uint16_t mesh{};
    std::uint8_t hand{},visible{};
    // Rotation quaternion, hand-relative translation, and scale.
    BoneTransform transform{};
    std::array<std::uint16_t,material_slots> materials{};
    std::uint32_t serial{};
    bool operator==(const EquipmentPart&) const = default;
};
struct Equipment {
    std::uint16_t skin{1},weapon{};
    std::array<EquipmentPart,equipment_slots> parts{};
    bool operator==(const Equipment&) const = default;
};
struct EquipmentBinding {
    std::uint16_t skin{},weapon{};
    struct Part {
        std::int32_t object_index{},object_serial{};
        std::uint16_t mesh{},visible{};
    };
    std::array<Part,equipment_slots> parts{};
};

inline bool valid_gear_transform(const BoneTransform& transform){
    float norm=0;
    for(unsigned i=0;i<transform.size();++i){
        const auto value=transform[i];
        if(!std::isfinite(value)||std::abs(value)>(i<4?1.001f:i<7?10000.f:100.f))return false;
        if(i<4)norm+=value*value;
    }
    return norm>=.99f&&norm<=1.01f;
}
inline bool valid_part(const EquipmentPart& part){
    if(!part.mesh)return part==EquipmentPart{};
    if(part.mesh>=std::size(equipment_meshes)||!part.serial||part.hand<1||part.hand>2||part.visible>1||
       !valid_gear_transform(part.transform))return false;
    for(const auto material:part.materials)
        if(material>=std::size(equipment_materials))return false;
    return true;
}
inline bool valid_equipment(const Equipment& equipment){
    if(!valid_skin(equipment.skin)||equipment.weapon>=std::size(weapons))return false;
    for(const auto& part:equipment.parts)if(!valid_part(part))return false;
    return true;
}
}
