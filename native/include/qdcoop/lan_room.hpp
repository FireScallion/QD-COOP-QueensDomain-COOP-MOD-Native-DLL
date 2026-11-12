#pragma once
#include "qdcoop/enemy_enrollment.hpp"

namespace qdcoop::lan_room {
namespace E=enemy;

struct Token {
    std::uint64_t high{},low{};
    bool operator==(const Token&) const=default;
    explicit operator bool() const {return high&&low;}
};
enum class Op : std::uint32_t {hello=1,challenge,proof,welcome,ready,active,ping,bye,refused,restart};
enum class Phase : std::uint32_t { idle,waiting,active,closed,failed,recovering };
enum class Failure : std::uint32_t { none,game_build,socket,random,timeout,full,departure,recovery,durable };
inline constexpr std::uint32_t protocol=0x4e344c1f,magic=0x4e414c51;
inline constexpr unsigned unassigned=255,bytes=72;
struct Control {
    Op op{};
    std::uint32_t epoch{},slot{unassigned},seats{};
    Token client,proof,room;
    bool operator==(const Control&) const=default;
};
struct Status {
    Phase phase{};
    Failure failure{};
    std::uint32_t epoch{},seats{},connected{},peer_mask{},slot{unassigned};
    Token room;
};
using LocalRoster=std::array<std::uint64_t,3>;

inline bool valid(const Control& control){
    if(!control.client||control.op<Op::hello||control.op>Op::restart)return false;
    if(control.op==Op::hello)
        return !control.epoch&&control.slot==unassigned&&!control.seats&&control.proof==Token{}&&control.room==Token{};
    if(!control.epoch||!control.proof||control.seats<2||control.seats>4)return false;
    if(control.op==Op::challenge||control.op==Op::proof||control.op==Op::refused)
        return control.slot==unassigned&&control.room==Token{};
    return control.slot<control.seats-1&&static_cast<bool>(control.room);
}

inline bool encode(const Control& control,E::Packet& packet){
    if(!valid(control))return false;
    using E::animation::wire::put;
    packet.size=bytes;
    put(packet,0,magic);
    put(packet,4,protocol);
    put(packet,8,static_cast<std::uint32_t>(control.op));
    put(packet,12,control.epoch);
    put(packet,16,control.slot);
    put(packet,20,control.seats);
    put(packet,24,control.client.high);
    put(packet,32,control.client.low);
    put(packet,40,control.proof.high);
    put(packet,48,control.proof.low);
    put(packet,56,control.room.high);
    put(packet,64,control.room.low);
    return true;
}

inline bool decode(const E::Packet& packet,Control& out){
    using E::animation::wire::get;
    if(packet.size!=bytes||get<std::uint32_t>(packet,0)!=magic||get<std::uint32_t>(packet,4)!=protocol)return false;
    const Control control{static_cast<Op>(get<std::uint32_t>(packet,8)),get<std::uint32_t>(packet,12),
        get<std::uint32_t>(packet,16),get<std::uint32_t>(packet,20),
        {get<std::uint64_t>(packet,24),get<std::uint64_t>(packet,32)},
        {get<std::uint64_t>(packet,40),get<std::uint64_t>(packet,48)},
        {get<std::uint64_t>(packet,56),get<std::uint64_t>(packet,64)}};
    if(!valid(control))return false;
    out=control;
    return true;
}
}
