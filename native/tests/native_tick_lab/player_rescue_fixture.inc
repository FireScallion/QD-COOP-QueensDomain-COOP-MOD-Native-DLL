struct RescueScenario {
    unsigned target,donor;
    const wchar_t* ready;
    const wchar_t* down;
    const wchar_t* rising;
    const wchar_t* done;
};
static constexpr RescueScenario rescue_scenarios[]{
    {0,1,L"rescue-a-ready.txt",L"rescue-a-down.txt",L"rescue-a-rise.txt",L"rescue-a-ok.txt"},
    {2,0,L"rescue-b-ready.txt",L"rescue-b-down.txt",L"rescue-b-rise.txt",L"rescue-b-ok.txt"}
};
static unsigned rescue_case,rescue_check;
static bool rescue_prepared,rescue_fired,rescue_saw_down,rescue_saw_rising,rescue_case_done;
static ULONGLONG rescue_hold_since;
static TestPlayerPose rescue_target_pose;
static Address rescue_instance;
static U::WeakKey rescue_save,rescue_selected;
static int rescue_selection_at=-1;
static int rescue_suspend_before;
static int rescue_medicine_before;
static bool rescue_damage_before,rescue_yaw_before;
#include "player_presentation_fixture.inc"

static bool rescue_bool(Address object,const char* name){
    const auto property=field_of(at<Address>(object,16),name,1);
    return property&&U::get_bool(reinterpret_cast<void*>(property),reinterpret_cast<void*>(object),0);
}

static void restore_rescue_fixture_selection(){
    if(rescue_selection_at<0)return;
    if(const auto save=resolve(rescue_save)){
        const auto selected=resolve(rescue_selected);
        std::memcpy(reinterpret_cast<void*>(save+rescue_selection_at),&selected,sizeof(selected));
    }
    rescue_selection_at=-1;
}

static bool prepare_free_rescue(){
    const auto save=object_field(rescue_instance,"ActiveSaveSlot");
    const auto selected=save?field_of(at<Address>(save,16),"SelectedConsumable",8):0;
    if(!selected)return false;
    rescue_selection_at=at<int>(selected,76);
    const auto old=at<Address>(save,rescue_selection_at);
    remember(rescue_save,save);
    if(old)remember(rescue_selected,old);
    else rescue_selected={};
    const auto original=old?name(old):"none";
    write_file(marker(role_index,L"rescue-selection.txt").c_str(),original.data(),static_cast<unsigned>(original.size()));
    const Address none{};
    std::memcpy(reinterpret_cast<void*>(save+rescue_selection_at),&none,sizeof(none));
    return true;
}
static int rescue_medicine_count(){
    const auto save=object_field(rescue_instance,"ActiveSaveSlot");
    const auto selected=save?object_field(save,"SelectedConsumable"):0;
    const auto fn=resolve(rescue_inventory_count);
    if(!selected||name(selected)!="HealingPotion"||!fn||at<std::uint16_t>(fn,182)!=12||
       parameter_offset(fn,"Item",8)!=0||parameter_offset(fn,"ReturnValue",4)!=8)return -1;
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&selected,8);
    event(rescue_instance,fn,args.data());
    return U::field<int>(args.data(),8);
}
static bool rescue_replay_check(){
    qdcoop::rescue::BindingView medic{};
    if(!core.inspect_medic(&medic)||!medic.identity.valid()||medic.outgoing||medic.errors||
       medic.debits||medic.revives||medic.refunds!=static_cast<unsigned>(player_medic_refund&&role_index==1))return false;
    const int count=rescue_medicine_count();
    if(count!=(!player_medic_refund&&role_index<2?10:11))return false;
    char evidence[160]{};
    const auto size=std::snprintf(evidence,sizeof(evidence),"count=%d generation=%llu new_debits=0 new_revives=0 refunds=%llu\n",
                                 count,static_cast<unsigned long long>(medic.generation),
                                 static_cast<unsigned long long>(medic.refunds));
    if(size>0)write_file(marker(role_index,L"medic-replay.txt").c_str(),evidence,static_cast<unsigned>(size));
    return true;
}

static void rescue_test_input(Address pawn,ULONGLONG time){
    if(player_test_stage!=4||rescue_case>=std::size(rescue_scenarios)||
       role_index!=rescue_scenarios[rescue_case].donor||!rescue_prepared)return;
    const auto held=field_of(at<Address>(pawn,16),"InteractHeld",1);
    if(!held)return;
    const auto elapsed=rescue_hold_since?time-rescue_hold_since:0;
    const bool press=(elapsed>0&&elapsed<600)||(elapsed>900&&elapsed<8500);
    U::set_bool(reinterpret_cast<void*>(held),reinterpret_cast<void*>(pawn),press,0);
    if(!press&&(!presentation_lab||!view_test.aim_at))return;
    Address controller{};
    event(pawn,resolve(rescue_lab_methods[rescue_controller]),&controller);
    const auto camera=object_field(pawn,"FirstPersonCamera");
    if(!controller||!camera)return;
    const auto eye=component_location(camera);
    const auto dx=rescue_target_pose.position[0]-eye[0],dy=rescue_target_pose.position[1]-eye[1];
    const auto dz=rescue_target_pose.position[2]-62.f-eye[2];
    constexpr float degrees=57.29577951308232f;
    std::array<float,3> view{std::atan2(dz,std::hypot(dx,dy))*degrees,std::atan2(dy,dx)*degrees,0};
    if(presentation_lab&&time-view_test.aim_at<500)view[1]+=180.f;
    event(controller,resolve(rescue_lab_methods[rescue_control]),view.data());
}

static bool rescue_remote_animation(unsigned target,const char* clip,float weight){
    const auto actor=resolve(avatar_keys[target]);
    const auto component=actor?object_field(actor,"SkeletalMeshComponent"):0;
    const auto instance=test_anim_instance(component);
    if(!instance)return false;
    const auto field=field_of(at<Address>(instance,16),"LifeWeight",4);
    const auto asset=object_field(instance,"LifeSequence");
    return field&&asset&&name(asset)==clip&&at<float>(instance,at<int>(field,76))>weight;
}

static bool run_rescue_stage(ULONGLONG time){
    if(rescue_case==std::size(rescue_scenarios))return true;
    const auto& scenario=rescue_scenarios[rescue_case];
    const auto pawn=resolve(held_pawn);
    const auto health=pawn?field_of(at<Address>(pawn,16),"Health",4):0;
    const auto suspend=rescue_instance?field_of(at<Address>(rescue_instance,16),"SuspendIsBlocked",4):0;
    P::RescueView view{};
    const auto layout=core.inspect_rescue(&view);
    const auto hp=health?at<float>(pawn,at<int>(health,76)):-1.f;
    const auto lock=suspend?at<int>(rescue_instance,at<int>(suspend,76)):-1;
    qdcoop::rescue::BindingView medic{};
    if(player_medic_lab&&!core.inspect_medic(&medic))return false;
    char status[1000]{};
    const auto count=std::snprintf(status,sizeof(status),
        "case=%u check=%u layout=%d phase=%u armed=%u locked=%u target=%u hold=%.3f hp=%.3f saves=%d gate=%u med=%u out=%u bind=%u gen=%llu debits=%llu revived=%llu error=%llu view=%u ease=%u camera=%u away=%u hover=%u fill=%.3f active=%.3f manager_error=%.3f manager=(%.2f,%.2f,%.2f) component=(%.2f,%.2f,%.2f) before_z=%.2f aim_ms=%llu short=%u reset=%u long=%u full=%u base=(%.3f,%.3f)\n",
        rescue_case,rescue_check,layout,static_cast<unsigned>(view.life.phase),view.armed,view.locked,
        view.target,view.progress,hp,lock,view.hold_stage,medic.phase,medic.outgoing,medic.bind_check,
        static_cast<unsigned long long>(medic.generation),static_cast<unsigned long long>(medic.debits),
        static_cast<unsigned long long>(medic.revives),static_cast<unsigned long long>(medic.errors),
        view_test.check,view_test.intermediate,view_test.camera_down,view_test.no_highlight,view_test.highlight,
        view_test.sample_fill,view_test.sample_active,view_test.manager_error,
        view_test.manager_now[0],view_test.manager_now[1],view_test.manager_now[2],
        view_test.component_now[0],view_test.component_now[1],view_test.component_now[2],view_test.manager_before[2],
        static_cast<unsigned long long>(view_test.aim_at?time-view_test.aim_at:0),
        view_test.short_fill,view_test.short_reset,view_test.long_fill,view_test.full,view_test.base_fill,view_test.base_active);
    if(count>0)
        write_file(marker(role_index,L"rescue-progress.txt").c_str(),status,static_cast<unsigned>(count));
    if(rescue_case_done){
        if(!all_player_markers(scenario.done))return false;
        ++rescue_case;
        view_test={};
        rescue_prepared=rescue_fired=rescue_saw_down=rescue_saw_rising=rescue_case_done=false;
        rescue_check=0;
        return rescue_case==std::size(rescue_scenarios);
    }
    if(!pawn||!health||!suspend||layout!=8)return false;
    if(!rescue_prepared){
        if(!view.armed||(player_medic_lab&&!medic.identity.valid())||!read_expected_pose(scenario.target,1,rescue_target_pose))return false;
        if(role_index==scenario.donor){
            if(player_medic_lab){
                rescue_medicine_before=rescue_medicine_count();
                if(rescue_medicine_before!=11){rescue_check=10;return false;}
            }else if(!prepare_free_rescue()){rescue_check=9;return false;}
            held_pose.position=rescue_target_pose.position;
            held_pose.position[0]-=125.f;
            write_file(marker(role_index,scenario.ready).c_str(),"ok",2);
        }
        if(role_index==scenario.target){
            rescue_suspend_before=lock;
            rescue_damage_before=rescue_bool(pawn,"bCanBeDamaged");
            rescue_yaw_before=rescue_bool(pawn,"bUseControllerRotationYaw");
            if(presentation_lab)presentation_test_capture(pawn);
        }
        rescue_prepared=true;
        return false;
    }
    if(!exists(marker(scenario.donor,scenario.ready)))return false;
    if(role_index==scenario.target&&!rescue_fired){
        rescue_check=1;
        if(!view.armed)return false;
        float one=1.f;
        event(pawn,resolve(rescue_lab_methods[rescue_health]),&one);
        std::uint8_t crushing{1};
        event(pawn,resolve(rescue_lab_methods[rescue_pit]),&crushing);
        rescue_fired=true;
        return false;
    }
    if(!rescue_saw_down){
        rescue_check=2;
        if(role_index==scenario.target){
            if(view.life.phase!=P::LifePhase::down||!view.life.token||!view.locked||hp!=0||
               lock!=rescue_suspend_before+1||!rescue_bool(pawn,"IsDead")||rescue_bool(pawn,"bCanBeDamaged"))
                return false;
            if(presentation_lab&&!presentation_test_downed(pawn,time))return false;
        }else if(!rescue_remote_animation(scenario.target,"QD_DownedEnter_Proof",.5f))return false;
        rescue_saw_down=true;
        write_file(marker(role_index,scenario.down).c_str(),"ok",2);
    }
    if(!all_player_markers(scenario.down))return false;
    if(role_index==scenario.donor){
        if(!rescue_hold_since){
            if(presentation_lab&&!presentation_test_aim(pawn,time))return false;
            rescue_hold_since=time;
        }else if(presentation_lab)presentation_test_charge(pawn,time-rescue_hold_since);
    }
    if(player_medic_interrupt){
        constexpr auto marker_name=L"medic-interrupted.txt";
        if(role_index==scenario.target&&medic.ready_sent)player_silence=true;
        if(role_index==scenario.donor&&medic.debits==1&&
           medic.outgoing==static_cast<unsigned>(qdcoop::rescue::Debit::persisted)&&rescue_medicine_count()==10)
            write_file(marker(role_index,marker_name).c_str(),"durable debit",13);
        if(!exists(marker(scenario.donor,marker_name)))return false;
        if(role_index==scenario.target&&(!player_silence||!view.locked||hp!=0))return false;
        write_file(marker(role_index,L"medic-interrupt-ok.txt").c_str(),"ok",2);
        return all_player_markers(L"medic-interrupt-ok.txt");
    }
    if(!rescue_saw_rising){
        rescue_check=3;
        if(role_index==scenario.target){
            if(view.life.phase!=P::LifePhase::rising||view.locked||(player_medic_lab?hp<=1:hp!=1)||
               lock!=rescue_suspend_before||rescue_bool(pawn,"IsDead")||
               rescue_bool(pawn,"bCanBeDamaged")!=rescue_damage_before||
               rescue_bool(pawn,"bUseControllerRotationYaw")!=rescue_yaw_before)return false;
            if(presentation_lab&&!std::ranges::equal(presentation_test_relative(pawn),view_test.camera_origin,
                    [](float a,float b){return std::abs(a-b)<.02f;}))return false;
        }else if(!rescue_remote_animation(scenario.target,"QD_GetUp_Proof",.2f))return false;
        if(role_index==scenario.donor&&time-rescue_hold_since<3500){phase=96;return false;}
        rescue_saw_rising=true;
        write_file(marker(role_index,scenario.rising).c_str(),"ok",2);
    }
    if(!all_player_markers(scenario.rising))return false;
    if(player_medic_lab){
        if(role_index==scenario.donor&&(medic.outgoing||medic.debits!=1||rescue_medicine_count()!=rescue_medicine_before-1))
            return false;
        if(role_index==scenario.target&&medic.revives!=1)return false;
    }
    if(presentation_lab){
        if(role_index==scenario.donor&&!presentation_test_released(pawn))return false;
        write_presentation_test(rescue_case);
    }
    if(role_index==scenario.donor){
        restore_rescue_fixture_selection();
        rescue_hold_since=0;
        const auto held=field_of(at<Address>(pawn,16),"InteractHeld",1);
        U::set_bool(reinterpret_cast<void*>(held),reinterpret_cast<void*>(pawn),false,0);
        held_pose=moved_poses[role_index];
    }
    write_file(marker(role_index,scenario.done).c_str(),"ok",2);
    rescue_case_done=true;
    return false;
}
