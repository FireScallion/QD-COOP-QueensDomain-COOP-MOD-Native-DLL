#include "qdcoop/visual_mount.hpp"
#include <cassert>
#include <cstdio>

int main(){
    namespace E=qdcoop::enemy;
    namespace M=qdcoop::visual_mount;
    constexpr E::Envelope scope{1,2,3,0},foreign{1,2,4,0};
    M::Frame frame;
    frame.stream=M::capacity;
    frame.generation=17;
    frame.owner=23;
    frame.owner_generation=9;
    frame.sequence=42;
    frame.active=frame.visible=frame.has_dissolve=true;
    frame.asset=1;
    frame.anchor=M::Anchor::bone;
    frame.name="WeaponEffect";
    frame.socket="hand_r";
    frame.relative={0,0,0,1,117,9,48,.2f,.2f,.2f};
    frame.materials[0]=11;
    frame.dissolve=.375f;
    E::Packet packet;
    M::Frame decoded;
    assert(M::encode(scope,frame,packet)&&M::decode(scope,packet,decoded)&&decoded==frame);
    assert(!M::decode(foreign,packet,decoded));
    auto invalid=frame;
    invalid.materials[3]=UINT16_MAX;
    assert(!M::encode(scope,invalid,packet));
    invalid=frame;
    invalid.socket="/Game/Unknown";
    assert(!M::encode(scope,invalid,packet));
    frame.player=true;
    frame.owner=5;
    frame.anchor=M::Anchor::hand_left;
    frame.socket.clear();
    assert(M::encode(scope,frame,packet)&&M::decode(scope,packet,decoded)&&decoded==frame);
    frame.owner=6;
    assert(!M::encode(scope,frame,packet));
    frame.owner=4;
    frame.anchor=M::Anchor::mesh;
    assert(!M::encode(scope,frame,packet));
    frame.player=false;
    frame.anchor=M::Anchor::world;
    frame.relative[4]=250000.f;
    assert(M::encode(scope,frame,packet)&&M::decode(scope,packet,decoded)&&decoded==frame);
    frame.anchor=M::Anchor::mesh;
    assert(!M::encode(scope,frame,packet));
    std::puts("PASS scoped visual mounts, stock assets, socket labels and player anchors");
}
