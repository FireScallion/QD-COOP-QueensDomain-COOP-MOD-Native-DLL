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

struct Motion {
    world_progress::Guid key{};
    std::uint64_t sequence{};
    double time{};
    std::array<float,3> position{};
    std::array<float,4> rotation{0.f,0.f,0.f,1.f};
    std::uint32_t index{};
    bool running{},end{};
};
inline constexpr unsigned motion_kind=36,motion_bytes=108;
inline bool valid(const Motion& motion){
    if(!motion.sequence||!std::isfinite(motion.time)||motion.time<0.||motion.index>4096||
       motion.key==world_progress::Guid{}||world_progress::personal(motion.key))return false;
    if(!std::ranges::all_of(motion.position,[](float value){return std::isfinite(value)&&std::abs(value)<=10000000.f;}))
        return false;
    float norm{};
    for(float value:motion.rotation)norm+=value*value;
    return std::isfinite(norm)&&std::abs(norm-1.f)<.02f;
}
inline bool encode(const E::Envelope& scope,const Motion& motion,E::Packet& packet){
    if(!scope.world||!valid(motion))return false;
    E::enrollment::write_header(scope,motion_kind,motion_bytes,packet);
    using E::animation::wire::put;
    for(unsigned i=0;i<motion.key.size();++i)put(packet,40+i*4,std::bit_cast<std::uint32_t>(motion.key[i]));
    put(packet,56,motion.sequence);
    put(packet,64,std::bit_cast<std::uint64_t>(motion.time));
    for(unsigned i=0;i<3;++i)put(packet,72+i*4,std::bit_cast<std::uint32_t>(motion.position[i]));
    for(unsigned i=0;i<4;++i)put(packet,84+i*4,std::bit_cast<std::uint32_t>(motion.rotation[i]));
    put(packet,100,motion.index);
    put(packet,104,(motion.running?1u:0u)|(motion.end?2u:0u));
    return true;
}
inline bool decode(const E::Envelope& scope,const E::Packet& packet,Motion& out){
    if(!scope.world||!E::enrollment::check_header(scope,packet,motion_kind,motion_bytes))return false;
    using E::animation::wire::get;
    Motion motion;
    for(unsigned i=0;i<motion.key.size();++i)motion.key[i]=std::bit_cast<std::int32_t>(get<std::uint32_t>(packet,40+i*4));
    motion.sequence=get<std::uint64_t>(packet,56);
    motion.time=std::bit_cast<double>(get<std::uint64_t>(packet,64));
    for(unsigned i=0;i<3;++i)motion.position[i]=std::bit_cast<float>(get<std::uint32_t>(packet,72+i*4));
    for(unsigned i=0;i<4;++i)motion.rotation[i]=std::bit_cast<float>(get<std::uint32_t>(packet,84+i*4));
    motion.index=get<std::uint32_t>(packet,100);
    const auto flags=get<std::uint32_t>(packet,104);
    motion.running=(flags&1)!=0;
    motion.end=(flags&2)!=0;
    if(flags>3||!valid(motion))return false;
    out=motion;
    return true;
}
}
