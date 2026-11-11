namespace PT=qdcoop::persistent_travel;
namespace SC=qdcoop::native_save_clone;

struct PersistentLoad {
    PT::Passport target;
    Weak instance,old_world,original,replacement;
    void* references{};
    int active_at{},base_at{};
    AnimU::NameKey level{};
    ULONGLONG expires{};
    bool original_rooted{},replacement_rooted{},opened{};
};
struct PersistentTravel {
    PT::Passport local,remote;
    E::Envelope remote_scope;
    WP::Inbox draft;
    PersistentLoad load;
    ULONGLONG next_publish{},next_prepare{},received_at{},missing_since{},host_clean_until{};
};
static PersistentTravel persistent_travel;

static PT::Map persistent_map(void* world){
    std::string path;
    return catalog_path(world,path)?PT::identify(path):PT::Map::none;
}
static void prepare_persistent_world_scope(void* world){
    world_stream.scripted_world=persistent_map(world)==PT::Map::intro;
}
static E::Envelope persistent_envelope(const PT::Passport& passport){
    return {envelope.room_high,envelope.room_low,PT::epoch(PT::canonical_epoch(passport.map),passport.nonce)};
}
static bool receive_persistent_travel(const ReceivedPacket& received,unsigned kind){
    auto& state=persistent_travel;
    if(kind==PT::wire_kind){
        PT::Passport value;
        if(!host_role&&!received.peer&&(active_peer_mask&1)&&PT::decode(envelope,received.packet,value)&&
           value.serial>state.remote.serial){
            state.remote=value;
            state.remote_scope=persistent_envelope(value);
            state.received_at=GetTickCount64();
            state.draft={};
            persistent_stage=1;
        }
        return true;
    }
    if(host_role||received.peer||kind!=WP::snapshot_kind||!state.remote.nonce)return false;
    const auto& scope=state.remote_scope;
    if(scope.world==envelope.world)return false;
    WP::Part part;
    if(!WP::decode(scope,received.packet,part))return false;
    state.draft.push(part);
    if(state.draft.complete())persistent_stage=2;
    return true;
}

static std::array<void*,2> persistent_startup_fields(void* save){
    if(!save)return {};
    const auto type=AnimU::field<void*>(save,16);
    const auto intro=catalog_field(type,"DoIntro",1),wake=catalog_field(type,"DoWakeOnBeach",1);
    if(!medic_field_class(intro,"BoolProperty")||!medic_field_class(wake,"BoolProperty"))return {};
    return {intro,wake};
}
static void publish_persistent_world(const LocalWorld& local,ULONGLONG time){
    auto& state=persistent_travel;
    if(!host_role||!active_peer_mask||time<state.next_publish)return;
    state.next_publish=time+750;
    const auto map=persistent_map(local.world);
    if(map==PT::Map::none)return;
    if(state.local.map!=map||state.local.nonce!=checkpoint_world_nonce){
        if(state.local.serial==UINT64_MAX)return;
        const auto save=catalog_object_field(local.instance,"ActiveSaveSlot");
        const auto flags=persistent_startup_fields(save);
        if(!flags[0])return;
        const auto startup=(api.getbool(flags[0],save,0)?PT::do_intro:0u)|
                           (api.getbool(flags[1],save,0)?PT::wake_on_beach:0u);
        state.local={state.local.serial+1,checkpoint_world_nonce,map,startup};
    }
    E::Packet packet{};
    if(PT::encode(envelope,state.local,packet))send(packet,active_peer_mask);
}

static bool persistent_reference(PersistentLoad& load,Weak& reference,bool& owned,bool add){
    if(!add&&!owned)return true;
    const auto instance=api.weakget(&load.instance),object=api.weakget(&reference);
    if(!instance||!object){owned=false;return !add;}
    const auto field=load.references;
    auto* address=static_cast<unsigned char*>(instance)+*api.offset(field);
    const auto current=AnimU::field<WorldArray>(address,0);
    if(!current.valid(4096))return false;
    std::vector<void*> refs;
    if(current.count){
        const auto data=static_cast<void* const*>(current.data);
        refs.assign(data,data+current.count);
    }
    const auto found=std::find(refs.begin(),refs.end(),object);
    if(add&&found!=refs.end())return true;
    if(!add&&found==refs.end()){owned=false;return true;}
    if(add){
        if(refs.size()>=4096)return false;
        refs.push_back(object);
    }else refs.erase(found);
    const auto count=static_cast<int>(refs.size());
    const WorldArray values{refs.data(),count,count};
    const auto copy=AnimU::field<const SC::CopyValue*>(field,0)[23];
    copy(field,address,&values,1);
    owned=add;
    const auto updated=AnimU::field<WorldArray>(address,0);
    if(!updated.valid(4096)||updated.count!=count)return false;
    const auto data=static_cast<void* const*>(updated.data);
    return !count||std::equal(refs.begin(),refs.end(),data);
}

static bool persistent_travel_close(){
    auto& load=persistent_travel.load;
    if(!load.target.serial)return true;
    const auto gi=api.weakget(&load.instance),original=api.weakget(&load.original),
               clone=api.weakget(&load.replacement);
    if(gi&&original&&clone){
        auto restore=[&](int offset){
            if(AnimU::field<void*>(gi,offset)==clone)
                std::memcpy(static_cast<unsigned char*>(gi)+offset,&original,sizeof(original));
        };
        restore(load.active_at);
        restore(load.base_at);
    }
    if(!persistent_reference(load,load.replacement,load.replacement_rooted,false)||
       !persistent_reference(load,load.original,load.original_rooted,false))return false;
    load={};
    return true;
}

static bool persistent_save_binding(const LocalWorld& local,PersistentLoad& load,void* save){
    const auto type=AnimU::field<void*>(local.instance,16);
    const auto active=catalog_field(type,"ActiveSaveSlot",8),base=catalog_field(type,"BaseActiveSave",8);
    const auto references=catalog_field(type,"ReferencedObjects",16);
    if(!medic_field_class(active,"ObjectProperty")||!medic_field_class(base,"ObjectProperty")||
       !medic_field_class(references,"ArrayProperty"))return false;
    const auto inner=AnimU::field<void*>(references,0x78);
    std::string path;
    if(!medic_field_class(inner,"ObjectProperty")||*api.size(inner)!=8||
       !catalog_path(AnimU::field<void*>(inner,0x78),path)||path!="/Script/CoreUObject.Object")return false;
    const auto vtable=AnimU::field<const std::uintptr_t*>(references,0);
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(!vtable||vtable[23]!=image+0x116fa40)return false;
    load.active_at=*api.offset(active);
    load.base_at=*api.offset(base);
    load.references=references;
    return load.active_at!=load.base_at&&AnimU::field<void*>(local.instance,load.active_at)==save&&
           AnimU::field<void*>(local.instance,load.base_at)==save&&makeweak(load.original,save)&&
           makeweak(load.old_world,local.world)&&makeweak(load.instance,local.instance);
}

static void* prepare_persistent_save(const LocalWorld& local,const WP::Snapshot& draft,const PT::Passport& target,
                                     PT::Map origin,PersistentLoad& load){
    const auto save=medic_active(medic_runtime.save,local.pawn);
    const auto make=world_input(world_travel_create_save);
    if(!make){scan_world_inputs(4096);return nullptr;}
    if(!save||!world_progress_ready()||!persistent_save_binding(local,load,save)||
       !medic_params(world_travel_create_save,16,{{"SaveGameClass",8,0},{"ReturnValue",8,8}}))return nullptr;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto type=AnimU::field<std::uintptr_t>(save,16);
    SC::Layout binding;
    if(!SC::bind(image,catalog_name_pool,type,catalog_read,binding))return nullptr;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,type);
    api.event(factory_object(factory_gameplay),make,args.data());
    const auto clone=AnimU::field<void*>(args.data(),8);
    if(!SC::copy(binding,clone,save)||!makeweak(load.replacement,clone)||!world_progress_apply(clone,draft,false,true))
        return nullptr;
    if(origin!=target.map){
        const auto flags=persistent_startup_fields(clone);
        if(!flags[0])return nullptr;
        api.setbool(flags[0],clone,(target.startup&PT::do_intro)!=0,0);
        api.setbool(flags[1],clone,(target.startup&PT::wake_on_beach)!=0,0);
    }
    return clone;
}

static NativeWorld::Phase finish_persistent_load(const LocalWorld& local,ULONGLONG time){
    using Phase=NativeWorld::Phase;
    auto& load=persistent_travel.load;
    if(time>=load.expires){persistent_travel_close();return Phase::failed;}
    if(!local.pawn||local.world==api.weakget(&load.old_world))return Phase::suspended;
    const auto clone=api.weakget(&load.replacement);
    if(!clone||local.instance!=api.weakget(&load.instance)||persistent_map(local.world)!=load.target.map||
       AnimU::field<void*>(local.instance,load.active_at)!=clone||AnimU::field<void*>(local.instance,load.base_at)!=clone){
        persistent_travel_close();
        return Phase::failed;
    }
    if(!persistent_reference(load,load.original,load.original_rooted,false))return Phase::suspended;
    const auto nonce=load.target.nonce;
    load={};
    persistent_travel.draft={};
    reset_native_gameplay_scope();
    checkpoint_rollback_reset();
    checkpoint_world_nonce=nonce;
    ++persistent_guest_changes;
    persistent_stage=6;
    return Phase::suspended;
}

static NativeWorld::Phase start_persistent_load(const LocalWorld& local,ULONGLONG time){
    using Phase=NativeWorld::Phase;
    auto& load=persistent_travel.load;
    const auto old=api.weakget(&load.original),clone=api.weakget(&load.replacement);
    const auto gi=api.weakget(&load.instance);
    if(time>=load.expires||!gi||!old||!clone||local.instance!=gi||local.world!=api.weakget(&load.old_world)||
       AnimU::field<void*>(gi,load.active_at)!=old||AnimU::field<void*>(gi,load.base_at)!=old){
        persistent_travel_close();
        return Phase::failed;
    }
    if(!cleanup_native_actors())return Phase::suspended;
    const auto open=world_input(world_cp_travel),gs=factory_object(factory_gameplay);
    if(!open||!gs){persistent_travel_close();return Phase::failed;}
    auto set=[&](int offset){std::memcpy(static_cast<unsigned char*>(gi)+offset,&clone,sizeof(clone));};
    set(load.active_at);
    set(load.base_at);
    if(!persistent_reference(load,load.replacement,load.replacement_rooted,false)){
        persistent_travel_close();
        return Phase::failed;
    }
    alignas(16) FactoryParameters args{};
    factory_put(args,0,gi);
    factory_put(args,8,load.level);
    args[16]=1;
    load.opened=true;
    load.expires=time+30000;
    persistent_stage=5;
    api.event(gs,open,args.data());
    return Phase::suspended;
}

static NativeWorld::Phase persistent_world_transition(const LocalWorld& local,ULONGLONG time){
    using Phase=NativeWorld::Phase;
    auto& state=persistent_travel;
    if(state.load.target.serial)
        return state.load.opened?finish_persistent_load(local,time):start_persistent_load(local,time);
    if(!automatic_limit||rollback_busy())return Phase::idle;
    if(host_role){
        if(state.local.map==PT::Map::none)return Phase::idle;
        if(!local.pawn){
            if(!state.missing_since)state.missing_since=time;
            if(time-state.missing_since<15000){suspend_native_life();return Phase::suspended;}
            return Phase::world_changed;
        }
        state.missing_since=0;
        if(local.world==api.weakget(&attached_world))return Phase::idle;
        const auto destination=persistent_map(local.world);
        if(destination==PT::Map::none||reward_busy()||receipt_enabled)return Phase::world_changed;
        if(!state.host_clean_until)state.host_clean_until=time+5000;
        if(time>=state.host_clean_until)return Phase::failed;
        if(!cleanup_native_actors())return Phase::suspended;
        std::uint64_t nonce{};
        if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&nonce),sizeof(nonce),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0||
           !nonce||nonce==checkpoint_world_nonce)return Phase::failed;
        reset_native_gameplay_scope();
        checkpoint_rollback_reset();
        checkpoint_world_nonce=nonce;
        world_stream.new_world_arrival=destination==PT::Map::ruins;
        state.next_publish=state.host_clean_until=0;
        ++persistent_host_changes;
        return Phase::suspended;
    }
    if(!state.remote.nonce||state.remote_scope.world==envelope.world)
        return Phase::idle;
    if(time-state.received_at>30000)return Phase::failed;
    if(!local.pawn||time<state.next_prepare||!state.draft.complete())return Phase::idle;
    state.next_prepare=time+250;
    const auto origin=persistent_map(local.world);
    if(origin==PT::Map::none||!checkpoint_medic_idle()||checkpoint_worker.busy()||
       reward_busy()||receipt_enabled||local_life.locked||local_life.pending||!(checkpoint_flags()&1))return Phase::idle;
    WP::Snapshot snapshot;
    if(!state.draft.snapshot(snapshot)||(state.remote.map==PT::Map::ruins&&!snapshot.arrival.serial))return Phase::idle;
    persistent_stage=3;
    const auto open=world_input(world_cp_travel),gs=factory_object(factory_gameplay);
    if(!open||!gs||!medic_params(world_cp_travel,40,{{"WorldContextObject",8,0},{"LevelName",8,8},
                                                 {"bAbsolute",1,16},{"Options",16,24}}))return Phase::idle;
    const auto path=PT::package(state.remote.map);
    const std::wstring package(path.begin(),path.end());
    const auto level=medic_name(package);
    if(!medic_name_is(level,package))return Phase::failed;
    PersistentLoad load;
    const auto clone=prepare_persistent_save(local,snapshot,state.remote,origin,load);
    if(!clone)return Phase::idle;
    persistent_stage=4;
    load.target=state.remote;
    load.level=level;
    load.expires=time+5000;
    state.load=load;
    auto& staged=state.load;
    if(!persistent_reference(staged,staged.original,staged.original_rooted,true)||
       !persistent_reference(staged,staged.replacement,staged.replacement_rooted,true)){
        persistent_stage=40;
        persistent_travel_close();
        return Phase::failed;
    }
    return start_persistent_load(local,time);
}
