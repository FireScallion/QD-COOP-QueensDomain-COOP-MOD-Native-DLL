static bool probe_stock_checkpoint(Address engine){
    static bool done;
    if(done)return true;
    const auto gi=object_field(engine,"GameInstance");
    const auto active=gi?object_field(gi,"ActiveSaveSlot"):0;
    if(!active)return false;
    const auto base=object_field(gi,"BaseActiveSave");
    std::string text="base_active="+std::to_string(base!=0)+" same="+std::to_string(base==active)+"\n";
    for(unsigned i=0;i<cp_probe_methods.size();++i){
        const auto fn=resolve(cp_probe_methods[i]);
        text+=std::string(cp_probe_paths[i])+"\n";
        if(!fn||i==cp_utils)continue;
        const auto size=at<int>(fn,104);
        text+="  size="+std::to_string(size)+" parms="+std::to_string(at<std::uint16_t>(fn,182))+"\n";
        for(auto prop=at<Address>(fn,80);prop;prop=at<Address>(prop,32)){
            if(!(at<std::uint64_t>(prop,64)&0x80u))continue;
            std::string field;
            if(U::name_string(image+0x4a27f00,at<U::NameKey>(prop,40),read_local,field))
                text+="  "+field+" "+std::to_string(at<int>(prop,76))+" "+std::to_string(at<int>(prop,60))+"\n";
        }
        if(i==cp_save_blueprint&&size==2387)
            write_file(marker(role_index,L"stock-save-script.bin").c_str(),at<const char*>(fn,96),size);
    }
    int index=-1;
    event(gi,resolve(cp_probe_methods[cp_get_index]),&index);
    const auto path=marker(role_index,(L"profile/Saved/SaveGames/World"+std::to_wstring(index)+L".sav").c_str());
    const auto slot=L"FULLPATH:"+path;
    alignas(16) Parameters args{};
    struct FString {const wchar_t* chars;int count,capacity;};
    const auto length=static_cast<int>(slot.size()+1);
    const FString parameter{slot.c_str(),length,length};
    std::memcpy(args.data(),&parameter,sizeof(parameter));
    event(resolve(gameplay),resolve(cp_probe_methods[cp_load]),args.data());
    const auto loaded=U::field<Address>(args.data(),24);
    const auto save_type=at<Address>(active,16);
    const auto level=field_of(save_type,"SaveLevel",8),location=field_of(save_type,"SaveLocation",48);
    if(!loaded||loaded==active||at<Address>(loaded,16)!=save_type||!level||!location)return false;
    std::string level_name,active_level;
    const auto at_level=at<int>(level,76);
    if(!U::name_string(image+0x4a27f00,at<U::NameKey>(loaded,at_level),read_local,level_name)||
       !U::name_string(image+0x4a27f00,at<U::NameKey>(active,at_level),read_local,active_level))return false;
    const auto pos=at<std::array<float,3>>(loaded,at<int>(location,76)+16);
    const auto pawn=local_test_pawn();
    std::string world;
    if(!pawn||!U::object_path(at<Address>(at<Address>(pawn,32),32),image+0x4a27f00,read_local,world))return false;
    text+="active_index="+std::to_string(index)+" active_level="+active_level+" disk_level="+level_name+
          " loaded_pos="+std::to_string(pos[0])+","+std::to_string(pos[1])+","+std::to_string(pos[2])+
          " world="+world+"\n";
    write_file(marker(role_index,L"checkpoint-layout.txt").c_str(),text.data(),static_cast<unsigned>(text.size()));
    done=true;
    return true;
}
