#pragma once
#include "qdcoop/enemy_enrollment.hpp"

namespace qdcoop::boss_interact {
namespace E=enemy;
enum class Result : std::uint32_t { request,accepted,refused };
struct Message {
    std::uint64_t serial{};
    std::uint32_t id{},generation{},member{};
    Result result{};
    bool operator==(const Message&) const=default;
};
inline constexpr unsigned kind=25,bytes=64;
inline bool valid(const Message& message){
    return message.serial&&message.id&&message.id<=E::capacity&&message.generation&&message.member&&message.result<=Result::refused;
}
inline bool encode(const E::Envelope& scope,const Message& message,E::Packet& packet){
    if(!scope.world||!valid(message))return false;
    E::enrollment::write_header(scope,kind,bytes,packet);
    using E::animation::wire::put;
    put(packet,40,message.serial);
    put(packet,48,message.id);
    put(packet,52,message.generation);
    put(packet,56,message.member);
    put(packet,60,static_cast<std::uint32_t>(message.result));
    return true;
}
inline bool decode(const E::Envelope& scope,const E::Packet& packet,Message& out){
    if(!scope.world||!E::enrollment::check_header(scope,packet,kind,bytes))return false;
    using E::animation::wire::get;
    const Message message{get<std::uint64_t>(packet,40),get<std::uint32_t>(packet,48),get<std::uint32_t>(packet,52),
        get<std::uint32_t>(packet,56),static_cast<Result>(get<std::uint32_t>(packet,60))};
    if(!valid(message))return false;
    out=message;
    return true;
}
}
