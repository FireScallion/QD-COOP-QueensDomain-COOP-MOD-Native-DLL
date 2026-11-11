#pragma once
#include "qdcoop/animation_state.hpp"
namespace qdcoop::enemy::death {
// Exactly one terminal death per registered generation. Rewards use a separate
// transaction and MUST NOT be inferred from receipt of this presentation event.
struct Event {std::uint32_t id{},generation{};std::uint64_t sequence{1};};
inline bool valid(const Event& e){return e.id&&e.id<=capacity&&e.generation&&e.sequence==1;}
inline bool encode(const Envelope& room,const Event& e,bool ack,Packet& p){
 if(!valid(e))return false;
 using animation::wire::put;p={};p.size=56;
 put(p,0,std::uint32_t{0x45344e51});put(p,4,std::uint32_t{1});put(p,8,room.room_high);put(p,16,room.room_low);put(p,24,room.world);put(p,32,std::uint32_t(ack?5:4));put(p,36,std::uint32_t{16});put(p,40,e.id);put(p,44,e.generation);put(p,48,e.sequence);return true;
}
inline bool decode(const Envelope& room,const Packet& p,bool ack,Event& out){
 using animation::wire::get;
 if(p.size!=56||get<std::uint32_t>(p,0)!=0x45344e51||get<std::uint32_t>(p,4)!=1||get<std::uint64_t>(p,8)!=room.room_high||get<std::uint64_t>(p,16)!=room.room_low||get<std::uint64_t>(p,24)!=room.world||get<std::uint32_t>(p,32)!=(ack?5u:4u)||get<std::uint32_t>(p,36)!=16)return false;
 Event e{get<std::uint32_t>(p,40),get<std::uint32_t>(p,44),get<std::uint64_t>(p,48)};if(!valid(e))return false;out=e;return true;
}
struct Pending {
 Event event{};unsigned waiting{};double retry{};bool terminal{};
 bool begin(Event e,unsigned peers){if(terminal||!valid(e)||peers>7)return false;event=e;waiting=peers;retry=0;terminal=true;return true;}
 bool acknowledge(Event e,unsigned peer){if(!terminal||peer>=3||event.id!=e.id||event.generation!=e.generation||event.sequence!=e.sequence)return false;waiting&=~(1u<<peer);return true;}
 bool due(double time){if(!waiting||time<retry)return false;retry=time+.2;return true;}
};
}
