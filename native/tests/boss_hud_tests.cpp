#include "qdcoop/boss_hud.hpp"
#include <cassert>
#include <cstdio>

int main(){
    namespace E=qdcoop::enemy;
    namespace H=qdcoop::boss_hud;
    constexpr E::Envelope scope{11,12,13,0},foreign{11,12,14,0};
    H::Frame state;
    state.sequence=1;
    state.count=2;
    state.targets[0]={1,7};
    state.targets[1]={18,3};
    state.group="/Game/Enemies/Boss/Group.Group";
    E::Packet packet;
    H::Frame decoded;
    assert(H::encode(scope,state,packet)&&H::decode(scope,packet,decoded));
    assert(decoded.sequence==1&&decoded.same_membership(state));
    assert(!H::decode(foreign,packet,decoded));
    E::animation::wire::put(packet,48,std::uint32_t{H::capacity+1});
    assert(!H::decode(scope,packet,decoded));
    state.targets[1]={1,8};
    assert(!H::encode(scope,state,packet));
    state.targets[1]={18,3};
    state.group="/Game/../Some.Some";
    assert(!H::encode(scope,state,packet));
    state={};
    state.sequence=2;
    assert(H::encode(scope,state,packet)&&H::decode(scope,packet,decoded)&&decoded.same_membership(state));
    std::puts("PASS scoped boss HUD membership, canonical targets, group path and explicit retirement");
}
