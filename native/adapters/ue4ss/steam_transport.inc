#include "qdcoop/checkpoint_wire.hpp"
#include "qdcoop/world_progress.hpp"
#include "qdcoop/enemy_damage.hpp"

namespace SR=qdcoop::steam_room;
using SteamId=SR::SteamId;
struct SteamIdentity { alignas(8) unsigned char bytes[136]{}; };
struct SteamMessagePrefix {
    const void* data{};
    int size{},connection{};
    SteamIdentity identity{};
};
static_assert(offsetof(SteamMessagePrefix,identity)==16);
struct LobbyCreated { int result{}; SteamId lobby{}; };
struct LobbyEntered { SteamId lobby{}; unsigned permissions{}; bool locked{}; unsigned response{}; };
static_assert(sizeof(LobbyCreated)==16&&offsetof(LobbyCreated,lobby)==8);
static_assert(sizeof(LobbyEntered)==24&&offsetof(LobbyEntered,response)==16);

struct SteamApi {
    void *user{},*utils{},*apps{},*matchmaking{},*messages{},*networking{};
    int (*pipe)(){};
    void (*release_thread)(){};
    unsigned (*app)(void*){};
    bool (*logged_on)(void*){},(*subscribed)(void*){};
    SteamId (*self)(void*){};
    bool (*complete)(void*,SteamId,bool*){},(*result)(void*,SteamId,void*,int,int,bool*){};
    SteamId (*create)(void*,int,int){},(*join)(void*,SteamId){},(*owner)(void*,SteamId){};
    void (*leave)(void*,SteamId){};
    int (*count)(void*,SteamId){},(*limit)(void*,SteamId){};
    SteamId (*member)(void*,SteamId,int){};
    bool (*set_data)(void*,SteamId,const char*,const char*){},(*joinable)(void*,SteamId,bool){},
         (*visibility)(void*,SteamId,int){};
    const char* (*get_data)(void*,SteamId,const char*){};
    void (*relay)(void*){};
    void (*set_identity)(SteamIdentity*,SteamId){};
    SteamId (*get_identity)(const SteamIdentity*){};
    int (*send_message)(void*,const SteamIdentity*,const void*,unsigned,int,int){};
    int (*receive)(void*,int,SteamMessagePrefix**,int){};
    bool (*close_channel)(void*,const SteamIdentity*,int){};
    void (*release_message)(SteamMessagePrefix*){};

    bool bind(HMODULE module,SR::Status& status){
        void* (*get_user)(){},*(*get_utils)(){},*(*get_apps)(){},*(*get_matchmaking)(){},
            *(*get_messages)(){},*(*get_networking)(){};
#define STEAM(field, name) if(!proc(module,field,"SteamAPI_" name))return false
        STEAM(pipe,"GetHSteamPipe");
        STEAM(release_thread,"ReleaseCurrentThreadMemory");
        STEAM(get_user,"SteamUser_v023");
        STEAM(get_utils,"SteamUtils_v010");
        STEAM(get_apps,"SteamApps_v008");
        STEAM(get_matchmaking,"SteamMatchmaking_v009");
        STEAM(get_messages,"SteamNetworkingMessages_SteamAPI_v002");
        STEAM(get_networking,"SteamNetworkingUtils_SteamAPI_v004");
        STEAM(app,"ISteamUtils_GetAppID");
        STEAM(logged_on,"ISteamUser_BLoggedOn");
        STEAM(subscribed,"ISteamApps_BIsSubscribed");
        STEAM(self,"ISteamUser_GetSteamID");
        STEAM(complete,"ISteamUtils_IsAPICallCompleted");
        STEAM(result,"ISteamUtils_GetAPICallResult");
        STEAM(create,"ISteamMatchmaking_CreateLobby");
        STEAM(join,"ISteamMatchmaking_JoinLobby");
        STEAM(owner,"ISteamMatchmaking_GetLobbyOwner");
        STEAM(leave,"ISteamMatchmaking_LeaveLobby");
        STEAM(count,"ISteamMatchmaking_GetNumLobbyMembers");
        STEAM(limit,"ISteamMatchmaking_GetLobbyMemberLimit");
        STEAM(member,"ISteamMatchmaking_GetLobbyMemberByIndex");
        STEAM(set_data,"ISteamMatchmaking_SetLobbyData");
        STEAM(get_data,"ISteamMatchmaking_GetLobbyData");
        STEAM(joinable,"ISteamMatchmaking_SetLobbyJoinable");
        STEAM(visibility,"ISteamMatchmaking_SetLobbyType");
        STEAM(relay,"ISteamNetworkingUtils_InitRelayNetworkAccess");
        STEAM(set_identity,"SteamNetworkingIdentity_SetSteamID64");
        STEAM(get_identity,"SteamNetworkingIdentity_GetSteamID64");
        STEAM(send_message,"ISteamNetworkingMessages_SendMessageToUser");
        STEAM(receive,"ISteamNetworkingMessages_ReceiveMessagesOnChannel");
        STEAM(close_channel,"ISteamNetworkingMessages_CloseChannelWithUser");
        STEAM(release_message,"SteamNetworkingMessage_t_Release");
#undef STEAM
        if(!pipe()){
            int (*initialize)(char*){};
            if(!proc(module,initialize,"SteamAPI_InitFlat"))return false;
            char error[1024]{};
            status.init_result=initialize(error);
            if(status.init_result||!pipe())return false;
            status.initialized_here=1;
        }
        user=get_user();utils=get_utils();apps=get_apps();matchmaking=get_matchmaking();
        messages=get_messages();networking=get_networking();
        return user&&utils&&apps&&matchmaking&&messages&&networking;
    }
};

struct SteamRequest { SteamId lobby{}; unsigned expected{}; bool friends{}; };
static SteamRequest steam_request;
static SRWLOCK steam_status_lock=SRWLOCK_INIT;
static SR::Status steam_status;
static bool steam_transport,steam_committed;
static SR::RecoveryGate steam_recovery;
static std::atomic<SteamId> steam_local_account{};
static std::array<std::atomic<SteamId>,3> steam_peer_accounts{};

static void publish_steam_status(const SR::Status& status){
    AcquireSRWLockExclusive(&steam_status_lock);
    steam_status=status;
    ReleaseSRWLockExclusive(&steam_status_lock);
}
static SR::Status read_steam_status(){
    AcquireSRWLockShared(&steam_status_lock);
    const auto status=steam_status;
    ReleaseSRWLockShared(&steam_status_lock);
    return status;
}
static void steam_phase(SR::Status& status,SR::Phase phase){
    status.phase=static_cast<unsigned>(phase);
    publish_steam_status(status);
}
static bool steam_stopping(){return WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0;}
static SteamId steam_number(std::string_view text){
    SteamId value{};
    const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    return result.ec==std::errc{}&&result.ptr==text.data()+text.size()?value:0;
}

static std::string steam_data(const SteamApi& api,SteamId lobby,const char* key){
    const auto value=api.get_data(api.matchmaking,lobby,key);
    return value?std::string(value):std::string{};
}
static bool steam_set_number(const SteamApi& api,SteamId lobby,const char* key,SteamId value){
    return api.set_data(api.matchmaking,lobby,key,std::to_string(value).c_str());
}
static bool steam_roster(const SteamApi& api,SteamId lobby,SR::Roster& roster){
    const auto count=api.count(api.matchmaking,lobby);
    if(count<1||count>static_cast<int>(qdcoop::max_players))return false;
    std::array<SteamId,qdcoop::max_players> members{};
    for(int i=0;i<count;++i)members[i]=api.member(api.matchmaking,lobby,i);
    return SR::make_roster(api.owner(api.matchmaking,lobby),members,static_cast<unsigned>(count),roster);
}
static bool steam_member(const SR::Roster& roster,SteamId self){
    return std::binary_search(roster.members.begin(),roster.members.begin()+roster.count,self);
}

static SR::Failure steam_enter(const SteamApi& api,SR::Status& status){
    const auto call=host_role?api.create(api.matchmaking,0,static_cast<int>(steam_request.expected)):
        api.join(api.matchmaking,steam_request.lobby);
    if(!call)return SR::Failure::call;
    const auto deadline=GetTickCount64()+15000;
    do{
        bool failed{};
        if(api.complete(api.utils,call,&failed)){
            if(failed)return SR::Failure::call;
            if(host_role){
                LobbyCreated result{};
                if(!api.result(api.utils,call,&result,sizeof(result),513,&failed)||failed||
                   result.result!=1||!SR::lobby_id(result.lobby))return SR::Failure::call;
                status.lobby=result.lobby;
            }else{
                LobbyEntered result{};
                if(!api.result(api.utils,call,&result,sizeof(result),504,&failed)||failed)return SR::Failure::call;
                status.enter_response=result.response;
                if(result.response!=1||result.lobby!=steam_request.lobby)return SR::Failure::call;
                status.lobby=result.lobby;
            }
            return SR::Failure::none;
        }
        api.release_thread();
        // A pending lobby call can still finish after cancellation. Observe
        // its result and leave any room it acquired before exiting.
        Sleep(20);
    }while(GetTickCount64()<deadline);
    return SR::Failure::timeout;
}

static bool steam_publish_room(const SteamApi& api,SR::Status& status,SteamId self){
    const auto lobby=status.lobby;
    status.expected=steam_request.expected;
    return api.joinable(api.matchmaking,lobby,false)&&
        api.set_data(api.matchmaking,lobby,"mod","QDCoop")&&
        api.set_data(api.matchmaking,lobby,"transport",SR::transport)&&
        api.set_data(api.matchmaking,lobby,"build",SR::build)&&
        steam_set_number(api,lobby,"owner",self)&&
        steam_set_number(api,lobby,"qd_target",status.expected)&&
        (!steam_request.friends||api.visibility(api.matchmaking,lobby,1));
}

static bool steam_publish_epoch(const SteamApi& api,SteamId lobby,const SR::Epoch& epoch){
    const auto encoded=SR::encode_epoch(epoch);
    return api.set_data(api.matchmaking,lobby,"qd_epoch",encoded.c_str());
}

static bool steam_open_epoch(const SteamApi& api,SR::Status& status,SteamId self,SR::Epoch& epoch){
    SteamId nonce{};
    if(epoch.serial==UINT32_MAX||api.owner(api.matchmaking,status.lobby)!=self||
       BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&nonce),sizeof(nonce),
                       BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0||!nonce||nonce==epoch.nonce)
        return false;
    const SR::Epoch next{epoch.serial+1,nonce,{}};
    if(!steam_publish_epoch(api,status.lobby,next)||!api.joinable(api.matchmaking,status.lobby,true))
        return false;
    epoch=next;
    status.epoch=epoch.serial;
    status.nonce=epoch.nonce;
    return true;
}

static bool steam_read_epoch(const SteamApi& api,SteamId lobby,SteamId owner,SR::Epoch& epoch){
    return SR::decode_epoch(steam_data(api,lobby,"qd_epoch"),owner,epoch);
}

static SR::Failure steam_wait_roster(const SteamApi& api,SR::Status& status,SteamId self,
    SteamId& room_owner,SR::Epoch& epoch,std::uint32_t minimum,SR::Roster& locked){
    const auto lobby=status.lobby;
    const auto metadata_deadline=GetTickCount64()+(minimum>1?45000:5000);
    ULONGLONG invalid_since{};
    while(!steam_stopping()){
        if(!api.logged_on(api.user))return SR::Failure::unavailable;
        SR::Roster roster{};
        if(!steam_roster(api,lobby,roster)){
            const auto time=GetTickCount64();
            if(!invalid_since)invalid_since=time;
            if(time-invalid_since>=1000)return SR::Failure::membership;
            api.release_thread();
            WaitForSingleObject(stop_event,100);
            continue;
        }
        invalid_since=0;
        status.members=roster.count;
        status.limit=static_cast<unsigned>(api.limit(api.matchmaking,lobby));
        if(status.limit<2||status.limit>qdcoop::max_players||(status.expected&&status.limit!=status.expected)||
           !steam_member(roster,self)||
           (room_owner&&room_owner!=roster.owner)||(!host_role&&roster.owner==self))return SR::Failure::membership;
        room_owner=roster.owner;
        if(!host_role){
            const auto expected=steam_number(steam_data(api,lobby,"qd_target"));
            SR::Epoch current{};
            const bool valid=steam_data(api,lobby,"mod")=="QDCoop"&&
                steam_data(api,lobby,"transport")==SR::transport&&steam_data(api,lobby,"build")==SR::build&&
                steam_number(steam_data(api,lobby,"owner"))==room_owner&&expected==status.limit&&
                steam_read_epoch(api,lobby,room_owner,current);
            if(!valid){
                if(epoch.serial>=minimum||GetTickCount64()>=metadata_deadline)return SR::Failure::metadata;
            }else if(current.serial>=minimum){
                const auto change=SR::epoch_change(epoch,current);
                if(change==SR::EpochChange::invalid)return SR::Failure::metadata;
                if(change!=SR::EpochChange::stale){
                    epoch=current;
                    status.nonce=epoch.nonce;
                    status.epoch=epoch.serial;
                    status.expected=static_cast<unsigned>(expected);
                }
            }else if(GetTickCount64()>=metadata_deadline){
                return SR::Failure::recovery;
            }
        }
        steam_phase(status,SR::Phase::waiting);
        if(status.expected&&roster.count>status.expected)return SR::Failure::membership;
        if(epoch.serial>=minimum&&roster.count==status.expected){
            if(host_role){
                epoch.locked=roster;
                if(!api.joinable(api.matchmaking,lobby,false)||!steam_publish_epoch(api,lobby,epoch))
                    return SR::Failure::metadata;
            }
            if(epoch.locked==roster){
                locked=roster;
                return SR::Failure::none;
            }
        }
        api.release_thread();
        WaitForSingleObject(stop_event,100);
    }
    return SR::Failure::none;
}

struct SteamLink {
    SR::Peers peers{};
    std::array<SteamIdentity,3> identities{};
    std::array<ULONGLONG,3> last_seen{};
    SR::Handshake handshake{};
};

static int steam_send(const SteamApi& api,const SteamIdentity& peer,const E::Packet& packet,int flags){
    return api.send_message(api.messages,&peer,packet.data.data(),packet.size,flags,SR::channel);
}
static constexpr int steam_control_flags=8|1|32; // Reliable, NoNagle, AutoRestart.
static constexpr int steam_data_flags=1|32; // Unreliable, NoNagle, AutoRestart.

static void steam_receive(const SteamApi& api,SR::Status& status,SteamLink& link,bool active,ULONGLONG time){
    std::array<SteamMessagePrefix*,32> batch{};
    for(unsigned i=0;i<8;++i){
        const auto count=api.receive(api.messages,SR::channel,batch.data(),static_cast<int>(batch.size()));
        if(count<=0)break;
        for(int j=0;j<count&&j<static_cast<int>(batch.size());++j){
            auto message=batch[j];
            if(!message)continue;
            SteamMessagePrefix prefix{};
            std::memcpy(&prefix,message,sizeof(prefix));
            const auto slot=link.peers.find(api.get_identity(&prefix.identity));
            E::Packet packet{};
            if(slot<0||!prefix.data||prefix.size<1||prefix.size>static_cast<int>(packet.data.size())){
                ++foreign;
                api.release_message(message);
                continue;
            }
            packet.size=static_cast<unsigned>(prefix.size);
            std::memcpy(packet.data.data(),prefix.data,packet.size);
            api.release_message(message);
            const auto peer=static_cast<unsigned>(slot);
            const auto bit=1u<<peer;
            SR::Control control{};
            if(SR::decode_control(packet,status.lobby,status.nonce,control)){
                link.last_seen[peer]=time;
                link.handshake.observe(peer,control);
                if(control==SR::Control::hello){
                    const auto response=SR::control(status.lobby,status.nonce,SR::Control::ack);
                    steam_send(api,link.identities[peer],response,steam_control_flags);
                }
                continue;
            }
            if(!SR::match_datagram(packet,status.lobby,status.nonce)){++foreign;continue;}
            if(!active||!(link.handshake.connected()&bit))continue;
            link.last_seen[peer]=time;
            if(!incoming.push({packet,peer}))++worker_drops;
            else ++rx;
        }
    }
}

struct SteamLinkEnd { SR::Failure failure{}; bool rekey{}; };

static SteamLinkEnd steam_run_link(const SteamApi& api,SR::Status& status,SteamId self,
    const SR::Roster& roster,const SR::Epoch& epoch,SteamLink& link){
    if(!SR::map_peers(status.lobby,status.nonce,self,roster,link.peers))return {SR::Failure::membership};
    steam_local_account.store(self,std::memory_order_release);
    for(unsigned i=0;i<link.peers.users.size();++i)
        steam_peer_accounts[i].store(link.peers.users[i],std::memory_order_release);
    for(unsigned i=0;i<link.peers.users.size();++i)
        if(link.peers.users[i])api.set_identity(&link.identities[i],link.peers.users[i]);
    status.peer_mask=link.peers.mask;
    steam_phase(status,SR::Phase::handshake);
    const auto deadline=GetTickCount64()+20000;
    ULONGLONG next_hello{},next_roster{},invalid_since{};
    bool active=false;
    const auto hello=SR::control(status.lobby,status.nonce,SR::Control::hello);
    while(!steam_stopping()){
        const auto time=GetTickCount64();
        if(time>=next_roster){
            SR::Roster current{};
            if(!api.logged_on(api.user))return {SR::Failure::unavailable};
            if(!steam_roster(api,status.lobby,current)){
                if(!invalid_since)invalid_since=time;
                if(time-invalid_since>=1000)return {SR::Failure::membership};
                api.release_thread();
                WaitForSingleObject(stop_event,25);
                continue;
            }
            invalid_since=0;
            if(current.owner!=roster.owner||!steam_member(current,self))return {SR::Failure::membership};
            if(current!=roster)return {SR::Failure::membership,true};
            if(!host_role){
                SR::Epoch offered{};
                if(!steam_read_epoch(api,status.lobby,roster.owner,offered))return {SR::Failure::metadata};
                const auto change=SR::epoch_change(epoch,offered);
                if(change==SR::EpochChange::next)return {SR::Failure::membership,true};
                if(change==SR::EpochChange::invalid||
                   (change==SR::EpochChange::same&&offered.locked!=roster))return {SR::Failure::metadata};
            }
            next_roster=time+250;
        }
        if(time>=next_hello){
            for(unsigned i=0;i<link.peers.users.size();++i)
                if(link.peers.users[i])steam_send(api,link.identities[i],hello,steam_control_flags);
            next_hello=time+(active?2000:250);
        }
        steam_receive(api,status,link,active,time);
        if(!active){
            const auto connected=link.handshake.connected();
            const auto count=static_cast<unsigned>(std::popcount(connected));
            if(count!=status.connected){status.connected=count;publish_steam_status(status);}
            if(connected==link.peers.mask){
                active=true;
                steam_phase(status,SR::Phase::active);
                build_state=1;
            }else if(time>=deadline)return {SR::Failure::handshake,true};
        }
        if(active){
            for(unsigned i=0;i<link.peers.users.size();++i)
                if(link.peers.users[i]&&time-link.last_seen[i]>15000)return {SR::Failure::handshake,true};
            TargetedPacket target{};
            for(unsigned i=0;i<256&&outgoing.pop(target);++i){
                if(!SR::match_datagram(target.packet,status.lobby,status.nonce))continue;
                const auto kind=target.packet.size>=40?qdcoop::enemy::animation::wire::get<std::uint32_t>(target.packet,32):0;
                const auto flags=kind==qdcoop::checkpoint::wire_kind||kind==qdcoop::world_progress::snapshot_kind||
                    kind==qdcoop::world_progress::ack_kind||kind==qdcoop::enemy::damage::hit_kind||
                    kind==qdcoop::enemy::damage::ack_kind?steam_control_flags:steam_data_flags;
                for(unsigned peer=0;peer<link.peers.users.size();++peer){
                    if(!(target.peers&(1u<<peer))||!link.peers.users[peer])continue;
                    if(steam_send(api,link.identities[peer],target.packet,flags)==1)++tx;
                    else ++socket_errors;
                }
            }
        }
        api.release_thread();
        HANDLE events[]{stop_event,wake_event};
        WaitForMultipleObjects(2,events,FALSE,10);
    }
    return {};
}

static SR::Failure steam_quiesce(const SteamApi& api,SR::Status& status,SteamId owner){
    if(host_role&&!api.joinable(api.matchmaking,status.lobby,false))return SR::Failure::metadata;
    steam_recovery.request(status.epoch);
    status.connected=status.peer_mask=0;
    steam_phase(status,SR::Phase::recovering);
    const auto deadline=GetTickCount64()+10000;
    while(!steam_stopping()){
        const auto state=steam_recovery.state(status.epoch);
        if(state==SR::Recovery::clean)return SR::Failure::none;
        if(state==SR::Recovery::blocked)return SR::Failure::durable;
        if(!api.logged_on(api.user)||api.owner(api.matchmaking,status.lobby)!=owner)
            return SR::Failure::membership;
        if(GetTickCount64()>=deadline)return SR::Failure::recovery;
        api.release_thread();
        WaitForSingleObject(stop_event,25);
    }
    return SR::Failure::none;
}

static SR::Failure steam_session(const SteamApi& api,SR::Status& status,SteamId self){
    SR::Epoch epoch{};
    auto owner=host_role?self:0;
    if(host_role&&(!steam_publish_room(api,status,self)||!steam_open_epoch(api,status,self,epoch)))
        return SR::Failure::metadata;
    std::uint32_t minimum=1;
    SR::Roster timeout_roster{};
    while(!steam_stopping()){
        SR::Roster roster{};
        const auto waiting=steam_wait_roster(api,status,self,owner,epoch,minimum,roster);
        if(waiting!=SR::Failure::none||steam_stopping())return waiting;
        SteamLink link{};
        const auto end=steam_run_link(api,status,self,roster,epoch,link);
        build_state=0;
        for(unsigned i=0;i<link.peers.users.size();++i)
            if(link.peers.users[i])api.close_channel(api.messages,&link.identities[i],SR::channel);
        if(!end.rekey||steam_stopping())return end.failure;
        if(end.failure==SR::Failure::handshake){
            if(timeout_roster==roster)return end.failure;
            timeout_roster=roster;
        }else timeout_roster={};
        if(epoch.serial==UINT32_MAX)return SR::Failure::recovery;
        const auto recovery=steam_quiesce(api,status,owner);
        if(recovery!=SR::Failure::none||steam_stopping())return recovery;
        minimum=epoch.serial+1;
        if(host_role&&!steam_open_epoch(api,status,self,epoch))return SR::Failure::metadata;
    }
    return SR::Failure::none;
}

static DWORD WINAPI steam_network(void*){
    SR::Status status{};
    status.expected=steam_request.expected;
    steam_phase(status,SR::Phase::connecting);
    SteamApi api{};
    SteamId self{};
    HMODULE module{};
    SR::Failure failure=SR::Failure::none;
    if(!verified_game())failure=SR::Failure::game_build;
    else if(!GetModuleHandleExW(0,L"steam_api64.dll",&module)||!api.bind(module,status))
        failure=SR::Failure::unavailable;
    else if(api.app(api.utils)!=SR::app_id||!api.logged_on(api.user)||!api.subscribed(api.apps)||
            !SR::user_id(self=api.self(api.user)))failure=SR::Failure::app;
    else if(!steam_stopping()){
        api.relay(api.networking);
        failure=steam_enter(api,status);
        if(failure==SR::Failure::none&&!steam_stopping())failure=steam_session(api,status,self);
    }
    if(failure!=SR::Failure::none&&!steam_stopping())build_state=-1;
    if(status.lobby){
        if(host_role&&api.owner(api.matchmaking,status.lobby)==self)api.joinable(api.matchmaking,status.lobby,false);
        api.leave(api.matchmaking,status.lobby);
        status.left=1;
    }
    if(api.release_thread)api.release_thread();
    if(module)FreeLibrary(module);
    status.failure=static_cast<unsigned>(failure);
    steam_phase(status,steam_stopping()||failure==SR::Failure::none?SR::Phase::closed:SR::Phase::failed);
    stopped=true;
    return 0;
}

static bool transport_ready(){
    if(build_state.load()!=1)return false;
    if(steam_transport&&!steam_committed){
        const auto status=read_steam_status();
        if(status.phase!=static_cast<unsigned>(SR::Phase::active))return false;
        envelope.room_high=status.lobby;
        envelope.room_low=status.nonce;
        active_peer_mask=status.peer_mask;
        steam_committed=true;
    }
    return true;
}
