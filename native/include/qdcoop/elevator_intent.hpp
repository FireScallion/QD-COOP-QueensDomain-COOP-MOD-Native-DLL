#pragma once
#include "qdcoop/world_progress.hpp"

namespace qdcoop::elevator_intent {
namespace E=enemy;
enum class Result : std::uint32_t { request, accepted, refused };
struct Message {
    std::uint64_t serial{};
    world_progress::Guid key{};
    std::uint32_t generation{};
    bool end{};
    Result result{};
    bool operator==(const Message&) const=default;
};
inline constexpr unsigned kind=22,bytes=76;
inline bool valid(const Message& message){
    return message.serial&&message.generation&&message.key!=world_progress::Guid{}&&message.result<=Result::refused;
}
inline bool encode(const E::Envelope& scope,const Message& message,E::Packet& packet){
    if(!scope.world||!valid(message))return false;
    E::enrollment::write_header(scope,kind,bytes,packet);
    using E::animation::wire::put;
    put(packet,40,message.serial);
    for(unsigned i=0;i<message.key.size();++i)put(packet,48+i*4,std::bit_cast<std::uint32_t>(message.key[i]));
    put(packet,64,message.generation);
    put(packet,68,static_cast<std::uint32_t>(message.end));
    put(packet,72,static_cast<std::uint32_t>(message.result));
    return true;
}
inline bool decode(const E::Envelope& scope,const E::Packet& packet,Message& out){
    if(!scope.world||!E::enrollment::check_header(scope,packet,kind,bytes))return false;
    using E::animation::wire::get;
    Message message{};
    message.serial=get<std::uint64_t>(packet,40);
    for(unsigned i=0;i<message.key.size();++i)message.key[i]=std::bit_cast<std::int32_t>(get<std::uint32_t>(packet,48+i*4));
    message.generation=get<std::uint32_t>(packet,64);
    const auto end=get<std::uint32_t>(packet,68);
    message.end=end!=0;
    message.result=static_cast<Result>(get<std::uint32_t>(packet,72));
    if(end>1||!valid(message))return false;
    out=message;
    return true;
}
}
