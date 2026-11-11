#include "native_damage_tags.inc"
#include "native_hit_queries.inc"

struct NativeDamageBinding {
    Weak function;
    void *hit{},*input{},*applied{},*result{},*point_valid{},*type{};
    const unsigned char* code{};
    int health{-1};
};
struct NativeDamageProxyType {
    Weak type,reset,tag,data;
    NativeDamageBinding damage;
    int scratch{-1};
};
struct NativeDamageOutgoing {Hit::Header header;Hit::Bytes body;double next{},expires{};unsigned peer{};};
struct NativeDamageIncoming {Hit::Assembly assembly;double expires{};};
struct NativeDamagePeer {
    Weak proxy,damageable_world;
    std::uint32_t generation{};
    std::uint64_t serial{};
    double pose_received{},next_spawn{};
    double next_arm{},health_received{};
    void* dead_property{};
    int proxy_health{-1};
    bool armed{},standing{};
    Hit::ReplayWindow replay;
    std::vector<NativeDamageIncoming> incoming;
};
struct NativeHostDamage {
    void* frame{};
    void* actor{};
    unsigned index{};
    std::uint32_t generation{};
    float before{};
};
struct NativeDamageRuntime {
    NativeDamageBinding binding,player;
    NativeDamageProxyType proxy;
    std::array<NativeDamagePeer,Player::slots-1> peers;
    NativeDamagePeer host;
    std::array<NativeHostDamage,16> host_hits{};
    std::vector<NativeDamageOutgoing> outgoing;
    std::uint64_t serial{};
    double next_bind{},next_player{},next_proxy{},next_queries{},next_default{};
    unsigned query_cursor{};
};
static NativeDamageRuntime native_damage;
static std::atomic<KillExec> native_damage_original{};
static std::atomic<const unsigned char*> native_damage_entry{};
static void* volatile* native_damage_cell;
static std::atomic<KillExec> native_proxy_damage_original{};
static std::atomic<const unsigned char*> native_proxy_damage_entry{};
static void* volatile* native_proxy_damage_cell;
static std::atomic<KillExec> native_host_damage_original{};
static std::atomic<const unsigned char*> native_host_damage_tail{};
static void* volatile* native_host_damage_cell;

static bool damage_struct_property(void* field){
    if(!medic_field_class(field,"StructProperty")||AnimU::field<int>(field,56)!=1)return false;
    const auto vtable=AnimU::field<const std::uintptr_t*>(field,0);
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    return vtable&&vtable[23]==image+0x118e4b0&&vtable[30]==image+0x118e4c0&&vtable[31]==image+0x11914f0;
}
static void* damage_field(void* type,std::string_view name,int width,int at,std::string_view kind){
    const auto field=catalog_field(type,name,width);
    return medic_field_class(field,kind)&&*api.offset(field)==at?field:nullptr;
}
static bool damage_type_path(void* field,std::string_view expected){
    std::string path;
    return field&&catalog_path(AnimU::field<void*>(field,0x78),path)&&path==expected;
}
static bool damage_parameter(void* field){return field&&(AnimU::field<std::uint64_t>(field,64)&0x80u);}
#include "native_threat_proxy.inc"
#include "native_threat_runtime.inc"
static bool damage_layout(void* fn,NativeDamageBinding& binding){
    if(!fn||*api.parms(fn)!=590)return false;
    binding.hit=damage_field(fn,"Hit",136,0,"StructProperty");
    binding.input=damage_field(fn,"DamageToApply",216,136,"StructProperty");
    binding.applied=damage_field(fn,"AppliedDamage",216,360,"StructProperty");
    binding.result=damage_field(fn,"ReturnValue",1,589,"BoolProperty");
    binding.point_valid=damage_field(fn,"HitLocationIsValid",1,588,"BoolProperty");
    const auto source=damage_field(fn,"Applier",8,352,"ObjectProperty");
    const auto point=damage_field(fn,"HitLocation",12,576,"StructProperty");
    for(auto field:{binding.hit,binding.input,binding.applied,binding.result,binding.point_valid,source,point})
        if(!damage_parameter(field))return false;
    if(!damage_struct_property(binding.hit)||!damage_struct_property(binding.input)||!damage_struct_property(binding.applied)||
       !damage_type_path(binding.hit,"/Script/Engine.HitResult")||
       !damage_type_path(binding.input,"/Script/FBAttributes.FB_DamageAttributeSet")||
       !damage_type_path(point,"/Script/CoreUObject.Vector"))return false;
    binding.type=AnimU::field<void*>(binding.input,0x78);
    if(binding.type!=AnimU::field<void*>(binding.applied,0x78))return false;
    const auto tags=damage_field(binding.type,"DamageTags",32,8,"StructProperty");
    const auto data=damage_field(binding.type,"DataTags",80,40,"MapProperty");
    if(!data||!damage_type_path(tags,"/Script/GameplayTags.GameplayTagContainer")||
       !damage_field(binding.type,"DamageAmount",4,0,"FloatProperty")||
       !damage_type_path(damage_field(binding.type,"SourceGUID",16,200,"StructProperty"),"/Script/CoreUObject.Guid")||
       AnimU::field<std::array<int,6>>(data,0x88)!=std::array<int,6>{8,12,16,20,4,20}||AnimU::field<unsigned>(data,0xa0))return false;
    const auto key=AnimU::field<void*>(data,0x78),value=AnimU::field<void*>(data,0x80);
    const auto container=AnimU::field<void*>(tags,0x78);
    const auto tag_array=world_actor_array(container,"GameplayTags",8,"/Script/GameplayTags.GameplayTag");
    if(!medic_field_class(key,"StructProperty")||!medic_field_class(value,"FloatProperty")||
       !damage_type_path(key,"/Script/GameplayTags.GameplayTag")||*api.size(key)!=8||*api.size(value)!=4||
       !tag_array.field||tag_array.offset!=0)return false;
    binding.code=AnimU::field<const unsigned char*>(fn,96);
    return binding.code&&makeweak(binding.function,fn);
}
static bool damage_bind(void* fn){
    NativeDamageBinding binding{};
    if(!damage_layout(fn,binding)||AnimU::field<int>(fn,104)!=9543)return false;
    const auto hp=catalog_field(base_class,"HP",4);
    if(!hp||binding.code[0]!=0x4c||AnimU::field<std::uint32_t>(binding.code,1)!=9532||
       binding.code[9532]!=0x04||binding.code[9533]!=0x48||AnimU::field<void*>(binding.code,9534)!=binding.result)return false;
    binding.health=*api.offset(hp);
    native_damage.binding=binding;
    return true;
}
static bool damage_bind_player(void* fn){
    NativeDamageBinding binding{};
    const auto type=world_input(world_player_type);
    const auto hp=catalog_field(type,"Health",4);
    if(!hp||!damage_layout(fn,binding)||binding.type!=native_damage.binding.type||AnimU::field<int>(fn,104)!=3541)return false;
    binding.health=*api.offset(hp);
    native_damage.player=binding;
    return true;
}
static bool damage_capture(const void* source,Hit::Value& out){
    struct NamedData {AnimU::NameKey key;float value;};
    static_assert(sizeof(NamedData)==12);
    Hit::Value value{};
    value.amount=AnimU::field<float>(source,0);
    value.source=AnimU::field<std::array<std::uint32_t,4>>(source,200);
    const auto tags=AnimU::field<WorldArray>(source,8);
    if(!tags.valid(Hit::tag_limit))return false;
    const auto keys=static_cast<const AnimU::NameKey*>(tags.data);
    for(int i=0;i<tags.count;++i){
        std::string name;
        if(!AnimU::name_string(catalog_name_pool,keys[i],catalog_read,name))return false;
        value.tags.push_back(std::move(name));
    }
    std::vector<NamedData> data;
    if(!WP::read_sparse_map<NamedData,20>(reinterpret_cast<std::uintptr_t>(source)+40,catalog_read,data)||
       data.size()>Hit::tag_limit+1)return false;
    for(const auto& entry:data){
        std::string name;
        if(!AnimU::name_string(catalog_name_pool,entry.key,catalog_read,name))return false;
        if(name=="None"&&entry.value==0.f)continue;
        value.data.push_back({std::move(name),entry.value});
    }
    std::ranges::sort(value.tags,Hit::tag_less);
    std::ranges::sort(value.data,Hit::tag_less,&Hit::TagValue::name);
    if(!Hit::valid(value))return false;
    out=std::move(value);
    return true;
}
static bool damage_queue(unsigned index,void* locals,const NativeDamageBinding& binding,
                         std::uint32_t generation,std::uint64_t& serial,unsigned peer){
    auto& state=native_damage;
    if(!generation||state.outgoing.size()>=32||serial==UINT64_MAX)return false;
    Hit::Value damage{};
    Hit::Bytes bytes;
    if(!damage_capture(static_cast<unsigned char*>(locals)+136,damage)||!Hit::encode(damage,bytes)){
        ++damage_rejected;
        return false;
    }
    Hit::Header header{{++serial,index+1,entities[index].generation,generation},{},
                       Hit::digest(bytes),static_cast<unsigned>(bytes.size()),api.getbool(binding.point_valid,locals,0)};
    if(header.point_valid)header.point=AnimU::field<std::array<float,3>>(locals,576);
    if(!std::ranges::all_of(header.point,[](float value){return std::isfinite(value)&&std::abs(value)<=10000000.f;}))return false;
    state.outgoing.push_back({header,std::move(bytes),0,now()+5.,peer});
    ++damage_sent;
    return true;
}
static void damage_queue_guest(unsigned index,void* locals){
    const auto pawn=api.weakget(&automatic_context);
    if(pawn&&AnimU::field<void*>(locals,352)==pawn&&local_life.life.phase==LifePhase::standing)
        damage_queue(index,locals,native_damage.binding,local_life_pose.generation,native_damage.serial,0);
}
static void record_host_damage_entry(void* context,void* frame,void* locals){
    auto& records=native_damage.host_hits;
    auto selected=std::ranges::find(records,frame,&NativeHostDamage::frame);
    if(selected!=records.end())*selected={};
    const auto pawn=api.weakget(&automatic_context);
    if(!native_host_damage_cell||!context||!pawn||AnimU::field<void*>(locals,352)!=pawn)return;
    const auto health=native_damage.binding.health;
    if(health<0)return;
    const auto before=AnimU::field<float>(context,health);
    if(!std::isfinite(before)||before<=0)return;
    if(std::ranges::find(records,context,&NativeHostDamage::actor)!=records.end())return;
    if(selected==records.end()){
        selected=std::ranges::find(records,nullptr,&NativeHostDamage::frame);
    }
    if(selected==records.end())return;
    for(unsigned index=0;index<automatic_limit;++index){
        const auto& enemy=entities[index];
        if(enemy.used&&api.weakget(&enemy.actor)==context){
            *selected={frame,context,index,enemy.generation,before};
            return;
        }
    }
}
static void native_host_damage_observe(void* context,void* frame,void* result){
    const auto original=native_host_damage_original.load(std::memory_order_acquire);
    const auto expected=native_host_damage_tail.load(std::memory_order_relaxed);
    if(expected&&frame&&AnimU::field<const unsigned char*>(frame,32)==expected&&GetCurrentThreadId()==game_thread.load()&&
       native_host_damage_cell&&host_role&&automatic_limit&&active_peer_mask){
        const auto fn=api.weakget(&native_damage.binding.function);
        if(fn&&AnimU::field<void*>(frame,16)==fn){
            auto& records=native_damage.host_hits;
            const auto saved=std::ranges::find(records,frame,&NativeHostDamage::frame);
            if(saved!=records.end()&&saved->actor==context){
                const auto sample=*saved;
                *saved={};
                original(context,frame,result);
                const auto& entity=entities[sample.index];
                if(entity.generation!=sample.generation)return;
                const auto actor=api.weakget(&entity.actor);
                const auto after=actor?std::max(0.f,AnimU::field<float>(actor,native_damage.binding.health)):0.f;
                if(after<sample.before){
                    native_threat_record(sample.index,0,sample.before-after,now());
                    ++threat_host_hits;
                }
                return;
            }
        }
    }
    original(context,frame,result);
}
static void native_damage_observe(void* context,void* frame,void* result){
    const auto original=native_damage_original.load(std::memory_order_acquire);
    const auto expected=native_damage_entry.load(std::memory_order_relaxed);
    if(expected&&frame&&AnimU::field<const unsigned char*>(frame,32)==expected&&
       GetCurrentThreadId()==game_thread.load()&&native_damage_cell&&automatic_limit&&active_peer_mask){
        const auto& binding=native_damage.binding;
        const auto function=api.weakget(&binding.function);
        const auto locals=AnimU::field<void*>(frame,40);
        if(function&&expected==binding.code+1&&AnimU::field<void*>(frame,16)==function&&locals){
            if(host_role){
                record_host_damage_entry(context,frame,locals);
                original(context,frame,result);
                return;
            }
            for(unsigned i=0;i<E::capacity;++i){
                const auto& entry=enrollments[i];
                if(!entry.factory_ready||entry.retiring||!ownership[i].active||deaths[i].terminal||
                   api.weakget(&entry.replica)!=context)continue;
                damage_queue_guest(i,locals);
                api.setbool(binding.result,locals,false,0);
                const auto terminal=binding.code+9532;
                std::memcpy(static_cast<unsigned char*>(frame)+32,&terminal,sizeof(terminal));
                ++damage_suppressed;
                return;
            }
        }
    }
    original(context,frame,result);
}
static bool install_damage_slot(unsigned opcode,std::uintptr_t rva,KillExec observer,
                                std::atomic<KillExec>& saved,void* volatile*& hooked){
    if(hooked)return true;
    if(build_state.load()!=1)return false;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto slot=image+0x4a62e10+opcode*sizeof(void*);
    const auto original=reinterpret_cast<void*>(image+rva);
    auto cell=reinterpret_cast<void* volatile*>(slot);
    MEMORY_BASIC_INFORMATION page{};
    if(*cell!=original||!VirtualQuery(reinterpret_cast<const void*>(slot),&page,sizeof(page))||
       page.State!=MEM_COMMIT||!(page.Protect&PAGE_READWRITE)||(page.Protect&PAGE_GUARD)||
       reinterpret_cast<std::uintptr_t>(page.BaseAddress)+page.RegionSize-slot<sizeof(void*))return false;
    saved.store(reinterpret_cast<KillExec>(original),std::memory_order_release);
    if(InterlockedCompareExchangePointer(cell,reinterpret_cast<void*>(observer),original)!=original)return false;
    hooked=cell;
    return true;
}
static bool native_damage_install(){
    if(!install_damage_slot(0x4c,0x11b7960,&native_damage_observe,native_damage_original,native_damage_cell))return false;
    const auto entry=native_damage.binding.code+1;
    if(native_damage_entry.load(std::memory_order_relaxed)!=entry)
        native_damage_entry.store(entry,std::memory_order_relaxed);
    return true;
}
static void native_proxy_damage_observe(void* context,void* frame,void* result);
static bool restore_damage_slot(KillExec observer,const std::atomic<KillExec>& saved,void* volatile*& slot){
    if(!slot)return true;
    const auto original=reinterpret_cast<void*>(saved.load(std::memory_order_acquire));
    const auto hook=reinterpret_cast<void*>(observer);
    const auto prior=InterlockedCompareExchangePointer(slot,original,hook);
    slot=nullptr;
    return prior==original||prior==hook;
}
#include "native_threat_memory.inc"
static bool native_damage_cleanup(){
    bool clean=native_threat_cleanup();
    if(!host_role)for(unsigned i=0;i<E::capacity;++i){
        auto& entry=enrollments[i];
        if(entry.hit_queries&&!disable_native_hit_queries(i,api.weakget(&entry.replica)))clean=false;
    }
    native_damage_entry.store(nullptr,std::memory_order_relaxed);
    native_proxy_damage_entry.store(nullptr,std::memory_order_relaxed);
    native_host_damage_tail.store(nullptr,std::memory_order_relaxed);
    if(!restore_damage_slot(&native_damage_observe,native_damage_original,native_damage_cell))clean=false;
    if(!restore_damage_slot(&native_proxy_damage_observe,native_proxy_damage_original,native_proxy_damage_cell))clean=false;
    if(!restore_damage_slot(&native_host_damage_observe,native_host_damage_original,native_host_damage_cell))clean=false;
    for(auto& peer:native_damage.peers)if(!destroy_damage_proxy(peer))clean=false;
    return clean;
}
static void native_damage_reset(){native_damage={};}

static void* declared_script_method(void* type,std::string_view name,unsigned parms,int script){
    const auto fn_type=api.weakget(&catalog_classes[catalog_function]);
    auto fn=AnimU::field<void*>(type,72);
    for(unsigned i=0;fn&&i<1024;++i,fn=AnimU::field<void*>(fn,40)){
        if(AnimU::field<void*>(fn,16)!=fn_type||AnimU::field<void*>(fn,32)!=type)continue;
        std::string path;
        if(catalog_path(fn,path)&&catalog_leaf(path)==name&&*api.parms(fn)==parms&&AnimU::field<int>(fn,104)==script)return fn;
    }
    return nullptr;
}
static bool damage_bind_proxy(double time){
    auto& state=native_damage;
    if(api.weakget(&state.proxy.type)&&api.weakget(&state.proxy.reset)&&api.weakget(&state.proxy.tag)&&
       api.weakget(&state.proxy.data)&&api.weakget(&state.proxy.damage.function))return true;
    if(time<state.next_proxy)return false;
    state.next_proxy=time+1.;
    const auto base=world_input(world_player_type);
    auto type=world_input(world_damage_proxy_class);
    if(!type&&!world_input(world_load_class))scan_world_inputs(2048);
    if(!type)type=load_world_asset(world_paths[world_damage_proxy_class],catalog_blueprint,true,base);
    if(!type||!base||!catalog_subclass(type,base))return false;
    const auto reset=declared_script_method(type,"ResetDamage",0,30),tag=declared_script_method(type,"AddDamageTag",8,77);
    const auto data=declared_script_method(type,"AddDamageData",12,58);
    const auto hit=declared_script_method(type,"TryApplyDamage",590,3);
    const auto scratch=catalog_field(type,"DamageScratch",216);
    if(!scratch||!damage_struct_property(scratch)||AnimU::field<void*>(scratch,0x78)!=state.binding.type||
       !tag||!data||!reset||factory_parameter(tag,"Tag",8)!=0||factory_parameter(data,"Tag",8)!=0||
       factory_parameter(data,"Value",4)!=8)return false;
    NativeDamageProxyType binding{};
    if(!makeweak(binding.type,type)||!makeweak(binding.reset,reset)||!makeweak(binding.tag,tag)||!makeweak(binding.data,data)||
       !damage_layout(hit,binding.damage)||binding.damage.type!=state.binding.type||
       binding.damage.code[0]!=0x04||binding.damage.code[1]!=0x27||binding.damage.code[2]!=0x53)return false;
    binding.scratch=*api.offset(scratch);
    state.proxy=binding;
    return true;
}
static void* damage_proxy(unsigned peer,void* pawn,double time){
    auto& state=native_damage.peers[peer];
    const auto& sender=life_peers[peer+1];
    auto actor=api.weakget(&state.proxy);
    if(!actor){
        if(time<state.next_spawn)return nullptr;
        state.next_spawn=time+.5;
        state.armed=false;
        state.standing=false;
        state.damageable_world={};
        state.next_arm=state.health_received=0;
        actor=factory_spawn(pawn,api.weakget(&native_damage.proxy.type),sender.position,state.proxy,true);
        if(!actor){destroy_owned_actor(state.proxy,false);return nullptr;}
        own_set(actor,world_input(world_actor_tick),false);
        own_set(actor,factory_object(factory_collision),false);
        if(const auto move=catalog_object_field(actor,"CharacterMovement"))own_set(move,world_input(world_component_tick),false);
        if(const auto mesh=catalog_object_field(actor,"Mesh"))own_set(mesh,world_input(world_component_tick),false);
        ++damage_proxies;
        state.pose_received=0;
    }
    if(state.pose_received!=sender.received){
        alignas(16) FactoryParameters transform{};
        factory_put(transform,0,sender.position);
        if(const auto sample=remote_players[peer+1].timeline.latest())
            factory_put(transform,12,std::array<float,3>{sample->v[9],sample->v[10],sample->v[11]});
        transform[164]=1;
        api.event(actor,actor_fn,transform.data());
        state.pose_received=sender.received;
    }
    return actor;
}
static unsigned damage_source_enemy(void* source,unsigned peer){
    std::array<void*,16> chain{source};
    unsigned size=1;
    for(unsigned i=0;i<size&&chain[i];++i){
        const auto object=chain[i];
        for(unsigned index=0;index<automatic_limit;++index){
            const auto& entity=entities[index];
            if(entity.used&&!enrollments[index].retiring&&(enrollment_peer_mask(index)&(1u<<peer))&&
               api.weakget(&entity.actor)==object)return index;
        }
        const auto append=[&](void* candidate){
            const auto end=chain.begin()+size;
            if(candidate&&size<chain.size()&&std::find(chain.begin(),end,candidate)==end)
                chain[size++]=candidate;
        };
        append(AnimU::field<void*>(object,32));
        if(api.isa(object,actor_class)){
            append(catalog_object_field(object,"Owner"));
            append(catalog_object_field(object,"Instigator"));
        }
    }
    return E::capacity;
}
static void native_proxy_damage_observe(void* context,void* frame,void* result){
    const auto original=native_proxy_damage_original.load(std::memory_order_acquire);
    const auto expected=native_proxy_damage_entry.load(std::memory_order_relaxed);
    if(expected&&frame&&AnimU::field<const unsigned char*>(frame,32)==expected&&
       GetCurrentThreadId()==game_thread.load()&&native_proxy_damage_cell&&host_role&&automatic_limit&&active_peer_mask){
        auto& state=native_damage;
        const auto& binding=state.proxy.damage;
        const auto fn=api.weakget(&binding.function);
        const auto locals=AnimU::field<void*>(frame,40);
        if(fn&&expected==binding.code+2&&AnimU::field<void*>(frame,16)==fn&&locals){
            for(unsigned peer=0;peer<state.peers.size();++peer){
                auto& target=state.peers[peer];
                if(!(active_peer_mask&(1u<<peer))||api.weakget(&target.proxy)!=context)continue;
                const auto& remote=life_peers[peer+1];
                if(target.generation!=remote.generation||!fresh_life(remote,LifePhase::standing,now()))break;
                const auto index=damage_source_enemy(AnimU::field<void*>(locals,352),peer);
                if(index<E::capacity)damage_queue(index,locals,binding,target.generation,target.serial,peer);
                break;
            }
        }
    }
    original(context,frame,result);
}
static void* damage_build(void* proxy,const Hit::Value& damage){
    const auto& binding=native_damage.proxy;
    const auto reset=api.weakget(&binding.reset),tag=api.weakget(&binding.tag),data=api.weakget(&binding.data);
    if(!reset||!tag||!data||binding.scratch<0)return nullptr;
    api.event(proxy,reset,nullptr);
    auto scratch=static_cast<unsigned char*>(proxy)+binding.scratch;
    std::memcpy(scratch,&damage.amount,sizeof(damage.amount));
    std::memcpy(scratch+200,damage.source.data(),sizeof(damage.source));
    for(const auto& name:damage.tags){
        const auto tag_name=stock_damage_tag(name);
        auto key=medic_name(std::wstring(tag_name.begin(),tag_name.end()));
        api.event(proxy,tag,&key);
    }
    for(const auto& entry:damage.data){
        struct Data {AnimU::NameKey key;float value;};
        const auto tag_name=stock_damage_tag(entry.name);
        Data args{medic_name(std::wstring(tag_name.begin(),tag_name.end())),entry.value};
        api.event(proxy,data,&args);
    }
    return scratch;
}
enum class DamageReplay { retry, rejected, applied };
static bool registered_damage(const Hit::Value& damage){
    const auto registered=[](std::string_view tag){return !stock_damage_tag(tag).empty();};
    return std::ranges::all_of(damage.tags,registered)&&std::ranges::all_of(damage.data,registered,&Hit::TagValue::name);
}
static bool stock_damage_event(const NativeDamageBinding& binding,Weak& target_key,void* source,const void* scratch,
                               const std::array<float,3>& point,bool point_valid){
    const auto target=api.weakget(&target_key),fn=api.weakget(&binding.function);
    if(!target||!fn||binding.health<0)return false;
    const auto before=AnimU::field<float>(target,binding.health);
    if(!std::isfinite(before)||before<=0)return false;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto init=reinterpret_cast<void(*)(void*,void*)>(image+0x11914f0);
    const auto destroy=reinterpret_cast<void(*)(void*,void*)>(image+0x118e4c0);
    const auto copy=reinterpret_cast<void(*)(void*,void*,const void*,int)>(image+0x118e4b0);
    alignas(16) std::array<unsigned char,592> args{};
    init(binding.hit,args.data());
    init(binding.input,args.data()+136);
    init(binding.applied,args.data()+360);
    copy(binding.input,args.data()+136,scratch,1);
    std::memcpy(args.data()+352,&source,sizeof(source));
    std::memcpy(args.data()+576,point.data(),sizeof(point));
    api.setbool(binding.point_valid,args.data(),point_valid,0);
    api.event(target,fn,args.data());
    const auto observed=api.weakget(&target_key);
    const bool applied=!observed||AnimU::field<float>(observed,binding.health)<before;
    destroy(binding.applied,args.data()+360);
    destroy(binding.input,args.data()+136);
    destroy(binding.hit,args.data());
    return applied;
}
static DamageReplay damage_replay(const Hit::Header& header,const Hit::Value& damage,unsigned peer,double time){
    const auto& key=header.key;
    const auto& sender=life_peers[peer+1];
    const auto index=key.enemy-1;
    const auto& entry=enrollments[index];
    auto& entity=entities[index];
    if(!registered_damage(damage)||key.source_generation!=sender.generation||!fresh_life(sender,LifePhase::standing,time)||
       !entity.used||entity.generation!=key.generation||entry.offer.id!=key.enemy||entry.retiring||deaths[index].terminal||
       !(enrollment_peer_mask(index)&(1u<<peer)))return DamageReplay::rejected;
    const auto pawn=api.weakget(&automatic_context),target=api.weakget(&entity.actor);
    const auto& binding=native_damage.binding;
    const auto fn=api.weakget(&binding.function);
    if(!pawn||!fn||binding.health<0)return DamageReplay::retry;
    if(!target)return DamageReplay::rejected;
    alignas(16) FactoryParameters location{};
    api.event(target,getters[0].fn,location.data());
    const auto target_at=AnimU::field<std::array<float,3>>(location.data(),getters[0].at);
    float distance{};
    for(unsigned i=0;i<3;++i){const auto delta=target_at[i]-sender.position[i];distance+=delta*delta;}
    const auto before=AnimU::field<float>(target,binding.health);
    if(!std::isfinite(distance)||distance>8000.f*8000.f||!std::isfinite(before)||before<=0)return DamageReplay::rejected;
    const auto proxy=damage_bind_proxy(time)?damage_proxy(peer,pawn,time):nullptr;
    const auto scratch=proxy?damage_build(proxy,damage):nullptr;
    if(!scratch)return DamageReplay::retry;
    float deviation{};
    for(unsigned i=0;i<3;++i){const auto delta=header.point[i]-target_at[i];deviation+=delta*delta;}
    const auto point=header.point_valid&&deviation<=1000.f*1000.f?header.point:target_at;
    const bool applied=stock_damage_event(binding,entity.actor,proxy,scratch,point,true);
    if(applied){
        const auto observed=api.weakget(&entity.actor);
        const auto after=observed?std::max(0.f,AnimU::field<float>(observed,binding.health)):0.f;
        native_threat_record(index,peer+1,before-after,time);
    }
    if(const auto reset=api.weakget(&native_damage.proxy.reset);reset&&api.weakget(&native_damage.peers[peer].proxy)==proxy)
        api.event(proxy,reset,nullptr);
    return applied?DamageReplay::applied:DamageReplay::rejected;
}
static DamageReplay damage_replay_player(const Hit::Header& header,const Hit::Value& damage,double time){
    enemy_damage_replay_stage=1;
    const auto& key=header.key;
    if(!registered_damage(damage)||key.source_generation!=local_life_pose.generation||local_life.life.phase!=LifePhase::standing)
        return DamageReplay::rejected;
    const auto index=key.enemy-1;
    const auto& entry=enrollments[index];
    const auto& entity=entities[index];
    if(entry.retiring||retired_generation(index,key.generation)||(entity.used&&entity.generation>key.generation))return DamageReplay::rejected;
    if(!entry.confirmed||!entity.used||entity.generation!=key.generation)return DamageReplay::retry;
    enemy_damage_replay_stage=2;
    const auto applier=api.weakget(&entity.actor),pawn=api.weakget(&automatic_context);
    if(!applier||!pawn||!api.weakget(&native_damage.player.function)||!damage_bind_proxy(time))return DamageReplay::retry;
    enemy_damage_replay_stage=3;
    auto defaults=world_input(world_damage_proxy_default);
    if(!defaults&&time>=native_damage.next_default){
        native_damage.next_default=time+.5;
        defaults=load_world_asset(world_paths[world_damage_proxy_default],UINT32_MAX,false,nullptr);
        if(defaults)makeweak(world_inputs[world_damage_proxy_default],defaults);
    }
    if(!defaults||!api.isa(defaults,api.weakget(&native_damage.proxy.type))||!(AnimU::field<unsigned>(defaults,8)&0x10u))return DamageReplay::retry;
    enemy_damage_replay_stage=4;
    const auto scratch=damage_build(defaults,damage);
    if(!scratch)return DamageReplay::retry;
    const auto target_at=native_actor_location(pawn);
    float deviation{};
    for(unsigned i=0;i<3;++i){const auto delta=header.point[i]-target_at[i];deviation+=delta*delta;}
    const bool point_valid=header.point_valid&&deviation<=1000.f*1000.f;
    const bool applied=stock_damage_event(native_damage.player,automatic_context,applier,scratch,point_valid?header.point:target_at,point_valid);
    if(applied)enemy_damage_replay_stage=5;
    if(const auto reset=api.weakget(&native_damage.proxy.reset))api.event(defaults,reset,nullptr);
    return applied?DamageReplay::applied:DamageReplay::rejected;
}
static void damage_send_ack(const Hit::Key& key,unsigned peer){
    E::Packet packet{};
    if(Hit::encode_ack(envelope,key,packet))send(packet,1u<<peer);
}
static bool damage_peer_generation(NativeDamagePeer& source,std::uint32_t generation){
    if(source.generation==generation)return true;
    if(host_role)native_threat_release_proxy(api.weakget(&source.proxy));
    if(!destroy_damage_proxy(source))return false;
    source={};
    source.generation=generation;
    return true;
}
static void receive_native_damage(const ReceivedPacket& received,unsigned kind,double time){
    auto& state=native_damage;
    const auto index=received.peer;
    if(!automatic_limit||!active_peer_mask||index>=state.peers.size()||!(active_peer_mask&(1u<<index))||(!host_role&&index))return;
    if(kind==Hit::ack_kind){
        Hit::Key key{};
        if(Hit::decode_ack(envelope,received.packet,key)){
            const auto removed=std::erase_if(state.outgoing,[&](const auto& item){return item.peer==index&&item.header.key==key;});
            damage_acked+=removed;
        }
        return;
    }
    Hit::Part part{};
    const auto generation=host_role?life_peers[index+1].generation:local_life_pose.generation;
    if(kind!=Hit::hit_kind||!Hit::decode(envelope,received.packet,part)||part.header.key.source_generation!=generation)return;
    auto& peer=host_role?state.peers[index]:state.host;
    const auto& key=part.header.key;
    if(!damage_peer_generation(peer,generation))return;
    if(peer.replay.known(key.serial)){
        ++damage_duplicates;
        damage_send_ack(key,index);
        return;
    }
    auto it=std::ranges::find(peer.incoming,key.serial,[](const auto& item){return item.assembly.header.key.serial;});
    if(it==peer.incoming.end()){
        if(peer.incoming.size()>=8)return;
        peer.incoming.push_back({{part.header,{},0},time+5.});
        it=peer.incoming.end()-1;
    }
    if(!it->assembly.push(part))++damage_rejected;
}
static void step_incoming_damage(NativeDamagePeer& source,unsigned peer,double time){
    std::erase_if(source.incoming,[&](auto& item){
        if(time>=item.expires)return true;
        if(!item.assembly.complete())return false;
        const auto& header=item.assembly.header;
        if(source.replay.known(header.key.serial)){
            ++damage_duplicates;
            damage_send_ack(header.key,peer);
            return true;
        }
        Hit::Value damage{};
        auto result=DamageReplay::rejected;
        if(item.assembly.value(damage))result=host_role?damage_replay(header,damage,peer,time):damage_replay_player(header,damage,time);
        if(result==DamageReplay::retry)return false;
        if(result==DamageReplay::applied)++damage_applied;else ++damage_rejected;
        source.replay.accept(header.key.serial);
        damage_send_ack(header.key,peer);
        return true;
    });
}
static void native_damage_step(void* pawn,double time){
    auto& state=native_damage;
    if(!active_peer_mask)return;
    if(!api.weakget(&state.binding.function)&&time>=state.next_bind){
        state.next_bind=time+.25;
        if(!world_input(world_damage_enemy))scan_world_inputs(2048);
        damage_bind(world_input(world_damage_enemy));
    }
    if(!api.weakget(&state.binding.function))return;
    if(host_role){
        // A Blueprint damage call cannot yield across world ticks. Discard
        // observations whose stock function exited before the verified return.
        state.host_hits={};
        if(install_damage_slot(0x48,0x11b6f10,&native_host_damage_observe,native_host_damage_original,native_host_damage_cell)&&
           native_damage_install()){
            const auto tail=state.binding.code+9534;
            if(native_host_damage_tail.load(std::memory_order_relaxed)!=tail)native_host_damage_tail.store(tail,std::memory_order_relaxed);
        }
        const bool proxy_ready=damage_bind_proxy(time)&&install_damage_slot(0x27,0x11b9d20,&native_proxy_damage_observe,
                                                                           native_proxy_damage_original,native_proxy_damage_cell);
        if(proxy_ready){
            const auto entry=state.proxy.damage.code+2;
            if(native_proxy_damage_entry.load(std::memory_order_relaxed)!=entry)
                native_proxy_damage_entry.store(entry,std::memory_order_relaxed);
        }
        for(unsigned peer=0;peer<state.peers.size();++peer){
            if(!(active_peer_mask&(1u<<peer)))continue;
            auto& source=state.peers[peer];
            const auto& remote=life_peers[peer+1];
            if(!damage_peer_generation(source,remote.generation))continue;
            if(proxy_ready&&source.pose_received!=remote.received&&fresh_life(remote,LifePhase::standing,time))damage_proxy(peer,pawn,time);
            update_threat_proxy(source,remote,time);
            if(!source.incoming.empty())step_incoming_damage(source,peer,time);
        }
        native_threat_step(pawn,time);
    }else{
        if(!native_damage_install())return;
        if(!api.weakget(&state.player.function)&&time>=state.next_player){
            state.next_player=time+.25;
            if(!world_input(world_damage_player))scan_world_inputs(2048);
            damage_bind_player(world_input(world_damage_player));
        }
        if(!state.host.incoming.empty())step_incoming_damage(state.host,0,time);
        if(time>=state.next_queries){
            state.next_queries=time+.1;
            if(!world_input(world_hit_profile))scan_world_inputs(2048);
            unsigned attempted{};
            for(unsigned i=0;i<automatic_limit&&attempted<2;++i){
                const auto index=state.query_cursor;
                state.query_cursor=(index+1)%automatic_limit;
                auto& entry=enrollments[index];
                if(!entry.factory_ready||!entry.confirmed||entry.retiring||entry.hit_queries||!ownership[index].active||
                   deaths[index].terminal||!entities[index].used)continue;
                ++attempted;
                entry.hit_queries=enable_native_hit_queries(entry);
            }
        }
    }
    std::erase_if(state.outgoing,[&](const auto& item){
        const auto generation=host_role?life_peers[item.peer+1].generation:local_life_pose.generation;
        return time>=item.expires||item.header.key.source_generation!=generation;
    });
    for(auto& item:state.outgoing){
        if(time<item.next)continue;
        item.next=time+.18;
        for(unsigned i=0;i<Hit::count(item.header.bytes);++i){
            const auto at=i*Hit::part_size;
            const auto part=std::span(item.body).subspan(at,std::min(Hit::part_size,item.header.bytes-at));
            E::Packet packet{};
            if(!Hit::encode(envelope,{item.header,i,part},packet)||!send(packet,1u<<item.peer))break;
        }
    }
}
