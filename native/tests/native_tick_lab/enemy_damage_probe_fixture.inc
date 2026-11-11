static void damage_layout_fields(std::string& report,Address owner,bool params){
    auto field=at<Address>(owner,80);
    for(unsigned i=0;field&&i<1024;++i,field=at<Address>(field,32)){
        if(params&&!(at<std::uint64_t>(field,64)&0x80u))continue;
        std::string name;
        if(!U::name_string(image+0x4a27f00,at<U::NameKey>(field,40),read_local,name))continue;
        const auto type=world_map_property_type(field);
        report+=" "+name+" "+type+" at="+std::to_string(at<int>(field,76))+" size="+std::to_string(at<int>(field,60));
        char flags[32]{};
        std::snprintf(flags,sizeof(flags),"0x%llx",static_cast<unsigned long long>(at<std::uint64_t>(field,64)));
        report+=" flags="+std::string(flags);
        if(type=="StructProperty"){
            std::string path;
            U::object_path(at<Address>(field,0x78),image+0x4a27f00,read_local,path);
            report+=" struct="+path;
        }else if(type=="ArrayProperty"){
            const auto inner=at<Address>(field,0x78);
            report+=" inner="+world_map_property_type(inner)+"/"+std::to_string(inner?at<int>(inner,60):0);
        }else if(type=="MapProperty"){
            const auto key=at<Address>(field,0x78),value=at<Address>(field,0x80);
            report+=" key="+world_map_property_type(key)+"/"+std::to_string(key?at<int>(key,60):0)+
                    " value="+world_map_property_type(value)+"/"+std::to_string(value?at<int>(value,60):0)+" layout=";
            for(const auto n:at<std::array<int,6>>(field,0x88))report+=std::to_string(n)+",";
        }
        report+="\n";
    }
}
static bool probe_stock_damage(){
    static bool done;
    if(done)return true;
    std::string report;
    for(unsigned i=0;i<damage_probe_methods.size();++i){
        const auto object=resolve(damage_probe_methods[i]);
        if(!object)return false;
        const bool fn=i<2||i==3||i==6;
        report+=damage_probe_paths[i];
        report+=" properties="+std::to_string(at<int>(object,88));
        if(fn)report+=" parms="+std::to_string(at<std::uint16_t>(object,182))+" bytes="+std::to_string(at<int>(object,104));
        report+="\n";
        if(i!=7)damage_layout_fields(report,object,fn);
    }
    const auto enemy=resolve(damage_probe_methods[0]);
    const auto field=field_of(enemy,"DamageToApply",216);
    if(!field)return false;
    const auto slot=at<Address>(field,0);
    if(!slot)return false;
    report+="FStructProperty vtable\n";
    for(unsigned i=0;i<84;++i){
        const auto address=at<Address>(slot,i*8);
        char line[48]{};
        std::snprintf(line,sizeof(line),"  [%u] 0x%llx\n",i,static_cast<unsigned long long>(address-image));
        report+=line;
    }
    const auto code=at<const unsigned char*>(enemy,96);
    if(!code||at<int>(enemy,104)!=9543)return false;
    char guard[180]{};
    std::snprintf(guard,sizeof(guard),"script0=%02x target=%u terminal=%02x/%02x push_exec=0x%llx\n",
        static_cast<unsigned>(code[0]),U::field<std::uint32_t>(code,1),static_cast<unsigned>(code[9532]),
        static_cast<unsigned>(code[9533]),static_cast<unsigned long long>(at<Address>(image+0x4a62e10,0x4c*8)-image));
    report+=guard;
    write_file(marker(role_index,L"stock-damage-layout.txt").c_str(),report.data(),static_cast<unsigned>(report.size()));
    done=true;
    return true;
}
static Address damage_probe_target(Address pawn,Address expected){
    const auto fn=resolve(damage_probe_methods[6]),type=resolve(damage_probe_methods[7]);
    alignas(16) Parameters args{};
    if(!fn||!type||at<std::uint16_t>(fn,182)!=32||!put(args,fn,"WorldContextObject",pawn)||
       !put(args,fn,"ActorClass",type))return 0;
    event(resolve(gameplay),fn,args.data());
    struct Actors {Address* data;int count,capacity;};
    const auto actors=U::field<Actors>(args.data(),16);
    const auto from=location(pawn);
    float closest=6000.f*6000.f;
    Address found{};
    if(actors.data&&actors.count>0&&actors.count<=8192&&actors.capacity>=actors.count){
        for(int i=0;i<actors.count;++i){
            const auto actor=actors.data[i];
            if(!actor)continue;
            const auto cls=at<Address>(actor,16),hp=field_of(cls,"HP",4);
            if(!hp||at<float>(actor,at<int>(hp,76))<20.f||test_world_method(cls,"TryApplyDamage")!=expected)continue;
            const auto point=location(actor);
            float d{};
            for(unsigned axis=0;axis<3;++axis){const auto delta=point[axis]-from[axis];d+=delta*delta;}
            if(d<closest){closest=d;found=actor;}
        }
    }
    if(actors.data)reinterpret_cast<void(*)(void*)>(image+0xeae0b0)(actors.data);
    return found;
}
static bool damage_probe_struct(Address property){
    if(!property||at<int>(property,56)!=1||world_map_property_type(property)!="StructProperty")return false;
    const auto vtable=at<Address>(property,0);
    return vtable&&at<Address>(vtable,23*8)==image+0x118e4b0&&
           at<Address>(vtable,30*8)==image+0x118e4c0&&at<Address>(vtable,31*8)==image+0x11914f0;
}
static bool damage_test_event(Address pawn,Address target,Address fn,float value,float& effective,std::uint8_t& accepted){
    if(!pawn||!target||!fn||at<std::uint16_t>(fn,182)!=590)return false;
    const auto hit=field_of(fn,"Hit",136),damage=field_of(fn,"DamageToApply",216),applied=field_of(fn,"AppliedDamage",216);
    if(!damage_probe_struct(hit)||!damage_probe_struct(damage)||!damage_probe_struct(applied)||
       at<int>(hit,76)!=0||at<int>(damage,76)!=136||at<int>(applied,76)!=360)return false;
    const auto type=resolve(damage_probe_methods[2]);
    const auto amount=field_of(type,"DamageAmount",4);
    if(!amount||at<int>(amount,76)!=0||at<Address>(damage,0x78)!=type||at<Address>(applied,0x78)!=type)return false;
    const auto init=reinterpret_cast<void(*)(Address,void*)>(image+0x11914f0);
    const auto destroy=reinterpret_cast<void(*)(Address,void*)>(image+0x118e4c0);
    alignas(16) std::array<unsigned char,592> args{};
    init(hit,args.data());
    init(damage,args.data()+136);
    init(applied,args.data()+360);
    const auto point=location(target);
    std::memcpy(args.data()+136,&value,sizeof(value));
    std::memcpy(args.data()+352,&pawn,sizeof(pawn));
    std::memcpy(args.data()+576,point.data(),sizeof(point));
    args[588]=1;
    event(target,fn,args.data());
    effective=U::field<float>(args.data(),360);
    accepted=args[589];
    destroy(applied,args.data()+360);
    destroy(damage,args.data()+136);
    destroy(hit,args.data());
    return true;
}
static bool apply_stock_damage_probe(){
    static bool done;
    if(done)return true;
    const auto pawn=local_test_pawn(),fn=resolve(damage_probe_methods[0]);
    if(!pawn||!fn)return false;
    const auto target=damage_probe_target(pawn,fn);
    const auto hp=target?field_of(at<Address>(target,16),"HP",4):0;
    if(!hp)return false;
    const auto before=at<float>(target,at<int>(hp,76));
    const float value=4.f;
    float effective{};
    std::uint8_t accepted{};
    if(!damage_test_event(pawn,target,fn,value,effective,accepted))return false;
    const auto after=at<float>(target,at<int>(hp,76));
    std::string path;
    U::object_path(target,image+0x4a27f00,read_local,path);
    char report[420]{};
    const auto size=std::snprintf(report,sizeof(report),"target=%s requested=%.3f hp_before=%.3f hp_after=%.3f applied=%.3f accepted=%u\n",
        path.c_str(),value,before,after,effective,static_cast<unsigned>(accepted));
    if(size>0&&size<static_cast<int>(sizeof(report)))write_file(marker(role_index,L"stock-damage-result.txt").c_str(),report,static_cast<unsigned>(size));
    done=true;
    return true;
}
static bool run_native_damage_stage(){
    static unsigned stage;
    static float initial;
    static bool fired;
    static constexpr const wchar_t* steps[]{L"damage-ready.txt",L"damage-one.txt",L"damage-two.txt",L"damage-three.txt"};
    if(stage==4)return all_player_markers(steps[3]);
    E::NativeBindingView binding{};
    const auto pawn=local_test_pawn(),fn=resolve(damage_probe_methods[0]);
    if(!pawn||!fn||!core.inspect_binding(1,&binding)||!binding.confirmed)return false;
    const auto target=resolve(binding.actor);
    const auto field=target?field_of(at<Address>(target,16),"HP",4):0;
    if(!field)return false;
    const auto hp_at=at<int>(field,76);
    const auto hp=at<float>(target,hp_at);
    if(stage==0){
        if(hp<25||!std::isfinite(hp)||(!host&&at<Address>(image+0x4a62e10,0x4c*8)==image+0x11b7960))return false;
        initial=hp;
        write_file(marker(role_index,steps[0]).c_str(),"ok",2);
        ++stage;
        return false;
    }
    if(!all_player_markers(steps[stage-1]))return false;
    const float amount=3.f+stage;
    if(role_index==stage&&!fired){
        float effective{};
        std::uint8_t accepted{};
        if(!damage_test_event(pawn,target,fn,amount,effective,accepted))return false;
        fired=true;
        const bool suppressed=at<float>(target,hp_at)==hp&&!accepted;
        write_file(marker(role_index,L"damage-fired.txt").c_str(),suppressed?"suppressed":"local-hit",suppressed?10:9);
        if(!suppressed){phase=94;return false;}
    }
    const auto expected=initial-(3.f*stage+stage*(stage+1)*.5f);
    if(std::abs(hp-expected)>.05f)return false;
    char report[130]{};
    const auto size=std::snprintf(report,sizeof(report),"generation=%u sender=%u hp=%.3f expected=%.3f\n",
                                  binding.generation,stage,hp,expected);
    if(size>0)write_file(marker(role_index,steps[stage]).c_str(),report,static_cast<unsigned>(size));
    ++stage;
    fired=false;
    return false;
}
static bool verify_damage_cleanup(){
    const bool restored=at<Address>(image+0x4a62e10,0x4c*8)==image+0x11b7960&&
                        at<Address>(image+0x4a62e10,0x27*8)==image+0x11b9d20&&
                        at<Address>(image+0x4a62e10,0x01*8)==image+0x11b5e60&&
                        at<Address>(image+0x4a62e10,0x48*8)==image+0x11b6f10;
    if(restored)write_file(marker(role_index,L"damage-clean.txt").c_str(),"ok",2);
    return restored;
}
