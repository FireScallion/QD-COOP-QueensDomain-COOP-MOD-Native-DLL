#include "qdcoop/animation_state.hpp"
#include <cassert>
#include <cstdio>
using namespace qdcoop::enemy;
namespace A=animation;
int main(){
 Envelope e{1,2,3,3};Packet packet;A::Frame f;f.id=1;f.generation=1;f.sequence=1;f.time=12;
 for(unsigned profile=1;profile<std::size(A::layouts);++profile){f.profile=profile;A::Frame decoded;
  assert(A::encode(e,f,packet));assert(A::decode(e,packet,decoded));assert(decoded.profile==profile);
  auto wrong=e;wrong.world++;assert(!A::decode(wrong,packet,decoded));
  auto short_packet=packet;--short_packet.size;assert(!A::decode(e,short_packet,decoded));
  for(unsigned asset=1;asset<std::size(A::montages);++asset){f.montage=asset;assert(A::valid(f)==(A::montages[asset]==A::layouts[profile].skeleton));}f.montage=0;
 }
 for(unsigned id:{33u,50u,51u}){const auto end=A::loop_end(id);assert(end>0);
  assert(!A::restart(id,id,end-.01,.01));assert(A::restart(id,id,end-.01,end+.01));
  assert(A::advance(id,end-.01,.1,1)<end);assert(A::advance(id,end,.1,1)>end);
  assert(A::drift(id,end-.01,.01)<.021);assert(std::isinf(A::drift(id,end-.01,end+.01)));
 }
 f.profile=3;f.inputs[0]=179;auto b=f;b.inputs[0]=-179;assert(std::abs(A::blend(f,b,.5).inputs[0]-180)<.001);
 f.inputs[3]=2;assert(!A::valid(f));f.inputs[3]=0;f.rate=std::numeric_limits<float>::quiet_NaN();assert(!A::valid(f));
 std::puts("PASS all 48 profile codecs, 31056 montage/skeleton combinations, loop/restart/drift and typed interpolation");
}
