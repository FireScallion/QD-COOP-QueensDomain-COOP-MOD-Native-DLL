#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstdlib>
#include "qdcoop/steam_room.hpp"
#include "qdcoop/enemy_enrollment.hpp"

namespace E=qdcoop::enemy;
namespace Wire=E::enrollment;
struct ReceivedPacket { E::Packet packet{}; unsigned peer{}; };
struct TargetedPacket { E::Packet packet{}; unsigned peers{}; };
static E::Spsc<ReceivedPacket,1024> incoming;
static E::Spsc<TargetedPacket,1024> outgoing;
static HANDLE stop_event{},wake_event{};
static bool host_role;
static std::atomic<bool> stopped{};
static std::atomic<int> build_state{};
static std::atomic<std::uint64_t> tx{},rx{},worker_drops{},foreign{},socket_errors{};
static unsigned active_peer_mask;
static E::Envelope envelope{};
template<class Function>
static bool proc(HMODULE module,Function& function,const char* name){
    const auto address=GetProcAddress(module,name);
    std::memcpy(&function,&address,sizeof(function));
    return function!=nullptr;
}
static bool verified_game(){return false;}
#include "../adapters/ue4ss/steam_transport.inc"
#include "steam_recovery_fixture.inc"

namespace {
using namespace RecoveryFixture;

void check(bool condition,const char* label){
    if(condition)return;
    const auto status=read_steam_status();
    std::fprintf(stderr,"FAIL %s (phase %u epoch %u members %u build %d)\n",
                 label,status.phase,status.epoch,status.members,build_state.load());
    std::exit(1);
}

template<class Predicate>
bool wait_for(Predicate predicate,unsigned milliseconds=4500){
    const auto deadline=GetTickCount64()+milliseconds;
    do {
        if(predicate())return true;
        Sleep(5);
    }while(GetTickCount64()<deadline);
    return false;
}

struct Run {
    Fixture& fixture;
    HANDLE thread{};
    SR::Failure result{};
    static DWORD WINAPI entry(void* context){
        auto& run=*static_cast<Run*>(context);
        SR::Status status{};
        status.lobby=lobby;
        run.result=steam_session(run.fixture.api(),status,run.fixture.self);
        return 0;
    }
    explicit Run(Fixture& environment,bool host_side):fixture(environment){
        host_role=host_side;
        steam_request={host_side?0:lobby,host_side?4u:0u,false};
        steam_transport=true;
        steam_committed=false;
        steam_recovery.reset();
        publish_steam_status({});
        build_state=0;
        incoming.discard_quiescent();
        outgoing.discard_quiescent();
        stop_event=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        wake_event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        check(stop_event&&wake_event,"fixture events");
        thread=CreateThread(nullptr,0,&entry,this,0,nullptr);
        check(thread!=nullptr,"fixture worker");
    }
    ~Run(){
        SetEvent(stop_event);
        WaitForSingleObject(thread,INFINITE);
        CloseHandle(thread);
        CloseHandle(stop_event);
        CloseHandle(wake_event);
    }
};

SR::Status phase(SR::Phase wanted,unsigned epoch){
    SR::Status status{};
    check(wait_for([&]{
        status=read_steam_status();
        return status.phase==static_cast<unsigned>(wanted)&&status.epoch==epoch&&
            (wanted!=SR::Phase::active||build_state.load()==1);
    }),"expected phase and epoch");
    return status;
}

void game_ack(unsigned serial){
    check(!transport_ready()&&steam_recovery.state(serial)==SR::Recovery::waiting,"quiescent game handoff");
    incoming.discard_quiescent();
    outgoing.discard_quiescent();
    steam_committed=false;
    check(steam_recovery.finish(serial,SR::Recovery::clean),"game acknowledged epoch");
}

void epoch_records(){
    SR::Roster roster{},different{};
    check(SR::make_roster(host,{host,a,b,c},4,roster)&&
          SR::make_roster(host,{host,a,b,replacement},4,different),"epoch roster construction");
    const SR::Epoch waiting{4,901,{}},locked{4,901,roster};
    SR::Epoch parsed{};
    check(SR::decode_epoch(SR::encode_epoch(locked),host,parsed)&&parsed.locked==roster&&
          SR::epoch_change(waiting,parsed)==SR::EpochChange::same,"atomic epoch record and lock");
    check(SR::epoch_change(locked,waiting)==SR::EpochChange::stale&&
          SR::epoch_change(locked,{4,901,different})==SR::EpochChange::invalid&&
          SR::epoch_change(locked,{5,901,roster})==SR::EpochChange::invalid&&
          SR::epoch_change(locked,{5,902,roster})==SR::EpochChange::next,"epoch rollback and scope rules");
    check(!SR::decode_epoch("0/1/",host,parsed)&&
          !SR::decode_epoch("4294967296/1/",host,parsed)&&
          !SR::decode_epoch("1/18446744073709551616/",host,parsed)&&
          !SR::decode_epoch(SR::encode_epoch(locked),replacement,parsed),"invalid epoch inputs rejected");
}

void host_recovery(){
    Fixture fixture;
    Run run(fixture,true);
    const auto first=phase(SR::Phase::active,1);
    check(first.peer_mask==7&&transport_ready()&&envelope.room_low==first.nonce,"initial four-person link");
    check(!fixture.joinable(),"initial locked lobby");
    fixture.set_roster({host,a,b,0},3);
    phase(SR::Phase::recovering,1);
    check(fixture.closed_channels()==3&&!fixture.joinable(),"closed all old peer channels");
    E::Packet old{};
    Wire::write_header({lobby,first.nonce,1,1},Wire::offer_kind,64,old);
    check(outgoing.push({old,7}),"seed paused old queue");
    check(!steam_recovery.finish(2,SR::Recovery::clean),"reject future game ack");
    check(!wait_for([&]{return fixture.joinable()||fixture.game_packets()!=0;},125),
          "no admission or traffic before game cleanup");
    game_ack(1);
    const auto waiting=phase(SR::Phase::waiting,2);
    check(waiting.lobby==lobby&&waiting.members==3&&waiting.nonce!=first.nonce&&
          fixture.joinable()&&!fixture.read_epoch().locked.count,"same lobby opens one seat");
    const auto previous_foreign=foreign.load();
    fixture.inject(a,old);
    fixture.set_roster({host,a,b,replacement},4);
    const auto second=phase(SR::Phase::active,2);
    check(second.peer_mask==7&&transport_ready()&&envelope.room_low==second.nonce&&
          fixture.read_epoch().locked.members[3]==replacement,"fresh link with authenticated replacement");
    check(wait_for([&]{return foreign.load()>previous_foreign;})&&fixture.game_packets()==0,
          "old packet and old queue discarded");
    fixture.set_roster({host,a,b,replacement},4,a);
    check(WaitForSingleObject(run.thread,3500)==WAIT_OBJECT_0&&run.result==SR::Failure::membership,
          "ownership transfer is terminal");
}

void guest_recovery(){
    Fixture fixture;
    fixture.self=a;
    fixture.metadata={{"mod","QDCoop"},{"transport",SR::transport},{"build",SR::build},
                      {"owner",std::to_string(host)},{"qd_target","4"}};
    SR::Roster roster{};
    check(SR::make_roster(host,{host,a,b,c},4,roster),"fixture roster");
    const SR::Epoch first{1,301,roster};
    fixture.set_epoch(first);
    Run run(fixture,false);
    phase(SR::Phase::active,1);
    check(transport_ready()&&active_peer_mask==1,"guest authenticated host");
    fixture.set_epoch({2,302,{}});
    phase(SR::Phase::recovering,1);
    fixture.set_epoch(first);
    game_ack(1);
    check(!wait_for([]{return transport_ready();},150),"guest does not resume a cached epoch");
    fixture.set_epoch({2,302,roster});
    phase(SR::Phase::active,2);
    check(transport_ready()&&envelope.room_low==302,"guest binds the next host epoch");
    fixture.set_roster({host,a,b,0},3);
    phase(SR::Phase::recovering,2);
    check(steam_recovery.finish(2,SR::Recovery::blocked),"game blocks unsettled durable work");
    check(WaitForSingleObject(run.thread,3500)==WAIT_OBJECT_0&&run.result==SR::Failure::durable,
          "durable block ends the room");
}
}

int main(){
    // The real entry is exercised separately in the isolated Steam smoke.
    (void)&steam_network;
    epoch_records();
    host_recovery();
    guest_recovery();
    std::puts("PASS Steam worker rekey, replacement, old packets, host loss and durable refusal");
}
