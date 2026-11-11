#pragma once
#include "qdcoop/reward_stream.hpp"
namespace qdcoop::enemy::reward::journal {
struct Identity {
 std::uint64_t room_high{},room_low{},world{},character_high{},character_low{},save_chain{};
 bool valid()const{return (room_high||room_low)&&world&&(character_high||character_low)&&save_chain;}
 bool operator==(const Identity&)const=default;
 Envelope envelope()const{return {room_high,room_low,world,1};}
};
struct SaveProof {
 std::uint64_t bytes{};std::array<std::uint8_t,32> sha256{};
 bool valid()const{bool any=false;for(auto b:sha256)any|=b!=0;return bytes&&any;}
 bool operator==(const SaveProof&)const=default;
};
struct Marker{Identity identity{};std::uint64_t applied{};};
using MarkerWords=std::array<std::uint32_t,16>;
inline std::uint32_t marker_crc(const MarkerWords& words){std::uint32_t c=~0u;for(unsigned i=0;i<15;++i)for(unsigned byte=0;byte<4;++byte){c^=(words[i]>>(8*byte))&255u;for(unsigned bit=0;bit<8;++bit)c=(c>>1)^(0xedb88320u&(0u-(c&1u)));}return ~c;}
inline bool marker_encode(const Marker& marker,MarkerWords& words){
 if(!marker.identity.valid()||marker.applied==UINT64_MAX)return false;
 const auto& id=marker.identity;const std::array<std::uint64_t,7> values{id.room_high,id.room_low,id.world,id.character_high,id.character_low,id.save_chain,marker.applied};words={};words[0]=0x51445231;
 for(unsigned i=0;i<7;++i){words[1+i*2]=static_cast<std::uint32_t>(values[i]);words[2+i*2]=static_cast<std::uint32_t>(values[i]>>32);}words[15]=marker_crc(words);return true;
}
inline bool marker_decode(const MarkerWords& words,Marker& marker){
 if(words[0]!=0x51445231||words[15]!=marker_crc(words))return false;
 std::array<std::uint64_t,7> values{};for(unsigned i=0;i<7;++i)values[i]=words[1+i*2]|(static_cast<std::uint64_t>(words[2+i*2])<<32);
 Marker result{{values[0],values[1],values[2],values[3],values[4],values[5]},values[6]};if(!result.identity.valid()||result.applied==UINT64_MAX)return false;marker=result;return true;
}
enum class Kind:std::uint32_t{baseline=1,begin=2,applied=3,saved=4};
constexpr unsigned record_size=1480,checksum_offset=1476;
struct Record{std::array<std::uint8_t,record_size> data{};};
template<class T>inline void put(Record& r,unsigned at,T v){std::memcpy(r.data.data()+at,&v,sizeof(v));}
template<class T>inline T get(const Record& r,unsigned at){T v{};std::memcpy(&v,r.data.data()+at,sizeof(v));return v;}
inline std::uint32_t crc(const Record& r){std::uint32_t value=~0u;for(unsigned i=0;i<checksum_offset;++i){value^=r.data[i];for(unsigned bit=0;bit<8;++bit)value=(value>>1)^(0xedb88320u& (0u-(value&1u)));}return ~value;}
inline bool encode(const Identity& id,std::uint64_t ordinal,Kind kind,std::uint64_t sequence,const Event* event,const SaveProof& save,Record& r){
 if(!id.valid()||!ordinal||ordinal==UINT64_MAX)return false;
 Packet packet{};if(event&&(!reward::encode(id.envelope(),*event,packet)||event->sequence!=sequence))return false;
 const bool checkpoint=kind==Kind::baseline||kind==Kind::saved;
 if((kind==Kind::begin)!=(event!=nullptr)||checkpoint!=save.valid()||(!checkpoint&&save!=SaveProof{})||kind<Kind::baseline||kind>Kind::saved)return false;
 r={};put(r,0,std::uint32_t{0x4a524451});put(r,4,std::uint32_t{1});put(r,8,ordinal);put(r,16,static_cast<std::uint32_t>(kind));put(r,20,packet.size);
 const std::array<std::uint64_t,6> values{id.room_high,id.room_low,id.world,id.character_high,id.character_low,id.save_chain};for(unsigned i=0;i<6;++i)put(r,24+i*8,values[i]);
 put(r,72,sequence);put(r,80,save.bytes);std::memcpy(r.data.data()+88,save.sha256.data(),32);if(packet.size)std::memcpy(r.data.data()+120,packet.data.data(),packet.size);put(r,checksum_offset,crc(r));return true;
}
// An applied engine result is deliberately NOT a durable game save receipt.
// On restart, every result after the last proven save requires reconciliation.
class State {
 Identity id_{};std::uint64_t records_{},applied_{},saved_{};Event pending_{};SaveProof proof_{};
public:
 explicit State(Identity id):id_(id){}
 bool consume(const Record& r){
  if(!id_.valid()||get<std::uint32_t>(r,0)!=0x4a524451||get<std::uint32_t>(r,4)!=1||get<std::uint32_t>(r,checksum_offset)!=crc(r)||get<std::uint64_t>(r,8)!=records_+1)return false;
  const Identity found{get<std::uint64_t>(r,24),get<std::uint64_t>(r,32),get<std::uint64_t>(r,40),get<std::uint64_t>(r,48),get<std::uint64_t>(r,56),get<std::uint64_t>(r,64)};if(found!=id_)return false;
  const auto kind=static_cast<Kind>(get<std::uint32_t>(r,16));const auto sequence=get<std::uint64_t>(r,72);const auto size=get<std::uint32_t>(r,20);SaveProof proof{};proof.bytes=get<std::uint64_t>(r,80);std::memcpy(proof.sha256.data(),r.data.data()+88,32);
  if(size>1356||sequence==UINT64_MAX)return false;
  for(unsigned i=120+size;i<checksum_offset;++i)if(r.data[i])return false;
  if(kind==Kind::baseline){if(records_||sequence||size||!proof.valid())return false;proof_=proof;}
  else if(!records_)return false;
  else if(kind==Kind::begin){if(pending_.sequence||sequence!=applied_+1||proof!=SaveProof{})return false;Packet packet{};packet.size=size;std::memcpy(packet.data.data(),r.data.data()+120,size);Event event{};if(!reward::decode(id_.envelope(),packet,event)||event.sequence!=sequence)return false;pending_=event;}
  else if(kind==Kind::applied){if(size||proof!=SaveProof{}||!pending_.sequence||pending_.sequence!=sequence)return false;applied_=sequence;pending_={};}
  else if(kind==Kind::saved){if(size||pending_.sequence||sequence!=applied_||!proof.valid())return false;saved_=sequence;proof_=proof;}
  else return false;
  ++records_;return true;
 }
 bool recovery_required()const{return pending_.sequence||applied_!=saved_;}
 bool matches_save(const SaveProof& proof)const{return records_&&proof.valid()&&proof==proof_;}
 std::uint64_t records()const{return records_;}std::uint64_t applied()const{return applied_;}std::uint64_t saved()const{return saved_;}
 const Event& pending()const{return pending_;}const Identity& identity()const{return id_;}
};
}
