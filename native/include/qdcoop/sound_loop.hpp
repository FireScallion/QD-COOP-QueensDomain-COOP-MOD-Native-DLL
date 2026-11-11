#pragma once
#include "qdcoop/sound_stream.hpp"

namespace qdcoop::sound::loop {
inline constexpr unsigned kind=28,bytes=112,capacity=32;
enum class Phase : std::uint32_t {fading_in,playing,fading,stopped};
inline bool fading(Phase phase){return phase==Phase::fading_in||phase==Phase::fading;}
struct Fade {
    std::uint32_t revision{},curve{};
    float remaining{},target{};
    bool operator==(const Fade&) const=default;
};
struct Frame {
    Event voice;
    std::uint32_t id{},generation{};
    Phase phase{Phase::playing};
    Fade fade{};
    bool operator==(const Frame&) const=default;
};
inline bool valid(const Frame& frame){
    const auto& value=frame.voice;
    if(!frame.id||frame.id>capacity||!frame.generation||frame.phase>Phase::stopped||!valid_source(value))return false;
    const auto& fade=frame.fade;
    if(fading(frame.phase)){
        if(!fade.revision||fade.curve>3||!(fade.remaining>0.f&&fade.remaining<=10.f&&fade.target>=0.f&&fade.target<=8.f))
            return false;
    }else if(fade!=Fade{})return false;
    if(frame.phase==Phase::stopped)return !value.asset&&!value.attenuation&&value.position==std::array<float,3>{}&&
        value.volume==0.f&&value.pitch==0.f&&value.start==0.f;
    return valid_voice(value)&&assets::sounds[value.asset-1].looping;
}
inline bool encode(const E::Envelope& scope,const Frame& frame,E::Packet& packet){
    if(!scope.world||!valid(frame))return false;
    E::enrollment::write_header(scope,kind,bytes,packet);
    write_event_fields(frame.voice,packet);
    using E::animation::wire::put;
    put(packet,84,frame.id);
    put(packet,88,frame.generation);
    put(packet,92,static_cast<std::uint32_t>(frame.phase));
    put(packet,96,frame.fade.revision);
    put(packet,100,frame.fade.curve);
    put(packet,104,frame.fade.remaining);
    put(packet,108,frame.fade.target);
    return true;
}
inline bool decode(const E::Envelope& scope,const E::Packet& packet,Frame& output){
    if(!scope.world||!E::enrollment::check_header(scope,packet,kind,bytes))return false;
    using E::animation::wire::get;
    const Frame frame{read_event_fields(packet),get<std::uint32_t>(packet,84),get<std::uint32_t>(packet,88),
                      static_cast<Phase>(get<std::uint32_t>(packet,92)),
                      {get<std::uint32_t>(packet,96),get<std::uint32_t>(packet,100),
                       get<float>(packet,104),get<float>(packet,108)}};
    if(!valid(frame))return false;
    output=frame;
    return true;
}
struct Track {
    Frame last{};
    double received{};
    bool push(const Frame& frame,double time){
        if(!valid(frame)||!std::isfinite(time)||time<received||frame.generation<last.generation)return false;
        if(frame.generation==last.generation&&
           (frame.voice.serial<=last.voice.serial||frame.voice.slot!=last.voice.slot||frame.voice.origin!=last.voice.origin||
            frame.phase<last.phase||
            (fading(frame.phase)&&frame.phase==last.phase&&frame.fade.revision<last.fade.revision)||
            (frame.phase!=Phase::stopped&&frame.voice.asset!=last.voice.asset)))return false;
        last=frame;
        received=time;
        return true;
    }
    bool playing(double time) const {
        return last.voice.serial&&last.phase!=Phase::stopped&&std::isfinite(time)&&time>=received&&time-received<=.65;
    }
};
}
