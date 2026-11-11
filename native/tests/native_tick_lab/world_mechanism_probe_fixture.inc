static std::array<U::WeakKey,2> mechanism_targets;
static U::WeakKey shrine_test_gate;
static constexpr std::string_view mechanism_target_paths[]{
    "/Game/RuinsLevel/Maps/100_GR.100_GR.PersistentLevel.AquaGate_01_BP10",
    "/Game/RuinsLevel/Maps/100_GR.100_GR.PersistentLevel.AquaLever_23"
};
static bool probe_stock_mechanisms(){
    static bool done;
    if(done)return true;
    const auto pawn=local_test_pawn(),fn=resolve(world_actor_methods[1]);
    if(!pawn||!fn||at<std::uint16_t>(fn,182)!=32)return false;
    std::string report;
    constexpr const char* properties[][3]{{"Opened","ScriptOpenable","CurrentActivationCount"},{"ToggledOn",nullptr,nullptr}};
    constexpr const char* methods[][3]{{"OpenDoor","CloseDoor",nullptr},{"AnimateLeverPull","AnimateLeverReset","Restore to switched"}};
    for(unsigned kind=0;kind<mechanism_types.size();++kind){
        const auto type=resolve(mechanism_types[kind]);
        if(!type)return false;
        report+=mechanism_paths[kind];
        report+='\n';
        for(const auto property:properties[kind]){
            if(!property)continue;
            const auto field=U::find_inherited_property(type,image+0x4a27f00,property,read_local);
            if(!field)return false;
            report+=std::string(property)+" "+world_map_property_type(field)+" offset="+
                     std::to_string(at<int>(field,76))+" size="+std::to_string(at<int>(field,60))+"\n";
        }
        for(const auto method:methods[kind]){
            if(!method)continue;
            const auto call=test_world_method(type,method);
            if(!call)return false;
            std::string path;
            U::object_path(call,image+0x4a27f00,read_local,path);
            report+=path+" params="+std::to_string(at<std::uint16_t>(call,182))+" code="+
                     std::to_string(at<int>(call,104))+"\n";
        }
        alignas(16) Parameters args{};
        if(!put(args,fn,"WorldContextObject",pawn)||!put(args,fn,"ActorClass",type))return false;
        event(resolve(gameplay),fn,args.data());
        struct ActorArray { Address* data; int count,capacity; };
        const auto actors=U::field<ActorArray>(args.data(),16);
        report+="live="+std::to_string(actors.count)+"\n";
        const auto guid_field=field_of(type,"GUIDs",16);
        if(actors.data&&actors.count>=0&&actors.count<=8192&&actors.capacity>=actors.count&&guid_field){
            const auto offset=at<int>(guid_field,76);
            for(int i=0;i<actors.count;++i){
                const auto actor=actors.data[i];
                if(!actor)continue;
                const auto keys=at<ActorArray>(actor,offset);
                if(!keys.data||keys.count<=0||keys.count>32||keys.capacity<keys.count)continue;
                std::string path,cls;
                U::object_path(actor,image+0x4a27f00,read_local,path);
                if(world_shrine_gate_lab&&path=="/Game/RuinsLevel/Maps/100_GR.100_GR.PersistentLevel.SwordShrineGate_BP2")
                    remember(shrine_test_gate,actor);
                if(path==mechanism_target_paths[kind]){
                    remember(mechanism_targets[kind],actor);
                    auto method=at<Address>(at<Address>(actor,16),72);
                    for(unsigned j=0;method&&j<1024;++j,method=at<Address>(method,40)){
                        const auto method_name=name(method);
                        if(method_name.find("Open")==std::string::npos&&method_name.find("Close")==std::string::npos&&
                           method_name.find("Lever")==std::string::npos&&method_name.find("Interact")==std::string::npos)continue;
                        report+=cls+"."+method_name+" params="+std::to_string(at<std::uint16_t>(method,182))+"\n";
                    }
                }
                U::object_path(at<Address>(actor,16),image+0x4a27f00,read_local,cls);
                const auto key=*reinterpret_cast<const TestWP::Guid*>(keys.data);
                const auto pos=location(actor);
                report+=path+" class="+cls+" key=";
                for(const auto part:key)report+=std::to_string(part)+",";
                report+=" xyz="+std::to_string(pos[0])+","+std::to_string(pos[1])+","+std::to_string(pos[2]);
                for(const auto property:properties[kind]){
                    if(!property)continue;
                    const auto field=U::find_inherited_property(at<Address>(actor,16),image+0x4a27f00,property,read_local);
                    if(!field)continue;
                    const auto value=world_map_property_type(field)=="BoolProperty"?
                        static_cast<int>(U::get_bool(reinterpret_cast<void*>(field),reinterpret_cast<void*>(actor),0)):
                        at<int>(actor,at<int>(field,76));
                    report+=" "+std::string(property)+"="+std::to_string(value);
                }
                report+="\n";
            }
        }
        if(actors.data)reinterpret_cast<void(*)(void*)>(image+0xeae0b0)(actors.data);
    }
    if(world_mechanism_lab&&std::ranges::any_of(mechanism_targets,[](const auto& key){return !resolve(key);}))return false;
    if(world_shrine_gate_lab&&!resolve(shrine_test_gate))return false;
    write_file(marker(role_index,L"world-mechanism-layout.txt").c_str(),report.data(),static_cast<unsigned>(report.size()));
    done=true;
    return true;
}
static float mechanism_timeline_position(Address actor){
    const auto fn=resolve(mechanism_timeline[0]),type=resolve(mechanism_timeline[1]);
    const auto position=resolve(world_actor_methods[4]);
    alignas(16) Parameters args{};
    if(!fn||!type||!position||at<std::uint16_t>(fn,182)!=24||
       !put(args,fn,"ComponentClass",type)||parameter_offset(fn,"ReturnValue",16)!=8)return -1.f;
    event(actor,fn,args.data());
    struct ComponentArray { Address* data; int count,capacity; };
    const auto components=U::field<ComponentArray>(args.data(),8);
    float best=-1.f;
    if(components.data&&components.count>0&&components.count<=32&&components.capacity>=components.count){
        for(int i=0;i<components.count;++i){
            float value{};
            if(!components.data[i])continue;
            event(components.data[i],position,&value);
            best=std::max(best,value);
        }
    }
    if(components.data)reinterpret_cast<void(*)(void*)>(image+0xeae0b0)(components.data);
    return best;
}
struct MechanismShape {Address component{};std::array<float,3> pos{},rot{};};
static bool mechanism_shape(Address actor,std::vector<MechanismShape>& out){
    const auto fn=resolve(mechanism_timeline[0]),type=resolve(mechanism_timeline[2]);
    const auto location=resolve(mechanism_timeline[3]),rotation=resolve(mechanism_timeline[4]);
    alignas(16) Parameters args{};
    if(!fn||!type||!location||!rotation||at<std::uint16_t>(fn,182)!=24||
       at<std::uint16_t>(location,182)!=12||at<std::uint16_t>(rotation,182)!=12||
       !put(args,fn,"ComponentClass",type)||parameter_offset(fn,"ReturnValue",16)!=8)return false;
    event(actor,fn,args.data());
    struct ComponentArray { Address* data; int count,capacity; };
    const auto components=U::field<ComponentArray>(args.data(),8);
    const bool valid=components.data&&components.count>0&&components.count<=64&&components.capacity>=components.count;
    if(valid)for(int i=0;i<components.count;++i){
        const auto component=components.data[i];
        if(!component)continue;
        MechanismShape entry{component,{},{}};
        event(component,location,entry.pos.data());
        event(component,rotation,entry.rot.data());
        out.push_back(entry);
    }
    if(components.data)reinterpret_cast<void(*)(void*)>(image+0xeae0b0)(components.data);
    return valid&&!out.empty();
}
static float mechanism_motion(const std::vector<MechanismShape>& original,const std::vector<MechanismShape>& current){
    float distance=0.f;
    for(const auto& entry:current){
        const auto prior=std::ranges::find(original,entry.component,&MechanismShape::component);
        if(prior==original.end())continue;
        for(unsigned axis=0;axis<3;++axis){
            distance=std::max(distance,std::abs(entry.pos[axis]-prior->pos[axis]));
            distance=std::max(distance,std::abs(std::remainder(entry.rot[axis]-prior->rot[axis],360.f)));
        }
    }
    return distance;
}
static bool run_world_mechanism_stage(){
    static unsigned stage;
    static bool changed;
    static ULONGLONG close_at;
    static std::vector<MechanismShape> closed;
    static float peak_motion;
    if(stage==3)return all_player_markers(L"world-mechanism-closed.txt");
    if(!probe_stock_mechanisms())return false;
    const auto door=resolve(mechanism_targets[0]),lever=resolve(mechanism_targets[1]);
    const auto type=resolve(mechanism_types[0]),lever_type=resolve(mechanism_types[1]);
    if(!door||!lever||!type||!lever_type)return false;
    const auto opened=field_of(type,"Opened",1),toggle=field_of(lever_type,"ToggledOn",1);
    const auto count=field_of(type,"CurrentActivationCount",4),script=field_of(type,"ScriptOpenable",1);
    if(!opened||!toggle||!count||!script)return false;
    const auto is=[](Address field,Address actor){
        return U::get_bool(reinterpret_cast<void*>(field),reinterpret_cast<void*>(actor),0);
    };
    const auto count_at=at<int>(count,76);
    const auto activation=at<int>(door,count_at);
    if(stage==0){
        if(is(opened,door)||is(toggle,lever)||activation||!is(script,door)||!mechanism_shape(door,closed))return false;
        write_file(marker(role_index,L"world-mechanism-ready.txt").c_str(),"ok",2);
        ++stage;
        return false;
    }
    const bool open=stage==1;
    if(!all_player_markers(open?L"world-mechanism-ready.txt":L"world-mechanism-open.txt"))return false;
    if(!open&&is(opened,door)){
        std::vector<MechanismShape> current;
        if(mechanism_shape(door,current))peak_motion=std::max(peak_motion,mechanism_motion(closed,current));
    }
    if(host&&!changed){
        if(!open){
            if(!close_at)close_at=GetTickCount64()+1000;
            if(GetTickCount64()<close_at)return false;
        }
        const auto door_event=test_world_method(at<Address>(door,16),open?"OpenDoor":"CloseDoor");
        const auto lever_event=test_world_method(at<Address>(lever,16),open?"AnimateLeverPull":"AnimateLeverReset");
        if(!door_event||!lever_event)return false;
        const int desired=open?2:1;
        std::memcpy(reinterpret_cast<void*>(door+count_at),&desired,sizeof(desired));
        U::set_bool(reinterpret_cast<void*>(opened),reinterpret_cast<void*>(door),open,0);
        U::set_bool(reinterpret_cast<void*>(toggle),reinterpret_cast<void*>(lever),open,0);
        event(door,door_event,nullptr);
        event(lever,lever_event,nullptr);
        changed=true;
        return false;
    }
    if(is(opened,door)!=open||is(toggle,lever)!=open||activation!=(open?2:1))return false;
    const auto door_time=mechanism_timeline_position(door),lever_time=mechanism_timeline_position(lever);
    std::vector<MechanismShape> current;
    if(!mechanism_shape(door,current))return false;
    const auto motion=mechanism_motion(closed,current);
    if(open?(motion<.5f||door_time<.005f||lever_time<.005f):
            (peak_motion<10.f||motion>.2f||door_time<0.f||door_time>.03f||lever_time<0.f||lever_time>.03f))return false;
    char report[200]{};
    const auto size=std::snprintf(report,sizeof(report),"opened=%u toggled=%u count=%d door_timeline=%.4f lever_timeline=%.4f door_motion=%.4f peak_door_motion=%.4f\n",
                                  static_cast<unsigned>(open),static_cast<unsigned>(open),activation,door_time,lever_time,motion,peak_motion);
    if(size>0)write_file(marker(role_index,open?L"world-mechanism-open.txt":L"world-mechanism-closed.txt").c_str(),
                        report,static_cast<unsigned>(size));
    ++stage;
    changed=false;
    return false;
}
