// The fixture verifies the resolver's selected profile and the actual playback.
static constexpr unsigned fixture_profile=31;
static constexpr const char* anim_class_path=
    "/Game/Enemies/Lizer_00/Default_Lizer_AnimBP.Default_Lizer_AnimBP_C";
static constexpr const char* montage_path=
    "/Game/Enemies/Lizer_00/Animations/Lizer_Stagger_Montage.Lizer_Stagger_Montage";
enum AnimMethod {get_instance,current_montage,play_montage,stop_montage,montage_position,
                 montage_rate,anim_method_count};
static constexpr const char* anim_paths[]{
    "/Script/Engine.SkeletalMeshComponent.GetAnimInstance",
    "/Script/Engine.AnimInstance.GetCurrentActiveMontage",
    "/Script/Engine.AnimInstance.Montage_Play",
    "/Script/Engine.AnimInstance.Montage_Stop",
    "/Script/Engine.AnimInstance.Montage_GetPosition",
    "/Script/Engine.AnimInstance.Montage_GetPlayRate"
};
static std::array<U::WeakKey,anim_method_count> animation_methods{};
static U::WeakKey montage_asset,fixture_instance;
static unsigned animation_fixture_step,animation_methods_found;
static unsigned catalog_profile,catalog_scan_slices;
static bool catalog_scan_ready;
static double catalog_scan_max_us;
static bool animation_started,animation_stopped,animation_first,animation_second,animation_advanced;
static Address marker_aim_field,marker_sleeping_field;
static float montage_first_position,montage_last_position,montage_observed_rate,aim_observed;
static constexpr float first_aim=31.25f,second_aim=-23.75f,fixture_rate=.35f;

static void discover_animation(Address object,std::string_view leaf){
    if(leaf=="Lizer_Stagger_Montage"&&name(at<Address>(object,16))=="AnimMontage"&&
       exact_path(object,montage_path))remember(montage_asset,object);
    for(unsigned i=0;i<animation_methods.size();++i){
        const std::string_view path=anim_paths[i];
        if(leaf==path.substr(path.find_last_of('.')+1)&&name(at<Address>(object,16))=="Function"&&
           exact_path(object,anim_paths[i]))remember(animation_methods[i],object);
    }
}

static bool animation_inputs_ready(){
    animation_methods_found=0;
    for(const auto& method:animation_methods)animation_methods_found+=resolve(method)!=0;
    return animation_methods_found==anim_method_count&&resolve(montage_asset);
}

static bool scan_fixture_catalog(){
    if(catalog_scan_ready)return true;
    LARGE_INTEGER begin{},end{};
    QueryPerformanceCounter(&begin);
    const auto result=core.scan_animation(2048);
    QueryPerformanceCounter(&end);
    ++catalog_scan_slices;
    catalog_scan_max_us=std::max(catalog_scan_max_us,static_cast<double>(end.QuadPart-begin.QuadPart)*counter_us);
    if(result<0)phase=42;
    catalog_scan_ready=result==1;
    return catalog_scan_ready;
}

static Address mesh_anim_instance(Address mesh){
    const auto function=resolve(animation_methods[get_instance]);
    if(!mesh||!function||parameter_offset(function,"ReturnValue",8)!=0)return 0;
    Address instance{};
    event(mesh,function,&instance);
    return instance;
}

static bool bind_fixture_animation(Address mesh){
    animation_fixture_step=1;
    if(core.bind_catalog_animation(1,2))return false;
    catalog_profile=core.bind_catalog_animation(1,1);
    if(catalog_profile!=fixture_profile)return false;
    animation_fixture_step=2;
    const auto source=mesh_anim_instance(mesh);
    if(!source)return false;
    const auto type=at<Address>(source,16);
    if(!exact_path(type,anim_class_path))return false;
    animation_fixture_step=3;
    auto target=source;
    if(!host){
        const auto visual=resolve(display);
        const auto pose=visual?object_field(visual,"SkeletalMeshComponent"):0;
        target=mesh_anim_instance(pose);
        if(!target||target==source||at<Address>(target,16)!=type)return false;
    }

    animation_fixture_step=4;
    marker_aim_field=field_of(type,"CurrentAimAngle",4);
    marker_sleeping_field=field_of(type,"Sleeping",1);
    if(!marker_aim_field||!marker_sleeping_field)return false;
    remember(fixture_instance,target);
    animation_fixture_step=5;
    return true;
}

static bool set_fixture_animation_markers(float aim,bool sleeping){
    const auto instance=resolve(fixture_instance);
    if(!instance)return false;
    std::memcpy(reinterpret_cast<void*>(instance+at<unsigned>(marker_aim_field,76)),&aim,4);
    U::set_bool(reinterpret_cast<void*>(marker_sleeping_field),reinterpret_cast<void*>(instance),sleeping,0);
    return true;
}

static bool start_fixture_montage(){
    const auto instance=resolve(fixture_instance),asset=resolve(montage_asset);
    const auto function=resolve(animation_methods[play_montage]);
    if(!instance||!asset||!function)return false;
    alignas(16) Parameters args{};
    const auto result=parameter_offset(function,"ReturnValue",4);
    if(result<0||!put(args,function,"MontageToPlay",asset)||!put(args,function,"InPlayRate",fixture_rate)||
       !put(args,function,"bStopAllMontages",std::uint8_t{1}))return false;
    event(instance,function,args.data());
    float duration{};
    std::memcpy(&duration,args.data()+result,4);
    animation_started=std::isfinite(duration)&&duration>0;
    return animation_started;
}

static bool stop_fixture_montage(){
    const auto instance=resolve(fixture_instance),asset=resolve(montage_asset);
    const auto current=resolve(animation_methods[current_montage]);
    const auto stop=resolve(animation_methods[stop_montage]);
    if(!instance||!asset||!current||!stop)return false;
    Address active{};
    event(instance,current,&active);
    if(active!=asset)return false;
    alignas(16) Parameters args{};
    std::memcpy(args.data()+8,&asset,sizeof(asset));
    event(instance,stop,args.data());
    active=0;
    event(instance,current,&active);
    animation_stopped=!active;
    return animation_stopped;
}

static float montage_value(Address instance,Address asset,AnimMethod method){
    const auto function=resolve(animation_methods[method]);
    if(!function||at<std::uint16_t>(function,182)!=12)return -1;
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&asset,sizeof(asset));
    event(instance,function,args.data());
    float value{};
    std::memcpy(&value,args.data()+8,4);
    return value;
}

static void sample_fixture_animation(){
    const auto instance=resolve(fixture_instance),asset=resolve(montage_asset);
    const auto current=resolve(animation_methods[current_montage]);
    if(!instance||!asset||!current)return;
    aim_observed=at<float>(instance,at<unsigned>(marker_aim_field,76));
    const auto sleeping=get_flag(instance,marker_sleeping_field);
    animation_first|=std::abs(aim_observed-first_aim)<.001f&&sleeping;
    animation_second|=std::abs(aim_observed-second_aim)<.001f&&!sleeping;
    Address active{};
    event(instance,current,&active);
    if(active!=asset)return;
    const auto rate=montage_value(instance,asset,montage_rate);
    const auto position=montage_value(instance,asset,montage_position);
    if(std::abs(rate-fixture_rate)>.001f||position<=0.f)return;
    montage_observed_rate=rate;
    if(!montage_first_position)montage_first_position=position;
    montage_last_position=position;
    animation_advanced|=position-montage_first_position>.02f;
}

static bool fixture_montage_stopped(){
    const auto instance=resolve(fixture_instance);
    const auto current=resolve(animation_methods[current_montage]);
    if(!instance||!current)return false;
    Address active{};
    event(instance,current,&active);
    return !active;
}
