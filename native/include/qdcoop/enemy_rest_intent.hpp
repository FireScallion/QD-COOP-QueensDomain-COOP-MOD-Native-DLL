#pragma once
#include "qdcoop/enemy_enrollment.hpp"

namespace qdcoop::enemy_rest_intent {
namespace E=enemy;
struct Message {
    std::uint64_t sequence{};
    std::uint32_t generation{};
    bool ack{};
};
inline constexpr unsigned kind=37,bytes=56;
inline bool encode(const E::Envelope& scope,const Message& message,E::Packet& packet){
    if(!scope.world||!message.sequence||!message.generation)return false;
    E::enrollment::write_header(scope,kind,bytes,packet);
    using E::animation::wire::put;
    put(packet,40,message.sequence);
    put(packet,48,message.generation);
    put(packet,52,static_cast<std::uint32_t>(message.ack));
    return true;
}
inline bool decode(const E::Envelope& scope,const E::Packet& packet,Message& out){
    if(!scope.world||!E::enrollment::check_header(scope,packet,kind,bytes))return false;
    using E::animation::wire::get;
    const auto sequence=get<std::uint64_t>(packet,40);
    const auto generation=get<std::uint32_t>(packet,48);
    const auto ack=get<std::uint32_t>(packet,52);
    if(!sequence||!generation||ack>1)return false;
    out={sequence,generation,ack!=0};
    return true;
}
}
