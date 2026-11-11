#pragma once
#include "qdcoop/player_pose.hpp"
#include <algorithm>
#include <numbers>

namespace qdcoop::player {
struct Locomotion {
    float walk_rate{},run_rate{},run_blend{},move_weight{},jump_blend{},jump_time{},turn{},stride{};
    float air_time{},landing{};
    bool was_air{};

    void advance(const Body& body,const std::array<float,3>& velocity,float yaw,double elapsed){
        const auto dt=static_cast<float>(std::clamp(elapsed,0.,.1));
        const auto speed=std::hypot(velocity[0],velocity[1]);
        const auto radians=yaw*(std::numbers::pi_v<float>/180.f);
        const auto forward=velocity[0]*std::cos(radians)+velocity[1]*std::sin(radians);
        const auto right=-velocity[0]*std::sin(radians)+velocity[1]*std::cos(radians);
        const auto reverse=forward<-std::abs(right)*.5f?-1.f:1.f;
        const auto angle=speed>8.f?std::clamp(std::atan2(right,std::abs(forward)+.001f)*
            (180.f/std::numbers::pi_v<float>),-45.f,45.f):0.f;
        const auto air=(body.flags&falling)!=0;
        const auto run=(body.flags&(running|dashing))!=0;
        const auto blend=1.f-std::exp(-20.f*dt);
        move_weight+=((speed>8.f?1.f:0.f)-move_weight)*(1.f-std::exp(-14.f*dt));
        run_blend+=(static_cast<float>(run)-run_blend)*blend;
        walk_rate=std::clamp(speed/body.walk_speed,.25f,1.8f)*reverse;
        run_rate=std::clamp(speed/(body.walk_speed*1.6f),.25f,1.8f)*reverse;
        stride=std::min(speed/body.walk_speed,1.f)*.45f;
        if(was_air&&!air)landing=.32f;
        was_air=air;
        air_time=air?air_time+dt:0.f;
        landing=std::max(0.f,landing-dt);
        jump_time=air?(velocity[2]>20.f?std::min(.6f,.15f+air_time*2.5f):1.25f):
            2.05f+(.32f-landing)*.7f;
        jump_blend=air?1.f:.60f*landing/.32f;
        turn+=((air?angle*.35f:landing>0?0.f:angle*move_weight)-turn)*blend;
    }
};

inline const SectionLoop* loop_for(unsigned clip,float position){
    for(const auto& loop:section_loops)
        if(loop.clip==clip&&position>=loop.start&&position<=loop.end+.02f)return &loop;
    return nullptr;
}

inline float advance_lane(const Lane& lane,double elapsed){
    const auto& clip=clips[lane.clip];
    auto position=lane.position;
    if(lane.playing)position+=static_cast<float>(std::clamp(elapsed,0.,.25))*lane.rate;
    if(const auto loop=loop_for(lane.clip,lane.position)){
        const auto span=loop->end-loop->start;
        if(position>=loop->end||position<loop->start){
            auto offset=std::fmod(position-loop->start,span);
            if(offset<0)offset+=span;
            position=loop->start+offset;
        }
    }
    return std::clamp(position,0.f,clip.length)*(clip.cooked/clip.length);
}
}
