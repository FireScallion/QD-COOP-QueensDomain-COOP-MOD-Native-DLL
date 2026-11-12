#pragma once
#include "qdcoop/enemy_enrollment.hpp"

namespace qdcoop::story_reward_intent {
enum class Stage : std::uint32_t { claimed=1, unlocked };
struct Message {
    std::uint32_t generation{};
    Stage stage{};
};
inline constexpr unsigned kind=38,bytes=48;

inline bool encode(const enemy::Envelope& scope,const Message& message,enemy::Packet& packet){
    if(!scope.world||!message.generation||message.stage<Stage::claimed||message.stage>Stage::unlocked)return false;
    enemy::enrollment::write_header(scope,kind,bytes,packet);
    enemy::animation::wire::put(packet,40,message.generation);
    enemy::animation::wire::put(packet,44,static_cast<std::uint32_t>(message.stage));
    return true;
}
inline bool decode(const enemy::Envelope& scope,const enemy::Packet& packet,Message& message){
    if(!scope.world||!enemy::enrollment::check_header(scope,packet,kind,bytes))return false;
    const auto generation=enemy::animation::wire::get<std::uint32_t>(packet,40);
    const auto stage=static_cast<Stage>(enemy::animation::wire::get<std::uint32_t>(packet,44));
    if(!generation||stage<Stage::claimed||stage>Stage::unlocked)return false;
    message={generation,stage};
    return true;
}
}
