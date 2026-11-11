#define DllMain UnusedWorldFixtureDllMain
#include "native_world.cpp"
#undef DllMain
#include "qdcoop/native_room_launch.hpp"

namespace Room=qdcoop::native_room;

struct LanBoot {
    int (*read_room)(Room::Status*,unsigned){};
    int (*read_lan)(Lan::Status*,unsigned){};
    int (*leave)(){};
    WorldPhase (*world)(){};
    wchar_t report[32768]{},ready[32768]{},peer[32768]{};
    Lan::Status active;
    ULONGLONG started{},paired{};
    bool verified_metadata{},leaving{};
};
static LanBoot boot_lan;
static std::atomic<EngineTick> boot_lan_original{};
static void* volatile* boot_lan_cell;

static bool bind_lan_boot(){
    if(boot_lan.read_room&&boot_lan.read_lan&&boot_lan.leave&&boot_lan.world)return true;
    const auto module=GetModuleHandleW(L"QDNativeBootstrap.dll");
    return module&&load(module,"QDNativeReadStatus",boot_lan.read_room)&&load(module,"QDNativeReadLanStatus",boot_lan.read_lan)&&
        load(module,"QDNativeLeaveRoom",boot_lan.leave)&&load(module,"QDNativeReadWorldPhase",boot_lan.world);
}

static void finish_lan_boot(Address gi,bool success,const Lan::Status& final){
    const auto quit_fn=resolve(quit),cdo=resolve(library);
    if(!quit_fn||!cdo||!gi)return;
    char report[420]{};
    const auto size=std::snprintf(report,sizeof(report),
        "{\"passed\":%s,\"host\":%s,\"epoch\":%u,\"seats\":%u,\"connected\":%u,\"slot\":%u,\"mask\":%u,"
        "\"scope\":\"%llx.%llx\",\"phase\":%u,\"failure\":%u,\"ue4ss_absent\":%s}\n",
        success?"true":"false",host?"true":"false",boot_lan.active.epoch,boot_lan.active.seats,boot_lan.active.connected,
        boot_lan.active.slot,boot_lan.active.peer_mask,static_cast<unsigned long long>(boot_lan.active.room.high),
        static_cast<unsigned long long>(boot_lan.active.room.low),static_cast<unsigned>(final.phase),static_cast<unsigned>(final.failure),
        GetModuleHandleW(L"UE4SS.dll")?"false":"true");
    if(size>0)write_file(boot_lan.report,report,static_cast<unsigned>(size));
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&gi,8);
    args[17]=1;
    quitting=true;
    event(cdo,quit_fn,args.data());
}

static void lan_boot_tick(void* engine,float,bool){
    if(quitting)return;
    const auto now=GetTickCount64();
    if(!boot_lan.started)boot_lan.started=now;
    const auto gi=object_field(reinterpret_cast<Address>(engine),"GameInstance");
    Lan::Status lan;
    if(now-boot_lan.started>85000){finish_lan_boot(gi,false,lan);return;}
    if(!boot_lan.verified_metadata){
        boot_lan.verified_metadata=metadata.step(image+0x4a64260,image+0x4a27f00,read_local,assign);
        if(!boot_lan.verified_metadata)return;
    }
    if(phase==0)title_step();
    if(!bind_lan_boot())return;
    Room::Status room;
    if(!boot_lan.read_room(&room,sizeof(room))||!boot_lan.read_lan(&lan,sizeof(lan)))return;
    if(room.result==Room::Result::rejected){finish_lan_boot(gi,false,lan);return;}
    if(!boot_lan.active.room&&room.result==Room::Result::running&&lan.phase==Lan::Phase::active&&
       boot_lan.world()==WorldPhase::active&&lan.epoch==1&&lan.seats==2&&lan.connected==1&&lan.peer_mask==1){
        boot_lan.active=lan;
        write_file(boot_lan.ready,"active",6);
    }
    if(host&&boot_lan.active.room&&!boot_lan.leaving&&exists(boot_lan.peer)){
        if(!boot_lan.paired)boot_lan.paired=now;
        if(now-boot_lan.paired>=750)boot_lan.leaving=boot_lan.leave()!=0;
    }
    if(boot_lan.active.room&&room.result==Room::Result::closed){
        const bool success=host?(boot_lan.leaving&&lan.phase==Lan::Phase::closed&&lan.failure==Lan::Failure::none):
            (lan.phase==Lan::Phase::failed&&lan.failure==Lan::Failure::departure);
        finish_lan_boot(gi,success,lan);
    }
}

static void lan_boot_observe(void* engine,float dt,bool idle){
    const auto original=boot_lan_original.load(std::memory_order_acquire);
    original(engine,dt,idle);
    lan_boot_tick(engine,dt,idle);
    if(quitting){
        DWORD protect{},ignored{};
        auto page=const_cast<void**>(boot_lan_cell);
        if(VirtualProtect(page,sizeof(void*),PAGE_READWRITE,&protect)){
            InterlockedCompareExchangePointer(boot_lan_cell,reinterpret_cast<void*>(original),reinterpret_cast<void*>(&lan_boot_observe));
            VirtualProtect(page,sizeof(void*),protect,&ignored);
        }
    }
}

static bool attach_lan_boot_observer(){
    const auto bootstrap=GetModuleHandleW(L"QDNativeBootstrap.dll");
    if(!bootstrap||!bind_lan_boot())return false;
    Room::Status status;
    if(!boot_lan.read_room(&status,sizeof(status))||!status.game_thread)return false;
    auto cell=reinterpret_cast<void* volatile*>(image+0x3e49ee8);
    const auto current=*cell;
    HMODULE owner{},probe{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(current),&owner)||owner!=bootstrap||
       !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&lan_boot_observe),&probe))return false;
    DWORD protect{},ignored{};
    auto page=const_cast<void**>(cell);
    if(!VirtualProtect(page,sizeof(void*),PAGE_READWRITE,&protect))return false;
    boot_lan_cell=cell;
    boot_lan_original.store(reinterpret_cast<EngineTick>(current),std::memory_order_release);
    const auto hook=reinterpret_cast<void*>(&lan_boot_observe);
    const auto previous=InterlockedCompareExchangePointer(cell,hook,current);
    if(!VirtualProtect(page,sizeof(void*),protect,&ignored)){
        InterlockedCompareExchangePointer(cell,current,hook);
        VirtualProtect(page,sizeof(void*),protect,&ignored);
        return false;
    }
    return previous==current;
}

static DWORD WINAPI start_lan_boot(void*){
    wchar_t role[16]{};
    if(!environment(L"QD_LAN_BOOT_ROLE",role,16)||!environment(L"QD_LAN_BOOT_REPORT",boot_lan.report,32768)||
       !environment(L"QD_LAN_BOOT_READY",boot_lan.ready,32768)||!environment(L"QD_LAN_BOOT_PEER",boot_lan.peer,32768))return 0;
    host=std::wcscmp(role,L"host")==0;
    image=reinterpret_cast<Address>(GetModuleHandleW(nullptr));
    const auto deadline=GetTickCount64()+40000;
    while(GetTickCount64()<deadline){
        if(attach_lan_boot_observer())return 1;
        Sleep(50);
    }
    constexpr char failure[]="{\"passed\":false,\"observer\":\"bootstrap tick unavailable\"}\n";
    write_file(boot_lan.report,failure,sizeof(failure)-1);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){
        DisableThreadLibraryCalls(instance);
        if(const auto thread=CreateThread(nullptr,0,start_lan_boot,nullptr,0,nullptr))CloseHandle(thread);
    }
    return TRUE;
}
