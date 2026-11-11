#include "qdcoop/reward_worker_windows.hpp"
#include <cassert>
#include <cstdio>
namespace R=qdcoop::enemy::reward;namespace J=R::journal;
struct Fake {
 struct State{std::uint64_t a{},s{};auto applied()const{return a;}auto saved()const{return s;}} snapshot;
 HANDLE gate;std::atomic<DWORD>* owner;bool fail;
 Fake(HANDLE g,std::atomic<DWORD>* o,bool f=false):gate(g),owner(o),fail(f){}
 bool begin(const R::Event&){owner->store(GetCurrentThreadId());return WaitForSingleObject(gate,4000)==WAIT_OBJECT_0&&!fail;}
 bool applied(std::uint64_t sequence){assert(owner->load()==GetCurrentThreadId());snapshot.a=sequence;return true;}
 bool saved(const J::SaveProof&){assert(owner->load()==GetCurrentThreadId());snapshot.s=snapshot.a;return true;}
 const State& state()const{return snapshot;}
};
static J::Completion wait(J::Worker<Fake>& worker){J::Completion result{};const auto deadline=GetTickCount64()+5000;while(!worker.poll(result)){assert(GetTickCount64()<deadline);Sleep(1);}return result;}
int main(){
 R::Event event{};event.sequence=1;event.kind=R::Kind::xp;event.amount=42;HANDLE gate=CreateEventW(nullptr,TRUE,FALSE,nullptr);assert(gate);std::atomic<DWORD> owner{};
 {J::Worker<Fake> worker;assert(worker.start(std::make_unique<Fake>(gate,&owner)));assert(worker.begin(event));assert(!worker.idle()&&!worker.applied(1));
  const auto deadline=GetTickCount64()+2000;while(!owner){assert(GetTickCount64()<deadline);Sleep(1);}assert(owner!=GetCurrentThreadId());J::Completion none{};assert(!worker.poll(none));assert(SetEvent(gate));auto done=wait(worker);assert(done.ok&&done.operation==J::Operation::begin&&done.sequence==1&&done.thread!=GetCurrentThreadId());
  assert(worker.applied(1));done=wait(worker);assert(done.ok&&done.applied==1&&done.saved==0);assert(worker.saved(1,{}));done=wait(worker);assert(done.ok&&done.saved==1&&worker.idle());
 }
 {J::Worker<Fake> worker;assert(worker.start(std::make_unique<Fake>(gate,&owner,true)));assert(worker.begin(event));auto done=wait(worker);assert(!done.ok&&!worker.idle()&&!worker.begin(event)&&!worker.applied(1));}
 CloseHandle(gate);std::puts("PASS journal worker: blocked storage leaves caller free, single-flight backpressure, value-only completion ordering, off-thread ownership and fail-closed storage error");return 0;
}
