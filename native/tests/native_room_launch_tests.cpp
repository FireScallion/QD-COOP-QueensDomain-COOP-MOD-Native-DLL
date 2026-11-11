#include "qdcoop/native_room_launch.hpp"
#include "qdcoop/lan_room.hpp"
#include "qdcoop/player_skin.hpp"
#include "qdcoop/steam_social.hpp"
#include <array>
#include <cstdio>
#include <string>

namespace Room=qdcoop::native_room;
using Room::Operation;
namespace Social=qdcoop::steam_social;
namespace Lan=qdcoop::lan_room;

static bool check_lan_controls(){
    Lan::Control hello{Lan::Op::hello,0,Lan::unassigned,0,{1,2},{},{}};
    Lan::E::Packet packet{};
    Lan::Control received;
    if(!Lan::encode(hello,packet)||!Lan::decode(packet,received)||received!=hello)return false;
    hello.room={3,4};
    if(Lan::encode(hello,packet))return false;
    Lan::Control admitted{Lan::Op::welcome,7,2,4,{1,2},{3,4},{5,6}};
    if(!Lan::encode(admitted,packet)||!Lan::decode(packet,received)||received!=admitted)return false;
    auto invalid=packet;
    invalid.data[4]^=1;
    if(Lan::decode(invalid,received)||received!=admitted)return false;
    --invalid.size;
    if(Lan::decode(invalid,received))return false;
    admitted.op=Lan::Op::restart;
    if(!Lan::encode(admitted,packet)||!Lan::decode(packet,received)||received!=admitted)return false;
    auto unscoped=admitted;
    unscoped.room={};
    if(Lan::encode(unscoped,packet))return false;
    admitted.slot=3;
    return !Lan::encode(admitted,packet);
}

bool check_invitation_callbacks() {
    constexpr Social::SteamId lobby=108086391056891905ull,friend_id=76561197960265729ull;
    Social::LobbyJoin lobby_join{lobby,friend_id};
    const auto native=Social::decode_invitation(Social::lobby_callback,&lobby_join);
    Social::PresenceJoin rich{friend_id};
    std::strcpy(rich.connect,"+qdcoop_lobby 108086391056891905");
    const auto presence=Social::decode_invitation(Social::presence_callback,&rich);
    if(native.lobby!=lobby||native.friend_id!=friend_id||native.source!=Social::Source::lobby||
       presence.lobby!=lobby||presence.friend_id!=friend_id||presence.source!=Social::Source::presence||
       Social::parse_connect("+connect_lobby 108086391056891905")!=lobby||
       Social::parse_connect("+qdcoop_lobby 108086391056891905 -ExecCmds=quit")||
       Social::parse_connect("+qdcoop_lobby 18446744073709551616")||
       Social::parse_connect("+qdcoop_lobby 0108086391056891905")||
       Social::parse_connect("+qdcoop_lobby 76561197960265729"))return false;
    std::memset(rich.connect,'9',sizeof(rich.connect));
    return !Social::decode_invitation(Social::presence_callback,&rich).lobby&&
        !Social::decode_invitation(0,&lobby_join).lobby&&
        !Social::decode_invitation(Social::lobby_callback,nullptr).lobby;
}

template<class... Text>
bool parse(Room::Request& request,Text... text) {
    std::array<std::wstring,sizeof...(text)> storage{std::wstring(text)...};
    std::array<wchar_t*,sizeof...(text)> arguments{};
    for(std::size_t i=0;i<storage.size();++i)arguments[i]=storage[i].data();
    return Room::parse(arguments,request);
}

int main() {
    if(!check_invitation_callbacks())return 1;
    std::vector<Social::Friend> friends{{1,0,L"Offline",false,false,false},{2,0,L"Online",false,true,false},
        {3,0,L"Playing",true,true,false},{4,108086391056891905ull,L"Joinable",true,true,false}};
    Social::sort_friends(friends);
    if(friends[0].id!=4||friends[1].id!=3||friends[2].id!=2||friends[3].id!=1)return 1;
    Room::Request request{};
    if(!parse(request,L"-NullRHI")||request.operation!=Operation::none||
       !parse(request,L"-QDCoopHost=4",L"-QDCoopFriends")||!Room::valid(request)||
       request.operation!=Operation::host||request.seats!=4||request.friends!=1||
       !parse(request,L"-QDCoopHost=2")||request.operation!=Operation::host||
       request.seats!=2||request.friends!=0||
       !parse(request,L"+connect_lobby",L"108086391056891905")||!Room::valid(request)||
       request.operation!=Operation::join||request.lobby!=108086391056891905ull||
       !parse(request,L"+qdcoop_lobby",L"108086391056891905")||!Room::valid(request)||
       request.operation!=Operation::join||
       parse(request,L"-QDCoopHost=1")||parse(request,L"-QDCoopHost=5")||
       parse(request,L"-QDCoopHost=04")||parse(request,L"-QDCoopFriends")||
       parse(request,L"-QDCoopHost=3",L"-QDCoopFriends",L"-QDCoopFriends")||
       parse(request,L"-QDCoopHost=4",L"+connect_lobby",L"108086391056891905")||
       parse(request,L"+connect_lobby")||parse(request,L"+connect_lobby",L"76561197960265729")||
       parse(request,L"+connect_lobby",L"18446744073709551616")||
       parse(request,L"+connect_lobby",L"108086391056891905 ",L"-NullRHI")||
       parse(request,L"+connect_lobby",L"0108086391056891905")||
       parse(request,L"+connect_lobby",L"108086391056891905",L"-QDCoopFriends"))return 1;
    if(!parse(request,L"-QDCoopLanHost=4")||!Room::valid(request)||request.operation!=Operation::host_lan||
       request.seats!=4||request.port!=47771||
       !parse(request,L"-QDCoopLanJoin=192.168.1.20:48931")||!Room::valid(request)||request.operation!=Operation::join_lan||
       Room::ipv4_view(request.ipv4)!=L"192.168.1.20"||request.port!=48931||
       parse(request,L"-QDCoopLanJoin=192.168.001.20")||parse(request,L"-QDCoopLanJoin=239.0.0.1")||
       parse(request,L"-QDCoopLanJoin=192.168.0.256")||parse(request,L"-QDCoopLanJoin=192.168.1.20:0")||
       parse(request,L"-QDCoopLanHost=4:65536")||parse(request,L"-QDCoopLanHost=4",L"-QDCoopFriends")||
       parse(request,L"-QDCoopLanHost=4",L"+connect_lobby",L"108086391056891905"))return 1;
    wchar_t arjaot[]=L"-QDCoopSkin=arjaot",imba[]=L"-QDCoopSkin=imba",bad[]=L"-QDCoopSkin=invalid";
    unsigned skin{};
    const std::array<wchar_t*,1> single{arjaot},invalid{bad};
    const std::array<wchar_t*,2> duplicate{arjaot,imba};
    if(!qdcoop::player::parse_skin({},skin)||skin!=1||
       !qdcoop::player::parse_skin(single,skin)||skin!=2||
       qdcoop::player::parse_skin(invalid,skin)||qdcoop::player::parse_skin(duplicate,skin))return 1;
    if(!check_lan_controls())return 1;
    std::puts("PASS native room launch, invite aliases, skin selection and LAN controls");
}
