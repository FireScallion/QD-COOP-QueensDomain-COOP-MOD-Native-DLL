#pragma once
#include "steam_room.hpp"
#include <array>
#include <span>
#include <string_view>

namespace qdcoop::native_room {
enum class Operation : std::uint32_t { none, host, join, leave, host_lan, join_lan };
enum class Result : std::uint32_t { idle, preparing, running, stopping, closed, rejected };
enum class Failure : std::uint32_t { none, arguments, legacy_loader, game_build, core, metadata, start, world, save_backup };

struct Request {
    Operation operation{};
    unsigned seats{};
    unsigned friends{};
    steam_room::SteamId lobby{};
    unsigned port{};
    std::array<wchar_t,16> ipv4{};
};

struct Status {
    Result result{};
    Failure failure{};
    unsigned game_thread{}, metadata_found{};
    steam_room::Status room{};
};
static_assert(sizeof(Status)==80);

inline constexpr unsigned default_lan_port=47771;

constexpr bool is_lan(Operation operation){return operation==Operation::host_lan||operation==Operation::join_lan;}
constexpr bool valid_lan_port(unsigned port){return port>=1024&&port<=65535;}

constexpr bool parse_lan_port(std::wstring_view text,unsigned& port){
    if(text.empty()||text.size()>5||text.front()==L'0')return false;
    unsigned value{};
    for(const auto digit:text){
        if(digit<L'0'||digit>L'9')return false;
        value=10*value+static_cast<unsigned>(digit-L'0');
    }
    if(!valid_lan_port(value))return false;
    port=value;
    return true;
}

constexpr bool valid_ipv4(std::wstring_view text){
    if(text.size()<7||text.size()>15)return false;
    for(unsigned octet=0;octet<4;++octet){
        const auto dot=text.find(L'.');
        if((dot==text.npos)!=(octet==3))return false;
        const auto part=text.substr(0,dot);
        if(part.empty()||part.size()>3||(part.size()>1&&part.front()==L'0'))return false;
        unsigned value{};
        for(const auto digit:part){
            if(digit<L'0'||digit>L'9')return false;
            value=10*value+static_cast<unsigned>(digit-L'0');
        }
        if(value>255||(octet==0&&(!value||value>=224)))return false;
        if(dot!=text.npos)text.remove_prefix(dot+1);
    }
    return true;
}

constexpr std::wstring_view ipv4_view(const std::array<wchar_t,16>& address){
    for(unsigned i=0;i<address.size();++i)if(!address[i])return {address.data(),i};
    return {};
}
constexpr bool set_ipv4(std::wstring_view text,std::array<wchar_t,16>& address){
    if(!valid_ipv4(text))return false;
    address={};
    for(unsigned i=0;i<text.size();++i)address[i]=text[i];
    return true;
}

constexpr bool valid(const Request& request) {
    const bool empty_lan=!request.port&&!request.ipv4[0];
    switch(request.operation) {
    case Operation::host:
        return request.seats>=2&&request.seats<=max_players&&request.friends<=1&&!request.lobby&&empty_lan;
    case Operation::join:
        return !request.seats&&!request.friends&&steam_room::lobby_id(request.lobby)&&empty_lan;
    case Operation::leave:
        return !request.seats&&!request.friends&&!request.lobby&&empty_lan;
    case Operation::host_lan:
        return request.seats>=2&&request.seats<=max_players&&!request.friends&&!request.lobby&&
            valid_lan_port(request.port)&&!request.ipv4[0];
    case Operation::join_lan:
        return !request.seats&&!request.friends&&!request.lobby&&valid_lan_port(request.port)&&valid_ipv4(ipv4_view(request.ipv4));
    default:
        return false;
    }
}

constexpr bool parse_lan(std::wstring_view text,Operation operation,Request& request){
    const auto separator=text.find(L':');
    const auto value=text.substr(0,separator);
    Request candidate{operation};
    candidate.port=default_lan_port;
    if(separator!=text.npos&&!parse_lan_port(text.substr(separator+1),candidate.port))return false;
    if(operation==Operation::host_lan){
        if(value.size()!=1||value.front()<L'2'||value.front()>L'4')return false;
        candidate.seats=static_cast<unsigned>(value.front()-L'0');
    }else if(!set_ipv4(value,candidate.ipv4))return false;
    request=candidate;
    return true;
}

constexpr bool parse_lobby(std::wstring_view text,steam_room::SteamId& lobby) {
    if(text.empty()||text.size()>20||text.front()==L'0')return false;
    steam_room::SteamId result{};
    for(wchar_t digit:text) {
        if(digit<L'0'||digit>L'9')return false;
        const auto value=static_cast<unsigned>(digit-L'0');
        if(result>(UINT64_MAX-value)/10)return false;
        result=result*10+value;
    }
    lobby=result;
    return steam_room::lobby_id(lobby);
}

inline bool parse(std::span<wchar_t* const> arguments,Request& out) {
    constexpr std::wstring_view lan_host=L"-QDCoopLanHost=",lan_join=L"-QDCoopLanJoin=";
    Request request{};
    bool friends=false;
    for(std::size_t i=0;i<arguments.size();++i) {
        const std::wstring_view arg=arguments[i];
        if(arg==L"-QDCoopFriends") {
            if(friends)return false;
            friends=true;
        } else if(arg.starts_with(L"-QDCoopHost=")) {
            if(request.operation!=Operation::none||arg.size()!=13||arg.back()<L'2'||arg.back()>L'4')
                return false;
            request={Operation::host,static_cast<unsigned>(arg.back()-L'0')};
        } else if(arg.starts_with(lan_host)) {
            if(request.operation!=Operation::none||!parse_lan(arg.substr(lan_host.size()),Operation::host_lan,request))return false;
        } else if(arg.starts_with(lan_join)) {
            if(request.operation!=Operation::none||!parse_lan(arg.substr(lan_join.size()),Operation::join_lan,request))return false;
        } else if(arg==L"+connect_lobby"||arg==L"+qdcoop_lobby") {
            if(request.operation!=Operation::none||++i==arguments.size()||
               !parse_lobby(arguments[i],request.lobby))return false;
            request.operation=Operation::join;
        }
    }
    if(friends&&request.operation!=Operation::host)return false;
    request.friends=friends;
    out=request;
    return true;
}
}
