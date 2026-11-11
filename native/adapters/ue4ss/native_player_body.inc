// All access to the local animation instance and remote display is on the game thread.
static void* load_player_asset(std::string_view path,unsigned type,bool class_asset=false);

struct BodyAssetIdentity {
    Weak asset{};
    std::uint16_t clip{};
};
struct SourceLane {
    BodyAssetIdentity identity;
    const void* instance{};
    std::uint32_t serial{};
    float position{};
};
struct PlayerSourceBody {
    Weak pawn_type,move_type,arms_instance,arms_type;
    void* running{};
    void* dashing{};
    int movement{-1},mode{-1},arms{-1},speed{-1},root{-1},left{-1};
    BodyAssetIdentity root_asset,left_asset;
    std::array<SourceLane,Player::lane_count> lanes{};
};
static PlayerSourceBody player_body_source;
static int player_montage_position=-1,player_montage_rate=-1;
static void* player_montage_playing;

static int player_offset(void* type,std::string_view name,int width){
    const auto field=catalog_field(type,name,width);
    return field?*api.offset(field):-1;
}

static std::uint16_t source_clip(void* asset,BodyAssetIdentity& cache){
    if(!asset)return 0;
    if(api.weakget(&cache.asset)==asset)return cache.clip;
    std::string path;
    unsigned clip=0;
    if(catalog_path(asset,path)){
        const auto end=std::end(Player::clips_by_path);
        const auto match=std::lower_bound(std::begin(Player::clips_by_path),end,path,
            [](unsigned id,std::string_view wanted){return Player::clips[id].source<wanted;});
        if(match!=end&&Player::clips[*match].source==path)clip=*match;
    }
    cache={};
    if(makeweak(cache.asset,asset))cache.clip=static_cast<std::uint16_t>(clip);
    return static_cast<std::uint16_t>(clip);
}

static bool player_montages_ready(){
    if(player_montage_playing)return true;
    const auto fn=anim_function(1),layout=world_input(world_montage_layout);
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    // GetCurrentActiveMontage in build 26496428 reads Num at 0x80, Data
    // at 0x78, Montage at 0, and the desired blend value at 0x150.
    constexpr unsigned char code[]{
        0x48,0x63,0x81,0x80,0,0,0,0x85,0xc0,0x7e,0x38,0x48,0x8d,0x50,0xff,
        0x0f,0x57,0xc0,0x48,0x8b,0x41,0x78
    };
    if(!fn||!layout||AnimU::field<std::uintptr_t>(fn,216)!=image+0x2d62540||
       std::memcmp(reinterpret_cast<const void*>(image+0x2694640),code,sizeof(code))||
       player_offset(layout,"Montage",8)!=0||*api.propertiesSize(layout)<0x154)
        return false;
    player_montage_position=player_offset(layout,"Position",4);
    player_montage_rate=player_offset(layout,"PlayRate",4);
    if(player_montage_position<0||player_montage_rate<0)return false;
    player_montage_playing=catalog_field(layout,"bPlaying",1);
    return player_montage_playing!=nullptr;
}

static void capture_player_lanes(void* arms,Player::Body& body){
    if(!player_montages_ready())return;
    struct MontageArray {const void* const* data;int count,capacity;};
    const auto montages=AnimU::field<MontageArray>(arms,0x78);
    if(montages.count<0||montages.count>32||montages.capacity<montages.count||
       montages.capacity>256||(montages.count&&!montages.data))return;
    auto& tracks=player_body_source.lanes;
    for(int i=montages.count;i-->0;){
        const auto instance=montages.data[i];
        if(!instance||AnimU::field<float>(instance,0x150)==0.f)continue;
        const auto asset=AnimU::field<void*>(instance,0);
        if(!asset||!catalog_class_matches(catalog_montage,AnimU::field<void*>(asset,16)))continue;
        BodyAssetIdentity identity;
        const auto known=std::find_if(tracks.begin(),tracks.end(),[&](const SourceLane& lane){
            return api.weakget(&lane.identity.asset)==asset;
        });
        const auto id=known==tracks.end()?source_clip(asset,identity):known->identity.clip;
        if(!id)continue;
        const auto group=Player::clips[id].lane;
        if(!group||body.lanes[group-1].clip)continue;
        const auto position=AnimU::field<float>(instance,player_montage_position);
        const auto rate=AnimU::field<float>(instance,player_montage_rate);
        if(!std::isfinite(position)||!std::isfinite(rate)||position<0||
           position>Player::clips[id].length+.02f||std::abs(rate)>8.f)continue;
        auto& track=tracks[group-1];
        const auto loop=Player::loop_for(id,track.position);
        const bool wrapped=loop&&position>=loop->start&&position<=loop->end;
        if(track.instance!=instance||track.identity.clip!=id||
           (rate>=0&&position+.1f<track.position&&!wrapped)){
            if(++track.serial==0)++track.serial;
        }
        if(known==tracks.end())track.identity=identity;
        track.instance=instance;
        track.position=position;
        body.lanes[group-1]={id,static_cast<std::uint16_t>(api.getbool(player_montage_playing,instance,0)),
                             track.serial,position,rate};
    }
    for(unsigned i=0;i<tracks.size();++i)if(!body.lanes[i].clip)tracks[i].instance=nullptr;
}

static Player::Body capture_player_body(void* pawn){
    auto& source=player_body_source;
    Player::Body body;
    const auto pawn_type=AnimU::field<void*>(pawn,16);
    if(api.weakget(&source.pawn_type)!=pawn_type){
        source={};
        if(!makeweak(source.pawn_type,pawn_type))return body;
        source.movement=player_offset(pawn_type,"CharacterMovement",8);
        source.arms=player_offset(pawn_type,"Arms",8);
        source.speed=player_offset(pawn_type,"InternalDefaultMaxWalkSpeed",4);
        source.running=catalog_field(pawn_type,"Running",1);
        source.dashing=catalog_field(pawn_type,"Dashing",1);
    }
    if(source.running&&api.getbool(source.running,pawn,0))body.flags|=Player::running;
    if(source.dashing&&api.getbool(source.dashing,pawn,0))body.flags|=Player::dashing;
    if(source.speed>=0){
        const auto speed=AnimU::field<float>(pawn,source.speed);
        if(std::isfinite(speed)&&speed>=1.f&&speed<=10000.f)body.walk_speed=speed;
    }
    const auto movement=source.movement>=0?AnimU::field<void*>(pawn,source.movement):nullptr;
    if(movement){
        const auto type=AnimU::field<void*>(movement,16);
        if(api.weakget(&source.move_type)!=type&&makeweak(source.move_type,type))
            source.mode=player_offset(type,"MovementMode",1);
        if(source.mode>=0&&AnimU::field<std::uint8_t>(movement,source.mode)==3)body.flags|=Player::falling;
    }
    if(const auto control=world_input(world_control_rotation);control&&*api.parms(control)==12){
        alignas(16) std::array<float,3> rotation{};
        api.event(pawn,control,rotation.data());
        if(std::isfinite(rotation[0]))body.pitch=std::clamp(std::remainder(rotation[0],360.f),-45.f,45.f);
    }
    const auto mesh=source.arms>=0?AnimU::field<void*>(pawn,source.arms):nullptr;
    const auto getter=anim_function(0),anim_type=world_input(world_anim_instance);
    void* arms{};
    if(!mesh||!api.isa(mesh,mesh_class)||!getter||!anim_type)return body;
    api.event(mesh,getter,&arms);
    if(!arms||!api.isa(arms,anim_type))return body;
    if(api.weakget(&source.arms_instance)!=arms){
        if(!makeweak(source.arms_instance,arms))return body;
        for(auto& lane:source.lanes)lane.instance=nullptr;
    }
    const auto type=AnimU::field<void*>(arms,16);
    if(api.weakget(&source.arms_type)!=type){
        if(!makeweak(source.arms_type,type))return body;
        source.root=player_offset(type,"RootAnim",8);
        source.left=player_offset(type,"LeftRootAnim",8);
    }
    const auto root=source.root>=0?source_clip(AnimU::field<void*>(arms,source.root),source.root_asset):0;
    const auto left=source.left>=0?source_clip(AnimU::field<void*>(arms,source.left),source.left_asset):0;
    if(!Player::clips[root].lane)body.root=static_cast<std::uint16_t>(root);
    if(!Player::clips[left].lane)body.left=static_cast<std::uint16_t>(left);
    capture_player_lanes(arms,body);
    return body;
}

enum GraphInput {
    graph_walk,graph_run,graph_run_blend,graph_move,graph_jump,graph_jump_time,graph_turn,
    graph_head,graph_idle,graph_left_idle,graph_stride,graph_left_stride,graph_root,graph_left,
    graph_life_sequence,graph_life_time,graph_life_weight,graph_input_count
};
struct GraphTrack {
    Weak asset;
    std::uint32_t serial{};
    std::uint16_t clip{};
    std::uint64_t sequence{};
};
struct PlayerGraph {
    Weak instance;
    Player::Locomotion motion;
    std::array<GraphTrack,Player::lane_count> lanes{};
    std::uint16_t root{},left{};
    Player::Life life;
    double life_started{};
    double previous{};
};
static Weak player_graph_class,player_graph_skeleton;
static std::array<int,graph_input_count> player_graph_offsets{};
static std::array<Weak,std::size(Player::clips)> player_body_assets{};
static std::array<double,std::size(Player::clips)> player_body_retries{};
static double player_asset_next;
static bool player_graph_unavailable;

static bool body_compression_ready(){
    static constexpr std::pair<std::string_view,std::string_view> required[]{
        {"/Game/QDCoopNative/Compression/QD_bone.QD_bone","/Script/Engine.AnimBoneCompressionSettings"},
        {"/Game/QDCoopNative/Compression/QD_curve.QD_curve","/Script/Engine.AnimCurveCompressionSettings"}
    };
    // The NativeBody package depends on codec settings in the NativeSpear
    // package. UE deserializes the dependent clips before class loading returns.
    for(const auto& [path,type]:required){
        const auto asset=load_player_asset(path,UINT32_MAX);
        std::string actual;
        if(!asset||!catalog_path(AnimU::field<void*>(asset,16),actual)||actual!=type)return false;
    }
    return true;
}

static void reset_player_body(){
    player_body_source={};
    player_graph_class=player_graph_skeleton={};
    player_body_assets={};
    player_body_retries={};
    player_asset_next=0;
    player_graph_unavailable=false;
    player_montage_playing=nullptr;
}

static bool bind_player_graph(void* component,void* mesh,PlayerGraph& graph){
    const auto setter=api.weakget(&catalog_methods[9]);
    const auto getter=anim_function(0);
    if(!setter||!getter||player_graph_unavailable||
       *api.parms(setter)!=8||factory_parameter(setter,"NewClass",8)!=0)
        return false;
    auto type=api.weakget(&player_graph_class);
    if(!type){
        if(!world_input(world_make_class)||!world_input(world_soft_class)||!world_input(world_load_class))
            return false;
        if(!body_compression_ready()){player_graph_unavailable=true;return false;}
        type=load_player_asset("/Game/QDCoopBody/ABP_QDBody.ABP_QDBody_C",catalog_blueprint,true);
        const auto skeleton=type?catalog_object_field(type,"TargetSkeleton"):nullptr;
        const auto body_skeleton=catalog_object_field(mesh,"Skeleton");
        if(!type||!skeleton||skeleton!=body_skeleton||!makeweak(player_graph_skeleton,skeleton)){
            player_graph_unavailable=true;
            return false;
        }
        static constexpr std::array<std::string_view,graph_input_count> names{
            "WalkRate","RunRate","RunBlend","MoveWeight","JumpBlend","JumpTime","LowerTurn",
            "HeadRotation","IdleWeight","LeftIdleWeight","ArmWalkWeight","LeftWalkWeight",
            "RootSequence","LeftSequence","LifeSequence","LifeTime","LifeWeight"
        };
        for(unsigned i=0;i<names.size();++i){
            const int width=i>=graph_root&&i<=graph_life_sequence?8:(i==graph_turn||i==graph_head?12:4);
            player_graph_offsets[i]=player_offset(type,names[i],width);
            if(player_graph_offsets[i]<0){player_graph_unavailable=true;return false;}
        }
        if(!makeweak(player_graph_class,type))return false;
    }
    api.event(component,setter,&type);
    void* instance{};
    api.event(component,getter,&instance);
    if(!instance||AnimU::field<void*>(instance,16)!=type||!makeweak(graph.instance,instance))return false;
    ++player_graph_bound;
    return true;
}

static void* body_clip_asset(unsigned clip,double time){
    if(!clip)return nullptr;
    if(const auto asset=api.weakget(&player_body_assets[clip]))return asset;
    if(time<player_asset_next||time<player_body_retries[clip])return nullptr;
    player_asset_next=time+.025; // Bound cold package work per game tick.
    const auto& definition=Player::clips[clip];
    const auto name=definition.name;
    std::string path=definition.lane?"/Game/QDCoopBody/Montages/":"/Game/QDCoopBody/Animations/";
    path.append(name).append(".").append(name);
    const auto asset=load_player_asset(path,definition.lane?catalog_montage:catalog_sequence);
    if(!asset||catalog_object_field(asset,"Skeleton")!=api.weakget(&player_graph_skeleton)||
       !makeweak(player_body_assets[clip],asset)){
        player_body_retries[clip]=time+5;
        ++player_body_missing;
        return nullptr;
    }
    return asset;
}

template<class T>static void write_graph(void* instance,GraphInput input,const T& value){
    std::memcpy(static_cast<unsigned char*>(instance)+player_graph_offsets[input],&value,sizeof(value));
}

static void apply_player_montage(void* instance,GraphTrack& track,const Player::Lane& lane,
                                 std::uint64_t sequence,double elapsed,double time){
    if(track.sequence==sequence)return;
    auto old=api.weakget(&track.asset);
    const bool new_action=track.clip!=lane.clip||track.serial!=lane.serial||(lane.clip&&!old);
    alignas(16) FactoryParameters args{};
    if(new_action&&old){
        if(const auto stop=anim_function(3)){
            factory_put(args,0,std::min(Player::clips[track.clip].blend,.25f));
            factory_put(args,8,old);
            api.event(instance,stop,args.data());
            ++player_action_stops;
        }
        track.asset={};
        track.clip=0;
        old=nullptr;
    }
    if(lane.clip){
        const auto& clip=Player::clips[lane.clip];
        const auto target=std::min(Player::advance_lane(lane,elapsed),std::nextafter(clip.cooked,0.f));
        const auto rate=lane.playing?lane.rate*(clip.cooked/clip.length):0.f;
        const auto asset=old?old:body_clip_asset(lane.clip,time);
        if(!asset)return;
        args={};
        if(new_action){
            const auto play=anim_function(2);
            if(!play)return;
            factory_put(args,0,asset);
            factory_put(args,8,rate!=0.f?rate:1.f);
            factory_put(args,16,target);
            // bStopAllMontages stays false. Each cooked lane has its own slot group.
            api.event(instance,play,args.data());
            if(!(AnimU::field<float>(args.data(),24)>0)||!makeweak(track.asset,asset))return;
            ++player_action_starts;
        }
        const auto position=anim_function(4),set_rate=anim_function(6),set_position=anim_function(7);
        if(position&&set_rate&&set_position){
            args={};
            factory_put(args,0,asset);
            api.event(instance,position,args.data());
            const auto actual=AnimU::field<float>(args.data(),8);
            auto error=target-actual;
            if(const auto loop=Player::loop_for(lane.clip,lane.position))
                error=std::remainder(error,(loop->end-loop->start)*(clip.cooked/clip.length));
            if(std::abs(error)>.65f){
                factory_put(args,8,target);
                api.event(instance,set_position,args.data());
            }
            factory_put(args,8,lane.playing?std::clamp(rate+error*8.f,-8.f,8.f):0.f);
            api.event(instance,set_rate,args.data());
        }
    }
    track.clip=lane.clip;
    track.serial=lane.serial;
    track.sequence=sequence;
}

static void apply_life_animation(void* instance,PlayerGraph& graph,Player::Life life,const E::Sample& sample,double time){
    if(graph.life!=life){
        const unsigned clip=life.phase==LifePhase::down?184:life.phase==LifePhase::rising?185:0;
        const auto asset=clip?body_clip_asset(clip,time):nullptr;
        if(clip&&!asset){
            write_graph(instance,graph_life_weight,0.f);
            return;
        }
        if(asset)write_graph(instance,graph_life_sequence,asset);
        graph.life=life;
        graph.life_started=sample.state.time;
    }
    const auto elapsed=static_cast<float>(std::max(0.,sample.time-graph.life_started));
    float weight{};
    if(life.phase==LifePhase::down){
        write_graph(instance,graph_life_time,std::min(elapsed,.72f));
        weight=std::clamp(elapsed/.15f,0.f,1.f);
    }else if(life.phase==LifePhase::rising){
        write_graph(instance,graph_life_time,std::min(elapsed,.48f));
        weight=1.f-std::clamp((elapsed-.33f)/.15f,0.f,1.f);
    }
    write_graph(instance,graph_life_weight,weight);
}

static bool apply_player_body(PlayerGraph& graph,const Player::Body& body,Player::Life life,const E::Sample& sample,double time){
    const auto instance=api.weakget(&graph.instance);
    if(!instance||AnimU::field<void*>(instance,16)!=api.weakget(&player_graph_class))return false;
    const auto& state=sample.state;
    auto& motion=graph.motion;
    motion.advance(body,{state.v[12],state.v[13],state.v[14]},state.v[10],
                   graph.previous?sample.time-graph.previous:0.);
    graph.previous=sample.time;
    write_graph(instance,graph_walk,motion.walk_rate);
    write_graph(instance,graph_run,motion.run_rate);
    write_graph(instance,graph_run_blend,motion.run_blend);
    write_graph(instance,graph_move,motion.move_weight);
    write_graph(instance,graph_jump,motion.jump_blend);
    write_graph(instance,graph_jump_time,motion.jump_time);
    write_graph(instance,graph_turn,std::array<float,3>{0,motion.turn,0});
    write_graph(instance,graph_head,std::array<float,3>{0,0,90.f-body.pitch});
    write_graph(instance,graph_idle,.35f);
    write_graph(instance,graph_left_idle,.35f);
    write_graph(instance,graph_stride,motion.stride);
    write_graph(instance,graph_left_stride,motion.stride);
    if(body.root!=graph.root){
        if(const auto asset=body_clip_asset(body.root,time)){
            write_graph(instance,graph_root,asset);
            graph.root=body.root;
        }
    }
    if(body.left!=graph.left){
        if(const auto asset=body_clip_asset(body.left,time)){
            write_graph(instance,graph_left,asset);
            graph.left=body.left;
        }
    }
    for(unsigned i=0;i<graph.lanes.size();++i)
        apply_player_montage(instance,graph.lanes[i],body.lanes[i],state.sequence,sample.time-state.time,time);
    apply_life_animation(instance,graph,life,sample,time);
    return true;
}
