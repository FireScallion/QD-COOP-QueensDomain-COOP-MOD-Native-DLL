#pragma once
#include "qdcoop/enemy_enrollment.hpp"

namespace qdcoop::player {
enum class LifePhase : std::uint32_t { unavailable, standing, down, rising };
struct Life {
    std::uint64_t token{};
    LifePhase phase{};
    bool operator==(const Life&) const = default;
};
inline bool valid_life(const Life& life){
    switch(life.phase){
    case LifePhase::unavailable:
    case LifePhase::standing:return !life.token;
    case LifePhase::down:
    case LifePhase::rising:return life.token!=0;
    }
    return false;
}
inline constexpr float rescue_range=235.f;
inline constexpr double rescue_hold=2.6,rescue_fresh=.5;
inline constexpr std::uint32_t rescue_kind=14,rescue_size=76;
enum class RescueOp : std::uint32_t { request=1, grant };
struct Rescue {
    std::uint32_t donor{},target{},donor_generation{},target_generation{};
    std::uint64_t token{},attempt{};
    RescueOp op{};
    bool operator==(const Rescue&) const = default;
};

inline bool valid_rescue(const Rescue& rescue){
    return rescue.target<4&&rescue.donor!=rescue.target&&rescue.donor_generation&&
        rescue.target_generation&&rescue.token&&rescue.attempt&&
        ((rescue.op==RescueOp::request&&(rescue.donor<4||rescue.donor==UINT32_MAX))||
         (rescue.op==RescueOp::grant&&rescue.donor<4));
}
inline bool encode_rescue(const enemy::Envelope& room,const Rescue& rescue,enemy::Packet& packet){
    if(!valid_rescue(rescue))return false;
    enemy::enrollment::write_header(room,rescue_kind,rescue_size,packet);
    using enemy::animation::wire::put;
    put(packet,40,rescue.donor);
    put(packet,44,rescue.target);
    put(packet,48,rescue.donor_generation);
    put(packet,52,rescue.target_generation);
    put(packet,56,rescue.token);
    put(packet,64,rescue.attempt);
    put(packet,72,static_cast<std::uint32_t>(rescue.op));
    return true;
}
inline bool decode_rescue(const enemy::Envelope& room,const enemy::Packet& packet,Rescue& out){
    if(!enemy::enrollment::check_header(room,packet,rescue_kind,rescue_size))return false;
    using enemy::animation::wire::get;
    const Rescue rescue{get<std::uint32_t>(packet,40),get<std::uint32_t>(packet,44),
        get<std::uint32_t>(packet,48),get<std::uint32_t>(packet,52),get<std::uint64_t>(packet,56),
        get<std::uint64_t>(packet,64),static_cast<RescueOp>(get<std::uint32_t>(packet,72))};
    if(!valid_rescue(rescue))return false;
    out=rescue;
    return true;
}
inline bool within_rescue_range(const std::array<float,3>& a,const std::array<float,3>& b){
    double squared=0;
    for(unsigned i=0;i<a.size();++i){
        const auto distance=static_cast<double>(a[i])-b[i];
        squared+=distance*distance;
    }
    return squared<=rescue_range*rescue_range;
}

struct RescueView {
    Life life;
    std::uint32_t armed{},locked{},target{UINT32_MAX};
    float progress{};
    std::uint32_t hold_stage{};
};
}
