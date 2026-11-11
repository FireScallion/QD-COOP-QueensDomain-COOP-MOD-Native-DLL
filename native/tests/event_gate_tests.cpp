#include "qdcoop/event_gate.hpp"
#include <cassert>
#include <cstdio>
using namespace qdcoop::enemy;
int main(){
 EventGate invalid({0,1,1});assert(invalid.inspect({{0,1,1},1,EventKind::death})==Decision::forbidden);
 for(unsigned generation=1;generation<=1000;++generation){
  Identity id{7,1,generation};EventGate gate(id);
  Event animation{id,1,EventKind::animation},death{id,2,EventKind::death};
  auto malformed=animation;malformed.kind=static_cast<EventKind>(999);assert(gate.inspect(malformed)==Decision::forbidden);
  assert(gate.inspect(death)==Decision::gap);
  assert(gate.inspect(animation)==Decision::apply);
  assert(gate.inspect(animation)==Decision::apply); // failed engine apply must remain retryable
  assert(gate.commit(animation));assert(!gate.commit(animation));
  auto stale=death;stale.owner.world=6;assert(gate.inspect(stale)==Decision::stale);
  stale=death;stale.owner.generation++;assert(gate.inspect(stale)==Decision::stale);
  assert(gate.commit(death));assert(!gate.commit(death));
  Event late{id,3,EventKind::animation};assert(gate.inspect(late)==Decision::forbidden);
  late.kind=EventKind::despawn;assert(gate.commit(late));
  late.sequence=4;assert(gate.inspect(late)==Decision::forbidden);
 }
 std::puts("PASS 1000 generations: gaps, retry-before-commit, duplicate death, stale world/generation and terminal despawn");
}
