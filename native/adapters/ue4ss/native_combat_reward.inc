// Native stock Kill/AddXP and TrySpawningItem on the verified Steam image.
struct NativeRewardSite {
    Weak function;
    void* node{};
    unsigned char* code{};
    unsigned char opcode{};
    KillExec stock{};
};
struct NativeCombatRewards {
    NativeRewardSite xp,drop;
    void* volatile* dispatch{};
    KillExec undefined{};
    const unsigned char* drop_outputs{};
    int xp_target{-1},xp_amount{-1};
    std::array<int,2> drop_result{};
    std::array<void*,6> drop_flags{};
    E::Envelope scope{};
    std::array<RW::Outbox,3> outboxes{};
    std::array<std::deque<RW::Event>,3> backlog{};
    std::vector<Weak> treasure_lizards;
    RW::Inbox inbox;
    bool applying{};
    double next_bind{},next_apply{};
};
static NativeCombatRewards combat_rewards;
static constexpr unsigned char reward_opcode=0x03;
static constexpr unsigned drop_flag_offsets[]{136,152,153,154,184,185};
static constexpr const char* drop_flag_names[]{
    "Disable Snap To Nav","Disable Bounce","DisableSnapToSurface","DropWithGravity","skip drop save","SaveLocOverride"
};
static std::uint64_t combat_host_xp{},combat_guest_xp{},combat_drop_offers{},combat_drop_rolls{},
    combat_drop_items{},combat_drop_suppressed{},combat_reward_rejected{};
static unsigned combat_reward_bind_stage{};

static std::string native_reward_stats(){
    return ",\"combat_reward_bind_stage\":"+std::to_string(combat_reward_bind_stage)+
           ",\"combat_host_xp\":"+std::to_string(combat_host_xp)+
           ",\"combat_guest_xp\":"+std::to_string(combat_guest_xp)+
           ",\"combat_drop_offers\":"+std::to_string(combat_drop_offers)+
           ",\"combat_drop_rolls\":"+std::to_string(combat_drop_rolls)+
           ",\"combat_drop_items\":"+std::to_string(combat_drop_items)+
           ",\"combat_drop_suppressed\":"+std::to_string(combat_drop_suppressed)+
           ",\"combat_reward_rejected\":"+std::to_string(combat_reward_rejected);
}

static bool reward_writeable(const void* p,unsigned bytes){
    MEMORY_BASIC_INFORMATION page{};
    const auto start=reinterpret_cast<std::uintptr_t>(p);
    return p&&VirtualQuery(p,&page,sizeof(page))&&page.State==MEM_COMMIT&&
        (page.Protect&PAGE_READWRITE)&&!(page.Protect&PAGE_GUARD)&&
        reinterpret_cast<std::uintptr_t>(page.BaseAddress)+page.RegionSize-start>=bytes;
}
static void native_reward_observe(void* context,void* frame,void* result);
static KillExec native_reward_original(unsigned opcode){
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto entry=reinterpret_cast<KillExec*>(image+0x4a62e10)[opcode];
    if(opcode==0x4c&&native_damage_cell&&entry==&native_damage_observe)
        return native_damage_original.load(std::memory_order_acquire);
    return entry;
}
static bool native_reward_dispatch(){
    auto& state=combat_rewards;
    if(state.dispatch)return true;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto natives=reinterpret_cast<void**>(image+0x4a62e10);
    const auto stock=natives[reward_opcode];
    if(!stock||stock!=natives[0x05]||stock!=natives[0x08]||
       native_reward_original(0x4c)!=reinterpret_cast<KillExec>(image+0x11b7960)||
       !reward_writeable(natives+reward_opcode,8))return false;
    state.undefined=reinterpret_cast<KillExec>(stock);
    if(InterlockedCompareExchangePointer(natives+reward_opcode,reinterpret_cast<void*>(&native_reward_observe),stock)!=stock)
        return false;
    state.dispatch=natives+reward_opcode;
    return true;
}
static bool reward_install_site(NativeRewardSite& site,void* function,unsigned char* code,unsigned char opcode){
    if(!function||!reward_writeable(code,1)||*code!=opcode)return false;
    NativeRewardSite found{};
    if(!makeweak(found.function,function))return false;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    found.stock=native_reward_original(opcode);
    MEMORY_BASIC_INFORMATION page{};
    if(!found.stock||!VirtualQuery(reinterpret_cast<const void*>(found.stock),&page,sizeof(page))||
       page.AllocationBase!=reinterpret_cast<void*>(image)||(page.Protect&0xf0u)==0||
       !native_reward_dispatch())return false;
    found.node=function;found.code=code;found.opcode=opcode;
    site=found;
    *code=reward_opcode;
    return true;
}
static void reward_restore_site(NativeRewardSite& site){
    if(api.weakget(&site.function)&&reward_writeable(site.code,1)&&*site.code==reward_opcode)
        *site.code=site.opcode;
    site={};
}
static void native_reward_unbind(){
    auto& state=combat_rewards;
    reward_restore_site(state.xp);
    reward_restore_site(state.drop);
    if(state.dispatch)InterlockedCompareExchangePointer(state.dispatch,reinterpret_cast<void*>(state.undefined),
                                                         reinterpret_cast<void*>(&native_reward_observe));
    state.dispatch=nullptr;
    state.undefined=nullptr;
    state.next_bind=0;
    state.treasure_lizards.clear();
    combat_reward_bind_stage=0;
}
static void native_reward_sync_scope(){
    auto& state=combat_rewards;
    if(state.scope.room_high==envelope.room_high&&state.scope.room_low==envelope.room_low&&
       state.scope.world==envelope.world)return;
    for(unsigned i=0;i<state.outboxes.size();++i){
        state.outboxes[i]={};
        state.backlog[i].clear();
    }
    state.inbox={};
    state.next_apply=0;
    state.scope=envelope;
}
static bool native_reward_bind_xp(){
    auto& state=combat_rewards;
    if(state.xp.code&&api.weakget(&state.xp.function))return true;
    const auto fn=world_input(world_kill);
    const auto code=fn?AnimU::field<unsigned char*>(fn,96):nullptr;
    if(!code||*api.parms(fn)!=2||AnimU::field<int>(fn,104)!=1411)return false;
    const auto target=catalog_field(base_class,"TargetPlayer",8);
    const auto xp=catalog_field(fn,"CallFunc_GetXPReward_ReturnValue",4);
    if(!medic_field_class(target,"ObjectProperty")||!medic_field_class(xp,"IntProperty"))return false;
    for(unsigned i=1018;i+46<1101;++i){
        auto site=code+i;
        if(site[0]!=0x19||site[1]!=0x01||AnimU::field<void*>(site,2)!=target||
           AnimU::field<std::uint32_t>(site,10)!=24||AnimU::field<void*>(site,14)||site[22]!=0x45||
           site[35]!=0x00||AnimU::field<void*>(site,36)!=xp||site[44]!=0x28||site[45]!=0x16)continue;
        std::string name;
        const AnimU::NameKey script_name{AnimU::field<std::uint32_t>(site,27),AnimU::field<std::uint32_t>(site,31)};
        if(!AnimU::name_string(catalog_name_pool,script_name,catalog_read,name)||name!="AddXP")continue;
        state.xp_target=*api.offset(target);
        state.xp_amount=*api.offset(xp);
        return reward_install_site(state.xp,fn,site,0x19);
    }
    return false;
}
static bool native_reward_bind_drop(){
    auto& state=combat_rewards;
    if(state.drop.code&&api.weakget(&state.drop.function))return true;
    const auto fn=world_input(world_reward_drop),lib=world_input(world_reward_drop_library);
    const auto code=fn?AnimU::field<unsigned char*>(fn,96):nullptr;
    if(!code||!lib||*api.parms(fn)!=224||AnimU::field<int>(fn,104)!=1497||code[0]!=0x4c)return false;
    const auto drop=damage_field(fn,"Drop",112,0,"StructProperty");
    if(!drop||!damage_field(fn,"Dropper",8,144,"ObjectProperty")||
       !damage_field(fn,"Source",8,160,"NameProperty")||!damage_field(fn,"__WorldContext",8,200,"ObjectProperty"))return false;
    const auto type=AnimU::field<void*>(drop,0x78);
    if(!damage_field(type,"Item_10_B0EB20114658902BDFBB5C9B0C9C4024",40,8,"SoftObjectProperty")||
       !damage_field(type,"Item2_15_C69280724CF002967E616FB459A2A5CA",40,64,"SoftObjectProperty"))return false;
    for(unsigned i=0;i<state.drop_flags.size();++i){
        state.drop_flags[i]=damage_field(fn,drop_flag_names[i],1,drop_flag_offsets[i],"BoolProperty");
        if(!state.drop_flags[i])return false;
    }
    const auto tail=code+1456;
    const char* outputs[]{"FirstItemActor","SecondItemActor"};
    const char* locals[]{"FirstDroppedItemActor","SecondDroppedItemActor"};
    for(unsigned i=0;i<2;++i){
        const auto out=damage_field(fn,outputs[i],8,208+8*i,"ObjectProperty");
        const auto local=catalog_field(fn,locals[i],8);
        const auto op=tail+19*i;
        if(!out||!medic_field_class(local,"ObjectProperty")||op[0]!=0x5f||op[1]!=0x48||
           AnimU::field<void*>(op,2)!=out||op[10]!=0x00||AnimU::field<void*>(op,11)!=local)return false;
        state.drop_result[i]=*api.offset(local);
    }
    if(tail[38]!=0x04||tail[39]!=0x0b||tail[40]!=0x53)return false;
    state.drop_outputs=tail;
    return reward_install_site(state.drop,fn,code,0x4c);
}

static void native_reward_enqueue(const RW::Event& event){
    native_reward_sync_scope();
    auto test=event;
    test.sequence=1;
    if(!RW::valid(test)){++combat_reward_rejected;return;}
    auto& state=combat_rewards;
    for(unsigned peer=0;peer<state.backlog.size();++peer)if(active_peer_mask&(1u<<peer)){
        auto& queue=state.backlog[peer];
        if(queue.empty()&&state.outboxes[peer].enqueue(event,1))continue;
        if(queue.size()<256)queue.push_back(event);
        else ++combat_reward_rejected;
    }
}
static void native_reward_xp(void* actor,void* frame,void* result,const NativeRewardSite& site){
    const auto& state=combat_rewards;
    const auto pawn=api.weakget(&automatic_context),locals=AnimU::field<void*>(frame,40);
    if(!pawn||!locals||!actor){site.stock(actor,frame,result);return;}
    const auto amount=AnimU::field<int>(locals,state.xp_amount);
    auto target_at=static_cast<unsigned char*>(actor)+state.xp_target;
    const auto target=AnimU::field<void*>(target_at,0);
    const bool remote=target&&std::ranges::any_of(native_damage.peers,[&](const NativeDamagePeer& peer){
        return api.weakget(&peer.proxy)==target;
    });
    Weak enemy{};
    const bool redirect=remote&&amount>0&&makeweak(enemy,actor);
    if(redirect)std::memcpy(target_at,&pawn,8);
    site.stock(actor,frame,result);
    if(redirect){
        if(api.weakget(&enemy)==actor&&AnimU::field<void*>(target_at,0)==pawn)std::memcpy(target_at,&target,8);
        combat_host_xp+=static_cast<unsigned>(amount);
    }
    if((target==pawn||redirect)&&amount>0){
        RW::Event event{};
        event.kind=RW::Kind::xp;
        event.amount=amount;
        native_reward_enqueue(event);
    }
}
static bool reward_soft_path(const unsigned char* value,RW::Text& target){
    const auto fn=world_input(world_reward_soft_string),cdo=world_input(world_kismet);
    if(!fn||!cdo||!native_free||*api.parms(fn)!=56||factory_parameter(fn,"ReturnValue",16)!=40)return false;
    alignas(16) unsigned char args[56]{};
    std::memcpy(args,value,40);
    api.event(cdo,fn,args);
    const auto text=AnimU::field<MedicString>(args,40);
    std::string path;
    bool valid=!text.data&&text.size==0;
    if(text.data&&text.size>0&&text.size<=301&&text.capacity>=text.size&&text.data[text.size-1]==0){
        valid=true;
        for(int i=0;i+1<text.size;++i){
            if(text.data[i]>127){valid=false;break;}
            path.push_back(static_cast<char>(text.data[i]));
        }
    }
    if(text.data)native_free(const_cast<wchar_t*>(text.data));
    return valid&&target.set(path=="None"?std::string_view{}:std::string_view(path))&&RW::asset(target,true);
}
static void native_reward_capture_drop(void* dropper,const unsigned char* locals,bool treasure_lizard){
    const auto data=catalog_object_field(dropper,"Data");
    std::string path,source;
    RW::Event event{};
    event.kind=RW::Kind::drop;
    if(treasure_lizard)event.flags=RW::drop_mark_first;
    if(!data||!catalog_path(data,path)||!event.text[0].set(path)||
       !AnimU::name_string(catalog_name_pool,AnimU::field<AnimU::NameKey>(locals,160),catalog_read,source)||
       !event.text[1].set(source)||!reward_soft_path(locals+8,event.text[2])||!reward_soft_path(locals+64,event.text[3])){
        ++combat_reward_rejected;
        return;
    }
    for(unsigned i=0;i<automatic_limit;++i)if(entities[i].used&&api.weakget(&entities[i].actor)==dropper){
        event.entity=i+1;
        event.generation=entities[i].generation;
        break;
    }
    for(unsigned i=0;i<combat_rewards.drop_flags.size();++i)
        if(api.getbool(combat_rewards.drop_flags[i],locals,0))event.flags|=1u<<i;
    std::memcpy(event.values.data(),locals+112,24);
    std::memcpy(event.values.data()+6,locals+172,12);
    std::memcpy(event.values.data()+9,locals+188,12);
    event.values[12]=AnimU::field<float>(locals,156);
    event.values[13]=AnimU::field<float>(locals,168);
    event.chances={AnimU::field<float>(locals,0),AnimU::field<float>(locals,4),
                   AnimU::field<float>(locals,52),AnimU::field<float>(locals,56)};
    event.counts={AnimU::field<int>(locals,48),AnimU::field<int>(locals,104)};
    native_reward_enqueue(event);
    ++combat_drop_offers;
}
static bool native_reward_duplicate_lizard(void* actor){
    auto& recorded=combat_rewards.treasure_lizards;
    std::erase_if(recorded,[](const Weak& entry){return !api.weakget(&entry);});
    if(std::ranges::any_of(recorded,[actor](const Weak& entry){return api.weakget(&entry)==actor;}))return true;
    Weak key{};
    if(makeweak(key,actor))recorded.push_back(key);
    return false;
}
static void native_reward_observe(void* context,void* frame,void* result){
    auto& state=combat_rewards;
    const auto code=frame?AnimU::field<const unsigned char*>(frame,32):nullptr;
    const auto node=frame?AnimU::field<void*>(frame,16):nullptr;
    const bool on_thread=frame&&GetCurrentThreadId()==game_thread.load();
    if(state.xp.code&&code==state.xp.code+1&&node==state.xp.node){
        if(on_thread&&host_role&&automatic_limit&&active_peer_mask)native_reward_xp(context,frame,result,state.xp);
        else state.xp.stock(context,frame,result);
        return;
    }
    if(state.drop.code&&code==state.drop.code+1&&node==state.drop.node){
        auto locals=on_thread?AnimU::field<unsigned char*>(frame,40):nullptr;
        const auto dropper=locals?AnimU::field<void*>(locals,144):nullptr;
        if(dropper&&automatic_limit&&active_peer_mask&&api.isa(dropper,base_class)){
            const auto mode=host_role?native_lizard_mode(dropper):nullptr;
            const bool treasure_lizard=mode&&!api.getbool(mode,dropper,0);
            const bool suppress=host_role?treasure_lizard&&native_reward_duplicate_lizard(dropper):
                !state.applying&&std::ranges::any_of(enrollments,[&](const auto& entry){
                    return api.weakget(&entry.replica)==dropper;
                });
            if(suppress){
                for(auto at:state.drop_result)std::memset(locals+at,0,8);
                std::memcpy(static_cast<unsigned char*>(frame)+32,&state.drop_outputs,8);
                ++combat_drop_suppressed;
                return;
            }
            if(host_role)native_reward_capture_drop(dropper,locals,treasure_lizard);
        }
        state.drop.stock(context,frame,result);
        return;
    }
    state.undefined(context,frame,result);
}

static void reward_ack_native(){
    E::Packet packet{};
    if(RW::encode_ack(envelope,combat_rewards.inbox.applied(),packet))send(packet,1);
}

static bool receive_native_reward(const ReceivedPacket& received){
    if(!(steam_transport||lan_transport)||!automatic_limit||received.packet.size<40)return false;
    auto& state=combat_rewards;
    const auto kind=A::wire::get<std::uint32_t>(received.packet,32);
    if(kind!=6&&kind!=7)return false;
    if(!RW::header(envelope,received.packet,kind))return true;
    native_reward_sync_scope();
    if(host_role&&kind==7&&received.peer<state.outboxes.size()){
        std::uint64_t sequence{};
        if(!RW::decode_ack(envelope,received.packet,sequence)||!state.outboxes[received.peer].applied_ack(0,sequence))
            ++combat_reward_rejected;
    }else if(!host_role&&kind==6&&received.peer==0){
        RW::Event event{};
        if(!RW::decode(envelope,received.packet,event)||(event.kind!=RW::Kind::drop&&event.kind!=RW::Kind::xp)){
            ++combat_reward_rejected;
        }else{
            const auto received_event=state.inbox.receive(event);
            if(received_event==RW::Result::conflict)state.inbox.conflict();
            if(received_event==RW::Result::duplicate&&event.sequence<=state.inbox.applied())reward_ack_native();
        }
    }
    return true;
}

static bool native_reward_finish(void* pawn,void* save){
    auto& inbox=combat_rewards.inbox;
    if(medic_active(medic_runtime.save,pawn)!=save){
        inbox.uncertain();
        ++combat_reward_rejected;
        return false;
    }
    if(!inbox.complete())return false;
    reward_ack_native();
    return true;
}

static bool native_reward_apply_xp(const RW::Event& event,void* pawn,void* save){
    const auto fn=world_input(world_reward_add_xp);
    if(!fn||*api.parms(fn)!=5||!damage_field(fn,"XP",4,0,"IntProperty")||
       !damage_field(fn,"FromSteal",1,4,"BoolProperty"))return false;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,event.amount);
    const auto amount=event.amount;
    if(!combat_rewards.inbox.begin())return false;
    api.event(pawn,fn,args.data());
    if(!native_reward_finish(pawn,save))return false;
    combat_guest_xp+=static_cast<unsigned>(amount);
    return true;
}

static constexpr std::array<int,6> reward_map_layout{8,16,20,24,8,24};
static MedicMap native_reward_map(void* type,std::string_view name){
    const auto field=catalog_field(type,name,80);
    if(!medic_field_class(field,"MapProperty"))return {};
    const auto key=AnimU::field<void*>(field,0x78),value=AnimU::field<void*>(field,0x80);
    if(!medic_field_class(key,"ObjectProperty")||!medic_field_class(value,"BoolProperty")||
       *api.size(key)!=8||*api.size(value)!=1||AnimU::field<unsigned>(field,0xa0)||
       AnimU::field<std::array<int,6>>(field,0x88)!=reward_map_layout)return {};
    return {field,*api.offset(field)};
}
static bool native_reward_mark(const MedicMap& map,void* save,void* item,bool& value){
    return qdcoop::ue427::object_bool(reinterpret_cast<std::uintptr_t>(map.address(save)),
        reinterpret_cast<std::uintptr_t>(item),reward_map_layout,reward_read,value);
}
static bool native_reward_soft(void* item,void* system,void* fn,unsigned char* destination){
    if(!item)return true;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,item);
    api.event(system,fn,args.data());
    const auto subpath=AnimU::field<MedicString>(args.data(),32);
    if(subpath.data){
        native_free(const_cast<wchar_t*>(subpath.data));
        return false;
    }
    if(subpath.size||subpath.capacity)return false;
    std::memcpy(destination,args.data()+8,40);
    return true;
}
static bool native_reward_apply_drop(const RW::Event& event,void* pawn,void* save){
    auto& state=combat_rewards;
    auto& binding=medic_runtime.save;
    const auto lib=world_input(world_reward_drop_library),fn=world_input(world_reward_drop);
    const auto system=world_input(world_kismet),soft=world_input(world_medic_soft);
    const auto item_type=world_input(world_reward_item_type),data_type=world_input(world_hud_group_type);
    const auto fog=world_input(world_reward_fog);
    if(!(combat_reward_bind_stage&2)||!lib||!fn||!system||!soft||!item_type||!data_type||!fog||
       *api.parms(fog)!=16||!binding.add||!native_free||
       !medic_params(world_medic_soft,48,{{"Object",8,0},{"ReturnValue",40,8}})||
       !world_input(world_strings)||!world_input(world_string_name))return false;
    const auto data=load_world_asset(event.text[0].view(),UINT32_MAX,false,nullptr);
    if(!data||!api.isa(data,data_type))return false;
    const auto save_type=AnimU::field<void*>(save,16);
    const std::array<MedicMap,2> maps{
        native_reward_map(save_type,"EnemyDropDataA"),native_reward_map(save_type,"EnemyDropDataB")
    };
    alignas(16) unsigned char args[224]{};
    for(unsigned i=0;i<maps.size();++i){
        bool marked{};
        if(!maps[i].field||!native_reward_mark(maps[i],save,data,marked))return false;
        if(!event.text[i+2].size)continue;
        const auto item=load_world_asset(event.text[i+2].view(),UINT32_MAX,false,nullptr);
        if(!item||!api.isa(item,item_type)||!native_reward_soft(item,system,soft,args+(i?64:8)))return false;
    }
    std::array<wchar_t,RW::text_capacity+1> source{};
    const auto& name=event.text[1];
    if(name.size&&!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name.data.data(),name.size,source.data(),RW::text_capacity))
        return false;
    const auto source_name=medic_name(source.data());
    void* replica=nullptr;
    if(event.entity){
        const auto& entity=entities[event.entity-1];
        if(entity.used&&entity.generation==event.generation)replica=api.weakget(&entity.actor);
    }
    const auto dropper=replica?replica:pawn;
    std::memcpy(args,event.chances.data(),8);
    std::memcpy(args+52,event.chances.data()+2,8);
    std::memcpy(args+48,&event.counts[0],4);
    std::memcpy(args+104,&event.counts[1],4);
    std::memcpy(args+112,event.values.data(),24);
    std::memcpy(args+144,&dropper,8);
    std::memcpy(args+156,&event.values[12],4);
    std::memcpy(args+160,&source_name,8);
    std::memcpy(args+168,&event.values[13],4);
    std::memcpy(args+172,event.values.data()+6,12);
    std::memcpy(args+188,event.values.data()+9,12);
    std::memcpy(args+200,&pawn,8);
    for(unsigned i=0;i<state.drop_flags.size();++i)
        api.setbool(state.drop_flags[i],args,(event.flags&(1u<<i))!=0,0);
    Weak live_replica{};
    if(replica&&!makeweak(live_replica,replica))return false;
    if(medic_active(binding,pawn)!=save||!state.inbox.begin())return false;
    state.applying=true;
    api.event(lib,fn,args);
    state.applying=false;
    ++combat_drop_rolls;
    std::array<void*,2> spawned{};
    std::memcpy(spawned.data(),args+208,16);
    if(medic_active(binding,pawn)!=save)return native_reward_finish(pawn,save);
    if(replica&&api.weakget(&live_replica)==replica)api.event(replica,fog,spawned.data());
    if(medic_active(binding,pawn)!=save)return native_reward_finish(pawn,save);
    // The treasure lizard's delayed Blueprint sets map A even when its roll misses.
    for(unsigned i=0;i<maps.size();++i)if(spawned[i]||(!i&&(event.flags&RW::drop_mark_first))){
        const unsigned char value=1;
        binding.add(maps[i].address(save),maps[i].field,&data,&value);
        bool marked{};
        if(!native_reward_mark(maps[i],save,data,marked)||!marked){
            state.inbox.uncertain();
            ++combat_reward_rejected;
            return false;
        }
        if(spawned[i])++combat_drop_items;
    }
    return native_reward_finish(pawn,save);
}

static void native_reward_step(void* pawn,double time){
    if(!(steam_transport||lan_transport))return;
    native_reward_sync_scope();
    auto& state=combat_rewards;
    const unsigned required=host_role?3:2;
    if(combat_reward_bind_stage!=required&&time>=state.next_bind){
        state.next_bind=time+.25;
        scan_world_inputs(4096);
        if(host_role&&native_reward_bind_xp())combat_reward_bind_stage|=1;
        if(native_reward_bind_drop())combat_reward_bind_stage|=2;
    }
    if(host_role){
        for(unsigned peer=0;peer<state.outboxes.size();++peer){
            if(!(active_peer_mask&(1u<<peer)))continue;
            auto& queue=state.backlog[peer];
            auto& out=state.outboxes[peer];
            while(!queue.empty()&&out.enqueue(queue.front(),1))queue.pop_front();
            for(unsigned i=0;i<8&&out.pending();++i){
                const auto event=out.due(0,time);
                if(!event)break;
                E::Packet packet{};
                if(RW::encode(envelope,*event,packet))send(packet,1u<<peer);
            }
        }
        return;
    }
    if(time<state.next_apply||state.inbox.phase()!=RW::Phase::waiting)return;
    const auto event=state.inbox.next();
    if(!event)return;
    const auto save=medic_active(medic_runtime.save,pawn);
    const bool applied=save&&(event->kind==RW::Kind::xp?native_reward_apply_xp(*event,pawn,save):
                                                      native_reward_apply_drop(*event,pawn,save));
    if(!applied)state.next_apply=time+.25;
}
