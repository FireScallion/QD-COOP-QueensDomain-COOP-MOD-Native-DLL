#pragma once
#include "qdcoop/reward_journal_windows.hpp"
#include <atomic>
#include <memory>
namespace qdcoop::enemy::reward::journal {
enum class Operation{begin,applied,saved};
struct Completion{Operation operation{};std::uint64_t sequence{},applied{},saved{},microseconds{};bool ok{};DWORD thread{};};
// One bounded request at a time. Only the worker owns/touches Store after start.
// Messages contain values only; no engine objects or callbacks cross threads.
template<class Store=File>class Worker {
 enum class Phase{idle,queued,running,done};
 std::atomic<Phase> phase_{Phase::idle};std::atomic<bool> stopping_{},failed_{};
 HANDLE wake_{},thread_{};std::unique_ptr<Store> store_;
 Operation operation_{};Event event_{};std::uint64_t sequence_{};SaveProof proof_{};Completion result_{};
 static DWORD WINAPI entry(void* context){static_cast<Worker*>(context)->run();return 0;}
 void run(){
  LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
  for(;;){if(WaitForSingleObject(wake_,INFINITE)!=WAIT_OBJECT_0){failed_=true;return;}
   if(phase_.load(std::memory_order_acquire)==Phase::queued){
    phase_.store(Phase::running,std::memory_order_release);LARGE_INTEGER start{},end{};QueryPerformanceCounter(&start);
    bool ok=operation_==Operation::begin?store_->begin(event_):operation_==Operation::applied?store_->applied(sequence_):store_->saved(proof_);
    QueryPerformanceCounter(&end);result_={operation_,sequence_,store_->state().applied(),store_->state().saved(),static_cast<std::uint64_t>((end.QuadPart-start.QuadPart)*1000000/frequency.QuadPart),ok,GetCurrentThreadId()};
    if(!ok){failed_.store(true,std::memory_order_release);}phase_.store(Phase::done,std::memory_order_release);
   }
   if(stopping_.load(std::memory_order_acquire))return;
  }
 }
 bool submit(Operation operation,std::uint64_t sequence,const Event& event,const SaveProof& proof){
  if(!thread_||stopping_||failed_||phase_.load(std::memory_order_acquire)!=Phase::idle)return false;
  operation_=operation;sequence_=sequence;event_=event;proof_=proof;phase_.store(Phase::queued,std::memory_order_release);
  if(!SetEvent(wake_)){failed_=true;return false;}return true;
 }
public:
 Worker()=default;Worker(const Worker&)=delete;Worker& operator=(const Worker&)=delete;
 ~Worker(){stopping_=true;if(wake_)SetEvent(wake_);if(thread_){WaitForSingleObject(thread_,INFINITE);CloseHandle(thread_);}if(wake_)CloseHandle(wake_);}
 bool start(std::unique_ptr<Store> store){
  if(thread_||wake_||!store){return false;}wake_=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!wake_)return false;
  store_=std::move(store);thread_=CreateThread(nullptr,0,entry,this,0,nullptr);if(!thread_){CloseHandle(wake_);wake_=nullptr;return false;}return true;
 }
 bool begin(const Event& event){return submit(Operation::begin,event.sequence,event,{});}
 bool applied(std::uint64_t sequence){return submit(Operation::applied,sequence,{},{});}
 bool saved(std::uint64_t sequence,const SaveProof& proof){return submit(Operation::saved,sequence,{},proof);}
 bool poll(Completion& out){if(phase_.load(std::memory_order_acquire)!=Phase::done)return false;out=result_;phase_.store(Phase::idle,std::memory_order_release);return true;}
 bool idle()const{return thread_&&phase_.load(std::memory_order_acquire)==Phase::idle&&!failed_;}
};
}
