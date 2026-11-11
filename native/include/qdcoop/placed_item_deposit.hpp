#pragma once
#include "qdcoop/placed_item_record.hpp"

namespace qdcoop::placed_item {
inline constexpr unsigned deposit_kind=31,deposit_header=44;
inline constexpr unsigned max_deposit=world_progress::max_name-escrow_prefix.size();

inline bool encode_deposit(const enemy::Envelope& scope,std::uint32_t generation,std::string_view raw,enemy::Packet& packet){
    if(!scope.world||!generation||raw.empty()||raw.size()>max_deposit)return false;
    const auto size=static_cast<unsigned>(raw.size());
    enemy::enrollment::write_header(scope,deposit_kind,deposit_header+size,packet);
    enemy::animation::wire::put(packet,40,generation);
    std::memcpy(packet.data.data()+deposit_header,raw.data(),size);
    return true;
}

inline bool decode_deposit(const enemy::Envelope& scope,const enemy::Packet& packet,std::uint32_t& generation,
                           std::string_view& raw,Drop& drop){
    if(!scope.world||packet.size<=deposit_header||packet.size>deposit_header+max_deposit||
       !enemy::enrollment::check_header(scope,packet,deposit_kind,packet.size))return false;
    const auto next=enemy::animation::wire::get<std::uint32_t>(packet,40);
    const std::string_view value(reinterpret_cast<const char*>(packet.data.data()+deposit_header),packet.size-deposit_header);
    if(!next||!decode(value,drop)||encode(drop)!=value)return false;
    generation=next;
    raw=value;
    return true;
}
}
