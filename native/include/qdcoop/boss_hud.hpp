#pragma once
#include "qdcoop/enemy_enrollment.hpp"
#include <string>

namespace qdcoop::boss_hud {
namespace E=enemy;
inline constexpr unsigned kind=34,base_size=56,capacity=32,group_limit=256;

struct Target {
    std::uint32_t id{},generation{};
    bool operator==(const Target&) const=default;
};
struct Frame {
    std::uint64_t sequence{};
    std::array<Target,capacity> targets{};
    unsigned count{};
    std::string group;
    bool same_membership(const Frame& other) const {
        return count==other.count&&targets==other.targets&&group==other.group;
    }
};

inline bool valid_group(std::string_view path){
    if(path.empty())return true;
    const auto dot=path.find('.');
    const auto slash=path.rfind('/');
    if(path.size()>group_limit||!path.starts_with("/Game/")||dot==path.npos||dot<=slash+1||
       dot+1==path.size()||path.find('.',dot+1)!=path.npos||path.find("//")!=path.npos)return false;
    return std::ranges::all_of(path,[](char c){
        return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='/'||c=='.';
    });
}

inline bool valid(const Frame& frame){
    if(!frame.sequence||frame.count>capacity||(!frame.count&&!frame.group.empty())||!valid_group(frame.group))return false;
    unsigned last{};
    for(unsigned i=0;i<frame.count;++i){
        const auto target=frame.targets[i];
        if(target.id<=last||target.id>E::capacity||!target.generation)return false;
        last=target.id;
    }
    return true;
}

inline bool encode(const E::Envelope& scope,const Frame& frame,E::Packet& packet){
    if(!scope.world||!valid(frame))return false;
    const auto group_at=base_size+8*frame.count;
    E::enrollment::write_header(scope,kind,group_at+static_cast<unsigned>(frame.group.size()),packet);
    using E::animation::wire::put;
    put(packet,40,frame.sequence);
    put(packet,48,std::uint32_t{frame.count});
    put(packet,52,static_cast<std::uint32_t>(frame.group.size()));
    for(unsigned i=0;i<frame.count;++i){
        put(packet,base_size+8*i,frame.targets[i].id);
        put(packet,base_size+8*i+4,frame.targets[i].generation);
    }
    std::copy(frame.group.begin(),frame.group.end(),packet.data.begin()+group_at);
    return true;
}

inline bool decode(const E::Envelope& scope,const E::Packet& packet,Frame& out){
    if(!scope.world||packet.size<base_size||packet.size>base_size+8*capacity+group_limit||
       !E::enrollment::check_header(scope,packet,kind,packet.size))return false;
    using E::animation::wire::get;
    const auto count=get<std::uint32_t>(packet,48),length=get<std::uint32_t>(packet,52);
    if(count>capacity||length>group_limit||packet.size!=base_size+8*count+length)return false;
    Frame frame;
    frame.sequence=get<std::uint64_t>(packet,40);
    frame.count=count;
    for(unsigned i=0;i<count;++i)frame.targets[i]={get<std::uint32_t>(packet,base_size+8*i),
                                                  get<std::uint32_t>(packet,base_size+8*i+4)};
    frame.group.assign(reinterpret_cast<const char*>(packet.data.data()+base_size+8*count),length);
    if(!valid(frame))return false;
    out=std::move(frame);
    return true;
}
}
