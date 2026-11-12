#pragma once
#include "qdcoop/world_progress.hpp"

namespace qdcoop::world_interaction_intent {
namespace E=enemy;
enum class Action : std::uint32_t {breakable=1,door,lever,temporary_lever,secret,dagger_button,gargoyle};
struct Message {
    world_progress::Guid key{};
    std::uint32_t generation{};
    Action action{};
};
inline constexpr unsigned kind=35,bytes=64;
inline bool valid(const Message& message){
    return message.generation&&message.key!=world_progress::Guid{}&&!world_progress::personal(message.key)&&
           message.action>=Action::breakable&&message.action<=Action::gargoyle;
}
inline bool encode(const E::Envelope& scope,const Message& message,E::Packet& packet){
    if(!scope.world||!valid(message))return false;
    E::enrollment::write_header(scope,kind,bytes,packet);
    using E::animation::wire::put;
    for(unsigned i=0;i<message.key.size();++i)put(packet,40+i*4,std::bit_cast<std::uint32_t>(message.key[i]));
    put(packet,56,message.generation);
    put(packet,60,static_cast<std::uint32_t>(message.action));
    return true;
}
inline bool decode(const E::Envelope& scope,const E::Packet& packet,Message& out){
    if(!scope.world||!E::enrollment::check_header(scope,packet,kind,bytes))return false;
    using E::animation::wire::get;
    Message message{};
    for(unsigned i=0;i<message.key.size();++i)message.key[i]=std::bit_cast<std::int32_t>(get<std::uint32_t>(packet,40+i*4));
    message.generation=get<std::uint32_t>(packet,56);
    message.action=static_cast<Action>(get<std::uint32_t>(packet,60));
    if(!valid(message))return false;
    out=message;
    return true;
}
}
