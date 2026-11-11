#pragma once
#include "qdcoop/enemy_enrollment.hpp"
#include "qdcoop/visual_mount_assets.hpp"
#include <string>

namespace qdcoop::visual_mount {
namespace E=enemy;
inline constexpr unsigned kind=33,capacity=128,header=136,max_label=80,material_slots=8;
enum class Anchor : std::uint8_t { root,mesh,bone,hand_right,hand_left,world };
struct Frame {
    std::uint32_t stream{},generation{},owner{},owner_generation{};
    std::uint64_t sequence{};
    std::uint16_t asset{};
    Anchor anchor{};
    bool active{},visible{},skeletal{},has_dissolve{},player{};
    std::array<float,10> relative{}; // rotation, translation, scale
    std::array<std::uint16_t,material_slots> materials{};
    float dissolve{};
    std::string name,socket;
    bool operator==(const Frame&) const=default;
};

inline bool label(std::string_view text){
    return !text.empty()&&text.size()<=max_label&&std::ranges::all_of(text,[](char c){
        return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c==' ';
    });
}
inline bool valid(const Frame& frame){
    if(!frame.stream||frame.stream>capacity||!frame.generation||!frame.sequence||
       !frame.owner||frame.owner>(frame.player?5u:E::capacity)||!frame.owner_generation)return false;
    if(!frame.active)return frame.asset==0&&!frame.visible&&!frame.skeletal&&!frame.has_dissolve&&frame.name.empty()&&
        frame.socket.empty()&&frame.anchor==Anchor::root&&frame.relative==std::array<float,10>{}&&
        frame.materials==std::array<std::uint16_t,material_slots>{}&&frame.dissolve==0.f;
    if(!frame.asset||frame.asset>=(frame.skeletal?std::size(assets::skeletal_paths):std::size(assets::mesh_paths))||
       (frame.player?(frame.anchor!=Anchor::hand_right&&frame.anchor!=Anchor::hand_left):
           (frame.anchor>Anchor::bone&&frame.anchor!=Anchor::world))||
       !label(frame.name)||(frame.anchor==Anchor::bone?!label(frame.socket):!frame.socket.empty())||
       !std::isfinite(frame.dissolve)||std::abs(frame.dissolve)>10000.f||(!frame.has_dissolve&&frame.dissolve!=0.f)||
       std::ranges::any_of(frame.materials,[](auto id){return id>=std::size(assets::material_paths);}))return false;
    float norm{};
    for(unsigned i=0;i<frame.relative.size();++i){
        const auto value=frame.relative[i];
        const auto limit=i<4?1.1f:i>=7?128.f:frame.anchor==Anchor::world?10000000.f:100000.f;
        if(!std::isfinite(value)||std::abs(value)>limit)return false;
        if(i<4)norm+=value*value;
    }
    return norm>=.8f&&norm<=1.2f;
}

inline bool encode(const E::Envelope& scope,const Frame& frame,E::Packet& packet){
    if(!scope.world||!valid(frame))return false;
    const auto size=header+static_cast<unsigned>(frame.name.size()+frame.socket.size());
    E::enrollment::write_header(scope,kind,size,packet);
    using E::animation::wire::put;
    put(packet,40,frame.stream);put(packet,44,frame.generation);
    put(packet,48,frame.owner);put(packet,52,frame.owner_generation);put(packet,56,frame.sequence);
    put(packet,64,frame.asset);put(packet,66,static_cast<std::uint8_t>(frame.anchor));
    put(packet,67,static_cast<std::uint8_t>((frame.active?1:0)|(frame.visible?2:0)|(frame.skeletal?4:0)|
        (frame.has_dissolve?8:0)|(frame.player?16:0)));
    for(unsigned i=0;i<frame.relative.size();++i)put(packet,68+4*i,std::bit_cast<std::uint32_t>(frame.relative[i]));
    for(unsigned i=0;i<material_slots;++i)put(packet,108+2*i,frame.materials[i]);
    put(packet,124,std::bit_cast<std::uint32_t>(frame.dissolve));
    put(packet,128,static_cast<std::uint32_t>(frame.name.size()));
    put(packet,132,static_cast<std::uint32_t>(frame.socket.size()));
    std::memcpy(packet.data.data()+header,frame.name.data(),frame.name.size());
    std::memcpy(packet.data.data()+header+frame.name.size(),frame.socket.data(),frame.socket.size());
    return true;
}

inline bool decode(const E::Envelope& scope,const E::Packet& packet,Frame& out){
    if(!scope.world||packet.size<header||packet.size>header+2*max_label||
       !E::enrollment::check_header(scope,packet,kind,packet.size))return false;
    using E::animation::wire::get;
    const auto name=get<std::uint32_t>(packet,128),socket=get<std::uint32_t>(packet,132);
    const auto flags=get<std::uint8_t>(packet,67);
    if(name>max_label||socket>max_label||packet.size!=header+name+socket||flags>31)return false;
    Frame frame;
    frame.stream=get<std::uint32_t>(packet,40);frame.generation=get<std::uint32_t>(packet,44);
    frame.owner=get<std::uint32_t>(packet,48);frame.owner_generation=get<std::uint32_t>(packet,52);
    frame.sequence=get<std::uint64_t>(packet,56);frame.asset=get<std::uint16_t>(packet,64);
    frame.anchor=static_cast<Anchor>(get<std::uint8_t>(packet,66));
    frame.active=flags&1;frame.visible=flags&2;frame.skeletal=flags&4;frame.has_dissolve=flags&8;frame.player=flags&16;
    for(unsigned i=0;i<frame.relative.size();++i)frame.relative[i]=std::bit_cast<float>(get<std::uint32_t>(packet,68+4*i));
    for(unsigned i=0;i<material_slots;++i)frame.materials[i]=get<std::uint16_t>(packet,108+2*i);
    frame.dissolve=std::bit_cast<float>(get<std::uint32_t>(packet,124));
    const auto text=reinterpret_cast<const char*>(packet.data.data()+header);
    frame.name.assign(text,name);
    frame.socket.assign(text+name,socket);
    if(!valid(frame))return false;
    out=std::move(frame);
    return true;
}
}
