#include "qdcoop/death_state.hpp"
#include <cassert>
#include <cstdio>
namespace E=qdcoop::enemy;namespace D=E::death;
int main(){
 E::Envelope room{11,22,33,1};E::Packet packet{};D::Event event{7,3,1},out{};
 assert(D::encode(room,event,false,packet));assert(D::decode(room,packet,false,out));assert(!D::decode(room,packet,true,out));
 for(unsigned i=0;i<56;++i){auto bad=packet;bad.data[i]^=128;if(i<40||i>=48)assert(!D::decode(room,bad,false,out));}
 for(unsigned n=0;n<56;++n){auto bad=packet;bad.size=n;assert(!D::decode(room,bad,false,out));}
 auto wrong=room;++wrong.world;assert(!D::decode(wrong,packet,false,out));
 assert(!D::encode(room,{0,1,1},false,packet));assert(!D::encode(room,{1,0,1},false,packet));assert(!D::encode(room,{1,1,2},false,packet));
 D::Pending p;assert(p.begin(event,7));assert(!p.begin(event,7));assert(p.due(10));assert(!p.due(10.1));assert(p.due(10.21));
 assert(!p.acknowledge({7,4,1},0));assert(!p.acknowledge(event,3));assert(p.waiting==7);
 assert(p.acknowledge(event,1));assert(p.waiting==5);assert(p.acknowledge(event,1));assert(p.waiting==5);
 assert(p.acknowledge(event,0));assert(p.waiting==4);assert(p.due(11));assert(p.acknowledge(event,2));assert(!p.due(12));assert(p.terminal);
 std::puts("PASS death envelope bounds, epoch/generation guards, duplicate terminal reservation, lost ACK retries, independent three-peer acknowledgements");
}

