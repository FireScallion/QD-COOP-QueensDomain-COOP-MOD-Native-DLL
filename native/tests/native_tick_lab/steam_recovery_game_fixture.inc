// Real isolated engine, with a deterministic Steam service in the lab core.
enum class RecoveryStep {first,drain,waiting,second,stop};
static RecoveryStep recovery_step;
static SR::Status recovery_first{},recovery_waiting{},recovery_second{};
static E::NativeBindingView recovery_original{},recovery_new{};
static bool recovery_detached,recovery_sources_alive,recovery_final_clean;

static bool live_source(const E::NativeBindingView& binding){
    return binding.generation==1&&resolve(binding.actor)&&!fixture_actor_gone(binding.actor);
}

static void recovery_tick(void* engine,float dt){
    SR::Status room{};
    if(!core.steam_room(&room,sizeof(room))||room.phase==static_cast<unsigned>(SR::Phase::failed)){
        phase=82;
        return;
    }
    switch(recovery_step){
    case RecoveryStep::first:
        if(room.phase!=static_cast<unsigned>(SR::Phase::active))return;
        if(core.pump_world(engine,dt,1)!=WorldPhase::active)return;
        if(!core.inspect_binding(1,&recovery_original)||!recovery_original.confirmed)return;
        recovery_first=room;
        if(room.epoch!=1||room.members!=4||!live_source(recovery_original)||!core.change_recovery_roster(0)){
            phase=83;
            return;
        }
        recovery_step=RecoveryStep::drain;
        return;
    case RecoveryStep::drain: {
        if(room.phase!=static_cast<unsigned>(SR::Phase::recovering))return;
        const auto cleaned=core.recover();
        if(!cleaned)return;
        E::NativeBindingView stale{};
        recovery_detached=cleaned==1&&!core.inspect_binding(1,&stale)&&live_source(recovery_original);
        if(!recovery_detached){phase=84;return;}
        recovery_step=RecoveryStep::waiting;
        return;
    }
    case RecoveryStep::waiting:
        if(room.phase!=static_cast<unsigned>(SR::Phase::waiting))return;
        recovery_waiting=room;
        if(room.epoch!=2||room.nonce==recovery_first.nonce||room.lobby!=recovery_first.lobby||
           room.members!=3||room.peer_mask||room.left||!core.change_recovery_roster(1)){
            phase=85;
            return;
        }
        recovery_step=RecoveryStep::second;
        return;
    case RecoveryStep::second:
        if(room.phase!=static_cast<unsigned>(SR::Phase::active))return;
        if(core.pump_world(engine,dt,1)!=WorldPhase::active)return;
        if(!core.inspect_binding(1,&recovery_new)||!recovery_new.confirmed)return;
        recovery_second=room;
        recovery_sources_alive=live_source(recovery_original)&&live_source(recovery_new);
        if(room.epoch!=recovery_waiting.epoch||room.nonce!=recovery_waiting.nonce||
           room.lobby!=recovery_first.lobby||room.members!=4||room.peer_mask!=7||!recovery_sources_alive){
            phase=86;
            return;
        }
        recovery_step=RecoveryStep::stop;
        return;
    case RecoveryStep::stop: {
        if(!core.stop())return;
        started=false;
        E::NativeBindingView stale{};
        recovery_final_clean=!core.inspect_binding(1,&stale)&&
            live_source(recovery_original)&&live_source(recovery_new);
        passed=recovery_final_clean;
        phase=passed?3:87;
        return;
    }
    }
}

static void write_recovery_report(){
    char text[512]{};
    const auto count=std::snprintf(text,sizeof(text),
        "{\"detached\":%s,\"sources_alive\":%s,\"final_clean\":%s,\"same_lobby\":%s,"
        "\"first_epoch\":%u,\"second_epoch\":%u,\"vacant_members\":%u,\"new_generation\":%u}\n",
        recovery_detached?"true":"false",recovery_sources_alive?"true":"false",
        recovery_final_clean?"true":"false",
        recovery_first.lobby&&recovery_first.lobby==recovery_second.lobby?"true":"false",
        recovery_first.epoch,recovery_second.epoch,recovery_waiting.members,recovery_new.generation);
    const auto path=std::wstring(run_root)+L"/host/native-recovery.json";
    if(count>0&&static_cast<unsigned>(count)<sizeof(text))write_file(path.c_str(),text,static_cast<unsigned>(count));
}
