#pragma once
#include "qdcoop/world_progress.hpp"
#include <charconv>

namespace qdcoop::placed_item {
inline constexpr std::string_view drop_prefix="QDCoopDrop~";
inline constexpr std::string_view winner_prefix="QDCoopWinner~";
inline constexpr std::string_view ticket_prefix="QDCoopNativeClaim~";
inline constexpr std::string_view room_prefix="QDCoopRoom~";
inline constexpr std::string_view escrow_prefix="QDCoopPersonal~place~";
inline constexpr std::string_view identity_prefix="QDCoopPersonal~id~";
inline constexpr unsigned max_ground_items=64,max_item_id=80;

enum class Custody : std::int32_t { ground,pending,claimed };

struct Drop {
    std::string id,owner,room,item,map;
    std::array<std::int32_t,3> position{};
    std::int32_t yaw{};
    bool operator==(const Drop&) const=default;
};

inline constexpr bool ascii_alnum(char c){
    return (c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z');
}

inline bool valid_id(std::string_view value){
    return !value.empty()&&value.size()<=max_item_id&&std::ranges::all_of(value,[](char c){
        return ascii_alnum(c)||c=='_'||c=='-';
    });
}

inline bool prefix(std::string_view value,std::string_view start){
    return world_progress::name_equal(value.substr(0,start.size()),start);
}

inline bool owner_matches(std::string_view owner,std::string_view native_id,std::string_view legacy_id){
    return (!native_id.empty()&&world_progress::name_equal(owner,native_id))||
        (!legacy_id.empty()&&world_progress::name_equal(owner,legacy_id));
}

inline bool path(std::string_view value,std::string_view root,unsigned maximum){
    return value.size()>root.size()&&value.size()<=maximum&&prefix(value,root)&&
        value.find("..")==std::string_view::npos&&value.find("//")==std::string_view::npos&&
        std::ranges::all_of(value,[](char c){return ascii_alnum(c)||c=='_'||c=='/'||c=='.'||c=='-';});
}

inline bool valid(const Drop& value){
    const auto coordinate=[](std::int32_t v){return v>=-10000000&&v<=10000000;};
    return valid_id(value.id)&&valid_id(value.owner)&&valid_id(value.room)&&
        path(value.item,"/Game/Items/",250)&&path(value.map,"/Game/",200)&&
        std::ranges::all_of(value.position,coordinate)&&coordinate(value.yaw);
}

inline bool decode(std::string_view text,Drop& out){
    std::array<std::string_view,9> fields{};
    for(unsigned i=0;i<fields.size();++i){
        const auto separator=text.find('~');
        if((i+1<fields.size())!=(separator!=std::string_view::npos))return false;
        fields[i]=text.substr(0,separator);
        if(separator!=std::string_view::npos)text.remove_prefix(separator+1);
    }
    Drop value{std::string(fields[0]),std::string(fields[1]),std::string(fields[2]),
        std::string(fields[3]),std::string(fields[4])};
    const std::array coordinates{&value.position[0],&value.position[1],&value.position[2],&value.yaw};
    for(unsigned i=0;i<coordinates.size();++i){
        const auto field=fields[i+5];
        const auto end=field.data()+field.size();
        const auto parsed=std::from_chars(field.data(),end,*coordinates[i]);
        if(parsed.ec!=std::errc{}||parsed.ptr!=end)return false;
    }
    if(!valid(value))return false;
    out=std::move(value);
    return true;
}

inline std::string encode(const Drop& drop){
    if(!valid(drop))return {};
    std::string text=drop.id+"~"+drop.owner+"~"+drop.room+"~"+drop.item+"~"+drop.map;
    for(const auto coordinate:drop.position)text+="~"+std::to_string(coordinate);
    return text+"~"+std::to_string(drop.yaw);
}

struct Transfer {
    Drop drop;
    std::string save_key,winner;
    std::uint64_t ticket{};
    Custody state{};
    bool operator==(const Transfer&) const=default;
};

inline bool decode_ticket(std::string_view text,std::uint64_t& ticket){
    if(text.empty()||text.front()=='0')return false;
    std::uint64_t value{};
    const auto end=text.data()+text.size();
    const auto result=std::from_chars(text.data(),end,value);
    if(result.ec!=std::errc{}||result.ptr!=end||!value)return false;
    ticket=value;
    return true;
}

template<class Parse>
inline bool transfer_marker(const std::vector<world_progress::NamedState>& names,std::string_view stem,Parse parse){
    auto it=std::ranges::lower_bound(names,stem,world_progress::name_less,&world_progress::NamedState::key);
    bool found{};
    for(;it!=names.end()&&prefix(it->key,stem);++it){
        if(it->value!=1)continue;
        if(found||!parse(std::string_view(it->key).substr(stem.size())))return false;
        found=true;
    }
    return true;
}

inline std::vector<Transfer> transfers(const std::vector<world_progress::NamedState>& names){
    // World snapshots validate and sort names with name_less before this read.
    std::vector<Transfer> result;
    for(const auto& entry:names){
        if(result.size()==max_ground_items)break;
        if(entry.value<0||entry.value>=static_cast<int>(Custody::claimed)||!prefix(entry.key,drop_prefix))continue;
        Drop drop;
        if(!decode(std::string_view(entry.key).substr(drop_prefix.size()),drop))continue;
        const auto stem=std::string(drop_prefix)+drop.id+"~";
        auto other=std::ranges::lower_bound(names,stem,world_progress::name_less,&world_progress::NamedState::key);
        for(;other!=names.end()&&prefix(other->key,stem);++other){
            Drop duplicate;
            if(&*other!=&entry&&other->value>=0&&other->value<=static_cast<int>(Custody::claimed)&&
               decode(std::string_view(other->key).substr(drop_prefix.size()),duplicate))break;
        }
        if(other!=names.end()&&prefix(other->key,stem))continue;
        Transfer value{std::move(drop),entry.key,{},{},static_cast<Custody>(entry.value)};
        const auto winner=std::string(winner_prefix)+value.drop.id+"~";
        const auto ticket=std::string(ticket_prefix)+value.drop.id+"~";
        if(!transfer_marker(names,winner,[&](std::string_view text){
                if(!valid_id(text))return false;
                value.winner=text;
                return true;
            })||!transfer_marker(names,ticket,[&](std::string_view text){return decode_ticket(text,value.ticket);}))continue;
        result.push_back(std::move(value));
    }
    return result;
}
}
