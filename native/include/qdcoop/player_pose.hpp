#pragma once
#include "qdcoop/enemy_enrollment.hpp"
#include "qdcoop/player_catalog.hpp"
#include "qdcoop/player_equipment.hpp"
#include "qdcoop/player_life.hpp"
#include "qdcoop/session.hpp"

namespace qdcoop::player {
inline constexpr std::uint32_t pose_kind=12,unassigned=UINT32_MAX;
inline constexpr unsigned slots=4,lane_count=7,body_end=220,equipment_end=body_end+equipment_size,pose_size=equipment_end+44;
inline constexpr std::uint16_t falling=1,running=2,dashing=4;

struct Lane {
    std::uint16_t clip{},playing{};
    std::uint32_t serial{};
    float position{},rate{};
    bool operator==(const Lane&) const = default;
};

struct Body {
    std::uint16_t flags{},root{},left{};
    float walk_speed{400.f},pitch{};
    std::array<Lane,lane_count> lanes{};
    bool operator==(const Body&) const = default;
};

struct Pose {
    std::uint32_t slot{},generation{};
    std::uint64_t sequence{};
    double time{};
    std::array<float,3> position{};
    float yaw{};
    std::array<float,3> velocity{};
    Body body{};
    Equipment equipment{};
    Life life{};
    Id rescue_identity{};
    std::uint64_t rescue_account{};
    std::uint32_t checkpoint_flags{};
    float health{};
    bool operator==(const Pose&) const = default;
};

struct Binding {
    std::int32_t object_index{},object_serial{};
    std::uint32_t generation{},visible{};
    std::uint64_t sequence{};
};

inline bool valid(const Pose& value){
    if((value.slot>=slots&&value.slot!=unassigned)||!value.generation||!value.sequence||
       !std::isfinite(value.time)||value.time<0||!std::isfinite(value.yaw)||std::abs(value.yaw)>180.f||
       (value.checkpoint_flags&~7u)||!std::isfinite(value.health)||value.health<0.f||value.health>1000000.f)
        return false;
    for(const auto coordinate:value.position)
        if(!std::isfinite(coordinate)||std::abs(coordinate)>1e7f)return false;
    for(const auto speed:value.velocity)
        if(!std::isfinite(speed)||std::abs(speed)>1e5f)return false;
    const auto& body=value.body;
    if(body.flags&~(falling|running|dashing)||!std::isfinite(body.walk_speed)||
       body.walk_speed<1.f||body.walk_speed>10000.f||!std::isfinite(body.pitch)||std::abs(body.pitch)>45.f)
        return false;
    for(auto clip:{body.root,body.left})
        if(clip>=std::size(clips)||(clip&&(clips[clip].lane||clips[clip].source.empty())))return false;
    for(unsigned i=0;i<lane_count;++i){
        const auto& lane=body.lanes[i];
        if(!lane.clip){
            if(lane!=Lane{})return false;
        }else if(lane.clip>=std::size(clips)||clips[lane.clip].lane!=i+1||!lane.serial||
                 lane.playing>1||!std::isfinite(lane.position)||lane.position<0||
                 lane.position>clips[lane.clip].length+.02f||!std::isfinite(lane.rate)||std::abs(lane.rate)>8.f)
            return false;
    }
    return valid_equipment(value.equipment)&&valid_life(value.life);
}

inline bool encode(const enemy::Envelope& room,const Pose& value,enemy::Packet& packet){
    if(!valid(value))return false;
    enemy::enrollment::write_header(room,pose_kind,pose_size,packet);
    using enemy::animation::wire::put;
    put(packet,40,value.slot);
    put(packet,44,value.generation);
    put(packet,48,value.sequence);
    put(packet,56,value.time);
    for(unsigned i=0;i<3;++i){
        put(packet,64+i*4,value.position[i]);
        put(packet,80+i*4,value.velocity[i]);
    }
    put(packet,76,value.yaw);
    const auto& body=value.body;
    put(packet,92,body.flags);
    put(packet,94,body.root);
    put(packet,96,body.left);
    put(packet,100,body.walk_speed);
    put(packet,104,body.pitch);
    for(unsigned i=0;i<lane_count;++i){
        const auto& lane=body.lanes[i];
        const auto at=108+i*16;
        put(packet,at,lane.clip);
        put(packet,at+2,lane.playing);
        put(packet,at+4,lane.serial);
        put(packet,at+8,lane.position);
        put(packet,at+12,lane.rate);
    }
    put(packet,body_end,value.equipment.skin);
    put(packet,body_end+2,value.equipment.weapon);
    for(unsigned i=0;i<equipment_slots;++i){
        const auto at=body_end+4+i*equipment_part_size;
        const auto& part=value.equipment.parts[i];
        put(packet,at,part.mesh);
        put(packet,at+2,part.hand);
        put(packet,at+3,part.visible);
        for(unsigned j=0;j<part.transform.size();++j)put(packet,at+4+j*4,part.transform[j]);
        for(unsigned j=0;j<part.materials.size();++j)put(packet,at+44+j*2,part.materials[j]);
        put(packet,at+60,part.serial);
    }
    put(packet,equipment_end,value.life.token);
    put(packet,equipment_end+8,static_cast<std::uint32_t>(value.life.phase));
    put(packet,equipment_end+12,value.rescue_identity.high);
    put(packet,equipment_end+20,value.rescue_identity.low);
    put(packet,equipment_end+28,value.rescue_account);
    put(packet,equipment_end+36,value.checkpoint_flags);
    put(packet,equipment_end+40,value.health);
    return true;
}

inline bool decode(const enemy::Envelope& room,const enemy::Packet& packet,Pose& out){
    if(!enemy::enrollment::check_header(room,packet,pose_kind,pose_size))return false;
    using enemy::animation::wire::get;
    Pose value{get<std::uint32_t>(packet,40),get<std::uint32_t>(packet,44),
               get<std::uint64_t>(packet,48),get<double>(packet,56),{},get<float>(packet,76),{}};
    for(unsigned i=0;i<3;++i){
        value.position[i]=get<float>(packet,64+i*4);
        value.velocity[i]=get<float>(packet,80+i*4);
    }
    if(get<std::uint16_t>(packet,98))return false;
    auto& body=value.body;
    body.flags=get<std::uint16_t>(packet,92);
    body.root=get<std::uint16_t>(packet,94);
    body.left=get<std::uint16_t>(packet,96);
    body.walk_speed=get<float>(packet,100);
    body.pitch=get<float>(packet,104);
    for(unsigned i=0;i<lane_count;++i){
        const auto at=108+i*16;
        body.lanes[i]={get<std::uint16_t>(packet,at),get<std::uint16_t>(packet,at+2),
            get<std::uint32_t>(packet,at+4),get<float>(packet,at+8),get<float>(packet,at+12)};
    }
    value.equipment.skin=get<std::uint16_t>(packet,body_end);
    value.equipment.weapon=get<std::uint16_t>(packet,body_end+2);
    for(unsigned i=0;i<equipment_slots;++i){
        const auto at=body_end+4+i*equipment_part_size;
        auto& part=value.equipment.parts[i];
        part.mesh=get<std::uint16_t>(packet,at);
        part.hand=get<std::uint8_t>(packet,at+2);
        part.visible=get<std::uint8_t>(packet,at+3);
        for(unsigned j=0;j<part.transform.size();++j)part.transform[j]=get<float>(packet,at+4+j*4);
        for(unsigned j=0;j<part.materials.size();++j)part.materials[j]=get<std::uint16_t>(packet,at+44+j*2);
        part.serial=get<std::uint32_t>(packet,at+60);
    }
    value.life={get<std::uint64_t>(packet,equipment_end),
        static_cast<LifePhase>(get<std::uint32_t>(packet,equipment_end+8))};
    value.rescue_identity={get<std::uint64_t>(packet,equipment_end+12),get<std::uint64_t>(packet,equipment_end+20)};
    value.rescue_account=get<std::uint64_t>(packet,equipment_end+28);
    value.checkpoint_flags=get<std::uint32_t>(packet,equipment_end+36);
    value.health=get<float>(packet,equipment_end+40);
    if(!valid(value))return false;
    out=value;
    return true;
}

// Guests have no authority over slot assignment. The host never echoes their pose.
inline bool assign_slot(std::uint32_t& slot,bool host,unsigned source,unsigned peer_mask,unsigned& relay_mask){
    relay_mask=0;
    if(!host)return source==0&&slot<slots;
    if(source>=slots-1||!(peer_mask&(1u<<source))||slot!=unassigned)return false;
    slot=source+1;
    relay_mask=peer_mask&~(1u<<source);
    return true;
}
}
