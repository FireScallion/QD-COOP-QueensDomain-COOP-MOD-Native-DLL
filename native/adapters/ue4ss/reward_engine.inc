namespace RW=qdcoop::enemy::reward;
static RW::Inbox reward_inbox;static RW::Outbox reward_outbox;
static std::array<Weak,7> reward_metadata{};static bool reward_ready;
static int reward_save_offset,reward_xp_offset;
static std::uint64_t reward_applied{},reward_rejected{},reward_sent{},reward_xp_total{};
static bool reward_alive(){if(!reward_ready)return false;for(auto& w:reward_metadata)if(!api.weakget(&w)){reward_ready=false;return false;}return true;}
static bool reward_busy(){return reward_outbox.pending()||reward_inbox.pending()||reward_inbox.phase()!=RW::Phase::waiting;}
static void reward_ack(){E::Packet p{};if(RW::encode_ack(envelope,reward_inbox.applied(),p))send(p,1);}
static bool reward_begin();static bool reward_complete(unsigned created=0);
#include "bestiary_engine.inc"
#include "drop_engine.inc"
#include "receipt_engine.inc"
static bool reward_receive(const ReceivedPacket& received){
 const auto& p=received.packet;if(p.size<40)return false;const auto kind=A::wire::get<std::uint32_t>(p,32);
 if(host_role&&kind==7){std::uint64_t sequence{};if(!RW::decode_ack(envelope,p,sequence)||!reward_outbox.applied_ack(received.peer,sequence))++reward_rejected;return true;}
 if(!host_role&&kind==6){
  // No receipt or ACK before local save recovery. The authority retries.
  if(!receipt_ready)return true;
  RW::Event event{};if(received.peer!=0||!RW::decode(envelope,p,event)){++reward_rejected;return true;}
  auto result=reward_inbox.receive(event);if(result==RW::Result::conflict){reward_inbox.conflict();++reward_rejected;}else if(result!=RW::Result::accepted&&result!=RW::Result::duplicate)++reward_rejected;
  reward_ack();return true;
 }return false;
}
static void reward_pump(double time){
 if(host_role){if(!reward_outbox.pending())return;for(unsigned peer=0;peer<3;++peer)if(active_peer_mask&(1u<<peer))for(unsigned n=0;n<8;++n){auto e=reward_outbox.due(peer,time);if(!e)break;E::Packet p{};if(RW::encode(envelope,*e,p)&&send(p,1u<<peer))++reward_sent;}return;}
 receipt_poll();
 if(receipt_stage==ReceiptStage::saved_pending)return;
 if(!receipt_ready||reward_inbox.phase()!=RW::Phase::waiting||!reward_inbox.next()||!reward_alive())return;
 for(unsigned budget=0;budget<8;++budget){const auto* e=reward_inbox.next();if(!e)return;
  auto player=api.weakget(&reward_metadata[4]);auto gi=api.weakget(&reward_metadata[5]);auto save=api.weakget(&reward_metadata[6]);if(!player||!gi||!save)return;
  void* current{};std::memcpy(&current,static_cast<char*>(gi)+reward_save_offset,8);if(current!=save)return;
  unsigned char destroyed[256]{};api.event(player,getters[3].fn,destroyed);if(api.getbool(getters[3].ret,destroyed,0))return;
  if(e->kind==RW::Kind::bestiary){if(!bestiary_apply(*e,player,gi,save))return;continue;}
  if(e->kind==RW::Kind::drop){if(!drop_apply(*e,player,gi,save))return;continue;}
  if(e->kind!=RW::Kind::xp)return;
  std::int32_t before{};std::memcpy(&before,static_cast<char*>(save)+reward_xp_offset,4);if(before<0||before>INT32_MAX-e->amount)return;
  const auto amount=e->amount;if(!reward_begin())return;
  unsigned char args[5]{};std::memcpy(args,&amount,4);api.event(player,api.weakget(&reward_metadata[3]),args);
  // An engine call returning does not prove the side effect happened. Never
  // replay a partially applied/ambiguous award after a changed save or mismatch.
  if(api.weakget(&reward_metadata[5])!=gi){reward_inbox.uncertain();return;}
  std::memcpy(&current,static_cast<char*>(gi)+reward_save_offset,8);
  if(api.weakget(&reward_metadata[6])!=save||current!=save){reward_inbox.uncertain();return;}
  std::int32_t after{};std::memcpy(&after,static_cast<char*>(save)+reward_xp_offset,4);
  if(after!=before+amount){reward_inbox.uncertain();return;}
  if(!reward_complete())return;

 }
}
static bool reward_control(char op,char* p){
 auto number=[&](std::uint64_t& v){while(*p==' ')++p;if(*p<'0'||*p>'9')return false;char* end;v=std::strtoull(p,&end,10);if(end==p)return false;p=end;return true;};
 if(op=='j'||op=='s')return receipt_control(op,p);
 if(receipt_enabled&&(op=='w'||op=='b'||op=='l'))return false;
 if(op=='l'||op=='d')return drop_control(op,p);
 if(op=='b'||op=='a'||op=='k')return bestiary_control(op,p);
 if(op=='w'){
  std::array<std::uint64_t,9> a{};for(auto& n:a)if(!number(n))return false;if(*p)return false;
  auto ptr=[&](unsigned i){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(a[i]));};
  for(unsigned i=0;i<9;++i)if(!ptr(i))return false;
  if(*api.parms(ptr(3))!=5||!property(ptr(4),8,*api.propertiesSize(ptr(1)))||!property(ptr(5),4,*api.propertiesSize(ptr(2))))return false;
  for(unsigned i=0;i<3;++i)if(!api.isa(ptr(6+i),ptr(i)))return false;
  void* actual{};std::memcpy(&actual,static_cast<char*>(ptr(7))+*api.offset(ptr(4)),8);if(actual!=ptr(8))return false;
  std::array<Weak,7> weak{};for(unsigned i=0;i<4;++i)if(!makeweak(weak[i],ptr(i)))return false;for(unsigned i=0;i<3;++i)if(!makeweak(weak[4+i],ptr(6+i)))return false;
  reward_metadata=weak;reward_save_offset=*api.offset(ptr(4));reward_xp_offset=*api.offset(ptr(5));reward_ready=true;return true;
 }
 if(op=='q'&&host_role){std::uint64_t amount{};if(!number(amount)||*p||!amount||amount>10000000)return false;RW::Event e{};e.kind=RW::Kind::xp;e.amount=static_cast<std::int32_t>(amount);const auto peers=active_peer_mask;return reward_outbox.enqueue(e,peers);}
 return false;
}
