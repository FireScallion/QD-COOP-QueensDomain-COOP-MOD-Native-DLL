struct PresentationTest {
    U::WeakKey widget;
    std::array<float,3> camera_origin{},control_before{},manager_before{},manager_now{},component_now{};
    ULONGLONG down_at{},look_at{},aim_at{};
    unsigned look{},check{};
    float base_fill{},base_active{},sample_fill{},sample_active{},manager_error{},manager_radius{},manager_drop{};
    bool intermediate{},camera_down{},no_highlight{},highlight{},short_fill{},short_reset{},long_fill{},full{};
};
static PresentationTest view_test;

static Address presentation_test_camera(Address pawn){return object_field(pawn,"FirstPersonCamera");}
static std::array<float,3> presentation_test_relative(Address pawn){
    const auto camera=presentation_test_camera(pawn);
    const auto field=camera?field_of(at<Address>(camera,16),"RelativeLocation",12):0;
    return field?at<std::array<float,3>>(camera,at<int>(field,76)):std::array<float,3>{};
}
static std::array<float,3> presentation_test_rotation(Address pawn){
    std::array<float,3> value{};
    event(pawn,resolve(presentation_methods[probe_control]),value.data());
    return value;
}
static bool presentation_test_viewpoint(Address pawn,std::array<float,3>& position){
    const auto fn=resolve(presentation_methods[probe_cam_mgr]);
    alignas(16) Parameters args{};
    if(!put(args,fn,"WorldContextObject",pawn))return false;
    event(resolve(gameplay),fn,args.data());
    const auto camera=U::field<Address>(args.data(),16);
    if(!camera)return false;
    event(camera,resolve(presentation_methods[probe_cam_position]),position.data());
    return true;
}
static void presentation_test_capture(Address pawn){
    view_test.camera_origin=presentation_test_relative(pawn);
    presentation_test_viewpoint(pawn,view_test.manager_before);
}
static bool presentation_test_downed(Address pawn,ULONGLONG time){
    auto& t=view_test;
    if(!t.down_at)t.down_at=time;
    const auto camera=presentation_test_camera(pawn);
    const auto capsule=object_field(pawn,"CapsuleComponent");
    if(!camera||!capsule)return false;
    float half{};
    event(capsule,resolve(presentation_methods[probe_capsule]),&half);
    const auto actor=read_player_pose(pawn);
    const auto eye=component_location(camera);
    const auto elapsed=time-t.down_at;
    const auto final_z=actor.position[2]+20.f-half;
    if(elapsed>=130&&elapsed<450&&eye[2]>final_z+1.f&&eye[2]<actor.position[2]+t.camera_origin[2]-1.f)
        t.intermediate=true;
    if(elapsed>=820){
        constexpr float radians=.017453292519943295f;
        const auto yaw=held_pose.yaw*radians;
        const auto x=actor.position[0]+54.f*std::cos(yaw);
        const auto y=actor.position[1]+54.f*std::sin(yaw);
        std::array<float,3> manager{};
        if(!presentation_test_viewpoint(pawn,manager))return false;
        t.manager_now=manager;
        t.component_now=eye;
        t.manager_error=std::max({std::abs(manager[0]-x),std::abs(manager[1]-y),std::abs(manager[2]-final_z)});
        t.manager_radius=std::hypot(manager[0]-actor.position[0],manager[1]-actor.position[1]);
        t.manager_drop=t.manager_before[2]-manager[2];
        // The stock manager rotates the horizontal offset with control yaw.
        // The camera component itself stays aligned with the downed body.
        t.camera_down=std::abs(eye[0]-x)<.1f&&std::abs(eye[1]-y)<.1f&&std::abs(eye[2]-final_z)<.1f&&
                      std::abs(std::remainder(actor.yaw-held_pose.yaw,360.f))<.02f&&
                      t.manager_drop>50.f&&std::abs(manager[2]-final_z)<.2f&&std::abs(t.manager_radius-54.f)<.5f;
    }
    constexpr unsigned methods[]{probe_mouse_pitch,probe_mouse_yaw,probe_pitch,probe_yaw};
    if(t.look<std::size(methods)){
        const auto axis=t.look%2;
        if(!t.look_at){
            t.control_before=presentation_test_rotation(pawn);
            float input=t.look<2?.8f:.5f;
            event(pawn,resolve(presentation_methods[methods[t.look]]),&input);
            t.look_at=time;
        }else if(time-t.look_at>=60){
            const auto current=presentation_test_rotation(pawn);
            if(std::abs(std::remainder(current[axis]-t.control_before[axis],360.f))<.001f||
               std::abs(std::remainder(current[1-axis]-t.control_before[1-axis],360.f))>.08f)return false;
            ++t.look;
            t.look_at=0;
        }
    }
    t.check=t.look;
    return t.look==std::size(methods)&&t.intermediate&&t.camera_down;
}
static Address presentation_test_hud(Address pawn){
    Address controller{},hud{};
    event(pawn,resolve(rescue_lab_methods[rescue_controller]),&controller);
    if(controller)event(controller,resolve(presentation_methods[probe_hud_get]),&hud);
    return hud;
}
static bool presentation_test_highlight(Address pawn){
    const auto hud=presentation_test_hud(pawn);
    const auto method=resolve(presentation_methods[probe_hud]);
    alignas(16) Parameters args{};
    const auto interaction=object_field(pawn,"PlayerInteractionComponent");
    const auto field=interaction?field_of(at<Address>(interaction,16),"Current Interact Actor",8):0;
    if(!hud||!field||!put(args,method,"Player",pawn))return false;
    // Remove any nearby stock interactable only for this query. Restore the
    // fixture field immediately so its normal interaction still runs.
    const auto address=interaction+at<int>(field,76);
    const auto before=at<Address>(address,0);
    const Address none{};
    std::memcpy(reinterpret_cast<void*>(address),&none,8);
    event(hud,method,args.data());
    std::memcpy(reinterpret_cast<void*>(address),&before,8);
    return args[8]!=0;
}
static U::NameKey presentation_test_name(const wchar_t* name){
    struct StringArgument {const wchar_t* data;int count,capacity;};
    const auto length=static_cast<int>(std::wcslen(name)+1);
    const StringArgument value{name,length,length};
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&value,sizeof(value));
    event(resolve(presentation_methods[probe_strings]),resolve(presentation_methods[probe_name]),args.data());
    return U::field<U::NameKey>(args.data(),16);
}
static bool presentation_test_material(Address pawn,float& fill,float& active){
    auto widget=resolve(view_test.widget);
    if(!widget){
        scan([&](Address object){
            if(!widget&&name(at<Address>(object,16))=="StatusCluster_C"&&object_field(object,"PlayerChar")==pawn){
                widget=object;
                remember(view_test.widget,widget);
            }
        });
        if(!widget)return false;
    }
    const auto image=object_field(widget,"PowerMeterFill");
    const auto getter=resolve(presentation_methods[probe_scalar_get]);
    Address material{};
    if(!image||parameter_offset(getter,"ParameterName",8)!=0||parameter_offset(getter,"ReturnValue",4)!=8)return false;
    // Exercise the real widget update even when the headless role isn't painting.
    event(widget,resolve(presentation_methods[probe_power]),nullptr);
    event(image,resolve(presentation_methods[probe_dynamic]),&material);
    if(!material)return false;
    const auto read=[&](const wchar_t* name){
        alignas(16) Parameters args{};
        const auto key=presentation_test_name(name);
        std::memcpy(args.data(),&key,sizeof(key));
        event(material,getter,args.data());
        return U::field<float>(args.data(),8);
    };
    fill=read(L"ChargeFill");
    active=read(L"ChargeActive");
    view_test.sample_fill=fill;
    view_test.sample_active=active;
    return std::isfinite(fill)&&std::isfinite(active);
}
static bool presentation_test_aim(Address pawn,ULONGLONG time){
    auto& t=view_test;
    if(!t.aim_at){t.aim_at=time;return false;}
    const auto elapsed=time-t.aim_at;
    const bool highlighted=presentation_test_highlight(pawn);
    if(elapsed<500){
        if(!highlighted)t.no_highlight=true;
        t.check=highlighted?5:6;
        return false;
    }
    t.check=highlighted?8:7;
    if(elapsed<620||!t.no_highlight||!highlighted)return false;
    t.highlight=presentation_test_material(pawn,t.base_fill,t.base_active);
    t.check=10;
    return t.highlight;
}
static void presentation_test_charge(Address pawn,ULONGLONG elapsed){
    auto& t=view_test;
    float fill{},active{};
    if(!presentation_test_material(pawn,fill,active))return;
    if(elapsed>=180&&elapsed<550&&fill>.04f&&fill<.32f&&active==0.f)t.short_fill=true;
    if(elapsed>=690&&elapsed<860&&t.short_fill&&std::abs(fill-t.base_fill)<.04f&&active==t.base_active)
        t.short_reset=true;
    if(elapsed>=1500&&elapsed<3200&&fill>.12f&&fill<.95f&&active==0.f)t.long_fill=true;
    if(elapsed>=3500&&fill==1.f&&active==1.f)t.full=true;
    t.check=t.full?14:t.long_fill?13:t.short_reset?12:t.short_fill?11:10;
}
static bool presentation_test_released(Address pawn){
    float fill{},active{};
    const auto& t=view_test;
    return t.highlight&&t.short_fill&&t.short_reset&&t.long_fill&&(!player_medic_lab||t.full)&&
           !presentation_test_highlight(pawn)&&presentation_test_material(pawn,fill,active)&&
           std::abs(fill-t.base_fill)<.04f&&active==t.base_active;
}
static void write_presentation_test(unsigned scenario){
    const auto& t=view_test;
    char text[320]{};
    const auto bytes=std::snprintf(text,sizeof(text),
        "look_paths=%u eased_camera=%u final_camera=%u manager_radius=%.4f manager_drop=%.4f facing_away=%u hover=%u short_fill=%u short_reset=%u long_fill=%u full=%u\n",
        t.look,t.intermediate,t.camera_down,t.manager_radius,t.manager_drop,
        t.no_highlight,t.highlight,t.short_fill,t.short_reset,t.long_fill,t.full);
    if(bytes>0)write_file(marker(role_index,scenario?L"presentation-b.txt":L"presentation-a.txt").c_str(),
                          text,static_cast<unsigned>(bytes));
}
