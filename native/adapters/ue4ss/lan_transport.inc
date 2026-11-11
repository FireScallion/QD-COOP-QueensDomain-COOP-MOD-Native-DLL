namespace Lan=qdcoop::lan_room;

struct LanRequest {std::uint32_t address{};unsigned port{},seats{};};
struct LanPeer {
    sockaddr_in address{};
    Lan::Control scope;
    ULONGLONG last_seen{};
    bool ready{},departed{};
};
static LanRequest lan_request;
static SRWLOCK lan_status_lock=SRWLOCK_INIT;
static Lan::Status lan_status;
static Lan::LocalRoster lan_local_roster;
static bool lan_transport,lan_committed;
static SR::RecoveryGate lan_recovery;

static Lan::Status read_lan_status(){
    AcquireSRWLockShared(&lan_status_lock);
    const auto status=lan_status;
    ReleaseSRWLockShared(&lan_status_lock);
    return status;
}
static void publish_lan_status(const Lan::Status& status){
    AcquireSRWLockExclusive(&lan_status_lock);
    lan_status=status;
    ReleaseSRWLockExclusive(&lan_status_lock);
}
static void publish_lan_roster(const Lan::LocalRoster& roster){
    AcquireSRWLockExclusive(&lan_status_lock);
    lan_local_roster=roster;
    ReleaseSRWLockExclusive(&lan_status_lock);
}

static bool lan_random(Lan::Token& token){
    return BCryptGenRandom(nullptr,reinterpret_cast<unsigned char*>(&token),sizeof(token),BCRYPT_USE_SYSTEM_PREFERRED_RNG)==0&&
        static_cast<bool>(token);
}
static bool lan_same(const sockaddr_in& first,const sockaddr_in& second){
    return first.sin_family==AF_INET&&second.sin_family==AF_INET&&first.sin_addr.s_addr==second.sin_addr.s_addr&&
        first.sin_port==second.sin_port;
}
static bool lan_cookie(BCRYPT_ALG_HANDLE algorithm,const Lan::Token& secret,const sockaddr_in& address,
                       const Lan::Control& hello,Lan::Token& result){
    std::array<unsigned char,26> data{};
    std::memcpy(data.data(),&hello.client,16);
    std::memcpy(data.data()+16,&address.sin_addr.s_addr,4);
    std::memcpy(data.data()+20,&hello.epoch,4);
    std::memcpy(data.data()+24,&address.sin_port,2);
    auto key=secret;
    BCRYPT_HASH_HANDLE hash{};
    std::array<unsigned char,32> output{};
    if(BCryptCreateHash(algorithm,&hash,nullptr,0,reinterpret_cast<unsigned char*>(&key),sizeof(key),0)!=0)return false;
    const bool ok=BCryptHashData(hash,data.data(),static_cast<ULONG>(data.size()),0)==0&&
        BCryptFinishHash(hash,output.data(),static_cast<ULONG>(output.size()),0)==0;
    BCryptDestroyHash(hash);
    if(!ok)return false;
    std::memcpy(&result,output.data(),sizeof(result));
    return static_cast<bool>(result);
}
static void lan_datagram(SOCKET socket,const sockaddr_in& address,const E::Packet& packet,bool gameplay){
    const auto sent=sendto(socket,reinterpret_cast<const char*>(packet.data.data()),static_cast<int>(packet.size),0,
        reinterpret_cast<const sockaddr*>(&address),sizeof(address));
    if(sent==static_cast<int>(packet.size)){if(gameplay)++tx;return;}
    if(sent==SOCKET_ERROR){
        const auto error=WSAGetLastError();
        if(error==WSAECONNRESET){++udp_unreachable;return;}
        if(error==WSAEWOULDBLOCK)return;
    }
    ++socket_errors;
}
static void lan_control(SOCKET socket,const sockaddr_in& address,Lan::Control control,Lan::Op operation){
    control.op=operation;
    E::Packet packet{};
    if(Lan::encode(control,packet))lan_datagram(socket,address,packet,false);
}
static void lan_receive_game(E::Packet& packet,unsigned peer){
    if(!incoming.push({packet,peer}))++worker_drops;
    else ++rx;
}

struct LanHost {
    std::array<LanPeer,3> peers{};
    std::array<LanPeer,3> previous{};
    Lan::Status status;
    Lan::Token secret;
    BCRYPT_ALG_HANDLE cookie{};
    ULONGLONG next_ping{};
};
static void lan_publish_host_roster(const LanHost& host){
    Lan::LocalRoster roster{};
    const auto id=[](const LanPeer& peer) -> std::uint64_t {
        return peer.scope.client&&!peer.departed?(std::uint64_t{peer.address.sin_addr.s_addr}<<16)|peer.address.sin_port:0;
    };
    std::ranges::transform(host.peers,roster.begin(),id);
    if(host.status.phase==Lan::Phase::waiting)
        for(const auto& peer:host.previous){
            const auto previous=id(peer);
            if(!previous||std::ranges::find(roster,previous)!=roster.end())continue;
            if(const auto empty=std::ranges::find(roster,std::uint64_t{});empty!=roster.end())*empty=previous;
        }
    publish_lan_roster(roster);
}
static bool lan_scope(const Lan::Control& control,const Lan::Control& scope){
    return control.epoch==scope.epoch&&control.seats==scope.seats&&control.slot==scope.slot&&control.client==scope.client&&
        control.proof==scope.proof&&control.room==scope.room;
}
static void lan_host_control(SOCKET socket,LanHost& host,const sockaddr_in& from,const Lan::Control& input,ULONGLONG time){
    const auto limit=host.status.seats-1;
    const auto last=host.peers.begin()+limit;
    auto peer=std::find_if(host.peers.begin(),last,[&](const auto& entry){return lan_same(entry.address,from);});
    if(input.op==Lan::Op::hello||input.op==Lan::Op::proof){
        Lan::Control reply{Lan::Op::challenge,host.status.epoch,Lan::unassigned,host.status.seats,input.client,{},{}};
        if(!lan_cookie(host.cookie,host.secret,from,reply,reply.proof))return;
        if(input.op==Lan::Op::hello){lan_control(socket,from,reply,Lan::Op::challenge);return;}
        if(input.epoch!=reply.epoch||input.seats!=reply.seats||input.proof!=reply.proof)return;
        if(peer==last&&host.status.phase==Lan::Phase::waiting)
            peer=std::find_if(host.peers.begin(),last,[](const auto& entry){return !entry.scope.client;});
        if(peer==last||(peer->scope.client&&peer->scope.client!=input.client)){
            lan_control(socket,from,reply,Lan::Op::refused);
            return;
        }
        if(!peer->scope.client){
            reply.slot=static_cast<unsigned>(peer-host.peers.begin());
            reply.room=host.status.room;
            *peer={from,reply,time,false};
            lan_publish_host_roster(host);
        }
        peer->last_seen=time;
        lan_control(socket,from,peer->scope,host.status.phase==Lan::Phase::active?Lan::Op::active:Lan::Op::welcome);
        return;
    }
    if(peer==last||!lan_scope(input,peer->scope)){
        if(input.op==Lan::Op::ping||input.op==Lan::Op::ready){
            const auto old=std::ranges::find_if(host.previous,[&](const auto& entry){
                return entry.scope.client&&lan_same(entry.address,from)&&lan_scope(input,entry.scope);
            });
            if(old!=host.previous.end())lan_control(socket,from,old->scope,Lan::Op::restart);
        }
        return;
    }
    if(input.op==Lan::Op::ready||input.op==Lan::Op::ping){
        if(input.op==Lan::Op::ready)peer->ready=true;
        peer->last_seen=time;
    }else if(input.op==Lan::Op::bye){
        if(host.status.phase==Lan::Phase::active){
            peer->departed=true;
            host.status.failure=Lan::Failure::departure;
        }
        else *peer={};
        lan_publish_host_roster(host);
    }
}
static void lan_host_tick(SOCKET socket,LanHost& host,ULONGLONG time){
    const auto limit=host.status.seats-1;
    unsigned mask{};
    for(unsigned i=0;i<limit;++i){
        auto& peer=host.peers[i];
        if(!peer.scope.client)continue;
        if(time-peer.last_seen>=15000){
            if(host.status.phase==Lan::Phase::active){
                peer.departed=true;
                host.status.failure=Lan::Failure::departure;
            }
            else peer={};
            continue;
        }
        if(peer.ready)mask|=1u<<i;
    }
    if(host.status.phase==Lan::Phase::waiting){
        host.status.connected=std::popcount(mask);
        if(mask==((1u<<limit)-1)){
            host.status.phase=Lan::Phase::active;
            host.status.peer_mask=mask;
            publish_lan_status(host.status);
            build_state=1;
            host.next_ping=0;
        }
    }
    if(time<host.next_ping)return;
    host.next_ping=time+500;
    publish_lan_status(host.status);
    lan_publish_host_roster(host);
    for(const auto& peer:host.peers)if(peer.scope.client)
        lan_control(socket,peer.address,peer.scope,host.status.phase==Lan::Phase::active?Lan::Op::active:Lan::Op::welcome);
}

struct LanGuest {
    sockaddr_in host{};
    Lan::Status status;
    Lan::Control scope;
    ULONGLONG deadline{},last_seen{},next_ping{};
    std::uint32_t minimum_epoch{1};
    bool rekey{};
};
static void lan_guest_control(LanGuest& guest,const Lan::Control& input,ULONGLONG time){
    if(input.client!=guest.scope.client)return;
    if(input.op==Lan::Op::challenge&&!guest.scope.room&&input.epoch>=guest.minimum_epoch){
        guest.scope=input;
        guest.last_seen=time;
        guest.next_ping=0;
        return;
    }
    if(!guest.scope.proof||input.proof!=guest.scope.proof||input.epoch!=guest.scope.epoch||input.seats!=guest.scope.seats)return;
    if(input.op==Lan::Op::refused&&!guest.scope.room){guest.status.failure=Lan::Failure::full;return;}
    if(input.op==Lan::Op::welcome||input.op==Lan::Op::active){
        if(guest.scope.room&&!lan_scope(input,guest.scope))return;
        guest.scope=input;
        guest.last_seen=time;
        guest.status.epoch=input.epoch;
        guest.status.slot=input.slot;
        guest.status.seats=input.seats;
        guest.status.room=input.room;
        if(input.op==Lan::Op::active&&guest.status.phase!=Lan::Phase::active){
            guest.status.phase=Lan::Phase::active;
            guest.status.connected=input.seats-1;
            guest.status.peer_mask=1;
            publish_lan_status(guest.status);
            build_state=1;
            guest.next_ping=0;
        }
    }else if(lan_scope(input,guest.scope)){
        if(input.op==Lan::Op::bye)guest.status.failure=Lan::Failure::departure;
        else if(input.op==Lan::Op::restart&&guest.scope.room)guest.rekey=true;
    }
}
static void lan_guest_tick(SOCKET socket,LanGuest& guest,ULONGLONG time){
    if((!guest.scope.room&&time>=guest.deadline)||(guest.scope.room&&time-guest.last_seen>=15000)){
        guest.status.failure=guest.status.phase==Lan::Phase::active?Lan::Failure::departure:Lan::Failure::timeout;
        return;
    }
    if(time<guest.next_ping)return;
    guest.next_ping=time+500;
    const auto op=guest.status.phase==Lan::Phase::active?Lan::Op::ping:guest.scope.room?Lan::Op::ready:
        guest.scope.proof?Lan::Op::proof:Lan::Op::hello;
    lan_control(socket,guest.host,guest.scope,op);
    if(guest.status.phase==Lan::Phase::waiting)publish_lan_status(guest.status);
}

static Lan::Failure lan_quiesce(SOCKET socket,Lan::Status& status,const std::array<LanPeer,3>* departing){
    if(status.epoch==UINT32_MAX)return Lan::Failure::recovery;
    build_state=0;
    lan_recovery.request(status.epoch);
    status.phase=Lan::Phase::recovering;
    status.connected=status.peer_mask=0;
    status.failure=Lan::Failure::none;
    publish_lan_status(status);
    const auto deadline=GetTickCount64()+10000;
    ULONGLONG next{};
    while(WaitForSingleObject(stop_event,0)!=WAIT_OBJECT_0){
        const auto time=GetTickCount64();
        if(departing&&time>=next){
            next=time+250;
            for(const auto& peer:*departing)if(peer.scope.client)lan_control(socket,peer.address,peer.scope,Lan::Op::restart);
        }
        switch(lan_recovery.state(status.epoch)){
        case SR::Recovery::clean:return Lan::Failure::none;
        case SR::Recovery::blocked:return Lan::Failure::durable;
        default:break;
        }
        if(time>=deadline)return Lan::Failure::recovery;
        WaitForSingleObject(stop_event,25);
    }
    return Lan::Failure::none;
}

static Lan::Failure lan_host_rekey(SOCKET socket,LanHost& host){
    const auto failure=lan_quiesce(socket,host.status,&host.peers);
    if(failure!=Lan::Failure::none||WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0)return failure;
    Lan::Token secret,room;
    if(!lan_random(secret)||!lan_random(room))return Lan::Failure::random;
    host.secret=secret;
    host.previous=host.peers;
    host.peers={};
    host.status.room=room;
    ++host.status.epoch;
    host.status.phase=Lan::Phase::waiting;
    host.next_ping=0;
    publish_lan_status(host.status);
    return Lan::Failure::none;
}

static Lan::Failure lan_guest_rekey(SOCKET socket,LanGuest& guest){
    const auto failure=lan_quiesce(socket,guest.status,nullptr);
    if(failure!=Lan::Failure::none||WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0)return failure;
    const auto endpoint=guest.host;
    const auto minimum=guest.status.epoch+1;
    guest={};
    guest.host=endpoint;
    guest.minimum_epoch=minimum;
    guest.status.phase=Lan::Phase::waiting;
    guest.deadline=GetTickCount64()+30000;
    if(!lan_random(guest.scope.client))return Lan::Failure::random;
    publish_lan_status(guest.status);
    return Lan::Failure::none;
}

static Lan::Failure lan_run(SOCKET socket,WSAEVENT event){
    LanHost host;
    LanGuest guest;
    auto& status=host_role?host.status:guest.status;
    status.phase=Lan::Phase::waiting;
    if(host_role){
        status.seats=lan_request.seats;
        status.epoch=1;
        if(!lan_random(status.room)||!lan_random(host.secret)||
           BCryptOpenAlgorithmProvider(&host.cookie,BCRYPT_SHA256_ALGORITHM,nullptr,BCRYPT_ALG_HANDLE_HMAC_FLAG)!=0)
            return Lan::Failure::random;
    }else{
        guest.host.sin_family=AF_INET;
        guest.host.sin_port=htons(static_cast<unsigned short>(lan_request.port));
        guest.host.sin_addr.s_addr=lan_request.address;
        if(!lan_random(guest.scope.client))return Lan::Failure::random;
        guest.deadline=GetTickCount64()+30000;
    }
    publish_lan_status(status);
    HANDLE events[]{stop_event,wake_event,event};
    while(WaitForSingleObject(stop_event,0)!=WAIT_OBJECT_0){
        if(host_role&&status.phase==Lan::Phase::active&&status.failure==Lan::Failure::departure){
            lan_publish_host_roster(host);
            status.failure=lan_host_rekey(socket,host);
        }
        else if(!host_role&&guest.rekey)status.failure=lan_guest_rekey(socket,guest);
        if(status.failure!=Lan::Failure::none||WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0)break;
        const auto time=GetTickCount64();
        if(host_role)lan_host_tick(socket,host,time);
        else lan_guest_tick(socket,guest,time);
        if(status.phase==Lan::Phase::active&&status.failure==Lan::Failure::none){
            TargetedPacket target{};
            for(unsigned n=0;n<256&&outgoing.pop(target);++n){
                if(!SR::match_datagram(target.packet,status.room.high,status.room.low))continue;
                if(host_role){
                    for(unsigned i=0;i<host.peers.size();++i)if(target.peers&(1u<<i)&status.peer_mask)
                        lan_datagram(socket,host.peers[i].address,target.packet,true);
                }else if(target.peers&1u)lan_datagram(socket,guest.host,target.packet,true);
            }
        }
        WaitForMultipleObjects(3,events,FALSE,10);
        WSANETWORKEVENTS occurred{};
        WSAEnumNetworkEvents(socket,event,&occurred);
        E::Packet packet{};
        for(unsigned n=0;n<256&&status.failure==Lan::Failure::none&&!guest.rekey;++n){
            sockaddr_in from{};
            int length=sizeof(from);
            const auto size=recvfrom(socket,reinterpret_cast<char*>(packet.data.data()),E::packet_capacity,0,
                reinterpret_cast<sockaddr*>(&from),&length);
            if(size==SOCKET_ERROR){
                const auto error=WSAGetLastError();
                if(error==WSAECONNRESET){++udp_unreachable;continue;}
                if(error!=WSAEWOULDBLOCK)++socket_errors;
                break;
            }
            if(length!=sizeof(from)||from.sin_family!=AF_INET||(!host_role&&!lan_same(from,guest.host))){++foreign;continue;}
            packet.size=static_cast<unsigned>(size);
            const auto received=GetTickCount64();
            Lan::Control control;
            if(Lan::decode(packet,control)){
                if(host_role)lan_host_control(socket,host,from,control,received);
                else lan_guest_control(guest,control,received);
                continue;
            }
            if(status.phase!=Lan::Phase::active||!SR::match_datagram(packet,status.room.high,status.room.low)){++foreign;continue;}
            if(host_role){
                const auto peer=std::ranges::find_if(host.peers,[&](const auto& item){return item.ready&&lan_same(item.address,from);});
                if(peer==host.peers.end()){++foreign;continue;}
                peer->last_seen=received;
                lan_receive_game(packet,static_cast<unsigned>(peer-host.peers.begin()));
            }else{
                guest.last_seen=received;
                lan_receive_game(packet,0);
            }
        }
    }
    if(host_role){
        for(const auto& peer:host.peers)if(peer.scope.client)lan_control(socket,peer.address,peer.scope,Lan::Op::bye);
        BCryptCloseAlgorithmProvider(host.cookie,0);
    }else if(guest.scope.room)lan_control(socket,guest.host,guest.scope,Lan::Op::bye);
    return status.failure;
}

static DWORD WINAPI lan_network(void*){
    Lan::Failure failure=Lan::Failure::game_build;
    if(verified_game()){
        failure=Lan::Failure::socket;
        WSADATA data{};
        if(WSAStartup(MAKEWORD(2,2),&data)==0){
            const auto socket=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
            const auto event=WSACreateEvent();
            sockaddr_in local{};
            local.sin_family=AF_INET;
            local.sin_addr.s_addr=htonl(INADDR_ANY);
            local.sin_port=host_role?htons(static_cast<unsigned short>(lan_request.port)):0;
            BOOL exclusive=TRUE;
            if(socket!=INVALID_SOCKET&&event!=WSA_INVALID_EVENT&&
               setsockopt(socket,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&exclusive),sizeof(exclusive))==0&&
               ::bind(socket,reinterpret_cast<sockaddr*>(&local),sizeof(local))==0&&WSAEventSelect(socket,event,FD_READ)==0)
                failure=lan_run(socket,event);
            if(socket!=INVALID_SOCKET)closesocket(socket);
            if(event!=WSA_INVALID_EVENT)WSACloseEvent(event);
            WSACleanup();
        }
    }
    auto status=read_lan_status();
    status.failure=failure;
    status.phase=failure==Lan::Failure::none?Lan::Phase::closed:Lan::Phase::failed;
    status.peer_mask=0;
    publish_lan_status(status);
    if(failure!=Lan::Failure::none)build_state=-1;
    stopped=true;
    return 0;
}

static bool lan_transport_ready(){
    if(!lan_transport||lan_committed)return true;
    const auto status=read_lan_status();
    if(status.phase!=Lan::Phase::active)return false;
    envelope.room_high=status.room.high;
    envelope.room_low=status.room.low;
    active_peer_mask=status.peer_mask;
    lan_committed=true;
    return true;
}
