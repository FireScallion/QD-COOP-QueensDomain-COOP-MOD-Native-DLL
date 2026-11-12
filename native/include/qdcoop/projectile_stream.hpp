#pragma once
#include "qdcoop/enemy_enrollment.hpp"
#include "qdcoop/player_pose.hpp"
#include "qdcoop/projectile_assets.hpp"
#include "qdcoop/projectile_materials.hpp"
#include <bit>
#include <cmath>

namespace qdcoop::projectile {
namespace E=enemy;
inline constexpr unsigned kind=26,bytes=168,capacity=32,parameter_capacity=8,parameter_header=3,parameter_max_bytes=19;
enum class Profile : std::uint32_t { none,stock_player,stock_enemy,stock_enemy_effect,stock_player_effect,stock_enemy_particle };
inline bool particle(Profile profile){return profile==Profile::stock_enemy_particle;}
inline bool effect(Profile profile){return profile==Profile::stock_enemy_effect||profile==Profile::stock_player_effect||particle(profile);}
inline bool enemy_source(Profile profile){return profile==Profile::stock_enemy||profile==Profile::stock_enemy_effect||particle(profile);}
enum class Phase : std::uint32_t { flying,retired };
struct Transform {
    std::array<float,3> position{},scale{};
    std::array<float,4> rotation{};
    bool operator==(const Transform&) const=default;
};
enum class ParameterType : std::uint8_t { none,floating,boolean,integer,vector,color };
inline constexpr unsigned parameter_words(ParameterType type){
    switch(type){
    case ParameterType::floating:case ParameterType::boolean:case ParameterType::integer:return 1;
    case ParameterType::vector:return 3;
    case ParameterType::color:return 4;
    default:return 0;
    }
}
struct Parameter {
    std::uint16_t id{};
    ParameterType type{};
    std::array<std::uint32_t,4> value{};
    bool operator==(const Parameter&) const=default;
};
struct Frame {
    std::uint32_t id{},generation{};
    Profile profile{};
    Phase phase{};
    std::uint64_t sequence{};
    Transform mesh{},trail{};
    std::array<float,3> velocity{};
    bool trail_active{};
    std::uint32_t slot{};
    std::uint16_t mesh_asset{},trail_asset{};
    std::array<Parameter,parameter_capacity> parameters{};
    bool trail_hidden{};
    std::uint16_t mesh_material{};
    bool operator==(const Frame&) const=default;
};
inline unsigned parameter_count(const Frame& frame){
    return static_cast<unsigned>(std::find_if(frame.parameters.begin(),frame.parameters.end(),
        [](const Parameter& item){return !item.id;})-frame.parameters.begin());
}
inline bool valid_parameter(const Parameter& item,Profile profile){
    const auto limit=enemy_source(profile)?assets::variables.size():assets::player_variables;
    const auto words=parameter_words(item.type);
    if(!item.id||item.id>limit||!words||(particle(profile)&&item.type!=ParameterType::floating))return false;
    if(item.type==ParameterType::boolean&&item.value[0]>1)return false;
    if(item.type!=ParameterType::integer&&item.type!=ParameterType::boolean)
        for(unsigned i=0;i<words;++i){
            const auto value=std::bit_cast<float>(item.value[i]);
            if(!std::isfinite(value)||std::abs(value)>10000000.f)return false;
        }
    return std::all_of(item.value.begin()+words,item.value.end(),[](std::uint32_t word){return !word;});
}
inline bool set_parameter(Frame& frame,const Parameter& value){
    if(!valid_parameter(value,frame.profile))return false;
    const auto end=frame.parameters.begin()+parameter_count(frame);
    const auto found=std::lower_bound(frame.parameters.begin(),end,value.id,
        [](const Parameter& item,std::uint16_t key){return item.id<key;});
    if(found!=end&&found->id==value.id){
        *found=value;
        return true;
    }
    if(end==frame.parameters.end())return false;
    std::move_backward(found,end,end+1);
    *found=value;
    return true;
}
inline bool set_scalar(Frame& frame,std::uint16_t id,float value){
    return set_parameter(frame,{id,ParameterType::floating,{std::bit_cast<std::uint32_t>(value)}});
}
template<std::size_t Size>inline bool bounded(const std::array<float,Size>& values,float limit){
    for(const auto value:values)if(!std::isfinite(value)||std::abs(value)>limit)return false;
    return true;
}
inline bool valid(const Frame& value){
    if(!value.id||value.id>capacity||!value.generation||!value.sequence||
       value.profile<Profile::stock_player||value.profile>Profile::stock_enemy_particle||value.phase>Phase::retired||
       (value.slot>=qdcoop::max_players&&value.slot!=UINT32_MAX))return false;
    const bool enemy=enemy_source(value.profile),fx=effect(value.profile);
    if(enemy&&value.slot!=0)return false;
    if(value.phase==Phase::retired)return value.mesh==Transform{}&&value.trail==Transform{}&&
        !value.trail_active&&!value.trail_hidden&&!value.mesh_asset&&!value.trail_asset&&!value.mesh_material&&
        value.velocity==std::array<float,3>{}&&
        value.parameters==std::array<Parameter,parameter_capacity>{};
    const auto count=parameter_count(value);
    if(count&&!value.trail_asset)return false;
    std::uint16_t previous{};
    for(unsigned i=0;i<count;++i){
        const auto& item=value.parameters[i];
        if(item.id<=previous||!valid_parameter(item,value.profile))return false;
        previous=item.id;
    }
    for(unsigned i=count;i<parameter_capacity;++i)if(value.parameters[i]!=Parameter{})return false;
    const bool has_trail_pose=value.trail_active||fx;
    if(value.trail_hidden&&!has_trail_pose)return false;
    const auto trail_limit=particle(value.profile)?assets::particles.size():(enemy?assets::trails.size():assets::player_trails);
    if(value.mesh_asset>(enemy?assets::meshes.size():assets::player_meshes)||value.trail_asset>trail_limit||
       (!value.mesh_asset&&!has_trail_pose)||(has_trail_pose&&!value.trail_asset))return false;
    if(value.mesh_material&&(!value.mesh_asset||value.mesh_material>assets::mesh_materials.size()))return false;
    if(fx&&
       (value.mesh_asset||value.mesh!=value.trail||value.velocity!=std::array<float,3>{}))return false;
    const auto transform=[](const Transform& pose){
        float norm{};
        for(const auto component:pose.rotation)norm+=component*component;
        return bounded(pose.position,10000000.f)&&norm>=.98f&&norm<=1.02f&&bounded(pose.scale,1000.f);
    };
    return transform(value.mesh)&&bounded(value.velocity,1000000.f)&&
        (has_trail_pose?transform(value.trail):value.trail==Transform{});
}
inline void put_transform(E::Packet& packet,unsigned offset,const Transform& pose){
    using E::animation::wire::put;
    for(unsigned i=0;i<3;++i){
        put(packet,offset+4*i,pose.position[i]);
        put(packet,offset+28+4*i,pose.scale[i]);
    }
    for(unsigned i=0;i<4;++i)put(packet,offset+12+4*i,pose.rotation[i]);
}
inline Transform get_transform(const E::Packet& packet,unsigned offset){
    using E::animation::wire::get;
    Transform pose;
    for(unsigned i=0;i<3;++i){
        pose.position[i]=get<float>(packet,offset+4*i);
        pose.scale[i]=get<float>(packet,offset+28+4*i);
    }
    for(unsigned i=0;i<4;++i)pose.rotation[i]=get<float>(packet,offset+12+4*i);
    return pose;
}
inline bool encode(const E::Envelope& scope,const Frame& value,E::Packet& packet){
    if(!scope.world||!valid(value))return false;
    const auto parameters=parameter_count(value);
    unsigned size=bytes;
    for(unsigned i=0;i<parameters;++i)size+=parameter_header+4*parameter_words(value.parameters[i].type);
    E::enrollment::write_header(scope,kind,size,packet);
    using E::animation::wire::put;
    put(packet,40,value.id);
    put(packet,44,value.generation);
    put(packet,48,static_cast<std::uint32_t>(value.profile));
    put(packet,52,static_cast<std::uint32_t>(value.phase));
    put(packet,56,value.sequence);
    put_transform(packet,64,value.mesh);
    for(unsigned i=0;i<3;++i)put(packet,104+4*i,value.velocity[i]);
    const std::uint32_t visual_flags=(value.trail_active?1u:0u)|(value.trail_hidden?2u:0u)|
        (static_cast<std::uint32_t>(value.mesh_material)<<8);
    put(packet,116,visual_flags);
    put_transform(packet,120,value.trail);
    put(packet,160,value.slot);
    put(packet,164,value.mesh_asset);
    put(packet,166,value.trail_asset);
    unsigned offset=bytes;
    for(unsigned i=0;i<parameters;++i){
        const auto& parameter=value.parameters[i];
        put(packet,offset,parameter.id);
        put(packet,offset+2,static_cast<std::uint8_t>(parameter.type));
        offset+=parameter_header;
        for(unsigned word=0;word<parameter_words(parameter.type);++word,offset+=4)put(packet,offset,parameter.value[word]);
    }
    return true;
}
inline bool decode(const E::Envelope& scope,const E::Packet& packet,Frame& out){
    if(!scope.world||packet.size<bytes||packet.size>bytes+parameter_capacity*parameter_max_bytes||
       !E::enrollment::check_header(scope,packet,kind,packet.size))return false;
    using E::animation::wire::get;
    Frame value;
    value.id=get<std::uint32_t>(packet,40);
    value.generation=get<std::uint32_t>(packet,44);
    value.profile=static_cast<Profile>(get<std::uint32_t>(packet,48));
    value.phase=static_cast<Phase>(get<std::uint32_t>(packet,52));
    value.sequence=get<std::uint64_t>(packet,56);
    value.mesh=get_transform(packet,64);
    for(unsigned i=0;i<3;++i)value.velocity[i]=get<float>(packet,104+4*i);
    const auto visual_flags=get<std::uint32_t>(packet,116);
    if(visual_flags&~0x00ffff03u)return false;
    value.trail_active=(visual_flags&1u)!=0;
    value.trail_hidden=(visual_flags&2u)!=0;
    value.mesh_material=static_cast<std::uint16_t>(visual_flags>>8);
    value.trail=get_transform(packet,120);
    value.slot=get<std::uint32_t>(packet,160);
    value.mesh_asset=get<std::uint16_t>(packet,164);
    value.trail_asset=get<std::uint16_t>(packet,166);
    for(unsigned offset=bytes,i=0;offset<packet.size;++i){
        if(i==parameter_capacity||packet.size-offset<parameter_header)return false;
        auto& parameter=value.parameters[i];
        parameter.id=get<std::uint16_t>(packet,offset);
        parameter.type=static_cast<ParameterType>(get<std::uint8_t>(packet,offset+2));
        const auto words=parameter_words(parameter.type);
        offset+=parameter_header;
        if(!parameter.id||!words||packet.size-offset<4*words)return false;
        for(unsigned word=0;word<words;++word,offset+=4)parameter.value[word]=get<std::uint32_t>(packet,offset);
    }
    if(!valid(value))return false;
    out=value;
    return true;
}
inline bool accept_origin(Frame& frame,bool host,unsigned source,unsigned peers,unsigned& relay){
    if(host&&enemy_source(frame.profile))return false;
    return player::assign_slot(frame.slot,host,source,peers,relay);
}
struct Track {
    Frame last{};
    double received{};
    bool push(const Frame& value,double now){
        if(!valid(value)||!std::isfinite(now)||now<received||value.generation<last.generation)return false;
        if(value.generation==last.generation&&(value.sequence<=last.sequence||value.profile!=last.profile||
           value.slot!=last.slot||(last.phase==Phase::retired&&value.phase!=Phase::retired)))return false;
        last=value;
        received=now;
        return true;
    }
    bool visible(double now) const {
        return last.sequence&&last.phase==Phase::flying&&std::isfinite(now)&&now>=received&&now-received<=.35;
    }
};
}
