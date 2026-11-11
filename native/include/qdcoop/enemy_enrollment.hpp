#pragma once
#include "qdcoop/animation_state.hpp"
#include "qdcoop/enemy_catalog.hpp"

namespace qdcoop::enemy::enrollment {
inline constexpr std::uint32_t offer_kind=8,ack_kind=9,retire_kind=10,retire_ack_kind=11;

struct Retirement {
    std::uint32_t id{},generation{};
    bool operator==(const Retirement&) const = default;
};

inline bool valid(const Retirement& value){
    return value.id&&value.id<=capacity&&value.generation;
}

struct Offer {
    std::uint32_t id{},generation{};
    std::uint16_t class_id{},mesh_id{};
    std::array<float,3> location{};
    bool operator==(const Offer&) const = default;
};
static_assert(sizeof(Offer)==24);

inline bool valid(const Offer& offer){
    if(!offer.id||offer.id>capacity||!offer.generation||
       !offer.class_id||offer.class_id>=std::size(catalog::classes)||
       !offer.mesh_id||offer.mesh_id>=std::size(catalog::meshes))return false;
    for(const auto coordinate:offer.location)
        if(!std::isfinite(coordinate)||std::abs(coordinate)>10000000.f)return false;
    return true;
}

inline void write_header(const Envelope& room,std::uint32_t kind,unsigned size,Packet& packet){
    using animation::wire::put;
    packet={};
    packet.size=size;
    put(packet,0,std::uint32_t{0x45344e51});
    put(packet,4,std::uint32_t{1});
    put(packet,8,room.room_high);
    put(packet,16,room.room_low);
    put(packet,24,room.world);
    put(packet,32,kind);
    put(packet,36,packet.size-40);
}

inline bool check_header(const Envelope& room,const Packet& packet,std::uint32_t kind,unsigned size){
    using animation::wire::get;
    return packet.size==size&&get<std::uint32_t>(packet,0)==0x45344e51&&
        get<std::uint32_t>(packet,4)==1&&get<std::uint64_t>(packet,8)==room.room_high&&
        get<std::uint64_t>(packet,16)==room.room_low&&get<std::uint64_t>(packet,24)==room.world&&
        get<std::uint32_t>(packet,32)==kind&&get<std::uint32_t>(packet,36)==size-40;
}

inline bool encode(const Envelope& room,const Offer& offer,bool ack,Packet& packet){
    if(!valid(offer))return false;
    write_header(room,ack?ack_kind:offer_kind,64,packet);
    using animation::wire::put;
    put(packet,40,offer.id);
    put(packet,44,offer.generation);
    put(packet,48,offer.class_id);
    put(packet,50,offer.mesh_id);
    for(unsigned i=0;i<offer.location.size();++i)put(packet,52+i*4,offer.location[i]);
    return true;
}

inline bool decode(const Envelope& room,const Packet& packet,bool ack,Offer& out){
    using animation::wire::get;
    if(!check_header(room,packet,ack?ack_kind:offer_kind,64))return false;
    Offer candidate{get<std::uint32_t>(packet,40),get<std::uint32_t>(packet,44),
                    get<std::uint16_t>(packet,48),get<std::uint16_t>(packet,50),{}};
    for(unsigned i=0;i<candidate.location.size();++i)candidate.location[i]=get<float>(packet,52+i*4);
    if(!valid(candidate))return false;
    out=candidate;
    return true;
}

inline bool encode(const Envelope& room,const Retirement& value,bool ack,Packet& packet){
    if(!valid(value))return false;
    write_header(room,ack?retire_ack_kind:retire_kind,48,packet);
    animation::wire::put(packet,40,value.id);
    animation::wire::put(packet,44,value.generation);
    return true;
}

inline bool decode(const Envelope& room,const Packet& packet,bool ack,Retirement& out){
    if(!check_header(room,packet,ack?retire_ack_kind:retire_kind,48))return false;
    const Retirement value{animation::wire::get<std::uint32_t>(packet,40),
                           animation::wire::get<std::uint32_t>(packet,44)};
    if(!valid(value))return false;
    out=value;
    return true;
}
}
