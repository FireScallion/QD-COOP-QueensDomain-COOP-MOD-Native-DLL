#pragma once
#include "qdcoop/enemy_enrollment.hpp"
#include "qdcoop/player_pose.hpp"
#include "qdcoop/sound_assets.hpp"
#include <cmath>

namespace qdcoop::sound {
namespace E=enemy;
inline constexpr unsigned kind=27,bytes=84,control_kind=29,control_bytes=72;
enum class Origin : std::uint32_t {player=1,enemy};
struct Event {
    std::uint64_t serial{};
    std::uint32_t slot{};
    Origin origin{};
    std::uint16_t asset{},attenuation{};
    std::array<float,3> position{};
    float volume{},pitch{},start{};
    bool operator==(const Event&) const=default;
};
struct Control {
    std::uint64_t serial{};
    std::uint32_t slot{};
    Origin origin{};
    std::uint32_t revision{};
    float duration{},target{};
    std::uint32_t curve{};
    bool operator==(const Control&) const=default;
};
template<class Voice>inline bool valid_source(const Voice& value){
    return value.serial&&(value.slot<player::slots||value.slot==player::unassigned)&&
        (value.origin==Origin::player||(value.origin==Origin::enemy&&value.slot==0));
}
inline bool valid_voice(const Event& value){
    if(!value.asset||value.asset>assets::sounds.size()||value.attenuation>assets::attenuations.size()||
       !(value.volume>=0.f&&value.volume<=8.f&&value.pitch>=.1f&&value.pitch<=4.f&&value.start>=0.f&&value.start<=600.f))
        return false;
    for(const auto axis:value.position)if(!std::isfinite(axis)||std::abs(axis)>10000000.f)return false;
    return true;
}
inline bool valid(const Event& value){
    return valid_source(value)&&valid_voice(value)&&value.volume>0.f&&!assets::sounds[value.asset-1].looping;
}
inline void write_event_fields(const Event& value,E::Packet& packet){
    using E::animation::wire::put;
    put(packet,40,value.serial);
    put(packet,48,value.slot);
    put(packet,52,static_cast<std::uint32_t>(value.origin));
    put(packet,56,value.asset);
    put(packet,58,value.attenuation);
    for(unsigned i=0;i<3;++i)put(packet,60+4*i,value.position[i]);
    put(packet,72,value.volume);
    put(packet,76,value.pitch);
    put(packet,80,value.start);
}
inline Event read_event_fields(const E::Packet& packet){
    using E::animation::wire::get;
    Event value;
    value.serial=get<std::uint64_t>(packet,40);
    value.slot=get<std::uint32_t>(packet,48);
    value.origin=static_cast<Origin>(get<std::uint32_t>(packet,52));
    value.asset=get<std::uint16_t>(packet,56);
    value.attenuation=get<std::uint16_t>(packet,58);
    for(unsigned i=0;i<3;++i)value.position[i]=get<float>(packet,60+4*i);
    value.volume=get<float>(packet,72);
    value.pitch=get<float>(packet,76);
    value.start=get<float>(packet,80);
    return value;
}
inline bool encode(const E::Envelope& scope,const Event& value,E::Packet& packet){
    if(!scope.world||!valid(value))return false;
    E::enrollment::write_header(scope,kind,bytes,packet);
    write_event_fields(value,packet);
    return true;
}
inline bool decode(const E::Envelope& scope,const E::Packet& packet,Event& output){
    if(!scope.world||!E::enrollment::check_header(scope,packet,kind,bytes))return false;
    const auto value=read_event_fields(packet);
    if(!valid(value))return false;
    output=value;
    return true;
}
template<class Voice>inline bool accept_origin(Voice& event,bool host,unsigned source,unsigned peers,unsigned& relay){
    if(host&&event.origin!=Origin::player)return false;
    return player::assign_slot(event.slot,host,source,peers,relay);
}
inline bool valid(const Control& value){
    if(!valid_source(value)||!value.revision||!(value.duration>=0.f&&value.duration<=10.f))return false;
    if(value.duration==0.f)return value.target==0.f&&value.curve==0;
    return value.target>=0.f&&value.target<=8.f&&value.curve<=3;
}
inline bool encode(const E::Envelope& scope,const Control& value,E::Packet& packet){
    if(!scope.world||!valid(value))return false;
    E::enrollment::write_header(scope,control_kind,control_bytes,packet);
    using E::animation::wire::put;
    put(packet,40,value.serial);
    put(packet,48,value.slot);
    put(packet,52,static_cast<std::uint32_t>(value.origin));
    put(packet,56,value.revision);
    put(packet,60,value.duration);
    put(packet,64,value.target);
    put(packet,68,value.curve);
    return true;
}
inline bool decode(const E::Envelope& scope,const E::Packet& packet,Control& output){
    if(!scope.world||!E::enrollment::check_header(scope,packet,control_kind,control_bytes))return false;
    using E::animation::wire::get;
    const Control value{get<std::uint64_t>(packet,40),get<std::uint32_t>(packet,48),
        static_cast<Origin>(get<std::uint32_t>(packet,52)),get<std::uint32_t>(packet,56),
        get<float>(packet,60),get<float>(packet,64),get<std::uint32_t>(packet,68)};
    if(!valid(value))return false;
    output=value;
    return true;
}
struct ControlHistory {
    std::array<Control,64> entries{};
    unsigned next{};
    const Control* find(std::uint64_t serial) const {
        if(!serial)return nullptr;
        for(const auto& entry:entries)if(entry.serial==serial)return &entry;
        return nullptr;
    }
    bool push(const Control& value){
        for(auto& entry:entries)if(entry.serial==value.serial){
            if(!entry.duration||entry.revision>=value.revision)return false;
            entry=value;
            return true;
        }
        entries[next]=value;
        next=(next+1)%entries.size();
        return true;
    }
};
}
