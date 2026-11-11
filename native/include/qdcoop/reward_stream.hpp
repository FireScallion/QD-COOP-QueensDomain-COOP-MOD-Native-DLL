#pragma once
#include "qdcoop/animation_state.hpp"
#include <string_view>
namespace qdcoop::enemy::reward {
constexpr unsigned window=64,text_capacity=300;
constexpr unsigned drop_mark_first=1u<<6;
enum class Kind:std::uint32_t {xp=1,bestiary=2,drop=3};
struct Text {
 std::array<char,text_capacity+1> data{};std::uint16_t size{};
 bool set(std::string_view value){if(value.size()>text_capacity)return false;data={};size=static_cast<std::uint16_t>(value.size());std::copy(value.begin(),value.end(),data.begin());return true;}
 std::string_view view()const{return {data.data(),size};}
 bool operator==(const Text&)const=default;
};
struct Event {
 std::uint64_t sequence{};Kind kind{};std::uint32_t entity{},generation{},flags{};std::int32_t amount{};
 // location3, rotation3, force3, override location3, extra rate, pity modifier.
 std::array<float,14> values{};
 // chance1, pity1, chance2, pity2; original helper rolls each character's RNG.
 std::array<float,4> chances{};std::array<std::int32_t,2> counts{};
 // EnemyData, source FName text, item1, item2. No truncated resource paths.
 std::array<Text,4> text{};
 bool operator==(const Event&)const=default;
};
inline bool clean(const Text& t){if(t.size>text_capacity)return false;for(unsigned i=0;i<t.size;++i)if(!t.data[i]||t.data[i]=='\r'||t.data[i]=='\n')return false;return true;}
inline bool asset(const Text& t,bool empty=false){
 if(!clean(t))return false;
 if(t.size==0)return empty;
 if(!t.view().starts_with("/Game/")||t.size<=6)return false;
 for(char c:t.view())if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='/'||c=='.'||c=='-'))return false;
 return true;
}
inline bool valid(const Event& e){
 if(!e.sequence||e.sequence==UINT64_MAX||e.entity>capacity||((e.entity==0)!=(e.generation==0)))return false;
 for(const auto& t:e.text)if(!clean(t))return false;
 for(auto v:e.values)if(!std::isfinite(v)||std::abs(v)>1e7f)return false;
 for(auto v:e.chances)if(!std::isfinite(v)||std::abs(v)>1e7f)return false;
 if(e.kind==Kind::xp)return !e.entity&&!e.flags&&e.amount>=1&&e.amount<=10000000;
 if(e.kind==Kind::bestiary)return !e.entity&&e.flags<=3&&asset(e.text[0]);
 if(e.kind!=Kind::drop||e.flags>127||!asset(e.text[0])||!asset(e.text[2],true)||!asset(e.text[3],true))return false;
 for(auto n:e.counts)if(n< -1||n>10000)return false;
 return true;
}
inline bool header(const Envelope& e,const Packet& p,unsigned kind){
 using animation::wire::get;return p.size>=40&&p.size<=packet_capacity&&get<std::uint32_t>(p,0)==0x45344e51&&get<std::uint32_t>(p,4)==1&&get<std::uint64_t>(p,8)==e.room_high&&get<std::uint64_t>(p,16)==e.room_low&&get<std::uint64_t>(p,24)==e.world&&get<std::uint32_t>(p,32)==kind&&get<std::uint32_t>(p,36)==p.size-40;
}
inline void header(const Envelope& e,Packet& p,unsigned kind){using animation::wire::put;put(p,0,std::uint32_t{0x45344e51});put(p,4,std::uint32_t{1});put(p,8,e.room_high);put(p,16,e.room_low);put(p,24,e.world);put(p,32,kind);put(p,36,p.size-40);}
inline bool encode(const Envelope& room,const Event& e,Packet& p){
 if(!valid(e))return false;
 using animation::wire::put;p={};unsigned cursor=148;
 for(const auto& t:e.text){put(p,cursor,t.size);cursor+=2;std::memcpy(p.data.data()+cursor,t.data.data(),t.size);cursor+=t.size;}
 p.size=cursor;header(room,p,6);put(p,40,e.sequence);put(p,48,static_cast<std::uint32_t>(e.kind));put(p,52,e.entity);put(p,56,e.generation);put(p,60,e.flags);put(p,64,e.amount);
 for(unsigned i=0;i<14;++i)put(p,68+4*i,e.values[i]);
 for(unsigned i=0;i<4;++i)put(p,124+4*i,e.chances[i]);
 for(unsigned i=0;i<2;++i)put(p,140+4*i,e.counts[i]);
 return true;
}
inline bool decode(const Envelope& room,const Packet& p,Event& out){
 if(p.size<156||p.size>1356||!header(room,p,6))return false;
 using animation::wire::get;Event e{};e.sequence=get<std::uint64_t>(p,40);e.kind=static_cast<Kind>(get<std::uint32_t>(p,48));e.entity=get<std::uint32_t>(p,52);e.generation=get<std::uint32_t>(p,56);e.flags=get<std::uint32_t>(p,60);e.amount=get<std::int32_t>(p,64);
 for(unsigned i=0;i<14;++i)e.values[i]=get<float>(p,68+4*i);
 for(unsigned i=0;i<4;++i)e.chances[i]=get<float>(p,124+4*i);
 for(unsigned i=0;i<2;++i)e.counts[i]=get<std::int32_t>(p,140+4*i);
 unsigned cursor=148;for(auto& t:e.text){if(cursor+2>p.size)return false;const auto size=get<std::uint16_t>(p,cursor);cursor+=2;if(size>text_capacity||cursor+size>p.size)return false;t.size=size;std::memcpy(t.data.data(),p.data.data()+cursor,size);cursor+=size;}
 if(cursor!=p.size||!valid(e))return false;
 out=e;return true;
}
inline bool encode_ack(const Envelope& room,std::uint64_t applied,Packet& p){if(!applied||applied==UINT64_MAX)return false;p={};p.size=48;header(room,p,7);animation::wire::put(p,40,applied);return true;}
inline bool decode_ack(const Envelope& room,const Packet& p,std::uint64_t& applied){if(p.size!=48||!header(room,p,7))return false;auto n=animation::wire::get<std::uint64_t>(p,40);if(!n||n==UINT64_MAX)return false;applied=n;return true;}
enum class Result {accepted,duplicate,gap,conflict,invalid,quarantined};
enum class Phase {waiting,started,quarantined};
// Applied cursor is never evicted. Receipt alone never permits a second roll.
// In-memory ledger only: process recovery requires an external durable journal.
class Inbox {
 std::array<Event,window> slots_{};std::uint64_t next_{1};Phase phase_{};
public:
 Result receive(const Event& e){
  if(!valid(e))return Result::invalid;
  if(e.sequence<next_)return Result::duplicate;
  if(e.sequence-next_>=window)return Result::gap;
  auto& slot=slots_[e.sequence%window];if(slot.sequence)return slot==e?Result::duplicate:Result::conflict;
  slot=e;return Result::accepted;
 }
 const Event* next()const{const auto& e=slots_[next_%window];return e.sequence==next_?&e:nullptr;}
 bool begin(){if(phase_!=Phase::waiting||!next())return false;phase_=Phase::started;return true;}
 // A failed resource preflight occurs BEFORE begin(), so it needs no rollback.
 bool complete(){if(phase_!=Phase::started)return false;slots_[next_%window]={};++next_;phase_=Phase::waiting;return true;}
 bool uncertain(){if(phase_!=Phase::started)return false;phase_=Phase::quarantined;return true;}
 void conflict(){phase_=Phase::quarantined;}
 unsigned pending()const{unsigned n=0;for(const auto& e:slots_)n+=e.sequence!=0;return n;}
 // Local persistence adapter only: restore after validating the loaded save
 // marker, journal identity and durable proof. Never accept a remote ACK here.
 bool restore_applied(std::uint64_t sequence){if(next_!=1||phase_!=Phase::waiting||pending()||sequence==UINT64_MAX)return false;next_=sequence+1;return true;}
 std::uint64_t applied()const{return next_-1;}
 Phase phase()const{return phase_;}
};
struct Pending {Event event{};unsigned waiting{};std::array<double,3> retry{};};
class Outbox {
 std::array<Pending,window> slots_{};std::uint64_t next_{1};unsigned count_{};
public:
 bool enqueue(Event event,unsigned peers){
  if(!peers||peers>7||next_==UINT64_MAX)return false;
  event.sequence=next_;if(!valid(event))return false;
  auto& slot=slots_[next_%window];if(slot.event.sequence)return false;
  slot={};slot.event=event;slot.waiting=peers;++next_;++count_;return true;
 }
 const Event* due(unsigned peer,double now){if(peer>=3)return nullptr;Pending* best=nullptr;for(auto& s:slots_)if((s.waiting&(1u<<peer))&&now>=s.retry[peer]&&(!best||s.event.sequence<best->event.sequence))best=&s;if(!best)return nullptr;best->retry[peer]=now+.2;return &best->event;}
 bool applied_ack(unsigned peer,std::uint64_t sequence){
  if(peer>=3||!sequence||sequence>=next_)return false;
  for(auto& s:slots_)if(s.event.sequence&&s.event.sequence<=sequence){s.waiting&=~(1u<<peer);if(!s.waiting){s={};--count_;}}
  return true;
 }
 unsigned pending()const{return count_;}
};
}
