namespace BossHud=qdcoop::boss_hud;

struct HealthbarFields {
    void* hp{};
    void* max_hp{};
    void* faded{};
    void* timer{};
    void* enemy{};
    void* targets{};
    void* group{};
};
struct SavedBossHud {
    Weak widget,group;
    std::array<Weak,BossHud::capacity> targets{};
    unsigned count{};
};
struct NativeHealthbar {
    BossHud::Frame sent,wanted;
    SavedBossHud original;
    GearAsset group;
    std::string group_path;
    double step{},send{},expires{};
};
static NativeHealthbar native_healthbar;

static void* healthbar_field(void* type,std::string_view name,int size,std::string_view field_type){
    const auto field=catalog_field(type,name,size);
    return medic_field_class(field,field_type)?field:nullptr;
}

static void* healthbar_object(void* object,std::string_view name){
    if(!object)return nullptr;
    const auto field=healthbar_field(AnimU::field<void*>(object,16),name,8,"ObjectProperty");
    return field?AnimU::field<void*>(object,*api.offset(field)):nullptr;
}

static void* healthbar_array_field(void* type,std::string_view name){
    const auto field=healthbar_field(type,name,16,"ArrayProperty");
    const auto inner=field?AnimU::field<void*>(field,0x78):nullptr;
    return medic_field_class(inner,"ObjectProperty")&&*api.size(inner)==8?field:nullptr;
}

static bool healthbar_fields(void* widget,bool boss,HealthbarFields& out){
    const auto method=world_input(boss?world_hud_boss_damage:world_hud_enemy_damage);
    if(!widget||!method||!api.isa(widget,AnimU::field<void*>(method,32)))return false;
    const auto type=AnimU::field<void*>(widget,16);
    HealthbarFields fields{
        healthbar_field(type,"CurrentHP",4,"FloatProperty"),healthbar_field(type,"MaxHP",4,"FloatProperty"),
        healthbar_field(type,"FadedOut",1,"BoolProperty"),healthbar_field(type,"DisappearTimer",4,"FloatProperty")};
    if(boss){
        fields.targets=healthbar_array_field(type,"Targets");
        fields.group=healthbar_field(type,"CurrentGroup",8,"ObjectProperty");
        if(!fields.targets||!fields.group)return false;
    }else if(!(fields.enemy=healthbar_field(type,"Enemy",8,"ObjectProperty")))return false;
    if(!fields.hp||!fields.max_hp||!fields.faded||!fields.timer)return false;
    out=fields;
    return true;
}

template<class T> static void healthbar_put(void* widget,void* field,const T& value){
    std::memcpy(static_cast<unsigned char*>(widget)+*api.offset(field),&value,sizeof(value));
}

static bool healthbar_targets(void* widget,const HealthbarFields& fields,NativeActorArray& targets){
    targets=AnimU::field<NativeActorArray>(widget,*api.offset(fields.targets));
    return targets.count>=0&&targets.count<=static_cast<int>(BossHud::capacity)&&
           targets.capacity>=targets.count&&(!targets.count||targets.data);
}

static unsigned healthbar_entity(void* actor){
    if(!actor)return E::capacity;
    for(unsigned index=0;index<E::capacity;++index){
        const auto& enemy=entities[index];
        if(enemy.used&&enrollments[index].offer.id&&!enrollments[index].retiring&&
           (host_role||ownership[index].active)&&api.weakget(&enemy.actor)==actor)return index;
    }
    return E::capacity;
}

static void* healthbar_method(void* widget,unsigned id){
    const auto method=world_input(id);
    return widget&&method&&api.isa(widget,AnimU::field<void*>(method,32))?method:nullptr;
}

static void healthbar_fill(void* widget,float value){
    const auto image=healthbar_object(widget,"Fill");
    const auto dynamic=healthbar_method(image,world_view_dynamic);
    if(!dynamic||!medic_params(world_view_dynamic,8,{{"ReturnValue",8,0}})||
       !medic_params(world_view_scalar,12,{{"ParameterName",8,0},{"Value",4,8}}))return;
    void* material{};
    api.event(image,dynamic,&material);
    const auto scalar=healthbar_method(material,world_view_scalar);
    const auto name=placed_pickup_name("Fill");
    if(!scalar||!name.index)return;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,name);
    factory_put(args,8,value);
    api.event(material,scalar,args.data());
}

static void healthbar_hide(void* widget,const HealthbarFields& fields){
    if(const auto stop=healthbar_method(widget,world_hud_stop);stop&&*api.parms(stop)==0)api.event(widget,stop,nullptr);
    const auto segments=healthbar_object(widget,"DamagedSegments");
    if(const auto clear=healthbar_method(segments,world_hud_clear);clear&&*api.parms(clear)==0)
        api.event(segments,clear,nullptr);
    const auto type=AnimU::field<void*>(widget,16);
    if(const auto running=healthbar_array_field(type,"CurrentlyRunningDamageSegments")){
        auto* data=static_cast<unsigned char*>(widget)+*api.offset(running);
        const auto array=AnimU::field<NativeActorArray>(data,0);
        if(array.count>=0&&array.count<=array.capacity)std::memset(data+8,0,4);
    }
    healthbar_put(widget,fields.hp,0.f);
    healthbar_put(widget,fields.timer,0.f);
    api.setbool(fields.faded,widget,true,0);
    if(fields.enemy)healthbar_put<void*>(widget,fields.enemy,nullptr);
    healthbar_fill(widget,0.f);
    if(const auto opacity=healthbar_method(widget,world_hud_opacity);
       opacity&&medic_params(world_hud_opacity,4,{{"InOpacity",4,0}})){
        float zero{};
        api.event(widget,opacity,&zero);
    }
}

static bool healthbar_update(void* widget,const HealthbarFields& fields,float hp,bool boss){
    const auto current=AnimU::field<float>(widget,*api.offset(fields.hp));
    if(!std::isfinite(hp)||!std::isfinite(current))return false;
    if(hp<=0){
        if(!fields.enemy&&current==0&&api.getbool(fields.faded,widget,0))return false;
        healthbar_hide(widget,fields);
        return true;
    }
    if(std::abs(current-hp)<=.000001f)return false;
    if(!boss&&hp>current){
        const auto maximum=AnimU::field<float>(widget,*api.offset(fields.max_hp));
        healthbar_put(widget,fields.hp,hp);
        if(maximum>0&&std::isfinite(maximum))healthbar_fill(widget,std::clamp(hp/maximum,0.f,1.f));
        return true;
    }
    const auto id=boss?world_hud_boss_damage:world_hud_enemy_damage;
    const auto valid=boss?medic_params(id,4,{{"NewHP",4,0}}):medic_params(id,5,{{"NewHP",4,0},{"Dead",1,4}});
    if(!valid)return false;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,hp);
    api.event(widget,world_input(id),args.data());
    return true;
}

static void healthbar_capture(void* widget,const HealthbarFields& fields,double time){
    auto& state=native_healthbar;
    BossHud::Frame frame;
    NativeActorArray targets{};
    if(widget&&!healthbar_targets(widget,fields,targets))return;
    if(widget&&!api.getbool(fields.faded,widget,0)&&targets.count){
        for(int i=0;i<targets.count;++i){
            const auto index=healthbar_entity(targets.data[i]);
            if(index==E::capacity)return;
            frame.targets[frame.count++]={index+1,entities[index].generation};
        }
        std::ranges::sort(frame.targets.begin(),frame.targets.begin()+frame.count,{},&BossHud::Target::id);
        const auto group=AnimU::field<void*>(widget,*api.offset(fields.group));
        const auto type=world_input(world_hud_group_type);
        if(group&&type&&api.isa(group,type)&&(!catalog_path(group,frame.group)||!BossHud::valid_group(frame.group)))
            frame.group.clear();
    }
    if(frame.same_membership(state.sent)&&time<state.send)return;
    if(!frame.count&&!state.sent.sequence)return;
    frame.sequence=state.sent.sequence+1;
    E::Packet packet{};
    if(BossHud::encode(envelope,frame,packet)&&send(packet,active_peer_mask)){
        state.sent=std::move(frame);
        state.send=time+2.;
        ++boss_hud_sent;
    }
}

static bool receive_boss_hud(const ReceivedPacket& received,unsigned kind,double time){
    if(kind!=BossHud::kind)return false;
    BossHud::Frame frame;
    auto& state=native_healthbar;
    if(host_role||received.peer||!active_peer_mask||!BossHud::decode(envelope,received.packet,frame)||
       frame.sequence<=state.wanted.sequence)return true;
    state.wanted=std::move(frame);
    state.expires=time+5.;
    ++boss_hud_received;
    return true;
}

static bool healthbar_replace(void* widget,const HealthbarFields& fields,const NativeActorArray& actual,
                              std::span<void* const> wanted,void* group){
    const auto add=healthbar_method(widget,world_hud_boss_add),remove=healthbar_method(widget,world_hud_boss_remove);
    if(!add||!remove||!medic_params(world_hud_boss_add,16,{{"Enemy",8,0},{"Group",8,8}})||
       !medic_params(world_hud_boss_remove,16,{{"Group",8,0},{"Enemy",8,8}}))return false;
    const auto old_group=AnimU::field<void*>(widget,*api.offset(fields.group));
    std::array<void*,BossHud::capacity> previous{};
    if(actual.count)std::copy_n(actual.data,actual.count,previous.data());
    for(int i=0;i<actual.count;++i){
        if(!previous[i]||!api.isa(previous[i],base_class))continue;
        void* args[]{old_group,previous[i]};
        api.event(widget,remove,args);
    }
    NativeActorArray remaining{};
    if(!healthbar_targets(widget,fields,remaining)||remaining.count)return false;
    healthbar_put<void*>(widget,fields.group,nullptr);
    for(const auto actor:wanted){
        void* args[]{actor,group};
        api.event(widget,add,args);
    }
    return true;
}

static void healthbar_apply_boss(void* widget,const HealthbarFields& fields,double time){
    auto& state=native_healthbar;
    if(!state.wanted.sequence)return;
    const auto count=time<state.expires?state.wanted.count:0u;
    std::array<void*,BossHud::capacity> actors{};
    float hp{};
    for(unsigned i=0;i<count;++i){
        const auto target=state.wanted.targets[i];
        const auto index=target.id-1;
        const auto& enemy=entities[index];
        if(!enemy.used||!ownership[index].active||enemy.generation!=target.generation||
           !(actors[i]=api.weakget(&enemy.actor)))return;
        hp+=std::max(0.f,AnimU::field<float>(actors[i],hp_at));
    }
    void* group{};
    if(count&&!state.wanted.group.empty()){
        if(state.group_path!=state.wanted.group){
            state.group_path=state.wanted.group;
            state.group={};
        }
        const auto type=world_input(world_hud_group_type);
        if(!type)return;
        group=gear_load(state.group,state.group_path,time,UINT32_MAX,type);
        if(!group)return;
    }
    NativeActorArray actual{};
    if(!healthbar_targets(widget,fields,actual))return;
    const auto old_group=AnimU::field<void*>(widget,*api.offset(fields.group));
    const std::span<void* const> wanted(actors.data(),count);
    bool same=actual.count==static_cast<int>(count)&&(!count||old_group==group);
    if(same)for(int i=0;i<actual.count&&same;++i)same=std::ranges::find(wanted,actual.data[i])!=wanted.end();
    if(!same){
        if(api.weakget(&state.original.widget)!=widget){
            SavedBossHud original;
            if(!makeweak(original.widget,widget))return;
            if(old_group&&!makeweak(original.group,old_group))return;
            for(int i=0;i<actual.count;++i)
                if(actual.data[i]&&makeweak(original.targets[original.count],actual.data[i]))++original.count;
            state.original=original;
        }
        if(!healthbar_replace(widget,fields,actual,wanted,group))return;
    }
    if(healthbar_update(widget,fields,hp,true))++boss_hud_updates;
}

static void native_healthbar_cleanup(){
    auto& original=native_healthbar.original;
    const auto widget=api.weakget(&original.widget);
    HealthbarFields fields{};
    NativeActorArray current{};
    if(healthbar_fields(widget,true,fields)&&healthbar_targets(widget,fields,current)){
        std::array<void*,BossHud::capacity> actors{};
        unsigned count{};
        for(unsigned i=0;i<original.count;++i)if(const auto actor=api.weakget(&original.targets[i]))actors[count++]=actor;
        healthbar_replace(widget,fields,current,{actors.data(),count},api.weakget(&original.group));
    }
    original={};
}

static void native_healthbar_reset(){native_healthbar={};}

static void native_healthbar_step(void* pawn,double time){
    auto& state=native_healthbar;
    if(!native_bootstrap||!active_peer_mask||time<state.step)return;
    state.step=time+.2;
    const auto hud=healthbar_object(pawn,"HUD");
    const auto zone=healthbar_object(hud,"EnemyStatusZone");
    const auto boss=healthbar_object(zone,"BossHealthBar");
    HealthbarFields fields{};
    const auto bound=healthbar_fields(boss,true,fields);
    if(host_role){healthbar_capture(bound?boss:nullptr,fields,time);return;}
    if(bound)healthbar_apply_boss(boss,fields,time);
    const auto enemy=healthbar_object(zone,"EnemyHealthBar_base");
    if(!healthbar_fields(enemy,false,fields))return;
    const auto actor=AnimU::field<void*>(enemy,*api.offset(fields.enemy));
    const auto index=healthbar_entity(actor);
    if(index==E::capacity||(!entities[index].placed&&!deaths[index].terminal))return;
    if(healthbar_update(enemy,fields,deaths[index].terminal?0.f:AnimU::field<float>(actor,hp_at),false))++enemy_hud_updates;
}
