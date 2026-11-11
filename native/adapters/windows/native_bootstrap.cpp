#include "sibling_module.hpp"
#include <shellapi.h>
#include <algorithm>
#include <atomic>
#include <utility>
#include <vector>
#include "qdcoop/enemy_metadata.hpp"
#include "qdcoop/native_room_launch.hpp"
#include "qdcoop/lan_room.hpp"
#include "qdcoop/native_world.hpp"
#include "qdcoop/player_skin.hpp"
#include "qdcoop/reflection_view.hpp"
#include "qdcoop/steam_social.hpp"

extern "C" int QDInstallNativeTick(void(*)(void*,float,bool));

namespace {
namespace Room=qdcoop::native_room;
namespace Steam=qdcoop::steam_room;
namespace Lan=qdcoop::lan_room;
namespace UE=qdcoop::ue427;
namespace World=qdcoop::native_world;
namespace Player=qdcoop::player;
using qdcoop::windows::load_export;

#include "steam_social.inc"

struct Core {
    int (*initialize)(const std::uint64_t*,unsigned){};
    int (*start)(unsigned,Steam::SteamId,unsigned,unsigned){};
    int (*read)(Steam::Status*,unsigned){};
    int (*start_lan)(unsigned,const wchar_t*,unsigned,unsigned){};
    int (*read_lan)(Lan::Status*,unsigned){};
    int (*read_lan_roster)(Lan::LocalRoster*,unsigned){};
    int (*stop)(){};
    int (*recover)(){};
    int (*skin)(unsigned){};
    int (*place_available)(void*){};
    std::uint64_t (*place_queue)(void*,void*){};
    int (*place_result)(std::uint64_t){};
    World::Phase (*pump_world)(void*,float,unsigned){};
};

Core core;
SRWLOCK control_lock=SRWLOCK_INIT;
Room::Request pending{};
Room::Status snapshot{};
Lan::Status lan_snapshot{};
std::atomic<bool> scheduled{};
std::atomic<World::Phase> world_phase{};
Social::Status social_snapshot{};
Social::Invitation pending_invitation{};
std::uint64_t invitation_ticket{};
std::atomic<Overlay> pending_overlay{};
bool hooked{},core_bound{};
unsigned selected_skin=1;
bool skin_command_line{};

// Game-thread state. Control and status calls only touch the mailbox.
UE::EnemyMetadata metadata;
Room::Request preparing{},after_leave{};
Room::Status status{};
Lan::Status lan_status{};
ULONGLONG prepare_deadline{},next_step{};
ULONGLONG next_social{};
Steam::SteamId previous_presence{};
SteamSocial social;
bool started{},room_entered_world{};
struct InvitationGate {std::uint64_t offered{},confirmed{};};
InvitationGate invitation_gate;

void receive_invitation(Social::Invitation invitation) {
    AcquireSRWLockExclusive(&control_lock);
    const bool current=snapshot.room.lobby==invitation.lobby&&
        (snapshot.result==Room::Result::running||snapshot.result==Room::Result::stopping);
    const bool duplicate=pending_invitation.lobby==invitation.lobby&&
        pending_invitation.friend_id==invitation.friend_id;
    if(!current&&!duplicate) {
        if(++invitation_ticket==0)++invitation_ticket;
        invitation.ticket=invitation_ticket;
        pending_invitation=invitation;
    }
    ReleaseSRWLockExclusive(&control_lock);
}

void service_social(ULONGLONG now) {
    const auto owned=status.result==Room::Result::running&&preparing.operation==Room::Operation::host&&
        status.room.phase==static_cast<unsigned>(Steam::Phase::waiting)&&status.room.members<status.room.limit?
        status.room.lobby:0;
#ifdef QDCOOP_STEAM_SOCIAL_LAB
    const auto desired=owned; // Private lobby, and a key without a Join Game action.
#else
    const auto desired=preparing.friends?owned:0;
#endif
    if(now<next_social&&desired==previous_presence&&pending_overlay.load()==Overlay::none)return;
    next_social=now+1000;
    previous_presence=desired;
    const auto action=pending_overlay.exchange(Overlay::none);
    const auto result=social.service(desired,action,owned);
    AcquireSRWLockExclusive(&control_lock);
    const auto old_action=social_snapshot.action;
    social_snapshot=result;
    social_snapshot.action=pending_overlay.load()!=Overlay::none?Social::Action::queued:
        action==Overlay::none?old_action:result.action;
    ReleaseSRWLockExclusive(&control_lock);
}

bool read_local(std::uintptr_t address,void* out,std::size_t size) {
    std::memcpy(out,reinterpret_cast<const void*>(address),size);
    return true;
}

int submit(Room::Request request);
int request_overlay(Overlay action);
int respond_to_invitation(std::uint64_t ticket,unsigned accept);
unsigned current_skin() {
    AcquireSRWLockShared(&control_lock);
    const auto skin=selected_skin;
    ReleaseSRWLockShared(&control_lock);
    return skin;
}
int select_native_skin(unsigned skin,bool stored=false) {
    if(!Player::valid_skin(skin))return 0;
    AcquireSRWLockExclusive(&control_lock);
    const bool apply=!stored||!skin_command_line;
    if(apply){
        selected_skin=skin;
        if(core_bound)core.skin(skin);
    }
    ReleaseSRWLockExclusive(&control_lock);
    return apply;
}
Social::Invitation read_invitation() {
    AcquireSRWLockShared(&control_lock);
    const auto invitation=pending_invitation;
    ReleaseSRWLockShared(&control_lock);
    return invitation;
}
#include "native_save_backup.inc"
#include "native_title_menu.inc"
#include "native_pause_invite.inc"
#include "native_place_menu.inc"
#include "native_room_notices.inc"

bool discover() {
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto assign=reinterpret_cast<void(*)(UE::WeakKey*,const void*)>(image+0x11BB360);
    return metadata.step(image+0x4A64260,image+0x4A27F00,read_local,assign);
}

void publish() {
    AcquireSRWLockExclusive(&control_lock);
    if(pending.operation==Room::Operation::none) {
        snapshot=status;
        lan_snapshot=lan_status;
        scheduled.store(status.result==Room::Result::preparing||
                        status.result==Room::Result::running||
                        status.result==Room::Result::stopping,std::memory_order_release);
    }
    ReleaseSRWLockExclusive(&control_lock);
}

void read_room(){
    if(Room::is_lan(preparing.operation))core.read_lan(&lan_status,sizeof(lan_status));
    else core.read(&status.room,sizeof(status.room));
}

void prepare_room(ULONGLONG now) {
    using Backup=TitleUi::SaveBackup::State;
    const auto backup=TitleUi::menu.backup.state();
    if(backup!=Backup::ready){
        if(backup!=Backup::failed&&now<prepare_deadline)return;
        status.result=Room::Result::rejected;
        status.failure=Room::Failure::save_backup;
        return;
    }
    const bool found=discover();
    status.metadata_found=metadata.found;
    if(!found&&now<prepare_deadline)return;
    if(!found||!core.initialize(metadata.arguments.data(),static_cast<unsigned>(metadata.arguments.size()))) {
        status.result=Room::Result::rejected;
        status.failure=Room::Failure::metadata;
        return;
    }
    if(Room::is_lan(preparing.operation)){
        const bool host=preparing.operation==Room::Operation::host_lan;
        started=core.start_lan(host,host?nullptr:preparing.ipv4.data(),preparing.port,preparing.seats)!=0;
    }else started=core.start(preparing.operation==Room::Operation::host,preparing.lobby,
                             preparing.seats,preparing.friends)!=0;
    status.result=started?Room::Result::running:Room::Result::rejected;
    status.failure=started?Room::Failure::none:Room::Failure::start;
    if(started)read_room();
}

void pump_world(void* engine,float dt) {
    const bool active=Room::is_lan(preparing.operation)?lan_status.phase==Lan::Phase::active:
        status.room.phase==static_cast<unsigned>(Steam::Phase::active);
    if(status.result!=Room::Result::running||!active)return;
    const auto phase=core.pump_world(engine,dt,qdcoop::enemy::capacity);
    world_phase.store(phase,std::memory_order_release);
    if(phase==World::Phase::failed||phase==World::Phase::world_changed) {
        status.result=Room::Result::stopping;
        status.failure=Room::Failure::world;
        next_step=0;
    }
}

void begin_request(Room::Request command,ULONGLONG now) {
    TitleUi::menu.backup.start(TitleUi::menu.preferences.directory);
    preparing=command;
    TitleUi::room_notices.begin();
    prepare_deadline=now+30000;
    status.result=Room::Result::preparing;
    status.failure=Room::Failure::none;
    status.room={};
    lan_status={};
    room_entered_world=false;
    world_phase.store(World::Phase::idle,std::memory_order_release);
}

void tick(void* engine,float dt,bool) {
    const auto now=GetTickCount64();
    const auto thread=GetCurrentThreadId();
    if(status.game_thread&&status.game_thread!=thread)return;
    TitleUi::menu.tick(reinterpret_cast<std::uintptr_t>(engine),now,&TitleUi::observe,&TitleUi::observe_open);
    if(TitleUi::menu.level_known&&(status.result==Room::Result::preparing||status.result==Room::Result::running)){
        if(!TitleUi::menu.on_title)room_entered_world=true;
        else if(room_entered_world){
            submit({Room::Operation::leave});
            room_entered_world=false;
        }
    }
    TitleUi::pause_invite.tick(reinterpret_cast<std::uintptr_t>(engine),now,&TitleUi::observe_pause_click);
    TitleUi::place_menu.tick(reinterpret_cast<std::uintptr_t>(engine),now);
    if(!scheduled.load(std::memory_order_acquire)) {
        service_social(now);
        TitleUi::room_notices.tick(reinterpret_cast<std::uintptr_t>(engine),now);
        return;
    }
    if(now<next_step) {
        pump_world(engine,dt);
        service_social(now);
        return;
    }
    status.game_thread=thread;
    next_step=now+25;
    AcquireSRWLockExclusive(&control_lock);
    const auto command=pending;
    pending={};
    ReleaseSRWLockExclusive(&control_lock);
    if(command.operation==Room::Operation::leave) {
        after_leave={};
        TitleUi::room_notices.local_leave=true;
        status.result=Room::Result::stopping;
    } else if((command.operation==Room::Operation::join||command.operation==Room::Operation::join_lan)&&started) {
        after_leave=command;
        status.result=Room::Result::stopping;
    } else if(command.operation!=Room::Operation::none) {
        begin_request(command,now);
    }
    if(status.result==Room::Result::preparing) {
        prepare_room(now);
    } else if(status.result==Room::Result::running) {
        read_room();
        const bool lan=Room::is_lan(preparing.operation);
        const auto steam_phase=static_cast<Steam::Phase>(status.room.phase);
        const bool closed=lan?lan_status.phase==Lan::Phase::closed||lan_status.phase==Lan::Phase::failed:
            steam_phase==Steam::Phase::closed||steam_phase==Steam::Phase::failed;
        const bool recovering=lan?lan_status.phase==Lan::Phase::recovering:steam_phase==Steam::Phase::recovering;
        if(closed)status.result=Room::Result::stopping;
        else if(recovering) {
            world_phase.store(World::Phase::suspended,std::memory_order_release);
            core.recover();
        }
    }
    pump_world(engine,dt);
    if(status.result==Room::Result::stopping&&(!started||core.stop())) {
        if(started)read_room();
        started=false;
        status.result=Room::Result::closed;
        if(status.failure!=Room::Failure::world)world_phase.store(World::Phase::idle,std::memory_order_release);
        if(after_leave.operation!=Room::Operation::none) {
            begin_request(after_leave,now);
            after_leave={};
        }
    }
    publish();
    service_social(now);
    TitleUi::room_notices.tick(reinterpret_cast<std::uintptr_t>(engine),now);
}

Room::Failure ensure_hook() {
    if(GetModuleHandleW(L"UE4SS.dll"))return Room::Failure::legacy_loader;
    if(!hooked) {
        wchar_t path[32768]{};
        if(qdcoop::windows::sibling_path(nullptr,L"dwmapi.dll",path)&&GetModuleHandleW(path))
            return Room::Failure::legacy_loader;
        if(!QDInstallNativeTick(&tick))return Room::Failure::game_build;
        hooked=true;
    }
    return Room::Failure::none;
}

Room::Failure bind_core() {
    const auto failure=ensure_hook();
    if(failure!=Room::Failure::none||core_bound)return failure;
    HMODULE self{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&bind_core),&self))
        return Room::Failure::core;
    const auto module=qdcoop::windows::load_sibling(self,L"QDCoopNativeM1.dll");
    if(!module)return Room::Failure::core;
    if(!load_export(module,"QDEnemyInitializeNative",core.initialize)||
       !load_export(module,"QDEnemyStartNativeSteam",core.start)||
       !load_export(module,"QDEnemyReadNativeSteamRoom",core.read)||
       !load_export(module,"QDEnemyStartNativeLan",core.start_lan)||
       !load_export(module,"QDEnemyReadNativeLanRoom",core.read_lan)||
       !load_export(module,"QDEnemyReadNativeLanRoster",core.read_lan_roster)||
       !load_export(module,"QDEnemyStopNative",core.stop)||
       !load_export(module,"QDEnemyPollNativeRecovery",core.recover)||
       !load_export(module,"QDEnemySetNativeSkin",core.skin)||
       !load_export(module,"QDEnemyNativePlaceAvailability",core.place_available)||
       !load_export(module,"QDEnemyQueueNativePlaceFromInspect",core.place_queue)||
       !load_export(module,"QDEnemyReadNativePlaceResult",core.place_result)||
       !load_export(module,"QDEnemyPumpNativeWorld",core.pump_world)) {
        FreeLibrary(module);
        return Room::Failure::core;
    }
    core.skin(selected_skin);
    core_bound=true;
    return Room::Failure::none;
}

int queue_room(Room::Request request,bool switch_room) {
    const auto failure=bind_core();
    if(failure!=Room::Failure::none) {
        if(!switch_room) {
            snapshot.result=Room::Result::rejected;
            snapshot.failure=failure;
        }
        return 0;
    }
    pending=request;
    snapshot.result=switch_room?Room::Result::stopping:Room::Result::preparing;
    snapshot.failure=Room::Failure::none;
    if(!switch_room){snapshot.room={};lan_snapshot={};}
    scheduled.store(true,std::memory_order_release);
    return 1;
}

int submit(Room::Request request) {
    if(!Room::valid(request))return 0;
    AcquireSRWLockExclusive(&control_lock);
    int accepted=0;
    if(request.operation==Room::Operation::leave) {
        if(snapshot.result==Room::Result::preparing||snapshot.result==Room::Result::running) {
            pending=request;
            snapshot.result=Room::Result::stopping;
            accepted=1;
        }
    } else if(snapshot.result==Room::Result::idle||snapshot.result==Room::Result::closed||
              snapshot.result==Room::Result::rejected) {
        accepted=queue_room(request,false);
    }
    ReleaseSRWLockExclusive(&control_lock);
    return accepted;
}

int request_overlay(Overlay action) {
    AcquireSRWLockExclusive(&control_lock);
    int result=0;
    if(hooked&&pending_overlay.load()==Overlay::none) {
        pending_overlay.store(action);
        social_snapshot.action=Social::Action::queued;
        result=1;
    }
    ReleaseSRWLockExclusive(&control_lock);
    return result;
}

int respond_to_invitation(std::uint64_t ticket,unsigned accept) {
    if(!ticket||accept>1)return 0;
    AcquireSRWLockExclusive(&control_lock);
    int result=0;
    if(pending_invitation.ticket==ticket) {
        const auto state=snapshot.result;
        const bool switching=state==Room::Result::preparing||state==Room::Result::running||
            state==Room::Result::stopping;
        if(!accept||queue_room({Room::Operation::join,0,0,pending_invitation.lobby},switching)) {
            pending_invitation={};
            result=1;
        }
    }
    ReleaseSRWLockExclusive(&control_lock);
    return result;
}
}

extern "C" __declspec(dllexport) void QDNativeStart() {
    static std::atomic_flag once{};
    if(once.test_and_set())return;
    int count{};
    auto arguments=CommandLineToArgvW(GetCommandLineW(),&count);
    Room::Request request{};
    unsigned skin=1;
    const auto tail=arguments&&count>0?std::span{arguments+1,static_cast<std::size_t>(count-1)}:std::span<wchar_t*>{};
    const bool valid=arguments&&count>0&&Room::parse(tail,request)&&Player::parse_skin(tail,skin);
    const bool override_skin=valid&&std::ranges::any_of(tail,[](const wchar_t* arg){
        return std::wstring_view(arg).starts_with(L"-QDCoopSkin=");
    });
    if(arguments)LocalFree(arguments);
    if(valid){
        AcquireSRWLockExclusive(&control_lock);
        selected_skin=skin;
        skin_command_line=override_skin;
        if(core_bound)core.skin(skin);
        ReleaseSRWLockExclusive(&control_lock);
    }
    if(valid&&request.operation!=Room::Operation::none)submit(request);
    else {
        AcquireSRWLockExclusive(&control_lock);
        const auto failure=ensure_hook();
        if(snapshot.result==Room::Result::idle&&(!valid||failure!=Room::Failure::none)) {
            snapshot.result=Room::Result::rejected;
            snapshot.failure=valid?failure:Room::Failure::arguments;
        }
        ReleaseSRWLockExclusive(&control_lock);
    }
}

extern "C" __declspec(dllexport) int QDNativeHostRoom(unsigned seats,unsigned friends) {
    return submit({Room::Operation::host,seats,friends});
}
extern "C" __declspec(dllexport) int QDNativeJoinRoom(Steam::SteamId lobby) {
    return submit({Room::Operation::join,0,0,lobby});
}
extern "C" __declspec(dllexport) int QDNativeHostLan(unsigned seats,unsigned port) {
    Room::Request request{Room::Operation::host_lan,seats};
    request.port=port;
    return submit(request);
}
extern "C" __declspec(dllexport) int QDNativeJoinLan(const wchar_t* ipv4,unsigned port) {
    if(!ipv4)return 0;
    Room::Request request{Room::Operation::join_lan};
    unsigned size{};
    while(size<request.ipv4.size()&&ipv4[size])++size;
    if(size==request.ipv4.size()||!Room::set_ipv4({ipv4,size},request.ipv4))return 0;
    request.port=port;
    return submit(request);
}
extern "C" __declspec(dllexport) int QDNativeReadLanStatus(Lan::Status* out,unsigned size) {
    if(!out||size!=sizeof(*out))return 0;
    AcquireSRWLockShared(&control_lock);
    *out=lan_snapshot;
    ReleaseSRWLockShared(&control_lock);
    return 1;
}
extern "C" __declspec(dllexport) int QDNativeLeaveRoom() {
    return submit({Room::Operation::leave});
}
extern "C" __declspec(dllexport) int QDNativeSetSkin(unsigned skin) {
    return select_native_skin(skin);
}
extern "C" __declspec(dllexport) int QDNativeReadStatus(Room::Status* out,unsigned size) {
    if(!out||size!=sizeof(*out))return 0;
    AcquireSRWLockShared(&control_lock);
    *out=snapshot;
    ReleaseSRWLockShared(&control_lock);
    return 1;
}
extern "C" __declspec(dllexport) World::Phase QDNativeReadWorldPhase() {
    return world_phase.load(std::memory_order_acquire);
}
extern "C" __declspec(dllexport) int QDNativeReadSteamSocial(Social::Status* out,unsigned size) {
    if(!out||size!=sizeof(*out))return 0;
    AcquireSRWLockShared(&control_lock);
    *out=social_snapshot;
    ReleaseSRWLockShared(&control_lock);
    return 1;
}
extern "C" __declspec(dllexport) int QDNativeReadSteamInvitation(Social::Invitation* out,unsigned size) {
    if(!out||size!=sizeof(*out))return 0;
    *out=read_invitation();
    return 1;
}
extern "C" __declspec(dllexport) int QDNativeRespondSteamInvitation(std::uint64_t ticket,unsigned accept) {
    return respond_to_invitation(ticket,accept);
}
extern "C" __declspec(dllexport) int QDNativeOpenSteamFriends() {
    return request_overlay(Overlay::friends);
}
extern "C" __declspec(dllexport) int QDNativeOpenSteamInvite() {
    return request_overlay(Overlay::invite);
}

#ifdef QDCOOP_STEAM_SOCIAL_LAB
// Compiled only for the isolated callback ingress test.
extern "C" __declspec(dllexport) int QDNativeProbeSteamCallback(int id,void* data,unsigned size) {
    Social::Status status_copy{};
    QDNativeReadSteamSocial(&status_copy,sizeof(status_copy));
    auto callback=id==Social::lobby_callback?&lobby_invite:id==Social::presence_callback?&presence_invite:nullptr;
    if(!callback||status_copy.callbacks!=2||!data||size!=static_cast<unsigned>(callback->vtable->size(callback)))
        return 0;
    callback->vtable->run(callback,data);
    return 1;
}
extern "C" __declspec(dllexport) unsigned QDNativeProbeSteamPumpCount() {
    return engine_dispatches.load();
}
extern "C" __declspec(dllexport) unsigned QDNativeProbePauseInvitation() {
    const auto& pause=TitleUi::pause_invite;
    const auto& menu=TitleUi::menu;
    return static_cast<unsigned>(menu.on_title)|
        (static_cast<unsigned>(menu.ui.get(pause.pause)!=0)<<1)|
        (static_cast<unsigned>(pause.modal_ticket!=0)<<2)|
        (static_cast<unsigned>(pause.dialog_visible)<<3)|
        (static_cast<unsigned>(menu.ui.get(TitleUi::local_pawn)!=0)<<4)|
        (static_cast<unsigned>(pause.method(TitleUi::PauseMethod::open)!=0)<<5)|
        (static_cast<unsigned>(pause.method(TitleUi::PauseMethod::quit)!=0)<<6)|pause.test_observations;
}
#endif
