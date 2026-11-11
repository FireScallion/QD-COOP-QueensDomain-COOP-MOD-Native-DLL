#pragma once
#include "qdcoop/enemy_enrollment.hpp"
#include "qdcoop/visual_mount_assets.hpp"

namespace qdcoop::body_material {
namespace E=enemy;
inline constexpr unsigned kind=32,bytes=80,material_slots=8;
struct Frame {
    std::uint32_t id{},generation{};
    std::uint64_t sequence{};
    float dissolve{};
    bool active{};
    std::array<std::uint16_t,material_slots> materials{};
    bool operator==(const Frame&) const=default;
};

inline bool valid(const Frame& frame){
    return frame.id&&frame.id<=E::capacity&&frame.generation&&frame.sequence&&std::isfinite(frame.dissolve)&&
        std::abs(frame.dissolve)<=10000.f&&(frame.active||
            (frame.dissolve==0.f&&frame.materials==std::array<std::uint16_t,material_slots>{}))&&
        std::ranges::all_of(frame.materials,[](auto id){return id<std::size(visual_mount::assets::material_paths);});
}

inline bool encode(const E::Envelope& scope,const Frame& frame,E::Packet& packet){
    if(!scope.world||!valid(frame))return false;
    E::enrollment::write_header(scope,kind,bytes,packet);
    using E::animation::wire::put;
    put(packet,40,frame.id);
    put(packet,44,frame.generation);
    put(packet,48,frame.sequence);
    put(packet,56,std::bit_cast<std::uint32_t>(frame.dissolve));
    put(packet,60,std::uint32_t{frame.active});
    for(unsigned i=0;i<material_slots;++i)put(packet,64+2*i,frame.materials[i]);
    return true;
}

inline bool decode(const E::Envelope& scope,const E::Packet& packet,Frame& out){
    if(!scope.world||!E::enrollment::check_header(scope,packet,kind,bytes))return false;
    using E::animation::wire::get;
    const auto active=get<std::uint32_t>(packet,60);
    Frame frame{get<std::uint32_t>(packet,40),get<std::uint32_t>(packet,44),get<std::uint64_t>(packet,48),
        std::bit_cast<float>(get<std::uint32_t>(packet,56)),active!=0};
    for(unsigned i=0;i<material_slots;++i)frame.materials[i]=get<std::uint16_t>(packet,64+2*i);
    if(active>1||!valid(frame))return false;
    out=frame;
    return true;
}
}
