#pragma once
#include "qdcoop/enemy_enrollment.hpp"

namespace qdcoop::cutscene {
namespace E=enemy;

enum class Manager : std::uint32_t { ending1=1,ending2,ending3,ending4,ritual,sipeha };
enum class Phase : std::uint32_t { prepare=1,ready,play,skip,finished,commit,ack };

struct Message {
    std::uint64_t serial{},revision{};
    Manager manager{};
    Phase phase{};
    std::uint32_t position_ms{};
    bool operator==(const Message&) const=default;
};

inline constexpr unsigned kind=23,bytes=72;
inline constexpr std::uint32_t max_position_ms=7'200'000;

inline bool valid(const Message& message){
    return message.serial&&message.manager>=Manager::ending1&&message.manager<=Manager::sipeha&&
        message.phase>=Phase::prepare&&message.phase<=Phase::ack&&
        ((message.phase==Phase::commit)==(message.revision!=0))&&
        (message.phase==Phase::play?message.position_ms<=max_position_ms:message.position_ms==0);
}

inline bool encode(const E::Envelope& scope,const Message& message,E::Packet& packet){
    if(!scope.world||!valid(message))return false;
    E::enrollment::write_header(scope,kind,bytes,packet);
    using E::animation::wire::put;
    put(packet,40,message.serial);
    put(packet,48,message.revision);
    put(packet,56,static_cast<std::uint32_t>(message.manager));
    put(packet,60,static_cast<std::uint32_t>(message.phase));
    put(packet,64,message.position_ms);
    put(packet,68,std::uint32_t{});
    return true;
}

inline bool decode(const E::Envelope& scope,const E::Packet& packet,Message& out){
    if(!scope.world||!E::enrollment::check_header(scope,packet,kind,bytes))return false;
    using E::animation::wire::get;
    const Message message{get<std::uint64_t>(packet,40),get<std::uint64_t>(packet,48),
        static_cast<Manager>(get<std::uint32_t>(packet,56)),static_cast<Phase>(get<std::uint32_t>(packet,60)),
        get<std::uint32_t>(packet,64)};
    if(get<std::uint32_t>(packet,68)||!valid(message))return false;
    out=message;
    return true;
}
}
