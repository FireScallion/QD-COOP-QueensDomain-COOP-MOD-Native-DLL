#pragma once
#include "steam_room.hpp"
#include <algorithm>
#include <charconv>
#include <cstring>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace qdcoop::steam_social {
using steam_room::SteamId;
enum class Source : std::uint32_t { none, lobby, presence };
enum class Action : std::uint32_t { none, queued, opened, unavailable };

struct Invitation {
    SteamId lobby{},friend_id{};
    std::uint64_t ticket{};
    Source source{};
    std::uint32_t reserved{};
};
struct Status {
    unsigned ready{},callbacks{},overlay{};
    Action action{};
    SteamId published_lobby{};
};
static_assert(sizeof(Invitation)==32&&sizeof(Status)==24);

constexpr int lobby_callback=333,presence_callback=337;
struct LobbyJoin { SteamId lobby{},friend_id{}; };
struct PresenceJoin { SteamId friend_id{}; char connect[256]{}; };
static_assert(sizeof(LobbyJoin)==16&&sizeof(PresenceJoin)==264);

inline constexpr std::string_view connect_prefix="+qdcoop_lobby ";

struct Friend {
    SteamId id{},lobby{};
    std::wstring name;
    bool playing{},online{},in_room{};
    bool operator==(const Friend&) const=default;
};

inline void sort_friends(std::vector<Friend>& friends){
    std::ranges::sort(friends,[](const Friend& a,const Friend& b){
        if(a.playing!=b.playing)return a.playing;
        if(bool(a.lobby)!=bool(b.lobby))return bool(a.lobby);
        if(a.online!=b.online)return a.online;
        if(a.name!=b.name)return a.name<b.name;
        return a.id<b.id;
    });
}

inline SteamId parse_connect(std::string_view value) {
    if(value.starts_with(connect_prefix))value.remove_prefix(connect_prefix.size());
    else if(value.starts_with("+connect_lobby "))value.remove_prefix(15);
    else return 0;
    SteamId lobby{};
    if(value.empty()||value.size()>20||value.front()=='0')return 0;
    const auto end=value.data()+value.size();
    const auto parsed=std::from_chars(value.data(),end,lobby);
    return parsed.ec==std::errc{}&&parsed.ptr==end&&steam_room::lobby_id(lobby)?lobby:0;
}

inline Invitation decode_invitation(int callback,const void* data) {
    if(!data)return {};
    SteamId lobby{},friend_id{};
    Source source{};
    if(callback==lobby_callback) {
        LobbyJoin request{};
        std::memcpy(&request,data,sizeof(request));
        lobby=request.lobby;
        friend_id=request.friend_id;
        source=Source::lobby;
    } else if(callback==presence_callback) {
        PresenceJoin request{};
        std::memcpy(&request,data,sizeof(request));
        const auto end=std::find(std::begin(request.connect),std::end(request.connect),'\0');
        if(end==std::end(request.connect))return {};
        lobby=parse_connect({request.connect,static_cast<std::size_t>(end-request.connect)});
        friend_id=request.friend_id;
        source=Source::presence;
    }
    if(!steam_room::lobby_id(lobby))return {};
    return {lobby,steam_room::user_id(friend_id)?friend_id:0,0,source};
}
}
