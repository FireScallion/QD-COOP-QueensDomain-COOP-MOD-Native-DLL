#pragma once
#include "animation_layout.hpp"
#include "qdcoop/enemy_stream.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <bit>
#include <limits>
namespace qdcoop::enemy::animation {
struct Frame {
 std::uint32_t id{},generation{},profile{},montage{},flags{};
 std::uint64_t sequence{},serial{};double time{};
 float position{},rate{1},aim{};std::array<float,8> inputs{};
};
inline bool valid(const Frame& f){
 if(!f.id||f.id>capacity||!f.generation||!f.sequence||!f.profile||f.profile>=std::size(layouts)||f.montage>=std::size(montages)||f.flags>31)return false;
 if(!std::isfinite(f.time)||f.time<0||f.time>1e8||!std::isfinite(f.position)||f.position<0||f.position>3600||!std::isfinite(f.rate)||f.rate<0||f.rate>10||!std::isfinite(f.aim)||std::abs(f.aim)>360)return false;
 const auto& l=layouts[f.profile];if(f.montage&&montages[f.montage]!=l.skeleton)return false;
 for(unsigned i=0;i<l.count;++i){const float v=f.inputs[i];const auto k=l.kinds[i];if(!std::isfinite(v)||std::abs(v)>1e7f)return false;
  if(k=='b'&&v!=0&&v!=1)return false;
  if((k=='i'||k=='a')&&std::trunc(v)!=v)return false;
  if(k=='a'&&(v<0||v>=std::size(sequences)||(v>0&&sequences[static_cast<unsigned>(v)]!=l.skeleton)))return false;
 }return true;
}
namespace wire {
template<class T>inline void put(Packet& p,unsigned at,T v){std::memcpy(p.data.data()+at,&v,sizeof(v));}
template<class T>inline T get(const Packet& p,unsigned at){T v;std::memcpy(&v,p.data.data()+at,sizeof(v));return v;}
}
inline bool encode(const Envelope& e,const Frame& f,Packet& p){
 static_assert(std::endian::native==std::endian::little);
 if(!valid(f))return false;
 p={};p.size=96+4*layouts[f.profile].count;
 using wire::put;put(p,0,std::uint32_t{0x45344e51});put(p,4,std::uint32_t{1});put(p,8,e.room_high);put(p,16,e.room_low);put(p,24,e.world);put(p,32,std::uint32_t{3});put(p,36,p.size-40);
 put(p,40,f.id);put(p,44,f.generation);put(p,48,f.sequence);put(p,56,f.time);put(p,64,f.profile);put(p,68,f.montage);put(p,72,f.serial);put(p,80,f.flags);put(p,84,f.position);put(p,88,f.rate);put(p,92,f.aim);
 for(unsigned i=0;i<layouts[f.profile].count;++i)put(p,96+4*i,f.inputs[i]);
 return true;
}
inline bool decode(const Envelope& e,const Packet& p,Frame& output){
 using wire::get;
 if(p.size<96||p.size>128||get<std::uint32_t>(p,0)!=0x45344e51||get<std::uint32_t>(p,4)!=1||get<std::uint64_t>(p,8)!=e.room_high||get<std::uint64_t>(p,16)!=e.room_low||get<std::uint64_t>(p,24)!=e.world||get<std::uint32_t>(p,32)!=3||get<std::uint32_t>(p,36)!=p.size-40)return false;
 Frame f{};f.id=get<std::uint32_t>(p,40);f.generation=get<std::uint32_t>(p,44);f.sequence=get<std::uint64_t>(p,48);f.time=get<double>(p,56);f.profile=get<std::uint32_t>(p,64);f.montage=get<std::uint32_t>(p,68);f.serial=get<std::uint64_t>(p,72);f.flags=get<std::uint32_t>(p,80);f.position=get<float>(p,84);f.rate=get<float>(p,88);f.aim=get<float>(p,92);
 if(!f.profile||f.profile>=std::size(layouts)||p.size!=96+4*layouts[f.profile].count)return false;
 for(unsigned i=0;i<layouts[f.profile].count;++i)f.inputs[i]=get<float>(p,96+4*i);
 if(!valid(f))return false;
 output=f;return true;
}
inline bool in_loop(unsigned id,double p){const auto end=loop_end(id);return end>0&&p>=0&&p<end;}
inline bool restart(unsigned old,unsigned id,double before,double after){return old!=id||in_loop(id,before)!=in_loop(id,after)||(!in_loop(id,before)&&after<before-.04);}
inline double advance(unsigned id,double p,double elapsed,double rate){
 const auto end=loop_end(id);double target=std::max(0.,p+std::max(0.,elapsed)*rate);
 if(in_loop(id,p))return std::min(std::fmod(target,end),end-.000002);
 if(end>0&&p>=end)target=std::max(target,end+.000002);
 return target;
}
inline double drift(unsigned id,double current,double target){
 if(in_loop(id,current)!=in_loop(id,target))return std::numeric_limits<double>::infinity();
 if(in_loop(id,target)){const auto end=loop_end(id);double d=std::fmod(current-target+end/2,end);if(d<0)d+=end;return std::abs(d-end/2);}
 return std::abs(current-target);
}
inline Frame blend(const Frame& a,const Frame& b,double u){
 Frame out=a;if(a.profile!=b.profile)return out;
 u=std::clamp(u,0.,1.);const auto& layout=layouts[a.profile];
 for(unsigned i=0;i<layout.count;++i){const auto k=layout.kinds[i];double delta=b.inputs[i]-a.inputs[i];
  if(k=='r'){delta=std::fmod(delta+180,360);if(delta<0)delta+=360;delta-=180;}
  if(k=='r'||k=='v'||k=='f')out.inputs[i]=static_cast<float>(a.inputs[i]+delta*u);
 }return out;
}
}
