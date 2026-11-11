#include "qdcoop/reward_stream.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include <random>
#include <string>
namespace E=qdcoop::enemy;namespace R=E::reward;
static R::Event example(unsigned n){R::Event e{};e.sequence=n;e.kind=R::Kind::xp;e.amount=17;return e;}
int main(){
 E::Envelope room{1,2,3,1};E::Packet p{};R::Event decoded{};
 auto e=example(1);assert(R::encode(room,e,p)&&R::decode(room,p,decoded)&&decoded==e);
 e.kind=R::Kind::drop;e.amount=0;e.entity=7;e.generation=9;e.flags=63|R::drop_mark_first;e.counts={-1,10000};
 assert(e.text[0].set("/Game/Enemies/Fisher/Fisher_Data.Fisher_Data"));assert(e.text[1].set("EnemyReward"));assert(e.text[2].set("/Game/Items/IronFruit.IronFruit"));e.values[12]=.75f;e.chances={.1f,2,.2f,3};
 assert(R::encode(room,e,p)&&R::decode(room,p,decoded)&&decoded==e);
 auto bad=e;bad.counts[0]=-2;assert(!R::valid(bad));bad=e;bad.chances[0]=std::numeric_limits<float>::quiet_NaN();assert(!R::valid(bad));bad=e;bad.text[2].set("/Script/Injected");assert(!R::valid(bad));bad=e;bad.text[1].set("bad\nname");assert(!R::valid(bad));bad=e;bad.flags|=128;assert(!R::valid(bad));
 for(unsigned size=0;size<p.size;++size){auto packet=p;packet.size=size;assert(!R::decode(room,packet,decoded));}
 auto packet=p;++packet.size;assert(!R::decode(room,packet,decoded));auto other=room;++other.world;assert(!R::decode(other,p,decoded));
 for(auto& t:e.text){std::string value="/Game/";value.append(294,'a');assert(t.set(value));}assert(R::encode(room,e,p)&&p.size==1356&&R::decode(room,p,decoded)&&decoded==e);
 packet=p;E::animation::wire::put(packet,148,std::uint16_t{301});assert(!R::decode(room,packet,decoded));
 std::uint64_t acknowledged{};assert(R::encode_ack(room,17,p));assert(R::decode_ack(room,p,acknowledged)&&acknowledged==17);assert(!R::decode_ack(other,p,acknowledged));assert(!R::encode_ack(room,0,p));p.size=47;assert(!R::decode_ack(room,p,acknowledged));
 R::Inbox inbox;assert(inbox.receive(example(2))==R::Result::accepted);assert(!inbox.begin());assert(inbox.receive(example(1))==R::Result::accepted);assert(inbox.begin());assert(!inbox.begin());assert(inbox.receive(example(1))==R::Result::duplicate);assert(inbox.uncertain());assert(!inbox.begin()&&!inbox.complete()&&inbox.applied()==0);assert(inbox.phase()==R::Phase::quarantined);
 R::Inbox conflict;assert(conflict.receive(example(1))==R::Result::accepted);bad=example(1);bad.amount=18;assert(conflict.receive(bad)==R::Result::conflict);
 unsigned deliveries=0;
 for(unsigned seed=1;seed<=12;++seed){
  R::Outbox out;std::array<R::Inbox,3> receivers{};std::array<unsigned,3> calls{};std::mt19937 rng(seed);unsigned produced=0;
  for(unsigned tick=1;tick<60000;++tick){
   if(produced<500&&out.enqueue(example(1),7))++produced;
   for(unsigned peer=0;peer<3;++peer){
    if(auto event=out.due(peer,tick*.01)){if(rng()%100>=35){assert(R::encode(room,*event,p)&&R::decode(room,p,decoded));auto result=receivers[peer].receive(decoded);assert(result==R::Result::accepted||result==R::Result::duplicate||result==R::Result::gap);++deliveries;}}
    auto& receiver=receivers[peer];if(receiver.next()&&rng()%100>=20){assert(receiver.begin());++calls[peer];assert(receiver.complete());}
    if(receiver.applied()&&rng()%100>=35)assert(out.applied_ack(peer,receiver.applied()));
   }
   if(produced==500&&!out.pending())break;
  }
  assert(produced==500&&out.pending()==0);for(auto n:calls)assert(n==500);
 }
 R::Outbox bounded;for(unsigned i=0;i<64;++i)assert(bounded.enqueue(example(1),7));assert(!bounded.enqueue(example(1),7));assert(!bounded.applied_ack(3,1));assert(!bounded.applied_ack(0,65));assert(bounded.applied_ack(0,64)&&bounded.pending()==64);assert(bounded.applied_ack(1,64)&&bounded.pending()==64);assert(bounded.applied_ack(2,64)&&bounded.pending()==0);
 std::printf("PASS reward path/count/NaN/truncation validation, maximum 1356-byte packet, conflict/quarantine guards; 12 x 3 x 500 single applications under 35%% packet/ACK loss, %u deliveries\n",deliveries);
}

