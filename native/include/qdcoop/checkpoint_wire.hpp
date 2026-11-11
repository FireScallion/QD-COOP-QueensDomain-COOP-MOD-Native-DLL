#pragma once
#include "qdcoop/enemy_enrollment.hpp"
#include "qdcoop/reward_journal.hpp"
#include "qdcoop/session.hpp"
#include "qdcoop/steam_room.hpp"

namespace qdcoop::checkpoint {
using enemy::reward::journal::SaveProof;
inline constexpr unsigned wire_kind=16,wire_size=352;
enum class Op : std::uint32_t {
    request=1, prepare, ready, reject, commit, committed, abort,
    wipe_prepare,wipe_ready,wipe_reject,wipe_commit,wipe_ack,wipe_launch,wipe_arrived,wipe_abort
};
inline bool wipe_op(Op op){return op>=Op::wipe_prepare&&op<=Op::wipe_abort;}
enum class RollbackStage { idle, verifying, prepared, writing, committed, launch, cleaning, loading, arrived, failed };
struct Participant {
    Id character;
    std::uint64_t account{};
    std::uint32_t generation{};
    bool operator==(const Participant&) const = default;
};
struct Roster {
    std::uint32_t mask{};
    std::array<Participant,max_players> members{};
    bool operator==(const Roster&) const = default;

    bool valid() const {
        if(!(mask&1)||!(mask&14)||(mask&~15u))return false;
        const bool steam=members[0].account!=0;
        for(unsigned i=0;i<members.size();++i){
            const auto& member=members[i];
            if(!(mask&(1u<<i))){
                if(member!=Participant{})return false;
                continue;
            }
            if(!member.character.valid()||!member.generation||
               (steam?!steam_room::user_id(member.account):member.account!=0))return false;
            for(unsigned j=0;j<i;++j)
                if((mask&(1u<<j))&&(members[j].character==member.character||
                   (steam&&members[j].account==member.account)))return false;
        }
        return true;
    }
};
struct Manifest {
    std::uint64_t token{},serial{};
    Roster roster;
    bool refresh{};
    std::array<SaveProof,max_players> saves{};
    std::uint64_t rollback{};
    bool operator==(const Manifest&) const = default;
};
struct Message {
    Manifest manifest;
    Op op{};
    std::uint32_t sender{};
};
struct BindingView {
    Manifest latest;
    enemy::Envelope room;
    std::uint64_t stock{},archives{},commits{},errors{},rollbacks{},world_nonce{};
    std::uint32_t stage{},slot{},flags{},failure{};
    RollbackStage rollback_stage{};
};
inline bool valid(const Message& message){
    const auto& m=message.manifest;
    if(message.op<Op::request||message.op>Op::wipe_abort||!m.roster.valid()||!m.serial||
       (message.op==Op::request?m.token!=0||m.refresh:!m.token)||message.sender>=max_players||
       !(m.roster.mask&(1u<<message.sender))||(wipe_op(message.op)!=(m.rollback!=0)))return false;
    const bool guest=message.op==Op::request||message.op==Op::ready||message.op==Op::reject||
                     message.op==Op::committed||message.op==Op::wipe_ready||message.op==Op::wipe_reject||
                     message.op==Op::wipe_ack||message.op==Op::wipe_arrived;
    if((message.sender!=0)!=guest)return false;
    const bool whole=message.op==Op::commit||message.op==Op::wipe_prepare||
                     message.op==Op::wipe_commit||message.op==Op::wipe_launch;
    const bool one=message.op==Op::ready||message.op==Op::committed||message.op==Op::wipe_ready||
                   message.op==Op::wipe_ack||message.op==Op::wipe_arrived;
    const auto expected=whole?m.roster.mask:one?1u<<message.sender:0u;
    for(unsigned i=0;i<m.saves.size();++i){
        if(expected&(1u<<i)){
            if(!m.saves[i].valid()||m.saves[i].bytes>256ull*1024*1024)return false;
        }else if(m.saves[i]!=SaveProof{})return false;
    }
    return true;
}
inline bool encode(const enemy::Envelope& room,const Message& message,enemy::Packet& packet){
    if(!valid(message))return false;
    enemy::enrollment::write_header(room,wire_kind,wire_size,packet);
    using enemy::animation::wire::put;
    const auto& m=message.manifest;
    put(packet,40,static_cast<std::uint32_t>(message.op));
    put(packet,44,message.sender);
    put(packet,48,m.token);
    put(packet,56,m.serial);
    put(packet,64,m.roster.mask);
    put(packet,68,static_cast<std::uint32_t>(m.refresh));
    for(unsigned i=0;i<m.roster.members.size();++i){
        const auto at=72+28*i;
        const auto& member=m.roster.members[i];
        put(packet,at,member.character.high);
        put(packet,at+8,member.character.low);
        put(packet,at+16,member.account);
        put(packet,at+24,member.generation);
        const auto proof_at=184+40*i;
        put(packet,proof_at,m.saves[i].bytes);
        std::memcpy(packet.data.data()+proof_at+8,m.saves[i].sha256.data(),32);
    }
    put(packet,344,m.rollback);
    return true;
}
inline bool decode(const enemy::Envelope& room,const enemy::Packet& packet,Message& out){
    if(!enemy::enrollment::check_header(room,packet,wire_kind,wire_size))return false;
    using enemy::animation::wire::get;
    Message value{};
    value.op=static_cast<Op>(get<std::uint32_t>(packet,40));
    value.sender=get<std::uint32_t>(packet,44);
    auto& m=value.manifest;
    m.token=get<std::uint64_t>(packet,48);
    m.serial=get<std::uint64_t>(packet,56);
    m.roster.mask=get<std::uint32_t>(packet,64);
    const auto refresh=get<std::uint32_t>(packet,68);
    if(refresh>1)return false;
    m.refresh=refresh!=0;
    for(unsigned i=0;i<m.roster.members.size();++i){
        const auto at=72+28*i;
        m.roster.members[i]={{get<std::uint64_t>(packet,at),get<std::uint64_t>(packet,at+8)},
                             get<std::uint64_t>(packet,at+16),get<std::uint32_t>(packet,at+24)};
        const auto proof_at=184+40*i;
        m.saves[i].bytes=get<std::uint64_t>(packet,proof_at);
        std::memcpy(m.saves[i].sha256.data(),packet.data.data()+proof_at+8,32);
    }
    m.rollback=get<std::uint64_t>(packet,344);
    if(!valid(value))return false;
    out=value;
    return true;
}
}
