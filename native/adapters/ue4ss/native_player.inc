#include "native_player_body.inc"
#include "native_player_equipment.inc"
#include "native_player_gear_bones.inc"

struct PlayerBodyFrame {
    std::uint64_t sequence{};
    Player::Body body;
    Player::Equipment equipment;
    Player::Life life;
};
struct RemotePlayer {
    E::Timeline timeline;
    E::Clock clock;
    Weak actor;
    PlayerGraph graph;
    DisplayEquipment equipment;
    std::array<PlayerBodyFrame,E::history> body_frames{};
    std::array<Player::GearHistory,Player::equipment_slots> gear_frames{};
    std::uint32_t generation{},actor_generation{};
    double received{},next_spawn{};
    std::array<float,6> placed{};
    bool visible{},positioned{};
};

static std::array<RemotePlayer,Player::slots> remote_players;

static unsigned native_enrollment_origins(void* context,std::array<std::array<float,3>,Player::slots>& origins){
    origins[0]=native_actor_location(context);
    unsigned count=1;
    if(!host_role||!automatic_limit||!active_peer_mask)return count;
    const auto time=now();
    for(unsigned slot=1;slot<origins.size();++slot){
        if(!(active_peer_mask&(1u<<(slot-1))))continue;
        const auto& player=remote_players[slot];
        const auto latest=player.timeline.latest();
        if(!latest||latest->generation!=player.generation||time-player.received>Player::rescue_fresh)continue;
        std::copy_n(latest->v.begin()+6,3,origins[count++].begin());
    }
    return count;
}
static Weak player_source,player_mesh,player_idle;
static std::uint32_t player_generation;
static std::uint64_t player_sequence;
static double player_next_send,player_next_load;
static bool player_idle_unavailable;
static constexpr float player_height=85.f;
static constexpr std::string_view player_mesh_path=
    "/Game/Player/Assets/FPModel/Player2026/Player_FullChar_2026.Player_FullChar_2026";
static constexpr std::string_view player_idle_path=
    "/Game/Player/Assets/FPModel/Player2026/Player_FullChar_2026_coolPose.Player_FullChar_2026_coolPose";

static void player_visibility(RemotePlayer& player,bool visible){
    if(player.visible==visible)return;
    if(const auto actor=api.weakget(&player.actor)){
        unsigned char hidden=!visible;
        api.event(actor,hide_fn,&hidden);
    }
    player.visible=visible;
}

static bool destroy_player(RemotePlayer& player){
    if(!player.actor_generation&&!api.weakget(&player.actor))return true;
    player_visibility(player,false);
    if(!destroy_owned_actor(player.actor,false))return false;
    player.actor={};
    player.graph={};
    player.equipment={};
    player.actor_generation=0;
    player.positioned=false;
    ++player_destroyed;
    return true;
}

static bool cleanup_players(){
    bool clean=true;
    for(auto& player:remote_players)if(!destroy_player(player))clean=false;
    return clean;
}

static void reset_players(){
    remote_players={};
    player_source=player_mesh=player_idle={};
    player_generation=0;
    player_sequence=0;
    player_next_send=player_next_load=0;
    player_idle_unavailable=false;
    reset_player_body();
    reset_player_equipment();
}

static void receive_gear_pose(const ReceivedPacket& received){
    Player::GearPose pose{};
    unsigned relay{};
    if(!automatic_limit||!Player::decode_gear_pose(envelope,received.packet,pose)||
       !Player::assign_slot(pose.slot,host_role,received.peer,active_peer_mask,relay)){
        ++gear_pose_rejected;
        return;
    }
    auto& player=remote_players[pose.slot];
    const auto latest=player.timeline.latest();
    if(!latest||pose.generation!=player.generation){
        ++gear_pose_rejected;
        return;
    }
    // Accept a reordered companion ahead of the body. When its exact body is
    // present, require the same mesh and component identity before relaying.
    const auto& anchor=player.body_frames[pose.sequence%player.body_frames.size()];
    const auto& current=player.body_frames[latest->sequence%player.body_frames.size()];
    if((anchor.sequence==pose.sequence&&!Player::same_part(pose,anchor.equipment.parts[pose.part]))||
       (anchor.sequence!=pose.sequence&&pose.sequence<=latest->sequence&&
        !Player::same_part(pose,current.equipment.parts[pose.part]))||
       !player.gear_frames[pose.part].push(pose,latest->sequence)){
        ++gear_pose_rejected;
        return;
    }
    ++gear_pose_received;
    E::Packet packet{};
    if(relay&&Player::encode_gear_pose(envelope,pose,packet))send(packet,relay);
}

static bool receive_player(const ReceivedPacket& received,double time){
    if(received.packet.size<40)return false;
    const auto kind=A::wire::get<std::uint32_t>(received.packet,32);
    if(receive_persistent_travel(received,kind))return true;
    if(receive_elevator_intent(received,kind,time))return true;
    if(receive_world_interaction(received,kind,time))return true;
    if(receive_door_trigger(received,kind,time))return true;
    if(receive_enemy_rest(received,kind,time))return true;
    if(receive_story_reward(received,kind,time))return true;
    if(receive_world_scene(received,kind,time))return true;
    if(receive_native_boss(received,kind))return true;
    if(receive_boss_interact(received,kind,time))return true;
    if(receive_projectile(received,kind,time))return true;
    if(receive_sound(received,kind,time))return true;
    if(receive_placed_claim(received,kind,time))return true;
    if(receive_placed_deposit(received,kind,time))return true;
    if(receive_body_material(received,kind,time))return true;
    if(receive_visual_mount(received,kind,time))return true;
    if(receive_boss_hud(received,kind,time))return true;
    if(kind==Hit::hit_kind||kind==Hit::ack_kind){receive_native_damage(received,kind,time);return true;}
    if(kind==Player::gear_pose_kind){receive_gear_pose(received);return true;}
    if(kind==Player::rescue_kind){receive_native_rescue(received,time);return true;}
    if(kind==Medic::wire_kind){receive_native_medic(received,time);return true;}
    if(kind==CP::wire_kind){receive_checkpoint(received,time);return true;}
    if(kind==WP::snapshot_kind||kind==WP::ack_kind){receive_world_progress(received,kind,time);return true;}
    if(kind!=Player::pose_kind)return false;
    Player::Pose pose{};
    unsigned relay{};
    if(!automatic_limit||!Player::decode(envelope,received.packet,pose)||
       !Player::assign_slot(pose.slot,host_role,received.peer,active_peer_mask,relay)){
        ++player_rejected;
        return true;
    }
    auto& player=remote_players[pose.slot];
    if(host_role)pose.rescue_account=steam_transport?
        steam_peer_accounts[received.peer].load(std::memory_order_acquire):0;
    if(pose.generation<player.generation){
        ++player_rejected;
        return true;
    }
    if(pose.generation>player.generation){
        player.generation=pose.generation;
        player.timeline={};
        player.clock={};
        player.body_frames={};
        player.gear_frames={};
        player.next_spawn=0;
    }
    E::State state{pose.slot+1,pose.generation,pose.sequence,pose.time,{}};
    state.v[0]=1;
    state.v[10]=pose.yaw;
    for(unsigned i=0;i<3;++i){
        state.v[6+i]=pose.position[i];
        state.v[12+i]=pose.velocity[i];
    }
    state.v[21]=state.v[22]=state.v[23]=state.v[24]=1;
    if(!player.timeline.push(state,time)){
        ++player_rejected;
        return true;
    }
    player.clock.push(pose.time,time);
    player.body_frames[pose.sequence%player.body_frames.size()]={pose.sequence,pose.body,pose.equipment,pose.life};
    observe_life_pose(pose,time);
    player.received=time;
    ++player_received;
    E::Packet packet{};
    if(relay&&Player::encode(envelope,pose,packet)&&send(packet,relay))++player_relayed;
    return true;
}

static void send_player(void* context,double time){
    if(!active_peer_mask||time<player_next_send)return;
    player_next_send=time+.05;
    if(api.weakget(&player_source)!=context){
        if(!makeweak(player_source,context))return;
        ++player_generation;
        player_sequence=0;
        player_body_source={};
        gear_source={};
    }
    Player::Pose pose{host_role?0u:Player::unassigned,player_generation,++player_sequence,time,
                      native_actor_location(context),0,{}};
    alignas(16) FactoryParameters args{};
    api.event(context,getters[1].fn,args.data());
    pose.yaw=std::remainder(AnimU::field<float>(args.data(),getters[1].at+4),360.f);
    args={};
    api.event(context,getters[2].fn,args.data());
    pose.velocity=AnimU::field<std::array<float,3>>(args.data(),getters[2].at);
    pose.body=capture_player_body(context);
    pose.equipment=capture_equipment(context,time);
    if(local_life.actor==context){
        pose.life=local_life.life;
        if(local_life.layout.health>=0){
            const auto health=AnimU::field<float>(context,local_life.layout.health);
            if(std::isfinite(health))pose.health=std::clamp(health,0.f,1000000.f);
        }
    }
    pose.rescue_identity=medic_identity();
    pose.rescue_account=medic_account();
    pose.checkpoint_flags=checkpoint_flags();
    E::Packet packet{};
    if(Player::encode(envelope,pose,packet)&&send(packet,active_peer_mask)){
        ++player_sent;
        remember_local_life(pose,time);
        send_gear_bones(pose,time);
    }
}

// The cooked assets use top-level object paths. The FString subpath in the
// returned FSoftObjectPath stays empty, so these temporaries own no allocation.
static void* load_world_asset(std::string_view path,unsigned type,bool class_asset,void* class_base){
    const auto kismet=world_input(world_kismet);
    const auto make_path=world_input(class_asset?world_make_class:world_make_path);
    const auto make_ref=world_input(class_asset?world_soft_class:world_soft_ref);
    const auto load=world_input(class_asset?world_load_class:world_load_asset);
    player_asset_stage=1;
    if(!kismet||!make_path||!make_ref||!load)return nullptr;
    const auto path_input=factory_parameter(make_path,"PathString",16);
    const auto path_output=factory_parameter(make_path,"ReturnValue",24);
    const auto ref_input=factory_parameter(make_ref,class_asset?"SoftClassPath":"SoftObjectPath",24);
    const auto ref_output=factory_parameter(make_ref,"ReturnValue",40);
    const auto asset_input=factory_parameter(load,class_asset?"AssetClass":"Asset",40);
    const auto asset_output=factory_parameter(load,"ReturnValue",8);
    player_asset_stage=2;
    if(path_input<0||path_output<0||ref_input<0||ref_output<0||asset_input<0||asset_output<0)return nullptr;
    std::wstring wide(path.begin(),path.end());
    const auto size=static_cast<int>(wide.size()+1);
    struct StringArgument {const wchar_t* data;int count,capacity;};
    alignas(16) FactoryParameters args{},ref{},asset{};
    factory_put(args,path_input,StringArgument{wide.c_str(),size,size});
    api.event(kismet,make_path,args.data());
    std::memcpy(ref.data()+ref_input,args.data()+path_output,24);
    api.event(kismet,make_ref,ref.data());
    std::memcpy(asset.data()+asset_input,ref.data()+ref_output,40);
    api.event(kismet,load,asset.data());
    const auto object=AnimU::field<void*>(asset.data(),asset_output);
    std::string actual;
    player_asset_stage=3;
    if(!object||!catalog_path(object,actual)||actual!=path)return nullptr;
    if(class_asset){
        if(!class_base||!catalog_subclass(object,class_base))return nullptr;
    }else if(type!=UINT32_MAX&&!catalog_class_matches(type,AnimU::field<void*>(object,16)))return nullptr;
    return object;
}
static void* load_player_asset(std::string_view path,unsigned type,bool class_asset){
    return load_world_asset(path,type,class_asset,class_asset?world_input(world_anim_instance):nullptr);
}

static void* player_body(double time){
    if(const auto mesh=api.weakget(&player_mesh))return mesh;
    if(time<player_next_load)return nullptr;
    player_next_load=time+2;
    if(!world_input(world_load_asset)||!world_input(world_make_path)||!world_input(world_soft_ref))
        scan_world_inputs(2048);
    const auto mesh=load_player_asset(player_mesh_path,catalog_skeletal_mesh);
    if(!mesh||!makeweak(player_mesh,mesh))return nullptr;
    return mesh;
}

static void play_player_idle(void* component){
    const auto play=world_input(world_play_animation);
    if(!play)return;
    const auto animation=factory_parameter(play,"NewAnimToPlay",8);
    const auto loop=factory_parameter(play,"bLooping",1);
    if(animation<0||loop<0)return;
    auto idle=api.weakget(&player_idle);
    if(!idle&&!player_idle_unavailable){
        idle=load_player_asset(player_idle_path,catalog_sequence);
        player_idle_unavailable=!idle||!makeweak(player_idle,idle);
    }
    if(!idle)return;
    alignas(16) FactoryParameters args{};
    factory_put(args,animation,idle);
    factory_put(args,loop,std::uint8_t{1});
    api.event(component,play,args.data());
    player_asset_stage=5;
}

static bool spawn_player(void* context,RemotePlayer& player,const std::array<float,6>& pose,double time){
    if(time<player.next_spawn)return false;
    player.next_spawn=time+.25;
    const auto mesh=player_body(time);
    const auto collision=factory_object(factory_collision);
    if(!mesh||!collision)return false;
    player.actor_generation=0;
    player.graph={};
    player.equipment={};
    const auto actor=factory_spawn(context,factory_object(factory_visual_type),{pose[0],pose[1],pose[2]},player.actor);
    const auto component=actor?catalog_object_field(actor,"SkeletalMeshComponent"):nullptr;
    if(!actor||!factory_apply_mesh(component,mesh)||!makeweak(player.equipment.body,component)){
        destroy_player(player);
        return false;
    }
    unsigned char disabled{};
    api.event(actor,collision,&disabled);
    player_asset_stage=4;
    if(!bind_player_graph(component,mesh,player.graph))play_player_idle(component);
    player.actor_generation=player.generation;
    player.visible=true;
    ++player_created;
    return true;
}

static void player_step(void* context,double time){
    if(!factory_configured||!api.weakget(&replica_destroy_method))return;
    send_player(context,time);
    for(auto& player:remote_players){
        if(!player.generation)continue;
        if((player.actor_generation!=player.generation||time-player.received>3)&&!destroy_player(player))continue;
        if(time-player.received>.5){
            player_visibility(player,false);
            continue;
        }
        E::Sample sample{};
        if(!player.timeline.sample(player.clock,time,sample))continue;
        const auto& v=sample.state.v;
        std::array<float,6> pose{v[6],v[7],v[8]-player_height,0,v[10],0};
        auto actor=api.weakget(&player.actor);
        if(!actor){
            player.positioned=false;
            if(!spawn_player(context,player,pose,time))continue;
            actor=api.weakget(&player.actor);
            if(!actor)continue;
        }
        const auto sequence=sample.state.sequence;
        const auto& body=player.body_frames[sequence%player.body_frames.size()];
        if(body.sequence==sequence&&prepare_gear(time)){
            const auto component=api.weakget(&player.equipment.body);
            const auto prince=api.weakget(&player_mesh);
            if(component&&prince){
                if(const auto mesh=apply_player_skin(player.equipment,component,prince,body.equipment.skin,time);
                   mesh&&!api.weakget(&player.graph.instance)){
                    player.graph={};
                    if(!bind_player_graph(component,mesh,player.graph))play_player_idle(component);
                }
                if(player.equipment.weapon!=body.equipment.weapon&&body.equipment.weapon)++player_equipped;
                apply_equipment(actor,component,player.equipment,body.equipment,time);
                for(unsigned i=0;i<player.gear_frames.size();++i){
                    const auto& part=body.equipment.parts[i];
                    if(part.visible&&part.mesh&&Player::equipment_meshes[part.mesh].bone_count)
                        if(const auto bones=player.gear_frames[i].sample(sequence,part))
                            apply_gear_bones(player.equipment.parts[i],*bones,time);
                }
            }
        }
        if(api.weakget(&player.graph.instance)){
            pose[4]=std::remainder(pose[4]-90.f,360.f); // Preview3 mesh-to-Pawn alignment.
            if(body.sequence==sequence&&apply_player_body(player.graph,body.body,body.life,sample,time))++player_body_frames;
        }
        if(!player.positioned||player.placed!=pose){
            alignas(16) FactoryParameters args{};
            std::memcpy(args.data(),pose.data(),sizeof(pose));
            args[164]=1; // TeleportPhysics, without a sweep.
            api.event(actor,actor_fn,args.data());
            player.placed=pose;
            player.positioned=true;
        }
        player_visibility(player,true);
    }
}

extern "C" __declspec(dllexport) int QDEnemyInspectNativePlayer(unsigned slot,Player::Binding* out){
    if(GetCurrentThreadId()!=game_thread.load()||slot>=remote_players.size()||!out)return 0;
    const auto& player=remote_players[slot];
    if(!player.positioned||!api.weakget(&player.actor))return 0;
    const auto latest=player.timeline.latest();
    Player::Binding result{};
    AnimU::WeakKey key{};
    std::memcpy(&key,player.actor.bytes,sizeof(key));
    result.object_index=key.index;
    result.object_serial=key.serial;
    result.generation=player.actor_generation;
    result.visible=player.visible;
    result.sequence=latest?latest->sequence:0;
    *out=result;
    return 1;
}

extern "C" __declspec(dllexport) int QDEnemyInspectNativeEquipment(unsigned slot,Player::EquipmentBinding* out){
    if(GetCurrentThreadId()!=game_thread.load()||slot>=remote_players.size()||!out)return 0;
    const auto& player=remote_players[slot];
    if(!player.positioned||!api.weakget(&player.actor))return 0;
    Player::EquipmentBinding value{};
    value.skin=static_cast<std::uint16_t>(player.equipment.skin);
    value.weapon=static_cast<std::uint16_t>(player.equipment.weapon);
    for(unsigned i=0;i<player.equipment.parts.size();++i){
        const auto& part=player.equipment.parts[i];
        if(!api.weakget(&part.component))continue;
        AnimU::WeakKey key{};
        std::memcpy(&key,part.component.bytes,sizeof(key));
        value.parts[i]={key.index,key.serial,part.mesh,static_cast<std::uint16_t>(part.visible)};
    }
    *out=value;
    return 1;
}

extern "C" __declspec(dllexport) int QDEnemyInspectNativeEquipmentPose(unsigned slot,unsigned part,Player::GearPoseBinding* out){
    if(GetCurrentThreadId()!=game_thread.load()||slot>=remote_players.size()||part>=Player::equipment_slots||!out)
        return 0;
    const auto& player=remote_players[slot];
    if(!player.positioned||!api.weakget(&player.actor))return 0;
    const auto& display=player.equipment.parts[part];
    if(!api.weakget(&display.component))return 0;
    *out={display.bone_sequence,display.serial,display.mesh,display.hand,static_cast<std::uint8_t>(display.visible)};
    return 1;
}
