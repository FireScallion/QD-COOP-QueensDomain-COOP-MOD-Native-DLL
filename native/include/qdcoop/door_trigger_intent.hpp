#pragma once
#include "qdcoop/world_progress.hpp"

namespace qdcoop::door_trigger_intent {
struct Message {
    world_progress::Guid trigger{};
    std::uint64_t sequence{};
    std::uint32_t generation{};
    bool ack{};
    world_progress::Guid close_door{};
};
inline constexpr unsigned kind=39,bytes=88;

inline bool valid(const Message& message){
    return message.trigger!=world_progress::Guid{}&&!world_progress::personal(message.trigger)&&
        (message.close_door==world_progress::Guid{}||!world_progress::personal(message.close_door))&&
        message.sequence&&message.generation;
}
inline bool encode(const enemy::Envelope& scope,const Message& message,enemy::Packet& packet){
    if(!scope.world||!valid(message))return false;
    enemy::enrollment::write_header(scope,kind,bytes,packet);
    using enemy::animation::wire::put;
    for(unsigned i=0;i<message.trigger.size();++i)put(packet,40+i*4,message.trigger[i]);
    put(packet,56,message.sequence);
    put(packet,64,message.generation);
    put(packet,68,static_cast<std::uint32_t>(message.ack));
    for(unsigned i=0;i<message.close_door.size();++i)put(packet,72+i*4,message.close_door[i]);
    return true;
}
inline bool decode(const enemy::Envelope& scope,const enemy::Packet& packet,Message& out){
    if(!scope.world||!enemy::enrollment::check_header(scope,packet,kind,bytes))return false;
    using enemy::animation::wire::get;
    Message message;
    for(unsigned i=0;i<message.trigger.size();++i)message.trigger[i]=get<std::int32_t>(packet,40+i*4);
    message.sequence=get<std::uint64_t>(packet,56);
    message.generation=get<std::uint32_t>(packet,64);
    const auto ack=get<std::uint32_t>(packet,68);
    for(unsigned i=0;i<message.close_door.size();++i)message.close_door[i]=get<std::int32_t>(packet,72+i*4);
    if(ack>1||!valid(message))return false;
    message.ack=ack!=0;
    out=message;
    return true;
}
}
