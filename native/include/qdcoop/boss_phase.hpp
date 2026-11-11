#pragma once
#include "qdcoop/enemy_catalog.hpp"
#include "qdcoop/enemy_enrollment.hpp"

namespace qdcoop::boss_phase {
namespace E=enemy;
enum class Profile : std::uint8_t { none,queen,sipeha,josami,whale,shrine };
enum class Field : std::uint8_t { absent,integer,boolean };
enum Flag : std::uint8_t { dead=1,suspended=2,attacking=4 };

struct Descriptor {
    std::string_view path,component;
    std::array<std::string_view,3> names;
    std::array<Field,3> kinds;
};
inline constexpr std::array<Descriptor,6> descriptors{{
    {},
    {"/Game/Enemies/Queen/Queen_BP.Queen_BP_C","InteractSphere",
     {"CurrentStateIndex","DoingPhaseAnim","InTrapMode"},{Field::integer,Field::boolean,Field::boolean}},
    {"/Game/Enemies/Sipeha/Sipeha_BP.Sipeha_BP_C","HeadInteract",
     {"CurrentStateIndex"},{Field::integer}},
    {"/Game/Enemies/JosamiLord/JosamiLord_BP.JosamiLord_BP_C","InteractCollision",
     {"CurrentStateIndex","ActivePhaseIndex","StandaloneBoss"},{Field::integer,Field::integer,Field::boolean}},
    {"/Game/Enemies/WhaleKing/Whaleking_Boss_BP.Whaleking_Boss_BP_C","InteractBox",
     {"CurrentStateIndex","DoingPhaseAttack","Phases Executed On"},{Field::integer,Field::boolean,Field::integer}},
    {"/Game/Enemies/Boner_00/Skeletons24/ShrineBoss/Boner_Shrineboss_BP.Boner_Shrineboss_BP_C","Box",{},{}},
}};
inline constexpr auto classes=[] {
    std::array<Profile,std::size(E::catalog::classes)> result{};
    for(unsigned id=1;id<result.size();++id)
        for(unsigned profile=1;profile<descriptors.size();++profile)
            if(E::catalog::classes[id]==descriptors[profile].path)result[id]=static_cast<Profile>(profile);
    return result;
}();
inline constexpr Profile profile_for(unsigned class_id){return class_id<classes.size()?classes[class_id]:Profile::none;}

struct State {
    std::uint32_t id{},generation{};
    std::uint64_t sequence{};
    Profile profile{};
    std::uint8_t flags{},interaction{};
    std::array<std::int32_t,3> phase{};
    bool operator==(const State&) const=default;
};
inline constexpr unsigned kind=24,bytes=72;

inline bool valid(const State& state){
    const auto profile=static_cast<unsigned>(state.profile);
    if(!state.id||state.id>E::capacity||!state.generation||!state.sequence||!profile||profile>=descriptors.size()||
       (state.flags&~7u)||state.interaction>3)return false;
    const auto& fields=descriptors[profile].kinds;
    for(unsigned i=0;i<fields.size();++i){
        if(fields[i]==Field::absent&&state.phase[i])return false;
        if(fields[i]==Field::boolean&&(state.phase[i]<0||state.phase[i]>1))return false;
    }
    return true;
}

inline bool encode(const E::Envelope& scope,const State& state,E::Packet& packet){
    if(!scope.world||!valid(state))return false;
    E::enrollment::write_header(scope,kind,bytes,packet);
    using E::animation::wire::put;
    put(packet,40,state.id);
    put(packet,44,state.generation);
    put(packet,48,state.sequence);
    packet.data[56]=static_cast<std::uint8_t>(state.profile);
    packet.data[57]=state.flags;
    packet.data[58]=state.interaction;
    packet.data[59]=0;
    for(unsigned i=0;i<state.phase.size();++i)put(packet,60+4*i,std::bit_cast<std::uint32_t>(state.phase[i]));
    return true;
}
inline bool decode(const E::Envelope& scope,const E::Packet& packet,State& out){
    if(!scope.world||!E::enrollment::check_header(scope,packet,kind,bytes)||packet.data[59])return false;
    using E::animation::wire::get;
    State state{get<std::uint32_t>(packet,40),get<std::uint32_t>(packet,44),get<std::uint64_t>(packet,48),
        static_cast<Profile>(packet.data[56]),packet.data[57],packet.data[58],{}};
    for(unsigned i=0;i<state.phase.size();++i)state.phase[i]=std::bit_cast<std::int32_t>(get<std::uint32_t>(packet,60+4*i));
    if(!valid(state))return false;
    out=state;
    return true;
}
}
