#pragma once
#include "qdcoop/enemy_enrollment.hpp"
#include <array>
#include <bit>
#include <string_view>

namespace qdcoop::persistent_travel {
namespace E=enemy;
enum class Map : std::uint32_t { none,ruins,intro };
inline constexpr std::array<std::string_view,2> packages{
    "/Game/RuinsLevel/Maps/Ruins_Level_00",
    "/Game/RuinsLevel/Maps/Intro_Level_00"
};
inline constexpr std::string_view package(Map map){
    const auto index=static_cast<unsigned>(map)-1u;
    return index<packages.size()?packages[index]:std::string_view{};
}
inline constexpr Map identify(std::string_view path){
    const auto dot=path.rfind('.');
    const auto slash=path.rfind('/');
    if(dot==path.npos||slash==path.npos||dot<=slash+1||
       path.substr(slash+1,dot-slash-1)!=path.substr(dot+1))return Map::none;
    for(unsigned i=0;i<packages.size();++i)
        if(path.substr(0,dot)==packages[i])return static_cast<Map>(i+1);
    return Map::none;
}
inline constexpr std::uint64_t canonical_epoch(Map map){
    const auto path=package(map);
    if(path.empty())return 0;
    std::uint64_t hash=14695981039346656037ull;
    for(unsigned char byte:path)hash=(hash^byte)*1099511628211ull;
    hash=(hash^'.')*1099511628211ull;
    for(unsigned char byte:path.substr(path.rfind('/')+1))hash=(hash^byte)*1099511628211ull;
    return hash?hash:1;
}
inline constexpr std::uint64_t epoch(std::uint64_t canonical,std::uint64_t nonce){
    const auto scoped=canonical^std::rotl(nonce,17);
    return scoped?scoped:1;
}

struct Passport {
    std::uint64_t serial{},nonce{};
    Map map{};
    std::uint32_t startup{};
    bool operator==(const Passport&) const = default;
};
inline constexpr unsigned do_intro=1,wake_on_beach=2,startup_mask=do_intro|wake_on_beach;
inline constexpr unsigned wire_kind=21,wire_size=64;
inline bool valid(const Passport& value){
    return value.serial&&(value.serial==1||value.nonce)&&!package(value.map).empty()&&!(value.startup&~startup_mask);
}
inline E::Envelope room_scope(const E::Envelope& room){return {room.room_high,room.room_low,0};}
inline bool encode(const E::Envelope& room,const Passport& value,E::Packet& packet){
    if(!valid(value))return false;
    E::enrollment::write_header(room_scope(room),wire_kind,wire_size,packet);
    using E::animation::wire::put;
    put(packet,40,value.serial);
    put(packet,48,value.nonce);
    put(packet,56,static_cast<std::uint32_t>(value.map));
    put(packet,60,value.startup);
    return true;
}
inline bool decode(const E::Envelope& room,const E::Packet& packet,Passport& out){
    if(!E::enrollment::check_header(room_scope(room),packet,wire_kind,wire_size))return false;
    using E::animation::wire::get;
    const Passport value{get<std::uint64_t>(packet,40),get<std::uint64_t>(packet,48),
                         static_cast<Map>(get<std::uint32_t>(packet,56)),get<std::uint32_t>(packet,60)};
    if(!valid(value))return false;
    out=value;
    return true;
}
}
