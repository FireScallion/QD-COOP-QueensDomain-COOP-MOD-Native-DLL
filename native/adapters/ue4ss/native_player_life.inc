namespace Player=qdcoop::player;
using LifePhase=Player::LifePhase;

struct LifeLayout {
    int health{-1},suspend{-1},move_mode{-1},custom_mode{-1},camera_location{-1};
    void* dead{};
    void* damage{};
    void* yaw{};
    void* held{};
    void* modal{};
    void* no_interact{};
    std::array<int,7> trace{};
    std::array<int,2> camera{};
    const unsigned char* health_return{};
};
struct LocalLife {
    Weak pawn,instance,controller,movement,arms,camera,interaction;
    void* actor{};
    void* setter{};
    LifeLayout layout;
    Player::Life life;
    std::array<float,3> camera_origin{};
    double next_bind{},rise_until{};
    bool armed{},pending{},internal{},blocked{},locked{},camera_lowered{};
    bool was_damage{},was_yaw{},were_arms_visible{};
    std::uint8_t move_mode{},custom_mode{};
};
struct LifePeer {
    std::array<float,3> position{};
    Player::Life life;
    std::uint32_t generation{};
    double received{};
    qdcoop::Id rescue_identity;
    std::uint64_t rescue_account{};
    std::uint32_t checkpoint_flags{};
    float health{};
};
struct RescueHold {
    Player::Rescue message;
    Weak selection;
    double started{},next_request{};
    float health{};
    unsigned stage{};
    unsigned medicine{};
    bool selected{},attempted{};
};
struct RescueGrant {
    Player::Rescue message;
    double next_send{};
};
static LocalLife local_life;
static LifePeer local_life_pose;
static std::array<LifePeer,Player::slots> life_peers;
static std::array<RescueGrant,Player::slots> life_grants_pending;
static RescueHold rescue_hold;
static std::atomic<KillExec> life_multicast{};
static std::atomic<void*> life_watched_pawn{};
static void* volatile* life_opcode_slot;
static constexpr std::uintptr_t life_opcode_rva=0x4a63128,life_multicast_rva=0x11b4b20;
static bool medic_cleanup();
static void presentation_stop_look();
static void presentation_stop_camera();
static bool presentation_cleanup();
static void presentation_down_start(void* pawn,double time);
static void presentation_down_poll(void* pawn,double time);
static void presentation_hud_step(bool aimed,float progress);
static void world_scene_observe_finish(void* context,void* frame);

static int life_offset(void* type,std::string_view name,int size){
    const auto field=catalog_field(type,name,size);
    return field?*api.offset(field):-1;
}

static void observe_life_health(void* context,void* frame,void* result){
    if(GetCurrentThreadId()==game_thread.load())world_scene_observe_finish(context,frame);
    life_multicast.load(std::memory_order_acquire)(context,frame,result);
    if(context!=life_watched_pawn.load(std::memory_order_relaxed)||!context||
       GetCurrentThreadId()!=game_thread.load()||!frame||
       local_life.internal||(!local_life.armed&&!local_life.pending&&!local_life.locked))return;
    if(AnimU::field<void*>(frame,16)!=local_life.setter||
       AnimU::field<void*>(frame,24)!=context||
       AnimU::field<const unsigned char*>(frame,32)!=local_life.layout.health_return)return;
    auto* health=reinterpret_cast<float*>(static_cast<unsigned char*>(context)+local_life.layout.health);
    if(local_life.locked){
        *health=0;
    }else if(*health<=0&&!api.getbool(local_life.layout.dead,context,0)){
        *health=1;
        local_life.pending=true;
        local_life.armed=false;
        ++life_intercepted;
    }
}

static bool install_life_health(){
    if(life_opcode_slot)return true;
    if(!native_bootstrap||build_state.load()!=1||GetModuleHandleW(L"UE4SS.dll"))return false;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto original=image+life_multicast_rva;
    constexpr unsigned char entry[]{0x48,0x8b,0xca,0xe9,0x58,0x59,0xfe,0xff};
    if(std::memcmp(reinterpret_cast<const void*>(original),entry,sizeof(entry)))return false;
    const auto address=image+life_opcode_rva;
    MEMORY_BASIC_INFORMATION data{};
    if(!VirtualQuery(reinterpret_cast<const void*>(address),&data,sizeof(data))||
       data.State!=MEM_COMMIT||(data.Protect&PAGE_GUARD)||
       !(data.Protect&(PAGE_READWRITE|PAGE_WRITECOPY))||
       reinterpret_cast<std::uintptr_t>(data.BaseAddress)+data.RegionSize-address<sizeof(void*))return false;
    HMODULE module{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                         reinterpret_cast<LPCWSTR>(&observe_life_health),&module))return false;
    auto slot=reinterpret_cast<void* volatile*>(address);
    life_multicast.store(reinterpret_cast<KillExec>(original),std::memory_order_release);
    if(InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(&observe_life_health),
                                       reinterpret_cast<void*>(original))!=reinterpret_cast<void*>(original))return false;
    life_opcode_slot=slot;
    return true;
}

static void life_set_health(void* pawn,float health){
    if(world_input(world_life_health)!=local_life.setter)return;
    local_life.internal=true;
    api.event(pawn,local_life.setter,&health);
    local_life.internal=false;
}

static void life_set_camera(void* camera,const std::array<float,3>& position){
    const auto fn=world_input(world_life_relative);
    if(!fn)return;
    alignas(16) FactoryParameters args{};
    factory_put(args,local_life.layout.camera[0],position);
    factory_put(args,local_life.layout.camera[1],std::uint8_t{1});
    api.event(camera,fn,args.data());
}

static void life_unlock(bool restore_health=true){
    auto& state=local_life;
    state.armed=state.pending=false;
    presentation_stop_look();
    presentation_stop_camera();
    if(state.blocked){
        const auto gi=api.weakget(&state.instance);
        const auto fn=world_input(world_life_unblock);
        if(gi&&fn)api.event(gi,fn,nullptr);
        state.blocked=false;
    }
    if(!state.locked)return;
    state.locked=false;
    std::uint8_t ignore{};
    if(const auto controller=api.weakget(&state.controller))
        if(const auto fn=world_input(world_life_ignore))api.event(controller,fn,&ignore);
    if(const auto camera=api.weakget(&state.camera);camera&&state.camera_lowered)
        life_set_camera(camera,state.camera_origin);
    state.camera_lowered=false;
    const auto pawn=api.weakget(&state.pawn);
    if(!pawn||actor_is_destroying(pawn))return;
    api.setbool(state.layout.dead,pawn,false,0);
    api.setbool(state.layout.damage,pawn,state.was_damage,0);
    api.setbool(state.layout.yaw,pawn,state.was_yaw,0);
    std::array<std::uint8_t,2> visibility{state.were_arms_visible,0};
    if(const auto arms=api.weakget(&state.arms))
        if(const auto fn=world_input(world_life_visibility))api.event(arms,fn,visibility.data());
    if(const auto movement=api.weakget(&state.movement)){
        std::array<std::uint8_t,2> modes{state.move_mode?state.move_mode:std::uint8_t{1},state.custom_mode};
        if(const auto fn=world_input(world_life_mode))api.event(movement,fn,modes.data());
    }
    if(restore_health)life_set_health(pawn,1.f);
}

static bool cleanup_native_life(){
    local_life.armed=false;
    const bool presentation_clean=presentation_cleanup();
    if(!medic_cleanup())return false;
    life_watched_pawn.store(nullptr,std::memory_order_relaxed);
    life_unlock();
    local_life={};
    if(!life_opcode_slot)return presentation_clean;
    auto slot=life_opcode_slot;
    life_opcode_slot=nullptr;
    const auto hook=reinterpret_cast<void*>(&observe_life_health);
    const auto original=reinterpret_cast<void*>(life_multicast.load(std::memory_order_acquire));
    const auto previous=InterlockedCompareExchangePointer(slot,original,hook);
    if(previous==hook)++life_restored;
    return presentation_clean&&(previous==hook||previous==original);
}

static void reset_native_life(){
    local_life={};
    local_life_pose={};
    life_peers={};
    life_grants_pending={};
    rescue_hold={};
}

static void suspend_native_life(){
    local_life.armed=false;
    rescue_hold={};
    presentation_hud_step(false,0);
}

static bool prepare_life_functions(LifeLayout& layout){
    for(unsigned i=world_life_first;i<world_medic_first;++i)if(!world_input(i))return false;
    if(!world_input(world_kismet)||!world_input(world_control_rotation))return false;
    life_stage=2;
    const auto fn=world_input(world_life_health);
    const auto code=AnimU::field<const unsigned char*>(fn,96);
    if(!code||AnimU::field<int>(fn,104)!=179||code[176]!=0x04||
       AnimU::field<unsigned>(fn,176)!=0x0c020000||
       *api.parms(fn)!=4||factory_parameter(fn,"NewVal",4)!=0)return false;
    layout.health_return=code+176;
    for(const auto method:{world_life_cancel,world_life_cure,world_life_block,world_life_unblock,
                           world_life_stop,world_life_disable})
        if(*api.parms(world_input(method)))return false;
    for(const auto method:{world_life_interact,world_life_visible})
        if(*api.parms(world_input(method))!=1||factory_parameter(world_input(method),"ReturnValue",1)!=0)
            return false;
    life_stage=3;
    const auto count=world_input(world_life_count);
    if(*api.parms(count)!=12||factory_parameter(count,"Item",8)!=0||factory_parameter(count,"ReturnValue",4)!=8||
       *api.parms(world_input(world_life_controller))!=8||
       factory_parameter(world_input(world_life_controller),"ReturnValue",8)!=0||
       *api.parms(world_input(world_life_ignore))!=1||
       factory_parameter(world_input(world_life_ignore),"bNewMoveInput",1)!=0||
       *api.parms(world_input(world_life_mode))!=2||
       factory_parameter(world_input(world_life_mode),"NewMovementMode",1)!=0||
       factory_parameter(world_input(world_life_mode),"NewCustomMode",1)!=1||
       *api.parms(world_input(world_life_visibility))!=2||
       factory_parameter(world_input(world_life_visibility),"bNewVisibility",1)!=0||
       factory_parameter(world_input(world_life_visibility),"bPropagateToChildren",1)!=1||
       *api.parms(world_input(world_control_rotation))!=12||
       factory_parameter(world_input(world_control_rotation),"ReturnValue",12)!=0)return false;
    life_stage=4;
    const auto trace=world_input(world_life_trace);
    layout.trace={factory_parameter(trace,"WorldContextObject",8),factory_parameter(trace,"Start",12),
        factory_parameter(trace,"End",12),factory_parameter(trace,"bIgnoreSelf",1),
        factory_parameter(trace,"ReturnValue",1),factory_parameter(trace,"TraceChannel",1),
        factory_parameter(trace,"ActorsToIgnore",16)};
    const auto camera=world_input(world_life_relative);
    layout.camera={factory_parameter(camera,"NewLocation",12),factory_parameter(camera,"bTeleport",1)};
    return std::ranges::min(layout.trace)>=0&&std::ranges::min(layout.camera)>=0;
}

static bool prepare_life_actor(void* pawn,void* instance,LocalLife& state){
    life_stage=5;
    const auto type=AnimU::field<void*>(pawn,16);
    auto& layout=state.layout;
    layout.health=life_offset(type,"Health",4);
    layout.dead=catalog_field(type,"IsDead",1);
    layout.damage=catalog_field(type,"bCanBeDamaged",1);
    layout.yaw=catalog_field(type,"bUseControllerRotationYaw",1);
    layout.held=catalog_field(type,"InteractHeld",1);
    layout.modal=catalog_field(type,"IsInGameplayModal",1);
    layout.suspend=life_offset(AnimU::field<void*>(instance,16),"SuspendIsBlocked",4);
    if(layout.health<0||layout.suspend<0||!layout.dead||!layout.damage||!layout.yaw||!layout.held||
       !layout.modal)return false;
    void* controller{};
    api.event(pawn,world_input(world_life_controller),&controller);
    const auto movement=catalog_object_field(pawn,"CharacterMovement");
    const auto arms=catalog_object_field(pawn,"Arms");
    const auto camera=catalog_object_field(pawn,"FirstPersonCamera");
    const auto interaction=catalog_object_field(pawn,"PlayerInteractionComponent");
    if(!controller||!movement||!arms||!camera||!interaction||
       !makeweak(state.pawn,pawn)||!makeweak(state.instance,instance)||!makeweak(state.controller,controller)||
       !makeweak(state.movement,movement)||!makeweak(state.arms,arms)||!makeweak(state.camera,camera)||
       !makeweak(state.interaction,interaction))return false;
    life_stage=6;
    const auto move_type=AnimU::field<void*>(movement,16);
    layout.move_mode=life_offset(move_type,"MovementMode",1);
    layout.custom_mode=life_offset(move_type,"CustomMovementMode",1);
    layout.camera_location=life_offset(AnimU::field<void*>(camera,16),"RelativeLocation",12);
    layout.no_interact=catalog_field(AnimU::field<void*>(interaction,16),"DisableInteraction",1);
    return layout.move_mode>=0&&layout.custom_mode>=0&&layout.camera_location>=0&&layout.no_interact;
}

static void bind_native_life(void* pawn,void* instance,double time){
    if(!native_bootstrap)return;
    if(local_life.actor==pawn&&api.weakget(&local_life.pawn)==pawn&&
       api.weakget(&local_life.instance)==instance&&world_input(world_life_health)==local_life.setter)return;
    if(time<local_life.next_bind&&!local_life.actor)return;
    if(!cleanup_native_life())return;
    local_life.next_bind=time+.2;
    scan_world_inputs(2048);
    life_stage=1;
    LocalLife state{};
    if(!prepare_life_functions(state.layout)||!prepare_life_actor(pawn,instance,state))return;
    life_stage=7;
    if(!install_life_health())return;
    state.actor=pawn;
    state.setter=world_input(world_life_health);
    local_life=state;
    life_watched_pawn.store(pawn,std::memory_order_relaxed);
    life_stage=8;
}

static std::uint64_t new_life_token(){
    std::uint64_t token{};
    if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&token),sizeof(token),
                       BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)return 0;
    return token;
}

static void life_ground(void* movement){
    const auto mode=AnimU::field<std::uint8_t>(movement,local_life.layout.move_mode);
    if(!mode||mode==3)return; // MOVE_None or MOVE_Falling.
    if(const auto stop=world_input(world_life_stop))api.event(movement,stop,nullptr);
    if(const auto disable=world_input(world_life_disable))api.event(movement,disable,nullptr);
}

static bool enter_native_down(void* pawn,double time){
    auto& state=local_life;
    state.pending=false;
    const auto gi=api.weakget(&state.instance),controller=api.weakget(&state.controller);
    const auto movement=api.weakget(&state.movement),arms=api.weakget(&state.arms);
    const auto camera=api.weakget(&state.camera);
    const auto token=new_life_token();
    if(!gi||!controller||!movement||!arms||!camera||!token)return false;
    for(unsigned i=world_life_first;i<world_medic_first;++i)if(!world_input(i))return false;
    const auto before=AnimU::field<int>(gi,state.layout.suspend);
    if(before<0||before==INT_MAX)return false;
    api.event(gi,world_input(world_life_block),nullptr);
    if(AnimU::field<int>(gi,state.layout.suspend)!=before+1)return false;
    state.blocked=true;
    state.was_damage=api.getbool(state.layout.damage,pawn,0);
    state.was_yaw=api.getbool(state.layout.yaw,pawn,0);
    std::uint8_t visible{};
    api.event(arms,world_input(world_life_visible),&visible);
    state.were_arms_visible=visible!=0;
    state.move_mode=AnimU::field<std::uint8_t>(movement,state.layout.move_mode);
    state.custom_mode=AnimU::field<std::uint8_t>(movement,state.layout.custom_mode);
    state.camera_origin=AnimU::field<std::array<float,3>>(camera,state.layout.camera_location);
    state.locked=true;
    state.life={token,LifePhase::down};
    api.event(pawn,world_input(world_life_cancel),nullptr);
    api.event(pawn,world_input(world_life_cure),nullptr);
    api.setbool(state.layout.damage,pawn,false,0);
    api.setbool(state.layout.dead,pawn,true,0);
    api.setbool(state.layout.yaw,pawn,false,0);
    std::uint8_t ignore{1};
    api.event(controller,world_input(world_life_ignore),&ignore);
    life_ground(movement);
    std::array<std::uint8_t,2> hidden{};
    api.event(arms,world_input(world_life_visibility),hidden.data());
    presentation_down_start(pawn,time);
    life_set_health(pawn,0);
    ++life_downed;
    return true;
}

#include "native_rescue_save.inc"
#include "native_rescue_presentation.inc"

static bool fresh_life(const LifePeer& peer,LifePhase phase,double time){
    return peer.generation&&peer.life.phase==phase&&time-peer.received<=Player::rescue_fresh;
}

static void observe_life_pose(const Player::Pose& pose,double time){
    life_peers[pose.slot]={pose.position,pose.life,pose.generation,time,pose.rescue_identity,pose.rescue_account,
                           pose.checkpoint_flags,pose.health};
}

static void remember_local_life(const Player::Pose& pose,double time){
    local_life_pose={pose.position,pose.life,pose.generation,time,pose.rescue_identity,pose.rescue_account,
                     pose.checkpoint_flags,pose.health};
    if(host_role)life_peers[0]=local_life_pose;
}

static bool life_matches(const Player::Rescue& rescue,const LifePeer& donor,const LifePeer& target,double time){
    return fresh_life(donor,LifePhase::standing,time)&&donor.generation==rescue.donor_generation&&
           fresh_life(target,LifePhase::down,time)&&target.generation==rescue.target_generation&&
           target.life.token==rescue.token&&Player::within_rescue_range(donor.position,target.position);
}

#include "native_rescue_runtime.inc"

static bool apply_life_grant(const Player::Rescue& grant,double time){
    const auto pawn=api.weakget(&local_life.pawn);
    if(!pawn||!local_life.locked||medic_reserved_life(grant.token)||
       local_life.life!=Player::Life{grant.token,LifePhase::down}||
       !life_matches(grant,life_peers[grant.donor],local_life_pose,time)||
       !Player::within_rescue_range(life_peers[grant.donor].position,native_actor_location(pawn)))return false;
    life_unlock();
    local_life.life={grant.token,LifePhase::rising};
    local_life.rise_until=time+.48;
    ++life_revived;
    return true;
}

static void send_life_grant(RescueGrant& grant,double time){
    if(time<grant.next_send)return;
    grant.next_send=time+.12;
    const auto& rescue=grant.message;
    if(!rescue.target){
        if(apply_life_grant(rescue,time))++life_grants;
        return;
    }
    E::Packet packet{};
    const auto mask=(1u<<(rescue.target-1))&active_peer_mask;
    if(mask&&Player::encode_rescue(envelope,rescue,packet)&&send(packet,mask))++life_grants;
}

static void accept_life_request(Player::Rescue rescue,double time){
    if(!life_matches(rescue,life_peers[rescue.donor],life_peers[rescue.target],time))return;
    auto& grant=life_grants_pending[rescue.target];
    if(grant.message.token==rescue.token)return;
    rescue.op=Player::RescueOp::grant;
    grant={rescue,0};
    send_life_grant(grant,time);
}

static void receive_native_rescue(const ReceivedPacket& received,double time){
    Player::Rescue rescue;
    if(!automatic_limit||!Player::decode_rescue(envelope,received.packet,rescue))return;
    if(!host_role){
        if(received.peer==0&&rescue.op==Player::RescueOp::grant&&rescue.target)
            apply_life_grant(rescue,time);
        return;
    }
    unsigned relay{};
    if(rescue.op!=Player::RescueOp::request||
       !Player::assign_slot(rescue.donor,true,received.peer,active_peer_mask,relay)||
       rescue.donor==rescue.target)return;
    accept_life_request(rescue,time);
}

static bool life_rescue_aim(void* pawn,const LifePeer& peer){
    const auto camera=api.weakget(&local_life.camera);
    const auto rotation_fn=world_input(world_control_rotation);
    const auto trace=world_input(world_life_trace),kismet=world_input(world_kismet);
    if(!camera||!rotation_fn||!trace||!kismet)return false;
    std::array<float,3> start{};
    if(camera_manager_ready){
        if(!presentation_camera_location(pawn,start))return false;
    }else{
        alignas(16) FactoryParameters transform{};
        api.event(camera,getters[4].fn,transform.data());
        start=AnimU::field<std::array<float,3>>(transform.data(),getters[4].at+16);
    }
    std::array<float,3> rotation{};
    api.event(pawn,rotation_fn,rotation.data());
    constexpr double radians=.017453292519943295;
    const auto pitch=rotation[0]*radians,yaw=rotation[1]*radians;
    const auto dx=peer.position[0]-start[0],dy=peer.position[1]-start[1],dz=peer.position[2]-62.f-start[2];
    const auto along=dx*std::cos(pitch)*std::cos(yaw)+dy*std::cos(pitch)*std::sin(yaw)+dz*std::sin(pitch);
    if(along<0||dx*dx+dy*dy+dz*dz-along*along>65.*65.)return false;
    auto end=peer.position;
    end[2]-=58.f;
    alignas(16) FactoryParameters args{};
    const auto& at=local_life.layout.trace;
    factory_put(args,at[0],pawn);
    factory_put(args,at[1],start);
    factory_put(args,at[2],end);
    factory_put(args,at[3],std::uint8_t{1});
    api.event(kismet,trace,args.data());
    return !args[at[4]];
}

static bool life_selection(void*& item,unsigned& medicine){
    const auto instance=api.weakget(&local_life.instance);
    const auto save=instance?catalog_object_field(instance,"ActiveSaveSlot"):nullptr;
    if(!save)return false;
    const auto at=life_offset(AnimU::field<void*>(save,16),"SelectedConsumable",8);
    if(at<0)return false;
    item=AnimU::field<void*>(save,at);
    if(!item)return true;
    std::string path;
    if(!catalog_path(item,path))return false;
    const auto match=std::ranges::find(medicine_paths,path);
    if(match==medicine_paths.end())return true;
    const auto count=world_input(world_life_count);
    if(!count)return false;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,item);
    api.event(instance,count,args.data());
    const auto quantity=AnimU::field<int>(args.data(),8);
    if(quantity>0)medicine=static_cast<unsigned>(match-medicine_paths.begin())+1;
    return quantity>=0;
}

static unsigned life_target(void* pawn,double time,unsigned preferred){
    const auto& state=local_life;
    if(!std::ranges::any_of(life_peers,[&](const auto& peer){return fresh_life(peer,LifePhase::down,time);}))
        return Player::unassigned;
    const auto interaction=api.weakget(&state.interaction);
    const auto interact=world_input(world_life_interact);
    if(!interaction||!interact||api.getbool(state.layout.modal,pawn,0)||
       api.getbool(state.layout.no_interact,interaction,0))return Player::unassigned;
    const auto position=native_actor_location(pawn);
    for(unsigned i=0;i<life_peers.size();++i){
        const auto& peer=life_peers[i];
        if((preferred!=Player::unassigned&&preferred!=i)||!fresh_life(peer,LifePhase::down,time)||
           !Player::within_rescue_range(position,peer.position))continue;
        std::uint8_t can_interact{};
        api.event(pawn,interact,&can_interact);
        if(!can_interact)return Player::unassigned;
        if(life_rescue_aim(pawn,peer))return i;
    }
    return Player::unassigned;
}

static void native_rescue_hold(void* pawn,double time,unsigned target,bool pressed){
    auto& hold=rescue_hold;
    const auto& state=local_life;
    if(!pressed){hold={};return;}
    hold.stage=1;
    const auto cancel=[&](){hold.message={};hold.started=0;};
    if(hold.attempted&&!hold.message.token)return;
    if(target==Player::unassigned){cancel();return;}
    hold.stage=2;
    void* selected{};
    unsigned medicine{};
    if(!life_selection(selected,medicine)){cancel();return;}
    hold.stage=4;
    const auto health=AnimU::field<float>(pawn,state.layout.health);
    const auto& peer=life_peers[target];
    if(!hold.message.token){
        hold.stage=6;
        if(medicine&&(!medic_identity().valid()||!peer.rescue_identity.valid()))return;
        const auto attempt=new_life_token();
        if(!attempt||!local_life_pose.generation||(selected&&!makeweak(hold.selection,selected)))return;
        hold.message={host_role?0u:Player::unassigned,target,local_life_pose.generation,peer.generation,
                      peer.life.token,attempt,Player::RescueOp::request};
        hold.started=time;
        hold.health=health;
        hold.selected=selected!=nullptr;
        hold.medicine=medicine;
        hold.attempted=true;
        return;
    }
    const auto& rescue=hold.message;
    if(health<hold.health-.1f||medicine!=hold.medicine||
       (hold.selected?api.weakget(&hold.selection)!=selected||!selected:selected!=nullptr)||
       target!=rescue.target||peer.life.token!=rescue.token||peer.generation!=rescue.target_generation){
        cancel();
        return;
    }
    hold.health=std::max(hold.health,health);
    hold.stage=7;
    if(time-hold.started<Player::rescue_hold||time<hold.next_request)return;
    if(time-hold.started>Player::rescue_hold+5.){cancel();return;}
    hold.next_request=time+.12;
    hold.stage=8;
    ++life_requests;
    if(hold.medicine){
        medic_hold_complete(hold.medicine,selected);
        return;
    }
    if(host_role)accept_life_request(rescue,time);
    else{
        E::Packet packet{};
        if(Player::encode_rescue(envelope,rescue,packet))send(packet,1);
    }
}

static void native_life_step(void* pawn,double time){
    auto& state=local_life;
    if(state.actor!=pawn)return;
    presentation_prepare(time);
    if(!camera_manager_ready)camera_manager_ready=presentation_manager_layout();
    if(state.pending)enter_native_down(pawn,time);
    if(state.locked){
        if(const auto movement=api.weakget(&state.movement))life_ground(movement);
        presentation_down_poll(pawn,time);
    }else{
        const bool alive=AnimU::field<float>(pawn,state.layout.health)>0&&!api.getbool(state.layout.dead,pawn,0);
        if(!alive||time>=state.rise_until)state.life={0,alive?LifePhase::standing:LifePhase::unavailable};
        state.armed=alive&&active_peer_mask&&(std::ranges::any_of(life_peers,[&](const auto& peer){
            return (!host_role||&peer!=&life_peers[0])&&fresh_life(peer,LifePhase::standing,time);
        })||checkpoint_can_wipe(time));
    }
    auto target=Player::unassigned;
    if(state.life.phase==LifePhase::standing&&active_peer_mask){
        const bool pressed=api.getbool(state.layout.held,pawn,0);
        const auto preferred=pressed&&rescue_hold.message.token?rescue_hold.message.target:Player::unassigned;
        target=life_target(pawn,time,preferred);
        native_rescue_hold(pawn,time,target,pressed);
    }else rescue_hold={};
    medic_pump(pawn,time,active_peer_mask!=0);
    if(host_role)for(auto& grant:life_grants_pending){
        const auto& rescue=grant.message;
        if(!rescue.token)continue;
        if(life_matches(rescue,life_peers[rescue.donor],life_peers[rescue.target],time))send_life_grant(grant,time);
        else grant={};
    }
    const auto debit=medic_runtime.ledger.outgoing.phase;
    const auto progress=rescue_hold.started&&(!rescue_hold.medicine||debit<Medic::Debit::intended)?
        static_cast<float>(std::clamp((time-rescue_hold.started)/Player::rescue_hold,.001,1.)):0.f;
    presentation_hud_step(target!=Player::unassigned,progress);
}

extern "C" __declspec(dllexport) int QDEnemyInspectNativeRescue(Player::RescueView* out){
    if(GetCurrentThreadId()!=game_thread.load()||!out)return 0;
    const auto progress=rescue_hold.started?std::clamp((now()-rescue_hold.started)/Player::rescue_hold,0.,1.):0.;
    *out={local_life.life,local_life.armed,local_life.locked,
          rescue_hold.message.token?rescue_hold.message.target:UINT32_MAX,static_cast<float>(progress),rescue_hold.stage};
    return life_stage;
}
