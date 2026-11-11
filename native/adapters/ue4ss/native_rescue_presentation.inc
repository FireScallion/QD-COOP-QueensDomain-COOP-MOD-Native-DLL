// Pinned Blueprint dispatch cells. Every installed observer forwards the
// original opcode. Only exact script sites on the registered thread may act.
struct ViewOpcode {
    std::atomic<KillExec> original{};
    void* volatile* slot{};
    bool install(unsigned opcode,std::uintptr_t rva,std::span<const unsigned char> prefix,KillExec hook){
        if(slot)return true;
        if(!life_opcode_slot||build_state.load()!=1)return false;
        const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        const auto address=image+0x4a62e10+8*opcode;
        const auto entry=reinterpret_cast<void*>(image+rva);
        MEMORY_BASIC_INFORMATION region{};
        if(std::memcmp(entry,prefix.data(),prefix.size())||
           !VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region))||region.State!=MEM_COMMIT||
           !(region.Protect&PAGE_READWRITE)||(region.Protect&PAGE_GUARD)||
           reinterpret_cast<std::uintptr_t>(region.BaseAddress)+region.RegionSize-address<sizeof(void*))return false;
        original.store(reinterpret_cast<KillExec>(entry),std::memory_order_release);
        auto cell=reinterpret_cast<void* volatile*>(address);
        if(InterlockedCompareExchangePointer(cell,reinterpret_cast<void*>(hook),entry)!=entry)return false;
        slot=cell;
        return true;
    }
    bool remove(KillExec hook){
        if(!slot)return true;
        const auto entry=reinterpret_cast<void*>(original.load(std::memory_order_acquire));
        const auto observer=reinterpret_cast<void*>(hook);
        const auto previous=InterlockedCompareExchangePointer(slot,entry,observer);
        slot=nullptr;
        if(previous==observer)++presentation_restored;
        return previous==observer||previous==entry;
    }
};
static ViewOpcode view_jump,view_let_bool,view_pop,view_pop_if;
struct ViewScript {
    void* node{};
    const unsigned char* code{};
};
static ViewScript view_script(WorldObject method,int size){
    const auto fn=world_input(method);
    if(!fn||AnimU::field<int>(fn,104)!=size)return {};
    return {fn,AnimU::field<const unsigned char*>(fn,96)};
}
static bool view_param(WorldObject method,int bytes,std::string_view key,int width,int offset){
    const auto fn=world_input(method);
    return fn&&*api.parms(fn)==bytes&&factory_parameter(fn,key,width)==offset;
}

struct DownView {
    Weak capsule;
    ViewScript tick;
    double started{},last_tick{};
    float yaw{};
    bool world_camera{};
};
static DownView down_view;
static double view_next_scan;
static bool presentation_install_pop();

static std::array<float,3> presentation_rotation(void* pawn){
    alignas(16) FactoryParameters args{};
    api.event(pawn,getters[1].fn,args.data());
    return AnimU::field<std::array<float,3>>(args.data(),getters[1].at);
}
static void presentation_prepare(double time){
    if(time<view_next_scan)return;
    view_next_scan=time+.25;
    for(unsigned i=world_view_first;i<world_cp_first;++i)if(!world_input(i)){
        scan_world_inputs(2048);
        break;
    }
}
static bool presentation_camera_ready(void* pawn){
    const auto capsule=catalog_object_field(pawn,"CapsuleComponent");
    return capsule&&view_param(world_view_move,153,"NewLocation",12,0)&&
           view_param(world_view_move,153,"bTeleport",1,152)&&
           view_param(world_view_rotate,14,"NewRotation",12,0)&&
           view_param(world_view_rotate,14,"bTeleportPhysics",1,12)&&
           view_param(world_view_capsule,4,"ReturnValue",4,0)&&makeweak(down_view.capsule,capsule);
}
static bool presentation_camera_location(void* pawn,std::array<float,3>& position){
    const auto gameplay=world_input(static_cast<unsigned>(world_object_count)+factory_gameplay);
    const auto manager=world_input(world_view_manager),location=world_input(world_view_location);
    if(!gameplay||!manager||!location)return false;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,pawn);
    api.event(gameplay,manager,args.data());
    const auto camera=AnimU::field<void*>(args.data(),16);
    if(!camera)return false;
    api.event(camera,location,position.data());
    return std::ranges::all_of(position,[](float v){return std::isfinite(v);});
}
static bool presentation_manager_layout(){
    return view_param(world_view_manager,24,"WorldContextObject",8,0)&&
           view_param(world_view_manager,24,"PlayerIndex",4,8)&&
           view_param(world_view_manager,24,"ReturnValue",8,16)&&
           view_param(world_view_location,12,"ReturnValue",12,0);
}

struct LookSite {
    void* node{};
    const unsigned char* branch{};
    int axis{};
    bool pitch{},mouse{};
};
static std::array<LookSite,4> look_sites{};
static std::array<int,3> look_rates{};

static void presentation_look(void* pawn,const LookSite& site,void* frame){
    if(!local_life.locked||api.getbool(local_life.layout.modal,pawn,0)||
       AnimU::field<const unsigned char*>(frame,32)!=site.branch+13)return;
    const auto locals=AnimU::field<void*>(frame,40);
    const auto multiplier=world_input(site.pitch?world_view_pitch_multiplier:world_view_yaw_multiplier);
    const auto input=world_input(site.pitch?world_view_pitch_input:world_view_yaw_input);
    if(!locals||!multiplier||!input)return;
    const auto axis=AnimU::field<float>(locals,site.axis);
    if(!std::isfinite(axis)||axis==0)return;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,std::uint8_t{site.mouse});
    api.event(pawn,multiplier,args.data());
    const auto factor=AnimU::field<float>(args.data(),4);
    auto scale=AnimU::field<float>(pawn,look_rates[site.mouse?2u:site.pitch?0u:1u]);
    if(!site.mouse){
        const auto gameplay=world_input(static_cast<unsigned>(world_object_count)+factory_gameplay);
        const auto delta=world_input(world_view_delta);
        if(!gameplay||!delta)return;
        args={};
        factory_put(args,0,pawn);
        api.event(gameplay,delta,args.data());
        scale*=AnimU::field<float>(args.data(),8);
    }
    auto amount=axis*scale*factor;
    if(!std::isfinite(amount)||amount==0)return;
    api.event(pawn,input,&amount);
    ++presentation_look_events;
}
static void observe_view_jump(void* context,void* frame,void* result){
    const auto original=view_jump.original.load(std::memory_order_acquire);
    if(context!=life_watched_pawn.load(std::memory_order_relaxed)||!frame||
       GetCurrentThreadId()!=game_thread.load()||!local_life.locked||
       AnimU::field<void*>(frame,24)!=context){
        original(context,frame,result);
        return;
    }
    const auto node=AnimU::field<void*>(frame,16);
    const auto branch=AnimU::field<const unsigned char*>(frame,32);
    original(context,frame,result);
    for(const auto& site:look_sites)if(site.node==node&&site.branch==branch){
        presentation_look(context,site,frame);
        break;
    }
}
static bool presentation_dead_branch(const unsigned char* code,std::uint32_t live,unsigned terminal){
    return code&&code[0]==0x07&&AnimU::field<std::uint32_t>(code,1)==live&&code[5]==0x01&&
           AnimU::field<void*>(code,6)==local_life.layout.dead&&code[14]==terminal;
}
static bool presentation_prepare_look(void* pawn){
    if(!view_param(world_view_pitch_input,4,"Val",4,0)||!view_param(world_view_yaw_input,4,"Val",4,0)||
       !view_param(world_view_delta,12,"WorldContextObject",8,0)||!view_param(world_view_delta,12,"ReturnValue",4,8))
        return false;
    for(const auto method:{world_view_pitch_multiplier,world_view_yaw_multiplier})
        if(!view_param(method,8,"Mouse",1,0)||!view_param(method,8,"ReturnValue",4,4))return false;
    const auto type=AnimU::field<void*>(pawn,16);
    look_rates={life_offset(type,"BaseLookUpRate",4),life_offset(type,"BaseTurnRate",4),
                life_offset(type,"MouseSensitivity",4)};
    if(std::ranges::min(look_rates)<0)return false;
    const auto pitch=view_script(world_view_pitch,300),yaw=view_script(world_view_yaw,423);
    if(!view_param(world_view_pitch,4,"Pitch",4,0)||!view_param(world_view_yaw,4,"InputPin",4,0)||
       !presentation_dead_branch(pitch.code,19,0x06)||!presentation_dead_branch(yaw.code,19,0x06)||
       AnimU::field<std::uint32_t>(pitch.code,15)!=297||AnimU::field<std::uint32_t>(yaw.code,15)!=420)return false;
    const auto uber=view_script(world_view_uber,113524);
    const auto mouse_pitch=view_script(world_view_mouse_pitch,36),mouse_yaw=view_script(world_view_mouse_yaw,36);
    if(!uber.code||!mouse_pitch.code||!mouse_yaw.code)return false;
    const auto axes=std::array{
        catalog_field(uber.node,"K2Node_InputAxisEvent_AxisValue",4),
        catalog_field(uber.node,"K2Node_InputAxisEvent_AxisValue_1",4)
    };
    constexpr unsigned entries[]{110218,109944};
    const ViewScript wrappers[]{mouse_pitch,mouse_yaw};
    look_sites[0]={pitch.node,pitch.code+1,0,true,false};
    look_sites[1]={yaw.node,yaw.code+1,0,false,false};
    for(unsigned i=0;i<2;++i){
        const auto code=wrappers[i].code;
        const auto entry=entries[i];
        if(!axes[i]||code[0]!=0x64||AnimU::field<void*>(code,1)!=axes[i]||code[18]!=0x46||
           AnimU::field<void*>(code,19)!=uber.node||code[27]!=0x1d||AnimU::field<unsigned>(code,28)!=entry||
           !presentation_dead_branch(uber.code+entry,entry+15,0x4d))return false;
        look_sites[i+2]={uber.node,uber.code+entry+1,*api.offset(axes[i]),i==0,true};
    }
    return true;
}
static void presentation_stop_look(){view_jump.remove(&observe_view_jump);}
static void presentation_down_start(void* pawn,double time){
    down_view={};
    down_view.started=time;
    down_view.yaw=presentation_rotation(pawn)[1];
    down_view.world_camera=presentation_camera_ready(pawn);
    const auto tick=view_script(world_view_tick,2726);
    if(down_view.world_camera&&tick.code&&view_param(world_view_tick,4,"DT",4,0)&&tick.code[0]==0x4c&&
       AnimU::field<unsigned>(tick.code,1)==2723&&tick.code[2722]==0x4d&&tick.code[2723]==0x04&&
       presentation_install_pop())down_view.tick=tick;
    if(!down_view.world_camera){
        auto lowered=local_life.camera_origin;
        lowered[2]-=72.f;
        if(const auto camera=api.weakget(&local_life.camera))life_set_camera(camera,lowered);
        local_life.camera_lowered=true;
    }
    if(presentation_prepare_look(pawn)){
        constexpr unsigned char jump[]{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20};
        view_jump.install(0x07,0x11b6560,jump,&observe_view_jump);
    }
}
static void presentation_down_step(void* pawn,double time){
    const auto camera=api.weakget(&local_life.camera),capsule=api.weakget(&down_view.capsule);
    const auto move=world_input(world_view_move),rotate=world_input(world_view_rotate);
    const auto height=world_input(world_view_capsule);
    if(!down_view.world_camera||!camera||!capsule||!move||!rotate||!height)return;
    const auto rotation=presentation_rotation(pawn);
    const std::array<float,3> facing{0,down_view.yaw,0};
    if(rotation!=facing){
        alignas(16) FactoryParameters args{};
        factory_put(args,0,facing);
        factory_put(args,12,std::uint8_t{1});
        api.event(pawn,rotate,args.data());
    }
    float half_height{};
    api.event(capsule,height,&half_height);
    if(!std::isfinite(half_height)||half_height<=0)return;
    const auto t=std::clamp(static_cast<float>((time-down_view.started)/.72),0.f,1.f);
    const auto eased=t*t*(3.f-2.f*t);
    constexpr float radians=.017453292519943295f;
    const auto yaw=down_view.yaw*radians;
    auto position=native_actor_location(pawn);
    position[0]+=std::cos(yaw)*54.f*eased;
    position[1]+=std::sin(yaw)*54.f*eased;
    position[2]+=std::lerp(local_life.camera_origin[2],20.f-half_height,eased);
    alignas(16) FactoryParameters args{};
    api.event(camera,getters[4].fn,args.data());
    const auto actual=AnimU::field<std::array<float,3>>(args.data(),getters[4].at+16);
    if(std::ranges::equal(actual,position,[](float a,float b){return std::abs(a-b)<.001f;}))return;
    args={};
    factory_put(args,0,position);
    factory_put(args,152,std::uint8_t{1});
    api.event(camera,move,args.data());
    local_life.camera_lowered=true;
    ++presentation_camera_updates;
}
static void presentation_down_poll(void* pawn,double time){
    if(time-down_view.last_tick>.05)presentation_down_step(pawn,time);
}

struct RescueHud {
    Weak actor,widget,material;
    ViewScript highlight,power;
    AnimU::NameKey fill{},active{};
    int player{-1},image{-1};
    std::uint8_t out_mask{};
    float progress{},stock_fill{},stock_active{};
};
static RescueHud rescue_hud;
static bool camera_manager_ready;

static void observe_view_highlight(void* context,void* frame,void* result){
    view_let_bool.original.load(std::memory_order_acquire)(context,frame,result);
    const auto& hud=rescue_hud;
    if(GetCurrentThreadId()!=game_thread.load()||!frame||
       !hud.highlight.code||
       AnimU::field<void*>(frame,16)!=hud.highlight.node||
       AnimU::field<const unsigned char*>(frame,32)!=hud.highlight.code+245||
       api.weakget(&hud.actor)!=context)return;
    const auto locals=AnimU::field<void*>(frame,40);
    auto* output=AnimU::field<std::uint8_t*>(frame,56);
    if(!locals||!output||AnimU::field<void*>(locals,0)!=local_life.actor)return;
    *output|=hud.out_mask;
    ++presentation_highlights;
}
static bool presentation_highlight_ready(){
    auto& hud=rescue_hud;
    const auto controller=api.weakget(&local_life.controller);
    if(!controller||!view_param(world_view_hud,8,"ReturnValue",8,0))return false;
    void* actor{};
    api.event(controller,world_input(world_view_hud),&actor);
    const auto script=view_script(world_view_highlight,248);
    if(!actor||!script.code||!api.isa(actor,AnimU::field<void*>(script.node,32))||
       !view_param(world_view_highlight,9,"Player",8,0)||!view_param(world_view_highlight,9,"Return Value",1,8))
        return false;
    const auto field=catalog_field(script.node,"Return Value",1);
    if(!field||script.code[234]!=0x14||script.code[235]!=0x48||AnimU::field<void*>(script.code,236)!=field||
       script.code[244]!=0x28||script.code[245]!=0x04||AnimU::field<std::uint8_t>(field,121)!=0||
       !makeweak(hud.actor,actor))return false;
    hud.highlight=script;
    hud.out_mask=AnimU::field<std::uint8_t>(field,122);
    return hud.out_mask!=0;
}
static float presentation_read_scalar(void* material,void* read,const AnimU::NameKey& name){
    alignas(16) FactoryParameters args{};
    factory_put(args,0,name);
    api.event(material,read,args.data());
    return AnimU::field<float>(args.data(),8);
}
static void presentation_write_meter(void* material,void* scalar,float fill,float active){
    const auto& hud=rescue_hud;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,hud.fill);
    factory_put(args,8,fill);
    api.event(material,scalar,args.data());
    factory_put(args,0,hud.active);
    factory_put(args,8,active);
    api.event(material,scalar,args.data());
}
static void presentation_restore_meter(){
    auto& hud=rescue_hud;
    const auto material=api.weakget(&hud.material),scalar=world_input(world_view_scalar);
    if(material&&scalar)presentation_write_meter(material,scalar,hud.stock_fill,hud.stock_active);
    hud.material={};
}
static void presentation_power(void* context,void* frame){
    auto& hud=rescue_hud;
    if(GetCurrentThreadId()!=game_thread.load()||hud.progress<=0||!frame||
       AnimU::field<void*>(frame,16)!=hud.power.node||
       AnimU::field<const unsigned char*>(frame,32)!=hud.power.code+2028||
       AnimU::field<void*>(frame,24)!=context||AnimU::field<void*>(context,hud.player)!=local_life.actor)return;
    const auto image=AnimU::field<void*>(context,hud.image);
    const auto dynamic=world_input(world_view_dynamic),scalar=world_input(world_view_scalar);
    const auto read=world_input(world_view_read_scalar);
    if(!image||!dynamic||!scalar||!read)return;
    void* material{};
    api.event(image,dynamic,&material);
    if(!material||!api.isa(material,AnimU::field<void*>(scalar,32)))return;
    if(api.weakget(&hud.material)!=material){
        presentation_restore_meter();
        if(!makeweak(hud.widget,context)||!makeweak(hud.material,material))return;
        hud.stock_fill=presentation_read_scalar(material,read,hud.fill);
        hud.stock_active=presentation_read_scalar(material,read,hud.active);
    }
    presentation_write_meter(material,scalar,hud.progress,hud.progress>=1.f?1.f:0.f);
    ++presentation_power_updates;
}
static void observe_view_pop(void* context,void* frame,void* result){
    view_pop.original.load(std::memory_order_acquire)(context,frame,result);
    const auto& tick=down_view.tick;
    if(context==life_watched_pawn.load(std::memory_order_relaxed)&&frame&&
       GetCurrentThreadId()==game_thread.load()&&local_life.locked&&tick.code&&
       AnimU::field<void*>(frame,16)==tick.node&&AnimU::field<void*>(frame,24)==context&&
       AnimU::field<const unsigned char*>(frame,32)==tick.code+2723){
        down_view.last_tick=now();
        presentation_down_step(context,down_view.last_tick);
        ++presentation_camera_ticks;
    }
    presentation_power(context,frame);
}
static bool presentation_install_pop(){
    constexpr unsigned char pop[]{0x83,0x7a,0x68,0x00,0x4c,0x8b,0xd2};
    return view_pop.install(0x4d,0x11b7860,pop,&observe_view_pop);
}
static void observe_view_pop_if(void* context,void* frame,void* result){
    view_pop_if.original.load(std::memory_order_acquire)(context,frame,result);
    presentation_power(context,frame);
}
static bool presentation_power_ready(){
    auto& hud=rescue_hud;
    const auto script=view_script(world_view_power,2031);
    if(!script.code||*api.parms(script.node)!=0||script.code[0]!=0x4c||
       AnimU::field<unsigned>(script.code,1)!=2028||script.code[2027]!=0x4d||script.code[2028]!=0x04||
       !view_param(world_view_dynamic,8,"ReturnValue",8,0)||
       !view_param(world_view_scalar,12,"ParameterName",8,0)||!view_param(world_view_scalar,12,"Value",4,8)||
       !view_param(world_view_read_scalar,12,"ParameterName",8,0)||
       !view_param(world_view_read_scalar,12,"ReturnValue",4,8)||
       !world_input(world_strings)||!view_param(world_string_name,24,"ReturnValue",8,16))return false;
    const auto type=AnimU::field<void*>(script.node,32);
    hud.player=life_offset(type,"PlayerChar",8);
    hud.image=life_offset(type,"PowerMeterFill",8);
    hud.fill=medic_name(L"ChargeFill");
    hud.active=medic_name(L"ChargeActive");
    hud.power=script;
    return hud.player>=0&&hud.image>=0&&medic_name_is(hud.fill,L"ChargeFill")&&medic_name_is(hud.active,L"ChargeActive");
}
static bool presentation_stop_power(){
    auto& hud=rescue_hud;
    hud.progress=0;
    presentation_restore_meter();
    if(const auto widget=api.weakget(&hud.widget)){
        if(const auto stock=world_input(world_view_power);
           stock&&local_life.actor&&AnimU::field<void*>(widget,hud.player)==local_life.actor)
            api.event(widget,stock,nullptr);
        hud.widget={};
    }
    const bool pop=down_view.tick.node||view_pop.remove(&observe_view_pop);
    const bool pop_if=view_pop_if.remove(&observe_view_pop_if);
    return pop&&pop_if;
}
static void presentation_hud_step(bool aimed,float progress){
    auto& hud=rescue_hud;
    if(aimed){
        if(!view_let_bool.slot&&presentation_highlight_ready()){
            constexpr unsigned char let_bool[]{0x40,0x53,0x55,0x48,0x83,0xec,0x28};
            view_let_bool.install(0x14,0x11b6810,let_bool,&observe_view_highlight);
        }
    }else view_let_bool.remove(&observe_view_highlight);
    if(progress<=0){if(hud.progress>0) presentation_stop_power();return;}
    if(!hud.progress&&presentation_power_ready()){
        constexpr unsigned char pop_if[]{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x42,0x20};
        presentation_install_pop();
        view_pop_if.install(0x4f,0x11b78b0,pop_if,&observe_view_pop_if);
    }
    if(hud.power.code&&(view_pop.slot||view_pop_if.slot))hud.progress=progress;
}
static void presentation_stop_camera(){
    down_view.tick={};
    if(rescue_hud.progress<=0)view_pop.remove(&observe_view_pop);
}
static bool presentation_cleanup(){
    const bool look=view_jump.remove(&observe_view_jump);
    const bool highlight=view_let_bool.remove(&observe_view_highlight);
    down_view.tick={};
    const bool power=presentation_stop_power();
    rescue_hud={};
    down_view={};
    camera_manager_ready=false;
    view_next_scan=0;
    return look&&highlight&&power;
}
