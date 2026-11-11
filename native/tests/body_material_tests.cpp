#include "qdcoop/body_material.hpp"
#include <cassert>
#include <cstdio>

int main(){
    namespace E=qdcoop::enemy;
    namespace B=qdcoop::body_material;
    constexpr E::Envelope scope{1,2,3,0},foreign{1,2,4,0};
    const B::Frame dissolve{E::capacity,17,42,.375f,true,{1,2}};
    E::Packet packet;
    B::Frame decoded;
    assert(B::encode(scope,dissolve,packet)&&B::decode(scope,packet,decoded)&&decoded==dissolve);
    assert(!B::decode(foreign,packet,decoded));
    auto invalid=dissolve;
    invalid.active=false;
    assert(!B::encode(scope,invalid,packet));
    invalid.dissolve=0.f;
    assert(!B::encode(scope,invalid,packet));
    invalid.materials={};
    assert(B::encode(scope,invalid,packet)&&B::decode(scope,packet,decoded)&&decoded==invalid);
    E::animation::wire::put(packet,60,std::uint32_t{2});
    assert(!B::decode(scope,packet,decoded));
    assert(B::encode(scope,dissolve,packet));
    E::animation::wire::put(packet,56,std::uint32_t{0x7fc00000});
    assert(!B::decode(scope,packet,decoded));
    std::puts("PASS scoped enemy material frames, scalar bounds and explicit reset");
}
