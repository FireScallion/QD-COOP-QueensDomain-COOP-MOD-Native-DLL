#pragma once
#include <cstdint>
namespace qdcoop::enemy {
struct Identity { std::uint64_t world; std::uint32_t entity,generation; };
enum class EventKind { animation, death, despawn };
struct Event { Identity owner; std::uint64_t sequence; EventKind kind; };
enum class Decision { apply, duplicate, stale, gap, forbidden };
// One game-thread owner. Commit only after the engine adapter succeeds.
// Delivery/ACK persistence and UObject ownership are deliberately external.
class EventGate {
 Identity owner_; std::uint64_t committed_{}; bool dead_{},gone_{};
public:
 explicit EventGate(Identity owner):owner_(owner){}
 Decision inspect(const Event& e) const {
  if(!owner_.world||!owner_.entity||!owner_.generation)return Decision::forbidden;
  if(e.kind!=EventKind::animation&&e.kind!=EventKind::death&&e.kind!=EventKind::despawn)return Decision::forbidden;
  if(e.owner.world!=owner_.world||e.owner.entity!=owner_.entity||e.owner.generation!=owner_.generation)return Decision::stale;
  if(!e.sequence)return Decision::forbidden;
  if(e.sequence<=committed_)return Decision::duplicate;
  if(e.sequence-committed_!=1)return Decision::gap;
  if(gone_||(dead_&&e.kind!=EventKind::despawn))return Decision::forbidden;
  return Decision::apply;
 }
 bool commit(const Event& e){
  if(inspect(e)!=Decision::apply)return false;
  committed_=e.sequence;dead_=dead_||e.kind==EventKind::death;
  gone_=e.kind==EventKind::despawn;return true;
 }
};
}
