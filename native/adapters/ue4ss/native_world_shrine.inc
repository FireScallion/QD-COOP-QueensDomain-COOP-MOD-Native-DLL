enum class ShrineGateKind {none,shrine,bridge,castle};
struct ShrineGateSpec {std::string_view type,timeline,part;};
static constexpr std::array shrine_gate_specs{
    ShrineGateSpec{"/Game/Props/SwordShrine/SwordShrineGate_BP.SwordShrineGate_BP_C","Timeline_0","ClosedBox"},
    ShrineGateSpec{"/Game/OtherWorld/CastleBridge/OW_BridgeDoor.OW_BridgeDoor_C","OpenDOors","LeftDoor"},
    ShrineGateSpec{"/Game/OtherWorld/Castle_Ext/CastleFrontDoor_BP.CastleFrontDoor_BP_C","DoorOpen","AquaDoor_00_SM"}
};
static ShrineGateKind shrine_gate_kind(std::string_view path){
    const auto found=std::ranges::find(shrine_gate_specs,path,&ShrineGateSpec::type);
    return found==shrine_gate_specs.end()?ShrineGateKind::none:
        static_cast<ShrineGateKind>(found-shrine_gate_specs.begin()+1);
}
struct ShrineGate {
    Weak open,part_type;
    ShrineGateKind kind{};
    int timeline{},part{},relative{},target{};
    explicit operator bool() const{return kind!=ShrineGateKind::none;}
};
static bool bind_shrine_gate(void* type,ShrineGateKind kind,ShrineGate& out){
    for(unsigned id=world_shrine_timeline;id<=world_shrine_seek;++id)
        if(!world_input(id)){scan_world_inputs(2048);break;}
    const auto timeline_type=world_input(world_shrine_timeline);
    if(!timeline_type||
       !medic_params(world_shrine_playing,1,{{"ReturnValue",1,0}})||
       !medic_params(world_shrine_reversing,1,{{"ReturnValue",1,0}})||
       !medic_params(world_shrine_position,4,{{"ReturnValue",4,0}})||
       !medic_params(world_shrine_length,4,{{"ReturnValue",4,0}})||
       !medic_params(world_shrine_stop,0,{})||
       !medic_params(world_shrine_seek,6,{{"NewPosition",4,0},{"bFireEvents",1,4},{"bFireUpdate",1,5}}))return false;
    const auto& spec=shrine_gate_specs[static_cast<unsigned>(kind)-1];
    const auto timeline=catalog_field(type,spec.timeline,8),part=catalog_field(type,spec.part,8);
    if(!medic_field_class(timeline,"ObjectProperty")||!medic_field_class(part,"ObjectProperty")||
       AnimU::field<void*>(timeline,0x78)!=timeline_type)return false;
    const auto part_type=AnimU::field<void*>(part,0x78);
    std::string path;
    if(!catalog_path(part_type,path))return false;
    ShrineGate binding{};
    if(kind==ShrineGateKind::shrine){
        if(path!="/Script/Engine.BoxComponent"||
           !medic_params(world_component_collision,1,{{"NewType",1,0}}))return false;
    }else{
        if(path!="/Script/Engine.StaticMeshComponent")return false;
        const auto relative=catalog_field(part_type,"RelativeLocation",12);
        if(!medic_field_class(relative,"StructProperty")||
           !catalog_path(AnimU::field<void*>(relative,0x78),path)||path!="/Script/CoreUObject.Vector")return false;
        const bool castle=kind==ShrineGateKind::castle;
        const auto target=catalog_field(type,castle?"doorLFinalState":"OpenWidth",castle?48:4);
        if(!medic_field_class(target,castle?"StructProperty":"FloatProperty")||
           (castle&&(!catalog_path(AnimU::field<void*>(target,0x78),path)||path!="/Script/CoreUObject.Transform")))return false;
        binding.relative=*api.offset(relative)+(castle?4:0);
        binding.target=*api.offset(target)+(castle?20:0);
    }
    if(!world_mechanism_method(type,"SetToOpenState",binding.open)||!makeweak(binding.part_type,part_type))return false;
    binding.kind=kind;
    binding.timeline=*api.offset(timeline);
    binding.part=*api.offset(part);
    out=binding;
    return true;
}
static void* shrine_gate_timeline(void* actor,const ShrineGate& binding){
    const auto timeline=AnimU::field<void*>(actor,binding.timeline),type=world_input(world_shrine_timeline);
    return timeline&&type&&api.isa(timeline,type)?timeline:nullptr;
}
static float shrine_timeline_length(void* timeline){
    float length{};
    api.event(timeline,world_input(world_shrine_length),&length);
    return length;
}
static std::optional<bool> shrine_gate_open(void* actor,const ShrineGate& binding,bool capture=false){
    const auto timeline=shrine_gate_timeline(actor,binding);
    const auto playing=world_input(world_shrine_playing),reversing=world_input(world_shrine_reversing),
               position=world_input(world_shrine_position),length=world_input(world_shrine_length);
    if(!timeline||!playing||!reversing||!position||!length)return std::nullopt;
    std::uint8_t flag{};
    api.event(timeline,playing,&flag);
    if(flag){
        api.event(timeline,reversing,&flag);
        return !flag;
    }
    if(capture&&binding.kind!=ShrineGateKind::shrine){
        const auto mesh=AnimU::field<void*>(actor,binding.part),type=api.weakget(&binding.part_type);
        if(!mesh||!type||!api.isa(mesh,type))return std::nullopt;
        const auto actual=AnimU::field<float>(mesh,binding.relative),target=AnimU::field<float>(actor,binding.target);
        if(!std::isfinite(actual)||!std::isfinite(target))return std::nullopt;
        return std::abs(actual-(binding.kind==ShrineGateKind::bridge?-target:target))<1.f;
    }
    float at{};
    api.event(timeline,position,&at);
    const auto end=shrine_timeline_length(timeline);
    if(!std::isfinite(at)||!std::isfinite(end)||end<=0.f)return std::nullopt;
    return at>=end-.001f;
}
static void shrine_gate_seek(void* timeline,void* stop,void* seek,float position){
    api.event(timeline,stop,nullptr);
    alignas(4) unsigned char args[6]{};
    std::memcpy(args,&position,sizeof(position));
    args[5]=1;
    api.event(timeline,seek,args);
}
static bool apply_shrine_gate(void* actor,const ShrineGate& binding,void* opened,void* close,bool desired){
    const auto actual=shrine_gate_open(actor,binding);
    if(!actual||*actual==desired)return false;
    const auto timeline=shrine_gate_timeline(actor,binding);
    const auto stop=world_input(world_shrine_stop),seek=world_input(world_shrine_seek);
    if(desired){
        const auto open=api.weakget(&binding.open);
        if(!timeline||!open||!stop||!seek||!world_input(world_shrine_length))return false;
        const auto length=shrine_timeline_length(timeline);
        if(!std::isfinite(length)||length<=0.f)return false;
        api.event(actor,open,nullptr);
        api.setbool(opened,actor,true,0);
        shrine_gate_seek(timeline,stop,seek,length);
    }else if(binding.kind==ShrineGateKind::castle){
        if(!timeline||!stop||!seek)return false;
        api.setbool(opened,actor,false,0);
        shrine_gate_seek(timeline,stop,seek,0.f);
    }else{
        if(!close)return false;
        void *box{},*collision{};
        if(binding.kind==ShrineGateKind::shrine){
            box=AnimU::field<void*>(actor,binding.part);
            collision=world_input(world_component_collision);
            const auto type=api.weakget(&binding.part_type);
            if(!box||!type||!api.isa(box,type)||!collision)return false;
        }
        api.setbool(opened,actor,false,0);
        if(box){
            std::uint8_t enabled=3;
            api.event(box,collision,&enabled);
        }
        api.event(actor,close,nullptr);
    }
    return true;
}
