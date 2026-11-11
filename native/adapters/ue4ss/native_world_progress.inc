namespace WP=qdcoop::world_progress;
using WorldMapRemove=bool(*)(void*,const void*,const void*);
static void world_actor_queue(const WP::Guid& guid);
static void world_mechanisms_capture(void* pawn,double time,std::vector<WP::MechanismState>& out);
static void world_barriers_capture(void* pawn,double time,WP::Snapshot& out);
static void world_barriers_reset();
static void world_stream_observe(void* pawn,double time);
static void world_stream_capture(void* pawn,double time,WP::Snapshot& out);
static void world_stream_reset();
static void world_music_capture(void* save,double time,std::string& out);
static void world_music_reset();
static void world_story_reset();
static void native_placed_items_snapshot(const std::vector<WP::NamedState>& names);
static void* load_world_asset(std::string_view path,unsigned type,bool class_asset,void* class_base);

struct WorldProgressPeer {
    WP::Revision ack;
    unsigned next{};
    double retry{};
};
struct WorldProgressRuntime {
    Weak save;
    MedicMap actors,items;
    WorldMapRemove remove{};
    WP::Revision published,applied,rejected;
    WP::Bytes payload;
    WP::Inbox inbox;
    WP::Snapshot authoritative;
    std::array<WorldProgressPeer,Player::slots-1> peers{};
    std::vector<AnimU::NameKey> quests;
    double next_bind{},next_capture{},next_apply{},next_ack{};
    double next_notify_bind{};
    bool ready{},fault{},notify{},notifiers_bound{},force_revision{};
};
static WorldProgressRuntime world_progress;

static void world_progress_reset(){
    world_progress={};
    world_stream_reset();
    world_music_reset();
    world_story_reset();
    world_barriers_reset();
}
static bool world_progress_ready(){return world_progress.ready&&!world_progress.fault;}
static MedicMap world_actor_map(void* type){
    const auto field=catalog_field(type,"SavableActorStates",80);
    if(!medic_field_class(field,"MapProperty"))return {};
    const auto key=AnimU::field<void*>(field,0x78),value=AnimU::field<void*>(field,0x80);
    std::string path;
    if(!medic_field_class(key,"StructProperty")||!medic_field_class(value,"IntProperty")||
       *api.size(key)!=16||*api.size(value)!=4||
       !catalog_path(AnimU::field<void*>(key,0x78),path)||path!="/Script/CoreUObject.Guid"||
       AnimU::field<unsigned>(field,0xa0)||
       AnimU::field<std::array<int,6>>(field,0x88)!=std::array<int,6>{16,20,24,28,4,28})return {};
    return {field,*api.offset(field)};
}
static MedicMap world_item_map(void* type){
    const auto field=catalog_field(type,"SavableActorItems",80);
    if(!medic_field_class(field,"MapProperty"))return {};
    const auto key=AnimU::field<void*>(field,0x78),value=AnimU::field<void*>(field,0x80);
    std::string path;
    if(!medic_field_class(key,"StructProperty")||!medic_field_class(value,"ObjectProperty")||
       *api.size(key)!=16||*api.size(value)!=8||*api.offset(value)!=16||
       !catalog_path(AnimU::field<void*>(key,0x78),path)||path!="/Script/CoreUObject.Guid"||
       !catalog_path(AnimU::field<void*>(value,0x78),path)||path!="/Game/Blueprints/ItemSystem/FS_ItemData.FS_ItemData_C"||
       AnimU::field<unsigned>(field,0xa0)||
       AnimU::field<std::array<int,6>>(field,0x88)!=std::array<int,6>{16,24,28,32,8,32})return {};
    return {field,*api.offset(field)};
}
static void* world_item_type(){
    const auto field=world_progress.items.field;
    return field?AnimU::field<void*>(AnimU::field<void*>(field,0x80),0x78):nullptr;
}
static bool world_progress_bind(void* save,double time){
    auto& state=world_progress;
    if(api.weakget(&state.save)==save&&state.remove)return true;
    if(time<state.next_bind)return false;
    state.next_bind=time+.25;
    if(!world_input(world_progress_remove))scan_world_inputs(2048);
    const auto fn=world_input(world_progress_remove);
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto remove=image+0x2756630;
    constexpr unsigned char dispatch[]{0xe8,0x63,0x65,0x9d,0xff};
    constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x08,0x55,0x48,0x8d,0x6c,0x24,0xa9,0x48,0x81,0xec,0xc0,0,0,0};
    if(!fn||*api.parms(fn)!=85||AnimU::field<std::uintptr_t>(fn,216)!=image+0x2d7ff70||
       std::memcmp(reinterpret_cast<const void*>(image+0x2d800c8),dispatch,sizeof(dispatch))||
       std::memcmp(reinterpret_cast<const void*>(remove),prefix,sizeof(prefix)))return false;
    const auto type=AnimU::field<void*>(save,16);
    const auto actors=world_actor_map(type),items=world_item_map(type);
    if(!actors.field||!items.field||!makeweak(state.save,save))return false;
    state.actors=actors;
    state.items=items;
    state.remove=reinterpret_cast<WorldMapRemove>(remove);
    state.ready=false;
    state.next_capture=state.next_apply=0;
    return true;
}
static bool world_progress_capture(void* save,WP::Snapshot& out){
    struct NameEntry { AnimU::NameKey key; std::int32_t value{}; };
    struct ItemEntry { WP::Guid key; void* item{}; };
    static_assert(sizeof(NameEntry)==12&&sizeof(WP::ActorState)==20&&sizeof(ItemEntry)==24);
    WP::Snapshot snapshot;
    std::vector<NameEntry> names;
    std::vector<ItemEntry> items;
    const auto map=reinterpret_cast<std::uintptr_t>(world_progress.actors.address(save));
    const auto markers=reinterpret_cast<std::uintptr_t>(medic_runtime.save.markers.address(save));
    if(!WP::read_sparse_map<WP::ActorState,28>(map,catalog_read,snapshot.actors)||
       !WP::read_sparse_map<NameEntry,20>(markers,catalog_read,names)||
       !WP::read_sparse_map<ItemEntry,32>(reinterpret_cast<std::uintptr_t>(world_progress.items.address(save)),
                                        catalog_read,items))return false;
    std::erase_if(snapshot.actors,[](const auto& entry){return WP::personal(entry.key);});
    for(const auto& entry:names){
        std::string name;
        if(!AnimU::name_string(catalog_name_pool,entry.key,catalog_read,name))return false;
        if(!WP::personal(name))snapshot.names.push_back({std::move(name),entry.value});
    }
    const auto item_type=world_item_type();
    if(!item_type)return false;
    for(const auto& entry:items){
        if(WP::personal(entry.key))continue;
        std::string path;
        if(entry.item&&(!api.isa(entry.item,item_type)||!catalog_path(entry.item,path)||!WP::stream_path(path)))return false;
        snapshot.items.push_back({entry.key,std::move(path)});
    }
    std::ranges::sort(snapshot.actors,{},&WP::ActorState::key);
    std::ranges::sort(snapshot.names,WP::name_less,&WP::NamedState::key);
    std::ranges::sort(snapshot.items,{},&WP::ActorItemState::key);
    if(!WP::valid(snapshot))return false;
    out=std::move(snapshot);
    return true;
}
static bool world_progress_name(std::string_view name,AnimU::NameKey& key){
    const auto length=static_cast<int>(name.size());
    const auto wide=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name.data(),length,nullptr,0);
    if(!wide)return false;
    std::wstring text(wide,L'\0');
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name.data(),length,text.data(),wide)!=wide)return false;
    key=medic_name(text);
    std::string observed;
    return AnimU::name_string(catalog_name_pool,key,catalog_read,observed)&&WP::name_equal(name,observed);
}
template<class Value>struct WorldProgressEdit {
    MedicMap map;
    alignas(8) std::array<unsigned char,16> key{};
    std::optional<Value> before,after;
};
template<class Entry,class Less,class Key,class Value>
static bool world_progress_plan(const std::vector<Entry>& old,const std::vector<Entry>& desired,
                                const MedicMap& map,Less less,Key key,std::vector<WorldProgressEdit<Value>>& edits){
    auto from=old.begin(),to=desired.begin();
    while(from!=old.end()||to!=desired.end()){
        WorldProgressEdit<Value> edit{map,{},std::nullopt,std::nullopt};
        if(to==desired.end()||(from!=old.end()&&less(from->key,to->key))){
            if(!key(from->key,edit.key))return false;
            edit.before=from++->value;
        }else if(from==old.end()||less(to->key,from->key)){
            if(!key(to->key,edit.key))return false;
            edit.after=to++->value;
        }else{
            if(from->value==to->value){++from;++to;continue;}
            if(!key(to->key,edit.key))return false;
            edit.before=from++->value;
            edit.after=to++->value;
        }
        edits.push_back(edit);
    }
    return true;
}
template<class Value>static bool world_progress_put(void* save,const WorldProgressEdit<Value>& edit,
                                                     const std::optional<Value>& value){
    const auto& binding=medic_runtime.save;
    const auto map=edit.map.address(save),field=edit.map.field;
    const auto key=edit.key.data();
    Value actual{};
    if(value)binding.add(map,field,key,&*value);
    else if(binding.find(map,field,key,&actual)&&!world_progress.remove(map,field,key))return false;
    const auto found=binding.find(map,field,key,&actual);
    return value?found&&actual==*value:!found;
}
static bool world_progress_plan_items(void* save,const WP::Snapshot& current,const WP::Snapshot& desired,
                                      std::vector<WorldProgressEdit<void*>>& edits){
    auto before=current.items.begin(),after=desired.items.begin();
    const auto map=world_progress.items;
    const auto item_type=world_item_type();
    const auto& binding=medic_runtime.save;
    if(!item_type)return false;
    std::vector<std::pair<Weak,void*>> assets;
    while(before!=current.items.end()||after!=desired.items.end()){
        const bool has_before=before!=current.items.end()&&(after==desired.items.end()||before->key<=after->key);
        const bool has_after=after!=desired.items.end()&&(before==current.items.end()||after->key<=before->key);
        const auto key=has_before?before->key:after->key;
        if(WP::personal(key))return false;
        if(has_before&&has_after&&before->path==after->path){
            ++before;
            ++after;
            continue;
        }
        WorldProgressEdit<void*> edit{map,{},{},{}};
        std::memcpy(edit.key.data(),key.data(),sizeof(key));
        if(has_before){
            void* object{};
            if(!binding.find(map.address(save),map.field,key.data(),&object))return false;
            edit.before=object;
            ++before;
        }
        if(has_after){
            void* object{};
            if(!after->path.empty()){
                object=load_world_asset(after->path,UINT32_MAX,false,nullptr);
                Weak key{};
                if(!object||!api.isa(object,item_type)||!makeweak(key,object))return false;
                assets.emplace_back(key,object);
            }
            edit.after=object;
            ++after;
        }
        edits.push_back(std::move(edit));
    }
    return std::ranges::all_of(assets,[](const auto& asset){return api.weakget(&asset.first)==asset.second;});
}
static bool world_progress_apply(void* save,const WP::Snapshot& desired,bool authoritative,bool staging=false){
    WP::Snapshot current;
    if(!world_progress_capture(save,current))return false;
    if(current.actors==desired.actors&&current.names==desired.names&&current.items==desired.items)return true;
    const auto guid=[](const WP::Guid& key,auto& output){
        if(WP::personal(key))return false;
        std::memcpy(output.data(),key.data(),sizeof(key));
        return true;
    };
    const auto name=[](std::string_view key,auto& output){
        AnimU::NameKey value{};
        if(WP::personal(key)||!world_progress_name(key,value))return false;
        std::memcpy(output.data(),&value,sizeof(value));
        return true;
    };
    std::vector<WorldProgressEdit<std::int32_t>> edits;
    std::vector<WorldProgressEdit<void*>> item_edits;
    if(!world_progress_plan(current.actors,desired.actors,world_progress.actors,std::less<>{},guid,edits)||
       !world_progress_plan(current.names,desired.names,medic_runtime.save.markers,WP::name_less,name,edits)||
       !world_progress_plan_items(save,current,desired,item_edits))return false;
    const auto apply=[&](const auto& plan,std::size_t& attempted){
        for(const auto& edit:plan){
            ++attempted;
            if(!world_progress_put(save,edit,edit.after))return false;
        }
        return true;
    };
    const auto undo=[&](const auto& plan,std::size_t attempted){
        while(attempted){
            const auto& edit=plan[--attempted];
            if(!world_progress_put(save,edit,edit.before))world_progress.fault=true;
        }
    };
    std::size_t int_attempted{},item_attempted{};
    if(!apply(edits,int_attempted)||!apply(item_edits,item_attempted)){
        if(!staging){
            undo(item_edits,item_attempted);
            undo(edits,int_attempted);
        }
        return false;
    }
    if(staging)return true;
    auto& state=world_progress;
    for(const auto& edit:edits){
        if(edit.map.field==state.actors.field){
            WP::Guid guid{};
            std::memcpy(guid.data(),edit.key.data(),sizeof(guid));
            world_actor_queue(guid);
        }else if(authoritative&&state.quests.size()<2*WP::max_entries){
            AnimU::NameKey name{};
            std::memcpy(&name,edit.key.data(),sizeof(name));
            if(std::ranges::none_of(state.quests,[&](auto key){return key.index==name.index&&key.number==name.number;}))
                state.quests.push_back(name);
        }
    }
    for(const auto& edit:item_edits){
        WP::Guid guid{};
        std::memcpy(guid.data(),edit.key.data(),sizeof(guid));
        world_actor_queue(guid);
    }
    if(authoritative&&(!edits.empty()||!item_edits.empty()))state.notify=true;
    world_progress_writes+=edits.size()+item_edits.size();
    return true;
}
static void world_progress_ack(WP::Revision revision,double time){
    auto& state=world_progress;
    if(time<state.next_ack)return;
    E::Packet packet{};
    if(WP::encode_ack(envelope,revision,packet)&&send(packet,1))state.next_ack=time+.15;
}
static void receive_world_progress(const ReceivedPacket& received,unsigned kind,double time){
    auto& state=world_progress;
    if(!automatic_limit||!active_peer_mask)return;
    if(host_role){
        WP::Revision ack{};
        if(kind==WP::ack_kind&&received.peer<state.peers.size()&&(active_peer_mask&(1u<<received.peer))&&
           WP::decode_ack(envelope,received.packet,ack)&&ack==state.published)state.peers[received.peer].ack=ack;
        return;
    }
    WP::Part part{};
    if(received.peer||kind!=WP::snapshot_kind||!WP::decode(envelope,received.packet,part))return;
    if(part.revision==state.applied){world_progress_ack(state.applied,time);return;}
    if(state.inbox.push(part))state.ready=false;
}
static void world_progress_host(void* save,void* pawn,double time){
    auto& state=world_progress;
    world_stream_observe(pawn,time);
    if(time>=state.next_capture&&!native_placed_item_save_pending()){
        state.next_capture=time+.5;
        WP::Snapshot snapshot;
        WP::Bytes payload;
        state.ready=world_progress_capture(save,snapshot);
        if(state.ready){
            world_mechanisms_capture(pawn,time,snapshot.mechanisms);
            world_stream_capture(pawn,time,snapshot);
            world_barriers_capture(pawn,time,snapshot);
            world_music_capture(save,time,snapshot.music);
            state.ready=WP::encode(snapshot,payload);
        }
        if(!state.ready)++world_progress_errors;
        else if((payload!=state.payload||state.force_revision)&&state.published.number<UINT64_MAX){
            native_placed_items_snapshot(snapshot.names);
            state.published={state.published.number+1,WP::digest(payload)};
            state.payload=std::move(payload);
            state.peers={};
            state.force_revision=false;
        }
    }
    if(!state.ready||state.payload.empty())return;
    const auto total=static_cast<unsigned>(state.payload.size());
    for(unsigned i=0;i<state.peers.size();++i){
        auto& peer=state.peers[i];
        if(!(active_peer_mask&(1u<<i))||peer.ack==state.published||time<peer.retry)continue;
        for(unsigned budget=0;budget<2&&peer.next<WP::count(total);++budget){
            const auto at=peer.next*WP::part_bytes;
            const auto payload=std::span(state.payload).subspan(at,std::min(WP::part_bytes,total-at));
            E::Packet packet{};
            if(!WP::encode(envelope,{state.published,peer.next,total,payload},packet)||!send(packet,1u<<i))break;
            ++peer.next;
            ++world_progress_sent;
        }
        if(peer.next==WP::count(total)){peer.next=0;peer.retry=time+.35;}
    }
}
static void world_progress_guest(void* save,double time){
    auto& state=world_progress;
    if(state.fault||!checkpoint_medic_idle())return;
    const auto revision=state.inbox.revision();
    if(revision.number>state.applied.number){
        if(!state.inbox.complete()||(revision==state.rejected&&time<state.next_apply))return;
        WP::Snapshot snapshot;
        if(!state.inbox.snapshot(snapshot)||!world_progress_apply(save,snapshot,true)){
            state.rejected=revision;
            state.next_apply=time+.5;
            ++world_progress_errors;
            return;
        }
        native_placed_items_snapshot(snapshot.names);
        state.authoritative=std::move(snapshot);
        state.applied=revision;
        state.rejected={};
        state.ready=true;
        state.next_apply=time+.5;
        ++world_progress_applied;
        world_progress_ack(revision,time);
    }else if(state.applied.number&&time>=state.next_apply){
        state.next_apply=time+.5;
        state.ready=world_progress_apply(save,state.authoritative,false);
        if(!state.ready)++world_progress_errors;
    }
}
static void world_progress_step(void* pawn,double time){
    const auto save=medic_active(medic_runtime.save,pawn);
    if(!active_peer_mask||!save||!world_progress_bind(save,time)){
        world_progress.ready=false;
        return;
    }
    native_placed_item_save_resume(save,time);
    if(host_role)world_progress_host(save,pawn,time);
    else if(!native_placed_item_save_pending())world_progress_guest(save,time);
}
static bool world_progress_bind_notifiers(double time){
    auto& state=world_progress;
    if(state.notifiers_bound&&world_input(world_quest_notify)&&world_input(world_quest_update))return true;
    if(time<state.next_notify_bind)return false;
    state.next_notify_bind=time+.25;
    if(!world_input(world_quest_notify)||!world_input(world_quest_update))scan_world_inputs(2048);
    const auto notify=world_input(world_quest_notify),update=world_input(world_quest_update);
    state.notifiers_bound=notify&&update&&AnimU::field<int>(notify,104)==18&&AnimU::field<int>(update,104)==36&&
        medic_params(world_quest_notify,0,{})&&medic_params(world_quest_update,8,{{"NSD",8,0}})&&
        medic_field_class(catalog_field(update,"NSD",8),"NameProperty");
    return state.notifiers_bound;
}
static void world_progress_notify(void* pawn,double time){
    auto& state=world_progress;
    const auto& binding=medic_runtime.save;
    const auto gi=api.weakget(&binding.gi);
    if(host_role||!world_progress_ready()||(!state.notify&&state.quests.empty())||!gi||
       !medic_active(binding,pawn)||!world_progress_bind_notifiers(time))return;
    if(state.notify){
        state.notify=false;
        api.event(gi,world_input(world_quest_notify),nullptr);
        ++world_state_notifies;
    }
    for(unsigned i=0;i<3&&!state.quests.empty()&&medic_active(binding,pawn);++i){
        auto name=state.quests.back();
        state.quests.pop_back();
        api.event(gi,world_input(world_quest_update),&name);
        ++world_quest_notifies;
    }
}
