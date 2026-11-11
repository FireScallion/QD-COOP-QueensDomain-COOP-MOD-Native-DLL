// Private-station test driver. The game's XInput import loads the bootstrap.
#include "../../adapters/windows/sibling_module.hpp"
#include "qdcoop/native_room_launch.hpp"
#include "qdcoop/weak_object_view.hpp"
#include "qdcoop/object_lookup.hpp"
#include "qdcoop/reflection_view.hpp"
#include "qdcoop/steam_social.hpp"
#include <cstdio>
#include <xinput.h>

namespace {
namespace Room=qdcoop::native_room;
namespace Steam=qdcoop::steam_room;
namespace UE=qdcoop::ue427;
using qdcoop::windows::load_export;
using Address=std::uintptr_t;
using Tick=void(*)(void*,float,bool);
void** tick_slot{};
Tick bootstrap_tick{};
Address image{};

int (*read_status)(Room::Status*,unsigned){};
int (*host_room)(unsigned,unsigned){};
int (*join_room)(Steam::SteamId){};
int (*leave_room)(){};

bool forwarded() {
    const auto system=qdcoop::windows::load_system(L"xinput1_3.dll");
    if(!system)return false;
    wchar_t path[32768]{};
    const auto length=GetModuleFileNameW(nullptr,path,32768);
    if(!length||length>=32768)return false;
    const auto slash=std::wcsrchr(path,L'\\');
    if(!slash||slash-path+15>=32768)return false;
    std::wcscpy(slash+1,L"xinput1_3.dll");
    const auto proxy=GetModuleHandleW(path);
    decltype(&XInputGetState) real{},forwarded_call{};
    XINPUT_STATE actual{},expected{};
    const auto ordinal=MAKEINTRESOURCEA(2);
    return proxy&&system!=proxy&&load_export(system,ordinal,real)&&load_export(proxy,ordinal,forwarded_call)&&
        real(XUSER_MAX_COUNT,&expected)==forwarded_call(XUSER_MAX_COUNT,&actual)&&
        std::memcmp(&actual,&expected,sizeof(actual))==0;
}

bool read_local(Address address,void* out,std::size_t size) {
    std::memcpy(out,reinterpret_cast<const void*>(address),size);
    return true;
}
template<class T>
T at(Address address,unsigned offset) {
    return UE::field<T>(reinterpret_cast<void*>(address),offset);
}
std::string leaf(Address object) {
    std::string name;
    if(object)UE::name_string(image+0x4A27F00,at<UE::NameKey>(object,24),read_local,name);
    return name;
}
bool path_is(Address object,std::string_view expected) {
    std::string path;
    return UE::object_path(object,image+0x4A27F00,read_local,path)&&path==expected;
}
bool parameter(Address function,std::string_view name,int offset,int width) {
    const auto property=UE::find_property(function,image+0x4A27F00,name,read_local);
    return property&&at<int>(property,76)==offset&&at<int>(property,60)==width&&
        (at<std::uint64_t>(property,64)&0x80u);
}

bool request_unreal_exit() {
    const auto array=at<UE::ObjectArray>(image+0x4A64260,0);
    if(!UE::object_array_shape(array))return false;
    Address context{},library{},quit{};
    for(unsigned i=0;i<static_cast<unsigned>(array.num_elements)&&!(context&&library&&quit);++i) {
        const auto chunk=at<Address>(array.chunks,(i/UE::objects_per_chunk)*8);
        if(!chunk)continue;
        const auto item=at<UE::ObjectItem>(chunk,(i%UE::objects_per_chunk)*sizeof(UE::ObjectItem));
        if(!item.object||(item.flags&0x30000000u))continue;
        const auto name=leaf(item.object);
        if(!library&&name=="Default__KismetSystemLibrary"&&
           path_is(item.object,"/Script/Engine.Default__KismetSystemLibrary"))
            library=item.object;
        if(!quit&&name=="QuitGame"&&path_is(item.object,"/Script/Engine.KismetSystemLibrary.QuitGame"))
            quit=item.object;
        if(!context&&!(at<unsigned>(item.object,8)&0x10u)&&leaf(at<Address>(item.object,16))=="TitleScreenUI_C")
            context=item.object;
    }
    if(!context||!library||!quit||at<std::uint16_t>(quit,182)!=18||
       !parameter(quit,"WorldContextObject",0,8)||!parameter(quit,"SpecificPlayer",8,8)||
       !parameter(quit,"QuitPreference",16,1)||!parameter(quit,"bIgnorePlatformRestrictions",17,1))return false;
    alignas(16) unsigned char arguments[18]{};
    std::memcpy(arguments,&context,sizeof(context));
    using ProcessEvent=void(*)(void*,void*,void*);
    at<ProcessEvent>(at<Address>(library,0),0x220)(reinterpret_cast<void*>(library),
        reinterpret_cast<void*>(quit),arguments);
    return true;
}

void quit_tick(void* engine,float dt,bool idle) {
    static ULONGLONG retry{};
    const auto time=GetTickCount64();
    if(time>=retry) {
        retry=time+250;
        if(request_unreal_exit()) {
            retry=~0ull;
            DWORD protect{},ignored{};
            if(VirtualProtect(tick_slot,sizeof(void*),PAGE_READWRITE,&protect)) {
                InterlockedCompareExchangePointer(tick_slot,reinterpret_cast<void*>(bootstrap_tick),
                    reinterpret_cast<void*>(&quit_tick));
                VirtualProtect(tick_slot,sizeof(void*),protect,&ignored);
            }
        }
    }
    bootstrap_tick(engine,dt,idle);
}

bool schedule_unreal_exit(HMODULE module) {
    // Run only after the production bridge has verified the Shipping image.
    image=reinterpret_cast<Address>(GetModuleHandleW(nullptr));
    tick_slot=reinterpret_cast<void**>(image+0x3E49EE8);
    const auto current=*tick_slot;
    HMODULE owner{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(current),&owner)||owner!=module)return false;
    std::memcpy(&bootstrap_tick,&current,sizeof(current));
    DWORD protect{},ignored{};
    if(!VirtualProtect(tick_slot,sizeof(void*),PAGE_READWRITE,&protect))return false;
    const auto replaced=InterlockedCompareExchangePointer(tick_slot,reinterpret_cast<void*>(&quit_tick),current);
    return VirtualProtect(tick_slot,sizeof(void*),protect,&ignored)&&replaced==current;
}

bool wait(Room::Status& status,Steam::Phase phase,unsigned seconds) {
    const auto deadline=GetTickCount64()+seconds*1000ull;
    do {
        if(!read_status(&status,sizeof(status)))return false;
        if(status.room.phase==static_cast<unsigned>(phase)&&
           (phase!=Steam::Phase::closed||status.result==Room::Result::closed))return true;
        if(status.result==Room::Result::rejected||
           (status.result==Room::Result::closed&&phase!=Steam::Phase::failed))return false;
        Sleep(50);
    }while(GetTickCount64()<deadline);
    return false;
}

#include "steam_social_fixture.inc"

bool real_waiting_epoch(const Room::Status& state) {
    const auto module=GetModuleHandleW(L"steam_api64.dll");
    void* (*matchmaking)(){};
    Steam::SteamId (*owner)(void*,Steam::SteamId){};
    const char* (*get)(void*,Steam::SteamId,const char*){};
    if(!module||!load_export(module,"SteamAPI_SteamMatchmaking_v009",matchmaking)||
       !load_export(module,"SteamAPI_ISteamMatchmaking_GetLobbyOwner",owner)||
       !load_export(module,"SteamAPI_ISteamMatchmaking_GetLobbyData",get))return false;
    const auto service=matchmaking();
    if(!service)return false;
    const auto tag=get(service,state.room.lobby,"transport");
    if(!tag||std::string_view(tag)!=Steam::transport)return false;
    const auto owner_id=owner(service,state.room.lobby);
    const auto encoded=get(service,state.room.lobby,"qd_epoch");
    Steam::Epoch epoch{};
    return encoded&&Steam::decode_epoch(encoded,owner_id,epoch)&&epoch.serial==1&&
        state.room.epoch==epoch.serial&&state.room.nonce==epoch.nonce&&!epoch.locked.count;
}

DWORD WINAPI probe(void*) {
    const auto module=GetModuleHandleW(L"QDNativeBootstrap.dll");
    const bool controls=module&&load_export(module,"QDNativeReadStatus",read_status)&&
        load_export(module,"QDNativeHostRoom",host_room)&&load_export(module,"QDNativeJoinRoom",join_room)&&
        load_export(module,"QDNativeLeaveRoom",leave_room);
    const bool forwarding=forwarded();
    if(controls&&load_export(module,"QDNativeProbeSteamCallback",inject_steam_callback))
        return social_probe(module,forwarding);
    Social::Status released_social{};
    const bool release_social=controls&&load_export(module,"QDNativeReadSteamSocial",read_social)&&
        poll_probe([&]{return read_social(&released_social,sizeof(released_social))&&
            released_social.ready&&released_social.callbacks==2;},15);
    Room::Status first{},first_closed{},second{},second_closed{},join{};
    const bool bad_rejected=controls&&!host_room(1,0)&&!join_room(42);
    const bool cli_room=controls&&wait(first,Steam::Phase::waiting,40);
    const bool first_epoch=cli_room&&poll_probe([&]{return real_waiting_epoch(first);});
    const bool cancel_first=cli_room&&leave_room()&&wait(first_closed,Steam::Phase::closed,10);
    const bool reopen=cancel_first&&host_room(4,0)&&wait(second,Steam::Phase::waiting,20);
    const bool second_epoch=reopen&&poll_probe([&]{return real_waiting_epoch(second);});
    const bool cancel_second=reopen&&leave_room()&&wait(second_closed,Steam::Phase::closed,10);
    const bool old_join=cancel_second&&join_room(first.room.lobby)&&wait(join,Steam::Phase::failed,15);
    const bool absent=!GetModuleHandleW(L"UE4SS.dll");
    const bool distinct=first.room.lobby&&second.room.lobby&&first.room.lobby!=second.room.lobby&&
        first.room.nonce&&second.room.nonce&&first.room.nonce!=second.room.nonce;
    const bool same_thread=first.game_thread&&first.game_thread==second.game_thread&&first.game_thread!=GetCurrentThreadId();
    const bool passed=controls&&release_social&&!released_social.published_lobby&&forwarding&&bad_rejected&&
        first_epoch&&second_epoch&&
        cli_room&&cancel_first&&reopen&&cancel_second&&
        old_join&&absent&&distinct&&same_thread&&first.metadata_found==42&&first.room.members==1&&
        second.room.members==1&&first.room.limit==4&&second.room.limit==4&&first_closed.room.left==1&&
        second_closed.room.left==1&&join.room.enter_response==2;
    if(controls)leave_room();
    wchar_t path[32768]{};
    if(const auto size=GetEnvironmentVariableW(L"QD_BOOT_REPORT",path,32768);size&&size<32768) {
        if(auto file=_wfopen(path,L"wb")) {
            std::fprintf(file,
                "{\"passed\":%u,\"controls\":%u,\"forwarding\":%u,\"invalid_rejected\":%u,\"steam_callbacks\":%u,"
                "\"cli_room\":%u,\"cancel_first\":%u,\"reopen\":%u,\"cancel_second\":%u,"
                "\"old_join\":%u,\"ue4ss_absent\":%u,\"distinct\":%u,\"same_thread\":%u,"
                "\"metadata\":%u,\"game_thread\":%u,\"first_phase\":%u,\"first_failure\":%u,"
                "\"first_result\":%u,\"bootstrap_failure\":%u,\"first_init_result\":%d,"
                "\"first_initialized_here\":%u,\"first_members\":%u,\"second_members\":%u,"
                "\"first_limit\":%u,\"second_limit\":%u,\"first_left\":%u,\"second_left\":%u,"
                "\"closed_join_response\":%u,\"first_epoch_verified\":%u,\"second_epoch_verified\":%u}\n",
                passed,controls,forwarding,bad_rejected,released_social.callbacks,cli_room,cancel_first,reopen,cancel_second,
                old_join,absent,distinct,same_thread,first.metadata_found,first.game_thread,first.room.phase,
                first.room.failure,static_cast<unsigned>(first.result),static_cast<unsigned>(first.failure),
                first.room.init_result,first.room.initialized_here,first.room.members,second.room.members,
                first.room.limit,second.room.limit,first_closed.room.left,second_closed.room.left,
                join.room.enter_response,first_epoch,second_epoch);
            std::fclose(file);
        }
    }
    return first.game_thread&&schedule_unreal_exit(module)?0:1;
}
}

BOOL WINAPI DllMain(HINSTANCE,DWORD reason,void*) {
    if(reason==DLL_PROCESS_ATTACH)
        if(const auto thread=CreateThread(nullptr,0,probe,nullptr,0,nullptr))CloseHandle(thread);
    return TRUE;
}
