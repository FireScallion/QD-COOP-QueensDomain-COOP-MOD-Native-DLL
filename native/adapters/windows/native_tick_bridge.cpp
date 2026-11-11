// Pinned Steam Win64 GameEngine virtual Tick. No UE4SS or C++ library ABI.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <cstring>
#include "verified_game.inc"
using EngineTick=void(*)(void*,float,bool);
using Tick=void(*)(float);
static EngineTick original;
static std::atomic<EngineTick> observer{};
static std::atomic<Tick> simple{};
static SRWLOCK install_lock=SRWLOCK_INIT;
static bool pinned(const void* address){HMODULE module{};return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(address),&module)!=0;}
static void hook(void* engine,float dt,bool idle){if(auto call=observer.load(std::memory_order_acquire))call(engine,dt,idle);original(engine,dt,idle);}
extern "C" __declspec(dllexport) int QDInstallNativeTick(EngineTick function){
 if(!function){return 0;}AcquireSRWLockExclusive(&install_lock);int result=0;
 do{if(auto installed=observer.load()){result=installed==function;break;}
  if(!verified_game())break;
  auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));auto cell=reinterpret_cast<void**>(base+0x3E49EE8);auto expected=reinterpret_cast<void*>(base+0x28C4D80);
  if(*cell!=expected||!pinned(reinterpret_cast<const void*>(&hook))||!pinned(reinterpret_cast<const void*>(function)))break;
  std::memcpy(&original,&expected,sizeof(original));DWORD protect{};if(!VirtualProtect(cell,sizeof(void*),PAGE_READWRITE,&protect))break;
  observer.store(function,std::memory_order_release);
  auto prior=InterlockedCompareExchangePointer(cell,reinterpret_cast<void*>(&hook),expected);DWORD ignored{};
  const bool restored=VirtualProtect(cell,sizeof(void*),protect,&ignored)!=0;
  if(prior!=expected){observer.store(nullptr);break;}
  if(!restored){InterlockedCompareExchangePointer(cell,expected,reinterpret_cast<void*>(&hook));observer.store(nullptr);VirtualProtect(cell,sizeof(void*),protect,&ignored);break;}
  result=1;
 }while(false);
 ReleaseSRWLockExclusive(&install_lock);return result;
}
static void dispatch(void*,float dt,bool){if(auto call=simple.load(std::memory_order_acquire))call(dt);}
extern "C" __declspec(dllexport) int QDInstallEngineTick(Tick function){
 if(!function||!pinned(reinterpret_cast<const void*>(function)))return 0;
 Tick expected=nullptr;if(!simple.compare_exchange_strong(expected,function)&&expected!=function)return 0;
 return QDInstallNativeTick(&dispatch);
}
