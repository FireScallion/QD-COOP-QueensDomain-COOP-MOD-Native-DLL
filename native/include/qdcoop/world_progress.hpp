#pragma once
#include "qdcoop/enemy_enrollment.hpp"
#include <algorithm>
#include <bit>
#include <bitset>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace qdcoop::world_progress {
using Guid=std::array<std::int32_t,4>;
struct ActorState { Guid key{}; std::int32_t value{}; bool operator==(const ActorState&) const = default; };
inline bool actor_state_rolled_back(const std::vector<ActorState>& local,const std::vector<ActorState>& host){
    return std::ranges::any_of(local,[&](const auto& state){
        if(state.value<=0)return false;
        const auto wanted=std::ranges::lower_bound(host,state.key,{},&ActorState::key);
        return wanted==host.end()||wanted->key!=state.key||wanted->value<state.value;
    });
}
struct ActorItemState { Guid key{}; std::string path; bool operator==(const ActorItemState&) const = default; };
struct NamedState { std::string key; std::int32_t value{}; bool operator==(const NamedState&) const = default; };
enum class BarrierKind : std::uint32_t {necro=1,desert,library,aqua_exit,graveyard,aqua_entry,bridge,castle};
inline constexpr std::array<float,10> identity_transform{0,0,0,1,0,0,0,1,1,1};
struct BarrierState {
    BarrierKind kind{};
    std::string actor,component;
    std::array<float,10> transform=identity_transform;
    unsigned collision{};
    bool hidden{},visible{true};
    bool operator==(const BarrierState&) const = default;
};
enum class MechanismKind : std::uint32_t { door=1,lever,secret,dagger_button,gargoyle };
struct MechanismState {
    Guid key{};
    MechanismKind kind{};
    std::uint32_t activations{};
    bool active{};
    bool operator==(const MechanismState&) const = default;
};
struct StreamEntry {std::string path;std::int32_t priority{};bool operator==(const StreamEntry&) const = default;};
struct Streaming {
    bool known{};
    std::array<std::vector<StreamEntry>,2> stacks;
    bool operator==(const Streaming&) const = default;
};
struct Arrival {
    std::uint64_t serial{};
    std::array<float,3> position{};
    float yaw{};
    bool operator==(const Arrival&) const = default;
};
inline constexpr unsigned max_stream_entries=64,max_stream_path=256,snapshot_prefix=60;
inline constexpr unsigned max_barriers=512,max_barrier_actor=450,max_barrier_component=120;
inline bool valid(const Arrival& arrival){
    if(!arrival.serial)return arrival==Arrival{};
    return std::isfinite(arrival.yaw)&&std::abs(arrival.yaw)<=180.f&&
           std::ranges::all_of(arrival.position,[](float value){return std::isfinite(value)&&std::abs(value)<=10000000.f;});
}
inline bool stream_path(std::string_view path){
    if(path.size()>max_stream_path||!path.starts_with("/Game/"))return false;
    const auto dot=path.find('.'),slash=path.rfind('/');
    if(dot==path.npos||dot<=slash+1||path.find('.',dot+1)!=path.npos||path.substr(slash+1,dot-slash-1)!=path.substr(dot+1))return false;
    const auto package=path.substr(6,dot-6);
    if(package.empty()||package.front()=='/'||package.find("//")!=package.npos)return false;
    return std::ranges::all_of(package,[](char c){
        return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='/';
    });
}
inline bool music_path(std::string_view path){
    return path.starts_with("/Game/Music/")&&stream_path(path);
}
inline bool barrier_less(const BarrierState& left,const BarrierState& right){
    if(left.kind!=right.kind)return left.kind<right.kind;
    return left.actor==right.actor?left.component<right.component:left.actor<right.actor;
}
inline bool valid(const BarrierState& state){
    constexpr std::string_view separator=".PersistentLevel.";
    const auto split=state.actor.find(separator);
    const auto symbol=[](char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';};
    if(state.kind<BarrierKind::necro||state.kind>BarrierKind::castle||state.actor.size()>max_barrier_actor||
       split==state.actor.npos||!stream_path(std::string_view(state.actor).substr(0,split)))return false;
    const auto actor=std::string_view(state.actor).substr(split+separator.size());
    if(actor.empty()||!std::ranges::all_of(actor,symbol)||state.component.size()>max_barrier_component||
       !std::ranges::all_of(state.component,[&](char c){return symbol(c)||c==' ';}))return false;
    if(state.kind==BarrierKind::bridge&&state.component!="BlockerBox")return false;
    if(state.kind==BarrierKind::castle&&state.component!="InteractBox")return false;
    if(state.component.empty())return state.collision<=1&&state.visible&&state.transform==identity_transform;
    if(state.collision>3||!std::ranges::all_of(state.transform,[](float value){
        return std::isfinite(value)&&std::abs(value)<=10000000.f;
    }))return false;
    float norm{};
    for(unsigned i=0;i<4;++i)norm+=state.transform[i]*state.transform[i];
    return std::abs(norm-1.f)<=.02f;
}
inline bool valid(const Streaming& value){
    for(const auto& stack:value.stacks){
        if((!value.known&&!stack.empty())||stack.size()>max_stream_entries)return false;
        for(const auto& entry:stack)
            if(!stream_path(entry.path)||entry.priority<-100000||entry.priority>100000)return false;
    }
    return true;
}
inline bool mechanism_less(const MechanismState& left,const MechanismState& right){
    return left.kind==right.kind?left.key<right.key:left.kind<right.kind;
}
struct Snapshot {
    std::vector<ActorState> actors;
    std::vector<NamedState> names;
    std::vector<MechanismState> mechanisms;
    Streaming streaming;
    Arrival arrival;
    std::string music;
    std::vector<ActorItemState> items;
    std::vector<BarrierState> barriers;
    bool operator==(const Snapshot&) const = default;
};
using Bytes=std::vector<std::uint8_t>;
inline constexpr unsigned max_entries=4096,max_mechanisms=4096,max_name=900,max_bytes=1024*1024;
inline constexpr unsigned snapshot_kind=17,ack_kind=18,header_bytes=68,part_bytes=1152;
inline constexpr unsigned max_parts=(max_bytes+part_bytes-1)/part_bytes;
inline constexpr char lower_ascii(char c){return c>='A'&&c<='Z'?static_cast<char>(c+('a'-'A')):c;}
inline bool name_less(std::string_view left,std::string_view right){
    return std::lexicographical_compare(left.begin(),left.end(),right.begin(),right.end(),[](char a,char b){
        return static_cast<unsigned char>(lower_ascii(a))<static_cast<unsigned char>(lower_ascii(b));
    });
}
inline bool name_equal(std::string_view left,std::string_view right){
    return std::ranges::equal(left,right,[](char a,char b){return lower_ascii(a)==lower_ascii(b);});
}
inline bool valid_name(std::string_view name){
    return !name.empty()&&name.size()<=max_name&&
           std::ranges::none_of(name,[](char c){return c=='\0'||c=='\r'||c=='\n'||c=='|';});
}
inline bool valid(const Snapshot& value){
    if(value.actors.size()>max_entries||value.items.size()>max_entries||value.barriers.size()>max_barriers||
       value.names.size()>max_entries||value.mechanisms.size()>max_mechanisms)return false;
    Guid prior{};
    for(unsigned i=0;i<value.actors.size();++i){
        const auto& key=value.actors[i].key;
        if(i&&prior>=key)return false;
        prior=key;
    }
    for(unsigned i=0;i<value.items.size();++i){
        const auto& state=value.items[i];
        if((i&&value.items[i-1].key>=state.key)||(!state.path.empty()&&!stream_path(state.path)))return false;
    }
    for(unsigned i=0;i<value.barriers.size();++i)
        if(!valid(value.barriers[i])||(i&&!barrier_less(value.barriers[i-1],value.barriers[i])))return false;
    std::string_view previous;
    for(const auto& name:value.names){
        if(!valid_name(name.key)||(!previous.empty()&&!name_less(previous,name.key)))return false;
        previous=name.key;
    }
    for(unsigned i=0;i<value.mechanisms.size();++i){
        const auto& state=value.mechanisms[i];
        if(state.key==Guid{}||state.kind<MechanismKind::door||state.kind>MechanismKind::gargoyle||
           state.activations>10000||(state.kind!=MechanismKind::door&&state.activations)||
           (i&&!mechanism_less(value.mechanisms[i-1],state)))return false;
    }
    return valid(value.streaming)&&valid(value.arrival)&&(!value.arrival.serial||value.streaming.known)&&
           (value.music.empty()||music_path(value.music));
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
inline bool read_int(std::span<const std::uint8_t>& bytes,std::int32_t& value){
    std::uint32_t raw{};
    if(!read32(bytes,raw))return false;
    value=std::bit_cast<std::int32_t>(raw);
    return true;
}
inline bool encode(const Snapshot& value,Bytes& out){
    if(!valid(value))return false;
    std::size_t size=snapshot_prefix+20*value.actors.size()+28*value.mechanisms.size()+value.music.size();
    for(const auto& state:value.names)size+=8+state.key.size();
    for(const auto& state:value.items)size+=20+state.path.size();
    for(const auto& state:value.barriers)size+=56+state.actor.size()+state.component.size();
    for(const auto& stack:value.streaming.stacks)for(const auto& entry:stack)size+=8+entry.path.size();
    if(size>max_bytes)return false;
    Bytes bytes;
    bytes.reserve(size);
    append32(bytes,static_cast<std::uint32_t>(value.actors.size()));
    append32(bytes,static_cast<std::uint32_t>(value.names.size()));
    append32(bytes,static_cast<std::uint32_t>(value.mechanisms.size()));
    append32(bytes,static_cast<std::uint32_t>(value.streaming.known));
    for(const auto& stack:value.streaming.stacks)append32(bytes,static_cast<std::uint32_t>(stack.size()));
    append32(bytes,static_cast<std::uint32_t>(value.arrival.serial));
    append32(bytes,static_cast<std::uint32_t>(value.arrival.serial>>32));
    for(const auto coordinate:value.arrival.position)append32(bytes,std::bit_cast<std::uint32_t>(coordinate));
    append32(bytes,std::bit_cast<std::uint32_t>(value.arrival.yaw));
    append32(bytes,static_cast<std::uint32_t>(value.music.size()));
    append32(bytes,static_cast<std::uint32_t>(value.items.size()));
    append32(bytes,static_cast<std::uint32_t>(value.barriers.size()));
    for(const auto& state:value.actors){
        for(const auto part:state.key)append32(bytes,std::bit_cast<std::uint32_t>(part));
        append32(bytes,std::bit_cast<std::uint32_t>(state.value));
    }
    for(const auto& state:value.names){
        append32(bytes,static_cast<std::uint32_t>(state.key.size()));
        append32(bytes,std::bit_cast<std::uint32_t>(state.value));
        bytes.insert(bytes.end(),state.key.begin(),state.key.end());
    }
    for(const auto& state:value.mechanisms){
        for(const auto part:state.key)append32(bytes,std::bit_cast<std::uint32_t>(part));
        append32(bytes,static_cast<std::uint32_t>(state.kind));
        append32(bytes,state.activations);
        append32(bytes,static_cast<std::uint32_t>(state.active));
    }
    for(const auto& stack:value.streaming.stacks)for(const auto& entry:stack){
        append32(bytes,static_cast<std::uint32_t>(entry.path.size()));
        append32(bytes,std::bit_cast<std::uint32_t>(entry.priority));
        bytes.insert(bytes.end(),entry.path.begin(),entry.path.end());
    }
    for(const auto& state:value.items){
        for(const auto part:state.key)append32(bytes,std::bit_cast<std::uint32_t>(part));
        append32(bytes,static_cast<std::uint32_t>(state.path.size()));
        bytes.insert(bytes.end(),state.path.begin(),state.path.end());
    }
    for(const auto& state:value.barriers){
        append32(bytes,static_cast<std::uint32_t>(state.kind));
        append32(bytes,static_cast<std::uint32_t>(state.actor.size()));
        append32(bytes,static_cast<std::uint32_t>(state.component.size()));
        append32(bytes,state.collision|(state.hidden?4u:0u)|(state.visible?8u:0u));
        for(const auto value:state.transform)append32(bytes,std::bit_cast<std::uint32_t>(value));
        bytes.insert(bytes.end(),state.actor.begin(),state.actor.end());
        bytes.insert(bytes.end(),state.component.begin(),state.component.end());
    }
    bytes.insert(bytes.end(),value.music.begin(),value.music.end());
    out=std::move(bytes);
    return true;
}
inline bool decode(std::span<const std::uint8_t> bytes,Snapshot& out){
    std::uint32_t actor_count{},name_count{},mechanism_count{},streams_known{},stream_count{},trigger_count{};
    if(bytes.size()>max_bytes||!read32(bytes,actor_count)||!read32(bytes,name_count)||!read32(bytes,mechanism_count)||
       !read32(bytes,streams_known)||!read32(bytes,stream_count)||!read32(bytes,trigger_count)||streams_known>1||
       stream_count>max_stream_entries||trigger_count>max_stream_entries||(!streams_known&&(stream_count||trigger_count))||
       actor_count>max_entries||name_count>max_entries||mechanism_count>max_mechanisms||
       bytes.size()<36u+20u*actor_count+9u*name_count+28u*mechanism_count+8u*(stream_count+trigger_count))return false;
    Snapshot value;
    std::uint32_t low{},high{},bits{};
    if(!read32(bytes,low)||!read32(bytes,high))return false;
    value.arrival.serial=std::uint64_t(high)<<32|low;
    for(auto& coordinate:value.arrival.position){
        if(!read32(bytes,bits))return false;
        coordinate=std::bit_cast<float>(bits);
    }
    if(!read32(bytes,bits))return false;
    value.arrival.yaw=std::bit_cast<float>(bits);
    std::uint32_t music_length{},item_count{},barrier_count{};
    if(!read32(bytes,music_length)||music_length>max_stream_path||!read32(bytes,item_count)||item_count>max_entries||
       !read32(bytes,barrier_count)||barrier_count>max_barriers||
       bytes.size()<20u*actor_count+9u*name_count+28u*mechanism_count+8u*(stream_count+trigger_count)+
                    20u*item_count+56u*barrier_count+music_length)return false;
    value.actors.resize(actor_count);
    value.names.resize(name_count);
    value.mechanisms.resize(mechanism_count);
    value.items.resize(item_count);
    value.barriers.resize(barrier_count);
    value.streaming.known=streams_known!=0;
    value.streaming.stacks[0].resize(stream_count);
    value.streaming.stacks[1].resize(trigger_count);
    for(auto& state:value.actors){
        for(auto& part:state.key)if(!read_int(bytes,part))return false;
        if(!read_int(bytes,state.value))return false;
    }
    for(auto& state:value.names){
        std::uint32_t length{};
        if(!read32(bytes,length)||!length||length>max_name||!read_int(bytes,state.value)||length>bytes.size())return false;
        state.key.assign(reinterpret_cast<const char*>(bytes.data()),length);
        bytes=bytes.subspan(length);
    }
    for(auto& state:value.mechanisms){
        for(auto& part:state.key)if(!read_int(bytes,part))return false;
        std::uint32_t kind{},active{};
        if(!read32(bytes,kind)||!read32(bytes,state.activations)||!read32(bytes,active)||active>1)return false;
        state.kind=static_cast<MechanismKind>(kind);
        state.active=active!=0;
    }
    for(auto& stack:value.streaming.stacks)for(auto& entry:stack){
        std::uint32_t length{};
        if(!read32(bytes,length)||!length||length>max_stream_path||!read_int(bytes,entry.priority)||length>bytes.size())return false;
        entry.path.assign(reinterpret_cast<const char*>(bytes.data()),length);
        bytes=bytes.subspan(length);
    }
    for(auto& state:value.items){
        for(auto& part:state.key)if(!read_int(bytes,part))return false;
        std::uint32_t length{};
        if(!read32(bytes,length)||length>max_stream_path||length>bytes.size())return false;
        if(length)state.path.assign(reinterpret_cast<const char*>(bytes.data()),length);
        bytes=bytes.subspan(length);
    }
    for(auto& state:value.barriers){
        std::uint32_t kind{},actor_length{},component_length{},flags{};
        if(!read32(bytes,kind)||!read32(bytes,actor_length)||!read32(bytes,component_length)||!read32(bytes,flags)||
           !actor_length||actor_length>max_barrier_actor||component_length>max_barrier_component||flags>15)return false;
        state.kind=static_cast<BarrierKind>(kind);
        state.collision=flags&3u;
        state.hidden=(flags&4u)!=0;
        state.visible=(flags&8u)!=0;
        for(auto& value:state.transform){
            if(!read32(bytes,bits))return false;
            value=std::bit_cast<float>(bits);
        }
        if(bytes.size()<actor_length+component_length)return false;
        state.actor.assign(reinterpret_cast<const char*>(bytes.data()),actor_length);
        bytes=bytes.subspan(actor_length);
        if(component_length)state.component.assign(reinterpret_cast<const char*>(bytes.data()),component_length);
        bytes=bytes.subspan(component_length);
    }
    if(bytes.size()!=music_length)return false;
    if(music_length)value.music.assign(reinterpret_cast<const char*>(bytes.data()),music_length);
    if(!valid(value))return false;
    out=std::move(value);
    return true;
}
inline std::uint64_t digest(std::span<const std::uint8_t> bytes){
    std::uint64_t hash=14695981039346656037ull;
    for(const auto byte:bytes)hash=(hash^byte)*1099511628211ull;
    return hash?hash:1;
}
struct Revision {std::uint64_t number{},hash{}; bool operator==(const Revision&) const = default;};
struct Part {
    Revision revision;
    unsigned index{},total{};
    std::span<const std::uint8_t> bytes;
};
inline unsigned count(unsigned total){return (total+part_bytes-1)/part_bytes;}
inline bool valid(const Part& part){
    return part.revision.number&&part.revision.hash&&part.total>=snapshot_prefix&&part.total<=max_bytes&&
           part.index<count(part.total)&&part.bytes.size()==std::min(part_bytes,part.total-part.index*part_bytes);
}
inline bool encode(const enemy::Envelope& scope,const Part& part,enemy::Packet& packet){
    if(!valid(part))return false;
    using enemy::animation::wire::put;
    enemy::enrollment::write_header(scope,snapshot_kind,header_bytes+static_cast<unsigned>(part.bytes.size()),packet);
    put(packet,40,part.revision.number);
    put(packet,48,part.revision.hash);
    put(packet,56,part.index);
    put(packet,60,part.total);
    put(packet,64,std::uint32_t{0});
    std::memcpy(packet.data.data()+header_bytes,part.bytes.data(),part.bytes.size());
    return true;
}
inline bool decode(const enemy::Envelope& scope,const enemy::Packet& packet,Part& out){
    if(packet.size<=header_bytes||packet.size>header_bytes+part_bytes||
       !enemy::enrollment::check_header(scope,packet,snapshot_kind,packet.size))return false;
    using enemy::animation::wire::get;
    const Part value{{get<std::uint64_t>(packet,40),get<std::uint64_t>(packet,48)},
                     get<std::uint32_t>(packet,56),get<std::uint32_t>(packet,60),
                     {packet.data.data()+header_bytes,packet.size-header_bytes}};
    if(get<std::uint32_t>(packet,64)||!valid(value))return false;
    out=value;
    return true;
}
inline bool encode_ack(const enemy::Envelope& scope,Revision revision,enemy::Packet& packet){
    if(!revision.number||!revision.hash)return false;
    using enemy::animation::wire::put;
    enemy::enrollment::write_header(scope,ack_kind,56,packet);
    put(packet,40,revision.number);
    put(packet,48,revision.hash);
    return true;
}
inline bool decode_ack(const enemy::Envelope& scope,const enemy::Packet& packet,Revision& revision){
    if(!enemy::enrollment::check_header(scope,packet,ack_kind,56))return false;
    using enemy::animation::wire::get;
    const Revision value{get<std::uint64_t>(packet,40),get<std::uint64_t>(packet,48)};
    if(!value.number||!value.hash)return false;
    revision=value;
    return true;
}
class Inbox {
    Revision revision_{};
    Bytes bytes_;
    std::bitset<max_parts> parts_;
    unsigned received_{};
public:
    Revision revision() const {return revision_;}
    bool push(const Part& part){
        if(!valid(part)||part.revision.number<revision_.number)return false;
        if(part.revision.number>revision_.number){
            revision_=part.revision;
            bytes_.resize(part.total);
            parts_.reset();
            received_=0;
        }
        if(part.revision!=revision_||part.total!=bytes_.size())return false;
        const auto at=bytes_.data()+part.index*part_bytes;
        if(parts_.test(part.index))return std::memcmp(at,part.bytes.data(),part.bytes.size())==0;
        std::memcpy(at,part.bytes.data(),part.bytes.size());
        parts_.set(part.index);
        ++received_;
        return true;
    }
    bool complete() const {return revision_.number&&received_==count(static_cast<unsigned>(bytes_.size()));}
    bool snapshot(Snapshot& out) const {return complete()&&digest(bytes_)==revision_.hash&&decode(bytes_,out);}
};
}
