#pragma once
#include "qdcoop/enemy_enrollment.hpp"
#include <algorithm>
#include <bit>
#include <string>
#include <string_view>
#include <vector>

namespace qdcoop::enemy::damage {
struct TagValue {
    std::string name;
    float value{};
    bool operator==(const TagValue&) const = default;
};
struct Value {
    float amount{};
    std::array<std::uint32_t,4> source{};
    std::vector<std::string> tags;
    std::vector<TagValue> data;
    bool operator==(const Value&) const = default;
};
using Bytes=std::vector<std::uint8_t>;
inline constexpr unsigned hit_kind=19,ack_kind=20,hit_header=96,ack_size=64;
inline constexpr unsigned tag_limit=40,name_limit=160,max_payload=5000,part_size=1100;
inline constexpr unsigned max_parts=(max_payload+part_size-1)/part_size;
inline bool tag_name(std::string_view name){
    constexpr std::string_view prefix="Damage.";
    return name.starts_with(prefix)&&name.size()>prefix.size()&&name.size()<=name_limit&&
           std::ranges::all_of(name.substr(prefix.size()),[](char c){
               return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.';
           });
}
inline constexpr char tag_fold(char value){return value>='A'&&value<='Z'?static_cast<char>(value+('a'-'A')):value;}
inline constexpr bool tag_less(std::string_view a,std::string_view b){
    return std::lexicographical_compare(a.begin(),a.end(),b.begin(),b.end(),[](char x,char y){
        return tag_fold(x)<tag_fold(y);
    });
}
inline bool valid(const Value& value){
    if(!std::isfinite(value.amount)||value.amount<=0.f||value.amount>10000.f||
       value.tags.size()>tag_limit||value.data.size()>tag_limit)return false;
    for(unsigned i=0;i<value.tags.size();++i)
        if(!tag_name(value.tags[i])||(i&&!tag_less(value.tags[i-1],value.tags[i])))return false;
    for(unsigned i=0;i<value.data.size();++i){
        const auto& tag=value.data[i];
        if(!tag_name(tag.name)||!std::isfinite(tag.value)||std::abs(tag.value)>100000.f||
           (i&&!tag_less(value.data[i-1].name,tag.name)))return false;
    }
    return true;
}
inline void append32(Bytes& bytes,std::uint32_t value){
    for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));
}
inline bool read32(std::span<const std::uint8_t>& bytes,std::uint32_t& value){
    if(bytes.size()<4)return false;
    value=0;
    for(unsigned i=0;i<4;++i)value|=std::uint32_t(bytes[i])<<(8*i);
    bytes=bytes.subspan(4);
    return true;
}
inline void append_tag(Bytes& bytes,std::string_view name){
    bytes.push_back(static_cast<std::uint8_t>(name.size()));
    bytes.insert(bytes.end(),name.begin(),name.end());
}
inline bool read_tag(std::span<const std::uint8_t>& bytes,std::string& name){
    if(bytes.empty()||!bytes[0]||bytes[0]>name_limit||bytes.size()<1u+bytes[0])return false;
    const auto length=bytes[0];
    name.assign(reinterpret_cast<const char*>(bytes.data()+1),length);
    bytes=bytes.subspan(1+length);
    return true;
}
inline bool encode(const Value& value,Bytes& out){
    if(!valid(value))return false;
    Bytes bytes;
    bytes.reserve(22+value.tags.size()*32+value.data.size()*40);
    append32(bytes,std::bit_cast<std::uint32_t>(value.amount));
    for(const auto word:value.source)append32(bytes,word);
    bytes.push_back(static_cast<std::uint8_t>(value.tags.size()));
    bytes.push_back(static_cast<std::uint8_t>(value.data.size()));
    for(const auto& tag:value.tags)append_tag(bytes,tag);
    for(const auto& tag:value.data){append_tag(bytes,tag.name);append32(bytes,std::bit_cast<std::uint32_t>(tag.value));}
    if(bytes.size()>max_payload)return false;
    out=std::move(bytes);
    return true;
}
inline bool decode(std::span<const std::uint8_t> bytes,Value& out){
    if(bytes.size()<22||bytes.size()>max_payload)return false;
    Value result{};
    std::uint32_t bits{};
    if(!read32(bytes,bits))return false;
    result.amount=std::bit_cast<float>(bits);
    for(auto& word:result.source)if(!read32(bytes,word))return false;
    const auto tags=bytes[0],data=bytes[1];
    if(tags>tag_limit||data>tag_limit)return false;
    bytes=bytes.subspan(2);
    result.tags.resize(tags);
    result.data.resize(data);
    for(auto& tag:result.tags)if(!read_tag(bytes,tag))return false;
    for(auto& tag:result.data){
        if(!read_tag(bytes,tag.name)||!read32(bytes,bits))return false;
        tag.value=std::bit_cast<float>(bits);
    }
    if(!bytes.empty()||!valid(result))return false;
    out=std::move(result);
    return true;
}
inline std::uint64_t digest(std::span<const std::uint8_t> bytes){
    std::uint64_t hash=14695981039346656037ull;
    for(const auto byte:bytes)hash=(hash^byte)*1099511628211ull;
    return hash?hash:1;
}
struct Key {
    std::uint64_t serial{};
    std::uint32_t enemy{},generation{},source_generation{};
    bool operator==(const Key&) const = default;
};
inline bool valid(const Key& key){return key.serial&&key.enemy&&key.enemy<=capacity&&key.generation&&key.source_generation;}
struct Header {
    Key key;
    std::array<float,3> point{};
    std::uint64_t hash{};
    std::uint32_t bytes{};
    bool point_valid{};
    bool operator==(const Header&) const = default;
};
struct Part {Header header;std::uint32_t index{};std::span<const std::uint8_t> bytes;};
inline unsigned count(unsigned bytes){return (bytes+part_size-1)/part_size;}
inline bool valid(const Part& part){
    const auto& header=part.header;
    return valid(header.key)&&header.hash&&header.bytes>=22&&header.bytes<=max_payload&&
           std::ranges::all_of(header.point,[](float n){return std::isfinite(n)&&std::abs(n)<=10000000.f;})&&
           part.index<count(header.bytes)&&part.bytes.size()==std::min(part_size,header.bytes-part.index*part_size);
}
inline void write_key(Packet& packet,const Key& key){
    using animation::wire::put;
    put(packet,40,key.serial);
    put(packet,48,key.enemy);
    put(packet,52,key.generation);
    put(packet,56,key.source_generation);
}
inline Key read_key(const Packet& packet){
    using animation::wire::get;
    return {get<std::uint64_t>(packet,40),get<std::uint32_t>(packet,48),get<std::uint32_t>(packet,52),get<std::uint32_t>(packet,56)};
}
inline bool encode(const Envelope& scope,const Part& part,Packet& packet){
    if(!valid(part))return false;
    using animation::wire::put;
    enrollment::write_header(scope,hit_kind,hit_header+static_cast<unsigned>(part.bytes.size()),packet);
    const auto& h=part.header;
    write_key(packet,h.key);
    put(packet,60,static_cast<std::uint32_t>(h.point_valid));
    put(packet,64,h.bytes);
    put(packet,68,part.index);
    put(packet,72,h.hash);
    std::memcpy(packet.data.data()+80,h.point.data(),sizeof(h.point));
    std::memcpy(packet.data.data()+hit_header,part.bytes.data(),part.bytes.size());
    return true;
}
inline bool decode(const Envelope& scope,const Packet& packet,Part& part){
    if(packet.size<=hit_header||packet.size>hit_header+part_size||
       !enrollment::check_header(scope,packet,hit_kind,packet.size))return false;
    using animation::wire::get;
    const auto flag=get<std::uint32_t>(packet,60);
    if(flag>1||get<std::uint32_t>(packet,92))return false;
    Part value{{read_key(packet),{},get<std::uint64_t>(packet,72),get<std::uint32_t>(packet,64),flag!=0},
               get<std::uint32_t>(packet,68),{packet.data.data()+hit_header,packet.size-hit_header}};
    std::memcpy(value.header.point.data(),packet.data.data()+80,sizeof(value.header.point));
    if(!valid(value))return false;
    part=value;
    return true;
}
inline bool encode_ack(const Envelope& scope,const Key& key,Packet& packet){
    if(!valid(key))return false;
    enrollment::write_header(scope,ack_kind,ack_size,packet);
    write_key(packet,key);
    return true;
}
inline bool decode_ack(const Envelope& scope,const Packet& packet,Key& key){
    if(!enrollment::check_header(scope,packet,ack_kind,ack_size)||animation::wire::get<std::uint32_t>(packet,60))return false;
    const auto result=read_key(packet);
    if(!valid(result))return false;
    key=result;
    return true;
}
struct Assembly {
    Header header;
    std::array<std::uint8_t,max_payload> bytes{};
    unsigned mask{};
    bool push(const Part& part){
        if(!valid(part)||part.header!=header)return false;
        auto at=bytes.data()+part.index*part_size;
        const auto flag=1u<<part.index;
        if(mask&flag)return std::memcmp(at,part.bytes.data(),part.bytes.size())==0;
        std::memcpy(at,part.bytes.data(),part.bytes.size());
        mask|=flag;
        return true;
    }
    bool complete() const {return header.bytes>=22&&header.bytes<=max_payload&&mask==((1u<<count(header.bytes))-1u);}
    bool value(Value& out) const {
        if(!complete())return false;
        const auto data=std::span(bytes).first(header.bytes);
        return digest(data)==header.hash&&decode(data,out);
    }
};
struct ReplayWindow {
    std::uint64_t highest{},seen{};
    bool known(std::uint64_t serial) const {
        if(!serial||serial>highest)return false;
        const auto distance=highest-serial;
        return distance>=64||(seen&(1ull<<distance));
    }
    void accept(std::uint64_t serial){
        if(!serial||known(serial))return;
        if(serial>highest){
            const auto distance=serial-highest;
            seen=distance>=64?0:seen<<distance;
            highest=serial;
        }
        seen|=1ull<<(highest-serial);
    }
};
}
