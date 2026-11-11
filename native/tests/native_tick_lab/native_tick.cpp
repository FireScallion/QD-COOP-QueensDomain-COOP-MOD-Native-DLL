#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "qdcoop/weak_object_view.hpp"
#include "native_status_text.hpp"

using EngineTick=void(*)(void*,float,bool);
extern "C" int QDInstallNativeTick(EngineTick);
static std::atomic<unsigned> ticks{},wrong_thread{},valid_engine{},state{};
static std::atomic<DWORD> game_thread{};
static wchar_t report[32768]{};
#include "qdcoop/object_lookup.hpp"
static std::atomic<unsigned> names_checked{},names_failed{},quit_size{},found_targets{};
static qdcoop::ue427::WeakKey quit_key{},library_key{},context_key{};
static unsigned scan_cursor;
static std::atomic<unsigned> layout_valid{};
static bool quitting;
static void write_report();
static bool local_read(std::uintptr_t p,void* out,std::size_t n){std::memcpy(out,reinterpret_cast<void*>(p),n);return true;}
static std::string object_name(std::uintptr_t object){qdcoop::ue427::NameKey key{};std::memcpy(&key,reinterpret_cast<void*>(object+24),8);std::string out;auto pool=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))+0x4A27F00;if(qdcoop::ue427::name_string(pool,key,local_read,out))++names_checked;else ++names_failed;return out;}
#include "qdcoop/enemy_metadata.hpp"
#include "qdcoop/enemy_stream.hpp"

struct CoreExports {
 int (*initialize)(const std::uint64_t*, unsigned){};
 int (*start)(unsigned, unsigned, std::uint64_t, std::uint64_t, unsigned, unsigned, unsigned){};
 unsigned (*status)(char*, unsigned){};
 int (*stop)(){};
 void (*tick)(float){};
};
static CoreExports core;
static qdcoop::ue427::EnemyMetadata enemy_metadata;
static std::atomic<unsigned> metadata_found{}, core_state{};
static unsigned core_frames;
static std::atomic<bool> transport_ready{}, probe_done{};
static std::atomic<unsigned> packets_sent{};
static constexpr unsigned local_port=49762, peer_port=49761;
static constexpr qdcoop::enemy::Envelope room{3819373611u,1729814417u,1,1};
static wchar_t core_report[32768]{};

template<class Fn>
static bool load_export(HMODULE dll, const char* name, Fn& target){
 auto address=GetProcAddress(dll,name);
 std::memcpy(&target,&address,sizeof(target));
 return address!=nullptr;
}

static void advance_core(float dt){
 const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
 if(core_state==0){
  const auto assign=reinterpret_cast<void(*)(void*,const void*)>(base+0x11BB360);
  const bool ready=enemy_metadata.step(base+0x4A64260,base+0x4A27F00,local_read,assign);
  metadata_found=enemy_metadata.found;
  if(!ready)return;
  if(!core.initialize(enemy_metadata.arguments.data(),static_cast<unsigned>(enemy_metadata.arguments.size()))){
   core_state=10;
   return;
  }
  if(!core.start(0,local_port,room.room_high,room.room_low,peer_port,0,0)){
   core_state=11;
   return;
  }
  core_state=1;
 }
 if(core_state==1){
  core.tick(dt);
  ++core_frames;
  if(!transport_ready&&core_frames%16==0){
   const auto status=read_native_status(core.status);
   transport_ready=status.find("\"build\":1,")!=std::string::npos;
  }
 }
}

static void write_core_report(){
 const auto status=read_native_status(core.status);
 if(status.empty())return;
 auto file=CreateFileW(core_report,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE)return;
 DWORD written{};
 WriteFile(file,status.data(),static_cast<DWORD>(status.size()),&written,nullptr);
 CloseHandle(file);
}
static void scan_targets(){
 auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));qdcoop::ue427::ObjectArray array{};local_read(base+0x4A64260,&array,sizeof(array));if(!qdcoop::ue427::object_array_shape(array))return;
 auto assign=reinterpret_cast<void(*)(void*,const void*)>(base+0x11BB360);
 for(unsigned step=0;found_targets<3&&step<2048&&scan_cursor<static_cast<unsigned>(array.num_elements);++step,++scan_cursor){std::uintptr_t chunk{};local_read(array.chunks+(scan_cursor/65536)*8,&chunk,8);if(!chunk)continue;qdcoop::ue427::ObjectItem item{};local_read(chunk+(scan_cursor%65536)*24,&item,24);if(!item.object||(item.flags&0x30000000u))continue;
  auto name=object_name(item.object);std::uintptr_t type{},outer{};local_read(item.object+16,&type,8);if(!type)continue;
  if(name=="Default__KismetSystemLibrary"&&object_name(type)=="KismetSystemLibrary")assign(&library_key,reinterpret_cast<void*>(item.object));
  if(name=="QuitGame"&&object_name(type)=="Function"){local_read(item.object+32,&outer,8);if(outer&&object_name(outer)=="KismetSystemLibrary"){assign(&quit_key,reinterpret_cast<void*>(item.object));std::uint16_t size{};local_read(item.object+182,&size,2);quit_size=size;}}
  if(!context_key.serial){std::uint32_t flags{};local_read(item.object+8,&flags,4);if(!(flags&0x10u)&&object_name(type)=="TitleScreenUI_C")assign(&context_key,reinterpret_cast<void*>(item.object));}
 }
 if(scan_cursor>=static_cast<unsigned>(array.num_elements))scan_cursor=0;
 auto resolve=[&](auto key){return qdcoop::ue427::resolve_weak(base+0x4A64260,key,local_read);};auto fn=resolve(quit_key),library=resolve(library_key),context=resolve(context_key);found_targets=unsigned(fn!=0)+unsigned(library!=0)+unsigned(context!=0);
 if(fn&&library&&context&&!layout_valid){
  const auto pool=base+0x4A27F00;std::string path;bool okay=qdcoop::ue427::object_path(fn,pool,local_read,path)&&path=="/Script/Engine.KismetSystemLibrary.QuitGame";
  const char* names[]{"WorldContextObject","SpecificPlayer","QuitPreference","bIgnorePlatformRestrictions"};const int offsets[]{0,8,16,17},sizes[]{8,8,1,1};
  for(unsigned i=0;i<4;++i){auto field=qdcoop::ue427::find_property(fn,pool,names[i],local_read);int offset=-1,size=-1;if(field){local_read(field+76,&offset,4);local_read(field+60,&size,4);}okay=okay&&offset==offsets[i]&&size==sizes[i];}
  layout_valid=okay?1:2;
 }
 if(fn&&library&&context&&quit_size==18&&layout_valid==1&&ticks>1000&&!quitting){
  if(core_state==0&&ticks<6000)return;
  if(core_state==1){
   if(core_frames<1000||(!probe_done&&ticks<6000))return;
   write_core_report();
   core_state=2;
  }
  if(core_state==2&&!core.stop())return;
  quitting=true;state=2;write_report();alignas(16) unsigned char parameters[32]{};std::memcpy(parameters,&context,8);std::uintptr_t table{};local_read(library,&table,8);void(*event)(void*,void*,void*){};local_read(table+0x220,&event,8);event(reinterpret_cast<void*>(library),reinterpret_cast<void*>(fn),parameters);}
}

static void hook(void* engine,float dt,bool idle){
 auto tid=GetCurrentThreadId();DWORD zero=0;game_thread.compare_exchange_strong(zero,tid);
 if(tid!=game_thread.load())++wrong_thread;
 if(tid==game_thread.load()&&engine&&std::isfinite(dt)&&dt>=0){
  auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));qdcoop::ue427::WeakKey key{};
  reinterpret_cast<void(*)(void*,const void*)>(base+0x11BB360)(&key,engine);
  auto read=[](std::uintptr_t p,void* out,std::size_t n){std::memcpy(out,reinterpret_cast<void*>(p),n);return true;};
  if(qdcoop::ue427::resolve_weak(base+0x4A64260,key,read)==reinterpret_cast<std::uintptr_t>(engine))++valid_engine;
 }
 const auto count=++ticks;
 (void)idle;
 if(count>60&&tid==game_thread.load()){
  advance_core(dt);
  scan_targets();
 }
}
static void write_report(){char text[512]{};auto n=std::snprintf(text,sizeof(text),"{\"state\":%u,\"ticks\":%u,\"game_thread\":%lu,\"wrong_thread\":%u,\"valid_engine\":%u,\"ue4ss_loaded\":%s,\"names_checked\":%u,\"names_failed\":%u,\"quit_size\":%u,\"found_targets\":%u,\"layout_valid\":%u,\"metadata_found\":%u,\"core_state\":%u,\"packets_sent\":%u}",state.load(),ticks.load(),static_cast<unsigned long>(game_thread.load()),wrong_thread.load(),valid_engine.load(),GetModuleHandleW(L"UE4SS.dll")?"true":"false",names_checked.load(),names_failed.load(),quit_size.load(),found_targets.load(),layout_valid.load(),metadata_found.load(),core_state.load(),packets_sent.load());auto f=CreateFileW(report,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f!=INVALID_HANDLE_VALUE){DWORD written{};WriteFile(f,text,n,&written,nullptr);CloseHandle(f);}}
static void send_probe_packets(){
 WSADATA data{};
 if(WSAStartup(MAKEWORD(2,2),&data)!=0){probe_done=true;return;}
 sockaddr_in target{};
 target.sin_family=AF_INET;
 target.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 target.sin_port=htons(local_port);
 auto send=[&](unsigned port,const qdcoop::enemy::Packet& packet){
  const auto socket_handle=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  if(socket_handle==INVALID_SOCKET)return;
  auto from=target;
  from.sin_port=htons(port);
  if(bind(socket_handle,reinterpret_cast<const sockaddr*>(&from),sizeof(from))==0){
   auto sent=sendto(socket_handle,reinterpret_cast<const char*>(packet.data.data()),static_cast<int>(packet.size),0,reinterpret_cast<const sockaddr*>(&target),sizeof(target));
   if(sent==static_cast<int>(packet.size))++packets_sent;
  }
  closesocket(socket_handle);
 };
 qdcoop::enemy::State unbound{};
 unbound.id=1;
 unbound.generation=1;
 unbound.sequence=1;
 unbound.v[21]=1;
 qdcoop::enemy::Packet packet{};
 if(qdcoop::enemy::encode(room,unbound,packet)){
  send(peer_port,packet);
  send(0,packet);
 }
 packet.size=1;
 send(peer_port,packet);
 WSACleanup();
 probe_done=true;
}

static DWORD WINAPI start(void*){
 if(!GetEnvironmentVariableW(L"QD_NATIVE_REPORT",report,32768))return 0;
 if(GetModuleHandleW(L"UE4SS.dll")){state=10;write_report();return 0;}
  wchar_t dll_path[32768]{};
 const auto dll_length=GetEnvironmentVariableW(L"QD_NATIVE_CORE",dll_path,32768);
 const auto report_length=GetEnvironmentVariableW(L"QD_NATIVE_CORE_REPORT",core_report,32768);
 if(!dll_length||dll_length>=32768||!report_length||report_length>=32768){
  state=12;write_report();return 0;
 }
 auto dll=LoadLibraryW(dll_path);
 if(!dll||!load_export(dll,"QDEnemyInitializeNative",core.initialize)||
    !load_export(dll,"QDEnemyStartNative",core.start)||
    !load_export(dll,"QDEnemyNativeStatus",core.status)||
    !load_export(dll,"QDEnemyStopNative",core.stop)||
    !load_export(dll,"QDEnemyEngineTick",core.tick)){
  state=13;write_report();return 0;
 }
 state=QDInstallNativeTick(&hook)?1:11;write_report();
  for(unsigned i=0;i<600&&state==1;++i){
  Sleep(25);
  if(transport_ready&&!probe_done)send_probe_packets();
  if(i%10==0&&state==1)write_report();
 }
 return 0;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(instance);auto thread=CreateThread(nullptr,0,start,nullptr,0,nullptr);if(thread)CloseHandle(thread);}return TRUE;}
