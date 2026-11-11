#define QDCOOP_LAN_TITLE_FIXTURE
#include "native_title_menu_probe.cpp"
#include "qdcoop/lan_room.hpp"
#include "qdcoop/native_world.hpp"
#ifdef QDCOOP_COMBAT_REWARD_FIXTURE
#include "qdcoop/enemy_discovery.hpp"
#include "qdcoop/object_count_map.hpp"
#endif
#ifdef QDCOOP_PLACED_ITEM_TITLE_FIXTURE
#include "qdcoop/world_progress_map.hpp"
#include "qdcoop/rescue_wire.hpp"
#include "qdcoop/placed_item_record.hpp"
#include <optional>
#endif
#ifdef QDCOOP_MOUNT_TITLE_FIXTURE
#include "qdcoop/visual_mount.hpp"
#include "qdcoop/enemy_discovery.hpp"
#endif
#include <cmath>

namespace {
namespace Lan=qdcoop::lan_room;
namespace World=qdcoop::native_world;
struct LanTitle {
    int (*read)(Lan::Status*,unsigned){};
    World::Phase (*world)(){};
#ifdef QDCOOP_PLACED_ITEM_TITLE_FIXTURE
    int (*host_lan)(unsigned,unsigned){};
    int (*join_lan)(const wchar_t*,unsigned){};
    Lan::Status first{};
    unsigned cycle{};
#endif
    wchar_t report[32768]{},ready[32768]{},peer[32768]{},listening[32768]{},ipv4[16]{},port[6]{};
    std::wstring role;
    unsigned stage{},seats{2};
    bool host{},validated{},selected{},leaving{},finished{};
    ULONGLONG started{},paired{};
    Lan::Status active{};
};
LanTitle lan;

bool environment_title(const wchar_t* name,wchar_t* output,DWORD capacity){
    const auto size=GetEnvironmentVariableW(name,output,capacity);
    return size&&size<capacity;
}
void marker(const wchar_t* path){
    if(auto file=_wfopen(path,L"wb")){
        std::fputs("ready\n",file);
        std::fclose(file);
    }
}
bool marker_exists(const wchar_t* path){return GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES;}

#ifdef QDCOOP_COMBAT_REWARD_FIXTURE
#include "combat_reward_title_fixture.inc"
#endif
#ifdef QDCOOP_WORLD_JOIN_FIXTURE
#include "world_join_title_fixture.inc"
#endif
#ifdef QDCOOP_PROJECTILE_TITLE_FIXTURE
#include "projectile_title_fixture.inc"
#endif
#ifdef QDCOOP_ATTACHED_FX_TITLE_FIXTURE
#include "attached_fx_title_fixture.inc"
#endif
#ifdef QDCOOP_SOUND_TITLE_FIXTURE
#include "sound_title_fixture.inc"
#endif
#ifdef QDCOOP_SOUND_LOOP_TITLE_FIXTURE
#include "sound_loop_title_fixture.inc"
#endif
#ifdef QDCOOP_PLACED_ITEM_TITLE_FIXTURE
#include "placed_item_title_fixture.inc"
#endif
#ifdef QDCOOP_MOUNT_TITLE_FIXTURE
#include "visual_mount_title_fixture.inc"
#endif
#ifdef QDCOOP_HEALTHBAR_TITLE_FIXTURE
#include "healthbar_title_fixture.inc"
#endif
#ifdef QDCOOP_PEER_RANGE_FIXTURE
#include "peer_range_title_fixture.inc"
#endif

void report_lan_title(const Lan::Status& final,bool success){
    if(auto file=_wfopen(lan.report,L"wb")){
        std::fprintf(file,"{\"passed\":%s,\"host\":%s,\"stage\":%u,\"invalid_rejected\":%s,\"stock_selected\":%s,"
            "\"epoch\":%u,\"seats\":%u,\"connected\":%u,\"slot\":%u,\"mask\":%u,\"scope\":\"%llx.%llx\","
            "\"phase\":%u,\"failure\":%u,\"ue4ss_absent\":%s}\n",
            success?"true":"false",lan.host?"true":"false",lan.stage,lan.validated?"true":"false",lan.selected?"true":"false",
            lan.active.epoch,lan.active.seats,lan.active.connected,lan.active.slot,lan.active.peer_mask,
            static_cast<unsigned long long>(lan.active.room.high),static_cast<unsigned long long>(lan.active.room.low),
            static_cast<unsigned>(final.phase),static_cast<unsigned>(final.failure),GetModuleHandleW(L"UE4SS.dll")?"false":"true");
        std::fclose(file);
    }
}
void finish_lan_title(Address instance,const Lan::Status& final,bool success){
    report_lan_title(final,success);
    if(instance&&quit_fn&&quit_cdo){
        alignas(8) unsigned char args[18]{};
        std::memcpy(args,&instance,8);
        args[17]=1;
        lan.finished=true;
        TitleUi::event(quit_cdo,quit_fn,args);
    }else lan.finished=request_unreal_exit();
}

Address find_edit(Address options,unsigned index){
    if(!ui.is(options,ui.get(TitleUi::vertical_box_type)))return 0;
    for(int i=0,count=ui.count(options);i<count&&i<24;++i){
        const auto row=ui.child(options,i);
        if(!ui.is(row,ui.get(TitleUi::size_type)))continue;
        const auto edit=ui.child(row,0);
        if(ui.is(edit,ui.get(TitleUi::edit_type))&&index--==0)return edit;
    }
    return 0;
}
void click_title(Address widget){TitleUi::event(widget,ui.get(TitleUi::click),nullptr);}
void pick_original_save(Address title){
    struct Args {int slot;bool backup;unsigned char padding[3];int backup_slot;} args{1,false,{},0};
    TitleUi::event(title,ui.get(TitleUi::load_slot),&args);
    lan.selected=true;
}

void lan_title_menu(Address title,Address options,bool in_save,const Room::Status& room){
    if(lan.host){
        if(lan.stage>=2&&lan.stage<=4){
            constexpr const wchar_t* captions[]{L"Players: 2",L"Players: 3",L"Players: 4"};
            const auto seats=named_button(options,captions[lan.stage-2]);
            if(!seats)return;
            if(lan.stage<4||lan.seats==2)click_title(seats);
        }else if(lan.stage==5||lan.stage==6){
            const auto port=find_edit(options,0),choose=named_button(options,L"Host game");
            if(!port||!choose||in_save||room.result!=Room::Result::idle||
               !named_button(options,lan.seats==4?L"Players: 4":L"Players: 2"))return;
            if(lan.stage==5&&ui.read_field(port,5)!=L"47771")return;
            lan.validated=lan.stage==6;
            ui.label(port,lan.validated?lan.port:L"42",TitleUi::edit_set);
            click_title(choose);
        }else if(lan.stage==7){
            if(!in_save)return;
#ifdef QDCOOP_PLACED_ITEM_TITLE_FIXTURE
            if(PlacedItemFixture::legacy_mode()&&!PlacedItemFixture::legacy_seed_title())return;
#endif
            pick_original_save(title);
        }
    }else if(lan.stage==2||lan.stage==3){
        const auto address=find_edit(options,0),port=find_edit(options,1),connect=named_button(options,L"Connect");
        if(!address||!port||!connect||in_save||room.result!=Room::Result::idle)return;
        if(lan.stage==2&&(ui.read_field(address,15)!=L"127.0.0.1"||ui.read_field(port,5)!=L"47771"))return;
        if(lan.stage==3&&!marker_exists(lan.listening))return;
        lan.validated=lan.stage==3;
        ui.label(address,lan.validated?lan.ipv4:L"999.0.1.1",TitleUi::edit_set);
        ui.label(port,lan.port,TitleUi::edit_set);
        click_title(connect);
    }else if(lan.stage==4){
        if(!in_save||room.result!=Room::Result::running)return;
        pick_original_save(title);
    }
    ++lan.stage;
}

void lan_title_tick(void* engine,float dt,bool idle){
    title_original.load(std::memory_order_acquire)(engine,dt,idle);
    if(lan.finished)return;
    const auto now=GetTickCount64();
    if(!lan.started)lan.started=now;
    const auto instance=ui.image?ui.object(reinterpret_cast<Address>(engine),"GameInstance"):0;
    Lan::Status status;
    Room::Status room;
    if(now-lan.started>85000){
#ifdef QDCOOP_SOUND_TITLE_FIXTURE
        SoundFixture::cleanup();
#endif
#ifdef QDCOOP_ATTACHED_FX_TITLE_FIXTURE
        AttachedFxFixture::cleanup();
#endif
#ifdef QDCOOP_SOUND_LOOP_TITLE_FIXTURE
        SoundLoopFixture::cleanup();
#endif
        finish_lan_title(instance,status,false);
        return;
    }
    if(now<not_before||!read_status(&room,sizeof(room))||!lan.read(&status,sizeof(status)))return;
    not_before=now+150;
#ifdef QDCOOP_WORLD_JOIN_FIXTURE
    WorldJoinFixture::observe(instance);
    wchar_t failure_test[2]{};
    if(environment_title(L"QD_WORLD_JOIN_FAILURE",failure_test,2)&&WorldJoinFixture::failed){
        finish_lan_title(instance,status,WorldJoinFixture::joining&&lan.validated&&
            room.result==Room::Result::closed&&status.failure==Lan::Failure::timeout);
        return;
    }
#endif
    if(lan.selected){
#ifdef QDCOOP_PLACED_ITEM_TITLE_FIXTURE
        if(lan.cycle==1){
            const auto port=static_cast<unsigned>(std::wcstoul(lan.port,nullptr,10));
            if(PlacedItemFixture::all(L"cycle-hook-restored")&&
               (lan.host?lan.host_lan(lan.seats,port):
                PlacedItemFixture::seen(L"host-reopen-listening.txt")&&lan.join_lan(lan.ipv4,port)))lan.cycle=2;
            return;
        }
        if(lan.cycle==2&&lan.host&&status.phase==Lan::Phase::waiting&&status.room)
            PlacedItemFixture::mark(L"reopen-listening");
#endif
        if(lan.host&&room.result==Room::Result::running&&status.phase==Lan::Phase::waiting&&status.seats==lan.seats&&status.room
#ifdef QDCOOP_WORLD_JOIN_FIXTURE
           &&WorldJoinFixture::seed(instance)
#endif
        )
            marker(lan.listening);
        if(!lan.active.room&&room.result==Room::Result::running&&status.phase==Lan::Phase::active&&
           lan.world()==World::Phase::active&&status.epoch==1&&status.seats==lan.seats&&status.connected==lan.seats-1){
            lan.active=status;
            marker(lan.ready);
#ifdef QDCOOP_PLACED_ITEM_TITLE_FIXTURE
            if(lan.cycle==2){
                if(lan.active.room==lan.first.room){finish_lan_title(instance,status,false);return;}
                lan.cycle=3;
                PlacedItemFixture::mark(L"reopened");
            }
#endif
        }
        bool release_allowed=true;
#ifdef QDCOOP_WORLD_JOIN_FIXTURE
        release_allowed=lan.active.room&&WorldJoinFixture::check(instance);
#endif
#ifdef QDCOOP_COMBAT_REWARD_FIXTURE
        release_allowed=lan.active.room&&CombatRewardFixture::step(instance,now);
#endif
#ifdef QDCOOP_PROJECTILE_TITLE_FIXTURE
        release_allowed=lan.active.room&&ProjectileFixture::step(instance,now);
#endif
#ifdef QDCOOP_ATTACHED_FX_TITLE_FIXTURE
        release_allowed=lan.active.room&&AttachedFxFixture::step(instance,now);
#endif
#ifdef QDCOOP_SOUND_TITLE_FIXTURE
        release_allowed=lan.active.room&&SoundFixture::step(instance,now);
#endif
#ifdef QDCOOP_SOUND_LOOP_TITLE_FIXTURE
        release_allowed=lan.active.room&&SoundLoopFixture::step(instance,now);
#endif
#ifdef QDCOOP_PLACED_ITEM_TITLE_FIXTURE
        release_allowed=lan.active.room&&PlacedItemFixture::step(instance,now);
        if(lan.host&&lan.active.room&&!lan.leaving&&!lan.cycle&&PlacedItemFixture::cycle_ready())
            lan.leaving=leave_room()!=0;
#endif
#ifdef QDCOOP_MOUNT_TITLE_FIXTURE
        release_allowed=lan.active.room&&MountFixture::step(instance,now);
#endif
#ifdef QDCOOP_HEALTHBAR_TITLE_FIXTURE
        release_allowed=release_allowed&&HealthbarFixture::step(instance,now);
#ifdef QDCOOP_PEER_RANGE_FIXTURE
        release_allowed=release_allowed&&PeerRangeFixture::step(instance,now);
#endif
        wchar_t exit_active[2]{};
        if(lan.host&&release_allowed&&status.phase==Lan::Phase::active&&
           environment_title(L"QD_HEALTHBAR_ACTIVE_EXIT",exit_active,2)&&exit_active[0]==L'1'){
            finish_lan_title(instance,status,lan.validated&&!GetModuleHandleW(L"UE4SS.dll"));
            return;
        }
#endif
        if(lan.host&&lan.active.room&&release_allowed&&!lan.leaving&&marker_exists(lan.peer)){
            if(!lan.paired)lan.paired=now;
            if(now-lan.paired>=750)lan.leaving=leave_room()!=0;
        }
        if(lan.active.room&&room.result==Room::Result::closed){
#ifdef QDCOOP_WORLD_JOIN_FIXTURE
            if(!lan.host&&!WorldJoinFixture::left)return;
#endif
            const bool clean=lan.host?(lan.leaving&&status.phase==Lan::Phase::closed&&status.failure==Lan::Failure::none):
                (status.phase==Lan::Phase::failed&&status.failure==Lan::Failure::departure);
            bool success=clean&&lan.validated&&!GetModuleHandleW(L"UE4SS.dll");
#ifdef QDCOOP_PLACED_ITEM_TITLE_FIXTURE
            if(!lan.cycle&&PlacedItemFixture::seen(L"host-cycle-requested.txt")){
                if(!success||!PlacedItemFixture::restored(true)){finish_lan_title(instance,status,false);return;}
                lan.first=lan.active;
                lan.active={};
                lan.paired=0;
                lan.leaving=false;
                lan.cycle=1;
                return;
            }
            success=success&&PlacedItemFixture::restored();
#endif
            finish_lan_title(instance,status,success);
        }
        return;
    }
    if(!ui.ready){ui.step(8192);return;}
    if(!ui.get(ui.active_title)&&!ui.find_title())return;
    if(!get_text||!text_to_string||!quit_cdo||!quit_fn)find_caption_methods();
    const auto title=ui.get(ui.active_title),source=ui.object(title,"LoadGameButton");
    const auto box=source?ui.parent(source):0;
    const auto panel=ui.object(instance,"CurrentOptionsMenu");
    const bool visible=ui.is(panel,ui.get(TitleUi::options_type))&&ui.shown(panel);
    const auto video=visible?ui.object(panel,"TopLevel_VideoButton"):0, row=video?ui.parent(video):0;
    const auto options=row?ui.parent(row):0;
    const auto save=ui.member(title,"InSaveMenu","BoolProperty",1);
    const bool in_save=save&&UE::get_bool(reinterpret_cast<void*>(save),reinterpret_cast<const void*>(title),0);
    if(lan.stage==0||lan.stage==1){
        const auto button=lan.stage==0?named_button(box,L"Multiplayer"):
            named_button(options,lan.host?L"Host LAN game":L"Join LAN game");
        if(!button)return;
        click_title(button);
        ++lan.stage;
    }else lan_title_menu(title,options,in_save,room);
}

DWORD WINAPI start_lan_title(void*){
    wchar_t role[16]{};
    if(!environment_title(L"QD_LAN_BOOT_ROLE",role,16)||!environment_title(L"QD_LAN_BOOT_REPORT",lan.report,32768)||
       !environment_title(L"QD_LAN_BOOT_READY",lan.ready,32768)||!environment_title(L"QD_LAN_BOOT_PEER",lan.peer,32768)||
       !environment_title(L"QD_LAN_TITLE_LISTENING",lan.listening,32768)||!environment_title(L"QD_LAN_BOOT_IPV4",lan.ipv4,16)||
       !environment_title(L"QD_LAN_TITLE_PORT",lan.port,6))return 1;
    lan.host=std::wcscmp(role,L"host")==0;
    lan.role=role;
    wchar_t seats[3]{};
    if(environment_title(L"QD_LAN_BOOT_SEATS",seats,3)&&std::wcscmp(seats,L"4")==0)lan.seats=4;
    const auto deadline=GetTickCount64()+30000;
    do{
        const auto module=GetModuleHandleW(L"QDNativeBootstrap.dll");
        if(module&&load_export(module,"QDNativeReadStatus",read_status)&&load_export(module,"QDNativeLeaveRoom",leave_room)&&
           load_export(module,"QDNativeReadLanStatus",lan.read)&&load_export(module,"QDNativeReadWorldPhase",lan.world)&&
#ifdef QDCOOP_PLACED_ITEM_TITLE_FIXTURE
           load_export(module,"QDNativeHostLan",lan.host_lan)&&load_export(module,"QDNativeJoinLan",lan.join_lan)&&
#endif
           attach_title_observer(module,&lan_title_tick))return 0;
        Sleep(100);
    }while(GetTickCount64()<deadline);
    report_lan_title({},false);
    return 1;
}
}

BOOL WINAPI DllMain(HINSTANCE,DWORD reason,void*){
    if(reason==DLL_PROCESS_ATTACH)
        if(const auto thread=CreateThread(nullptr,0,start_lan_title,nullptr,0,nullptr))CloseHandle(thread);
    return TRUE;
}
