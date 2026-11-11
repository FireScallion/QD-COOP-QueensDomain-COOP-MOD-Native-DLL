#pragma once
#include "qdcoop/animation_state.hpp"
#include "qdcoop/session.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace qdcoop::steam_room {
using SteamId=std::uint64_t;
constexpr int channel=18404;
constexpr std::uint32_t app_id=1799840;
inline constexpr char transport[]="QDN4-53";
inline constexpr char build[]="26496428";

constexpr bool user_id(SteamId id){
    return (id>>56)>0&&(id>>56)<5&&((id>>52)&15)==1&&static_cast<std::uint32_t>(id)!=0;
}
constexpr bool lobby_id(SteamId id){
    return (id>>56)>0&&(id>>56)<5&&((id>>52)&15)==8&&static_cast<std::uint32_t>(id)!=0;
}

enum class Phase : std::uint32_t { idle, connecting, waiting, handshake, active, closed, failed, recovering };
enum class Failure : std::uint32_t {
    none, game_build, unavailable, app, call, timeout, metadata, membership, handshake, send, recovery, durable
};
struct Status {
    std::uint32_t phase{},failure{},members{},limit{},connected{},expected{},peer_mask{},left{};
    SteamId lobby{},nonce{};
    std::uint32_t initialized_here{},enter_response{};
    std::int32_t init_result{};
    std::uint32_t epoch{};
};
static_assert(sizeof(Status)==64);

struct Roster {
    SteamId owner{};
    unsigned count{};
    std::array<SteamId,max_players> members{};
    bool operator==(const Roster&) const = default;
};

inline bool make_roster(SteamId owner,const std::array<SteamId,max_players>& members,unsigned count,Roster& out){
    if(!user_id(owner)||!count||count>members.size())return false;
    auto sorted=members;
    std::sort(sorted.begin(),sorted.begin()+count);
    if(!std::binary_search(sorted.begin(),sorted.begin()+count,owner))return false;
    for(unsigned i=0;i<count;++i)if(!user_id(sorted[i])||(i&&sorted[i]==sorted[i-1]))return false;
    for(unsigned i=count;i<sorted.size();++i)sorted[i]=0;
    out={owner,count,sorted};
    return true;
}

struct Epoch {
    std::uint32_t serial{};
    SteamId nonce{};
    Roster locked{};
};
enum class EpochChange { invalid, stale, same, next };
inline EpochChange epoch_change(const Epoch& previous,const Epoch& current){
    if(!current.serial||!current.nonce)return EpochChange::invalid;
    if(current.serial<previous.serial)return EpochChange::stale;
    if(current.serial==previous.serial){
        if(current.nonce!=previous.nonce)return EpochChange::invalid;
        if(previous.locked.count){
            if(!current.locked.count)return EpochChange::stale;
            if(current.locked!=previous.locked)return EpochChange::invalid;
        }
        return EpochChange::same;
    }
    return current.nonce!=previous.nonce?EpochChange::next:EpochChange::invalid;
}

inline bool decode_epoch(std::string_view value,SteamId owner,Epoch& out){
    if(value.empty()||value.size()>128||!user_id(owner))return false;
    auto number=[](std::string_view text,auto& result){
        if(text.empty()||text.front()=='0')return false;
        const auto end=text.data()+text.size();
        const auto parsed=std::from_chars(text.data(),end,result);
        return parsed.ec==std::errc{}&&parsed.ptr==end&&result!=0;
    };
    Epoch epoch{};
    const auto serial=value.find('/');
    if(serial==std::string_view::npos||!number(value.substr(0,serial),epoch.serial))return false;
    value.remove_prefix(serial+1);
    const auto nonce=value.find('/');
    if(nonce==std::string_view::npos||!number(value.substr(0,nonce),epoch.nonce))return false;
    value.remove_prefix(nonce+1);
    if(!value.empty()){
        std::array<SteamId,max_players> members{};
        unsigned count=0;
        while(true){
            const auto comma=value.find(',');
            if(count>=members.size()||!number(value.substr(0,comma),members[count]))return false;
            ++count;
            if(comma==std::string_view::npos)break;
            value.remove_prefix(comma+1);
        }
        if(count<2||!make_roster(owner,members,count,epoch.locked)||epoch.locked.members!=members)
            return false;
    }
    out=epoch;
    return true;
}

inline std::string encode_epoch(const Epoch& epoch){
    auto value=std::to_string(epoch.serial)+"/"+std::to_string(epoch.nonce)+"/";
    for(unsigned i=0;i<epoch.locked.count;++i){
        if(i)value.push_back(',');
        value+=std::to_string(epoch.locked.members[i]);
    }
    return value;
}

enum class Recovery : std::uint64_t { none, waiting, clean, blocked };
class RecoveryGate {
    std::atomic<std::uint64_t> value_{};
    static constexpr auto word(std::uint32_t epoch,Recovery state){
        return (std::uint64_t{epoch}<<2)|static_cast<std::uint64_t>(state);
    }
public:
    // The worker publishes this only after it ceases all queue access.
    void request(std::uint32_t epoch){value_.store(word(epoch,Recovery::waiting),std::memory_order_release);}
    Recovery state(std::uint32_t epoch)const{
        const auto value=value_.load(std::memory_order_acquire);
        return value>>2==epoch?static_cast<Recovery>(value&3u):Recovery::none;
    }
    bool finish(std::uint32_t epoch,Recovery result){
        if(result!=Recovery::clean&&result!=Recovery::blocked)return false;
        auto expected=word(epoch,Recovery::waiting);
        return value_.compare_exchange_strong(expected,word(epoch,result));
    }
    void reset(){value_.store(0,std::memory_order_release);}
};

struct Peers {
    std::array<SteamId,max_players-1> users{};
    unsigned mask{};
    int find(SteamId user)const{
        if(!user)return -1;
        for(unsigned i=0;i<users.size();++i)if(users[i]==user)return static_cast<int>(i);
        return -1;
    }
};

inline bool map_peers(SteamId lobby,SteamId nonce,SteamId self,const Roster& roster,Peers& out){
    Roster checked{};
    if(!lobby_id(lobby)||!nonce||roster.count<2||
       !make_roster(roster.owner,roster.members,roster.count,checked)||checked!=roster||
       !std::binary_search(roster.members.begin(),roster.members.begin()+roster.count,self))return false;
    Session session({lobby,nonce},{0,roster.owner});
    for(unsigned i=0;i<roster.count;++i){
        const auto id=roster.members[i];
        if(id!=roster.owner&&session.admit_authenticated({0,id},protocol_version).result!=Result::ok)
            return false;
    }
    Peers result{};
    if(self==roster.owner){
        for(unsigned i=1;i<session.members().size();++i){
            result.users[i-1]=session.members()[i].ref.player.low;
            if(result.users[i-1])result.mask|=1u<<(i-1);
        }
    }else{
        result.users[0]=roster.owner;
        result.mask=1;
    }
    out=result;
    return true;
}

constexpr std::uint32_t hello_magic=0x34534451; // QDS4
enum class Control : std::uint32_t { hello=1,ack=2 };
struct Handshake {
    unsigned hello{},ack{};
    void observe(unsigned peer,Control value){
        if(peer>=max_players-1)return;
        if(value==Control::hello)hello|=1u<<peer;
        else if(value==Control::ack)ack|=1u<<peer;
    }
    unsigned connected()const{return hello&ack;}
};
inline enemy::Packet control(SteamId lobby,SteamId nonce,Control type){
    using enemy::animation::wire::put;
    enemy::Packet packet{};
    packet.size=32;
    put(packet,0,hello_magic);
    put(packet,4,protocol_version);
    put(packet,8,lobby);
    put(packet,16,nonce);
    put(packet,24,static_cast<std::uint32_t>(type));
    put(packet,28,std::uint32_t{0});
    return packet;
}
inline bool decode_control(const enemy::Packet& packet,SteamId lobby,SteamId nonce,Control& out){
    using enemy::animation::wire::get;
    if(packet.size!=32||get<std::uint32_t>(packet,0)!=hello_magic||
       get<std::uint32_t>(packet,4)!=protocol_version||get<SteamId>(packet,8)!=lobby||
       get<SteamId>(packet,16)!=nonce||get<std::uint32_t>(packet,28))return false;
    const auto type=get<std::uint32_t>(packet,24);
    if(type!=static_cast<unsigned>(Control::hello)&&type!=static_cast<unsigned>(Control::ack))return false;
    out=static_cast<Control>(type);
    return true;
}
inline bool match_datagram(const enemy::Packet& packet,SteamId lobby,SteamId nonce){
    using enemy::animation::wire::get;
    return packet.size>=40&&packet.size<=packet.data.size()&&
        get<std::uint32_t>(packet,0)==0x45344e51&&get<std::uint32_t>(packet,4)==1&&
        get<SteamId>(packet,8)==lobby&&get<SteamId>(packet,16)==nonce&&
        get<std::uint32_t>(packet,36)==packet.size-40;
}
}
