#include "qdcoop/enemy_stream.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace qdcoop::enemy {
static constexpr unsigned header=40, state_bytes=124;
static_assert(std::endian::native==std::endian::little);
static void u32(std::uint8_t* p,std::uint32_t v){std::memcpy(p,&v,4);}
static void u64(std::uint8_t* p,std::uint64_t v){std::memcpy(p,&v,8);}
static std::uint32_t r32(const std::uint8_t* p){std::uint32_t v;std::memcpy(&v,p,4);return v;}
static std::uint64_t r64(const std::uint8_t* p){std::uint64_t v;std::memcpy(&v,p,8);return v;}
static void head(const Envelope& e,Packet& p,unsigned bytes){
    p={};p.size=header+bytes;u32(p.data.data(),0x45344e51);u32(p.data.data()+4,1);
    u64(p.data.data()+8,e.room_high);u64(p.data.data()+16,e.room_low);u64(p.data.data()+24,e.world);
    u32(p.data.data()+32,e.kind);u32(p.data.data()+36,bytes);
}
static bool check(const Envelope& e,const Packet& p,unsigned kind){
    return p.size>=header&&p.size<=packet_capacity&&r32(p.data.data())==0x45344e51&&r32(p.data.data()+4)==1&&
        r64(p.data.data()+8)==e.room_high&&r64(p.data.data()+16)==e.room_low&&r64(p.data.data()+24)==e.world&&
        r32(p.data.data()+32)==kind&&r32(p.data.data()+36)==p.size-header;
}
bool finite(const State& s){
    if(!s.id||s.id>capacity||!s.generation||!s.sequence||!std::isfinite(s.time)||s.time<0)return false;
    for(float f:s.v)if(!std::isfinite(f)||std::abs(f)>1e7f)return false;
    for(unsigned i=1;i<=5;++i)if(s.v[i]!=0&&s.v[i]!=1)return false;
    const double norm=double(s.v[18])*s.v[18]+double(s.v[19])*s.v[19]+double(s.v[20])*s.v[20]+double(s.v[21])*s.v[21];
    return s.v[0]>=0&&norm>.5&&norm<1.5;
}
bool encode(const Envelope& e,const State& s,Packet& p){
    if(!finite(s))return false;
    auto n=e;n.kind=1;head(n,p,state_bytes);auto* b=p.data.data()+header;
    u32(b,s.id);u32(b+4,s.generation);u64(b+8,s.sequence);u64(b+16,std::bit_cast<std::uint64_t>(s.time));
    for(unsigned i=0;i<25;++i)u32(b+24+i*4,std::bit_cast<std::uint32_t>(s.v[i]));
    return true;
}
bool decode(const Envelope& e,const Packet& p,State& s){
    if(!check(e,p,1)||p.size!=header+state_bytes)return false;
    const auto* b=p.data.data()+header;State out{};out.id=r32(b);out.generation=r32(b+4);out.sequence=r64(b+8);out.time=std::bit_cast<double>(r64(b+16));
    for(unsigned i=0;i<25;++i)out.v[i]=std::bit_cast<float>(r32(b+24+i*4));
    if(!finite(out))return false;
    s=out;return true;
}
bool text_packet(const Envelope& e,std::span<const std::uint8_t> bytes,Packet& p){
    if(bytes.empty()||bytes.size()>packet_capacity-header)return false;
    auto n=e;n.kind=2;head(n,p,static_cast<unsigned>(bytes.size()));std::memcpy(p.data.data()+header,bytes.data(),bytes.size());return true;
}
bool text_payload(const Envelope& e,const Packet& p,std::span<const std::uint8_t>& out){
    if(!check(e,p,2))return false;
    out={p.data.data()+header,p.size-header};return true;
}
float position(float a,float b,float va,float vb,double span,double u){
    const double d=double(b)-a;if(std::abs(d)<1e-6)return a;
    double x=std::clamp(va*span/d,0.,3.),y=std::clamp(vb*span/d,0.,3.);const auto n=x*x+y*y;
    if(n>9){const auto scale=3/std::sqrt(n);x*=scale;y*=scale;}
    const auto u2=u*u,u3=u2*u;
    return static_cast<float>((2*u3-3*u2+1)*a+(u3-2*u2+u)*x*d+(-2*u3+3*u2)*b+(u3-u2)*y*d);
}
void Clock::push(double source,double now){
    if(initialized&&source<=latest)return;
    if(initialized){const auto gap=source-latest,arrival=now-arrived;
        if(gap<.5){const auto steps=std::max(1.,std::floor(gap/interval+.5));interval+=(gap/steps-interval)*.08;jitter+=(std::abs(arrival-gap)-jitter)*.1;}}
    const auto measured=now-source;offset=initialized?std::min(measured,offset+.0001):measured;
    latest=source;arrived=now;initialized=true;delay=std::clamp(interval*1.4+jitter*3+.02,.14,.30);
}
bool Timeline::push(const State& f,double now){
    const auto* prev=latest();
    if(prev&&(f.sequence<=prev->sequence||f.time<=prev->time)){++stale;return false;}
    if(prev){double distance=0;for(unsigned i=6;i<9;++i){const auto d=double(f.v[i])-prev->v[i];distance+=d*d;}
        if(f.generation!=prev->generation||f.time-prev->time>.75||distance>1000.*1000.){
            begin_=count_=0;playing_=false;interval_=jitter_=0;delay_=.16;prev=nullptr;++teleports;}}
    if(prev){const auto gap=f.time-prev->time,arrival=now-arrived_;interval_=interval_?interval_+(std::min(gap,.3)-interval_)*.15:gap;
        jitter_+=(std::abs(arrival-gap)-jitter_)*.1;delay_=std::clamp(interval_*1.35+jitter_*2+.025,.14,.35);}
    if(count_==history){begin_=(begin_+1)%history;--count_;}
    frames_[(begin_+count_++)%history]=f;arrived_=now;++accepted;return true;
}
static float angle(float a,float b,double u){double d=std::fmod(double(b)-a+180,360);if(d<0)d+=360;return static_cast<float>(a+(d-180)*u);}
bool Timeline::sample(const Clock& clock,double now,Sample& out){
    if(!count_||!clock.initialized)return false;
    const auto target=now-clock.offset-std::max(clock.delay,delay_),dt=std::max(0.,now-sample_at_);
    if(!playing_)play_=target;else if(dt>.25)play_=std::max(play_,target);else play_+=dt*std::clamp(1+(target-play_-dt)*4,.85,1.15);
    playing_=true;sample_at_=now;play_=std::min(play_,latest()->time+.1);
    while(count_>2&&frames_[(begin_+1)%history].time<=play_){begin_=(begin_+1)%history;--count_;}
    const State* a=&frames_[begin_];const State* b=count_>1?&frames_[(begin_+1)%history]:nullptr;
    if(b&&play_>=b->time){a=b;b=nullptr;}
    const auto u=b?std::clamp((play_-a->time)/(b->time-a->time),0.,1.):0.;out={*a,0,play_};
    auto& v=out.state.v;
    if(b&&play_>a->time){
        const auto span=b->time-a->time;out.kind=1;
        for(unsigned i=6;i<9;++i)v[i]=position(a->v[i],b->v[i],a->v[i+6],b->v[i+6],span,u);
        for(unsigned i=9;i<12;++i)v[i]=angle(a->v[i],b->v[i],u);
    }else if(!b&&play_>a->time&&a->v[0]>0){out.kind=2;for(unsigned i=6;i<9;++i)v[i]+=a->v[i+6]*static_cast<float>(std::min(play_-a->time,.1));}
    if(b){
        for(unsigned i=15;i<18;++i)v[i]=static_cast<float>(a->v[i]+(b->v[i]-a->v[i])*u);
        for(unsigned i=22;i<25;++i)v[i]=static_cast<float>(a->v[i]+(b->v[i]-a->v[i])*u);
        double dot=0;for(unsigned i=18;i<22;++i)dot+=double(a->v[i])*b->v[i];const auto sign=dot<0?-1.:1.;dot=std::abs(dot);
        double x=1-u,y=u*sign;
        if(dot<=.9995){const auto t=std::acos(std::clamp(dot,0.,1.)),den=std::sin(t);x=std::sin((1-u)*t)/den;y=std::sin(u*t)/den*sign;}
        double norm=0;for(unsigned i=18;i<22;++i){v[i]=static_cast<float>(a->v[i]*x+b->v[i]*y);norm+=double(v[i])*v[i];}
        if(norm<1e-8)return false;
        for(unsigned i=18;i<22;++i)v[i]/=static_cast<float>(std::sqrt(norm));
    }
    for(unsigned i=0;i<3;++i){const auto linear=a->v[i+6]+(b?(b->v[i+6]-a->v[i+6])*u:0);v[i+15]+=static_cast<float>(v[i+6]-linear);}
    return true;
}
}
