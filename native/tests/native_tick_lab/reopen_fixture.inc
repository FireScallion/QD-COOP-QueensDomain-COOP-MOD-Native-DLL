// The second room reuses the processes and ports and promotes the first guest.
enum class ReopenStage {first,stop_first,waiting,second,stop_second,done};
static ReopenStage reopen_stage;
static constexpr std::uint64_t reopen_nonce=lab_nonce+1;
static constexpr float reopen_hp=103.5f;
static constexpr unsigned reopen_host_index=1;
static bool reopen_live_rejected,reopen_old_rejected,reopen_first_clean,reopen_second_clean;
static bool reopen_replay_sent,reopen_ready;
static std::uint64_t reopen_first_world,reopen_foreign;
static AutomaticSample reopen_sample;
static std::wstring marker(unsigned role,const wchar_t* file);
static bool exists(const std::wstring& path);

static void reopen_mark(const wchar_t* name){
    const auto path=marker(role_index,name);
    write_file(path.c_str(),"ready",5);
}
static bool reopen_all(const wchar_t* name,bool skip_host=false){
    for(unsigned i=0;i<=client_count;++i){
        if(skip_host&&i==reopen_host_index)continue;
        if(!exists(marker(i,name)))return false;
    }
    return true;
}
static bool start_reopened_room(std::uint64_t nonce){
    const bool authority=role_index==reopen_host_index;
    std::array<unsigned,3> peers{};
    if(authority){
        unsigned next=0;
        for(unsigned i=0;i<=client_count;++i)if(i!=role_index)peers[next++]=first_port+i;
    }else peers[0]=first_port+reopen_host_index;
    return core.start(authority,first_port+role_index,lab_room,nonce,peers[0],peers[1],peers[2])!=0;
}

static bool replay_first_room_offer(){
    E::Packet packet{};
    if(!R::encode({lab_room,lab_nonce,reopen_first_world,1},R::Offer{1,2000,1,1,{}},false,packet))
        return false;
    WSADATA data{};
    if(WSAStartup(MAKEWORD(2,2),&data))return false;
    const auto sender=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    sockaddr_in address{};
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    address.sin_port=htons(first_port+reopen_host_index);
    bool sent=sender!=INVALID_SOCKET&&bind(sender,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0;
    for(unsigned i=0;i<=client_count&&sent;++i){
        if(i==reopen_host_index)continue;
        address.sin_port=htons(static_cast<unsigned short>(first_port+i));
        sent=sendto(sender,reinterpret_cast<const char*>(packet.data.data()),static_cast<int>(packet.size),0,
                    reinterpret_cast<sockaddr*>(&address),sizeof(address))==static_cast<int>(packet.size);
    }
    if(sender!=INVALID_SOCKET)closesocket(sender);
    WSACleanup();
    return sent;
}

static void reopen_tick(void* engine,float dt){
    const bool authority=role_index==reopen_host_index;
    if(reopen_stage==ReopenStage::stop_first){
        if(!core.stop())return;
        started=false;
        E::NativeBindingView stale{};
        reopen_first_clean=first_room_clean()&&!core.inspect_binding(1,&stale);
        reopen_old_rejected=!start_reopened_room(lab_nonce);
        const auto status=read_native_status(core.status);
        reopen_first_world=counter(status.c_str(),"\"world_epoch\":\"");
        if(!reopen_first_clean||!reopen_old_rejected||reopen_first_world<=1){phase=76;return;}
        reopen_mark(L"first-closed.txt");
        reopen_stage=ReopenStage::waiting;
    }
    if(reopen_stage==ReopenStage::waiting){
        static ULONGLONG next_check{};
        const auto time=GetTickCount64();
        if(time<next_check)return;
        next_check=time+100;
        if(!reopen_all(L"first-closed.txt"))return;
        if(authority){
            if(!reopen_all(L"second-ready.txt",true))return;
            reopen_replay_sent=replay_first_room_offer();
            if(!reopen_replay_sent){phase=77;return;}
        }
        started=start_reopened_room(reopen_nonce);
        if(!started){phase=78;return;}
        reopen_stage=ReopenStage::second;
    }
    if(reopen_stage==ReopenStage::second){
        const auto world=core.pump_world(engine,dt,1);
        if(world==WorldPhase::failed||world==WorldPhase::world_changed){phase=79;return;}
        if(world!=WorldPhase::active)return;
        if(!reopen_ready){
            reopen_mark(L"second-ready.txt");
            reopen_ready=true;
        }
        if(!reopen_sample.binding.generation){
            if(!inspect_automatic(1,1,reopen_sample,authority,reopen_hp))return;
            reopen_mark(L"second-applied.txt");
        }
        // The restarted transport must remain live until every peer reads back
        // the new room. File checks are throttled outside the timing samples.
        static ULONGLONG next_check{};
        const auto time=GetTickCount64();
        if(time<next_check)return;
        next_check=time+100;
        if(!reopen_all(L"second-applied.txt"))return;
        reopen_stage=ReopenStage::stop_second;
    }
    if(reopen_stage==ReopenStage::stop_second){
        if(!core.stop())return;
        started=false;
        E::NativeBindingView stale{};
        const auto status=read_native_status(core.status);
        reopen_foreign=counter(status.c_str(),"\"foreign\":");
        reopen_second_clean=sample_clean(reopen_sample,authority)&&!core.inspect_binding(1,&stale);
        if(!reopen_second_clean||reopen_foreign!=(authority?0u:1u)){phase=80;return;}
        reopen_mark(L"second-closed.txt");
        reopen_stage=ReopenStage::done;
    }
    if(reopen_stage==ReopenStage::done){
        static ULONGLONG next_check{};
        const auto time=GetTickCount64();
        if(time<next_check)return;
        next_check=time+100;
        if(reopen_all(L"second-closed.txt")){
            passed=true;
            phase=3;
        }
    }
}

static void append_reopen_report(std::string& json){
    auto flag=[&](const char* name,bool value){
        json+=",\"";
        json+=name;
        json+=value?"\":true":"\":false";
    };
    flag("reopen_authority",role_index==reopen_host_index);
    flag("live_start_rejected",reopen_live_rejected);
    flag("old_start_rejected",reopen_old_rejected);
    flag("first_epoch_clean",reopen_first_clean);
    flag("second_epoch_clean",reopen_second_clean);
    flag("old_offer_sent",reopen_replay_sent);
    json+=",\"old_scope_dropped\":"+std::to_string(reopen_foreign);
    json+=",\"reopen_generation\":"+std::to_string(reopen_sample.binding.generation);
    json+=",\"reopen_hp\":"+std::to_string(reopen_sample.hp);
}
