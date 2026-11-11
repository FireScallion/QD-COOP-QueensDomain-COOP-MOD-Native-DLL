#include "qdcoop/sound_stream.hpp"
#include "qdcoop/sound_loop.hpp"
#include "qdcoop/enemy_damage.hpp"
#include <cassert>
#include <cstdio>
#include <limits>

int main(){
    namespace S=qdcoop::sound;
    namespace E=qdcoop::enemy;
    constexpr auto cue=S::assets::sound_id(
        "/Game/Items/Weapons/Daggers/HomingDagger/HomingDaggerThrowSound.HomingDaggerThrowSound");
    constexpr auto attenuation=S::assets::attenuation_id(
        "/Game/Player/Assets/Audio/CharacterAudioAtten.CharacterAudioAtten");
    static_assert(cue&&attenuation);
    const E::Envelope scope{1,2,3,0},other_world{1,2,4,0};
    S::Event event{21,qdcoop::player::unassigned,S::Origin::player,cue,attenuation,
        {19.5f,-5.f,112.f},.075f,.88f,.035f};
    E::Packet packet;
    S::Event decoded;
    assert(S::encode(scope,event,packet)&&S::decode(scope,packet,decoded)&&decoded==event);
    assert(!S::decode(other_world,packet,decoded));
    unsigned relay{};
    assert(S::accept_origin(decoded,true,1,7,relay)&&decoded.slot==2&&relay==5);
    assert(S::encode(scope,decoded,packet)&&S::decode(scope,packet,decoded));
    assert(S::accept_origin(decoded,false,0,1,relay)&&!relay);
    assert(!S::accept_origin(decoded,false,1,1,relay));
    event.origin=S::Origin::enemy;
    assert(!S::encode(scope,event,packet)&&!S::accept_origin(event,true,1,7,relay));
    event.slot=0;
    assert(S::encode(scope,event,packet)&&S::decode(scope,packet,decoded)&&decoded==event);
    assert(!S::accept_origin(decoded,true,0,7,relay));
    assert(S::accept_origin(decoded,false,0,1,relay)&&!relay);
    E::animation::wire::put(packet,60,std::numeric_limits<float>::infinity());
    assert(!S::decode(scope,packet,decoded));
    auto bad=event;
    bad.asset=UINT16_MAX;
    assert(!S::encode(scope,bad,packet));
    bad=event;
    bad.attenuation=UINT16_MAX;
    assert(!S::encode(scope,bad,packet));
    bad=event;
    bad.pitch=0.f;
    assert(!S::encode(scope,bad,packet));
    const auto loop=std::find_if(S::assets::sounds.begin(),S::assets::sounds.end(),
        [](const S::assets::Sound& sound){return sound.looping;});
    assert(loop!=S::assets::sounds.end());
    bad=event;
    bad.asset=static_cast<std::uint16_t>(loop-S::assets::sounds.begin()+1);
    assert(!S::encode(scope,bad,packet));
    E::damage::ReplayWindow window;
    assert(!window.known(21));
    window.accept(21);
    assert(window.known(21)&&!window.known(20));
    window.accept(20);
    assert(window.known(20)&&window.known(21));
    S::Control control{21,qdcoop::player::unassigned,S::Origin::player,1,.7f,.15f,2},decoded_control;
    assert(S::encode(scope,control,packet)&&S::decode(scope,packet,decoded_control)&&decoded_control==control);
    assert(!S::decode(other_world,packet,decoded_control));
    assert(S::accept_origin(decoded_control,true,1,7,relay)&&decoded_control.slot==2&&relay==5);
    S::ControlHistory controls;
    assert(controls.push(decoded_control)&&!controls.push(decoded_control)&&controls.find(21));
    const S::Control stop{21,2,S::Origin::player,2};
    assert(controls.push(stop)&&!controls.push(decoded_control)&&!controls.push(stop));
    assert(S::encode(scope,stop,packet)&&S::decode(scope,packet,decoded_control)&&decoded_control==stop);
    auto forged=control;
    forged.origin=S::Origin::enemy;
    assert(!S::encode(scope,forged,packet)&&!S::accept_origin(forged,true,1,7,relay));
    forged=control;
    forged.duration=std::numeric_limits<float>::quiet_NaN();
    assert(!S::encode(scope,forged,packet));
    namespace L=S::loop;
    L::Frame loop_frame{bad,4,2,L::Phase::playing},decoded_loop;
    assert(L::encode(scope,loop_frame,packet)&&L::decode(scope,packet,decoded_loop)&&decoded_loop==loop_frame);
    assert(!L::decode(other_world,packet,decoded_loop)&&!S::decode(scope,packet,decoded));
    assert(!S::accept_origin(decoded_loop.voice,true,1,7,relay));
    assert(S::accept_origin(decoded_loop.voice,false,0,1,relay));
    L::Track track;
    auto fade_in=loop_frame;
    fade_in.voice.serial=20;
    fade_in.phase=L::Phase::fading_in;
    fade_in.fade={1,1,.85f,1.f};
    assert(L::encode(scope,fade_in,packet)&&L::decode(scope,packet,decoded_loop)&&decoded_loop==fade_in);
    assert(track.push(fade_in,.9)&&track.playing(.95));
    assert(track.push(loop_frame,1.)&&track.playing(1.6)&&!track.playing(1.66));
    fade_in.voice.serial=22;
    assert(!track.push(fade_in,1.15));
    assert(!track.push(loop_frame,1.2));
    auto stale=loop_frame;
    auto fade=loop_frame;
    fade.voice.serial=22;
    fade.phase=L::Phase::fading;
    fade.fade={1,2,.7f,0.f};
    assert(L::encode(scope,fade,packet)&&L::decode(scope,packet,decoded_loop)&&decoded_loop==fade);
    assert(track.push(fade,1.25)&&track.playing(1.26));
    auto old_fade=fade;
    old_fade.voice.serial=23;
    old_fade.fade.curve=4;
    assert(!L::encode(scope,old_fade,packet));
    auto replacement=fade;
    replacement.voice.serial=23;
    replacement.fade.revision=2;
    assert(track.push(replacement,1.27));
    old_fade=fade;
    old_fade.voice.serial=24;
    assert(!track.push(old_fade,1.28));
    stale.voice.serial=24;
    assert(!track.push(stale,1.28));
    const L::Frame stopped{{25,0,S::Origin::enemy},4,2,L::Phase::stopped};
    assert(L::encode(scope,stopped,packet)&&L::decode(scope,packet,decoded_loop)&&decoded_loop==stopped);
    assert(track.push(stopped,1.3)&&!track.playing(1.3));
    stale.voice.serial=26;
    assert(!track.push(stale,1.4));
    stale.generation=3;
    stale.voice.serial=1;
    assert(track.push(stale,1.4)&&track.playing(1.4)&&!track.push(stopped,1.5));
    std::puts("PASS native looping sound lease, scoped retirement and generation reuse");
    std::puts("PASS native one-shot sound scope, asset whitelist, source authority and reordered retries");
}
